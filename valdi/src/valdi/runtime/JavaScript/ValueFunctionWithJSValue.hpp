//
//  ValueFunctionWithJSValue.hpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace Valdi {

class MainThreadManager;

/**
 A ValueFunction backed by a JS function.
 The ValueFunctionWithJSValue will handle conversions in and out to
 JS, and dispatch to the JS thred automatically.
 */
class ValueFunctionWithJSValue : public ValueFunction, public JSValueRefHolder {
public:
    ValueFunctionWithJSValue(IJavaScriptContext& context,
                             const JSValue& value,
                             bool isSingleCall,
                             const ReferenceInfo& referenceInfo,
                             JSExceptionTracker& exceptionTracker);
    ~ValueFunctionWithJSValue() override;

    Value operator()(const ValueFunctionCallContext& callContext) noexcept final;
    std::string_view getFunctionType() const final;
    bool prefersSyncCalls() const final;

    bool isSingleCall() const;
    void setSingleCall(bool singleCall);

    void setShouldBlockMainThread(bool shouldBlockMainThread);
    void setAllowSyncCall(bool allowSyncCall);
    void setIgnoreIfValdiContextIsDestroyed(bool ignoreIfValdiContextIsDestroyed);

protected:
    virtual Value callJsFunction(JavaScriptEntryParameters& jsEntry,
                                 const JSValue& function,
                                 const Value* parameters,
                                 size_t parametersSize,
                                 bool ignoreRetValue) const = 0;

    Result<Value> callSyncWithDeadline(const std::chrono::steady_clock::time_point& deadline,
                                       Value* parameters,
                                       size_t size) noexcept override;

private:
    std::atomic_int _callSequence;
    StringBox _functionName;
    MainThreadManager* _mainThreadManager;
    Weak<Context> _creationContext;
    bool _shouldBlockMainThread = false;
    bool _allowSyncCall = true;
    bool _ignoreIfValdiContextIsDestroyed = false;
    bool _isSingleCall;

    bool shouldCallSync(ValueFunctionFlags flags, JavaScriptTaskScheduler& taskScheduler) const;

    /** Returns false if a sync call was requested but is disallowed (schema has allowSyncCall false). */
    bool isSyncCallAllowed(ValueFunctionFlags flags) const;

    Value doJsCall(JavaScriptEntryParameters& jsEntry,
                   const Value* parameters,
                   size_t parametersSize,
                   ExceptionTracker* exceptionTracker,
                   bool ignoreRetValue);
    Value callSync(ValueFunctionFlags flags,
                   const Ref<JavaScriptTaskScheduler>& taskScheduler,
                   const ValueFunctionCallContext& callContext);

    const StringBox& getFunctionName();

    Value callPromise(const Ref<JavaScriptTaskScheduler>& taskScheduler, const ValueFunctionCallContext& callContext);
};

class UntypedValueFunctionWithJSValue : public ValueFunctionWithJSValue {
public:
    UntypedValueFunctionWithJSValue(IJavaScriptContext& context,
                                    const JSValue& value,
                                    const ReferenceInfo& referenceInfo,
                                    JSExceptionTracker& exceptionTracker);

protected:
    Value callJsFunction(JavaScriptEntryParameters& jsEntry,
                         const JSValue& function,
                         const Value* parameters,
                         size_t parametersSize,
                         bool ignoreRetValue) const final;
};

} // namespace Valdi
