//
//  JSFunctionValueFunction.cpp
//  valdi
//
//  Created by Simon Corsin on 5/11/21.
//

#include "valdi/runtime/JavaScript/JSFunctionWithValueFunction.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ContextEntry.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

namespace {

JavaScriptRuntime* javaScriptRuntimeOf(const Ref<Context>& context) {
    if (context == nullptr) {
        return nullptr;
    }
    auto* runtime = context->getRuntime();
    return runtime != nullptr ? runtime->getJavaScriptRuntime() : nullptr;
}

// Records the JS->native call the JS thread is inside so an ANR whose stack capture times out can
// name it. Saves and restores the previous name, so nested calls report the innermost and unwind
// to the parent. No-op when ANR diagnostics are off or off the runtime's JS thread.
class ScopedNativeCallActivity {
public:
    ScopedNativeCallActivity(JavaScriptRuntime* runtime, const StringBox& functionName) {
        if (runtime != nullptr && runtime->anrDiagnosticsActiveOnJsThread()) {
            _runtime = runtime;
            _previousName = runtime->swapCurrentNativeCallName(functionName);
        }
    }

    ~ScopedNativeCallActivity() {
        if (_runtime != nullptr) {
            _runtime->swapCurrentNativeCallName(std::move(_previousName));
        }
    }

    ScopedNativeCallActivity(const ScopedNativeCallActivity&) = delete;
    ScopedNativeCallActivity& operator=(const ScopedNativeCallActivity&) = delete;

private:
    JavaScriptRuntime* _runtime = nullptr;
    StringBox _previousName;
};

} // namespace

JSFunctionWithValueFunction::JSFunctionWithValueFunction(Ref<ValueFunction>&& function,
                                                         bool isSingleCall,
                                                         const ReferenceInfo& referenceInfo)
    : ContextAttachedObject(Ref(Context::currentRoot()), function),
      _referenceInfo(referenceInfo),
      _isSingleCall(isSingleCall) {}

JSFunctionWithValueFunction::~JSFunctionWithValueFunction() = default;

const ReferenceInfo& JSFunctionWithValueFunction::getReferenceInfo() const {
    return _referenceInfo;
}

Ref<ValueFunction> JSFunctionWithValueFunction::getFunction(IJavaScriptContext& jsContext,
                                                            JSExceptionTracker& exceptionTracker,
                                                            bool shouldClear) {
    auto fn = castOrNull<ValueFunction>(shouldClear ? removeInnerObject() : getInnerObject());
    if (fn == nullptr) {
        exceptionTracker.onError(Error(STRING_FORMAT(
            "Cannot unwrap native function '{}' as its reference was disposed. Reference was taken from context {}",
            _referenceInfo,
            getContext()->getIdAndPathString())));
    }
    return fn;
}

JSValueRef JSFunctionWithValueFunction::callWithOwnerContextAsCurrent(const Ref<ValueFunction>& function,
                                                                      JSFunctionNativeCallContext& callContext) const {
    ContextEntry entry(getContext());
    auto retValue = doCall(function, &_functionName, callContext);
    return retValue;
}

const StringBox& JSFunctionWithValueFunction::getFunctionName() {
    if (_functionName.isNull()) {
        _functionName = _referenceInfo.toFunctionIdentifier();
    }
    return _functionName;
}

JSValueRef JSFunctionWithValueFunction::operator()(JSFunctionNativeCallContext& callContext) noexcept {
    VALDI_TRACE_META("Valdi.jsBridgeToNative", getFunctionName());

    auto function = getFunction(callContext.getContext(), callContext.getExceptionTracker(), _isSingleCall);
    CHECK_CALL_CONTEXT(callContext);

    ScopedNativeCallActivity nativeCallActivity(javaScriptRuntimeOf(getContext()), getFunctionName());

    if (function->propagatesOwnerContextOnCall()) {
        return callWithOwnerContextAsCurrent(function, callContext);
    } else {
        return doCall(function, &_functionName, callContext);
    }
}

JSFunctionWithUntypedValueFunction::JSFunctionWithUntypedValueFunction(Ref<ValueFunction>&& function,
                                                                       const ReferenceInfo& referenceInfo)
    : JSFunctionWithValueFunction(std::move(function), false, referenceInfo) {}

JSValueRef JSFunctionWithUntypedValueFunction::doCall(const Ref<ValueFunction>& function,
                                                      const StringBox* precomputedFunctionIdentifier,
                                                      JSFunctionNativeCallContext& callContext) const {
    auto referenceInfoBuilder = ReferenceInfoBuilder(getReferenceInfo());
    Valdi::SmallVector<Value, 8> outParameters;
    {
        VALDI_TRACE("Valdi.jsParametersToCpp");

        for (size_t i = 0, size = callContext.getParameterSize(); i < size; i++) {
            auto jsValue = callContext.getParameter(i);

            auto convertedParameterResult = jsValueToValue(callContext.getContext(),
                                                           jsValue,
                                                           referenceInfoBuilder.withParameter(i),
                                                           callContext.getExceptionTracker());
            if (!callContext.getExceptionTracker()) {
                return callContext.getContext().newUndefined();
            }

            outParameters.emplace_back(std::move(convertedParameterResult));
        }
    }

    ValueFunctionCallContext innerCallContext(
        ValueFunctionFlagsNone, outParameters.data(), outParameters.size(), callContext.getExceptionTracker());
    innerCallContext.setFunctionIdentifier(precomputedFunctionIdentifier);

    auto outValue = (*function)(innerCallContext);

    VALDI_TRACE("Valdi.cppToJs");
    return valueToJSValue(
        callContext.getContext(), outValue, referenceInfoBuilder.withReturnValue(), callContext.getExceptionTracker());
}

} // namespace Valdi
