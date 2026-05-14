//
//  RuntimeManager.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 1/8/19.
//

#pragma once

#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Resources/UserSession.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/Utils/BridgeLogger.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/Holder.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include <atomic>
#include <vector>

struct YGConfig;

namespace snap::valdi {
class HTTPRequestManager;
class Keychain;
class RuntimeMessageHandler;
} // namespace snap::valdi

namespace snap::valdi_core {
class ModuleFactoriesProvider;
}

namespace Valdi {

class ViewPreloader;
class DispatchQueue;
class IDiskCache;
class IResourceLoader;
class DebuggerService;
class GlobalViewFactories;
class ViewManagerContext;
class AssetLoaderManager;
class BytesAssetLoader;
class ITweakValueProvider;
class AttributionResolver;
class Metrics;
class JavaScriptANRDetector;
class MetricsStopWatch;
class ValdiRuntimeTweaks;

struct RegisteredTypeConverter {
    StringBox className;
    StringBox functionPath;
};

class IRuntimeManagerListener : public SimpleRefCountable {
public:
    virtual void onRuntimeCreated(Runtime& runtime) = 0;
};

class RuntimeManager : public ValdiObject, protected ColorPaletteListener {
public:
    RuntimeManager(const Ref<IMainThreadDispatcher>& mainThreadDispatcher,
                   IJavaScriptBridge* jsBridge,
                   const Ref<IDiskCache>& diskCache,
                   Shared<snap::valdi::Keychain> keychain,
                   const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
                   PlatformType platformType,
                   ThreadQoSClass jsThreadQoS,
                   const Ref<ILogger>& logger,
                   bool enableDebuggerService,
                   bool disableHotReloader,
                   bool isStandalone);
    ~RuntimeManager() override;

    void postInit();
    void fullTeardown();

    SharedRuntime createRuntime(const Shared<IResourceLoader>& resourceLoader,
                                double displayScale,
                                const Ref<MainThreadManager>& mainThreadManagerOverride = nullptr,
                                std::optional<PlatformType> platformType = std::nullopt);
    Ref<ViewManagerContext> createViewManagerContext(IViewManager& viewManager,
                                                     bool enablePreloading,
                                                     const Ref<MainThreadManager>& mainThreadManagerOverride = nullptr);

    std::vector<SharedRuntime> getAllRuntimes();

    void attachHotReloader(const Ref<Runtime>& runtime);

    /**
     * Clears all the view pools, which effectively frees up all the retained views.
     */
    void clearViewPools();

    void forceBindAttributes(const StringBox& className);

    const Ref<DispatchQueue>& getWorkerQueue() const;

    void setRequestManager(const Shared<snap::valdi_core::HTTPRequestManager>& requestManager);

    const Ref<AssetLoaderManager>& getAssetLoaderManager() const;

    void registerBytesAssetLoader(const Ref<IDiskCache>& diskCache);

    void applicationWillPause();
    void applicationDidResume();
    void applicationIsLowInMemory();
    void applicationWillTerminate();

    bool debuggerServiceEnabled() const;

    void setUserSession(const StringBox& userId);
    void setApplicationId(const StringBox& applicationId);

    void setDisableRuntimeAutoInit(bool disableRuntimeAutoInit);

    void registerModuleFactoriesProvider(
        const Shared<snap::valdi_core::ModuleFactoriesProvider>& moduleFactoriesProvider);

    void registerTypeConverter(const StringBox& className, const StringBox& functionPath);

    MainThreadManager& getMainThreadManager();
    ILogger& getLogger() const;

    void setLogListener(Weak<BridgeLoggerListener> listener);

    const Ref<IDiskCache>& getDiskCache() const;
    const Holder<Ref<UserSession>>& getUserSession() const;

    void setTweakValueProvider(const Shared<ITweakValueProvider>& tweakValueProvider);

    JavaScriptContextMemoryStatistics dumpMemoryStatistics();

    void setJsThreadQoS(ThreadQoSClass jsThreadQoS);

    void setAttributionResolver(const Ref<AttributionResolver>& attributionResolver);

    void setKeepDebuggerServiceOnPause(bool keepDebuggerServiceOnPause);

    void addListener(const Ref<IRuntimeManagerListener>& listener);
    void removeListener(const Ref<IRuntimeManagerListener>& listener);

    void enqueueLoadOperation(Valdi::DispatchFunction&& loadOperation);
    void flushLoadOperations();

    void emitInitMetrics();
    void emitXpatCreateRuntimeMetrics();
    void emitIosRuntimeCreateMetrics();
    void emitUserSessionReadyMetrics();

    void setMetrics(const Ref<Metrics>& metrics);

    PlatformType getPlatformType() const;

    const Ref<JavaScriptANRDetector>& getANRDetector() const;

    VALDI_CLASS_HEADER(RuntimeManager)

protected:
    void onColorPaletteUpdated(const ColorPalette& colorPalette) override;

private:
    std::shared_ptr<MetricsStopWatch> _initStopWatch;
    std::shared_ptr<YGConfig> _yogaConfig;
    Shared<DebuggerService> _debuggerService;
    AttributeIds _attributeIds;

    task_id_t _deferredGCTask;

    Ref<MainThreadManager> _mainThreadManager;
    Ref<AssetLoaderManager> _assetLoaderManager;
    Ref<ILogger> _innerLogger;
    Ref<BridgeLogger> _logger;
    IJavaScriptBridge* _jsBridge;

    Ref<DispatchQueue> _workerQueue;
    Ref<IDiskCache> _diskCache;
    Shared<snap::valdi::Keychain> _keychain;
    Shared<snap::valdi::RuntimeMessageHandler> _runtimeMessageHandler;
    Ref<BytesAssetLoader> _bytesAssetLoader;

    mutable Mutex _mutex;
    Holder<Shared<snap::valdi_core::HTTPRequestManager>> _requestManager;

    std::vector<Weak<Runtime>> _runtimes;
    std::vector<Ref<ViewManagerContext>> _viewManagerContexts;
    std::vector<Shared<snap::valdi_core::ModuleFactory>> _registeredModuleFactories;
    std::vector<RegisteredTypeConverter> _registeredTypeConverters;
    std::vector<Ref<IRuntimeManagerListener>> _listeners;

    Ref<ColorPalette> _colorPalette;
    Ref<AttributionResolver> _attributionResolver;
    Holder<Ref<UserSession>> _userSession;
    Ref<ValdiRuntimeTweaks> _runtimeTweaks;
    Ref<Metrics> _metrics;
    Ref<JavaScriptANRDetector> _anrDetector;
    PlatformType _platformType;
    ThreadQoSClass _jsThreadQoS;
    bool _disableRuntimeAutoInit = false;
    bool _keepDebuggerServiceOnPause = false;
    bool _debuggerServiceEnabled = false;
    int _loadOperationsCount = 0;

    void removeExpiredRuntimes();

    void startDebuggerServices();
    void stopDebuggerServices();

    std::vector<SharedRuntime> getAllRuntimes(std::lock_guard<Mutex>& lock);

    void cancelDeferredGCTask();
    void scheduleDeferredGCTask();

    task_id_t swapDeferredGCTask(task_id_t task);

    void updateLoadOperationsCount(int increment);

    using MetricsDuration = snap::utils::time::Duration<std::chrono::steady_clock>;

    void emitMetrics(void (Metrics::*emitterFunc)(const MetricsDuration&));
};

} // namespace Valdi
