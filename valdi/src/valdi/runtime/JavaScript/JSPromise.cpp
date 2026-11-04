//
//  JSPromise.cpp
//  valdi
//
//  Created by Simon Corsin on 8/08/23.
//

#include "valdi/runtime/JavaScript/JSPromise.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRAttribution.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"

#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"

namespace Valdi {

STRING_CONST(thenPropertyKey, "then")

static JavaScriptRuntime* getAttributingRuntime(IJavaScriptContext& jsContext, const StringBox& anrAttribution) {
    return anrAttribution.isEmpty() ? nullptr : dynamic_cast<JavaScriptRuntime*>(jsContext.getTaskScheduler());
}

static JSPropertyName getCancelPropertyKey(IJavaScriptContext& jsContext) {
    static auto kCancel = STRING_LITERAL("cancel");
    return jsContext.getPropertyNameCached(kCancel);
}

JSPromiseCancel::JSPromiseCancel(const Ref<Promise>& promise) : _promise(weakRef(promise.get())) {}
JSPromiseCancel::~JSPromiseCancel() = default;

JSValueRef JSPromiseCancel::operator()(JSFunctionNativeCallContext& callContext) noexcept {
    auto promise = _promise.lock();
    if (promise != nullptr) {
        promise->cancel();
    }

    return callContext.getContext().newUndefined();
}

void JSPromiseCancel::attachToJSPromise(IJavaScriptContext& jsContext,
                                        const JSValue& jsPromise,
                                        const Ref<Promise>& nativePromise,
                                        JSExceptionTracker& exceptionTracker) {
    auto cancelFunction = jsContext.newFunction(makeShared<JSPromiseCancel>(nativePromise), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    jsContext.setObjectProperty(jsPromise, getCancelPropertyKey(jsContext), cancelFunction.get(), exceptionTracker);
}

JSValueRef JSPromiseCancel::getFromJSPromise(IJavaScriptContext& jsContext,
                                             const JSValue& jsPromise,
                                             JSExceptionTracker& exceptionTracker) {
    if (!jsContext.isValueObject(jsPromise)) {
        return jsContext.newUndefined();
    }

    return jsContext.getObjectProperty(jsPromise, getCancelPropertyKey(jsContext), exceptionTracker);
}

bool JSPromiseCancel::isJSPromiseCancelable(IJavaScriptContext& jsContext, const JSValue& jsPromise) {
    return jsContext.hasObjectProperty(jsPromise, getCancelPropertyKey(jsContext));
}

JSPromiseOnFulfilled::JSPromiseOnFulfilled(const Ref<PromiseCallback>& callback,
                                           const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                           StringBox anrAttribution)
    : _callback(callback), _valueMarshaller(valueMarshaller), _anrAttribution(std::move(anrAttribution)) {}

JSPromiseOnFulfilled::~JSPromiseOnFulfilled() = default;

JSValueRef JSPromiseOnFulfilled::operator()(JSFunctionNativeCallContext& callContext) noexcept {
    ScopedNativeCallActivity activity(getAttributingRuntime(callContext.getContext(), _anrAttribution),
                                      _anrAttribution);
    JSPromiseOnFulfilled::callCompletionWithValue(callContext.getContext(),
                                                  callContext.getParameter(0),
                                                  _valueMarshaller,
                                                  _callback,
                                                  callContext.getExceptionTracker());

    return callContext.getContext().newUndefined();
}

void JSPromiseOnFulfilled::callCompletionWithValue(IJavaScriptContext& jsContext,
                                                   const JSValue& value,
                                                   const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                                   const Ref<PromiseCallback>& callback,
                                                   JSExceptionTracker& exceptionTracker) {
    auto resolvedValue = JSValueRef::makeUnretained(jsContext, value);
    auto marshalledValue = valueMarshaller->marshall(nullptr, resolvedValue, ReferenceInfoBuilder(), exceptionTracker);

    if (!exceptionTracker) {
        callback->onFailure(exceptionTracker.extractError());
    } else {
        callback->onSuccess(marshalledValue);
    }
}

JSPromiseOnRejected::JSPromiseOnRejected(const Ref<PromiseCallback>& callback, StringBox anrAttribution)
    : _callback(callback), _anrAttribution(std::move(anrAttribution)) {}
JSPromiseOnRejected::~JSPromiseOnRejected() = default;

JSValueRef JSPromiseOnRejected::operator()(JSFunctionNativeCallContext& callContext) noexcept {
    ScopedNativeCallActivity activity(getAttributingRuntime(callContext.getContext(), _anrAttribution),
                                      _anrAttribution);
    auto resolvedValue = JSValueRef::makeUnretained(callContext.getContext(), callContext.getParameter(0));
    auto error = convertJSErrorToValdiError(callContext.getContext(), resolvedValue, nullptr);
    _callback->onFailure(std::move(error));

    return callContext.getContext().newUndefined();
}

PromiseExecutor::PromiseExecutor() = default;
PromiseExecutor::~PromiseExecutor() = default;

JSValueRef PromiseExecutor::extractResolve() {
    return std::move(_resolve);
}

JSValueRef PromiseExecutor::extractReject() {
    return std::move(_reject);
}

JSValueRef PromiseExecutor::operator()(JSFunctionNativeCallContext& callContext) noexcept {
    _resolve = JSValueRef::makeRetained(callContext.getContext(), callContext.getParameter(0));
    _reject = JSValueRef::makeRetained(callContext.getContext(), callContext.getParameter(1));

    return callContext.getContext().newUndefined();
}

JSPromise::JSPromise(IJavaScriptContext& jsContext,
                     const JSValue& jsValue,
                     const ReferenceInfo& referenceInfo,
                     const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                     JSExceptionTracker& exceptionTracker)
    : _jsValue(jsContext, jsValue, referenceInfo, exceptionTracker, false),
      _valueMarshaller(valueMarshaller),
      _cancelable(JSPromiseCancel::isJSPromiseCancelable(jsContext, jsValue)) {
    // onComplete() runs a task later, after the engine has already flushed its rejection tracker.
    jsContext.markPromiseRejectionHandled(jsValue);
}

JSPromise::~JSPromise() = default;

void JSPromise::onComplete(const Ref<PromiseCallback>& callback) {
    auto taskScheduler = _jsValue.getTaskScheduler();
    if (taskScheduler != nullptr) {
        taskScheduler->dispatchOnJsThreadAsync(
            _jsValue.getContext(), [self = strongSmallRef(this), callback](const JavaScriptEntryParameters& jsEntry) {
                self->doOnComplete(callback, jsEntry);
            });
    }
}

void JSPromise::cancel() {
    if (!_cancelable) {
        return;
    }

    auto taskScheduler = _jsValue.getTaskScheduler();
    if (taskScheduler != nullptr) {
        taskScheduler->dispatchOnJsThreadAsync(
            _jsValue.getContext(),
            [self = strongSmallRef(this)](const JavaScriptEntryParameters& jsEntry) { self->doCancel(jsEntry); });
    }
}

bool JSPromise::isCancelable() const {
    return _cancelable;
}

void JSPromise::doOnCompleteWithValue(const Ref<PromiseCallback>& callback,
                                      const JSValue& value,
                                      const StringBox& anrAttribution,
                                      const JavaScriptEntryParameters& jsEntry) {
    ScopedNativeCallActivity activity(getAttributingRuntime(jsEntry.jsContext, anrAttribution), anrAttribution);
    JSPromiseOnFulfilled::callCompletionWithValue(
        jsEntry.jsContext, value, _valueMarshaller, callback, jsEntry.exceptionTracker);
}

StringBox JSPromise::getCompletionAttribution(const JavaScriptEntryParameters& jsEntry) const {
    auto* taskScheduler = jsEntry.jsContext.getTaskScheduler();
    if (taskScheduler == nullptr || !taskScheduler->anrDiagnosticsEnabled()) {
        return StringBox();
    }
    return JavaScriptANRAttribution::referenceLabel(_jsValue.getReferenceInfo());
}

void JSPromise::doOnComplete(const Ref<PromiseCallback>& callback, const JavaScriptEntryParameters& jsEntry) {
    auto jsValue = _jsValue.getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    auto anrAttribution = getCompletionAttribution(jsEntry);

    if (!jsEntry.jsContext.isValueObject(jsValue)) {
        // Our JS value is not a promise
        doOnCompleteWithValue(callback, jsValue, anrAttribution, jsEntry);
        return;
    }

    auto thenCallback = jsEntry.jsContext.getObjectProperty(
        jsValue, jsEntry.jsContext.getPropertyNameCached(thenPropertyKey()), jsEntry.exceptionTracker);

    if (jsEntry.jsContext.isValueUndefined(thenCallback.get())) {
        // Not a promise
        doOnCompleteWithValue(callback, jsValue, anrAttribution, jsEntry);
        return;
    }

    std::array<JSValueRef, 2> parameters;

    parameters[0] = jsEntry.jsContext.newFunction(
        makeShared<JSPromiseOnFulfilled>(callback, _valueMarshaller, anrAttribution), jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    parameters[1] = jsEntry.jsContext.newFunction(makeShared<JSPromiseOnRejected>(callback, std::move(anrAttribution)),
                                                  jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    JSFunctionCallContext callContext(
        jsEntry.jsContext, parameters.data(), parameters.size(), jsEntry.exceptionTracker);
    callContext.setThisValue(jsValue);

    jsEntry.jsContext.callObjectAsFunction(thenCallback.get(), callContext);
}

void JSPromise::doCancel(const JavaScriptEntryParameters& jsEntry) {
    auto jsValue = _jsValue.getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    auto cancelCallback = JSPromiseCancel::getFromJSPromise(jsEntry.jsContext, jsValue, jsEntry.exceptionTracker);

    if (!jsEntry.exceptionTracker || jsEntry.jsContext.isValueUndefined(cancelCallback.get())) {
        return;
    }

    JSFunctionCallContext callContext(jsEntry.jsContext, nullptr, 0, jsEntry.exceptionTracker);
    callContext.setThisValue(jsValue);

    jsEntry.jsContext.callObjectAsFunction(cancelCallback.get(), callContext);
}

} // namespace Valdi
