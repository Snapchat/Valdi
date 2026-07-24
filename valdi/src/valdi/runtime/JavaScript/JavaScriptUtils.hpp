//
//  JavaScriptUtils.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/3/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"

namespace Valdi {

class ValueFunctionWithJSValue;

class JSValueForNativeObjectResolver {
public:
    virtual ~JSValueForNativeObjectResolver() = default;

    virtual std::optional<JSValue> getJSValueForNativeObject(RefCountable* object) = 0;
};

Error convertJSErrorToValdiError(IJavaScriptContext& jsContext, JSValueRef jsValue, const Error* cause);

JSValueRef convertValdiErrorToJSError(IJavaScriptContext& jsContext,
                                      const Error& error,
                                      JSExceptionTracker& exceptionTracker);

JSValueRef makeWrappedObject(IJavaScriptContext& jsContext,
                             const Ref<RefCountable>& wrappedObject,
                             JSExceptionTracker& exceptionTracker,
                             bool attachToContext);
Ref<RefCountable> unwrapWrappedObject(IJavaScriptContext& jsContext,
                                      const JSValue& jsValue,
                                      JSExceptionTracker& exceptionTracker);

JSValueRef makeOpaque(IJavaScriptContext& jsContext,
                      const JSValue& jsObject,
                      const ReferenceInfoBuilder& referenceInfoBuilder,
                      JSExceptionTracker& exceptionTracker);

std::string formatJsModule(const std::string_view& jsModule);
std::optional<BytesView> getPreCompiledJsModuleData(const BytesView& jsModule);
BytesView makePreCompiledJsModule(const Byte* byteCode, size_t length);

[[nodiscard]] JSValueRef valueToJSValue(IJavaScriptContext& jsContext,
                                        const Valdi::Value& value,
                                        JSValueForNativeObjectResolver* nativeObjectResolver,
                                        const ReferenceInfoBuilder& referenceInfoBuilder,
                                        JSExceptionTracker& exceptionTracker);

[[nodiscard]] JSValueRef valueToJSValue(IJavaScriptContext& jsContext,
                                        const Valdi::Value& value,
                                        const ReferenceInfoBuilder& referenceInfoBuilder,
                                        JSExceptionTracker& exceptionTracker);

[[nodiscard]] JSValueRef typedObjectToJSValue(IJavaScriptContext& jsContext,
                                              const Ref<ValueTypedObject>& typedObject,
                                              const ReferenceInfoBuilder& referenceInfoBuilder,
                                              JSExceptionTracker& exceptionTracker);

[[nodiscard]] JSValueRef proxyObjectToJSValue(IJavaScriptContext& jsContext,
                                              const Ref<ValueTypedProxyObject>& proxyObject,
                                              const ReferenceInfoBuilder& referenceInfoBuilder,
                                              JSExceptionTracker& exceptionTracker);
Value jsValueToValue(IJavaScriptContext& jsContext,
                     const JSValue& jsValue,
                     const ReferenceInfoBuilder& referenceInfoBuilder,
                     JSExceptionTracker& exceptionTracker);

Ref<ValueFunction> jsValueToFunction(IJavaScriptContext& jsContext,
                                     const JSValue& jsValue,
                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                     JSExceptionTracker& exceptionTracker);

using JSValueFunctionFactory = Function<Ref<ValueFunctionWithJSValue>(
    IJavaScriptContext&, const JSValue&, const ReferenceInfo&, JSExceptionTracker&)>;

Ref<ValueFunction> jsValueToFunction(IJavaScriptContext& jsContext,
                                     const JSValue& jsValue,
                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                     JSExceptionTracker& exceptionTracker,
                                     const JSValueFunctionFactory& factory);

Ref<ValueFunction> jsFunctionToFunction(IJavaScriptContext& jsContext,
                                        const JSValue& jsValue,
                                        const ReferenceInfoBuilder& referenceInfoBuilder,
                                        JSExceptionTracker& exceptionTracker,
                                        const JSValueFunctionFactory& factory);

Value jsWrappedObjectToValue(IJavaScriptContext& jsContext,
                             const JSValue& jsValue,
                             JSExceptionTracker& exceptionTracker);

size_t jsArrayGetLength(IJavaScriptContext& jsContext, const JSValue& jsValue, JSExceptionTracker& exceptionTracker);

Ref<ValdiObject> jsWrappedObjectToValdiObject(IJavaScriptContext& jsContext,
                                              const JSValue& jsValue,
                                              JSExceptionTracker& exceptionTracker);

Ref<ValdiObject> jsValueToValdiObject(IJavaScriptContext& jsContext,
                                      const JSValue& jsValue,
                                      const ReferenceInfoBuilder& referenceInfoBuilder,
                                      JSExceptionTracker& exceptionTracker);

JSValueRef valdiObjectToJSValue(IJavaScriptContext& jsContext,
                                const Ref<ValdiObject>& valdiObject,
                                JSExceptionTracker& exceptionTracker);

Ref<ValueTypedArray> jsTypedArrayToValueTypedArray(IJavaScriptContext& jsContext,
                                                   const JSValue& jsValue,
                                                   const ReferenceInfoBuilder& referenceInfoBuilder,
                                                   JSExceptionTracker& exceptionTracker);

JSValueRef newTypedArrayFromBytesView(IJavaScriptContext& jsContext,
                                      TypedArrayType arrayType,
                                      const BytesView& bytesView,
                                      JSExceptionTracker& exceptionTracker);

StringBox nameFromJSFunction(IJavaScriptContext& jsContext, const JSValue& jsValue);

} // namespace Valdi

#define RETURN_ERROR_ON_EXCEPTION(__exceptionTracker__)                                                                \
    if (!(__exceptionTracker__)) {                                                                                     \
        return convertJSErrorToValdiError((__exceptionTracker__).getExceptionAndClear());                              \
    }
