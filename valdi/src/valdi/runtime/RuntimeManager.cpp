//
//  RuntimeManager.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 1/8/19.
//

#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/RuntimeMessageHandler.hpp"
#include "valdi/runtime/Attributes/DefaultAttributeProcessors.hpp"
#include "valdi/runtime/Context/AttributionResolver.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Resources/BytesAssetLoader.hpp"
#include "valdi/runtime/ValdiBuildFlags.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"

#include "valdi/runtime/Resources/AssetLoaderManager.hpp"

#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"

#include "valdi_core/cpp/Utils/ContainerUtils.hpp"

#include "valdi/runtime/Debugger/DebuggerService.hpp"
#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Utils/ShutdownUtils.hpp"
#include "valdi/runtime/Views/GlobalViewFactories.hpp"
#include "valdi/runtime/Views/ViewPreloader.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"

#include "valdi_core/ModuleFactoriesProvider.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

#include <algorithm>

namespace Valdi {

Shared<DebuggerService> createDebuggerService(bool enableDebuggerService,
                                              bool disableHotReloader,
                                              bool isStandalone,
                                              PlatformType platformType,
                                              const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
                                              const Ref<ILogger>& logger) {
    if (!enableDebuggerService) {
        return nullptr;
    }

    if constexpr (kDebuggerServiceEnabled) {
        snap::valdi_core::Platform platform;
        switch (platformType) {
            case PlatformTypeIOS:
                platform = snap::valdi_core::Platform::Ios;
                break;
            case PlatformTypeAndroid:
                platform = snap::valdi_core::Platform::Android;
                break;
            case PlatformTypeMacOS:
            case PlatformTypeWeb:
                platform = snap::valdi_core::Platform::Ios;
                break;
        }
        auto debuggerPort = DebuggerService::resolveDebuggerPort(isStandalone);

        auto debuggerService = Valdi::makeShared<DebuggerService>(
            runtimeMessageHandler, platform, debuggerPort, disableHotReloader, logger);
        debuggerService->postInit();
        return debuggerService.toShared();
    } else {
        return nullptr;
    }
}

class ANRDetectorListener : public IJavaScriptANRDetectorListener {
public:
    explicit ANRDetectorListener(const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler)
        : _runtimeMessageHandler(runtimeMessageHandler) {}
    ~ANRDetectorListener() override = default;

    JavaScriptANRBehavior onANR(JavaScriptANR anr) override {
        if (anr.getCapturedStacktraces().empty()) {
            _runtimeMessageHandler->onJsCrash(anr.getModuleName(), anr.getMessage(), "", true);
        } else {
            auto stackTrace = anr.getCapturedStacktraces()[0].getStackTrace().slowToString();
            _runtimeMessageHandler->onJsCrash(anr.getModuleName(), anr.getMessage(), stackTrace, true);
        }

        if (_shouldCrashOnANR && !anr.hasRunningStacktrace()) {
            return JavaScriptANRBehavior::CRASH;
        } else {
            return JavaScriptANRBehavior::KEEP_GOING;
        }
    }

    void setShouldCrashOnANR(bool shouldCrashOnANR) {
        _shouldCrashOnANR = shouldCrashOnANR;
    }

private:
    Shared<snap::valdi::RuntimeMessageHandler> _runtimeMessageHandler;
    std::atomic_bool _shouldCrashOnANR = false;
};

RuntimeManager::RuntimeManager(const Ref<IMainThreadDispatcher>& mainThreadDispatcher,
                               IJavaScriptBridge* jsBridge,
                               const Ref<IDiskCache>& diskCache,
                               Shared<snap::valdi::Keychain> keychain,
                               const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
                               PlatformType platformType,
                               ThreadQoSClass jsThreadQoS,
                               const Ref<ILogger>& logger,
                               bool enableDebuggerService,
                               bool disableHotReloader,
                               bool isStandalone)
    : _initStopWatch(std::make_shared<MetricsStopWatch>()),
      _yogaConfig(Valdi::Yoga::createConfig(0)),
      _debuggerService(createDebuggerService(
          enableDebuggerService, disableHotReloader, isStandalone, platformType, runtimeMessageHandler, logger)),
      _deferredGCTask(DispatchQueue::TaskIDNull),
      _mainThreadManager(makeShared<MainThreadManager>(mainThreadDispatcher)),
      _assetLoaderManager(makeShared<AssetLoaderManager>()),
      _innerLogger(_debuggerService != nullptr ? _debuggerService->getBridgeLogger() : logger),
      _logger(makeShared<BridgeLogger>(_innerLogger)),
      _jsBridge(jsBridge),
      _diskCache(diskCache),
      _keychain(std::move(keychain)),
      _runtimeMessageHandler(runtimeMessageHandler),
      _colorPalette(makeShared<ColorPalette>()),
      _platformType(platformType),
      _jsThreadQoS(jsThreadQoS),
      _debuggerServiceEnabled(_debuggerService != nullptr) {
    _mainThreadManager->postInit();
    _workerQueue = DispatchQueue::create(STRING_LITERAL("Valdi Worker Thread"), ThreadQoSClassHigh);
    _anrDetector = makeShared<JavaScriptANRDetector>(_logger);
    if (runtimeMessageHandler != nullptr) {
        _anrDetector->setListener(makeShared<ANRDetectorListener>(runtimeMessageHandler));
    }

    _colorPalette->setListener(this);
}

RuntimeManager::~RuntimeManager() {
    fullTeardown();
}

void RuntimeManager::postInit() {
    registerModuleFactoriesProvider(ModuleFactoryRegistry::sharedInstance());
}

void RuntimeManager::fullTeardown() {
    if (_anrDetector != nullptr) {
        _anrDetector->stop();
        _anrDetector = nullptr;
    }
    stopDebuggerServices();
    std::vector<SharedRuntime> allRuntimes;
    {
        std::lock_guard<Mutex> lock(_mutex);
        allRuntimes = getAllRuntimes(lock);
        _runtimes.clear();
    }
    for (const auto& runtime : allRuntimes) {
        runtime->fullTeardown();
    }
    _mainThreadManager->clearAndTeardown();
    _workerQueue->flushAndTeardown();
    _debuggerService = nullptr;
}

SharedRuntime RuntimeManager::createRuntime(const Shared<IResourceLoader>& resourceLoader,
                                            double displayScale,
                                            const Ref<MainThreadManager>& mainThreadManagerOverride,
                                            std::optional<PlatformType> platformType) {
    auto runtime = Valdi::makeShared<Runtime>(_attributeIds,
                                              platformType ? platformType.value() : _platformType,
                                              mainThreadManagerOverride != nullptr ? mainThreadManagerOverride :
                                                                                     _mainThreadManager,
                                              _jsBridge,
                                              _workerQueue,
                                              _jsThreadQoS,
                                              _userSession,
                                              _keychain,
                                              resourceLoader,
                                              _assetLoaderManager,
                                              _requestManager,
                                              _colorPalette,
                                              _diskCache,
                                              _yogaConfig,
                                              _runtimeMessageHandler,
                                              _anrDetector,
                                              displayScale,
                                              _debuggerService != nullptr,
                                              _logger);

    std::vector<std::shared_ptr<::snap::valdi_core::ModuleFactory>> moduleFactories;
    std::vector<RegisteredTypeConverter> typeConverters;
    std::vector<Ref<IRuntimeManagerListener>> listeners;
    Ref<ValdiRuntimeTweaks> runtimeTweaks;
    Ref<AttributionResolver> attributionResolver;
    Ref<Metrics> metrics;

    bool shouldInit = true;
    bool autoRenderDisabled;
    {
        std::lock_guard<Mutex> guard(_mutex);
        removeExpiredRuntimes();
        _runtimes.emplace_back(runtime);
        shouldInit = !_disableRuntimeAutoInit;
        moduleFactories = _registeredModuleFactories;
        typeConverters = _registeredTypeConverters;
        listeners = _listeners;
        runtimeTweaks = _runtimeTweaks;
        attributionResolver = _attributionResolver;
        metrics = _metrics;
        autoRenderDisabled = _loadOperationsCount > 0;
    }

    runtime->setRuntimeTweaks(runtimeTweaks);
    runtime->setAutoRenderDisabled(autoRenderDisabled);
    runtime->setMetrics(metrics);
    runtime->getContextManager().setAttributionResolver(attributionResolver);

    if (shouldInit) {
        runtime->postInit();
    }

    for (const auto& moduleFactory : moduleFactories) {
        runtime->registerNativeModuleFactory(moduleFactory);
    }

    for (const auto& typeConverter : typeConverters) {
        runtime->registerTypeConverter(typeConverter.className, typeConverter.functionPath);
    }

    for (const auto& listener : listeners) {
        listener->onRuntimeCreated(*runtime);
    }

    return runtime;
}

void RuntimeManager::attachHotReloader(const Ref<Runtime>& runtime) {
    if (_debuggerService != nullptr) {
        _debuggerService->addListener(runtime.get());
    }
}

Ref<ViewManagerContext> RuntimeManager::createViewManagerContext(
    IViewManager& viewManager, bool enablePreloading, const Ref<MainThreadManager>& mainThreadManagerOverride) {
    auto viewManagerContext = makeShared<ViewManagerContext>(
        viewManager,
        _attributeIds,
        _colorPalette,
        _yogaConfig,
        enablePreloading,
        mainThreadManagerOverride != nullptr ? mainThreadManagerOverride : _mainThreadManager,
        *_logger);

    _viewManagerContexts.emplace_back(viewManagerContext);

    return viewManagerContext;
}

void RuntimeManager::registerModuleFactoriesProvider(
    const Shared<snap::valdi_core::ModuleFactoriesProvider>& moduleFactoriesProvider) {
    auto moduleFactories = moduleFactoriesProvider->createModuleFactories(Valdi::Value(Valdi::strongSmallRef(this)));

    std::vector<SharedRuntime> runtimes;
    {
        std::lock_guard<Mutex> guard(_mutex);
        for (const auto& moduleFactory : moduleFactories) {
            _registeredModuleFactories.emplace_back(moduleFactory);
        }
        runtimes = getAllRuntimes(guard);
    }

    // Register the ModuleFactories to the existing Runtimes
    for (const auto& runtime : runtimes) {
        for (const auto& moduleFactory : moduleFactories) {
            runtime->registerNativeModuleFactory(moduleFactory);
        }
    }
}

void RuntimeManager::registerTypeConverter(const StringBox& className, const StringBox& functionPath) {
    std::vector<SharedRuntime> runtimes;
    {
        std::lock_guard<Mutex> guard(_mutex);
        auto& it = _registeredTypeConverters.emplace_back();
        it.className = className;
        it.functionPath = functionPath;
        runtimes = getAllRuntimes(guard);
    }

    // Register the TypeConverter to the existing Runtimes
    for (const auto& runtime : runtimes) {
        runtime->registerTypeConverter(className, functionPath);
    }
}

void RuntimeManager::startDebuggerServices() {
    if (_debuggerServiceEnabled && _jsBridge != nullptr) {
        _jsBridge->startJsDebuggerServer(*_logger);
    }
    if (kDebuggerServiceEnabled && _debuggerService != nullptr) {
        _debuggerService->start();
    }
}

void RuntimeManager::stopDebuggerServices() {
    if (_debuggerServiceEnabled && _jsBridge != nullptr) {
        _jsBridge->stopJsDebuggerServer();
    }
    if (kDebuggerServiceEnabled && _debuggerService != nullptr) {
        _debuggerService->stop();
    }
}

void RuntimeManager::applicationDidResume() {
    VALDI_INFO(*_logger, "Application did resume");
    _anrDetector->onEnterForeground();
    startDebuggerServices();
    cancelDeferredGCTask();
}

void RuntimeManager::applicationWillPause() {
    VALDI_INFO(*_logger, "Application will pause");
    _anrDetector->onEnterBackground();
    if (!_keepDebuggerServiceOnPause) {
        stopDebuggerServices();
    }
    clearViewPools();
    scheduleDeferredGCTask();
}

void RuntimeManager::applicationIsLowInMemory() {
    VALDI_INFO(*_logger, "Application is low in memory");
    clearViewPools();
    for (const auto& runtime : getAllRuntimes()) {
        runtime->removeUnusedResources();
    }
}

void RuntimeManager::applicationWillTerminate() {
    VALDI_INFO(*_logger, "Application will terminate");
    Valdi::setApplicationShuttingDown(true);
    stopDebuggerServices();

    for (const auto& runtime : getAllRuntimes()) {
        runtime->getJavaScriptRuntime()->partialTeardown();
    }
}

void RuntimeManager::clearViewPools() {
    for (const auto& viewManagerContext : _viewManagerContexts) {
        viewManagerContext->clearViewPools();
    }
}

void RuntimeManager::removeExpiredRuntimes() {
    auto it = _runtimes.begin();
    while (it != _runtimes.end()) {
        if (it->expired()) {
            it = _runtimes.erase(it);
        } else {
            it++;
        }
    }
}

void RuntimeManager::forceBindAttributes(const StringBox& className) {
    // Will lazily trigger the binding attributes if needed.
    auto viewManagerContexts = _viewManagerContexts;

    _workerQueue->async([viewManagerContexts, className]() {
        for (const auto& viewManagerContext : viewManagerContexts) {
            viewManagerContext->getAttributesManager().getAttributesForClass(className);
        }
    });
}

std::vector<SharedRuntime> RuntimeManager::getAllRuntimes() {
    std::lock_guard<Mutex> guard(_mutex);
    return getAllRuntimes(guard);
}

std::vector<SharedRuntime> RuntimeManager::getAllRuntimes(std::lock_guard<Mutex>& /*lock*/) {
    std::vector<SharedRuntime> runtimes;
    runtimes.reserve(_runtimes.size());

    auto it = _runtimes.begin();
    while (it != _runtimes.end()) {
        if (auto runtime = it->lock()) {
            runtimes.emplace_back(std::move(runtime));
            it++;
        } else {
            it = _runtimes.erase(it);
        }
    }

    return runtimes;
}

const Ref<DispatchQueue>& RuntimeManager::getWorkerQueue() const {
    return _workerQueue;
}

const Ref<AssetLoaderManager>& RuntimeManager::getAssetLoaderManager() const {
    return _assetLoaderManager;
}

void RuntimeManager::registerBytesAssetLoader(const Ref<IDiskCache>& diskCache) {
    std::lock_guard<Mutex> guard(_mutex);
    if (_bytesAssetLoader != nullptr) {
        _assetLoaderManager->unregisterAssetLoader(_bytesAssetLoader);
    }

    _bytesAssetLoader = makeShared<BytesAssetLoader>(diskCache, _requestManager, _workerQueue, *_logger);
    _assetLoaderManager->registerAssetLoader(_bytesAssetLoader);
}

void RuntimeManager::setRequestManager(const Shared<snap::valdi_core::HTTPRequestManager>& requestManager) {
    _requestManager.set(requestManager);
}

bool RuntimeManager::debuggerServiceEnabled() const {
    return _debuggerService != nullptr;
}

void RuntimeManager::setUserSession(const StringBox& userId) {
    if (userId.isEmpty()) {
        _userSession.set(nullptr);
    } else {
        _userSession.set(Valdi::makeShared<UserSession>(userId));
    }
}

MainThreadManager& RuntimeManager::getMainThreadManager() {
    return *_mainThreadManager;
}

void RuntimeManager::setDisableRuntimeAutoInit(bool disableRuntimeAutoInit) {
    _disableRuntimeAutoInit = disableRuntimeAutoInit;
}

void RuntimeManager::setApplicationId(const StringBox& applicationId) {
    if (kDebuggerServiceEnabled && _debuggerService != nullptr) {
        _debuggerService->setApplicationId(applicationId);
    }
}

void RuntimeManager::onColorPaletteUpdated(const ColorPalette& /*colorPalette*/) {
    FlatSet<AttributeId> attributesToReapply;

    // Clear the cache for all color attributes
    for (const auto& viewManagerContext : _viewManagerContexts) {
        for (const auto& it : viewManagerContext->getAttributesManager().getAllBoundAttributes()) {
            for (const auto& handlerIt : it.second->getHandlers()) {
                auto* handler = it.second->getAttributeHandlerForId(handlerIt.first);

                if (handler->shouldReevaluateOnColorPaletteChange()) {
                    handler->clearPreprocessorCache();
                    attributesToReapply.insert(handlerIt.first);
                }
            }
        }
    }

    // Reapply all the color attributes
    auto allAttributes = makeShared<std::vector<AttributeId>>();
    allAttributes->insert(allAttributes->end(), attributesToReapply.begin(), attributesToReapply.end());

#if VALDI_DEBUG_TREE_UPDATES
    std::string applyTrigger = "apply_attributes";
    if (!_viewManagerContexts.empty() && !attributesToReapply.empty()) {
        const auto& attributeIds = _viewManagerContexts.front()->getAttributesManager().getAttributeIds();
        std::string names;
        size_t count = 0;
        constexpr size_t kMaxNames = 12;
        constexpr size_t kMaxLen = 80;
        for (AttributeId id : attributesToReapply) {
            if (count >= kMaxNames || (count > 0 && names.size() >= kMaxLen)) {
                names += ",...";
                break;
            }
            if (count != 0) {
                names += ",";
            }
            names += attributeIds.getNameForId(id).slowToString();
            ++count;
        }
        if (!names.empty()) {
            applyTrigger += ":";
            applyTrigger += names;
        }
    }

    for (const auto& runtime : getAllRuntimes()) {
        for (const auto& tree : runtime->getViewNodeTreeManager().getAllRootViewNodeTrees()) {
            tree->scheduleExclusiveUpdate(
                [treePtr = tree.get(), allAttributes]() {
                    auto rootViewNode = treePtr->getRootViewNode();
                    if (rootViewNode != nullptr) {
                        rootViewNode->reapplyAttributesRecursive(
                            treePtr->getCurrentViewTransactionScope(), *allAttributes, false);
                    }
                },
                Valdi::DispatchFunction(),
                applyTrigger);
        }
    }
#else
    for (const auto& runtime : getAllRuntimes()) {
        for (const auto& tree : runtime->getViewNodeTreeManager().getAllRootViewNodeTrees()) {
            tree->scheduleExclusiveUpdate(
                [treePtr = tree.get(), allAttributes]() {
                    auto rootViewNode = treePtr->getRootViewNode();
                    if (rootViewNode != nullptr) {
                        rootViewNode->reapplyAttributesRecursive(
                            treePtr->getCurrentViewTransactionScope(), *allAttributes, false);
                    }
                },
                Valdi::DispatchFunction());
        }
    }
#endif
}

Valdi::ILogger& RuntimeManager::getLogger() const {
    return *_logger;
}

void RuntimeManager::setLogListener(Weak<BridgeLoggerListener> listener) {
    _logger->setListener(listener);
}

const Ref<IDiskCache>& RuntimeManager::getDiskCache() const {
    return _diskCache;
}

const Holder<Ref<UserSession>>& RuntimeManager::getUserSession() const {
    return _userSession;
}

void RuntimeManager::setMetrics(const Ref<Metrics>& metrics) {
    std::lock_guard<Mutex> guard(_mutex);
    _metrics = metrics;
    _anrDetector->setMetrics(metrics);
}

void RuntimeManager::setTweakValueProvider(const Shared<ITweakValueProvider>& tweakValueProvider) {
    std::vector<SharedRuntime> runtimes;
    Ref<ValdiRuntimeTweaks> runtimeTweaks;

    if (tweakValueProvider != nullptr) {
        runtimeTweaks = makeShared<ValdiRuntimeTweaks>(tweakValueProvider);
    }

    {
        std::lock_guard<Mutex> guard(_mutex);
        _runtimeTweaks = runtimeTweaks;
        runtimes = getAllRuntimes(guard);
    }

    auto anrDetectorListener = castOrNull<ANRDetectorListener>(_anrDetector->getListener());

    if (anrDetectorListener != nullptr) {
        anrDetectorListener->setShouldCrashOnANR(runtimeTweaks != nullptr ? runtimeTweaks->shouldCrashOnANR() : false);
    }
    _anrDetector->setNudgeEnabled(runtimeTweaks != nullptr ? runtimeTweaks->shouldNudgeJSThread() : false);

    auto disableAnimationRemoveOnCompleteIos =
        runtimeTweaks != nullptr ? runtimeTweaks->disableAnimationRemoveOnCompleteIos() : false;
    for (const auto& viewManagerContext : _viewManagerContexts) {
        viewManagerContext->getViewManager().setDisableAnimationRemoveOnCompleteIos(
            disableAnimationRemoveOnCompleteIos);
    }

    for (const auto& runtime : runtimes) {
        runtime->setRuntimeTweaks(runtimeTweaks);
    }
}

void RuntimeManager::cancelDeferredGCTask() {
    const auto previousTaskID = swapDeferredGCTask(DispatchQueue::TaskIDNull);
    if (previousTaskID != DispatchQueue::TaskIDNull) {
        _workerQueue->cancel(previousTaskID);
    }
}

void RuntimeManager::scheduleDeferredGCTask() {
    auto weakThis = weakRef(this);
    const auto taskID = _workerQueue->asyncAfter(
        [weakThis]() {
            auto strongThis = weakThis.lock();
            if (!strongThis) {
                return;
            }
            const auto runtimes = strongThis->getAllRuntimes();
            if (runtimes.empty()) {
                return;
            }
            const bool enabledDeferredGC = runtimes[0]->getResourceManager().enableDeferredGC();
            if (!enabledDeferredGC) {
                return;
            }
            for (const auto& runtime : runtimes) {
                runtime->getJavaScriptRuntime()->performGc();
            }
        },
        std::chrono::seconds(5));

    const auto previousTaskID = swapDeferredGCTask(taskID);
    if (previousTaskID != DispatchQueue::TaskIDNull) {
        _workerQueue->cancel(previousTaskID);
    }
}

task_id_t RuntimeManager::swapDeferredGCTask(task_id_t task) {
    std::lock_guard<Mutex> guard(_mutex);
    std::swap(task, _deferredGCTask);
    return task;
}

void RuntimeManager::setJsThreadQoS(ThreadQoSClass jsThreadQoS) {
    std::lock_guard<Mutex> guard(_mutex);
    if (jsThreadQoS != _jsThreadQoS) {
        _jsThreadQoS = jsThreadQoS;
        auto runtimes = getAllRuntimes(guard);
        for (const auto& runtime : runtimes) {
            runtime->getJavaScriptRuntime()->setThreadQoS(jsThreadQoS);
        }
    }
}

void RuntimeManager::setAttributionResolver(const Ref<AttributionResolver>& attributionResolver) {
    std::lock_guard<Mutex> guard(_mutex);
    _attributionResolver = attributionResolver;
    auto runtimes = getAllRuntimes(guard);
    for (const auto& runtime : runtimes) {
        runtime->getContextManager().setAttributionResolver(attributionResolver);
    }
}

void RuntimeManager::addListener(const Ref<IRuntimeManagerListener>& listener) {
    std::vector<SharedRuntime> runtimes;
    {
        std::lock_guard<Mutex> guard(_mutex);
        _listeners.emplace_back(listener);
        runtimes = getAllRuntimes(guard);
    }

    for (const auto& runtime : runtimes) {
        listener->onRuntimeCreated(*runtime);
    }
}

void RuntimeManager::removeListener(const Ref<IRuntimeManagerListener>& listener) {
    std::lock_guard<Mutex> guard(_mutex);
    auto it = _listeners.begin();
    while (it != _listeners.end()) {
        if (*it == listener) {
            it = _listeners.erase(it);
        } else {
            it++;
        }
    }
}

void RuntimeManager::updateLoadOperationsCount(int increment) {
    std::lock_guard<Mutex> guard(_mutex);
    _loadOperationsCount += increment;

    if (_loadOperationsCount == 1 && increment > 0) {
        // Just added a new load operation. disable auto render
        for (const auto& runtime : getAllRuntimes(guard)) {
            runtime->setAutoRenderDisabled(true);
        }
    } else if (_loadOperationsCount == 0) {
        // Our load operation queue is empty, enable auto render
        for (const auto& runtime : getAllRuntimes(guard)) {
            runtime->setAutoRenderDisabled(false);
        }
    }
}

void RuntimeManager::enqueueLoadOperation(Valdi::DispatchFunction&& loadOperation) {
    if (Valdi::isApplicationShuttingDown()) {
        return;
    }
    updateLoadOperationsCount(1);

    _workerQueue->async([weakSelf = weakRef(this), loadOperation = std::move(loadOperation)]() {
        if (auto strongThis = weakSelf.lock()) {
            loadOperation();
            strongThis->updateLoadOperationsCount(-1);
        }
    });
}

void RuntimeManager::flushLoadOperations() {
    std::unique_lock<Mutex> guard(_mutex);

    while (_loadOperationsCount != 0) {
        guard.unlock();
        // Flush worker queue and try again
        _workerQueue->sync([]() {});
        guard.lock();
    }
}

JavaScriptContextMemoryStatistics RuntimeManager::dumpMemoryStatistics() {
    const auto runtimes = this->getAllRuntimes();
    JavaScriptContextMemoryStatistics stats;
    for (const auto& runtime : runtimes) {
        auto stat = runtime->getJavaScriptRuntime()->dumpMemoryStatistics();
        stats.memoryUsageBytes += stat.memoryUsageBytes;
        stats.objectsCount += stat.objectsCount;
    }

    return stats;
}

void RuntimeManager::emitMetrics(void (Metrics::*emitterFunc)(const MetricsDuration&)) {
    std::shared_ptr<MetricsStopWatch> initStopWatch;
    Ref<Metrics> metrics;

    {
        std::lock_guard<Mutex> guard(_mutex);
        initStopWatch = _initStopWatch;
        metrics = _metrics;
    }

    if (metrics != nullptr && initStopWatch != nullptr) {
        (metrics.get()->*emitterFunc)(initStopWatch->elapsed());
    }
}

void RuntimeManager::emitInitMetrics() {
    emitMetrics(&Metrics::emitRuntimeManagerInitLatency);
}

void RuntimeManager::emitXpatCreateRuntimeMetrics() {
    emitMetrics(&Metrics::emitRuntimeManagerXpatInitLatency);
}

void RuntimeManager::emitIosRuntimeCreateMetrics() {
    emitMetrics(&Metrics::emitRuntimeManagerIosInitLatency);
}

void RuntimeManager::emitUserSessionReadyMetrics() {
    emitMetrics(&Metrics::emitUserSessionReadyLatency);
}

void RuntimeManager::setKeepDebuggerServiceOnPause(bool keepDebuggerServiceOnPause) {
    _keepDebuggerServiceOnPause = keepDebuggerServiceOnPause;
}

PlatformType RuntimeManager::getPlatformType() const {
    return _platformType;
}

const Ref<JavaScriptANRDetector>& RuntimeManager::getANRDetector() const {
    return _anrDetector;
}

VALDI_CLASS_IMPL(RuntimeManager)

} // namespace Valdi
