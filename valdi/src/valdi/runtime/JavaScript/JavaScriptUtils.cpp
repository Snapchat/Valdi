//
//  JavaScriptUtils.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/3/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"

#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ContextAttachedValdiObject.hpp"

#include "valdi/runtime/JavaScript/JSFunctionWithValueFunction.hpp"
#include "valdi/runtime/JavaScript/JavaScriptCircularRefChecker.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptValueMarshaller.hpp"
#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"
#include "valdi/runtime/JavaScript/WrappedJSValueRef.hpp"

#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include <atomic>
#include <sstream>

namespace Valdi {

Error convertJSErrorToValdiError(IJavaScriptContext& jsContext, JSValueRef jsValue, const Error* cause) {
    // Attempt at symbolicating the error
    if (jsContext.getListener() != nullptr) {
        jsValue = jsContext.getListener()->symbolicateError(jsValue);
    }

    JSExceptionTracker exceptionTracker(jsContext);

    StringBox messageString;
    StringBox stackString;

    auto message = jsContext.getObjectProperty(jsValue.get(), "message", exceptionTracker);

    if (exceptionTracker) {
        messageString = jsContext.valueToString(message.get(), exceptionTracker);
    }
    exceptionTracker.clearError();

    auto stack = jsContext.getObjectProperty(jsValue.get(), "stack", exceptionTracker);
    if (exceptionTracker && !jsContext.isValueUndefined(stack.get())) {
        stackString = jsContext.valueToString(stack.get(), exceptionTracker);
    }
    exceptionTracker.clearError();

    if (messageString.isEmpty()) {
        messageString = STRING_LITERAL("Unable to build exception message");
    }

    return Error(std::move(messageString), std::move(stackString), cause);
}

JSValueRef convertValdiErrorToJSError(IJavaScriptContext& jsContext,
                                      const Error& error,
                                      JSExceptionTracker& exceptionTracker) {
    auto flattenedError = error.flatten();
    JSValueRef jsError;
    if (flattenedError.getStack().isEmpty()) {
        return jsContext.newError(flattenedError.getMessage().toStringView(), std::nullopt, exceptionTracker);
    } else {
        return jsContext.newError(
            flattenedError.getMessage().toStringView(), {flattenedError.getStack().toStringView()}, exceptionTracker);
    }
}

Ref<ValdiObject> jsValueToValdiObject(IJavaScriptContext& jsContext,
                                      const JSValue& jsValue,
                                      const ReferenceInfoBuilder& referenceInfoBuilder,
                                      JSExceptionTracker& exceptionTracker) {
    return makeShared<WrappedJSValueRef>(jsContext, jsValue, referenceInfoBuilder.build(), exceptionTracker);
}

JSValueRef makeOpaque(IJavaScriptContext& jsContext,
                      const JSValue& jsObject,
                      const ReferenceInfoBuilder& referenceInfoBuilder,
                      JSExceptionTracker& exceptionTracker) {
    auto wrappedJsValue = jsValueToValdiObject(jsContext, jsObject, referenceInfoBuilder, exceptionTracker);
    return jsContext.newWrappedObject(wrappedJsValue, exceptionTracker);
}

JSValueRef makeWrappedObject(IJavaScriptContext& jsContext,
                             const Ref<RefCountable>& wrappedObject,
                             JSExceptionTracker& exceptionTracker,
                             bool attachToContext) {
    if (attachToContext) {
        auto attachedRef = makeShared<ContextAttachedValdiObject>(Ref(Context::currentRoot()), wrappedObject);
        return jsContext.newWrappedObject(attachedRef, exceptionTracker);
    } else {
        return jsContext.newWrappedObject(wrappedObject, exceptionTracker);
    }
}

Ref<RefCountable> unwrapWrappedObject(IJavaScriptContext& jsContext,
                                      const JSValue& jsValue,
                                      JSExceptionTracker& exceptionTracker) {
    auto wrappedObject = jsContext.valueToWrappedObject(jsValue, exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    auto* wrapper = dynamic_cast<ContextAttachedValdiObject*>(wrappedObject.get());
    if (wrapper != nullptr) {
        auto innerObject = wrapper->getInnerObject();
        if (innerObject == nullptr) {
            exceptionTracker.onError(Error("Native object reference was destroyed"));
            return nullptr;
        }
        return innerObject;
    } else {
        return wrappedObject;
    }
}

std::optional<JSValue> unwrapJSValueIfNeeded(IJavaScriptContext& jsContext,
                                             RefCountable* object,
                                             JSExceptionTracker& exceptionTracker) {
    auto* wrappedJsValue = dynamic_cast<JSValueRefHolder*>(object);
    if (wrappedJsValue == nullptr) {
        return std::nullopt;
    }

    if (!wrappedJsValue->isFromJsContext(jsContext)) {
        // Making sure they come from the same JSContext.
        return std::nullopt;
    }
    return {wrappedJsValue->getJsValue(jsContext, exceptionTracker)};
}

JSValueRef valdiObjectToJSValue(IJavaScriptContext& jsContext,
                                const Ref<ValdiObject>& valdiObject,
                                JSExceptionTracker& exceptionTracker) {
    auto jsValue = unwrapJSValueIfNeeded(jsContext, valdiObject.get(), exceptionTracker);
    if (!jsValue || !exceptionTracker) {
        return jsContext.newUndefined();
    }

    return JSValueRef::makeRetained(jsContext, jsValue.value());
}

bool inflateJSValueFromTypedObject(IJavaScriptContext& jsContext,
                                   const Ref<ValueTypedObject>& typedObject,
                                   const ReferenceInfoBuilder& referenceInfoBuilder,
                                   const JSValue& outJsValue,
                                   JSExceptionTracker& exceptionTracker) {
    for (const auto& it : *typedObject) {
        auto value = valueToJSValue(jsContext, it.value, referenceInfoBuilder.withProperty(it.name), exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        jsContext.setObjectProperty(outJsValue, it.name.toStringView(), value.get(), exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }
    }

    return true;
}

JSValueRef typedObjectToJSValue(IJavaScriptContext& jsContext,
                                const Ref<ValueTypedObject>& typedObject,
                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                JSExceptionTracker& exceptionTracker) {
    auto* valueMarshaller = jsContext.getValueMarshaller();
    if (valueMarshaller != nullptr) {
        return valueMarshaller->unmarshallTypedObject(typedObject, referenceInfoBuilder, exceptionTracker);
    } else {
        auto object = jsContext.newObject(exceptionTracker);
        if (!exceptionTracker) {
            return jsContext.newUndefined();
        }

        if (!inflateJSValueFromTypedObject(
                jsContext, typedObject, referenceInfoBuilder, object.get(), exceptionTracker)) {
            return jsContext.newUndefined();
        }

        return object;
    }
}

JSValueRef proxyObjectToJSValue(IJavaScriptContext& jsContext,
                                const Ref<ValueTypedProxyObject>& proxyObject,
                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                JSExceptionTracker& exceptionTracker) {
    auto* valueMarshaller = jsContext.getValueMarshaller();
    if (valueMarshaller != nullptr) {
        return valueMarshaller->unmarshallProxyObject(proxyObject, referenceInfoBuilder, exceptionTracker);
    } else {
        auto object = makeWrappedObject(jsContext, proxyObject, exceptionTracker, true);
        if (!exceptionTracker) {
            return jsContext.newUndefined();
        }

        if (!inflateJSValueFromTypedObject(
                jsContext, proxyObject->getTypedObject(), referenceInfoBuilder, object.get(), exceptionTracker)) {
            return jsContext.newUndefined();
        }

        return object;
    }
}

JSValueRef valueToJSValue(IJavaScriptContext& jsContext,
                          const Valdi::Value& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          JSExceptionTracker& exceptionTracker) {
    switch (value.getType()) {
        case ValueType::InternedString:
            return jsContext.newStringUTF8(value.toStringBox().toStringView(), exceptionTracker);
        case ValueType::StaticString:
            return jsContext.newString(*value.getStaticString(), exceptionTracker);
        case ValueType::Double:
            return jsContext.newNumber(value.toDouble());
        case ValueType::Int:
            return jsContext.newNumber(value.toInt());
        case ValueType::Long:
            return jsContext.newLong(value.toLong(), exceptionTracker);
        case ValueType::Bool:
            return jsContext.newBool(value.toBool());
        case ValueType::Null:
            return jsContext.newNull();
        case ValueType::Undefined:
            return jsContext.newUndefined();
        case ValueType::TypedObject:
            return typedObjectToJSValue(jsContext, value.getTypedObjectRef(), referenceInfoBuilder, exceptionTracker);
        case ValueType::ProxyTypedObject:
            return proxyObjectToJSValue(
                jsContext, value.getTypedProxyObjectRef(), referenceInfoBuilder, exceptionTracker);
        case ValueType::ValdiObject: {
            auto jsValue = unwrapJSValueIfNeeded(jsContext, value.getValdiObject().get(), exceptionTracker);
            if (!exceptionTracker) {
                return jsContext.newUndefined();
            }

            if (jsValue) {
                return JSValueRef::makeRetained(jsContext, jsValue.value());
            }

            return makeWrappedObject(jsContext, value.getValdiObject(), exceptionTracker, true);
        }
        case ValueType::Map: {
            const auto& map = *value.getMap();

            auto object = jsContext.newObject(exceptionTracker);
            if (!exceptionTracker) {
                return jsContext.newUndefined();
            }

            for (const auto& it : map) {
                auto value =
                    valueToJSValue(jsContext, it.second, referenceInfoBuilder.withProperty(it.first), exceptionTracker);
                if (!exceptionTracker) {
                    return jsContext.newUndefined();
                }

                jsContext.setObjectProperty(object.get(), it.first.toStringView(), value.get(), exceptionTracker);
                if (!exceptionTracker) {
                    return jsContext.newUndefined();
                }
            }

            return object;
        }
        case ValueType::Array: {
            const auto& array = *value.getArray();

            auto jsArray = jsContext.newArray(array.size(), exceptionTracker);
            if (!exceptionTracker) {
                return jsContext.newUndefined();
            }

            size_t i = 0;
            for (const auto& item : array) {
                auto itemResult =
                    valueToJSValue(jsContext, item, referenceInfoBuilder.withArrayIndex(i), exceptionTracker);
                if (!exceptionTracker) {
                    return jsContext.newUndefined();
                }
                jsContext.setObjectPropertyIndex(jsArray.get(), i++, itemResult.get(), exceptionTracker);
                if (!exceptionTracker) {
                    return jsContext.newUndefined();
                }
            }

            return jsArray;
        }
        case ValueType::TypedArray: {
            const auto& typedArray = *value.getTypedArray();

            return newTypedArrayFromBytesView(
                jsContext, typedArray.getType(), typedArray.getBuffer(), exceptionTracker);
        }
        case ValueType::Error: {
            exceptionTracker.onError(value.getError());
            return JSValueRef();
        }
        case ValueType::Function: {
            auto func = value.getFunctionRef();

            auto jsValue = unwrapJSValueIfNeeded(jsContext, func.get(), exceptionTracker);
            if (!exceptionTracker) {
                return jsContext.newUndefined();
            }

            if (jsValue) {
                // Unwrap the original JS function.
                return JSValueRef::makeRetained(jsContext, jsValue.value());
            }

            auto callable = makeShared<JSFunctionWithUntypedValueFunction>(std::move(func),
                                                                           referenceInfoBuilder.asFunction().build());
            return jsContext.newFunction(callable, exceptionTracker);
        }
    }
    SC_ASSERT_FAIL("This should not happen");
}

StringBox nameFromJSFunction(IJavaScriptContext& jsContext, const JSValue& jsValue) {
    static auto kNameKey = STRING_LITERAL("name");
    static auto kAnonymousFunction = STRING_LITERAL("<anon>");
    static auto kAnonymousBoundFunction = STRING_LITERAL("bound <anon>");
    static auto kNameWhenBoundAnonymous = STRING_LITERAL("bound ");

    JSExceptionTracker exceptionTracker(jsContext);
    auto name = jsContext.getObjectProperty(jsValue, jsContext.getPropertyNameCached(kNameKey), exceptionTracker);
    if (!exceptionTracker || jsContext.isValueUndefined(name.get())) {
        exceptionTracker.clearError();
        return kAnonymousFunction;
    }

    auto str = jsContext.valueToString(name.get(), exceptionTracker);
    if (!exceptionTracker) {
        exceptionTracker.clearError();
        return kAnonymousFunction;
    }

    if (str.isEmpty()) {
        // In JS empty string means name no set
        return kAnonymousFunction;
    }

    if (str == kNameWhenBoundAnonymous) {
        return kAnonymousBoundFunction;
    }

    return str;
}

static bool checkRefNotCircular(const JSValue& value,
                                JavaScriptCircularRefChecker<JSValue>& circularRefChecker,
                                Valdi::JavaScriptRef<JSValue>& ref) {
    if (circularRefChecker.isRefAlreadyPresent(value)) {
        return false;
    }

    ref = circularRefChecker.push(value);
    return true;
}

struct ObjectConvertVisitor : public IJavaScriptPropertyNamesVisitor {
    const ReferenceInfoBuilder& referenceInfoBuilder;
    Ref<ValueMap> convertedObject;

    explicit ObjectConvertVisitor(const ReferenceInfoBuilder& referenceInfoBuilder)
        : referenceInfoBuilder(referenceInfoBuilder), convertedObject(makeShared<ValueMap>()) {}

    bool visitPropertyName(IJavaScriptContext& context,
                           JSValue object,
                           const JSPropertyName& propertyName,
                           JSExceptionTracker& exceptionTracker) override {
        auto propertyValueResult = context.getObjectProperty(object, propertyName, exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        auto convertedProperty =
            jsValueToValue(context, propertyValueResult.get(), referenceInfoBuilder, exceptionTracker);
        if (!exceptionTracker) {
            return false;
        }

        if (!context.isValueUndefined(propertyValueResult.get())) {
            auto cppPropertyName = context.propertyNameToString(propertyName);

            auto convertedProperty = jsValueToValue(context,
                                                    propertyValueResult.get(),
                                                    referenceInfoBuilder.withProperty(cppPropertyName),
                                                    exceptionTracker);
            if (!exceptionTracker) {
                return false;
            }

            (*convertedObject)[cppPropertyName] = std::move(convertedProperty);
        }

        return true;
    }
};

void throwInvalidType(IJavaScriptContext& jsContext,
                      const JSValue& jsValue,
                      JSExceptionTracker& exceptionTracker,
                      ValueType expectedType) {
    exceptionTracker.onError(Value::invalidTypeError(expectedType, jsContext.getValueType(jsValue)));
}

Value untypedJsObjectToValue(IJavaScriptContext& jsContext,
                             const JSValue& jsValue,
                             const ReferenceInfoBuilder& referenceInfoBuilder,
                             JSExceptionTracker& exceptionTracker) {
    ObjectConvertVisitor visitor(referenceInfoBuilder);
    jsContext.visitObjectPropertyNames(jsValue, exceptionTracker, visitor);

    if (!exceptionTracker) {
        return Value::undefined();
    }

    return Value(visitor.convertedObject);
}

Value jsObjectToValue(IJavaScriptContext& jsContext,
                      const JSValue& jsValue,
                      const ReferenceInfoBuilder& referenceInfoBuilder,
                      JSExceptionTracker& exceptionTracker) {
    auto* valueMarshaller = jsContext.getValueMarshaller();

    if (valueMarshaller == nullptr) {
        return untypedJsObjectToValue(jsContext, jsValue, referenceInfoBuilder, exceptionTracker);
    }

    auto output = valueMarshaller->tryUnwrap(jsValue, exceptionTracker);
    if (!exceptionTracker) {
        return output;
    }

    if (output.isUndefined()) {
        // No proxy objects found in the object, marshall as a plain map
        return untypedJsObjectToValue(jsContext, jsValue, referenceInfoBuilder, exceptionTracker);
    } else {
        // Proxy object found, return it
        return output;
    }
}

size_t jsArrayGetLength(IJavaScriptContext& jsContext, const JSValue& jsValue, JSExceptionTracker& exceptionTracker) {
    static auto kLength = STRING_LITERAL("length");

    auto length = jsContext.getObjectProperty(jsValue, jsContext.getPropertyNameCached(kLength), exceptionTracker);
    if (!exceptionTracker) {
        return 0;
    }

    return static_cast<size_t>(jsContext.valueToInt(length.get(), exceptionTracker));
}

Value jsArrayToValue(IJavaScriptContext& jsContext,
                     const JSValue& jsValue,
                     const ReferenceInfoBuilder& referenceInfoBuilder,
                     JSExceptionTracker& exceptionTracker) {
    auto arrayLength = jsArrayGetLength(jsContext, jsValue, exceptionTracker);
    if (!exceptionTracker) {
        return Value::undefined();
    }

    auto array = Valdi::ValueArray::make(arrayLength);

    for (size_t i = 0; i < arrayLength; i++) {
        auto propertyValue = jsContext.getObjectPropertyForIndex(jsValue, i, exceptionTracker);
        if (!exceptionTracker) {
            return Value::undefined();
        }

        auto convertedValue =
            jsValueToValue(jsContext, propertyValue.get(), referenceInfoBuilder.withArrayIndex(i), exceptionTracker);
        if (!exceptionTracker) {
            return Value::undefined();
        }

        array->emplace(i, std::move(convertedValue));
    }

    return Value(array);
}

STRING_CONST(refKey, "$ref");

static void attachRefCountablePtrToArrayBuffer(IJavaScriptContext& jsContext,
                                               const JSValue& arrayBuffer,
                                               const BytesView& bytesView,
                                               JSExceptionTracker& exceptionTracker) {
    if (bytesView.empty() || bytesView.getSource() == nullptr) {
        return;
    }

    auto wrappedObject = jsContext.newWrappedObject(bytesView.getSource(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    jsContext.setObjectProperty(
        arrayBuffer, jsContext.getPropertyNameCached(refKey()), wrappedObject.get(), exceptionTracker);
}

static Ref<RefCountable> getAttachedRefCountableFromArrayBuffer(IJavaScriptContext& jsContext,
                                                                const JSValue& arrayBuffer,
                                                                JSExceptionTracker& exceptionTracker) {
    auto jsSource =
        jsContext.getObjectProperty(arrayBuffer, jsContext.getPropertyNameCached(refKey()), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    if (jsContext.isValueUndefined(jsSource.get())) {
        return nullptr;
    }

    return jsContext.valueToWrappedObject(jsSource.get(), exceptionTracker);
}

Ref<ValueTypedArray> jsTypedArrayToValueTypedArray(IJavaScriptContext& jsContext,
                                                   const JSValue& jsValue,
                                                   const ReferenceInfoBuilder& referenceInfoBuilder,
                                                   JSExceptionTracker& exceptionTracker) {
    auto result = jsContext.valueToTypedArray(jsValue, exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    if (result.length == 0) {
        return makeShared<ValueTypedArray>(result.type, BytesView());
    }

    auto source = getAttachedRefCountableFromArrayBuffer(jsContext, result.arrayBuffer.get(), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    if (source == nullptr) {
        if (jsContext.getTaskScheduler() != nullptr) {
            // Schedule an unattributed task, so that we can retain the js array using the global native refs
            jsContext.getTaskScheduler()->dispatchOnJsThreadSync(nullptr, [&](auto& jsEntry) {
                source = jsValueToValdiObject(
                    jsEntry.jsContext, result.arrayBuffer.get(), referenceInfoBuilder, exceptionTracker);
            });
        } else {
            source = jsValueToValdiObject(jsContext, result.arrayBuffer.get(), referenceInfoBuilder, exceptionTracker);
        }
    }

    return makeShared<ValueTypedArray>(result.type,
                                       BytesView(source, reinterpret_cast<const Byte*>(result.data), result.length));
}

JSValueRef newTypedArrayFromBytesView(IJavaScriptContext& jsContext,
                                      TypedArrayType arrayType,
                                      const BytesView& bytesView,
                                      JSExceptionTracker& exceptionTracker) {
    JSValueRef arrayBuffer;
    auto unwrappedArrayBuffer = unwrapJSValueIfNeeded(jsContext, bytesView.getSource().get(), exceptionTracker);

    if (unwrappedArrayBuffer) {
        arrayBuffer = JSValueRef::makeRetained(jsContext, unwrappedArrayBuffer.value());
    } else {
        if (!exceptionTracker) {
            return arrayBuffer;
        }

        arrayBuffer = jsContext.newArrayBuffer(bytesView, exceptionTracker);
        if (!exceptionTracker) {
            return arrayBuffer;
        }

        attachRefCountablePtrToArrayBuffer(jsContext, arrayBuffer.get(), bytesView, exceptionTracker);
        if (!exceptionTracker) {
            return arrayBuffer;
        }
    }

    if (arrayType == TypedArrayType::ArrayBuffer) {
        return arrayBuffer;
    } else {
        return jsContext.newTypedArrayFromArrayBuffer(arrayType, arrayBuffer.get(), exceptionTracker);
    }
}

Value jsTypedArrayToValue(IJavaScriptContext& jsContext,
                          const JSValue& jsValue,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          JSExceptionTracker& exceptionTracker) {
    auto typedArray = jsTypedArrayToValueTypedArray(jsContext, jsValue, referenceInfoBuilder, exceptionTracker);
    return Value(typedArray);
}

JSValueFunctionFactory defaultJSValueFunctionFactory() {
    return [](IJavaScriptContext& jsContext,
              const JSValue& jsValue,
              const ReferenceInfo& referenceInfo,
              JSExceptionTracker& exceptionTracker) {
        return makeShared<UntypedValueFunctionWithJSValue>(jsContext, jsValue, referenceInfo, exceptionTracker);
    };
}

Ref<ValueFunction> jsFunctionToFunction(IJavaScriptContext& jsContext,
                                        const JSValue& jsValue,
                                        const ReferenceInfoBuilder& referenceInfoBuilder,
                                        JSExceptionTracker& exceptionTracker,
                                        const JSValueFunctionFactory& factory) {
    auto callableValueFunction =
        castOrNull<JSFunctionWithValueFunction>(jsContext.valueToFunction(jsValue, exceptionTracker));
    if (!exceptionTracker) {
        return nullptr;
    }

    if (callableValueFunction != nullptr) {
        // Unwrap the converted function if it is one
        return callableValueFunction->getFunction(jsContext, exceptionTracker, false);
    }

    // Otherwise we create a bridged function
    auto valueFunction = factory(jsContext, jsValue, referenceInfoBuilder.asFunction().build(), exceptionTracker);
    if (valueFunction != nullptr) {
        auto exportMode = jsContext.getValueFunctionExportMode(jsValue, exceptionTracker);
        if (exportMode != kJSFunctionExportModeEmpty) {
            valueFunction->setShouldBlockMainThread((exportMode & kJSFunctionExportModeSyncWithMainThread) != 0);
            valueFunction->setIgnoreIfValdiContextIsDestroyed((exportMode & kJSFunctionExportModeInterruptible) != 0);
            valueFunction->setSingleCall((exportMode & kJSFunctionExportModeSingleCall) != 0);
        }
    }

    return valueFunction;
}

Ref<ValueFunction> jsValueToFunction(IJavaScriptContext& jsContext,
                                     const JSValue& jsValue,
                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                     JSExceptionTracker& exceptionTracker) {
    return jsValueToFunction(
        jsContext, jsValue, referenceInfoBuilder, exceptionTracker, defaultJSValueFunctionFactory());
}

Ref<ValueFunction> jsValueToFunction(IJavaScriptContext& jsContext,
                                     const JSValue& jsValue,
                                     const ReferenceInfoBuilder& referenceInfoBuilder,
                                     JSExceptionTracker& exceptionTracker,
                                     const JSValueFunctionFactory& factory) {
    if (!jsContext.isValueFunction(jsValue)) {
        throwInvalidType(jsContext, jsValue, exceptionTracker, ValueType::Function);
        return nullptr;
    }

    return jsFunctionToFunction(jsContext, jsValue, referenceInfoBuilder, exceptionTracker, factory);
}

Ref<ValdiObject> jsWrappedObjectToValdiObject(IJavaScriptContext& jsContext,
                                              const JSValue& jsValue,
                                              JSExceptionTracker& exceptionTracker) {
    auto innerObject = unwrapWrappedObject(jsContext, jsValue, exceptionTracker);
    if (innerObject == nullptr) {
        return nullptr;
    }

    auto valdiObject = castOrNull<ValdiObject>(innerObject);
    if (valdiObject == nullptr) {
        exceptionTracker.onError(Error("Not a ValdiObject"));
        return nullptr;
    }

    return valdiObject;
}

Value jsWrappedObjectToValue(IJavaScriptContext& jsContext,
                             const JSValue& jsValue,
                             JSExceptionTracker& exceptionTracker) {
    auto valdiObject = jsWrappedObjectToValdiObject(jsContext, jsValue, exceptionTracker);
    if (!exceptionTracker) {
        return Value::undefined();
    }

    return Value(valdiObject);
}

Value jsErrorToValue(IJavaScriptContext& jsContext, const JSValue& jsValue, JSExceptionTracker& /*exceptionTracker*/) {
    return Value(convertJSErrorToValdiError(jsContext, JSValueRef::makeRetained(jsContext, jsValue), nullptr));
}

Value jsValueToValue(IJavaScriptContext& jsContext,
                     const JSValue& jsValue,
                     const ReferenceInfoBuilder& referenceInfoBuilder,
                     JSExceptionTracker& exceptionTracker) {
    Valdi::JavaScriptRef<JSValue> ref;

    if (!checkRefNotCircular(jsValue, exceptionTracker.getCircularRefChecker(), ref)) {
        static auto kCircular = STRING_LITERAL("<circular>");
        return Valdi::Value(kCircular);
    }

    switch (jsContext.getValueType(jsValue)) {
        case Valdi::ValueType::Null:
            return Valdi::Value();
        case Valdi::ValueType::Undefined:
            return Valdi::Value::undefined();
        case Valdi::ValueType::InternedString:
        case Valdi::ValueType::StaticString:
            // We always use the interned string API when going from an untyped JSValue to a Value
            return Value(jsContext.valueToString(jsValue, exceptionTracker));
        case Valdi::ValueType::Int:
            return Value(jsContext.valueToInt(jsValue, exceptionTracker));
        case Valdi::ValueType::Double:
            return Value(jsContext.valueToDouble(jsValue, exceptionTracker));
        case Valdi::ValueType::Long:
            return Value(jsContext.valueToLong(jsValue, exceptionTracker).toInt64());
        case Valdi::ValueType::Bool:
            return Value(jsContext.valueToBool(jsValue, exceptionTracker));
        case Valdi::ValueType::Map:
            return jsObjectToValue(jsContext, jsValue, referenceInfoBuilder, exceptionTracker);
        case Valdi::ValueType::Array:
            return jsArrayToValue(jsContext, jsValue, referenceInfoBuilder, exceptionTracker);
        case Valdi::ValueType::TypedArray:
            return jsTypedArrayToValue(jsContext, jsValue, referenceInfoBuilder, exceptionTracker);
        case Valdi::ValueType::Function:
            return Value(jsFunctionToFunction(
                jsContext, jsValue, referenceInfoBuilder, exceptionTracker, defaultJSValueFunctionFactory()));
        case Valdi::ValueType::TypedObject:
            std::abort();
        case Valdi::ValueType::ProxyTypedObject:
            std::abort();
        case Valdi::ValueType::ValdiObject:
            return jsWrappedObjectToValue(jsContext, jsValue, exceptionTracker);
        case Valdi::ValueType::Error:
            return jsErrorToValue(jsContext, jsValue, exceptionTracker);
    }
}

std::string formatJsModule(const std::string_view& jsModule) {
    std::string_view modulePrefix = "(function(require, module, exports) { ";
    std::string_view moduleSuffix = "\n})";

    std::string_view preamble;
    if (!hasPrefix(jsModule, 0, "\"use strict\";\n")) {
        preamble = "if (module) module.sourceMapLineOffset = 1;\n";
    }

    std::string formattedModule;
    formattedModule.reserve(modulePrefix.size() + preamble.size() + jsModule.size() + moduleSuffix.size());
    formattedModule.append(modulePrefix);
    formattedModule.append(preamble);

    formattedModule.append(reinterpret_cast<const char*>(jsModule.data()), jsModule.size());
    formattedModule.append(moduleSuffix);

    return formattedModule;
}

static uint32_t kPreCompiledJsModuleMagic = 0x0100C634;

BytesView makePreCompiledJsModule(const Byte* byteCode, size_t length) {
    auto jsModule = makeShared<ByteBuffer>();
    jsModule->reserve(sizeof(kPreCompiledJsModuleMagic) + length);
    jsModule->append(reinterpret_cast<const Byte*>(&kPreCompiledJsModuleMagic),
                     reinterpret_cast<const Byte*>(&kPreCompiledJsModuleMagic) + sizeof(kPreCompiledJsModuleMagic));
    jsModule->append(byteCode, byteCode + length);

    return jsModule->toBytesView();
}

std::optional<BytesView> getPreCompiledJsModuleData(const BytesView& jsModule) {
    if (jsModule.size() < sizeof(kPreCompiledJsModuleMagic)) {
        return std::nullopt;
    }

    const auto* data = jsModule.data();
    for (size_t i = 0; i < sizeof(kPreCompiledJsModuleMagic); i++) {
        if (data[i] != reinterpret_cast<const Byte*>(&kPreCompiledJsModuleMagic)[i]) {
            return std::nullopt;
        }
    }

    return BytesView(jsModule.getSource(),
                     jsModule.data() + sizeof(kPreCompiledJsModuleMagic),
                     jsModule.size() - sizeof(kPreCompiledJsModuleMagic));
}

} // namespace Valdi
