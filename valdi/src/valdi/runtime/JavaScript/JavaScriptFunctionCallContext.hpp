//
//  JavaScriptFunctionCallContext.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 5/6/21.
//

#pragma once

#include "utils/base/NonCopyable.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace Valdi {

class IJavaScriptContext;
class ReferenceInfo;

class JSFunctionCallContext : public snap::NonCopyable {
public:
    JSFunctionCallContext(IJavaScriptContext& context,
                          const JSValueRef* parameters,
                          size_t parametersSize,
                          JSExceptionTracker& exceptionTracker);

    constexpr IJavaScriptContext& getContext() const {
        return _context;
    }

    constexpr const JSValueRef* getParameters() const {
        return _parameters;
    }

    constexpr size_t getParameterSize() const {
        return _parametersSize;
    }

    constexpr JSExceptionTracker& getExceptionTracker() {
        return _exceptionTracker;
    }

    constexpr const JSValue& getThisValue() const {
        return _thisValue;
    }

    constexpr void setThisValue(const JSValue& thisValue) {
        _thisValue = thisValue;
    }

    JSValue getParameter(size_t index) const;

    JSValueRef throwError(Error&& error);

    JSFunctionCallContext makeSubContext(const JSValueRef* parameters, size_t parametersSize);

protected:
    IJavaScriptContext& _context;
    const JSValueRef* _parameters;
    size_t _parametersSize;
    JSExceptionTracker& _exceptionTracker;
    JSValue _thisValue;
};

class JSFunctionNativeCallContext : public JSFunctionCallContext {
public:
    JSFunctionNativeCallContext(IJavaScriptContext& context,
                                const JSValueRef* parameters,
                                size_t parametersSize,
                                JSExceptionTracker& exceptionTracker,
                                const ReferenceInfo& referenceInfo);

    const ReferenceInfo& getReferenceInfo() const;

    Value getParameterAsValue(size_t index);
    StringBox getParameterAsString(size_t index);
    Ref<StaticString> getParameterAsStaticString(size_t index);
    double getParameterAsDouble(size_t index);
    int32_t getParameterAsInt(size_t index);
    bool getParameterAsBool(size_t index);
    Ref<ValueFunction> getParameterAsFunction(size_t index);
    JSTypedArray getParameterAsTypedArray(size_t index);
    BytesView getParameterAsBytesView(size_t index);
    Ref<RefCountable> getParameterAsWrappedObject(size_t index);

private:
    const ReferenceInfo& _referenceInfo;
};

} // namespace Valdi

#define CHECK_CALL_CONTEXT(__callContext__)                                                                            \
    if (!(__callContext__).getExceptionTracker()) {                                                                    \
        return (__callContext__).getContext().newUndefined();                                                          \
    }
