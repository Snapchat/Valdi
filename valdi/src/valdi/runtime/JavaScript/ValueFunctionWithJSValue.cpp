//
//  ValueFunctionWithJSValue.cpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "utils/debugging/Assert.hpp"

#include <future>

namespace Valdi {

constexpr size_t kMaxMainThreadWaitTimeMs = 100;

ValueFunctionWithJSValue::ValueFunctionWithJSValue(IJavaScriptContext& context,
                                                   const JSValue& value,
                                                   bool isSingleCall,
                                                   const ReferenceInfo& referenceInfo,
                                                   JSExceptionTracker& exceptionTracker)
    : JSValueRefHolder(context, value, referenceInfo, exceptionTracker, true),
      _callSequence(0),
      _mainThreadManager(context.getListener() != nullptr ? context.getListener()->getMainThreadManager() : nullptr),
      _creationContext(weakRef(Context::current())),
      _isSingleCall(isSingleCall) {}

// See explanation in JSValueRefHolder.cpp
__attribute__((no_sanitize("thread"))) ValueFunctionWithJSValue::~ValueFunctionWithJSValue() = default;

const StringBox& ValueFunctionWithJSValue::getFunctionName() {
    if (_functionName.isNull()) {
        _functionName = getReferenceInfo().toFunctionIdentifier();
    }

    return _functionName;
}

bool ValueFunctionWithJSValue::prefersSyncCalls() const {
    return _shouldBlockMainThread;
}

void ValueFunctionWithJSValue::setShouldBlockMainThread(bool shouldBlockMainThread) {
    _shouldBlockMainThread = shouldBlockMainThread;
}

void ValueFunctionWithJSValue::setAllowSyncCall(bool allowSyncCall) {
    _allowSyncCall = allowSyncCall;
}

bool ValueFunctionWithJSValue::isSingleCall() const {
    return _isSingleCall;
}

void ValueFunctionWithJSValue::setSingleCall(bool singleCall) {
    _isSingleCall = singleCall;
}

bool ValueFunctionWithJSValue::shouldCallSync(ValueFunctionFlags callFlags,
                                              JavaScriptTaskScheduler& taskScheduler) const {
    if (taskScheduler.isInJsThread()) {
        return true;
    }

    // Flags is specifying to never call the function synchronously
    if ((callFlags & ValueFunctionFlagsNeverCallSync) != ValueFunctionFlagsNone) {
        return false;
    }

    if ((callFlags & ValueFunctionFlagsPropagatesError) != ValueFunctionFlagsNone) {
        return true;
    }

    return (callFlags & ValueFunctionFlagsCallSync) != ValueFunctionFlagsNone || _shouldBlockMainThread;
}

bool ValueFunctionWithJSValue::isSyncCallAllowed(ValueFunctionFlags flags) const {
    if ((flags & ValueFunctionFlagsCallSync) == ValueFunctionFlagsNone && !_shouldBlockMainThread) {
        return true;
    }
    return _allowSyncCall;
}

Value ValueFunctionWithJSValue::doJsCall(JavaScriptEntryParameters& jsEntry,
                                         const Value* parameters,
                                         size_t parametersSize,
                                         ExceptionTracker* exceptionTracker,
                                         bool ignoreRetValue) {
    VALDI_TRACE_META("Valdi.callJsFunction", getFunctionName());

    auto jsValue = getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        if (_ignoreIfValdiContextIsDestroyed) {
            jsEntry.exceptionTracker.clearError();
        } else if (exceptionTracker != nullptr) {
            exceptionTracker->onError(jsEntry.exceptionTracker.extractError());
        }
        return Value::undefined();
    }

    auto retValue = callJsFunction(jsEntry, jsValue, parameters, parametersSize, ignoreRetValue);
    if (!jsEntry.exceptionTracker && exceptionTracker != nullptr) {
        exceptionTracker->onError(jsEntry.exceptionTracker.extractError());
    }

    if (_isSingleCall) {
        clearJsValue(jsEntry.jsContext);
    }

    return retValue;
}

void ValueFunctionWithJSValue::setIgnoreIfValdiContextIsDestroyed(bool ignoreIfValdiContextIsDestroyed) {
    _ignoreIfValdiContextIsDestroyed = ignoreIfValdiContextIsDestroyed;
}

static Valdi::SmallVector<Value, 2> captureParameters(const Value* parameters, size_t parametersSize) {
    return Valdi::SmallVector<Value, 2>(parameters, parameters + parametersSize);
}

static Valdi::SmallVector<Value, 2> captureParameters(const ValueFunctionCallContext& callContext) {
    return captureParameters(callContext.getParameters(), callContext.getParametersSize());
}

struct MainThreadThrottledCall : public SimpleRefCountable {
    std::promise<void> promise;
    int callId;
    Valdi::SmallVector<Value, 2> parameters;

    MainThreadThrottledCall(int callId, Valdi::SmallVector<Value, 2>&& parameters)
        : callId(callId), parameters(std::move(parameters)) {}
    ~MainThreadThrottledCall() override = default;
};

Value ValueFunctionWithJSValue::callSync(ValueFunctionFlags flags,
                                         const Ref<JavaScriptTaskScheduler>& taskScheduler,
                                         const ValueFunctionCallContext& callContext) {
    auto retValue = Value::undefined();
    getContext()->withAttribution([&]() {
        auto shouldStartMainThreadBatch =
            _shouldBlockMainThread && _mainThreadManager != nullptr && _mainThreadManager->currentThreadIsMainThread();
        auto propagatesError = ((flags & ValueFunctionFlagsPropagatesError) != ValueFunctionFlagsNone);

        if (shouldStartMainThreadBatch) {
            _mainThreadManager->beginBatch();

            if ((flags & ValueFunctionFlagsAllowThrottling) != ValueFunctionFlagsNone) {
                // When using a main thread batch with allow throttling, we only allow locking
                // the main thread for up to 100ms

                auto throttledCall =
                    makeShared<MainThreadThrottledCall>(++_callSequence, captureParameters(callContext));
                auto future = throttledCall->promise.get_future();

                taskScheduler->dispatchOnJsThreadAsync(
                    getContext(), [self = strongSmallRef(this), throttledCall](auto& jsEntry) {
                        auto currentCallId = self->_callSequence.load();
                        if (currentCallId != throttledCall->callId) {
                            // Throttling call, our callId doesn't match what is in the
                            // sequence.
                            throttledCall->promise.set_value();
                            return;
                        }

                        MainThreadBatchAllowScope mainThreadBatchAllowScope;
                        self->doJsCall(
                            jsEntry, throttledCall->parameters.data(), throttledCall->parameters.size(), nullptr, true);
                        throttledCall->promise.set_value();
                    });

                future.wait_for(std::chrono::milliseconds(kMaxMainThreadWaitTimeMs));
            } else {
                taskScheduler->dispatchOnJsThreadSync(getContext(), [&](auto& jsEntry) {
                    MainThreadBatchAllowScope mainThreadBatchAllowScope;
                    retValue = this->doJsCall(jsEntry,
                                              callContext.getParameters(),
                                              callContext.getParametersSize(),
                                              propagatesError ? &callContext.getExceptionTracker() : nullptr,
                                              false);
                });
            }

            _mainThreadManager->endBatch();
        } else {
            taskScheduler->dispatchOnJsThreadSync(getContext(), [&](auto& jsEntry) {
                retValue = this->doJsCall(jsEntry,
                                          callContext.getParameters(),
                                          callContext.getParametersSize(),
                                          propagatesError ? &callContext.getExceptionTracker() : nullptr,
                                          false);
            });
        }
    });

    return retValue;
}

Value ValueFunctionWithJSValue::operator()(const ValueFunctionCallContext& callContext) noexcept {
    auto taskScheduler = getTaskScheduler();
    if (taskScheduler == nullptr) {
        return Value::undefined();
    }
    if (_ignoreIfValdiContextIsDestroyed) {
        if (Context::isDestroyedContextFixEnabled()) {
            auto ctx = _creationContext.lock();
            if (ctx == nullptr || ctx->isDestroyed()) {
                VALDI_WARN(getContext()->getLogger(),
                           "Function call skipped: creation context {} is destroyed (function: {})",
                           ctx != nullptr ? std::to_string(ctx->getContextId()) : "expired",
                           getReferenceInfo().toString());
                return Value::undefined();
            }
        } else if (getContext()->isDestroyed()) {
            VALDI_WARN(getContext()->getLogger(),
                       "Function call skipped: ValdiContext {} is destroyed (function: {})",
                       getContext()->getContextId(),
                       getReferenceInfo().toString());
            return Value::undefined();
        }
    }

    auto flags = callContext.getFlags();

    // TODO(simon): If we aren't in the JS thread, we dispatch automatically to the JS thread
    // and return null. Otherwise we call the JS function synchronously and return
    // the result. This API could be a bit surprising, will have to think about it more
    // later. In the mean time this allows native to return values to JS which is very useful.

    if (shouldCallSync(flags, *taskScheduler)) {
        // Only assert on main thread: sync JS calls from main thread can cause ANR; worker threads are less
        // problematic.
        if (_mainThreadManager != nullptr && _mainThreadManager->currentThreadIsMainThread()) {
            if (!isSyncCallAllowed(flags)) {
                SC_ASSERT(
                    false &&
                    "Sync JS call is not allowed: this module has async_strict_mode enabled and the function is not "
                    "annotated with @AllowSyncCall. Consider making the function return Promise or void, or add "
                    "@AllowSyncCall to allow blocking the JS thread.");
            }
        }
        return callSync(flags, taskScheduler, callContext);
    } else if ((flags & ValueFunctionFlagsAllowThrottling) != ValueFunctionFlagsNone) {
        auto callId = ++_callSequence;

        taskScheduler->dispatchOnJsThreadAsync(
            getContext(),
            [self = strongSmallRef(this), parameters = captureParameters(callContext), callId](auto& jsEntry) {
                auto currentCallId = self->_callSequence.load();
                if (currentCallId != callId) {
                    // Throttling call, our callId doesn't match what is in the
                    // sequence.
                    return;
                }

                self->doJsCall(jsEntry, parameters.data(), parameters.size(), nullptr, true);
            });
    } else if ((flags & ValueFunctionFlagsNeverCallSync) != ValueFunctionFlagsNone) {
        return callPromise(taskScheduler, callContext);
    } else {
        taskScheduler->dispatchOnJsThreadAsync(
            getContext(), [self = strongSmallRef(this), parameters = captureParameters(callContext)](auto& jsEntry) {
                self->doJsCall(jsEntry, parameters.data(), parameters.size(), nullptr, true);
            });
    }

    return Value::undefined();
}

Value ValueFunctionWithJSValue::callPromise(const Ref<JavaScriptTaskScheduler>& taskScheduler,
                                            const ValueFunctionCallContext& callContext) {
    auto promise = makeShared<ResolvablePromise>();

    taskScheduler->dispatchOnJsThreadAsync(
        getContext(),
        [self = strongSmallRef(this), promise, parameters = captureParameters(callContext)](auto& jsEntry) {
            Value result = self->doJsCall(jsEntry, parameters.data(), parameters.size(), nullptr, false);
            if (!jsEntry.exceptionTracker) {
                promise->fulfill(jsEntry.exceptionTracker.extractError());
                return;
            }

            // When an interruptible callback is skipped (Valdi context destroyed), doJsCall returns
            // undefined. Fulfilling the promise with undefined would cause native unmarshalling to
            // fail (e.g. expected NativeSnapDoc). Reject the promise so the caller can handle it.
            if (result.getType() == ValueType::Undefined && self->_ignoreIfValdiContextIsDestroyed &&
                self->getContext() != nullptr && self->getContext()->isDestroyed()) {
                promise->fulfill(Result<Value>(Error("Valdi context destroyed")));
                return;
            }

            auto nestedPromise = result.getTypedRef<Promise>();
            if (nestedPromise != nullptr) {
                // Flatten one level of promise, since we already returned a promise to the caller
                promise->fulfillWithPromiseResult(nestedPromise);
            } else {
                promise->fulfill(result);
            }
        });

    return Value(promise);
}

std::string_view ValueFunctionWithJSValue::getFunctionType() const {
    return "JSFunction";
}

Result<Value> ValueFunctionWithJSValue::callSyncWithDeadline(const std::chrono::steady_clock::time_point& deadline,
                                                             Value* parameters,
                                                             size_t size) noexcept {
    auto taskScheduler = getTaskScheduler();
    if (taskScheduler == nullptr) {
        return Value::undefined();
    }
    auto promise = std::make_shared<std::promise<Result<Value>>>();
    auto future = promise->get_future();

    SimpleExceptionTracker exceptionTracker;
    ValueFunctionCallContext callContext(
        ValueFunctionFlags::ValueFunctionFlagsCallSync, parameters, size, exceptionTracker);

    taskScheduler->dispatchOnJsThreadAsync(
        getContext(),
        [self = strongSmallRef(this), parameters = captureParameters(callContext), promise](auto& jsEntry) {
            auto result = self->doJsCall(jsEntry, parameters.data(), parameters.size(), nullptr, false);
            promise->set_value(Result(std::move(result)));
        });

    if (std::future_status::ready == future.wait_until(deadline)) {
        return future.get();
    } else {
        return Error("JS function timeout");
    }
}

UntypedValueFunctionWithJSValue::UntypedValueFunctionWithJSValue(IJavaScriptContext& context,
                                                                 const JSValue& value,
                                                                 const ReferenceInfo& referenceInfo,
                                                                 JSExceptionTracker& exceptionTracker)
    : ValueFunctionWithJSValue(context, value, false, referenceInfo, exceptionTracker) {}

Value UntypedValueFunctionWithJSValue::callJsFunction(JavaScriptEntryParameters& jsEntry,
                                                      const JSValue& function,
                                                      const Value* parameters,
                                                      size_t parametersSize,
                                                      bool ignoreRetValue) const {
    JSValueRef jsParameterRefs[parametersSize];

    for (size_t i = 0; i < parametersSize; i++) {
        jsParameterRefs[i] = valueToJSValue(jsEntry.jsContext,
                                            parameters[i],
                                            ReferenceInfoBuilder(getReferenceInfo()).withParameter(i),
                                            jsEntry.exceptionTracker);
        if (!jsEntry.exceptionTracker) {
            return Value::undefined();
        }
    }

    JSFunctionCallContext callContext(jsEntry.jsContext, &jsParameterRefs[0], parametersSize, jsEntry.exceptionTracker);
    auto result = jsEntry.jsContext.callObjectAsFunction(function, callContext);

    if (!jsEntry.exceptionTracker) {
        return Value::undefined();
    }

    if (ignoreRetValue) {
        return Value::undefined();
    }

    return jsValueToValue(jsEntry.jsContext,
                          result.get(),
                          ReferenceInfoBuilder(getReferenceInfo()).withReturnValue(),
                          jsEntry.exceptionTracker);
}

} // namespace Valdi