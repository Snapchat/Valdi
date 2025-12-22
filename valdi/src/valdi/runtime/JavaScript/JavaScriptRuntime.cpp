//
//  JavaScriptRuntime.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/28/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"

#include <boost/algorithm/string.hpp>
#include <fmt/ostream.h>

#include "utils/platform/BuildOptions.hpp"
#include "utils/time/StopWatch.hpp"
#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/JavaScript/JSFunctionWithCallable.hpp"
#include "valdi/runtime/JavaScript/JSFunctionWithMethod.hpp"
#include "valdi/runtime/JavaScript/JSFunctionWithValueFunction.hpp"
#include "valdi/runtime/JavaScript/JavaScriptBridgedObjectLeakTracker.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/JavaScript/JavaScriptErrorStackTrace.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptModuleContainer.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/JavaScript/JavaScriptValueMarshaller.hpp"
#include "valdi/runtime/JavaScript/JavaScriptWorker.hpp"
#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"
#include "valdi/runtime/Resources/DirectionalAsset.hpp"
#include "valdi/runtime/Resources/PlatformSpecificAsset.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi_core/JSRuntimeNativeObjectsManager.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Resources/ResourceId.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/TimePoint.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntimeDeserializers.hpp"
#include "valdi/runtime/JavaScript/Modules/JavaScriptModuleFactory.hpp"

#include "valdi/runtime/Interfaces/IDiskCache.hpp"

#include "valdi/runtime/Resources/AssetCatalog.hpp"
#include "valdi/runtime/Resources/AssetResolver.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi_core/cpp/Resources/Asset.hpp"

#include "valdi/runtime/CSS/CSSDocument.hpp"
#include "valdi/runtime/CSS/StyleAttributesCache.hpp"

#include "valdi/runtime/Context/ContextAttachedValdiObject.hpp"
#include "valdi/runtime/Context/ContextManager.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNodePath.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"

#include "valdi/runtime/Debugger/IDaemonClient.hpp"

#include "valdi/runtime/Rendering/RenderRequest.hpp"

#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"

#include "valdi/runtime/Metrics/Metrics.hpp"

#include "utils/encoding/Base64Utils.hpp"
#include "valdi/runtime/JavaScript/JavaScriptAssetLoadObserver.hpp"
#include "valdi_core/cpp/Utils/ContainerUtils.hpp"
#include <fmt/format.h>
#include <sstream>

#if defined(__ANDROID__)
#include <malloc.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

#define JS_BIND(context, exceptionTracker, jsObject, name, __function)                                                 \
    bindRuntimeFunction(context, exceptionTracker, jsObject, name, &JavaScriptRuntime::__function);                    \
    if (!(exceptionTracker)) {                                                                                         \
        return;                                                                                                        \
    }

namespace Valdi {

// static const long long kJsGarbageCollectionDelaySeconds = 2;
constexpr int kTraceRecordingTimeoutSeconds = 20;

constexpr size_t kLoadPropertyName = 0;
constexpr size_t kUnloadAllUnusedPropertyName = 1;
constexpr size_t kIsLoadedPropertyName = 2;
constexpr size_t kUnloadPropertyName = 3;
constexpr size_t kRegisterModulePropertyName = 4;
constexpr size_t kPreloadModulePropertyName = 5;

const ResourceId& valdiModuleResourceId() {
    static auto kValdiModuleResourceId =
        JavaScriptPathResolver::resolveResourceId(STRING_LITERAL("valdi_core/src/Valdi")).value();
    return kValdiModuleResourceId;
}

const ResourceId& symbolicatorResourceId() {
    static auto kSymbolicatorResourceId =
        JavaScriptPathResolver::resolveResourceId(STRING_LITERAL("valdi_core/src/Symbolicator")).value();
    return kSymbolicatorResourceId;
}

static ContextId getParameterAsContextId(JSFunctionNativeCallContext& callContext, size_t index) {
    return static_cast<ContextId>(callContext.getParameterAsInt(index));
}

static int64_t getMemoryUsageBytes() {
#if defined(__ANDROID__)
    // mallinfo2 is available from API 33 (Android 13) onwards.
    // __ANDROID_API__ is provided by the NDK.
#if defined(__ANDROID_API__) && __ANDROID_API__ >= 33
    struct mallinfo2 mi = mallinfo2();
    return static_cast<int64_t>(mi.uordblks);
#else
    // Fallback for older Android versions
    struct mallinfo mi = mallinfo();
    // In mallinfo, uordblks is an int (32-bit), so this may overflow at 2GB
    return static_cast<int64_t>(mi.uordblks);
#endif

#elif defined(__APPLE__)
    task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        return static_cast<int64_t>(info.resident_size);
    }
    return 0;

#else
    return 0;
#endif
}

static int64_t getCurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

STRING_CONST(callActionMessageName, "callAction")
STRING_CONST(callActionParameterActionKey, "action")
STRING_CONST(callActionParameterParametersKey, "parameters")

class JavaScriptRuntimeCallable : public JSFunctionWithMethod<JavaScriptRuntime> {
public:
    using JSFunctionWithMethod<JavaScriptRuntime>::JSFunctionWithMethod;

    ~JavaScriptRuntimeCallable() override = default;
};

class JavaScriptRuntimeTraceProxyCallable : public JSFunction {
public:
    JavaScriptRuntimeTraceProxyCallable(const StringBox& traceName,
                                        IJavaScriptContext& jsContext,
                                        const JSValue& callback)
        : _traceName(traceName),
          _referenceInfo(ReferenceInfoBuilder().withObject(nameFromJSFunction(jsContext, callback)).build()),
          _callback(callback) {}

    ~JavaScriptRuntimeTraceProxyCallable() override = default;

    const ReferenceInfo& getReferenceInfo() const override {
        return _referenceInfo;
    }

    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept override {
        VALDI_TRACE(_traceName);
        return callContext.getContext().callObjectAsFunction(_callback, callContext);
    }

private:
    StringBox _traceName;
    ReferenceInfo _referenceInfo;
    JSValue _callback;
};

class JSRuntimeNativeObjectsManagerImpl : public snap::valdi_core::JSRuntimeNativeObjectsManager {
public:
    JSRuntimeNativeObjectsManagerImpl(ContextManager& contextManager, Ref<Context>&& context)
        : _contextManager(contextManager), _context(context) {}

    ~JSRuntimeNativeObjectsManagerImpl() override {
        doDispose();
    }

    bool doDispose() {
        if (_context == nullptr) {
            return false;
        }
        auto context = std::move(_context);
        _contextManager.destroyContext(context);

        return true;
    }

    Valdi::Value getReachableObjectsDescription() override {
        if (_context == nullptr) {
            return Valdi::Value::undefined();
        }

        auto disposables = _context->getAllDisposables();
        ValueArrayBuilder output;
        for (auto* disposable : disposables) {
            auto* attachedObject = dynamic_cast<ContextAttachedValdiObject*>(disposable);
            if (attachedObject == nullptr) {
                continue;
            }
            auto innerObject = attachedObject->getInnerValdiObject();
            if (innerObject == nullptr) {
                continue;
            }
            output.append(Value(innerObject->getDebugDescription()));
        }

        return Value(output.build());
    }

    constexpr const Ref<Context>& getContext() const {
        return _context;
    }

private:
    ContextManager& _contextManager;
    Ref<Context> _context;
};

JavaScriptStacktraceCaptureSession::JavaScriptStacktraceCaptureSession() = default;
JavaScriptStacktraceCaptureSession::~JavaScriptStacktraceCaptureSession() = default;

void JavaScriptStacktraceCaptureSession::setCapturedStacktrace(const JavaScriptCapturedStacktrace& capturedStacktrace) {
    std::lock_guard<Mutex> lock(_mutex);
    _capturedStacktrace = {capturedStacktrace};
}

bool JavaScriptStacktraceCaptureSession::finishedCapture() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _capturedStacktrace.has_value();
}

std::optional<JavaScriptCapturedStacktrace> JavaScriptStacktraceCaptureSession::getCapturedStacktrace() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _capturedStacktrace;
}

JavaScriptRuntime::JavaScriptRuntime(IJavaScriptBridge& jsBridge,
                                     ResourceManager& resourceManager,
                                     ContextManager& contextManager,
                                     MainThreadManager& mainThreadManager,
                                     AttributeIds& attributeIds,
                                     PlatformType platformType,
                                     bool enableDebugger,
                                     ThreadQoSClass threadQoS,
                                     const Ref<JavaScriptANRDetector>& anrDetector,
                                     const Ref<ILogger>& logger,
                                     bool isWorker)
    : _javaScriptBridge(jsBridge),
      _resourceManager(resourceManager),
      _contextManager(contextManager),
      _mainThreadManager(mainThreadManager),
      _attributeIds(attributeIds),
      _logger(logger),
      _listener(nullptr),
      _anrDetector(anrDetector),
      _isDisposed(false),
      _enableDebugger(enableDebugger),
      _enableStackTraceCapture(enableDebugger),
      _platformType(platformType),
      _isWorker(isWorker) {
    VALDI_DEBUG(*_logger, "Creating JavaScriptRuntime (instance ptr {})", static_cast<void*>(this));

    auto queueName = STRING_LITERAL(isWorker ? "Valdi JS Worker Thread" : "Valdi JS Thread");
    if (jsBridge.requiresDedicatedThread()) {
        _dispatchQueue = DispatchQueue::createThreaded(queueName, threadQoS);
    } else {
        _dispatchQueue = DispatchQueue::create(queueName, threadQoS);
    }

    _propertyNameIndex.set(kLoadPropertyName, "load");
    _propertyNameIndex.set(kUnloadAllUnusedPropertyName, "unloadAllUnused");
    _propertyNameIndex.set(kIsLoadedPropertyName, "isLoaded");
    _propertyNameIndex.set(kUnloadPropertyName, "unload");
    _propertyNameIndex.set(kRegisterModulePropertyName, "registerModule");
    _propertyNameIndex.set(kPreloadModulePropertyName, "preload");

    _upTime.start();
    _initLock.enter();
    _dispatchQueue->async([this]() {
        _initLock.blockingWait(); // Wait for postInit() to be called first before initializing
        this->doInitialize();
    });

    _globalContext = _contextManager.createContext(nullptr, nullptr, /* deferRender */ true);
    // Keep a +1 disposable until we complete the teardown
    _globalContext->retainDisposables();
}

void JavaScriptRuntime::postInit() {
    if (!_isWorker && _anrDetector != nullptr) {
        _anrDetector->appendTaskScheduler(this);
    }

    _contextHandler = makeShared<JavaScriptComponentContextHandler>(*this, this, *_logger);
    _initLock.leaveIfNotCompleted();

    if (_listener != nullptr) {
        auto runtimeTweaks = _listener->getRuntimeTweaks();
        if (runtimeTweaks != nullptr) {
            _dispatchQueue->setDisableSyncCallsInCallingThread(runtimeTweaks->disableSyncCallsInCallingThread());
        }
    }
}

JavaScriptRuntime::~JavaScriptRuntime() {
    VALDI_DEBUG(*_logger, "Destroying JavaScriptRuntime (instance ptr {})", static_cast<void*>(this));
    fullTeardown();
}

void JavaScriptRuntime::doInitialize() {
    if (_isDisposed) {
        return;
    }

    VALDI_TRACE("Valdi.initializeJsRuntime");

    // Preload the JS context

    auto initializeResult = initializeContext();
    if (!initializeResult) {
        onInitError("initializeContext", initializeResult.error());
        return;
    }

    _running = true;

    makeJsThreadDispatchFunction(Ref(_globalContext), [&](JavaScriptEntryParameters& jsEntry) {
        this->initializeModuleLoader(jsEntry);
        if (!jsEntry.exceptionTracker) {
            onInitError("initializeModuleLoader", jsEntry.exceptionTracker.extractError());
            return;
        }
    })();
}

void JavaScriptRuntime::fullTeardown() {
    teardown(true);
}

void JavaScriptRuntime::partialTeardown() {
    teardown(false);
}

void JavaScriptRuntime::teardown(bool destroyContext) {
    auto wasDisposed = _isDisposed.exchange(true);
    if (wasDisposed) {
        return;
    }

    // Just in case postInit() was never called
    _initLock.leaveIfNotCompleted();

    if (destroyContext) {
        _dispatchQueue->sync([&]() {
            if (_anrDetector != nullptr) {
                _anrDetector->removeTaskScheduler(this);
            }
            // Prevent further dispatches to run
            _listener = nullptr;
            _dispatchQueue->fullTeardown();

            if (!destroyContext) {
                return;
            }

            // Bridged objects should be deallocated immediately
            auto nonDeferredPool = Valdi::RefCountableAutoreleasePool::makeNonDeferred();

            _globalContext->releaseDisposables();
            _contextManager.destroyContext(_globalContext);

            _modules.clear();
            _daemonClients.clear();
            _moduleLoader = JSValueRef();
            _symbolicateFunction = Result<JSValueRef>();
            _onDaemonClientEventFunction = Result<JSValueRef>();
            _uncaughtExceptionHandler = nullptr;
            _unhandledRejectionHandler = nullptr;
            if (_contextHandler != nullptr) {
                _contextHandler->clear();
                _contextHandler = nullptr;
            }
            _runtimeDeserializers = nullptr;
            _propertyNameIndex.setContext(nullptr);
            auto weakJavaScriptContext = weakRef(_javaScriptContext.get());
            _javaScriptContext = nullptr;

            SC_ASSERT(weakJavaScriptContext.expired());
        });
    } else {
        if (_anrDetector != nullptr) {
            _anrDetector->removeTaskScheduler(this);
        }
        _dispatchQueue->fullTeardown();
        _listener = nullptr;
    }
}

void JavaScriptRuntime::setThreadQoS(ThreadQoSClass threadQoS) {
    _dispatchQueue->setQoSClass(threadQoS);
}

void JavaScriptRuntime::setListener(Valdi::IJavaScriptRuntimeListener* listener) {
    _listener = listener;
}

IJavaScriptRuntimeListener* JavaScriptRuntime::getListener() const {
    return _listener;
}

void JavaScriptRuntime::bindRuntimeFunction(IJavaScriptContext& jsContext,
                                            JSExceptionTracker& exceptionTracker,
                                            const JSValueRef& jsObject,
                                            const char* functionName,
                                            JSValueRef (JavaScriptRuntime::*function)(JSFunctionNativeCallContext&)) {
    auto functionNameStr = STRING_LITERAL(functionName);
    auto objectName = STRING_LITERAL("runtime");
    auto callable = makeShared<JavaScriptRuntimeCallable>(
        *this,
        function,
        ReferenceInfoBuilder().withObject(objectName).withProperty(functionNameStr).asFunction().build());

    auto jsFunction = jsContext.newFunction(callable, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    jsContext.setObjectProperty(
        jsObject.get(), functionNameStr.toStringView(), jsFunction.get(), true, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
}

Result<Void> JavaScriptRuntime::initializeContext() {
    VALDI_TRACE("Valdi.createJsContext");
    Ref<ValdiRuntimeTweaks> runtimeTweaks;
    if (_listener != nullptr) {
        runtimeTweaks = _listener->getRuntimeTweaks();
    }

    VALDI_INFO(*_logger, "Creating JSContext from engine '{}'", _javaScriptBridge.getName());

    auto jsContext = _javaScriptBridge.createJsContext(this, *_logger);

    std::unique_ptr<JavaScriptRuntimeDeserializers> runtimeDeserializers;

    IJavaScriptContextConfig config;

    JSExceptionTracker exceptionTracker(*jsContext);
    jsContext->initialize(config, exceptionTracker);

    if (exceptionTracker) {
        jsContext->startDebugger(_isWorker);

        runtimeDeserializers =
            std::make_unique<JavaScriptRuntimeDeserializers>(*jsContext, _stringCache, getStyleAttributesCache());
        buildContext(*jsContext, runtimeTweaks, exceptionTracker);
    }

    if (exceptionTracker) {
        _javaScriptContext = std::move(jsContext);
        _runtimeDeserializers = std::move(runtimeDeserializers);
        _propertyNameIndex.setContext(_javaScriptContext.get());
        return Void();
    } else {
        return exceptionTracker.extractError();
    }
}

const Ref<DispatchQueue>& JavaScriptRuntime::getJsDispatchQueue() const {
    return _dispatchQueue;
}

void JavaScriptRuntime::setValueToGlobalObject(const StringBox& name, const Value& value) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        auto globalKey = STRING_LITERAL("global");
        auto jsValueResult = valueToJSValue(entry.jsContext,
                                            value,
                                            ReferenceInfoBuilder().withObject(globalKey).withProperty(name),
                                            entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("setValueToGlobalObject", entry.exceptionTracker);
            return;
        }

        auto globalObject = entry.jsContext.getGlobalObject(entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("setValueToGlobalObject", entry.exceptionTracker);
            return;
        }

        entry.jsContext.setObjectProperty(
            globalObject.get(), name.toStringView(), jsValueResult.get(), true, entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("setValueToGlobalObject", entry.exceptionTracker);
            return;
        }
    });
}

JSValueRef JavaScriptRuntime::runtimeOutputLog(JSFunctionNativeCallContext& callContext) {
    LogType logType;

    auto logTypeIndex = static_cast<int>(
        callContext.getContext().valueToInt(callContext.getParameter(0), callContext.getExceptionTracker()));
    CHECK_CALL_CONTEXT(callContext);

    if (logTypeIndex == 0) {
        logType = LogType::LogTypeDebug;
    } else if (logTypeIndex == 1) {
        logType = LogType::LogTypeInfo;
    } else if (logTypeIndex == 2) {
        logType = LogType::LogTypeInfo;
    } else if (logTypeIndex == 3) {
        logType = LogType::LogTypeWarn;
    } else if (logTypeIndex == 4) {
        logType = LogType::LogTypeError;
    } else {
        return callContext.throwError(Error("Invalid log type"));
    }

    if (snap::kIsAppstoreBuild && !_logger->isEnabledForType(logType)) {
        return callContext.getContext().newUndefined();
    }

    auto logContentString =
        callContext.getContext().valueToStaticString(callContext.getParameter(1), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    _logger->log(logType, logContentString->toStdString());

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimePostMessage(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto messageName = callContext.getParameterAsString(1);
    CHECK_CALL_CONTEXT(callContext);

    auto additionalParameters = callContext.getParameterAsValue(2);
    CHECK_CALL_CONTEXT(callContext);

    if (messageName == callActionMessageName()) {
        const auto& map = additionalParameters.getMap();
        if (map == nullptr) {
            return callContext.throwError(Error("Missing actionName"));
        }
        const auto& actionNameIt = map->find(callActionParameterActionKey());
        const auto& actionParametersIt = map->find(callActionParameterParametersKey());

        if (actionNameIt == map->end()) {
            return callContext.throwError(Error("Missing actionName"));
        }

        const auto& actionName = actionNameIt->second.toStringBox();

        Value actionParameters;
        if (actionParametersIt != map->end()) {
            actionParameters = actionParametersIt->second;
        }

        if (actionParameters.getArray() != nullptr) {
            dispatchOnMainThread([=]() {
                if (_listener != nullptr) {
                    _listener->receivedCallActionMessage(contextId, actionName, actionParameters.getArrayRef());
                }
            });
        } else {
            if (actionParameters.getType() != Valdi::ValueType::Null) {
                return callContext.throwError(
                    Error(STRING_FORMAT("actionParameters should be an array, got {}",
                                        Valdi::valueTypeToString(actionParameters.getType()))));
            }
            dispatchOnMainThread([=]() {
                if (_listener != nullptr) {
                    _listener->receivedCallActionMessage(contextId, actionName, nullptr);
                }
            });
        }
    } else {
        return callContext.throwError(Error(STRING_FORMAT("Unrecognized message '{}'", messageName)));
    }

    return callContext.getContext().newUndefined();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeConfigureCallback(JSFunctionNativeCallContext& callContext) {
    auto exportMode = static_cast<JSFunctionExportMode>(callContext.getParameterAsInt(0));
    CHECK_CALL_CONTEXT(callContext);

    callContext.getContext().setValueFunctionExportMode(
        callContext.getParameter(1), exportMode, callContext.getExceptionTracker());

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimePerformSyncWithMainThread(JSFunctionNativeCallContext& callContext) {
    auto wrappedJsFunctionResult = callContext.getParameterAsValue(0);
    CHECK_CALL_CONTEXT(callContext);
    auto wrappedJsFunction = wrappedJsFunctionResult.getFunctionRef();
    if (wrappedJsFunction == nullptr) {
        return callContext.throwError(Error("Expecting function"));
    }

    dispatchOnMainThread([wrappedJsFunction] {
        // This wrapped function will automatically manage the main thread batch.
        (*wrappedJsFunction)(Valdi::ValueFunctionFlagsCallSync, {});
    });

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeCallOnMainThread(JSFunctionNativeCallContext& callContext) {
    auto wrappedFunctionResult = callContext.getParameterAsValue(0);
    CHECK_CALL_CONTEXT(callContext);
    auto wrappedFunction = wrappedFunctionResult.getFunctionRef();
    if (wrappedFunction == nullptr) {
        return callContext.throwError(Error("Expecting function"));
    }

    auto functionParametersValue = callContext.getParameterAsValue(1);
    CHECK_CALL_CONTEXT(callContext);

    if (!functionParametersValue.isArray()) {
        return callContext.throwError(Error("Expecting parameters array"));
    }

    dispatchOnMainThread([wrappedFunction, functionParametersValue] {
        const auto* functionParameters = functionParametersValue.getArray();
        (*wrappedFunction)(functionParameters->begin(), functionParameters->size());
    });

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeGetCurrentPlatform(JSFunctionNativeCallContext& callContext) {
    switch (_platformType) {
        case PlatformTypeAndroid:
            return callContext.getContext().newNumber(static_cast<int32_t>(1));
        case PlatformTypeIOS:
            return callContext.getContext().newNumber(static_cast<int32_t>(2));
    }
}

JSValueRef JavaScriptRuntime::runtimeGetBackendRenderingTypeForContextId(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto contextResult = getContextForId(contextId);
    if (!contextResult) {
        return callContext.throwError(contextResult.moveError());
    }

    const auto& viewManagerContext = contextResult.value()->getViewManagerContext();
    if (viewManagerContext == nullptr) {
        return callContext.throwError(Error("Given Context does not have a ViewManagerContext set"));
    }

    static constexpr int32_t kBackendRenderingTypeIOS = 1;
    static constexpr int32_t kBackendRenderingTypeAndroid = 2;
    static constexpr int32_t kBackendRenderingTypeSnapDrawing = 3;

    int32_t result;
    if (viewManagerContext->getViewManager().getRenderingBackendType() == RenderingBackendTypeSnapDrawing) {
        result = kBackendRenderingTypeSnapDrawing;
    } else {
        switch (viewManagerContext->getViewManager().getPlatformType()) {
            case PlatformTypeAndroid:
                result = kBackendRenderingTypeAndroid;
                break;
            case PlatformTypeIOS:
                result = kBackendRenderingTypeIOS;
                break;
        }
    }

    return callContext.getContext().newNumber(result);
}

JSValueRef JavaScriptRuntime::runtimeInternString(JSFunctionNativeCallContext& callContext) {
    auto str = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto index = _stringCache.store(std::move(str));
    return callContext.getContext().newNumber(static_cast<int32_t>(index));
}

JSValueRef JavaScriptRuntime::runtimeGetAttributeId(JSFunctionNativeCallContext& callContext) {
    auto str = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto index = _attributeIds.getIdForName(str);
    return callContext.getContext().newNumber(static_cast<int32_t>(index));
}

JSValueRef JavaScriptRuntime::runtimeCreateContext(JSFunctionNativeCallContext& callContext) {
    auto jsContextHandler = callContext.getParameter(0);

    Ref<Context> context;
    if (callContext.getContext().isValueUndefined(jsContextHandler)) {
        context = _contextManager.createContext(nullptr, nullptr, /* deferRender */ true);
    } else {
        if (_defaultViewManagerContext == nullptr) {
            return callContext.throwError(Error("Cannot create context without a default ViewManagerContext"));
        }

        auto handler = Valdi::makeShared<JavaScriptComponentContextHandler>(*this, nullptr, *_logger);
        auto globalRef =
            makeShared<JSValueRefHolder>(callContext.getContext(),
                                         jsContextHandler,
                                         ReferenceInfoBuilder(callContext.getReferenceInfo()).withParameter(0).build(),
                                         callContext.getExceptionTracker(),
                                         true);
        handler->setJsContextHandler(globalRef);

        context = _contextManager.createContext(handler, _defaultViewManagerContext, /* deferRender */ true);
    }

    context->onCreate();

    auto callerContextRef = Context::currentRef();
    auto callerContextId = static_cast<int32_t>(callerContextRef->getContextId());
    auto newContextId = static_cast<int32_t>(context->getContextId());
    if (context->getParent() == nullptr && callerContextId != 1 /* Ignore root context */) {
        VALDI_DEBUG(*_logger, "Setting context {}'s parent context to {}", newContextId, callerContextId);

        context->setParent(callerContextRef);
        callerContextRef->getRoot()->retainDisposables();
    }

    return callContext.getContext().newNumber(newContextId);
}

JSValueRef JavaScriptRuntime::runtimeDestroyContext(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return callContext.getContext().newUndefined();
    }

    _contextManager.destroyContext(context);
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeSetLayoutSpecs(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return callContext.getContext().newUndefined();
    }

    auto width = callContext.getParameterAsDouble(1);
    CHECK_CALL_CONTEXT(callContext);
    auto height = callContext.getParameterAsDouble(2);
    CHECK_CALL_CONTEXT(callContext);
    auto isRTL = callContext.getParameterAsBool(3);
    CHECK_CALL_CONTEXT(callContext);

    _listener->resolveViewNodeTree(context, true, true, [=](const SharedViewNodeTree& viewNodeTree) {
        if (viewNodeTree == nullptr) {
            return;
        }

        viewNodeTree->setLayoutSpecs(Size(static_cast<float>(width), static_cast<float>(height)),
                                     isRTL ? LayoutDirectionRTL : LayoutDirectionLTR);
    });

    return callContext.getContext().newUndefined();
}

static MeasureMode toMeasureMode(int32_t measureModeInt) {
    switch (measureModeInt) {
        case 0:
            return MeasureModeUnspecified;
        case 1:
            return MeasureModeExactly;
        case 2:
            return MeasureModeAtMost;
    }

    SC_ASSERT_FAIL("Invalid measure mode");
    return MeasureModeUnspecified;
}

JSValueRef JavaScriptRuntime::runtimeMeasureContext(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return callContext.getContext().newUndefined();
    }

    auto maxWidth = callContext.getParameterAsDouble(1);
    CHECK_CALL_CONTEXT(callContext);
    auto widthMode = toMeasureMode(callContext.getParameterAsInt(2));
    CHECK_CALL_CONTEXT(callContext);
    auto maxHeight = callContext.getParameterAsDouble(3);
    CHECK_CALL_CONTEXT(callContext);
    auto heightMode = toMeasureMode(callContext.getParameterAsInt(4));
    CHECK_CALL_CONTEXT(callContext);
    auto isRTL = callContext.getParameterAsBool(5);
    CHECK_CALL_CONTEXT(callContext);

    Size measuredSize;

    _listener->resolveViewNodeTree(context, false, false, [&](const SharedViewNodeTree& viewNodeTree) {
        if (viewNodeTree == nullptr) {
            return;
        }

        measuredSize = viewNodeTree->measureLayout(static_cast<float>(maxWidth),
                                                   widthMode,
                                                   static_cast<float>(maxHeight),
                                                   heightMode,
                                                   isRTL ? LayoutDirectionRTL : LayoutDirectionLTR);
    });

    auto outputWidthJS = callContext.getContext().newNumber(static_cast<double>(measuredSize.width));
    auto outputHeightJS = callContext.getContext().newNumber(static_cast<double>(measuredSize.height));

    std::initializer_list<JSValue> outputSize = {outputWidthJS.get(), outputHeightJS.get()};

    return callContext.getContext().newArrayWithValues(
        outputSize.begin(), outputSize.size(), callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeSubmitRenderRequest(JSFunctionNativeCallContext& callContext) {
    const auto& referenceInfo = callContext.getReferenceInfo();
    auto& exceptionTracker = callContext.getExceptionTracker();
    auto rawRequest = callContext.getParameter(0);
    CHECK_CALL_CONTEXT(callContext);
    auto callback = callContext.getParameterAsFunction(1);
    CHECK_CALL_CONTEXT(callContext);
    auto renderRequest = _runtimeDeserializers->deserializeRenderRequest(rawRequest, referenceInfo, exceptionTracker);
    if (exceptionTracker && _listener != nullptr) {
        _listener->receivedRenderRequest(renderRequest);
    }
    if (callback != nullptr) {
        (*callback)();
    }
    return callContext.getContext().newUndefined();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeTrace(JSFunctionNativeCallContext& callContext) {
    auto traceName = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    VALDI_TRACE_META("Valdi.jsTrace", traceName);

    auto subCallContext = callContext.makeSubContext(nullptr, 0);

    return callContext.getContext().callObjectAsFunction(callContext.getParameter(1), subCallContext);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeMakeTraceProxy(JSFunctionNativeCallContext& callContext) {
    auto traceName = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto callback = callContext.getParameter(1);

    JavaScriptContextEntry entry(nullptr);
    return callContext.getContext().newFunction(
        makeShared<JavaScriptRuntimeTraceProxyCallable>(traceName, callContext.getContext(), callback),
        callContext.getExceptionTracker());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeStartTraceRecording(JSFunctionNativeCallContext& callContext) {
    auto id = Tracer::shared().startRecording();

    // Schedule auto stop recording to make sure we are not leaking.
    _dispatchQueue->asyncAfter([id]() { Tracer::shared().stopRecording(id); },
                               std::chrono::seconds(kTraceRecordingTimeoutSeconds));

    return callContext.getContext().newNumber(static_cast<int32_t>(id));
}

static double traceTimePointToEpochMicroseconds(const TraceTimePoint& timePoint) {
    auto duration = timePoint.time_since_epoch();
    auto asMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    return static_cast<double>(asMicroseconds.count());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeStopTraceRecording(JSFunctionNativeCallContext& callContext) {
    auto id = static_cast<size_t>(callContext.getParameterAsInt(0));
    CHECK_CALL_CONTEXT(callContext);

    auto traces = Tracer::shared().stopRecording(static_cast<size_t>(id));
    std::sort(traces.begin(), traces.end(), [](const RecordedTrace& left, const RecordedTrace& right) -> bool {
        return left.start < right.start;
    });

    ValueArrayBuilder output;

    for (auto& trace : traces) {
        output.append(Value(StringCache::getGlobal().makeString(std::move(trace.trace))));

        output.append(Value(traceTimePointToEpochMicroseconds(trace.start)));
        output.append(Value(traceTimePointToEpochMicroseconds(trace.end)));
        output.append(Value(static_cast<int32_t>(trace.threadId)));
    }

    auto array = output.build();
    return valueToJSValue(callContext.getContext(),
                          Value(array),
                          ReferenceInfoBuilder(callContext.getReferenceInfo()).withReturnValue(),
                          callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeSubmitDebugMessage(JSFunctionNativeCallContext& callContext) {
    auto debugLevel = static_cast<int32_t>(callContext.getParameterAsInt(0));
    CHECK_CALL_CONTEXT(callContext);
    auto message = callContext.getParameterAsString(1);
    CHECK_CALL_CONTEXT(callContext);

    if (_listener != nullptr) {
        _listener->onDebugMessage(debugLevel, message);
    }
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeOnUncaughtError(JSFunctionNativeCallContext& callContext) {
    auto errorMessage = callContext.getParameterAsString(0);
    callContext.getExceptionTracker().clearError();
    auto error =
        convertJSErrorToValdiError(callContext.getContext(),
                                   JSValueRef::makeUnretained(callContext.getContext(), callContext.getParameter(1)),
                                   nullptr);
    callContext.getExceptionTracker().clearError();
    handleUncaughtJsError(callContext.getContext(), Context::currentRef(), error.rethrow(errorMessage), false);

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeProtectNativeRefs(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    Ref<Context> context;
    if (contextId == ContextIdNull) {
        context = Context::currentRoot();
    } else {
        auto result = getContextForId(contextId);
        if (!result) {
            return callContext.throwError(result.moveError());
        }
        context = result.moveValue();
    }

    context->retainDisposables();

    auto releaseLambda =
        makeShared<JSFunctionWithCallable>(ReferenceInfoBuilder(callContext.getReferenceInfo()).withReturnValue(),
                                           [context](JSFunctionNativeCallContext& callContext) -> JSValueRef {
                                               context->releaseDisposables();
                                               return callContext.getContext().newUndefined();
                                           });

    return callContext.getContext().newFunction(releaseLambda, callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeGetFrameForViewId(JSFunctionNativeCallContext& callContext) {
    return handleViewNodeSpecificActionSync(
        callContext, [](ViewNode& viewNode) { return Valdi::Value(viewNode.getDirectionAgnosticFrame().toMap()); });
}

JSValueRef JavaScriptRuntime::runtimeGetNativeViewForViewId(JSFunctionNativeCallContext& callContext) {
    return handleViewNodeSpecificActionSync(callContext,
                                            [](ViewNode& viewNode) { return viewNode.toPlaformRepresentation(true); });
}

JSValueRef JavaScriptRuntime::runtimeGetLayoutDebugInfo(JSFunctionNativeCallContext& callContext) {
    return handleViewNodeSpecificActionSync(
        callContext, [](ViewNode& viewNode) { return Value(viewNode.getLayoutDebugDescription()); });
}

JSValueRef JavaScriptRuntime::runtimeGetNativeNodeForElementId(JSFunctionNativeCallContext& callContext) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto nodeId = static_cast<RawViewNodeId>(callContext.getParameterAsInt(1));
    CHECK_CALL_CONTEXT(callContext);

    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return callContext.getContext().newUndefined();
    }

    auto output = Value::undefined();

    _listener->resolveViewNodeTree(context, false, false, [&](const SharedViewNodeTree& viewNodeTree) {
        auto viewNode = viewNodeTree->getViewNode(nodeId);
        if (viewNode != nullptr) {
            output = viewNode->toPlaformRepresentation(false);
        }
    });

    return valueToJSValue(callContext.getContext(),
                          output,
                          ReferenceInfoBuilder(callContext.getReferenceInfo()).withReturnValue(),
                          callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeTakeElementSnapshot(JSFunctionNativeCallContext& callContext) {
    return handleViewNodeSpecificActionAsync(callContext, [](ViewNode& viewNode, Function<void(const Value&)> cb) {
        viewNode.createSnapshot(viewNode.getViewNodeTree()->getCurrentViewTransactionScope(),
                                [cb = std::move(cb)](Result<BytesView> result) {
                                    if (!result || result.value().empty()) {
                                        cb(Value::undefined());
                                        return;
                                    }

                                    auto base64 = snap::utils::encoding::binaryToBase64(result.value().data(),
                                                                                        result.value().size());

                                    cb(Value(StringCache::getGlobal().makeString(std::move(base64))));
                                });
    });
}

JSValueRef JavaScriptRuntime::runtimeGetViewNodeDebugInfo(JSFunctionNativeCallContext& callContext) {
    return handleViewNodeSpecificActionSync(
        callContext, [](ViewNode& viewNode) { return Value(viewNode.getDebugDescriptionMap()); });
}

void JavaScriptRuntime::callViewNodeActionCallback(const Shared<JSValueRefHolder>& callbackRef, const Value& result) {
    dispatchOnJsThreadAsync(callbackRef->getContext(), [=](JavaScriptEntryParameters& entryParameters) {
        auto jsResult = valueToJSValue(entryParameters.jsContext,
                                       result,
                                       ReferenceInfoBuilder(callbackRef->getReferenceInfo()),
                                       entryParameters.exceptionTracker);
        if (!entryParameters.exceptionTracker) {
            return;
        }

        auto jsValue = callbackRef->getJsValue(entryParameters.jsContext, entryParameters.exceptionTracker);
        if (!entryParameters.exceptionTracker) {
            return;
        }

        auto callContext =
            JSFunctionCallContext(entryParameters.jsContext, &jsResult, 1, entryParameters.exceptionTracker);

        entryParameters.jsContext.callObjectAsFunction(jsValue, callContext);
    });
}

JSValueRef JavaScriptRuntime::handleViewNodeSpecificAction(
    JSFunctionNativeCallContext& callContext,
    Function<void(ViewNode&, Function<void(const Value&)>)>&& handleViewNode) {
    auto contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    auto nodeId = static_cast<RawViewNodeId>(callContext.getParameterAsInt(1));
    CHECK_CALL_CONTEXT(callContext);

    auto callbackParameterIndex = callContext.getParameterSize() - 1;

    auto callbackRef = JSValueRefHolder::makeRetainedCallback(
        callContext.getContext(),
        callContext.getParameter(callbackParameterIndex),
        ReferenceInfoBuilder(callContext.getReferenceInfo()).withParameter(callbackParameterIndex),
        callContext.getExceptionTracker());

    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return callContext.throwError(Valdi::Error(STRING_FORMAT("Could not resolve Context {}", contextId)));
    }

    _listener->resolveViewNodeTree(
        context,
        true,
        true,
        [self = strongSmallRef(this), callbackRef, nodeId, contextId, handleViewNode = std::move(handleViewNode)](
            const SharedViewNodeTree& viewNodeTree) {
            if (viewNodeTree == nullptr) {
                [[maybe_unused]] auto capturedContextId = contextId;
                VALDI_ERROR(*self->_logger, "Could not resolve ViewNodeTree of Context {}", capturedContextId);
                return;
            }

            viewNodeTree->withLock([&]() {
                auto viewNode = viewNodeTree->getViewNode(nodeId);
                if (viewNode != nullptr) {
                    handleViewNode(*viewNode, [self, callbackRef](const auto& result) {
                        self->callViewNodeActionCallback(callbackRef, result);
                    });
                } else {
                    self->callViewNodeActionCallback(callbackRef, Value::undefined());
                }
            });
        });

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::handleViewNodeSpecificActionSync(JSFunctionNativeCallContext& callContext,
                                                               Value (*handleViewNode)(ViewNode&)) {
    return handleViewNodeSpecificAction(
        callContext,
        [handleViewNode](ViewNode& viewNode,
                         Function<void(const Value&)> callback) { // NOLINT(performance-unnecessary-value-param)
            callback(handleViewNode(viewNode));
        });
}

JSValueRef JavaScriptRuntime::handleViewNodeSpecificActionAsync(JSFunctionNativeCallContext& callContext,
                                                                void (*handleViewNode)(ViewNode&,
                                                                                       Function<void(const Value&)>)) {
    return handleViewNodeSpecificAction(callContext,
                                        [handleViewNode](ViewNode& viewNode, Function<void(const Value&)> callback) {
                                            handleViewNode(viewNode, std::move(callback));
                                        });
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeMakeOpaque(JSFunctionNativeCallContext& callContext) {
    return makeOpaque(callContext.getContext(),
                      callContext.getParameter(0),
                      ReferenceInfoBuilder(callContext.getReferenceInfo()).withReturnValue(),
                      callContext.getExceptionTracker());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeBytesToString(JSFunctionNativeCallContext& callContext) {
    auto convertedBytes =
        callContext.getContext().valueToTypedArray(callContext.getParameter(0), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return callContext.getContext().newStringUTF8(
        std::string_view(reinterpret_cast<const char*>(convertedBytes.data), convertedBytes.length),
        callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeIsModuleLoaded(JSFunctionNativeCallContext& callContext) {
    auto moduleName = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto moduleLoaded = _resourceManager.isBundleLoaded(moduleName);
    return callContext.getContext().newBool(moduleLoaded);
}

JSValueRef JavaScriptRuntime::runtimeGetModuleEntry(JSFunctionNativeCallContext& callContext) {
    auto moduleName = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto path = callContext.getParameterAsString(1);
    CHECK_CALL_CONTEXT(callContext);

    auto asString = callContext.getParameterAsBool(2);
    CHECK_CALL_CONTEXT(callContext);

    auto bundle = _resourceManager.getBundle(moduleName);

    if (bundle->hasRemoteArchiveNeedingLoad()) {
        return callContext.throwError(
            Error(STRING_FORMAT("Bundle '{}' is a remote bundle which was not loaded", moduleName)));
    }

    auto entry = bundle->getEntry(path);
    if (!entry) {
        return callContext.throwError(entry.moveError());
    }

    if (asString) {
        return callContext.getContext().newStringUTF8(entry.value().asStringView(), callContext.getExceptionTracker());
    } else {
        return newTypedArrayFromBytesView(
            callContext.getContext(), TypedArrayType::Uint8Array, entry.value(), callContext.getExceptionTracker());
    }
}

JSValueRef JavaScriptRuntime::runtimeGetModuleJsPaths(JSFunctionNativeCallContext& callContext) {
    auto moduleName = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto bundle = _resourceManager.getBundle(moduleName);

    if (bundle->hasRemoteArchiveNeedingLoad()) {
        return callContext.throwError(
            Error(STRING_FORMAT("Bundle '{}' is a remote bundle which was not loaded", moduleName)));
    }

    auto jsPaths = bundle->getAllJsPaths();
    auto output = callContext.getContext().newArray(jsPaths.size(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    size_t index = 0;
    for (const auto& jsPath : jsPaths) {
        auto jsPathJs =
            callContext.getContext().newStringUTF8(jsPath.toStringView(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
        callContext.getContext().setObjectPropertyIndex(
            output.get(), index++, jsPathJs.get(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);
    }

    return output;
}

JSValueRef JavaScriptRuntime::runtimeLoadModule(JSFunctionNativeCallContext& callContext) {
    auto moduleName = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    Shared<JSValueRefHolder> callback;
    auto callbackParam = callContext.getParameter(1);
    if (callContext.getContext().isValueUndefined(callbackParam)) {
        if (!_resourceManager.isLazyModulePreloadingEnabled()) {
            // When not providing a callback, we consider it as a preload operation, which
            // we dont need if eager module preloading is enabled
            return callContext.getContext().newUndefined();
        }
    } else {
        callback = JSValueRefHolder::makeRetainedCallback(
            callContext.getContext(),
            callbackParam,
            ReferenceInfoBuilder(callContext.getReferenceInfo()).withParameter(1),
            callContext.getExceptionTracker());
    }

    _resourceManager.loadModuleAsync(
        moduleName, ResourceManagerLoadModuleType::Sources, [this, callback](const Result<Void>& result) {
            if (callback == nullptr) {
                return;
            }

            this->dispatchOnJsThreadAsync(callback->getContext(), [callback, result](JavaScriptEntryParameters& entry) {
                auto jsValue = callback->getJsValue(entry.jsContext, entry.exceptionTracker);
                if (!entry.exceptionTracker) {
                    return;
                }

                if (result) {
                    JSFunctionCallContext callContext(entry.jsContext, nullptr, 0, entry.exceptionTracker);
                    entry.jsContext.callObjectAsFunction(jsValue, callContext);
                } else {
                    auto errorString = entry.jsContext.newStringUTF8(result.error().toString(), entry.exceptionTracker);
                    if (!entry.exceptionTracker) {
                        return;
                    }

                    JSFunctionCallContext callContext(entry.jsContext, &errorString, 1, entry.exceptionTracker);
                    entry.jsContext.callObjectAsFunction(jsValue, callContext);
                }
            });
        });

    return callContext.getContext().newUndefined();
}

const char* moduleLoadModeToString(ModuleLoadMode moduleLoadMode) {
    switch (moduleLoadMode) {
        case ModuleLoadMode::JS_SOURCE:
            return "js source";
        case ModuleLoadMode::JS_BYTECODE:
            return "js bytecode";
        case ModuleLoadMode::NATIVE:
            return "native";
    }
}

JSValueRef JavaScriptRuntime::loadJsModule(IJavaScriptContext& jsContext,
                                           const StringBox& importPath,
                                           bool failOnRemoteArchive,
                                           const JSValueRef* parameters,
                                           size_t parametersLength,
                                           JSExceptionTracker& exceptionTracker) {
    VALDI_TRACE_META("Valdi.loadJsModule", importPath);
    snap::utils::time::StopWatch sw;
    sw.start();

    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(importPath);
    if (!resourceIdResult) {
        exceptionTracker.onError(resourceIdResult.moveError());
        return jsContext.newUndefined();
    }

    auto resourceId = resourceIdResult.moveValue();

    ModuleLoadResult result;
    auto importPathStringView = importPath.toStringView();
    auto nativeModuleInfo = jsContext.getNativeModuleInfo(importPathStringView);

    int64_t moduleDuration = 0;
    int64_t childrenDuration = 0;

    int64_t moduleMemoryUsage = 0;
    int64_t childrenMemoryUsage = 0;

    if (nativeModuleInfo && _resourceManager.enableTSN() &&
        _resourceManager.enableTSNForModule(nativeModuleInfo.value().name)) {
        // TODO(simon): Add support for hot reloading native modules.
        result = loadJsModuleFromNative(jsContext, importPath, parameters, parametersLength, exceptionTracker);
    } else {
        auto bundle = _resourceManager.getBundle(resourceId.bundleName);

        if (bundle->hasRemoteArchiveNeedingLoad()) {
            auto errorMessage =
                fmt::format("Bundle '{}' is a remote bundle which was not loaded", resourceId.bundleName);

            if (failOnRemoteArchive) {
                exceptionTracker.onError(Error(StringCache::getGlobal().makeString(std::move(errorMessage))));
                return jsContext.newUndefined();
            } else {
                // We return a string for retryable errors.
                return jsContext.newStringUTF8(errorMessage, exceptionTracker);
            }
        }

        auto jsFileContent = bundle->getJs(resourceId.resourcePath);
        if (!jsFileContent) {
            exceptionTracker.onError(jsFileContent.moveError());
            return jsContext.newUndefined();
        }

        // Record memory watermark and timestamp mark before load
        auto memoryWaterMarkBefore = getMemoryUsageBytes();
        auto timestampBefore = getCurrentTimestampMs();

        // 0 if the platform does not support querying memory usage
        if (memoryWaterMarkBefore != 0) {
            _moduleResourceTracker.push_back({memoryWaterMarkBefore, 0, timestampBefore, 0});
        }

        result = loadJsModuleFromBytes(
            jsContext, jsFileContent.value().content, importPath, parameters, parametersLength, exceptionTracker);

        if (memoryWaterMarkBefore != 0) {
            // Compute the memory usage: watermark_after - watermark_before
            moduleMemoryUsage = getMemoryUsageBytes() - _moduleResourceTracker.back().memoryWaterMark;

            // Compute the duration: timestamp_mark_after - timestamp_mark_before
            moduleDuration = getCurrentTimestampMs() - _moduleResourceTracker.back().timestamp;

            // This module's children total (0 if no children)
            childrenMemoryUsage = _moduleResourceTracker.back().childrenMemoryUsage;
            childrenDuration = _moduleResourceTracker.back().childrenDuration;
            _moduleResourceTracker.pop_back();
            if (!_moduleResourceTracker.empty()) {
                // If we are not a top-level module, add usage to the parent module's children total
                _moduleResourceTracker.back().childrenMemoryUsage += moduleMemoryUsage;
                _moduleResourceTracker.back().childrenDuration += moduleDuration;
            }
            const auto& metrics = getMetrics();
            if (metrics != nullptr) {
                metrics->emitLoadModuleMemory(importPath, moduleMemoryUsage, moduleMemoryUsage - childrenMemoryUsage);
                metrics->emitLoadModuleDuration(importPath, moduleDuration, moduleDuration - childrenDuration);
            }
        }
    }

    if (parametersLength >= 3) {
        // require, module, exports
        // We pass the exports parameter to onModuleLoaded()
        onModuleLoaded(jsContext, resourceId, parameters[2].get(), /* isFromJsEval */ true, exceptionTracker);
    }

    if (Valdi::traceLoadModules) {
        VALDI_INFO(*_logger,
                   "Loaded JS module {} in {} (load mode: {}) own memory usage: {} total memory usage: {} own "
                   "duration: {} total duration: {}",
                   importPath,
                   sw.elapsed(),
                   moduleLoadModeToString(result.second),
                   moduleMemoryUsage - childrenMemoryUsage,
                   moduleMemoryUsage,
                   moduleDuration - childrenDuration,
                   moduleDuration);
    }

    return std::move(result.first);
}

ModuleLoadResult JavaScriptRuntime::loadJsModuleFromBytes(IJavaScriptContext& jsContext,
                                                          const BytesView& jsModule,
                                                          const StringBox& importPath,
                                                          const JSValueRef* parameters,
                                                          size_t parametersLength,
                                                          JSExceptionTracker& exceptionTracker) {
    // Assign global context when loading a JS module
    JavaScriptContextEntry contextEntry(_globalContext);

    JSValueRef evalResult;
    ModuleLoadMode moduleLoadMode;

    {
        VALDI_TRACE_META(
            "Valdi.evalJsModule",
            STRING_FORMAT("importPath: {}, jsModule size: {} bytes", importPath.toStringView(), jsModule.size()));
        auto preCompiledContent = getPreCompiledJsModuleData(jsModule);

        if (preCompiledContent) {
            moduleLoadMode = ModuleLoadMode::JS_BYTECODE;
            evalResult =
                jsContext.evaluatePreCompiled(preCompiledContent.value(), importPath.toStringView(), exceptionTracker);
        } else {
            moduleLoadMode = ModuleLoadMode::JS_SOURCE;
            auto moduleSrc = jsModule.asStringView();
            auto formattedSource = formatJsModule(moduleSrc);
            evalResult = jsContext.evaluate(formattedSource, importPath.toStringView(), exceptionTracker);
        }
    }

    if (!exceptionTracker) {
        return std::make_pair(jsContext.newUndefined(), moduleLoadMode);
    }

    JSFunctionCallContext callContext(jsContext, parameters, parametersLength, exceptionTracker);
    return std::make_pair(jsContext.callObjectAsFunction(evalResult.get(), callContext), moduleLoadMode);
}

ModuleLoadResult JavaScriptRuntime::loadJsModuleFromNative(IJavaScriptContext& jsContext,
                                                           const StringBox& importPath,
                                                           const JSValueRef* parameters,
                                                           size_t parametersLength,
                                                           JSExceptionTracker& exceptionTracker) {
    // Assign global context when loading a JS module
    JavaScriptContextEntry contextEntry(_globalContext);

    JSValueRef evalResult;

    {
        VALDI_TRACE("Valdi.evalNativeModule");
        evalResult = jsContext.evaluateNative(importPath.toStringView(), exceptionTracker);
    }

    if (!exceptionTracker) {
        return std::make_pair(jsContext.newUndefined(), ModuleLoadMode::NATIVE);
    }

    JSFunctionCallContext callContext(jsContext, parameters, parametersLength, exceptionTracker);
    return std::make_pair(jsContext.callObjectAsFunction(evalResult.get(), callContext), ModuleLoadMode::NATIVE);
}

JSValueRef JavaScriptRuntime::runtimeLoadJsModule(JSFunctionNativeCallContext& callContext) {
    auto importPath = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    // The rest of the parameters are "require, module, exports"
    const auto* parameters = &callContext.getParameters()[1];
    auto parametersLength = callContext.getParameterSize() - 1;

    return loadJsModule(
        callContext.getContext(), importPath, false, parameters, parametersLength, callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeGetSourceMap(JSFunctionNativeCallContext& callContext) {
    auto importPath = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(importPath);
    if (!resourceIdResult) {
        return callContext.getContext().newUndefined();
    }

    auto resourceId = resourceIdResult.moveValue();
    auto bundle = _resourceManager.getBundle(resourceId.bundleName);
    auto jsFileContent = bundle->getJs(resourceId.resourcePath);
    if (!jsFileContent) {
        return callContext.getContext().newUndefined();
    }

    auto& jsFile = jsFileContent.value();

    if (!getPreCompiledJsModuleData(jsFile.content)) {
        static std::string_view kSourceMapPrefix = "//# sourceMappingURL=data:application/json;base64,";

        // Not a precompiled module, try to extract the source map from the script itself
        auto moduleSrc = jsFile.content.asStringView();
        auto sourceMapIndex = moduleSrc.find(kSourceMapPrefix);
        if (sourceMapIndex != std::string_view::npos) {
            auto sourceMapBase64 = moduleSrc.substr(sourceMapIndex + kSourceMapPrefix.size());
            auto endLine = sourceMapBase64.find('\n');
            if (endLine != std::string_view::npos) {
                sourceMapBase64 = sourceMapBase64.substr(0, endLine);
            }
            auto decodedSourceMapping = snap::utils::encoding::base64ToBinary(sourceMapBase64);
            return callContext.getContext().newStringUTF8(
                std::string_view(reinterpret_cast<const char*>(decodedSourceMapping.data()),
                                 decodedSourceMapping.size()),
                callContext.getExceptionTracker());
        }
    }

    if (!jsFile.sourceMap.isEmpty()) {
        return callContext.getContext().newStringUTF8(jsFile.sourceMap.toStringView(),
                                                      callContext.getExceptionTracker());
    }

    // No source map available
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeGetCSSModule(JSFunctionNativeCallContext& callContext) {
    auto cssModulePath = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(cssModulePath);
    if (!resourceIdResult) {
        return callContext.throwError(resourceIdResult.moveError());
    }

    auto resourceId = resourceIdResult.moveValue();

    auto bundle = _resourceManager.getBundle(resourceId.bundleName);

    auto cssDocumentResult = bundle->getCSSDocument(resourceId.resourcePath, _attributeIds);
    if (!cssDocumentResult) {
        return callContext.throwError(cssDocumentResult.moveError());
    }

    auto cssDocument = cssDocumentResult.moveValue();

    static auto kFunctionName = STRING_LITERAL("getRule");

    auto lambdaFunction = makeShared<JSFunctionWithCallable>(
        ReferenceInfoBuilder(callContext.getReferenceInfo()).withProperty(kFunctionName),
        [cssDocument](JSFunctionNativeCallContext& callContext) -> JSValueRef {
            auto ruleName = callContext.getParameterAsString(0);
            CHECK_CALL_CONTEXT(callContext);

            auto attributes = cssDocument->getAttributesForClass(ruleName);
            if (!attributes) {
                return callContext.throwError(attributes.moveError());
            }

            return makeWrappedObject(
                callContext.getContext(), attributes.value(), callContext.getExceptionTracker(), false);
        });

    auto jsGetRuleFunction = callContext.getContext().newFunction(lambdaFunction, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    auto jsCssDocument =
        makeWrappedObject(callContext.getContext(), cssDocument, callContext.getExceptionTracker(), false);
    CHECK_CALL_CONTEXT(callContext);

    callContext.getContext().setObjectProperty(jsCssDocument.get(),
                                               kFunctionName.toStringView(),
                                               jsGetRuleFunction.get(),
                                               true,
                                               callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    return jsCssDocument;
}

JSValueRef JavaScriptRuntime::runtimeCreateCSSRule(JSFunctionNativeCallContext& callContext) {
    auto attributesResult = callContext.getParameterAsValue(0);
    CHECK_CALL_CONTEXT(callContext);

    auto attributes = attributesResult.getMapRef();
    if (attributes == nullptr) {
        return callContext.throwError(Error("Invalid attributes"));
    }

    auto index = getStyleAttributesCache().resolveAttributesIndex(attributes);

    return callContext.getContext().newNumber(static_cast<int32_t>(index));
}

JSValueRef JavaScriptRuntime::runtimeMakeAssetFromBytes(JSFunctionNativeCallContext& callContext) {
    auto assetBytes = callContext.getParameterAsBytesView(0);
    CHECK_CALL_CONTEXT(callContext);

    auto asset = _resourceManager.getAssetsManager()->createAssetWithBytes(assetBytes);

    return makeWrappedObject(callContext.getContext(), asset, callContext.getExceptionTracker(), false);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JSValueRef JavaScriptRuntime::runtimeMakeAssetFromUrl(JSFunctionNativeCallContext& callContext) {
    auto assetUrl = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);

    auto asset = _resourceManager.getAssetsManager()->getAsset(AssetKey(assetUrl));

    return makeWrappedObject(callContext.getContext(), asset, callContext.getExceptionTracker(), false);
}

JSValueRef JavaScriptRuntime::runtimeMakeDirectionalAsset(JSFunctionNativeCallContext& callContext) {
    auto ltrAssetValue = callContext.getParameterAsValue(0);
    CHECK_CALL_CONTEXT(callContext);
    auto rtlAssetValue = callContext.getParameterAsValue(1);
    CHECK_CALL_CONTEXT(callContext);

    auto ltrAsset = AssetResolver::resolve(_resourceManager, ltrAssetValue);
    auto rtlAsset = AssetResolver::resolve(_resourceManager, rtlAssetValue);

    if (ltrAsset == nullptr || rtlAsset == nullptr) {
        return callContext.throwError(Error("Directional assets can only be created from URL or Valdi assets"));
    }

    auto asset = makeShared<DirectionalAsset>(ltrAsset, rtlAsset);

    return makeWrappedObject(callContext.getContext(), asset, callContext.getExceptionTracker(), false);
}

JSValueRef JavaScriptRuntime::runtimeMakePlatformSpecificAsset(JSFunctionNativeCallContext& callContext) {
    auto defaultAssetValue = callContext.getParameterAsValue(0);
    CHECK_CALL_CONTEXT(callContext);
    auto platformSpecificAssetsMapValue = callContext.getParameterAsValue(1);
    CHECK_CALL_CONTEXT(callContext);

    auto platformSpecificAssets = platformSpecificAssetsMapValue.getMapRef();
    if (platformSpecificAssets == nullptr) {
        return callContext.throwError(Error("Invalid platform specific assets object specified"));
    }

    auto defaultAsset = AssetResolver::resolve(_resourceManager, defaultAssetValue);

    Ref<Asset> iOSAsset;
    Ref<Asset> androidAsset;

    auto iosAssetValue = platformSpecificAssetsMapValue.getMapValue("ios");
    if (!iosAssetValue.isNullOrUndefined()) {
        iOSAsset = AssetResolver::resolve(_resourceManager, iosAssetValue);
    }
    auto androidAssetValue = platformSpecificAssetsMapValue.getMapValue("android");
    if (!androidAssetValue.isNullOrUndefined()) {
        androidAsset = AssetResolver::resolve(_resourceManager, androidAssetValue);
    }

    if (defaultAsset == nullptr || ((iOSAsset == nullptr) && (androidAsset == nullptr))) {
        return callContext.throwError(
            Error("Platform specific assets can only be created from URL or Valdi assets. They also require a "
                  "default asset, and at least one iOS or Android asset specified"));
    }

    auto asset = makeShared<PlatformSpecificAsset>(defaultAsset, iOSAsset, androidAsset);

    return makeWrappedObject(callContext.getContext(), asset, callContext.getExceptionTracker(), false);
}

JSValueRef JavaScriptRuntime::runtimeGetLoadedAssetMetadata(JSFunctionNativeCallContext& callContext) {
    auto loadedAsset = castOrNull<LoadedAsset>(callContext.getParameterAsWrappedObject(0));
    CHECK_CALL_CONTEXT(callContext);

    if (loadedAsset == nullptr) {
        return callContext.throwError(Error("Invalid loaded asset"));
    }

    auto metadata = loadedAsset->getMetadata();

    return valueToJSValue(
        callContext.getContext(), metadata, ReferenceInfoBuilder(), callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeAddAssetLoadObserver(JSFunctionNativeCallContext& callContext) {
    auto assetValue = callContext.getParameterAsValue(0);
    CHECK_CALL_CONTEXT(callContext);

    auto observerCallback = callContext.getParameterAsFunction(1);
    CHECK_CALL_CONTEXT(callContext);

    auto outputType = callContext.getParameterAsInt(2);
    CHECK_CALL_CONTEXT(callContext);

    auto preferredWidth = callContext.getParameterAsInt(3);
    CHECK_CALL_CONTEXT(callContext);

    auto preferredHeight = callContext.getParameterAsInt(4);
    CHECK_CALL_CONTEXT(callContext);

    auto asset = AssetResolver::resolve(_resourceManager, assetValue);

    if (asset == nullptr) {
        return callContext.throwError(Error("Cannot resolve asset"));
    }

    auto observer = makeShared<JavaScriptAssetLoadObserver>(observerCallback);
    auto unsubscribeFunc = makeShared<AssetLoadObserverUnsubscribeFunc>(asset, observer);

    asset->addLoadObserver(observer,
                           static_cast<snap::valdi_core::AssetOutputType>(outputType),
                           preferredWidth,
                           preferredHeight,
                           Valdi::Value::undefined());

    return callContext.getContext().newFunction(unsubscribeFunc, callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimeGetAssets(JSFunctionNativeCallContext& callContext) {
    auto catalogPathResult = callContext.getParameterAsString(0);
    CHECK_CALL_CONTEXT(callContext);
    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(catalogPathResult);
    if (!resourceIdResult) {
        return callContext.throwError(resourceIdResult.moveError());
    }

    auto resourceId = resourceIdResult.moveValue();

    auto bundle = _resourceManager.getBundle(resourceId.bundleName);

    auto assetCatalogResult = bundle->getAssetCatalog(resourceId.resourcePath);
    if (!assetCatalogResult) {
        return callContext.throwError(assetCatalogResult.moveError());
    }

    const auto& assets = assetCatalogResult.value()->getAssets();

    auto assetsArray = callContext.getContext().newArray(assets.size(), callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    auto pathPrefix = resourceId.bundleName.append(":");

    auto widthName = callContext.getContext().newPropertyName("width");
    auto heightName = callContext.getContext().newPropertyName("height");
    auto pathName = callContext.getContext().newPropertyName("path");

    size_t i = 0;
    for (const auto& assetSpecs : assets) {
        auto path = pathPrefix.append(assetSpecs.getName());

        auto pathResult =
            callContext.getContext().newStringUTF8(path.toStringView(), callContext.getExceptionTracker());
        CHECK_CALL_CONTEXT(callContext);

        switch (assetSpecs.getType()) {
            case AssetSpecsType::IMAGE: {
                auto asset = _resourceManager.getAssetsManager()->getAsset(AssetKey(bundle, assetSpecs.getName()));

                auto wrappedObject =
                    makeWrappedObject(callContext.getContext(), asset, callContext.getExceptionTracker(), false);
                CHECK_CALL_CONTEXT(callContext);

                auto assetWidth = callContext.getContext().newNumber(static_cast<int32_t>(assetSpecs.getWidth()));
                auto assetHeight = callContext.getContext().newNumber(static_cast<int32_t>(assetSpecs.getHeight()));

                callContext.getContext().setObjectProperty(
                    wrappedObject.get(), widthName.get(), assetWidth.get(), callContext.getExceptionTracker());
                CHECK_CALL_CONTEXT(callContext);

                callContext.getContext().setObjectProperty(
                    wrappedObject.get(), heightName.get(), assetHeight.get(), callContext.getExceptionTracker());
                CHECK_CALL_CONTEXT(callContext);

                callContext.getContext().setObjectProperty(
                    wrappedObject.get(), pathName.get(), pathResult.get(), callContext.getExceptionTracker());
                CHECK_CALL_CONTEXT(callContext);

                callContext.getContext().setObjectPropertyIndex(
                    assetsArray.get(), i++, wrappedObject.get(), callContext.getExceptionTracker());
                CHECK_CALL_CONTEXT(callContext);
            } break;
            case AssetSpecsType::FONT: {
                callContext.getContext().setObjectPropertyIndex(
                    assetsArray.get(), i++, pathResult.get(), callContext.getExceptionTracker());
                CHECK_CALL_CONTEXT(callContext);
            } break;
            case AssetSpecsType::UNKNOWN:
                break;
        }
    }

    return assetsArray;
}

JSValueRef JavaScriptRuntime::runtimeSetColorPalette(JSFunctionNativeCallContext& callContext) {
    if (!_isWorker) {
        auto colorPaletteMap = callContext.getParameterAsValue(0);
        CHECK_CALL_CONTEXT(callContext);

        dispatchOnMainThread([weakSelf = weakRef(this), colorPaletteMap = std::move(colorPaletteMap)]() {
            auto self = weakSelf.lock();
            if (self && self->_listener != nullptr) {
                self->_listener->updateColorPalette(colorPaletteMap);
            }
        });
    }
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeOnMainThreadIdle(JSFunctionNativeCallContext& callContext) {
    auto callback = callContext.getParameterAsFunction(0);
    CHECK_CALL_CONTEXT(callContext);

    _mainThreadManager.onIdle(callback);

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeScheduleWorkItem(JSFunctionNativeCallContext& callContext) {
    bool interruptible = false;
    auto func =
        JSValueRefHolder::makeRetainedCallback(callContext.getContext(),
                                               callContext.getParameter(0),
                                               ReferenceInfoBuilder(callContext.getReferenceInfo()).withParameter(0),
                                               callContext.getExceptionTracker());
    int64_t delayResult = 0;
    if (callContext.getParameterSize() > 1) {
        delayResult = callContext.getParameterAsInt(1);
        CHECK_CALL_CONTEXT(callContext);
    }
    if (callContext.getParameterSize() > 2) {
        interruptible = callContext.getParameterAsBool(2);
        CHECK_CALL_CONTEXT(callContext);
    }

    auto delayMs = std::chrono::milliseconds(static_cast<int64_t>(delayResult));

    auto funcContext = func->getContext();
    auto dispatchFunc = makeJsThreadDispatchFunction(
        Ref(std::move(funcContext)), [func = std::move(func), interruptible](JavaScriptEntryParameters& jsEntry) {
            auto jsValue = func->getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
            if (!jsEntry.exceptionTracker) {
                if (interruptible) {
                    jsEntry.exceptionTracker.clearError();
                }
                return;
            }

            JSFunctionCallContext callContext(jsEntry.jsContext, nullptr, 0, jsEntry.exceptionTracker);
            jsEntry.jsContext.callObjectAsFunction(jsValue, callContext);
        });

    task_id_t taskId = _dispatchQueue->asyncAfter(std::move(dispatchFunc), delayMs);

    // Return a numeric handle that can be used by clearTimeout
    return callContext.getContext().newNumber(static_cast<int32_t>(taskId));
}

JSValueRef JavaScriptRuntime::runtimeUnscheduleWorkItem(JSFunctionNativeCallContext& callContext) {
    auto taskId = callContext.getParameterAsInt(0);
    CHECK_CALL_CONTEXT(callContext);
    _dispatchQueue->cancel(static_cast<int64_t>(taskId));

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimePushCurrentContext(JSFunctionNativeCallContext& callContext) {
    ContextId contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);

    Ref<Context> newCurrentContext;
    if (contextId == ContextIdNull) {
        newCurrentContext = _globalContext;
    } else {
        auto result = getContextForId(contextId);
        if (!result) {
            return callContext.throwError(result.moveError());
        }
        newCurrentContext = result.moveValue();
    }

    auto previous = Context::currentRef();
    Context::setCurrent(newCurrentContext);

    _contextsStack.emplace_back(std::move(previous));

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimePopCurrentContext(JSFunctionNativeCallContext& callContext) {
    if (_contextsStack.empty()) {
        return callContext.throwError(Error("Context stack is empty"));
    }

    auto context = std::move(_contextsStack[_contextsStack.size() - 1]);
    _contextsStack.pop_back();

    Context::setCurrent(std::move(context));

    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeGetCurrentContext(JSFunctionNativeCallContext& callContext) {
    auto* context = Context::current();
    return callContext.getContext().newNumber(static_cast<int32_t>(context->getContextId()));
}

JSValueRef JavaScriptRuntime::runtimeSaveCurrentContext(JSFunctionNativeCallContext& callContext) {
    // store the current context
    auto context = Context::currentRef();
    _contextsStash.emplace_back(context);
    // and return its id so that it can be used to restore it
    return callContext.getContext().newNumber(static_cast<int32_t>(context->getContextId()));
}

JSValueRef JavaScriptRuntime::runtimeRestoreCurrentContext(JSFunctionNativeCallContext& callContext) {
    ContextId contextId = getParameterAsContextId(callContext, 0);
    CHECK_CALL_CONTEXT(callContext);
    // scan the vector of saved contexts for a matching id
    for (auto i = _contextsStash.begin(); i != _contextsStash.end(); ++i) {
        if ((*i)->getContextId() == contextId) {
            // apply current context
            Context::setCurrent(*i);
            // swap the target context with the context at the end
            // this is safe because we know the vector is not empty
            std::swap(*i, _contextsStash.back());
            // delete the last element from the vector
            _contextsStash.pop_back();
            break;
        }
    };
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeDumpMemoryStatistics(JSFunctionNativeCallContext& callContext) {
    auto memoryStatistics = callContext.getContext().dumpMemoryStatistics();
    auto serializedMemoryStatistics =
        Value()
            .setMapValue("memoryUsageBytes", Value(static_cast<double>(memoryStatistics.memoryUsageBytes)))
            .setMapValue("objectsCount", Value(static_cast<double>(memoryStatistics.objectsCount)));

    return valueToJSValue(callContext.getContext(),
                          serializedMemoryStatistics,
                          ReferenceInfoBuilder(),
                          callContext.getExceptionTracker());
}

JSValueRef JavaScriptRuntime::runtimePerformGC(JSFunctionNativeCallContext& callContext) {
    dispatchPerformGcToWorkers();
    performGcNow(callContext.getContext());
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::runtimeHeapDump(JSFunctionNativeCallContext& callContext) {
    auto dumpHeapResult = dumpHeap();
    CHECK_CALL_CONTEXT(callContext);
    return callContext.getContext().newArrayBuffer(dumpHeapResult.success() ? dumpHeapResult.value() : BytesView(),
                                                   callContext.getExceptionTracker());
}

static JSValueRef updateErrorHandler(JSFunctionNativeCallContext& callContext, Shared<JSValueRefHolder>& holder) {
    auto callback = callContext.getParameter(0);
    if (callContext.getContext().isValueFunction(callback)) {
        holder = JSValueRefHolder::makeRetainedCallback(
            callContext.getContext(),
            callback,
            ReferenceInfoBuilder(callContext.getReferenceInfo()).withParameter(0),
            callContext.getExceptionTracker());
    } else {
        holder = nullptr;
    }

    return callContext.getContext().newUndefined();
}

enum class ExceptionHandlerResult {
    NOTIFY = 0,
    IGNORE = 1,
    CRASH = 2,
};

/**
 * Call the error handler and return whether the error was fully handled.
 * If the function return false, shouldCrash will be set on whether
 * handling the error at the native level should result in a crash.
 */
static bool notifyErrorHandler(IJavaScriptContext& jsContext,
                               const Shared<JSValueRefHolder>& holder,
                               const Ref<Context>& context,
                               const JSValue& error,
                               bool& shouldCrash) {
    if (holder == nullptr) {
        return false;
    }
    JSExceptionTracker exceptionTracker(jsContext);
    auto jsValue = holder->getJsValue(jsContext, exceptionTracker);
    if (!exceptionTracker) {
        exceptionTracker.clearError();
        return false;
    }

    std::initializer_list<JSValueRef> parameters = {
        JSValueRef::makeRetained(jsContext, error),
        jsContext.newNumber(context != nullptr ? static_cast<int32_t>(context->getContextId()) : 0)};

    JSFunctionCallContext callContext(
        jsContext, parameters.begin(), /* parametersSize */ parameters.size(), exceptionTracker);
    auto result = jsContext.callObjectAsFunction(jsValue, callContext);
    if (!exceptionTracker) {
        exceptionTracker.clearError();
        return false;
    }

    auto handlerResult = static_cast<ExceptionHandlerResult>(jsContext.valueToInt(result.get(), exceptionTracker));
    if (!exceptionTracker) {
        exceptionTracker.clearError();
        return false;
    }

    switch (handlerResult) {
        case ExceptionHandlerResult::NOTIFY:
            return false;
        case ExceptionHandlerResult::IGNORE:
            return true;
        case ExceptionHandlerResult::CRASH:
            shouldCrash = true;
            return false;
    }

    SC_ASSERT_FAIL("Invalid exception handler result");

    return false;
}

JSValueRef JavaScriptRuntime::runtimeSetUncaughtExceptionHandler(JSFunctionNativeCallContext& callContext) {
    return updateErrorHandler(callContext, _uncaughtExceptionHandler);
}

JSValueRef JavaScriptRuntime::runtimeSetUnhandledRejectionHandler(JSFunctionNativeCallContext& callContext) {
    return updateErrorHandler(callContext, _unhandledRejectionHandler);
}

JSValueRef JavaScriptRuntime::runtimeCreateWorker(JSFunctionNativeCallContext& callContext) {
    auto workerRuntime = makeShared<JavaScriptRuntime>(_javaScriptBridge,
                                                       _resourceManager,
                                                       _contextManager,
                                                       _mainThreadManager,
                                                       _attributeIds,
                                                       _platformType,
                                                       _enableDebugger,
                                                       ThreadQoSClassHigh,
                                                       _anrDetector,
                                                       _logger,
                                                       true);
    workerRuntime->postInit();
    for (const auto& moduleFactory : _moduleFactories) {
        workerRuntime->registerJavaScriptModuleFactory(moduleFactory);
    }
    for (const auto& typeConverter : _typeConverters) {
        workerRuntime->registerTypeConverter(typeConverter.typeName, typeConverter.functionPath);
    }
    auto worker = makeShared<JavaScriptWorker>(workerRuntime, callContext.getParameterAsString(0));
    worker->postInit();
    CHECK_CALL_CONTEXT(callContext);
    // worker->init(); // call outside of ctor so that shared_from_this() is available
    auto workerJSValue = callContext.getContext().newWrappedObject(worker, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);
    bindRuntimeFunction(callContext.getContext(),
                        callContext.getExceptionTracker(),
                        workerJSValue,
                        "setOnMessage",
                        &JavaScriptRuntime::workerSetOnMessage);
    CHECK_CALL_CONTEXT(callContext);
    bindRuntimeFunction(callContext.getContext(),
                        callContext.getExceptionTracker(),
                        workerJSValue,
                        "postMessage",
                        &JavaScriptRuntime::workerPostMessage);
    CHECK_CALL_CONTEXT(callContext);
    bindRuntimeFunction(callContext.getContext(),
                        callContext.getExceptionTracker(),
                        workerJSValue,
                        "terminate",
                        &JavaScriptRuntime::workerTerminate);
    CHECK_CALL_CONTEXT(callContext);
    _jsWorkers.emplace_back(weakRef(workerRuntime.get()));
    return workerJSValue;
}

JSValueRef JavaScriptRuntime::queueMicrotask(JSFunctionNativeCallContext& callContext) {
    callContext.getContext().enqueueMicrotask(callContext.getParameter(0), callContext.getExceptionTracker());
    return callContext.getContext().newUndefined();
}

JSValueRef JavaScriptRuntime::performanceNow(JSFunctionNativeCallContext& callContext) {
    return callContext.getContext().newNumber(_upTime.elapsed().milliseconds());
}

template<typename T>
Ref<T> thisFromCallContext(JSFunctionNativeCallContext& callContext) {
    auto val =
        callContext.getContext().valueToWrappedObject(callContext.getThisValue(), callContext.getExceptionTracker());
    return castOrNull<T>(val);
}

// worker.onmessage = func
JSValueRef JavaScriptRuntime::workerSetOnMessage(JSFunctionNativeCallContext& callContext) {
    auto worker = thisFromCallContext<JavaScriptWorker>(callContext);
    if (worker != nullptr) {
        worker->setHostOnMessage(callContext.getParameterAsFunction(0));
        CHECK_CALL_CONTEXT(callContext);
    }
    return callContext.getContext().newUndefined();
}

// worker.postMessage(any)
JSValueRef JavaScriptRuntime::workerPostMessage(JSFunctionNativeCallContext& callContext) {
    auto worker = thisFromCallContext<JavaScriptWorker>(callContext);
    if (worker != nullptr) {
        worker->postMessage(callContext.getParameterAsValue(0));
        CHECK_CALL_CONTEXT(callContext);
    }
    return callContext.getContext().newUndefined();
}

// worker.terminate()
JSValueRef JavaScriptRuntime::workerTerminate(JSFunctionNativeCallContext& callContext) {
    auto worker = thisFromCallContext<JavaScriptWorker>(callContext);
    if (worker != nullptr) {
        worker->close();
    }
    return callContext.getContext().newUndefined();
}

constexpr static std::string_view getBuildType() {
    if (snap::kIsDevBuild) {
        return "dev";
    }

    if (snap::kIsGoldBuild) {
        return "gold";
    }

    if (snap::kIsAlphaBuild) {
        return "alpha";
    }

    if (snap::kIsAppstoreBuild) {
        return "prod";
    }

    return "unknown";
}

void JavaScriptRuntime::buildContext(Valdi::IJavaScriptContext& context,
                                     const Ref<ValdiRuntimeTweaks>& tweaks,
                                     JSExceptionTracker& exceptionTracker) {
    VALDI_TRACE("Valdi.configureJsContext");

    context.setListener(this);

    auto globalObject = context.getGlobalObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    // Expose the global object
    context.setObjectProperty(globalObject.get(), "global", globalObject.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto runtimeObject = context.newObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    JS_BIND(context, exceptionTracker, runtimeObject, "postMessage", runtimePostMessage);

    auto jsValdiVersion = context.newNumber(static_cast<int32_t>(valdiVersion));

    context.setObjectProperty(runtimeObject.get(), "version", jsValdiVersion.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto jsEnableDebugger = context.newBool(_enableDebugger);

    context.setObjectProperty(runtimeObject.get(), "isDebugEnabled", jsEnableDebugger.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    auto buildType = context.newStringUTF8(getBuildType(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    context.setObjectProperty(runtimeObject.get(), "buildType", buildType.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    std::string_view moduleLoaderTypeStr =
        (tweaks != nullptr && tweaks->enableCommonJsModuleLoader()) ? "commonjs" : "valdi";

    auto moduleLoaderType = context.newStringUTF8(moduleLoaderTypeStr, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    context.setObjectProperty(runtimeObject.get(), "moduleLoaderType", moduleLoaderType.get(), exceptionTracker);

    if (!exceptionTracker) {
        return;
    }

    JS_BIND(context, exceptionTracker, runtimeObject, "outputLog", runtimeOutputLog);
    JS_BIND(context, exceptionTracker, runtimeObject, "getFrameForElementId", runtimeGetFrameForViewId);
    JS_BIND(context, exceptionTracker, runtimeObject, "getNativeViewForElementId", runtimeGetNativeViewForViewId);
    JS_BIND(context, exceptionTracker, runtimeObject, "takeElementSnapshot", runtimeTakeElementSnapshot);
    JS_BIND(context, exceptionTracker, runtimeObject, "makeOpaque", runtimeMakeOpaque);
    JS_BIND(context, exceptionTracker, runtimeObject, "bytesToString", runtimeBytesToString);
    JS_BIND(context, exceptionTracker, runtimeObject, "configureCallback", runtimeConfigureCallback);
    JS_BIND(context, exceptionTracker, runtimeObject, "getViewNodeDebugInfo", runtimeGetViewNodeDebugInfo);
    JS_BIND(context, exceptionTracker, runtimeObject, "getLayoutDebugInfo", runtimeGetLayoutDebugInfo);
    JS_BIND(context, exceptionTracker, runtimeObject, "getNativeNodeForElementId", runtimeGetNativeNodeForElementId);
    JS_BIND(context, exceptionTracker, runtimeObject, "performSyncWithMainThread", runtimePerformSyncWithMainThread);
    JS_BIND(context, exceptionTracker, runtimeObject, "callOnMainThread", runtimeCallOnMainThread);
    JS_BIND(context, exceptionTracker, runtimeObject, "isModuleLoaded", runtimeIsModuleLoaded);
    JS_BIND(context, exceptionTracker, runtimeObject, "getModuleEntry", runtimeGetModuleEntry);
    JS_BIND(context, exceptionTracker, runtimeObject, "getModuleJsPaths", runtimeGetModuleJsPaths);
    JS_BIND(context, exceptionTracker, runtimeObject, "loadModule", runtimeLoadModule);
    JS_BIND(context, exceptionTracker, runtimeObject, "loadJsModule", runtimeLoadJsModule);
    JS_BIND(context, exceptionTracker, runtimeObject, "getSourceMap", runtimeGetSourceMap);
    JS_BIND(context, exceptionTracker, runtimeObject, "pushCurrentContext", runtimePushCurrentContext);
    JS_BIND(context, exceptionTracker, runtimeObject, "popCurrentContext", runtimePopCurrentContext);
    JS_BIND(context, exceptionTracker, runtimeObject, "getCurrentContext", runtimeGetCurrentContext);
    JS_BIND(context, exceptionTracker, runtimeObject, "saveCurrentContext", runtimeSaveCurrentContext);
    JS_BIND(context, exceptionTracker, runtimeObject, "restoreCurrentContext", runtimeRestoreCurrentContext);
    JS_BIND(context, exceptionTracker, runtimeObject, "getAssets", runtimeGetAssets);
    JS_BIND(context, exceptionTracker, runtimeObject, "makeAssetFromUrl", runtimeMakeAssetFromUrl);
    JS_BIND(context, exceptionTracker, runtimeObject, "makeAssetFromBytes", runtimeMakeAssetFromBytes);
    JS_BIND(context, exceptionTracker, runtimeObject, "addAssetLoadObserver", runtimeAddAssetLoadObserver);

    JS_BIND(context, exceptionTracker, runtimeObject, "makeDirectionalAsset", runtimeMakeDirectionalAsset);
    JS_BIND(context, exceptionTracker, runtimeObject, "makePlatformSpecificAsset", runtimeMakePlatformSpecificAsset);
    JS_BIND(context, exceptionTracker, runtimeObject, "getLoadedAssetMetadata", runtimeGetLoadedAssetMetadata);
    JS_BIND(context, exceptionTracker, runtimeObject, "setColorPalette", runtimeSetColorPalette);
    JS_BIND(context, exceptionTracker, runtimeObject, "onMainThreadIdle", runtimeOnMainThreadIdle);
    JS_BIND(context, exceptionTracker, runtimeObject, "createWorker", runtimeCreateWorker);

    // Rendering
    JS_BIND(context, exceptionTracker, runtimeObject, "submitRawRenderRequest", runtimeSubmitRenderRequest);

    JS_BIND(context, exceptionTracker, runtimeObject, "getCurrentPlatform", runtimeGetCurrentPlatform);
    JS_BIND(context,
            exceptionTracker,
            runtimeObject,
            "getBackendRenderingTypeForContextId",
            runtimeGetBackendRenderingTypeForContextId);

    JS_BIND(context, exceptionTracker, runtimeObject, "protectNativeRefs", runtimeProtectNativeRefs);

    JS_BIND(context, exceptionTracker, runtimeObject, "getCSSModule", runtimeGetCSSModule);
    JS_BIND(context, exceptionTracker, runtimeObject, "createCSSRule", runtimeCreateCSSRule);

    JS_BIND(context, exceptionTracker, runtimeObject, "internString", runtimeInternString);
    JS_BIND(context, exceptionTracker, runtimeObject, "getAttributeId", runtimeGetAttributeId);

    JS_BIND(context, exceptionTracker, runtimeObject, "createContext", runtimeCreateContext);
    JS_BIND(context, exceptionTracker, runtimeObject, "destroyContext", runtimeDestroyContext);

    JS_BIND(context, exceptionTracker, runtimeObject, "setLayoutSpecs", runtimeSetLayoutSpecs);
    JS_BIND(context, exceptionTracker, runtimeObject, "measureContext", runtimeMeasureContext);

    // Debugging
    JS_BIND(context, exceptionTracker, runtimeObject, "submitDebugMessage", runtimeSubmitDebugMessage);
    JS_BIND(context, exceptionTracker, runtimeObject, "onUncaughtError", runtimeOnUncaughtError);

    if constexpr (Valdi::kTracingEnabled) {
        JS_BIND(context, exceptionTracker, runtimeObject, "trace", runtimeTrace);
        JS_BIND(context, exceptionTracker, runtimeObject, "makeTraceProxy", runtimeMakeTraceProxy);
    }

    JS_BIND(context, exceptionTracker, runtimeObject, "startTraceRecording", runtimeStartTraceRecording);
    JS_BIND(context, exceptionTracker, runtimeObject, "stopTraceRecording", runtimeStopTraceRecording);

    JS_BIND(context, exceptionTracker, runtimeObject, "scheduleWorkItem", runtimeScheduleWorkItem);
    JS_BIND(context, exceptionTracker, runtimeObject, "unscheduleWorkItem", runtimeUnscheduleWorkItem);

    JS_BIND(context, exceptionTracker, runtimeObject, "dumpMemoryStatistics", runtimeDumpMemoryStatistics);
    JS_BIND(context, exceptionTracker, runtimeObject, "performGC", runtimePerformGC);

    JS_BIND(
        context, exceptionTracker, runtimeObject, "setUncaughtExceptionHandler", runtimeSetUncaughtExceptionHandler);
    JS_BIND(
        context, exceptionTracker, runtimeObject, "setUnhandledRejectionHandler", runtimeSetUnhandledRejectionHandler);

    if constexpr (Valdi::shouldEnableJsHeapDump()) {
        JS_BIND(context, exceptionTracker, runtimeObject, "dumpHeap", runtimeHeapDump);
    }

    context.setObjectProperty(globalObject.get(), "runtime", runtimeObject.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto performanceObject = context.newObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    JS_BIND(context, exceptionTracker, performanceObject, "now", performanceNow);

    auto timeOrigin = getPerformanceTimeOrigin();
    context.setObjectProperty(
        performanceObject.get(), "timeOrigin", context.newNumber(timeOrigin.getTime()).get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    context.setObjectProperty(globalObject.get(), "performance", performanceObject.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto processKey = STRING_LITERAL("process");

    auto processObject = makeShared<ValueMap>();
    (*processObject)[STRING_LITERAL("version")] = Value(42);
    (*processObject)[STRING_LITERAL("pid")] = Value(1);
    (*processObject)[STRING_LITERAL("arch")] = Value(STRING_LITERAL("arm64"));
    (*processObject)[STRING_LITERAL("env")] = Value(makeShared<ValueMap>());

    auto processResult =
        valueToJSValue(context, Value(processObject), ReferenceInfoBuilder().withObject(processKey), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    JS_BIND(context, exceptionTracker, globalObject, "queueMicrotask", queueMicrotask);

    context.setObjectProperty(runtimeObject.get(), processKey.toStringView(), processResult.get(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
}

Result<Void> JavaScriptRuntime::onComponentSetupFailed(const ResourceId& resourceId, const Error& error) {
    return Error(error.rethrow(STRING_FORMAT("Failed to create component {}", resourceId)));
}

Ref<ContextHandler> JavaScriptRuntime::getContextHandler() const {
    return _contextHandler;
}

void JavaScriptRuntime::requestUpdateJsContextHandler(JavaScriptEntryParameters& jsEntry,
                                                      JavaScriptComponentContextHandler& /*handler*/) {
    ensureValdiModuleIsLoaded(jsEntry);
}

std::vector<Ref<JavaScriptRuntime>> JavaScriptRuntime::getAllWorkers() {
    std::vector<Ref<JavaScriptRuntime>> out;
    out.reserve(_jsWorkers.size());

    auto it = _jsWorkers.begin();
    while (it != _jsWorkers.end()) {
        auto jsWorker = it->lock();
        if (jsWorker != nullptr) {
            out.emplace_back(std::move(jsWorker));
            it++;
        } else {
            it = _jsWorkers.erase(it);
        }
    }

    return out;
}

void JavaScriptRuntime::dispatchPerformGcToWorkers() {
    for (const auto& jsWorker : getAllWorkers()) {
        jsWorker->performGc();
    }
}

void JavaScriptRuntime::performGc() {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        dispatchPerformGcToWorkers();
        performGcNow(entry.jsContext);
    });
}

JavaScriptContextMemoryStatistics JavaScriptRuntime::dumpMemoryStatistics() {
    JavaScriptContextMemoryStatistics stats;
    dispatchSynchronouslyOnJsThread(
        [=, &stats](JavaScriptEntryParameters& entry) { stats = this->dumpMemoryStatistics(entry.jsContext); });
    return stats;
}

Shared<JavaScriptModuleContainer> JavaScriptRuntime::getValdiModule(JavaScriptEntryParameters& jsEntry) {
    return importModule(valdiModuleResourceId(), jsEntry);
}

void JavaScriptRuntime::ensureValdiModuleIsLoaded(JavaScriptEntryParameters& jsEntry) {
    getValdiModule(jsEntry);
}

bool JavaScriptRuntime::callComponentFunction(ContextId contextId,
                                              const StringBox& functionName,
                                              const Ref<ValueArray>& additionalParameters) {
    auto context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        return false;
    }

    callComponentFunction(context, functionName, additionalParameters);

    return true;
}

void JavaScriptRuntime::callComponentFunction(const Ref<Context>& context, const StringBox& functionName) {
    callComponentFunction(context, functionName, nullptr);
}

void JavaScriptRuntime::callComponentFunction(const Ref<Context>& context,
                                              const StringBox& functionName,
                                              const Ref<ValueArray>& additionalParameters) {
    _contextHandler->callComponentFunction(context, functionName, additionalParameters);
}

void JavaScriptRuntime::callModuleFunction(const StringBox& module,
                                           const StringBox& functionName,
                                           const Ref<ValueArray>& parameters) {
    dispatchOnJsThreadAsync(nullptr, [this, module, functionName, parameters](auto& jsEntry) {
        auto importResult = this->importModule(module, jsEntry);
        if (!jsEntry.exceptionTracker) {
            return;
        }

        auto moduleJsValue = importResult->getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            return;
        }

        if (parameters != nullptr && !parameters->empty()) {
            std::vector<JSValueRef> jsParameters;
            for (const auto& parameter : *parameters) {
                auto jsParameter =
                    valueToJSValue(jsEntry.jsContext, parameter, ReferenceInfoBuilder(), jsEntry.exceptionTracker);
                if (!jsEntry.exceptionTracker) {
                    return;
                }

                jsParameters.emplace_back(jsEntry.jsContext.ensureRetainedValue(std::move(jsParameter)));
            }

            JSFunctionCallContext callContext(
                jsEntry.jsContext, jsParameters.data(), jsParameters.size(), jsEntry.exceptionTracker);
            jsEntry.jsContext.callObjectProperty(moduleJsValue, functionName.toStringView(), callContext);
        } else {
            JSFunctionCallContext callContext(jsEntry.jsContext, nullptr, 0, jsEntry.exceptionTracker);
            jsEntry.jsContext.callObjectProperty(moduleJsValue, functionName.toStringView(), callContext);
        }
    });
}

Error JavaScriptRuntime::onModuleLoadFailed(const Error& error, const ResourceId& resourceId) {
    return error.rethrow(STRING_FORMAT("Failed to load JS module '{}'", resourceId));
}

void JavaScriptRuntime::initializeModuleLoader(JavaScriptEntryParameters& jsEntry) {
    loadJsModule(jsEntry.jsContext, STRING_LITERAL("valdi_core/src/Init"), true, nullptr, 0, jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }
    auto globalObjectResult = jsEntry.jsContext.getGlobalObject(jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    _moduleLoader =
        jsEntry.jsContext.getObjectProperty(globalObjectResult.get(), "moduleLoader", jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }
    _moduleLoader = jsEntry.jsContext.ensureRetainedValue(std::move(_moduleLoader));

    auto longObject = jsEntry.jsContext.getObjectProperty(globalObjectResult.get(), "Long", jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    if (!jsEntry.jsContext.isValueFunction(longObject.get())) {
        jsEntry.exceptionTracker.onError(Error("Failed to resolve Long constructor"));
        return;
    }

    jsEntry.jsContext.setLongConstructor(longObject);
}

Shared<JavaScriptModuleContainer> JavaScriptRuntime::importModule(const StringBox& path,
                                                                  JavaScriptEntryParameters& jsEntry) {
    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(path);
    if (!resourceIdResult) {
        return nullptr;
    }

    return importModule(resourceIdResult.value(), jsEntry);
}

Shared<JavaScriptModuleContainer> JavaScriptRuntime::importModule(const ResourceId& resourceId,
                                                                  JavaScriptEntryParameters& jsEntry) {
    const auto& it = _modules.find(resourceId);
    if (it != _modules.end()) {
        return it->second;
    }

    auto absoluteModulePath = resourceId.toAbsolutePath();

    VALDI_TRACE_META("Valdi.importModule", absoluteModulePath);
    JavaScriptContextEntry contextEntry(_globalContext);

    auto bundle = _resourceManager.getBundle(resourceId.bundleName);

    auto path = jsEntry.jsContext.newStringUTF8(absoluteModulePath, jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return nullptr;
    }

    std::initializer_list<JSValueRef> params = {
        std::move(path),
        jsEntry.jsContext.newBool(true), // disableProxy
    };

    JSFunctionCallContext callContext(jsEntry.jsContext, params.begin(), params.size(), jsEntry.exceptionTracker);

    auto result = jsEntry.jsContext.callObjectProperty(
        _moduleLoader.get(), _propertyNameIndex.getJsName(kLoadPropertyName), callContext);
    if (!jsEntry.exceptionTracker) {
        return nullptr;
    }

    auto jsModuleContainer = Valdi::makeShared<JavaScriptModuleContainer>(
        std::move(bundle), jsEntry.jsContext, result.get(), jsEntry.exceptionTracker);
    _modules[resourceId] = jsModuleContainer;

    onModuleLoaded(jsEntry.jsContext, resourceId, result.get(), /* isFromJsEval */ false, jsEntry.exceptionTracker);

    return jsModuleContainer;
}

void JavaScriptRuntime::onModuleLoaded(IJavaScriptContext& jsContext,
                                       const ResourceId& resourceId,
                                       const JSValue& value,
                                       bool isFromJsEval,
                                       JSExceptionTracker& exceptionTracker) {
    if (!exceptionTracker) {
        return;
    }

    if (resourceId == valdiModuleResourceId()) {
        _onDaemonClientEventFunction = exceptionTracker.toRetainedResult(
            jsContext.getObjectProperty(value, "onDaemonClientEvent", exceptionTracker));
        if (!_onDaemonClientEventFunction) {
            onRecoverableError("onModuleLoaded", _onDaemonClientEventFunction.error());
        }

        if (!isFromJsEval) {
            // TODO(simon): We can't stash the JSContextHandler during a jsEval.
            auto makeRootComponentsManagerFunction = exceptionTracker.toResult(
                jsContext.getObjectProperty(value, "makeRootComponentsManager", exceptionTracker));
            if (makeRootComponentsManagerFunction) {
                JSFunctionCallContext callContext(jsContext, nullptr, 0, exceptionTracker);

                auto result =
                    jsContext.callObjectAsFunction(makeRootComponentsManagerFunction.value().get(), callContext);
                if (callContext.getExceptionTracker()) {
                    static auto kRootComponentManagerKey = STRING_LITERAL("RootComponentManager");

                    _contextHandler->setJsContextHandler(makeShared<JSValueRefHolder>(
                        jsContext,
                        result.get(),
                        ReferenceInfoBuilder().withObject(kRootComponentManagerKey).build(),
                        exceptionTracker,
                        false));
                } else {
                    _contextHandler->setJsContextHandler(callContext.getExceptionTracker().extractError());
                }

            } else {
                _contextHandler->setJsContextHandler(makeRootComponentsManagerFunction.error());
            }
        }

        if (!_daemonClients.empty() && _onDaemonClientEventFunction) {
            for (const auto& it : _daemonClients) {
                notifyDaemonClientEvent(jsContext,
                                        DaemonClientEventTypeConnected,
                                        it.second.get(),
                                        JSValue(),
                                        _onDaemonClientEventFunction.value().get(),
                                        exceptionTracker);
            }
        }
    }
}

void JavaScriptRuntime::onModuleUnloaded(const ResourceId& resourceId, JavaScriptEntryParameters& /*jsEntry*/) {
    if (resourceId == valdiModuleResourceId()) {
        _onDaemonClientEventFunction = Result<JSValueRef>();

        _contextHandler->removeJsContextHandler();

        if (_contextHandler->needsJsContextHandler()) {
            _pendingModulesToReload.insert(resourceId);
        }
    } else if (resourceId == symbolicatorResourceId()) {
        _symbolicateFunction = Result<JSValueRef>();
    }
}

void JavaScriptRuntime::unloadAllModules() {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        if (!_modules.empty()) {
            std::vector<ResourceId> resourceIds;
            for (const auto& it : _modules) {
                resourceIds.emplace_back(it.first);
            }

            doUnloadModulesAndDependentModules(resourceIds, false, entry);
        }

        performGcNow(entry.jsContext);
    });
}

void JavaScriptRuntime::unloadUnusedModules(const Ref<AsyncGroup>& completionGroup) {
    completionGroup->enter();

    dispatchOnJsThreadUnattributed([this, completionGroup = completionGroup](JavaScriptEntryParameters& entry) {
        for (const auto& jsWorker : getAllWorkers()) {
            jsWorker->unloadUnusedModules(completionGroup);
        }

        if (_isWorker) {
            // TODO(simon): We cannot yet reliably detect which modules are used within workers,
            // so we just perform a GC instead
            performGcNow(entry.jsContext);
        } else {
            // Unload modules as much as we can, then kick off a GC.
            // Re-do this process until we can't unload a module anymore.
            auto unloaded = true;
            while (unloaded) {
                unloaded = false;
                while (unloadNextUnusedModule(entry)) {
                    unloaded = true;
                }

                performGcNow(entry.jsContext);
            }
        }

        completionGroup->leave();
    });
}

void JavaScriptRuntime::unloadUnusedModules(DispatchFunction completion) {
    auto group = makeShared<AsyncGroup>();

    unloadUnusedModules(group);

    group->notify(std::move(completion));
}

FlatSet<ResourceId> JavaScriptRuntime::getAllUsedModules() const {
    FlatSet<ResourceId> allUsedModules;
    for (const auto& context : _contextManager.getAllContexts()) {
        allUsedModules.insert(context->getPath().getResourceId());
    }
    for (const auto& it : _modules) {
        allUsedModules.insert(it.first);
    }
    return allUsedModules;
}

bool JavaScriptRuntime::unloadNextUnusedModule(JavaScriptEntryParameters& entry) {
    auto allUsedModules = getAllUsedModules();

    auto allUsedModulesArray = ValueArray::make(allUsedModules.size());
    size_t i = 0;
    for (const auto& resId : allUsedModules) {
        allUsedModulesArray->emplace(i++, Value(StringCache::getGlobal().makeString(resId.toAbsolutePath())));
    }

    auto allUsedModulesJs =
        valueToJSValue(entry.jsContext, Value(allUsedModulesArray), ReferenceInfoBuilder(), entry.exceptionTracker);
    if (!entry.exceptionTracker) {
        onRecoverableError("unloadNextUnusedModule", entry.exceptionTracker);
        return false;
    }

    JSFunctionCallContext callContext(entry.jsContext, &allUsedModulesJs, 1, entry.exceptionTracker);
    auto result = entry.jsContext.callObjectProperty(
        _moduleLoader.get(), _propertyNameIndex.getJsName(kUnloadAllUnusedPropertyName), callContext);
    if (!entry.exceptionTracker) {
        onRecoverableError("unloadNextUnusedModule", entry.exceptionTracker);
        return false;
    }

    FlatSet<ResourceId> allUnloadedModules;
    onModulesUnloaded(result.get(), allUnloadedModules, entry);

    return !allUnloadedModules.empty();
}

bool JavaScriptRuntime::isInEvalMode() const {
    return !_modulesToAutoReload.empty();
}

void JavaScriptRuntime::reevalUnloadedModulesIfNeeded() {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        auto it = _pendingModulesToReload.begin();
        while (it != _pendingModulesToReload.end()) {
            importModule(*it, entry);
            if (!entry.exceptionTracker) {
                onRecoverableError("reevalModule", entry.exceptionTracker);
            }

            it = _pendingModulesToReload.erase(it);
        }
    });
}

void JavaScriptRuntime::unloadModulesAndDependentModules(const std::vector<ResourceId>& resourceIds,
                                                         bool isHotReloading) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        for (const auto& jsWorker : getAllWorkers()) {
            jsWorker->unloadModulesAndDependentModules(resourceIds, isHotReloading);
        }

        doUnloadModulesAndDependentModules(resourceIds, isHotReloading, entry);
        if (isHotReloading) {
            reevalUnloadedModulesIfNeeded();
        }
    });
}

bool JavaScriptRuntime::isJsModuleLoaded(const ResourceId& resourceId) {
    bool loaded = false;

    dispatchSynchronouslyOnJsThread([&](JavaScriptEntryParameters& entry) {
        auto pathResult = entry.jsContext.newStringUTF8(resourceId.toAbsolutePath(), entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("isJsModuleLoaded", entry.exceptionTracker);
            return;
        }

        JSFunctionCallContext params(entry.jsContext, &pathResult, 1, entry.exceptionTracker);
        auto result = entry.jsContext.callObjectProperty(
            _moduleLoader.get(), _propertyNameIndex.getJsName(kIsLoadedPropertyName), params);
        if (!entry.exceptionTracker) {
            onRecoverableError("isJsModuleLoaded", entry.exceptionTracker);
            return;
        }

        auto convertResult =
            jsValueToValue(entry.jsContext, result.get(), ReferenceInfoBuilder(), entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("isJsModuleLoaded", entry.exceptionTracker);
            return;
        }

        loaded = convertResult.toBool();
    });

    return loaded;
}

void JavaScriptRuntime::doUnloadModulesAndDependentModules(const std::vector<ResourceId>& resourceIds,
                                                           bool isHotReloading,
                                                           JavaScriptEntryParameters& entry) {
    ValueArrayBuilder paths;
    for (const auto& resourceId : resourceIds) {
        paths.emplace(StringCache::getGlobal().makeString(resourceId.toAbsolutePath()));
    }

    bool disableHotReloaderLazyDenylist = false;
    if (isHotReloading && _listener != nullptr) {
        auto runtimeTweaks = _listener->getRuntimeTweaks();
        if (runtimeTweaks != nullptr) {
            disableHotReloaderLazyDenylist = runtimeTweaks->disableHotReloaderLazyDenylist();
        }
    }

    auto pathsResult =
        valueToJSValue(entry.jsContext, Value(paths.build()), ReferenceInfoBuilder(), entry.exceptionTracker);

    if (!entry.exceptionTracker) {
        onRecoverableError("unloadModule", entry.exceptionTracker);
        return;
    }

    std::initializer_list<JSValueRef> params = {std::move(pathsResult),
                                                entry.jsContext.newBool(isHotReloading),
                                                entry.jsContext.newBool(disableHotReloaderLazyDenylist)};

    JSFunctionCallContext callContext(entry.jsContext, params.begin(), params.size(), entry.exceptionTracker);

    auto unloadResult = entry.jsContext.callObjectProperty(
        _moduleLoader.get(), _propertyNameIndex.getJsName(kUnloadPropertyName), callContext);

    if (!entry.exceptionTracker) {
        onRecoverableError("unloadModule", entry.exceptionTracker);
        return;
    }

    FlatSet<ResourceId> allRemovedModules;
    allRemovedModules.insert(resourceIds.begin(), resourceIds.end());

    onModulesUnloaded(unloadResult.get(), allRemovedModules, entry);
}

void JavaScriptRuntime::onModulesUnloaded(const JSValue& value,
                                          FlatSet<ResourceId>& allUnloadedModules,
                                          JavaScriptEntryParameters& entry) {
    auto convertResult = jsValueToValue(entry.jsContext, value, ReferenceInfoBuilder(), entry.exceptionTracker);
    if (entry.exceptionTracker) {
        auto result = convertResult.getArrayRef();
        if (result != nullptr) {
            for (const auto& item : *result) {
                auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(item.toStringBox());
                if (!resourceIdResult) {
                    onRecoverableError("onModuleUnloaded", resourceIdResult.error());
                    continue;
                }
                allUnloadedModules.insert(resourceIdResult.moveValue());
            }
        } else {
            onRecoverableError("onModuleUnloaded", Error("Expected array result when calling unload()"));
        }
    } else {
        onRecoverableError("onModuleUnloaded", entry.exceptionTracker);
    }

    for (const auto& resourceId : allUnloadedModules) {
        if (_modulesToAutoReload.contains(resourceId)) {
            _pendingModulesToReload.insert(resourceId);
        }

        auto it = _modules.find(resourceId);
        if (it != _modules.end()) {
            auto moduleContainer = it->second;
            _modules.erase(it);

            onModuleUnloaded(resourceId, entry);
            if (!entry.exceptionTracker) {
                onRecoverableError("onModuleUnloaded", entry.exceptionTracker);
            }

            moduleContainer->didUnload();
        }
    }
}

void JavaScriptRuntime::registerJavaScriptModuleFactory(const Ref<JavaScriptModuleFactory>& moduleFactory) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        _moduleFactories.push_back(moduleFactory);
        auto modulePath = moduleFactory->getModulePath();
        auto pathResult = entry.jsContext.newStringUTF8(modulePath.toStringView(), entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("registerJavaScriptModuleFactory", entry.exceptionTracker);
            return;
        }

        auto lambdaFunction = makeShared<JSFunctionWithCallable>(
            ReferenceInfoBuilder().withObject(modulePath), [=](JSFunctionNativeCallContext& callContext) -> JSValueRef {
                JavaScriptContextEntry jsEntry(_globalContext);
                return moduleFactory->loadModule(callContext.getContext(),
                                                 ReferenceInfoBuilder(callContext.getReferenceInfo()),
                                                 callContext.getExceptionTracker());
            });

        auto functionResult = entry.jsContext.newFunction(lambdaFunction, entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            onRecoverableError("registerJavaScriptModuleFactory", entry.exceptionTracker);
            return;
        }

        std::initializer_list<JSValueRef> params = {std::move(pathResult), std::move(functionResult)};

        JSFunctionCallContext callContext(entry.jsContext, params.begin(), params.size(), entry.exceptionTracker);

        entry.jsContext.callObjectProperty(
            _moduleLoader.get(), _propertyNameIndex.getJsName(kRegisterModulePropertyName), callContext);
        if (!entry.exceptionTracker) {
            onRecoverableError("registerJavaScriptModuleFactory", entry.exceptionTracker);
            return;
        }
    });
}

void JavaScriptRuntime::registerTypeConverter(const StringBox& typeName, const StringBox& functionPath) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& jsEntry) {
        auto parsedPath = ComponentPath::parse(functionPath);

        auto importResult = this->importModule(parsedPath.getResourceId(), jsEntry);
        if (!jsEntry.exceptionTracker) {
            return;
        }

        auto jsModule = importResult->getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            return;
        }

        JSFunctionCallContext callContext(jsEntry.jsContext, nullptr, 0, jsEntry.exceptionTracker);
        auto converter =
            jsEntry.jsContext.callObjectProperty(jsModule, parsedPath.getSymbolName().toStringView(), callContext);
        if (!jsEntry.exceptionTracker) {
            return;
        }

        auto& typeConverter = _typeConverters.emplace_back();
        typeConverter.typeName = typeName;
        typeConverter.functionPath = functionPath;

        auto* valueMarshaller = jsEntry.jsContext.getValueMarshaller();
        if (valueMarshaller != nullptr) {
            valueMarshaller->registerTypeConverter(typeName, converter, jsEntry.exceptionTracker);
        }
    });
}

MainThreadManager* JavaScriptRuntime::getMainThreadManager() const {
    return &_mainThreadManager;
}

StyleAttributesCache& JavaScriptRuntime::getStyleAttributesCache() {
    if (VALDI_UNLIKELY(_styleAttributesCache == nullptr)) {
        _styleAttributesCache = std::make_unique<StyleAttributesCache>(_attributeIds);
    }
    return *_styleAttributesCache;
}

JSValueRef JavaScriptRuntime::symbolicateError(const JSValueRef& error) {
    if (_javaScriptContext == nullptr || _symbolicating) {
        return error;
    }

    auto& jsContext = *_javaScriptContext;
    _symbolicating = true;

    JSExceptionTracker exceptionTracker(jsContext);
    if (_symbolicateFunction.empty()) {
        JavaScriptEntryParameters jsEntry(jsContext, exceptionTracker, nullptr);
        auto module = exceptionTracker.toResult(importModule(symbolicatorResourceId(), jsEntry));
        if (module) {
            auto jsValue = module.value()->getJsValue(jsContext, exceptionTracker);
            if (exceptionTracker) {
                _symbolicateFunction = exceptionTracker.toRetainedResult(
                    jsContext.getObjectProperty(jsValue, "symbolicate", jsEntry.exceptionTracker));
            } else {
                _symbolicateFunction = exceptionTracker.toResult(JSValueRef::makeUnretained(jsContext, jsValue));
            }
        } else {
            _symbolicateFunction = module.moveError();
        }

        if (!_symbolicateFunction) {
            onRecoverableError("symbolicateError", _symbolicateFunction.error());
        }
    }

    if (_symbolicateFunction.success()) {
        JSFunctionCallContext callContext(jsContext, &error, 1, exceptionTracker);
        auto result = jsContext.callObjectAsFunction(_symbolicateFunction.value().get(), callContext);
        if (!exceptionTracker) {
            onRecoverableError("symbolicateError", exceptionTracker);
        } else {
            _symbolicating = false;
            return result;
        }
    }

    exceptionTracker.clearError();

    _symbolicating = false;
    return error;
}

void JavaScriptRuntime::onUnhandledRejectedPromise(IJavaScriptContext& jsContext, const JSValue& promiseResult) {
    auto currentContext = Context::currentRef();

    bool shouldCrash = false;

    if (notifyErrorHandler(jsContext, _unhandledRejectionHandler, currentContext, promiseResult, shouldCrash)) {
        return;
    }

    JSExceptionTracker exceptionTracker(jsContext);
    auto value = jsValueToValue(jsContext, promiseResult, ReferenceInfoBuilder(), exceptionTracker);
    exceptionTracker.clearError();

    if (value.isError()) {
        handleUncaughtJsErrorNoHandler(currentContext, value.getError(), /* isUnhandledPromise */ true, shouldCrash);
    } else {
        handleUncaughtJsErrorNoHandler(
            currentContext, Error(value.toStringBox()), /* isUnhandledPromise */ true, shouldCrash);
    }
}

Ref<JSStackTraceProvider> JavaScriptRuntime::captureCurrentStackTrace() {
    if (_javaScriptContext == nullptr || !_enableStackTraceCapture) {
        return nullptr;
    }

    return doCaptureCurrentStackTrace(*_javaScriptContext);
}

Ref<JSStackTraceProvider> JavaScriptRuntime::doCaptureCurrentStackTrace(IJavaScriptContext& jsContext) {
    JSExceptionTracker exceptionTracker(jsContext);
    auto error = _javaScriptContext->newError("", std::nullopt, exceptionTracker);
    if (!exceptionTracker) {
        exceptionTracker.clearError();
        return nullptr;
    }

    JavaScriptContextEntry entry(_globalContext);

    return makeShared<JavaScriptErrorStackTrace>(jsContext, error.get(), exceptionTracker);
}

void JavaScriptRuntime::performGcNow(IJavaScriptContext& jsContext) {
    _hasGcScheduled = false;
    jsContext.garbageCollect();
    VALDI_DEBUG(*_logger, "Performed JS GC");
}

JavaScriptContextMemoryStatistics JavaScriptRuntime::dumpMemoryStatistics(IJavaScriptContext& jsContext) {
    return jsContext.dumpMemoryStatistics();
}

Result<Value> JavaScriptRuntime::evaluateScript(const BytesView& script, const StringBox& sourceFilename) {
    Result<Value> result;
    dispatchSynchronouslyOnJsThread([&](JavaScriptEntryParameters& jsEntry) {
        auto loadResult =
            loadJsModuleFromBytes(jsEntry.jsContext, script, sourceFilename, nullptr, 0, jsEntry.exceptionTracker);

        if (!jsEntry.exceptionTracker) {
            result = jsEntry.exceptionTracker.extractError();
            return;
        }

        auto convertedValue =
            jsValueToValue(jsEntry.jsContext, loadResult.first.get(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);

        if (!jsEntry.exceptionTracker) {
            result = jsEntry.exceptionTracker.extractError();
            return;
        }

        result = convertedValue;
    });

    return result;
}

Result<Void> JavaScriptRuntime::evalModuleSync(const StringBox& path, bool reevalOnReload) {
    auto resourceIdResult = JavaScriptPathResolver::resolveResourceId(path);
    if (!resourceIdResult) {
        return resourceIdResult.moveError();
    }

    Result<Void> result = Void();

    dispatchSynchronouslyOnJsThread([&](JavaScriptEntryParameters& jsEntry) {
        if (reevalOnReload) {
            _modulesToAutoReload.insert(resourceIdResult.value());
        }

        auto importResult = this->importModule(resourceIdResult.value(), jsEntry);
        if (!jsEntry.exceptionTracker) {
            if (reevalOnReload) {
                onRecoverableError("evalModuleSync", jsEntry.exceptionTracker);
            } else {
                result = jsEntry.exceptionTracker.extractError();
            }
        }
    });

    return result;
}

int32_t JavaScriptRuntime::pushModuleToMarshaller(
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    const Valdi::StringBox& path,
    int64_t marshallerHandle) {
    auto marshaller = reinterpret_cast<Marshaller*>(marshallerHandle);
    return pushModuleToMarshaller(nativeObjectsManager, path, *marshaller);
}

int32_t JavaScriptRuntime::pushModuleToMarshaller(
    const /*not-null*/ std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    const Valdi::StringBox& path,
    Marshaller& marshaller) {
    int32_t retValue = 0;
    dispatchOnJsThreadSync(nullptr, [&](JavaScriptEntryParameters& jsEntry) {
        auto importResult = this->importModule(path, jsEntry);
        if (!jsEntry.exceptionTracker) {
            marshaller.getExceptionTracker().onError(jsEntry.exceptionTracker.extractError());
            return;
        }

        if (nativeObjectsManager != nullptr) {
            auto* impl = dynamic_cast<JSRuntimeNativeObjectsManagerImpl*>(nativeObjectsManager.get());
            auto context = impl->getContext();
            if (context == nullptr) {
                if constexpr (snap::kIsGoldBuild || snap::kIsDevBuild) {
                    marshaller.getExceptionTracker().onError("Associated JSRuntimeNativeObjectsManager was disposed");
                    return;
                } else {
                    // Fallback on global context on prod builds to avoid crashes.
                    context = _globalContext;
                }
            }

            ContextEntry subEntry(context);
            retValue = importResult->pushToMarshaller(jsEntry.jsContext, marshaller);
        } else {
            retValue = importResult->pushToMarshaller(jsEntry.jsContext, marshaller);
        }
    });

    return retValue;
}

void JavaScriptRuntime::addModuleUnloadObserver(const Valdi::StringBox& path, const Valdi::Value& observer) {
    dispatchOnJsThreadAsync(nullptr, [this, path, observer](JavaScriptEntryParameters& jsEntry) {
        auto importResult = this->importModule(path, jsEntry);
        if (!jsEntry.exceptionTracker) {
            onRecoverableError("getModule", jsEntry.exceptionTracker);
            return;
        }

        importResult->addUnloadObserver(observer);
    });
}

void JavaScriptRuntime::preloadModule(const StringBox& path, int32_t maxDepth) {
    dispatchOnJsThreadAsync(nullptr, [=](JavaScriptEntryParameters& jsEntry) {
        VALDI_TRACE_META("Valdi.preloadModule", path);

        std::initializer_list<JSValueRef> params = {
            jsEntry.jsContext.newStringUTF8(path.toStringView(), jsEntry.exceptionTracker),
            jsEntry.jsContext.newNumber(maxDepth)};

        if (!jsEntry.exceptionTracker) {
            return;
        }

        JSFunctionCallContext callContext(jsEntry.jsContext, params.begin(), params.size(), jsEntry.exceptionTracker);

        jsEntry.jsContext.callObjectProperty(
            _moduleLoader.get(), _propertyNameIndex.getJsName(kPreloadModulePropertyName), callContext);
    });
}

std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager> JavaScriptRuntime::createNativeObjectsManager() {
    auto context = _contextManager.createContext(nullptr, nullptr, /* deferRender */ true);

    return makeShared<JSRuntimeNativeObjectsManagerImpl>(_contextManager, std::move(context));
}

void JavaScriptRuntime::destroyNativeObjectsManager(
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager) {
    auto* impl = dynamic_cast<JSRuntimeNativeObjectsManagerImpl*>(nativeObjectsManager.get());
    dispatchOnJsThread(
        impl->getContext(), JavaScriptTaskScheduleTypeDefault, 0, [nativeObjectsManager](auto& /*jsEntry*/) {
            auto* impl = dynamic_cast<JSRuntimeNativeObjectsManagerImpl*>(nativeObjectsManager.get());
            impl->doDispose();
        });
}

std::shared_ptr<snap::valdi_core::JSRuntime> JavaScriptRuntime::createWorker() {
    auto workerRuntime = makeShared<JavaScriptRuntime>(_javaScriptBridge,
                                                       _resourceManager,
                                                       _contextManager,
                                                       _mainThreadManager,
                                                       _attributeIds,
                                                       _platformType,
                                                       _enableDebugger,
                                                       ThreadQoSClassHigh,
                                                       _anrDetector,
                                                       _logger,
                                                       true);
    workerRuntime->postInit();
    for (const auto& moduleFactory : _moduleFactories) {
        workerRuntime->registerJavaScriptModuleFactory(moduleFactory);
    }
    for (const auto& typeConverter : _typeConverters) {
        workerRuntime->registerTypeConverter(typeConverter.typeName, typeConverter.functionPath);
    }
    _jsWorkers.emplace_back(weakRef(workerRuntime.get()));
    return std::dynamic_pointer_cast<snap::valdi_core::JSRuntime>(*workerRuntime->getInnerSharedPtr());
}

void JavaScriptRuntime::runOnJsThread(const Value& runnable) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        auto* func = runnable.getFunction();
        if (func != nullptr) {
            (*func)();
        }
    });
}

void JavaScriptRuntime::daemonClientConnected(const Shared<IDaemonClient>& daemonClient) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& jsEntry) {
        Value wrappedDaemonClient;

        wrappedDaemonClient.setMapValue("connectionId", Value(daemonClient->getConnectionId()));
        wrappedDaemonClient.setMapValue(
            "submitRequest",
            Value(makeShared<ValueFunctionWithCallable>([=](const ValueFunctionCallContext& callContext) -> Value {
                auto payload = callContext.getParameter(0);

                auto callback = callContext.getParameterAsFunction(1);
                if (!callContext.getExceptionTracker()) {
                    return Value::undefined();
                }

                daemonClient->submitRequest(payload, callback);

                return Value::undefined();
            })));

        static auto kDaemonClientKey = STRING_LITERAL("DaemonClient");

        auto daemonClientJs = valueToJSValue(jsEntry.jsContext,
                                             wrappedDaemonClient,
                                             ReferenceInfoBuilder().withObject(kDaemonClientKey),
                                             jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            onRecoverableError("daemonClientConnected", jsEntry.exceptionTracker);
            return;
        }

        _daemonClients[daemonClient->getConnectionId()] =
            JSValueRef::makeRetained(jsEntry.jsContext, daemonClientJs.get());

        auto jsDebuggerInfo = jsEntry.jsContext.getDebuggerInfo();
        if (jsDebuggerInfo.has_value()) {
            daemonClient->sendJsDebuggerInfo(jsDebuggerInfo.value().debuggerPort,
                                             jsDebuggerInfo.value().websocketTargets);
        }

        notifyDaemonClientEvent(DaemonClientEventTypeConnected, daemonClientJs.get(), JSValue(), jsEntry);
        if (!jsEntry.exceptionTracker) {
            onRecoverableError("daemonClientConnected", jsEntry.exceptionTracker);
            return;
        }
    });
}

void JavaScriptRuntime::daemonClientDisconnected(const Shared<IDaemonClient>& daemonClient) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& jsEntry) {
        const auto& it = _daemonClients.find(daemonClient->getConnectionId());
        if (it != _daemonClients.end()) {
            auto daemonClientJs = it->second;
            _daemonClients.erase(it);

            notifyDaemonClientEvent(DaemonClientEventTypeDisconnected, daemonClientJs.get(), JSValue(), jsEntry);
            if (!jsEntry.exceptionTracker) {
                onRecoverableError("daemonClientDisconnected", jsEntry.exceptionTracker);
                return;
            }
        }
    });
}

void JavaScriptRuntime::daemonClientDidReceiveClientPayload(const Shared<IDaemonClient>& daemonClient,
                                                            int senderClientId,
                                                            const BytesView& payload) {
    dispatchOnJsThreadUnattributed([=](JavaScriptEntryParameters& entry) {
        const auto& it = _daemonClients.find(daemonClient->getConnectionId());
        if (it != _daemonClients.end()) {
            auto payloadString = entry.jsContext.newStringUTF8(payload.asStringView(), entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                onRecoverableError("daemonClientDidReceiveClientPayload", entry.exceptionTracker);
                return;
            }

            auto payloadObject = entry.jsContext.newObject(entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                onRecoverableError("daemonClientDidReceiveClientPayload", entry.exceptionTracker);
                return;
            }

            entry.jsContext.setObjectProperty(
                payloadObject.get(), "payload", payloadString.get(), entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                onRecoverableError("daemonClientDidReceiveClientPayload", entry.exceptionTracker);
                return;
            }

            auto jsSenderClientId = entry.jsContext.newNumber(senderClientId);
            entry.jsContext.setObjectProperty(
                payloadObject.get(), "senderClientId", jsSenderClientId.get(), entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                onRecoverableError("daemonClientDidReceiveClientPayload", entry.exceptionTracker);
                return;
            }

            notifyDaemonClientEvent(
                DaemonClientEventTypeReceivedClientPayload, it->second.get(), payloadObject.get(), entry);
        }
    });
}

void JavaScriptRuntime::notifyDaemonClientEvent(DaemonClientEventType eventType,
                                                const JSValue& daemonClient,
                                                const JSValue& payload,
                                                JavaScriptEntryParameters& jsEntry) {
    ensureValdiModuleIsLoaded(jsEntry);
    if (!jsEntry.exceptionTracker) {
        onRecoverableError("notifyDaemonClientEvent", jsEntry.exceptionTracker);
        return;
    }

    if (_onDaemonClientEventFunction) {
        notifyDaemonClientEvent(jsEntry.jsContext,
                                eventType,
                                daemonClient,
                                payload,
                                _onDaemonClientEventFunction.value().get(),
                                jsEntry.exceptionTracker);
    }
}

void JavaScriptRuntime::notifyDaemonClientEvent(IJavaScriptContext& jsContext,
                                                DaemonClientEventType eventType,
                                                const JSValue& daemonClient,
                                                const JSValue& payload,
                                                const JSValue& onDaemonClientEventFunction,
                                                JSExceptionTracker& exceptionTracker) {
    std::initializer_list<JSValueRef> params = {
        jsContext.newNumber(static_cast<int32_t>(eventType)),
        JSValueRef::makeUnretained(jsContext, daemonClient),
        JSValueRef::makeUnretained(jsContext, payload),
    };

    JSFunctionCallContext callContext(jsContext, params.begin(), params.size(), exceptionTracker);

    jsContext.callObjectAsFunction(onDaemonClientEventFunction, callContext);
    if (!exceptionTracker) {
        onRecoverableError("notifyDaemonClientEvent", exceptionTracker);
    }
}

void JavaScriptRuntime::setDefaultViewManagerContext(const Ref<ViewManagerContext>& viewManagerContext) {
    dispatchOnJsThreadUnattributed([=](auto& /*entry*/) { _defaultViewManagerContext = viewManagerContext; });
}

std::future<Result<DumpedLogs>> JavaScriptRuntime::dumpLogs(bool includeMetadata, bool includeVerbose) {
    auto promise = Valdi::makeShared<std::promise<Result<DumpedLogs>>>();
    auto future = promise->get_future();

    dispatchOnJsThreadUnattributed([promise, this, includeMetadata, includeVerbose](
                                       JavaScriptEntryParameters& jsEntry) {
        auto bugReportModule = importModule(STRING_LITERAL("valdi_core/src/BugReporter"), jsEntry);
        if (!jsEntry.exceptionTracker) {
            promise->set_value(jsEntry.exceptionTracker.extractError());
            return;
        }

        auto jsValue = bugReportModule->getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            promise->set_value(jsEntry.exceptionTracker.extractError());
            return;
        }

        auto dumpLogMetadataFunction =
            jsEntry.jsContext.getObjectProperty(jsValue, "dumpLogMetadata", jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            promise->set_value(jsEntry.exceptionTracker.extractError());
            return;
        }

        std::initializer_list<JSValueRef> params = {
            jsEntry.jsContext.newBool(includeMetadata),
            jsEntry.jsContext.newBool(includeVerbose),
        };

        JSFunctionCallContext callContext(jsEntry.jsContext, params.begin(), params.size(), jsEntry.exceptionTracker);

        auto dumpResult = jsEntry.jsContext.callObjectAsFunction(dumpLogMetadataFunction.get(), callContext);
        if (!jsEntry.exceptionTracker) {
            promise->set_value(jsEntry.exceptionTracker.extractError());
            return;
        }

        auto valueResult =
            jsValueToValue(jsEntry.jsContext, dumpResult.get(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            promise->set_value(jsEntry.exceptionTracker.extractError());
            return;
        }

        const auto* array = valueResult.getArray();
        if (array == nullptr || array->size() % 3 != 0) {
            promise->set_value(Value::invalidTypeError(ValueType::Array, valueResult.getType()));
            return;
        }

        DumpedLogs logs;

        size_t i = 0;
        while (i < array->size()) {
            auto name = (*array)[i++];
            auto verbose = (*array)[i++];
            auto metadata = (*array)[i++];

            if (name.isString() && !verbose.isNullOrUndefined()) {
                logs.appendVerbose(name.toStringBox().prepend("[JS] "), verbose);
            }
            if (metadata.isMap()) {
                for (const auto& it : *metadata.getMap()) {
                    logs.appendMetadata(it.first, it.second.toStringBox());
                }
            }
        }
        promise->set_value(std::move(logs));
    });

    return future;
}

std::vector<JavaScriptCapturedStacktrace> JavaScriptRuntime::captureStackTraces(
    std::chrono::steady_clock::duration timeout) {
    auto session = makeShared<JavaScriptStacktraceCaptureSession>();
    Ref<IJavaScriptContext> jsContext;

    {
        std::lock_guard<Mutex> lock(_mutex);
        jsContext = _javaScriptContext;

        if (jsContext == nullptr) {
            return {};
        }

        _stacktraceCaptureSessions.emplace_back(session);
        jsContext->requestInterrupt();
    }

    if (_dispatchQueue->isCurrent()) {
        onInterrupt(*jsContext);
    } else {
        _dispatchQueue->async([session]() {
            // If our dispatch queue evaluated our call, we mark the current capture as completed
            // because it would mean that we finished executing tasks that were in the JS thread before.
            session->setCapturedStacktrace(
                JavaScriptCapturedStacktrace(JavaScriptCapturedStacktrace::Status::NOT_RUNNING, StringBox(), nullptr));
        });
    }

    snap::utils::time::StopWatch sw;
    sw.start();

    while (!session->finishedCapture()) {
        if (sw.elapsed().chrono() > timeout) {
            session->setCapturedStacktrace(
                JavaScriptCapturedStacktrace(JavaScriptCapturedStacktrace::Status::TIMED_OUT, StringBox(), nullptr));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    auto capturedStacktrace = session->getCapturedStacktrace();
    if (!capturedStacktrace) {
        return {};
    }

    return {capturedStacktrace.value()};
}

void JavaScriptRuntime::onInterrupt(IJavaScriptContext& jsContext) {
    for (;;) {
        Ref<JavaScriptStacktraceCaptureSession> captureSession;

        {
            std::lock_guard<Mutex> lock(_mutex);
            if (_stacktraceCaptureSessions.empty()) {
                return;
            }
            captureSession = _stacktraceCaptureSessions.back();
            _stacktraceCaptureSessions.pop_back();
        }

        auto stacktrace = doCaptureCurrentStackTrace(jsContext);
        captureSession->setCapturedStacktrace(JavaScriptCapturedStacktrace(
            JavaScriptCapturedStacktrace::Status::RUNNING, stacktrace->getStackTrace(), Context::currentRef()));
    }
}

void JavaScriptRuntime::lockNextWorker(std::vector<IJavaScriptContext*>& jsContexts,
                                       std::vector<Ref<JavaScriptRuntime>>& jsWorkers,
                                       const DispatchFunction& onDone) {
    if (jsWorkers.empty()) {
        onDone();
        return;
    }

    auto jsWorker = jsWorkers.front();
    jsWorkers.erase(jsWorkers.begin());

    jsWorker->lockAllJSContexts(jsContexts, [&]() { lockNextWorker(jsContexts, jsWorkers, onDone); });
}

void JavaScriptRuntime::lockAllJSContexts(std::vector<IJavaScriptContext*>& jsContexts,
                                          const DispatchFunction& onDone) {
    dispatchSynchronouslyOnJsThread([&](JavaScriptEntryParameters& jsEntry) {
        jsContexts.emplace_back(&jsEntry.jsContext);

        auto jsWorkers = getAllWorkers();

        lockNextWorker(jsContexts, jsWorkers, onDone);
    });
}

void JavaScriptRuntime::dispatchOnJsThreadUnattributed(JavaScriptThreadTask&& function) {
    dispatchOnJsThread(nullptr, JavaScriptTaskScheduleTypeDefault, 0, std::move(function));
}

void JavaScriptRuntime::dispatchOnJsThread(Ref<Context> ownerContext,
                                           JavaScriptTaskScheduleType scheduleType,
                                           uint32_t delayMs,
                                           JavaScriptThreadTask&& function) {
    auto dispatchFunc = makeJsThreadDispatchFunction(
        ownerContext != nullptr ? std::move(ownerContext) : Ref(_globalContext), std::move(function));

    if (scheduleType == JavaScriptTaskScheduleTypeAlwaysAsync) {
        _dispatchQueue->asyncAfter(std::move(dispatchFunc), std::chrono::milliseconds(delayMs));
        return;
    }

    if (_dispatchQueue->isCurrent()) {
        dispatchFunc();
        return;
    }

    if (scheduleType == JavaScriptTaskScheduleTypeAlwaysSync) {
        _dispatchQueue->sync(dispatchFunc);
    } else {
        _dispatchQueue->async(std::move(dispatchFunc));
    }
}

void JavaScriptRuntime::dispatchSynchronouslyOnJsThread(JavaScriptThreadTask&& function) {
    dispatchOnJsThread(nullptr, JavaScriptTaskScheduleTypeAlwaysSync, 0, std::move(function));
}

void JavaScriptRuntime::dispatchOnMainThread(DispatchFunction func) {
    _mainThreadManager.dispatch(Context::currentRef(), std::move(func));
}

bool JavaScriptRuntime::isInJsThread() {
    return _dispatchQueue->isCurrent();
}

Ref<Context> JavaScriptRuntime::getLastDispatchedContext() const {
    return _contextManager.getContext(_lastDispatchedContextId.load());
}

DispatchFunction JavaScriptRuntime::makeJsThreadDispatchFunction(Ref<Context>&& ownerContext,
                                                                 JavaScriptThreadTask&& jsTask) {
    SC_ASSERT(ownerContext != nullptr);
    return [this, retainedContext = RetainedContext(std::move(ownerContext)), jsTask = std::move(jsTask)]() {
        if (_javaScriptContext == nullptr || !_running) {
            return;
        }

        const auto& ownerContext = retainedContext.context;
        _lastDispatchedContextId = ownerContext->getContextId();

        ownerContext->withAttribution([&]() {
            auto isSync = _dispatchQueue->isRunningSync();

            auto module = ownerContext->getPath().getResourceId().bundleName;
            ScopedMetrics metrics = isSync ? Metrics::thresholdedScopedSlowSyncJsCall(getMetrics(), module) :
                                             Metrics::thresholdedScopedSlowAsyncJsCall(getMetrics(), module);
            auto& jsContext = *_javaScriptContext;

            JavaScriptContextEntry contextEntry(ownerContext);
            JSExceptionTracker exceptionTracker(jsContext);
            {
                JavaScriptEntryParameters jsEntry(jsContext, exceptionTracker, ownerContext);

                jsTask(jsEntry);
            }

            if (!exceptionTracker) {
                handleUncaughtJsError(jsContext, ownerContext, exceptionTracker);
            }
        });
    };
}

void JavaScriptRuntime::onInitError(std::string_view failingAction, const Error& error) {
    _running = false;
    handleUncaughtJsErrorNoHandler(
        nullptr,
        error.rethrow(STRING_FORMAT("Fatal init error with performing action '{}'", failingAction)),
        /* isUnhandledPromise */ false,
        /* shouldCrash */ true);
}

void JavaScriptRuntime::onRecoverableError(std::string_view failingAction, JSExceptionTracker& exceptionTracker) {
    onRecoverableError(failingAction, exceptionTracker.extractError());
}

void JavaScriptRuntime::onRecoverableError(std::string_view failingAction, const Error& error) {
    handleUncaughtJsErrorNoHandler(
        nullptr,
        error.rethrow(STRING_FORMAT("Recoverable JS Error while performing action '{}'", failingAction)),
        /* isUnhandledPromise */ false,
        /* shouldCrash */ false);
}

void JavaScriptRuntime::handleUncaughtJsError(IJavaScriptContext& jsContext,
                                              const Ref<Context>& ownerContext,
                                              JSExceptionTracker& exceptionTracker) {
    bool shouldCrash = false;
    if (_uncaughtExceptionHandler != nullptr) {
        auto exception = exceptionTracker.getExceptionAndClear();
        if (notifyErrorHandler(jsContext, _uncaughtExceptionHandler, ownerContext, exception.get(), shouldCrash)) {
            return;
        }
        // Notify failed, store exception back
        exceptionTracker.storeException(std::move(exception));
    }

    auto error = exceptionTracker.extractError();
    handleUncaughtJsErrorNoHandler(ownerContext, error, /* isUnhandledPromise */ false, shouldCrash);
}

void JavaScriptRuntime::handleUncaughtJsError(IJavaScriptContext& jsContext,
                                              const Ref<Context>& ownerContext,
                                              const Error& error,
                                              bool isUnhandledPromise) {
    bool shouldCrash = false;
    if (_uncaughtExceptionHandler != nullptr) {
        JSExceptionTracker exceptionTracker(jsContext);
        auto jsError = convertValdiErrorToJSError(jsContext, error, exceptionTracker);
        if (exceptionTracker) {
            if (notifyErrorHandler(jsContext, _uncaughtExceptionHandler, ownerContext, jsError.get(), shouldCrash)) {
                return;
            }
        } else {
            exceptionTracker.clearError();
        }
    }

    handleUncaughtJsErrorNoHandler(ownerContext, error, isUnhandledPromise, shouldCrash);
}

void JavaScriptRuntime::handleUncaughtJsErrorNoHandler(const Ref<Context>& ownerContext,
                                                       const Error& error,
                                                       bool isUnhandledPromise,
                                                       bool shouldCrash) {
    StringBox moduleName;

    const auto& metrics = getMetrics();
    if (ownerContext != nullptr) {
        // Attributed call
        moduleName = ownerContext->getPath().getResourceId().bundleName;
        if (metrics != nullptr) {
            if (!moduleName.isEmpty()) {
                metrics->emitUncaughtError(moduleName);
            } else {
                metrics->emitUncaughtError();
            }
        }
    } else {
        if (metrics != nullptr) {
            metrics->emitUncaughtError();
        }
    }

    StringBox errorMessage;

    if (isUnhandledPromise) {
        if (!moduleName.isEmpty()) {
            errorMessage = STRING_FORMAT("Unhandled rejected JS promise in '{}': {}", moduleName, error);
        } else {
            errorMessage = STRING_FORMAT("Unhandled rejected JS promise: {}", error);
        }
    } else {
        if (!moduleName.isEmpty()) {
            errorMessage = STRING_FORMAT("Uncaught JS error in '{}': {}", moduleName, error);
        } else {
            errorMessage = STRING_FORMAT("Uncaught JS error: {}", error);
        }
    }

    auto errorMessageStr = errorMessage.slowToString();

    _logger->log(LogTypeError, errorMessageStr);

    if (_listener != nullptr) {
        if (error.hasStack() || snap::kIsDevBuild) {
            _listener->onDebugMessage(LogTypeError, errorMessage);
        }
        _listener->onUncaughtJsError(moduleName, error);
    }

    if (shouldCrash) {
        SC_ABORT(errorMessageStr.c_str());
    }

    if (ownerContext != nullptr) {
        ownerContext->onRuntimeIssue(errorMessage, true);
    }
}

const Ref<Metrics>& JavaScriptRuntime::getMetrics() const {
    return _resourceManager.getMetrics();
}

void JavaScriptRuntime::setEnableStackTraceCapture(bool enableStackTraceCapture) {
    _enableStackTraceCapture = enableStackTraceCapture;
}

void JavaScriptRuntime::startProfiling() {
    dispatchOnJsThreadUnattributed([](JavaScriptEntryParameters& entry) { entry.jsContext.startProfiling(); });
}

void JavaScriptRuntime::stopProfiling(Function<void(const Result<std::vector<std::string>>&)> onComplete) {
    dispatchOnJsThreadUnattributed([completion = std::move(onComplete)](JavaScriptEntryParameters& entry) {
        completion(entry.jsContext.stopProfiling());
    });
}

ILogger& JavaScriptRuntime::getLogger() const {
    return *_logger;
}

TimePoint JavaScriptRuntime::getPerformanceTimeOrigin() const {
    return TimePoint::fromChrono(_upTime.getStartTime());
}

Result<BytesView> JavaScriptRuntime::dumpHeap() {
    if constexpr (Valdi::shouldEnableJsHeapDump()) {
        Result<BytesView> result;
        dispatchSynchronouslyOnJsThread([this, &result](JavaScriptEntryParameters& entry) {
            std::vector<IJavaScriptContext*> jsContexts;

            lockAllJSContexts(jsContexts, [&]() {
                result = _javaScriptBridge.dumpHeap(
                    std::span<IJavaScriptContext*>(jsContexts.data(), jsContexts.size()), entry.exceptionTracker);
            });

            if (!entry.exceptionTracker) {
                result = entry.exceptionTracker.extractError();
            }
        });

        return result;
    } else {
        return BytesView();
    }
}

Result<Ref<Context>> JavaScriptRuntime::getContextForId(ContextId contextId) const {
    // We lookup in the ContextManager first, to handle both contexts that are created externally
    // and contexts that are created directly in JS (which happens when running tests)
    Ref<Context> context = _contextManager.getContext(contextId);
    if (context == nullptr) {
        // Fallback on a lookup on our contextHandler, in case the context was destroyed outside of the js thread
        return _contextHandler->getContextForId(contextId);
    }
    return context;
}

} // namespace Valdi
