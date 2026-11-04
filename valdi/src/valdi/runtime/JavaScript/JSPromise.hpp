//
//  JSPromise.hpp
//  valdi
//
//  Created by Simon Corsin on 8/08/23.
//

#pragma once

#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi_core/cpp/Utils/Promise.hpp"

namespace Valdi {

template<typename Value>
class ValueMarshaller;

struct JavaScriptEntryParameters;

class JSPromiseCancel : public JSFunction {
public:
    JSPromiseCancel(const Ref<Promise>& promise);
    ~JSPromiseCancel() override;

    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept final;

    static void attachToJSPromise(IJavaScriptContext& jsContext,
                                  const JSValue& jsPromise,
                                  const Ref<Promise>& nativePromise,
                                  JSExceptionTracker& exceptionTracker);
    static JSValueRef getFromJSPromise(IJavaScriptContext& jsContext,
                                       const JSValue& jsPromise,
                                       JSExceptionTracker& exceptionTracker);
    static bool isJSPromiseCancelable(IJavaScriptContext& jsContext, const JSValue& jsPromise);

private:
    Weak<Promise> _promise;
};

class JSPromiseOnFulfilled : public JSFunction {
public:
    JSPromiseOnFulfilled(const Ref<PromiseCallback>& callback,
                         const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                         StringBox anrAttribution);
    ~JSPromiseOnFulfilled() override;

    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept final;

    static void callCompletionWithValue(IJavaScriptContext& jsContext,
                                        const JSValue& value,
                                        const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                        const Ref<PromiseCallback>& callback,
                                        JSExceptionTracker& exceptionTracker);

private:
    Ref<PromiseCallback> _callback;
    Ref<ValueMarshaller<JSValueRef>> _valueMarshaller;
    StringBox _anrAttribution;
};

class JSPromiseOnRejected : public JSFunction {
public:
    JSPromiseOnRejected(const Ref<PromiseCallback>& callback, StringBox anrAttribution);
    ~JSPromiseOnRejected() override;

    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept final;

private:
    Ref<PromiseCallback> _callback;
    Ref<ValueMarshaller<JSValueRef>> _valueMarshaller;
    StringBox _anrAttribution;
};

class PromiseExecutor : public JSFunction {
public:
    PromiseExecutor();
    ~PromiseExecutor() override;

    JSValueRef extractResolve();

    JSValueRef extractReject();

    JSValueRef operator()(JSFunctionNativeCallContext& callContext) noexcept final;

private:
    JSValueRef _resolve;
    JSValueRef _reject;
};

class JSPromise : public Promise {
public:
    JSPromise(IJavaScriptContext& jsContext,
              const JSValue& jsValue,
              const ReferenceInfo& referenceInfo,
              const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
              JSExceptionTracker& exceptionTracker);

    ~JSPromise() override;

    void onComplete(const Ref<PromiseCallback>& callback) final;

    void cancel() final;

    bool isCancelable() const final;

private:
    JSValueRefHolder _jsValue;
    Ref<ValueMarshaller<JSValueRef>> _valueMarshaller;
    bool _cancelable;

    void doOnCompleteWithValue(const Ref<PromiseCallback>& callback,
                               const JSValue& value,
                               const StringBox& anrAttribution,
                               const JavaScriptEntryParameters& jsEntry);

    /** ANR label for the native callback, named after the reference the promise was bridged through. */
    StringBox getCompletionAttribution(const JavaScriptEntryParameters& jsEntry) const;

    void doOnComplete(const Ref<PromiseCallback>& callback, const JavaScriptEntryParameters& jsEntry);

    void doCancel(const JavaScriptEntryParameters& jsEntry);
};

} // namespace Valdi
