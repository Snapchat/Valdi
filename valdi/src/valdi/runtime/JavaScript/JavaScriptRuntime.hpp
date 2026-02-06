//
//  JavaScriptRuntime.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/28/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"

#include "valdi_core/cpp/Context/ContextId.hpp"

#include "valdi/runtime/Context/RawViewNodeId.hpp"

#include "valdi/runtime/Resources/Bundle.hpp"

#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi/runtime/Utils/DumpedLogs.hpp"

#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "valdi_core/cpp/Resources/ResourceId.hpp"

#include "valdi_core/AnimationType.hpp"
#include "valdi_core/JSRuntime.hpp"

#include "valdi/runtime/JavaScript/JSPropertyNameIndex.hpp"
#include "valdi/runtime/JavaScript/JavaScriptComponentContextHandler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptStringCache.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi_core/cpp/JavaScript/JavaScriptPathResolver.hpp"

#include "utils/time/Duration.hpp"
#include "utils/time/StopWatch.hpp"

#include "valdi_core/cpp/Utils/Function.hpp"
#include <future>
#include <tuple>
#include <utility>
#include <vector>

namespace Valdi {

class JavaScriptComponentContextHandler;
class JavaScriptModuleContainer;
class JavaScriptModule;
class JavaScriptModuleFactory;
class RenderRequest;
class ViewNodeTree;
class ContextManager;
class Context;
class ResourceManager;
class ContextComponentRenderer;
class ViewNode;
class JavaScriptRuntimeDeserializers;
class IDiskCache;
class AttributeIds;
class StyleAttributesCache;
class IDaemonClient;
class ViewManagerContext;
class AsyncGroup;
class Marshaller;
class TimePoint;
class JavaScriptANRDetector;

struct AnimationOptions;

enum DaemonClientEventType {
    DaemonClientEventTypeConnected = 1,
    DaemonClientEventTypeDisconnected = 2,
    DaemonClientEventTypeReceivedClientPayload = 3,
};

class ValdiRuntimeTweaks;

class JavaScriptRuntime;
class IJavaScriptRuntimeListener {
public:
    IJavaScriptRuntimeListener() = default;
    virtual ~IJavaScriptRuntimeListener() = default;

    virtual void receivedRenderRequest(const Ref<RenderRequest>& rawRenderRequest) = 0;

    virtual void receivedCallActionMessage(const ContextId& contextId,
                                           const StringBox& actionName,
                                           const Ref<ValueArray>& parameters) = 0;

    virtual void resolveViewNodeTree(const Ref<Context>& context,
                                     bool inMainThread,
                                     bool createIfNeeded,
                                     Function<void(const Ref<ViewNodeTree>&)>&& function) = 0;

    virtual void updateColorPalette(const Value& colorPaletteMap) = 0;

    virtual void onUncaughtJsError(const StringBox& moduleName, const Error& error) = 0;

    virtual void onDebugMessage(int32_t level, const StringBox& message) = 0;

    virtual Ref<ValdiRuntimeTweaks> getRuntimeTweaks() = 0;
};

class JavaScriptStacktraceCaptureSession : public SimpleRefCountable {
public:
    JavaScriptStacktraceCaptureSession();

    ~JavaScriptStacktraceCaptureSession() override;

    void setCapturedStacktrace(const JavaScriptCapturedStacktrace& capturedStacktrace);

    bool finishedCapture() const;

    std::optional<JavaScriptCapturedStacktrace> getCapturedStacktrace() const;

private:
    mutable Mutex _mutex;
    std::optional<JavaScriptCapturedStacktrace> _capturedStacktrace;
};

enum class ModuleLoadMode {
    /**
     * Module is loaded from JS source code
     */
    JS_SOURCE,

    /**
     * Module is loaded from JS bytecode that has been
     * compiled from JS source ahead of time
     */
    JS_BYTECODE,

    /**
     * Module is loaded from TS that has been natively
     * compiled with TSN
     */
    NATIVE,
};

using ModuleLoadResult = std::pair<JSValueRef, ModuleLoadMode>;

/**
Module resource consumption info, i.e. memory and duration;
Currently, we monitor resource consumption only if the memory usage is available.
*/
struct ModuleResourceConsumptionInfo {
    int64_t memoryWaterMark;     // The memory usage water mark before loading
    int64_t childrenMemoryUsage; // Total memory usage by children of this module

    int64_t timestamp;        // Timestamp before loading
    int64_t childrenDuration; // Total duration of children of this module
};

/**
 * Handles the JS logic. Implements JS module loading, using same principles as node.js, and exposes base runtime
 * methods. Compiled documents use the runtime methods to communicate with Valdi, for example to ask a new render
 * while providing the updated attributes to apply to the Valdi's context view tree.
 */
class JavaScriptRuntime : public JavaScriptTaskScheduler,
                          public IJavaScriptContextListener,
                          public snap::valdi_core::JSRuntime,
                          public JavaScriptComponentContextHandlerListener {
public:
    JavaScriptRuntime(IJavaScriptBridge& jsBridge,
                      ResourceManager& resourceManager,
                      ContextManager& contextManager,
                      MainThreadManager& mainThreadManager,
                      AttributeIds& attributeIds,
                      PlatformType platformType,
                      bool enableDebugger,
                      ThreadQoSClass threadQoS,
                      const Ref<JavaScriptANRDetector>& anrDetector,
                      const Ref<ILogger>& logger,
                      bool isWorker = false);
    ~JavaScriptRuntime() override;

    void postInit();

    void setListener(IJavaScriptRuntimeListener* listener);
    IJavaScriptRuntimeListener* getListener() const;

    void fullTeardown();
    void partialTeardown();

    void setThreadQoS(ThreadQoSClass threadQoS);

    Ref<ContextHandler> getContextHandler() const;

    bool callComponentFunction(ContextId contextId,
                               const StringBox& functionName,
                               const Ref<ValueArray>& additionalParameters);

    void callComponentFunction(const Ref<Context>& context, const StringBox& functionName);

    void callComponentFunction(const Ref<Context>& context,
                               const StringBox& functionName,
                               const Ref<ValueArray>& additionalParameters);

    void callModuleFunction(const StringBox& module, const StringBox& functionName, const Ref<ValueArray>& parameters);

    const Ref<DispatchQueue>& getJsDispatchQueue() const;

    void daemonClientConnected(const Shared<IDaemonClient>& daemonClient);
    void daemonClientDisconnected(const Shared<IDaemonClient>& daemonClient);
    void daemonClientDidReceiveClientPayload(const Shared<IDaemonClient>& daemonClient,
                                             int senderClientId,
                                             const BytesView& payload);

    void registerJavaScriptModuleFactory(const Ref<JavaScriptModuleFactory>& moduleFactory);
    void registerTypeConverter(const StringBox& typeName, const StringBox& functionPath);

    void setDefaultViewManagerContext(const Ref<ViewManagerContext>& viewManagerContext);

    void unloadAllModules();
    void unloadUnusedModules(DispatchFunction completion);
    void unloadUnusedModules(const Ref<AsyncGroup>& completionGroup);

    void unloadModulesAndDependentModules(const std::vector<ResourceId>& resourceIds, bool isHotReloading);

    MainThreadManager* getMainThreadManager() const override;

    void onUnhandledRejectedPromise(IJavaScriptContext& jsContext, const JSValue& promiseResult) override;
    void onInterrupt(IJavaScriptContext& jsContext) final;

    void dispatchOnJsThread(Ref<Context> ownerContext,
                            JavaScriptTaskScheduleType scheduleType,
                            uint32_t delayMs,
                            JavaScriptThreadTask&& function) final;
    void dispatchSynchronouslyOnJsThread(JavaScriptThreadTask&& function);
    bool isInJsThread() final;
    Ref<Context> getLastDispatchedContext() const final;

    void performGc();
    JavaScriptContextMemoryStatistics dumpMemoryStatistics();

    Result<Value> evaluateScript(const BytesView& script, const StringBox& sourceFilename);

    bool isJsModuleLoaded(const ResourceId& resourceId);

    Ref<JavaScriptModuleContainer> getModule(
        const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
        const Valdi::StringBox& path);

    void preloadModule(const StringBox& path, int32_t maxDepth) override;

    int32_t pushModuleToMarshaller(
        const /*not-null*/ std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
        const Valdi::StringBox& path,
        int64_t marshallerHandle) override;

    int32_t pushModuleToMarshaller(
        const /*not-null*/ std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
        const Valdi::StringBox& path,
        Marshaller& marshaller);

    void addModuleUnloadObserver(const Valdi::StringBox& path, const Valdi::Value& observer) override;

    std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager> createNativeObjectsManager(
        const std::string& scopeName) override;

    void destroyNativeObjectsManager(
        const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager) override;

    std::shared_ptr<snap::valdi_core::JSRuntime> createWorker() override;

    void runOnJsThread(const Value& runnable) override;

    JSValueRef symbolicateError(const JSValueRef& error) override;
    Ref<JSStackTraceProvider> captureCurrentStackTrace() override;

    void setValueToGlobalObject(const StringBox& name, const Value& value);
    [[nodiscard]] Result<Void> evalModuleSync(const StringBox& path, bool reevalOnReload);
    bool isInEvalMode() const;

    std::future<Result<DumpedLogs>> dumpLogs(bool includeMetadata, bool includeVerbose);

    void setForceStackTraceCapture(bool force);

    void requestUpdateJsContextHandler(JavaScriptEntryParameters& jsEntry,
                                       JavaScriptComponentContextHandler& handler) override;

    const Ref<Metrics>& getMetrics() const override;

    std::vector<JavaScriptCapturedStacktrace> captureStackTraces(std::chrono::steady_clock::duration timeout) override;

    /**
     Lock all of the JS Contexts, store them into the given vector, and call the onDone callback
     when all of the JSContext has been added to the vector.
     Exposed for tests only.
     */
    void lockAllJSContexts(std::vector<IJavaScriptContext*>& jsContexts, const DispatchFunction& onDone);

    void startProfiling();

    void stopProfiling(Function<void(const Result<std::vector<std::string>>&)> onComplete);

    ILogger& getLogger() const;

    TimePoint getPerformanceTimeOrigin() const;

    bool isDisposed() const override {
        return _isDisposed;
    }

    Result<BytesView> dumpHeap();

private:
    struct RegisteredTypeConverter {
        StringBox typeName;
        StringBox functionPath;
    };
    struct RawRenderRequest {
        Ref<Context> context;
        JSValueRef request;
        Ref<ValueFunction> callback;
    };

    Mutex _mutex;
    IJavaScriptBridge& _javaScriptBridge;
    Ref<IJavaScriptContext> _javaScriptContext;
    ResourceManager& _resourceManager;
    ContextManager& _contextManager;
    MainThreadManager& _mainThreadManager;
    AttributeIds& _attributeIds;
    [[maybe_unused]] Ref<ILogger> _logger;
    IJavaScriptRuntimeListener* _listener;

    FlatMap<ResourceId, Shared<JavaScriptModuleContainer>> _modules;
    Ref<JavaScriptComponentContextHandler> _contextHandler;

    std::unique_ptr<StyleAttributesCache> _styleAttributesCache;
    std::unique_ptr<JavaScriptRuntimeDeserializers> _runtimeDeserializers;
    Ref<JavaScriptANRDetector> _anrDetector;

    Result<JSValueRef> _symbolicateFunction;
    Result<JSValueRef> _onDaemonClientEventFunction;
    JSValueRef _moduleLoader;
    JSPropertyNameIndex<6> _propertyNameIndex;

    Ref<IDiskCache> _diskCache;
    // List of JS modules which should be reloaded whenever they are unloaded
    FlatSet<ResourceId> _modulesToAutoReload;
    // List of JS modules which were unloaded and need to be reloaded
    FlatSet<ResourceId> _pendingModulesToReload;

    FlatMap<int, JSValueRef> _daemonClients;

    JavaScriptStringCache _stringCache;

    Ref<DispatchQueue> _dispatchQueue;
    std::atomic<bool> _isDisposed;
    std::atomic<ContextId> _lastDispatchedContextId;
    // A lock that will block the JS thread until postInit() is called and the initialization has completed
    AsyncGroup _initLock;
    bool _hasGcScheduled = false;
    bool _symbolicating = false;
    bool _running = false;
    bool _enableDebugger;
    // Used for unit testing only
    std::atomic<bool> _forceStackTraceCapture = false;
    int _daemonClientListenerIdSequence = 0;

    PlatformType _platformType;
    Ref<ViewManagerContext> _defaultViewManagerContext;
    Ref<Context> _globalContext;
    std::vector<Ref<Context>> _contextsStack;
    std::vector<Ref<Context>> _contextsStash;
    snap::utils::time::StopWatch _upTime;

    std::vector<Ref<JavaScriptModuleFactory>> _moduleFactories;
    std::vector<RegisteredTypeConverter> _typeConverters;
    std::vector<Ref<JavaScriptStacktraceCaptureSession>> _stacktraceCaptureSessions;
    std::vector<Weak<JavaScriptRuntime>> _jsWorkers;

    Shared<JSValueRefHolder> _uncaughtExceptionHandler;
    Shared<JSValueRefHolder> _unhandledRejectionHandler;

    const bool _isWorker;

    std::vector<ModuleResourceConsumptionInfo> _moduleResourceTracker;

    void doInitialize();

    Result<Void> initializeContext();
    void buildContext(IJavaScriptContext& context,
                      const Ref<ValdiRuntimeTweaks>& tweaks,
                      JSExceptionTracker& exceptionTracker);

    void doUnloadModulesAndDependentModules(const std::vector<ResourceId>& resourceIds,
                                            bool isHotReloading,
                                            JavaScriptEntryParameters& entry);
    void onModulesUnloaded(const JSValue& value,
                           FlatSet<ResourceId>& allUnloadedModules,
                           JavaScriptEntryParameters& entry);

    JSValueRef doLog(LogType logType, JSFunctionNativeCallContext& callContext);

    void bindRuntimeFunction(IJavaScriptContext& context,
                             JSExceptionTracker& exceptionTracker,
                             const JSValueRef& jsObject,
                             const char* functionName,
                             JSValueRef (JavaScriptRuntime::*function)(JSFunctionNativeCallContext&));

    JSValueRef runtimeOutputLog(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimePostMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetFrameForViewId(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetNativeViewForViewId(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMakeOpaque(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeConfigureCallback(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimePerformSyncWithMainThread(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeCallOnMainThread(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetLayoutDebugInfo(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetViewNodeDebugInfo(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeIsModuleLoaded(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetModuleJsPaths(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetModuleEntry(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeLoadModule(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeLoadJsModule(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetSourceMap(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMakeAssetFromUrl(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMakeAssetFromBytes(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeAddAssetLoadObserver(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetAssets(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMakeDirectionalAsset(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMakePlatformSpecificAsset(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetLoadedAssetMetadata(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeSetColorPalette(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeTakeElementSnapshot(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetNativeNodeForElementId(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeBytesToString(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeOnMainThreadIdle(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeProtectNativeRefs(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeGetCurrentPlatform(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetBackendRenderingTypeForContextId(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeSubmitRenderRequest(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeGetCSSModule(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeCreateCSSRule(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeInternString(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetAttributeId(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeCreateContext(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeDestroyContext(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeSetLayoutSpecs(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMeasureContext(JSFunctionNativeCallContext& callContext);

    void callViewNodeActionCallback(const Shared<JSValueRefHolder>& callbackRef, const Value& result);

    JSValueRef handleViewNodeSpecificAction(JSFunctionNativeCallContext& callContext,
                                            Function<void(ViewNode&, Function<void(const Value&)>)>&& handleViewNode);

    JSValueRef handleViewNodeSpecificActionAsync(JSFunctionNativeCallContext& callContext,
                                                 void (*handleViewNode)(ViewNode&, Function<void(const Value&)>));

    JSValueRef handleViewNodeSpecificActionSync(JSFunctionNativeCallContext& callContext,
                                                Value (*handleViewNode)(ViewNode&));

    JSValueRef runtimeTrace(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeMakeTraceProxy(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeStartTraceRecording(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeStopTraceRecording(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeScheduleWorkItem(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeUnscheduleWorkItem(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeSubmitDebugMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeOnUncaughtError(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimePushCurrentContext(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimePopCurrentContext(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeGetCurrentContext(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeSaveCurrentContext(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeRestoreCurrentContext(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeDumpMemoryStatistics(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimePerformGC(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeHeapDump(JSFunctionNativeCallContext& callContext);

    JSValueRef runtimeSetUncaughtExceptionHandler(JSFunctionNativeCallContext& callContext);
    JSValueRef runtimeSetUnhandledRejectionHandler(JSFunctionNativeCallContext& callContext);

    JSValueRef performanceNow(JSFunctionNativeCallContext& callContext);

    JSValueRef queueMicrotask(JSFunctionNativeCallContext& callContext);

    // Worker methods
    // - new Worker(...)
    // - worker.onmessage = ...
    // - worker.postMessage(...)
    // - worker.terminate()
    JSValueRef runtimeCreateWorker(JSFunctionNativeCallContext& callContext);
    JSValueRef workerSetOnMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef workerPostMessage(JSFunctionNativeCallContext& callContext);
    JSValueRef workerTerminate(JSFunctionNativeCallContext& callContext);

    Shared<JavaScriptModuleContainer> importModule(const ResourceId& resourceId, JavaScriptEntryParameters& jsEntry);
    Shared<JavaScriptModuleContainer> importModule(const StringBox& path, JavaScriptEntryParameters& jsEntry);

    // The module will be lazily loaded the first time it is accessed
    Shared<JavaScriptModuleContainer> getValdiModule(JavaScriptEntryParameters& jsEntry);
    void ensureValdiModuleIsLoaded(JavaScriptEntryParameters& jsEntry);

    void onModuleLoaded(IJavaScriptContext& jsContext,
                        const ResourceId& resourceId,
                        const JSValue& value,
                        bool isFromJsEval,
                        JSExceptionTracker& exceptionTracker);
    void onModuleUnloaded(const ResourceId& resourceId, JavaScriptEntryParameters& jsEntry);

    FlatSet<ResourceId> getAllUsedModules() const;

    void reevalUnloadedModulesIfNeeded();

    std::vector<Ref<JavaScriptRuntime>> getAllWorkers();
    void dispatchPerformGcToWorkers();
    void performGcNow(IJavaScriptContext& jsContext);
    JavaScriptContextMemoryStatistics dumpMemoryStatistics(IJavaScriptContext& jsContext);

    [[nodiscard]] Result<Shared<JavaScriptModuleContainer>> loadModuleContent(
        const BytesView& content, const std::shared_ptr<JavaScriptModule>& jsModule, const Shared<Bundle>& bundle);

    bool unloadNextUnusedModule(JavaScriptEntryParameters& entry);

    void dispatchOnMainThread(DispatchFunction func);

    [[nodiscard]] static Error onModuleLoadFailed(const Error& error, const ResourceId& resourceId);

    [[nodiscard]] static Result<Void> onComponentSetupFailed(const ResourceId& resourceId, const Error& error);

    void initializeModuleLoader(JavaScriptEntryParameters& jsEntry);

    JSValueRef loadJsModule(IJavaScriptContext& jsContext,
                            const StringBox& importPath,
                            bool failOnRemoteArchive,
                            const JSValueRef* parameters,
                            size_t parametersLength,
                            JSExceptionTracker& exceptionTracker);

    ModuleLoadResult loadJsModuleFromBytes(IJavaScriptContext& jsContext,
                                           const BytesView& jsModule,
                                           const StringBox& importPath,
                                           const JSValueRef* parameters,
                                           size_t parametersLength,
                                           JSExceptionTracker& exceptionTracker);

    ModuleLoadResult loadJsModuleFromNative(IJavaScriptContext& jsContext,
                                            const StringBox& importPath,
                                            const JSValueRef* parameters,
                                            size_t parametersLength,
                                            JSExceptionTracker& exceptionTracker);

    StyleAttributesCache& getStyleAttributesCache();

    void notifyDaemonClientEvent(DaemonClientEventType eventType,
                                 const JSValue& daemonClient,
                                 const JSValue& payload,
                                 JavaScriptEntryParameters& jsEntry);

    void notifyDaemonClientEvent(IJavaScriptContext& jsContext,
                                 DaemonClientEventType eventType,
                                 const JSValue& daemonClient,
                                 const JSValue& payload,
                                 const JSValue& onDaemonClientEventFunction,
                                 JSExceptionTracker& exceptionTracker);

    void onInitError(std::string_view failingAction, const Error& error);
    void onRecoverableError(std::string_view failingAction, JSExceptionTracker& exceptionTracker);
    void onRecoverableError(std::string_view failingAction, const Error& error);

    DispatchFunction makeJsThreadDispatchFunction(Ref<Context>&& ownerContext, JavaScriptThreadTask&& jsTask);

    void dispatchOnJsThreadUnattributed(JavaScriptThreadTask&& function);

    void handleUncaughtJsError(IJavaScriptContext& jsContext,
                               const Ref<Context>& ownerContext,
                               JSExceptionTracker& exceptionTracker);
    void handleUncaughtJsError(IJavaScriptContext& jsContext,
                               const Ref<Context>& ownerContext,
                               const Error& error,
                               bool isUnhandledPromise);
    void handleUncaughtJsErrorNoHandler(const Ref<Context>& ownerContext,
                                        const Error& error,
                                        bool isUnhandledPromise,
                                        bool shouldCrash);

    void teardown(bool destroyContext);

    Ref<JSStackTraceProvider> doCaptureCurrentStackTrace(IJavaScriptContext& jsContext);

    static void lockNextWorker(std::vector<IJavaScriptContext*>& jsContexts,
                               std::vector<Ref<JavaScriptRuntime>>& jsWorkers,
                               const DispatchFunction& onDone);

    Result<Ref<Context>> getContextForId(ContextId contextId) const;
};

} // namespace Valdi
