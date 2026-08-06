//
//  V8JavaScriptContext.hpp
//  valdi-v8
//
//  Created by Ramzy Jaber on 6/13/23.
//

#pragma once

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

// TODO(rjaber): Have out own implementations of platform
#include "v8/libplatform/libplatform.h"
#include "v8/v8.h"

namespace Valdi::V8 {

class IndirectV8Persistent;

class V8JavaScriptContext : public IJavaScriptContext {
public:
    explicit V8JavaScriptContext(JavaScriptTaskScheduler* taskScheduler);
    ~V8JavaScriptContext() override;

    JSValueRef getGlobalObject(JSExceptionTracker& exceptionTracker) override;

    JSValueRef evaluate(const std::string& script,
                        const std::string_view& sourceFilename,
                        JSExceptionTracker& exceptionTracker) override;

    JSValueRef evaluatePreCompiled(const BytesView& /*bytes*/,
                                   const std::string_view& sourceFilename,
                                   JSExceptionTracker& exceptionTracker) override;

    BytesView preCompile(const std::string_view& /*script*/,
                         const std::string_view& /*sourceFilename*/,
                         JSExceptionTracker& exceptionTracker) override;

    JSPropertyNameRef newPropertyName(const std::string_view& str) override;

    StringBox propertyNameToString(const JSPropertyName& propertyName) override;

    JSValueRef newObject(JSExceptionTracker& exceptionTracker) override;

    JSValueRef newFunction(const Ref<JSFunction>& callable, JSExceptionTracker& exceptionTracker) override;

    JSValueRef newStringUTF8(const std::string_view& str, JSExceptionTracker& exceptionTracker) override;
    JSValueRef newStringUTF16(const std::u16string_view& str, JSExceptionTracker& exceptionTracker) override;

    JSValueRef newArray(size_t initialSize, JSExceptionTracker& exceptionTracker) override;
    JSValueRef newArrayWithValues(const JSValue* values, size_t size, JSExceptionTracker& exceptionTracker) override;

    JSValueRef newArrayBuffer(const BytesView& buffer, JSExceptionTracker& exceptionTracker) override;

    JSValueRef newTypedArrayFromArrayBuffer(const TypedArrayType& type,
                                            const JSValue& arrayBuffer,
                                            JSExceptionTracker& exceptionTracker) override;

    JSValueRef newWrappedObject(const Ref<RefCountable>& wrappedObject, JSExceptionTracker& exceptionTracker) override;

    JSValueRef newNativeClass(const Ref<RefCountable>& classOpaque,
                              const JSClassDefinition& classDefinition,
                              JSExceptionTracker& exceptionTracker) override;

    JSValueRef newObjectFromNativeClass(const Ref<RefCountable>& opaque,
                                        const JSValue& cls,
                                        JSExceptionTracker& exceptionTracker) override;

    JSValueRef newWeakRef(const JSValue& object, JSExceptionTracker& exceptionTracker) override;

    JSValueRef getObjectProperty(const JSValue& object,
                                 const std::string_view& propertyName,
                                 JSExceptionTracker& exceptionTracker) override;

    JSValueRef getObjectProperty(const JSValue& object,
                                 const JSPropertyName& propertyName,
                                 JSExceptionTracker& exceptionTracker) override;

    JSValueRef getObjectPropertyForIndex(const JSValue& object,
                                         size_t index,
                                         JSExceptionTracker& exceptionTracker) override;

    bool hasObjectProperty(const JSValue& object, const JSPropertyName& propertyName) override;

    void setObjectProperty(const JSValue& object,
                           const std::string_view& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) override;

    void setObjectProperty(const JSValue& object,
                           const JSPropertyName& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) override;

    void setObjectProperty(const JSValue& object,
                           const JSValue& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) override;

    void setObjectPropertyIndex(const JSValue& object,
                                size_t index,
                                const JSValue& value,
                                JSExceptionTracker& exceptionTracker) override;

    JSValueRef callObjectAsFunction(const JSValue& object, JSFunctionCallContext& callContext) override;

    JSValueRef callObjectAsConstructor(const JSValue& object, JSFunctionCallContext& callContext) override;

    void visitObjectPropertyNames(const JSValue& object,
                                  JSExceptionTracker& exceptionTracker,
                                  IJavaScriptPropertyNamesVisitor& visitor) override;

    JSValueRef derefWeakRef(const JSValue& weakRef, JSExceptionTracker& exceptionTracker) override;

    StringBox valueToString(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    Ref<StaticString> valueToStaticString(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    bool valueToBool(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    double valueToDouble(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    int32_t valueToInt(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    Ref<RefCountable> valueToWrappedObject(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    JSTypedArray valueToTypedArray(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    Ref<JSFunction> valueToFunction(const JSValue& value, JSExceptionTracker& exceptionTracker) override;
    int64_t valueToLong(const JSValue& value, JSExceptionTracker& exceptionTracker);

    ValueType getValueType(const JSValue& value) override;

    bool isValueEqual(const JSValue& left, const JSValue& right) override;
    bool isValueUndefined(const JSValue& value) override;
    bool isValueNull(const JSValue& value) override;
    bool isValueFunction(const JSValue& value) override;
    bool isValueNumber(const JSValue& value) override;
    bool isValueLong(const JSValue& value) override;
    bool isValueObject(const JSValue& value) override;

    void retainValue(const JSValue& value) override;
    void releaseValue(const JSValue& value) override;

    void retainPropertyName(const JSPropertyName& value) override;
    void releasePropertyName(const JSPropertyName& value) override;

    void garbageCollect() override;

    JavaScriptContextMemoryStatistics dumpMemoryStatistics() override;
    void enqueueMicrotask(const JSValue& value, JSExceptionTracker& exceptionTracker) override;

    void willEnterVM() override;
    void willExitVM(JSExceptionTracker& exceptionTracker) override;

protected:
    // The public variants of those methods implement some caching to avoid repeatedly creating
    // values from the engine for common values.

    JSValueRef onNewBool(bool boolean) override;

    JSValueRef onNewNumber(int32_t number) override;

    JSValueRef onNewNumber(double number) override;

    JSValueRef onNewNull() override;

    JSValueRef onNewUndefined() override;

    void onInitialize(JSExceptionTracker& exceptionTracker) override;

private:
    inline JSValueRef toRetainedJSValueRef(const IndirectV8Persistent& value);
    inline JSValueRef toUnretainedJSValueRef(const IndirectV8Persistent& value);

    v8::ArrayBuffer::Allocator* _allocator;
    v8::Isolate::CreateParams _params;
    v8::Isolate* _isolate;
    v8::Global<v8::Context> _context;
    v8::Persistent<v8::ObjectTemplate> _valdiObjectTemplate;
    v8::Persistent<v8::Private> _functionDataProperty;
};

} // namespace Valdi::V8
