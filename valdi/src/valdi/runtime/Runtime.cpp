//
//  Runtime.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include <fmt/ostream.h>

#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi_core/cpp/Constants.hpp"

#include "valdi/runtime/Attributes/AttributeOwner.hpp"

#include "valdi/runtime/CSS/CSSDocument.hpp"

#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi/runtime/Rendering/ViewNodeRenderer.hpp"

#include "valdi/runtime/Resources/AssetCatalog.hpp"

#include "valdi/runtime/Debugger/DebuggerService.hpp"

#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNodeViewStats.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"

#include "valdi/runtime/JavaScript/Modules/AttributedTextNativeModuleFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/FileSystemFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/JavaScriptModuleFactoryBridge.hpp"
#include "valdi/runtime/JavaScript/Modules/PersistentStoreModuleFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/ProtobufModuleFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/TCPSocketModuleFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/UnicodeModuleFactory.hpp"

#include "valdi/runtime/Metrics/Metrics.hpp"

#include "valdi/RuntimeMessageHandler.hpp"
#include "valdi/runtime/ErrorCodes.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi_core/cpp/Resources/ResourceId.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"

#include "utils/platform/BuildOptions.hpp"
#include "utils/time/StopWatch.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <vector>
#include <yoga/YGNode.h>

namespace Valdi {

constexpr bool kTCPSocketEnabled = (snap::kIsGoldBuild || snap::kIsDevBuild);

float kUndefinedFloatValue = YGUndefined;

Runtime::Runtime(AttributeIds& attributeIds,
                 PlatformType platformType,
                 const Ref<MainThreadManager>& mainThreadManager,
                 IJavaScriptBridge* jsBridge,
                 Ref<DispatchQueue> workerQueue,
                 ThreadQoSClass jsRuntimeThreadQoS,
                 const SharedAtomicObject<UserSession>& userSession,
                 const Shared<snap::valdi::Keychain>& keychain,
                 const Shared<IResourceLoader>& resourceLoader,
                 const Ref<AssetLoaderManager>& assetLoaderManager,
                 const Holder<Shared<snap::valdi_core::HTTPRequestManager>>& requestManager,
                 const Ref<ColorPalette>& colorPalette,
                 const Ref<IDiskCache>& diskCache,
                 const std::shared_ptr<YGConfig>& yogaConfig,
                 const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
                 const Ref<JavaScriptANRDetector>& anrDetector,
                 double deviceDensity,
                 bool debuggerServiceEnabled,
                 const Ref<ILogger>& logger)
    : _initStopWatch(std::make_unique<MetricsStopWatch>()),
      _attributeIds(attributeIds),
      _platformType(platformType),
      _resourceManager(makeShared<ResourceManager>(resourceLoader,
                                                   diskCache,
                                                   assetLoaderManager,
                                                   requestManager,
                                                   workerQueue,
                                                   *mainThreadManager,
                                                   deviceDensity,
                                                   debuggerServiceEnabled,
                                                   *logger)),
      _contextManager(logger, this),
      _viewNodeManager(*mainThreadManager, *logger),
      _mainThreadManager(mainThreadManager),
      _colorPalette(colorPalette),
      _diskCache(diskCache),
      _userSession(userSession),
      _requestManager(requestManager),
      _keychain(keychain),
      _yogaConfig(yogaConfig),
      _workerQueue(std::move(workerQueue)),
      _runtimeMessageHandler(runtimeMessageHandler),
      _logger(logger),
      _debuggerServiceEnabled(debuggerServiceEnabled),
      _listener(nullptr) {
    if (jsBridge != nullptr) {
        _javaScriptRuntime = Valdi::makeShared<JavaScriptRuntime>(*jsBridge,
                                                                  *_resourceManager,
                                                                  _contextManager,
                                                                  *_mainThreadManager,
                                                                  attributeIds,
                                                                  platformType,
                                                                  debuggerServiceEnabled,
                                                                  jsRuntimeThreadQoS,
                                                                  anrDetector,
                                                                  logger);
    }

    // Uncomment to print the internal Valdi object sizes
    //      VALDI_INFO(_logger, "ViewNode size: {}", sizeof(ViewNode));
    //      VALDI_INFO(_logger, "ViewNodeAttributesApplier size: {}", sizeof(ViewNodeAttributesApplier));
    //      VALDI_INFO(_logger, "ViewNodeAttribute size: {}", sizeof(ViewNodeAttribute));
}

Runtime::~Runtime() {
    fullTeardown();
}

void Runtime::fullTeardown() {
    if (Valdi::traceInitialization) {
        VALDI_INFO(*_logger, "Tearing down Valdi Runtime");
    }
    _viewNodeManager.removeAllViewNodeTrees();
    _contextManager.destroyAllContexts();

    if (_javaScriptRuntime != nullptr) {
        _javaScriptRuntime->fullTeardown();
    }
}

void Runtime::postInit() {
    if (_didInit) {
        return;
    }
    _didInit = true;

    if (_javaScriptRuntime != nullptr) {
        _javaScriptRuntime->setListener(this);
    }
    _viewNodeManager.setRuntime(weakRef(this));
    _contextManager.setListener(this);

    if (_javaScriptRuntime != nullptr) {
        _javaScriptRuntime->postInit();

        if (_diskCache != nullptr) {
            registerNativeModuleFactory(Valdi::makeShared<PersistentStoreModuleFactory>(
                _diskCache, _workerQueue, _userSession, _keychain, *_logger, disablePersistentStoreEncryption()));
        }
        registerNativeModuleFactory(makeShared<FileSystemFactory>().toShared());
        registerNativeModuleFactory(makeShared<AttributedTextNativeModuleFactory>(_colorPalette, *_logger).toShared());

        registerJavaScriptModuleFactory(makeShared<ProtobufModuleFactory>(*_resourceManager, _workerQueue, *_logger));
        registerJavaScriptModuleFactory(makeShared<UnicodeModuleFactory>());

        if constexpr (kTCPSocketEnabled) {
            registerNativeModuleFactory(makeShared<TCPSocketModuleFactory>().toShared());
        }
    }

    if (Valdi::traceInitialization) {
        VALDI_INFO(*_logger, "Initialized Valdi Runtime");
    }
}

void Runtime::removeUnusedResources() {
    if (_javaScriptRuntime != nullptr) {
        auto self = strongSmallRef(this);
        _javaScriptRuntime->unloadUnusedModules([self]() { self->_resourceManager->removeUnusedResources(); });
    } else {
        _resourceManager->removeUnusedResources();
    }
}

SharedContext Runtime::createContext(const Ref<ViewManagerContext>& viewManagerContext,
                                     const StringBox& path,
                                     const Shared<ValueConvertible>& initialViewModel,
                                     const Shared<ValueConvertible>& componentContext,
                                     bool deferRender) {
    auto componentPath = ComponentPath::parse(path);
    return createContext(viewManagerContext, componentPath, initialViewModel, componentContext, deferRender);
}

SharedContext Runtime::createContext(const Ref<ViewManagerContext>& viewManagerContext,
                                     const ComponentPath& componentPath,
                                     const Shared<ValueConvertible>& initialViewModel,
                                     const Shared<ValueConvertible>& componentContext,
                                     bool deferRender) {
    Ref<ContextHandler> contextHandler;
    if (_javaScriptRuntime != nullptr) {
        contextHandler = _javaScriptRuntime->getContextHandler();
    }

    auto context = _contextManager.createContext(contextHandler,
                                                 viewManagerContext,
                                                 componentPath,
                                                 initialViewModel,
                                                 componentContext,
                                                 _shouldProcessUpdatesSynchronously,
                                                 deferRender);

    _resourceManager->preloadForComponentPath(componentPath);

    return context;
}

SharedViewNodeTree Runtime::createViewNodeTreeAndContext(const Ref<ViewManagerContext>& viewManagerContext,
                                                         const StringBox& path) {
    return createViewNodeTreeAndContext(viewManagerContext, path, nullptr);
}

SharedViewNodeTree Runtime::createViewNodeTreeAndContext(const Ref<ViewManagerContext>& viewManagerContext,
                                                         const StringBox& path,
                                                         const Shared<ValueConvertible>& initialViewModel) {
    return createViewNodeTreeAndContext(viewManagerContext, path, initialViewModel, nullptr);
}

SharedViewNodeTree Runtime::createViewNodeTreeAndContext(const Ref<ViewManagerContext>& viewManagerContext,
                                                         const StringBox& path,
                                                         const Shared<ValueConvertible>& initialViewModel,
                                                         const Shared<ValueConvertible>& componentContext) {
    auto context = createContext(viewManagerContext, path, initialViewModel, componentContext);
    context->onCreate();
    return createViewNodeTree(context);
}

bool Runtime::hasViewNodeTreeForContext(const SharedContext& context) const {
    return _viewNodeManager.getViewNodeTreeForContextId(context->getContextId()) != nullptr;
}

SharedViewNodeTree Runtime::createViewNodeTree(const SharedContext& context,
                                               ViewNodeTreeThreadAffinity threadAffinity) {
    return _viewNodeManager.createViewNodeTreeForContext(context, threadAffinity);
}

SharedViewNodeTree Runtime::getOrCreateViewNodeTreeForContextId(ContextId contextId) {
    auto tree = _viewNodeManager.getViewNodeTreeForContextId(contextId);
    if (tree != nullptr) {
        return tree;
    }

    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return nullptr;
    }

    // Use the thread-safe getOrCreateViewNodeTreeForContext which holds the lock
    // during both the existence check and creation to avoid TOCTOU race conditions.
    return _viewNodeManager.getOrCreateViewNodeTreeForContext(context, ViewNodeTreeThreadAffinity::MAIN_THREAD);
}

void Runtime::destroyContext(const SharedContext& context) {
    // Destroying the context will release the Runtime ref.
    // If the Context is the last one holding a reference on the Runtime,
    // we want to make sure the Runtime will stay alive until
    // this function is called.
    auto strongRef = Valdi::strongRef(this);

    if (_mainThreadManager->currentThreadIsMainThread()) {
        doDestroyContext(context);
    } else {
        _mainThreadManager->dispatch(context, [=]() { strongRef->doDestroyContext(context); });
    }
}

void Runtime::doDestroyContext(const SharedContext& context) {
    ScopedMetrics metrics =
        Metrics::scopedDestroyContextLatency(getMetrics(), context->getPath().getResourceId().bundleName);

    _contextManager.destroyContext(context);
    auto viewNodeTree = _viewNodeManager.getViewNodeTreeForContextId(context->getContextId());
    if (viewNodeTree != nullptr) {
        destroyViewNodeTree(*viewNodeTree);
    }
}

void Runtime::destroyViewNodeTreeWithId(ContextId contextId) {
    auto viewNodeTree = _viewNodeManager.getViewNodeTreeForContextId(contextId);
    if (viewNodeTree != nullptr) {
        destroyViewNodeTree(*viewNodeTree);
    }
}

void appendAllViews(ViewNode* viewNode, std::vector<Ref<View>>& views) {
    if (viewNode->hasView()) {
        views.emplace_back(viewNode->getView());
    }

    for (auto* child : *viewNode) {
        appendAllViews(child, views);
    }
}

void Runtime::destroyViewNodeTree(ViewNodeTree& viewNodeTree) {
    _viewNodeManager.removeViewNodeTree(viewNodeTree);

    viewNodeTree.scheduleExclusiveUpdate([&]() {
        auto rootViewNode = viewNodeTree.getRootViewNode();
        if (rootViewNode != nullptr) {
            viewNodeTree.removeViewNode(rootViewNode->getRawId());
        }

        viewNodeTree.clear();
    });
}

void Runtime::updateAttributeState(ViewNode& viewNode, const StringBox& attributeName, const Value& attributeValue) {
    auto attributeId = _attributeIds.getIdForName(attributeName);

    viewNode.valueChanged(attributeId, attributeValue, false);
}

DumpedLogs Runtime::dumpContextsLogs() const {
    DumpedLogs logs;

    ValueArrayBuilder builder;

    auto contexts = _contextManager.getAllContexts();
    std::sort(contexts.begin(), contexts.end(), [](const auto& left, const auto& right) -> bool {
        return left->getContextId() < right->getContextId();
    });

    for (const auto& context : contexts) {
        if (!context->getPath().isEmpty()) {
            builder.append(Value(context->getPath().toString()));
        }
    }

    logs.appendMetadata(STRING_LITERAL("ComponentPaths"), Value(builder.build()).toStringBox());

    return logs;
}

DumpedLogs Runtime::dumpLogs(bool includeMetadata, bool includeVerbose, bool isCrashing) const {
    std::future<Result<DumpedLogs>> jsDump;
    if (_javaScriptRuntime != nullptr && !isCrashing) {
        jsDump = _javaScriptRuntime->dumpLogs(includeMetadata, includeVerbose);
    }

    DumpedLogs logs;

    if (includeMetadata) {
        logs = dumpContextsLogs();
    }

    if (jsDump.valid()) {
        auto status = jsDump.wait_for(std::chrono::seconds(2));

        if (status == std::future_status::ready) {
            auto result = jsDump.get();
            if (result) {
                logs.append(result.moveValue());
            } else {
                logs.appendVerbose(STRING_LITERAL("JS logs"),
                                   Value(STRING_FORMAT("<Dumping data failed: {}>", result.error())));
            }
        } else {
            logs.appendVerbose(STRING_LITERAL("JS logs"), Value(STRING_LITERAL("<Dumping data timed out>")));
        }
    }

    return logs;
}

std::string Runtime::dumpVerboseLogs() const {
    return dumpLogs(false, true, false).verboseLogsToString();
}

void Runtime::receivedUpdatedResources(const std::vector<Shared<Resource>>& resources) {
    updateResources(resources);
}

void Runtime::updateResource(const BytesView& resource,
                             const StringBox& bundleName,
                             const StringBox& filePathWithinBundle) {
    std::vector<Shared<Resource>> resources;
    resources.emplace_back(Valdi::makeShared<Resource>(ResourceId(bundleName, filePathWithinBundle), resource));
    updateResources(resources);
}

void Runtime::updateResources(const std::vector<Shared<Resource>>& resources) {
    runWithExclusiveJsThreadLock([this, resources]() {
        snap::utils::time::StopWatch sw;
        sw.start();
        VALDI_INFO(*_logger, "Hot reloading {} resources", resources.size());

        if (Valdi::traceReloaderPerformance) {
            VALDI_INFO(*_logger, "Runtime::updateResources started");
        }

        std::vector<ResourceId> unloadedJsFiles;

        for (const auto& resource : resources) {
            const auto& bundleName = resource->resourceId.bundleName;
            const auto& filePathWithinBundle = resource->resourceId.resourcePath;

            if (Valdi::traceReloaderPerformance) {
                VALDI_INFO(*_logger, "Updating resource {}:{} at {}", bundleName, filePathWithinBundle, sw.elapsed());
            }

            auto bundle = getResourceManager().getBundle(bundleName);

            if (filePathWithinBundle.hasSuffix(".valdicss")) {
                auto document = CSSDocument::parse(
                    resource->resourceId, resource->data.data(), resource->data.size(), _attributeIds);
                if (!document) {
                    VALDI_ERROR(*_logger,
                                "Unable to parse CSS document '{}' in bundle '{}' for reloading: {}",
                                filePathWithinBundle,
                                bundleName,
                                document.error());
                    continue;
                }
                bundle->setCSSDocument(filePathWithinBundle, document.value());
            } else if (filePathWithinBundle.hasSuffix(".js")) {
                auto jsPath = filePathWithinBundle.substring(0, filePathWithinBundle.length() - 3);

                bundle->setJs(jsPath, JavaScriptFile(resource->data, StringBox::emptyString()));

                unloadedJsFiles.emplace_back(bundleName, jsPath);

            } else if (filePathWithinBundle.hasSuffix(".assetcatalog")) {
                auto assetCatalogPath = filePathWithinBundle.substring(0, filePathWithinBundle.length() - 13);

                auto assetCatalogResult = AssetCatalog::parse(resource->data.data(), resource->data.size());

                if (!assetCatalogResult) {
                    VALDI_ERROR(*_logger,
                                "Failed to parse AssetCatalog '{}' in bundle '{}': {}",
                                filePathWithinBundle,
                                bundleName,
                                assetCatalogResult.error());
                    continue;
                }

                bundle->setAssetCatalog(assetCatalogPath, assetCatalogResult.value());
                _resourceManager->onAssetCatalogChanged(bundle);
            } else if (filePathWithinBundle.hasSuffix(".protodecl")) {
                bundle->setResourceContent(filePathWithinBundle, BundleResourceContent(resource->data));
            } else if (filePathWithinBundle.hasSuffix(".png") || filePathWithinBundle.hasSuffix(".webp")) {
                _resourceManager->insertImageAssetInBundle(bundle, filePathWithinBundle, resource->data);
            } else if (filePathWithinBundle.hasSuffix(".assetpackage")) {
                _resourceManager->insertAssetPackageInBundle(bundle, resource->data);
            } else {
                if (!filePathWithinBundle.hasSuffix(".json")) {
                    VALDI_WARN(
                        *_logger,
                        "Unknown file extension for reloaded resource '{}' in bundle '{}', treating as 'plain' resource"
                        "resource content",
                        filePathWithinBundle,
                        bundleName);
                }
                bundle->setResourceContent(filePathWithinBundle, BundleResourceContent(resource->data));
            }

            bundle->setEntry(filePathWithinBundle, resource->data);

            if (Valdi::traceReloaderPerformance) {
                VALDI_INFO(*_logger, "Updated {}:{} at {}", bundleName, filePathWithinBundle, sw.elapsed());
            }
        }

        _javaScriptRuntime->unloadModulesAndDependentModules(unloadedJsFiles, true);
        _hotReloadSequence++;
    });
}

void Runtime::onViewNodeTreeLayoutBecameDirty(ViewNodeTree& viewNodeTree) {
    if (_listener != nullptr) {
        _listener->onContextLayoutBecameDirty(*this, viewNodeTree.getContext());
    }
}

void Runtime::runWithExclusiveJsThreadLock(DispatchFunction&& cb) {
    if (_didInit) {
        _javaScriptRuntime->dispatchOnJsThreadAsync(nullptr, [cb = std::move(cb)](auto& /*jsEntry*/) { cb(); });
    } else {
        _mainThreadManager->dispatch(nullptr, std::move(cb));
    }
}

bool Runtime::disablePersistentStoreEncryption() {
    const auto& runtimeTweaks = getRuntimeTweaks();
    if (runtimeTweaks == NULL) {
        return false;
    }

    return runtimeTweaks->disablePersistentStoreEncryption();
}

void Runtime::daemonClientConnected(const Shared<IDaemonClient>& daemonClient) {
    if (_javaScriptRuntime != nullptr) {
        _javaScriptRuntime->daemonClientConnected(daemonClient);
    }
}

void Runtime::daemonClientDisconnected(const Shared<IDaemonClient>& daemonClient) {
    if (_javaScriptRuntime != nullptr) {
        _javaScriptRuntime->daemonClientDisconnected(daemonClient);
    }
}

void Runtime::daemonClientDidReceiveClientPayload(const Shared<IDaemonClient>& daemonClient,
                                                  int senderClientId,
                                                  const BytesView& payload) {
    if (_javaScriptRuntime != nullptr) {
        _javaScriptRuntime->daemonClientDidReceiveClientPayload(daemonClient, senderClientId, payload);
    }
}

void Runtime::setAutoRenderDisabled(bool autoRenderDisabled) {
    auto oldValue = _autoRenderDisabled.exchange(autoRenderDisabled);

    if (!autoRenderDisabled && oldValue) {
        for (const auto& context : _contextManager.getAllContexts()) {
            if (context->hasPendingRenderRequests()) {
                getMainThreadManager().dispatch(context, [context]() { context->flushRenderRequests(); });
            }
        }
    }
}

void Runtime::receivedRenderRequest(const Ref<RenderRequest>& renderRequest) {
    auto context = _contextManager.getContext(renderRequest->getContextId());
    if (context == nullptr) {
        return;
    }

    auto taskIdOptional = context->enqueueRenderRequest(renderRequest);

    if (taskIdOptional && !_autoRenderDisabled) {
        getMainThreadManager().dispatch(
            context, [context, taskId = taskIdOptional.value()]() { context->runRenderRequest(taskId); });
    }
}

void Runtime::processRenderRequest(const Ref<RenderRequest>& rawRenderRequest) {
    snap::utils::time::StopWatch sw;
    sw.start();

    if (_debugRequests) {
        VALDI_INFO(*_logger,
                   "Rendering context {} with render request: {}",
                   rawRenderRequest->getContextId(),
                   rawRenderRequest->serialize(_attributeIds).toString(true));
    }
    auto viewNodeTree = getOrCreateViewNodeTreeForContextId(rawRenderRequest->getContextId());
    if (viewNodeTree == nullptr) {
        VALDI_INFO(*_logger, "Ignoring render for invalid or destroyed context {}", rawRenderRequest->getContextId());
        return;
    }

    viewNodeTree->scheduleExclusiveUpdate(
        [=]() {
            VALDI_TRACE("Valdi.processRenderRequest");

            ViewNodeRenderer renderer(*viewNodeTree, viewNodeTree->getContext()->getLogger(), _limitToViewportDisabled);

            renderer.render(*rawRenderRequest);

            if (Valdi::traceRenderingPerformance) {
                VALDI_INFO(
                    *_logger, "Finished rendering {} in {}", viewNodeTree->getContext()->getPath(), sw.elapsed());
            }

            if (rawRenderRequest->getEntriesSize() >= Valdi::kEmitProcessRequestLatencyEntriesThreshold) {
                const auto& metrics = getMetrics();
                if (metrics != nullptr) {
                    metrics->emitProcessRequestLatency(viewNodeTree->getContext()->getPath().getResourceId().bundleName,
                                                       sw.elapsed());
                }
            }

            viewNodeTree->getContext()->onRendered();
        },
        [=]() {
            if (_listener != nullptr) {
                _listener->onContextRendered(*this, viewNodeTree->getContext());
            }
        });
}

void Runtime::receivedCallActionMessage(const ContextId& contextId,
                                        const StringBox& actionName,
                                        const Ref<ValueArray>& parameters) {
    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        VALDI_WARN(*_logger, "Cannot call action: Context {} was already destroyed.", contextId);
        return;
    }
    auto viewNodeTree = _viewNodeManager.getViewNodeTreeForContextId(context->getContextId());
    if (viewNodeTree == nullptr) {
        VALDI_WARN(*_logger, "Cannot call action: no ViewNodeTree associated with context {}", context->getContextId());
        return;
    }

    context->getViewManagerContext()->getViewManager().callAction(viewNodeTree.get(), actionName, parameters);
}

void Runtime::resolveViewNodeTree(const SharedContext& context,
                                  bool inMainThread,
                                  bool createIfNeeded,
                                  Function<void(const SharedViewNodeTree&)>&& function) {
    Ref<ViewNodeTree> viewNodeTree;
    if (createIfNeeded) {
        viewNodeTree =
            _viewNodeManager.getOrCreateViewNodeTreeForContext(context, ViewNodeTreeThreadAffinity::MAIN_THREAD);
    } else {
        viewNodeTree = _viewNodeManager.getViewNodeTreeForContextId(context->getContextId());
    }

    DispatchFunction dispatchFn = [viewNodeTree, function = std::move(function)]() {
        auto lock = viewNodeTree != nullptr ? viewNodeTree->lock() : TrackedLock();
        function(viewNodeTree);
    };

    if (inMainThread && viewNodeTree->shouldRenderInMainThread()) {
        _mainThreadManager->dispatch(context, std::move(dispatchFn));
    } else {
        dispatchFn();
    }
}

void Runtime::registerNativeModuleFactory(const Shared<snap::valdi_core::ModuleFactory>& moduleFactory) {
    registerJavaScriptModuleFactory(makeShared<JavaScriptModuleFactoryBridge>(moduleFactory));
}

void Runtime::registerTypeConverter(const StringBox& className, const StringBox& functionPath) {
    _javaScriptRuntime->registerTypeConverter(className, functionPath);
}

void Runtime::setRuntimeTweaks(const Ref<ValdiRuntimeTweaks>& runtimeTweaks) {
    _resourceManager->setRuntimeTweaks(runtimeTweaks);
}

void Runtime::setMetrics(const Ref<Metrics>& metrics) {
    _resourceManager->setMetrics(metrics);
}

const Ref<Metrics>& Runtime::getMetrics() const {
    return _resourceManager->getMetrics();
}

Ref<ValdiRuntimeTweaks> Runtime::getRuntimeTweaks() {
    return _resourceManager->getRuntimeTweaks();
}

void Runtime::registerJavaScriptModuleFactory(const Ref<JavaScriptModuleFactory>& moduleFactory) {
    _javaScriptRuntime->registerJavaScriptModuleFactory(moduleFactory);
}

void Runtime::updateColorPalette(const Value& colorPaletteMap) {
    if (colorPaletteMap.isMap()) {
        FlatMap<StringBox, Color> colors;
        for (const auto& it : *colorPaletteMap.getMap()) {
            auto colorResult = ValueConverter::toColor(*_colorPalette, it.second);
            if (!colorResult) {
                VALDI_ERROR(*_logger, "Failed to parse color '{}': {}", it.first, colorResult.error());
                continue;
            }

            colors[it.first] = colorResult.value();
        }

        _colorPalette->updateColors(colors);
    }
}

Value Runtime::getColorPalette() {
    auto valueMap = Valdi::makeShared<Valdi::ValueMap>();
    for (const auto& [name, color] : _colorPalette->getColors()) {
        (*valueMap)[name] = Valdi::Value(color.value);
    }

    return Valdi::Value(std::move(valueMap));
}

void Runtime::onUncaughtJsError(const StringBox& moduleName, const Error& error) {
    if (_runtimeMessageHandler != nullptr) {
        auto flattenedError = error.flatten();
        _runtimeMessageHandler->onUncaughtJsError(error.getErrorCode(),
                                                  moduleName,
                                                  flattenedError.getMessage().slowToString(),
                                                  flattenedError.getStack().slowToString());
    }
}

void Runtime::onDebugMessage(int32_t level, const StringBox& message) {
    if (_runtimeMessageHandler != nullptr) {
        _runtimeMessageHandler->onDebugMessage(level, message.slowToString());
    }
}

void Runtime::onContextCreated(const SharedContext& context) {
    if (_listener != nullptr) {
        _listener->onContextCreated(*this, context);
    }
}

void Runtime::onContextDestroyed(Context& context) {
    if (_listener != nullptr) {
        _listener->onContextDestroyed(*this, context);
        if (_contextManager.getContextsSize() == 0) {
            _listener->onAllContextsDestroyed(*this);
        }
    }
}

ResourceManager& Runtime::getResourceManager() {
    return *_resourceManager;
}

JavaScriptRuntime* Runtime::getJavaScriptRuntime() {
    return _javaScriptRuntime.get();
}

void Runtime::setListener(const std::shared_ptr<IRuntimeListener>& listener) {
    _listener = listener;
}

const std::shared_ptr<IRuntimeListener>& Runtime::getListener() const {
    return _listener;
}

MainThreadManager& Runtime::getMainThreadManager() {
    return *_mainThreadManager;
}

const ContextManager& Runtime::getContextManager() const {
    return _contextManager;
}

ContextManager& Runtime::getContextManager() {
    return _contextManager;
}

const ViewNodeTreeManager& Runtime::getViewNodeTreeManager() const {
    return _viewNodeManager;
}

void Runtime::setTraceViewLifecycle(bool traceViewLifecycle) {
    _traceViewLifecycle = traceViewLifecycle;
}

void Runtime::setDebugRequests(bool debugRequests) {
    _debugRequests = debugRequests;
}

void Runtime::setLimitToViewportDisabled(bool limitToViewportDisabled) {
    _limitToViewportDisabled = limitToViewportDisabled;
}

bool Runtime::limitToViewportDisabled() const {
    return _limitToViewportDisabled;
}

const Ref<DispatchQueue>& Runtime::getWorkerQueue() const {
    return _workerQueue;
}

ILogger& Runtime::getLogger() const {
    return *_logger;
}

AttributeIds& Runtime::getAttributeIds() const {
    return _attributeIds;
}

int Runtime::getHotReloadSequence() const {
    return _hotReloadSequence;
}

bool Runtime::debuggerServiceEnabled() const {
    return _debuggerServiceEnabled;
}

void Runtime::setRuntimeMessageHandler(const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler) {
    _runtimeMessageHandler = runtimeMessageHandler;
}

void Runtime::setShouldProcessUpdatesSynchronously(bool shouldProcessUpdatesSynchronously) {
    _shouldProcessUpdatesSynchronously = shouldProcessUpdatesSynchronously;
    for (const auto& context : _contextManager.getAllContexts()) {
        context->setUpdateHandlerSynchronously(shouldProcessUpdatesSynchronously);
    }
}

void Runtime::emitInitMetrics() {
    runWithExclusiveJsThreadLock([this]() {
        auto initStopWatch = std::move(_initStopWatch);
        auto metrics = getMetrics();
        if (metrics != nullptr && initStopWatch != nullptr) {
            metrics->emitRuntimeInitLatency(initStopWatch->elapsed());
        }
    });
}

const Ref<IDiskCache>& Runtime::getDiskCache() const {
    return _diskCache;
}

Shared<snap::valdi_core::HTTPRequestManager> Runtime::getRequestManager() const {
    return _requestManager.get();
}

Ref<Runtime> Runtime::currentRuntime() {
    auto* context = Context::current();
    if (context == nullptr) {
        return nullptr;
    }

    return Ref(context->getRuntime());
}

} // namespace Valdi
