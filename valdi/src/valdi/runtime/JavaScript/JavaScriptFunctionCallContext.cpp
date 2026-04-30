//
//  JavaScriptFunctionCallContext.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 5/6/21.
//

#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"

namespace Valdi {

JSFunctionCallContext::JSFunctionCallContext(IJavaScriptContext& context,
                                             const JSValueRef* parameters,
                                             size_t parametersSize,
                                             JSExceptionTracker& exceptionTracker)
    : _context(context),
      _parameters(parameters),
      _parametersSize(parametersSize),
      _exceptionTracker(exceptionTracker) {}

JSValue JSFunctionCallContext::getParameter(size_t index) const {
    if (index >= _parametersSize) {
        return _context.newUndefined().get();
    }
    return _parameters[index].get();
}

JSValueRef JSFunctionCallContext::throwError(Error&& error) {
    _exceptionTracker.onError(std::move(error));
    return getContext().newUndefined();
}

JSFunctionCallContext JSFunctionCallContext::makeSubContext(const JSValueRef* parameters, size_t parametersSize) {
    return JSFunctionCallContext(_context, parameters, parametersSize, _exceptionTracker);
}

JSFunctionNativeCallContext::JSFunctionNativeCallContext(IJavaScriptContext& context,
                                                         const JSValueRef* parameters,
                                                         size_t parametersSize,
                                                         JSExceptionTracker& exceptionTracker,
                                                         const ReferenceInfo& referenceInfo)
    : JSFunctionCallContext(context, parameters, parametersSize, exceptionTracker), _referenceInfo(referenceInfo) {}

bool JSFunctionNativeCallContext::getParameterAsBool(size_t index) {
    return _context.valueToBool(getParameter(index), _exceptionTracker);
}

Value JSFunctionNativeCallContext::getParameterAsValue(size_t index) {
    return jsValueToValue(
        _context, getParameter(index), ReferenceInfoBuilder(_referenceInfo).withParameter(index), _exceptionTracker);
}

double JSFunctionNativeCallContext::getParameterAsDouble(size_t index) {
    return _context.valueToDouble(getParameter(index), _exceptionTracker);
}

int32_t JSFunctionNativeCallContext::getParameterAsInt(size_t index) {
    return _context.valueToInt(getParameter(index), _exceptionTracker);
}

Ref<ValueFunction> JSFunctionNativeCallContext::getParameterAsFunction(size_t index) {
    return jsValueToFunction(
        _context, getParameter(index), ReferenceInfoBuilder(_referenceInfo).withParameter(index), _exceptionTracker);
}

StringBox JSFunctionNativeCallContext::getParameterAsString(size_t index) {
    return _context.valueToString(getParameter(index), _exceptionTracker);
}

Ref<StaticString> JSFunctionNativeCallContext::getParameterAsStaticString(size_t index) {
    return _context.valueToStaticString(getParameter(index), _exceptionTracker);
}

JSTypedArray JSFunctionNativeCallContext::getParameterAsTypedArray(size_t index) {
    return _context.valueToTypedArray(getParameter(index), _exceptionTracker);
}

Ref<RefCountable> JSFunctionNativeCallContext::getParameterAsWrappedObject(size_t index) {
    return _context.valueToWrappedObject(getParameter(index), _exceptionTracker);
}

BytesView JSFunctionNativeCallContext::getParameterAsBytesView(size_t index) {
    auto typedArray = jsTypedArrayToValueTypedArray(
        _context, getParameter(index), ReferenceInfoBuilder(_referenceInfo), _exceptionTracker);
    if (typedArray == nullptr) {
        return BytesView();
    }
    return typedArray->getBuffer();
}

const ReferenceInfo& JSFunctionNativeCallContext::getReferenceInfo() const {
    return _referenceInfo;
}

} // namespace Valdi
