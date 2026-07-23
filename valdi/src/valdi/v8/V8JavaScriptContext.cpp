//
//  V8JavaScriptContext.cpp
//  valdi-v8
//
//  Created by Ramzy Jaber on 6/13/23.
//

#include "valdi/v8/V8JavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/Utils/RefCountableAutoreleasePool.hpp"
#include "valdi/v8/HeapDumpOutputStream.hpp"
#include "valdi/v8/IndirectV8Persistent.hpp"

#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"

#include "valdi/v8/V8JavaScriptContext.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "v8/v8-profiler.h"
#include <utils/debugging/Assert.hpp>

#include <cassert>
#include <cstdio>
#include <iostream>

namespace {
void initializeV8Engine() {
    static std::once_flag flag;
    static std::unique_ptr<v8::Platform> platform;
    std::call_once(flag, [&]() {
        // TODO(rjaber): Not needed, not sure if that will change in the future keeping as a stub.
        //  v8::V8::InitializeICUDefaultLocation("");
        //  v8::V8::InitializeExternalStartupData("");
        platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform(platform.get());
        v8::V8::Initialize();
    });
}
} // namespace

namespace Valdi::V8 {

TypedArrayType getTypedArrayType(const v8::Local<v8::Value>& val) {
    if (val->IsUint8Array()) {
        return TypedArrayType::Uint8Array;
    } else if (val->IsUint16Array()) {
        return TypedArrayType::Uint16Array;
    } else if (val->IsUint32Array()) {
        return TypedArrayType::Uint32Array;
    } else if (val->IsInt8Array()) {
        return TypedArrayType::Int8Array;
    } else if (val->IsInt16Array()) {
        return TypedArrayType::Int16Array;
    } else if (val->IsInt32Array()) {
        return TypedArrayType::Int32Array;
    } else if (val->IsFloat32Array()) {
        return TypedArrayType::Float32Array;
    } else if (val->IsFloat64Array()) {
        return TypedArrayType::Float64Array;
    } else {
        SC_ABORT("Unrecognized TypedArray type");
        return Uint8Array;
    }
}

enum class IsolateDataSlot { ContextSlot = 0, Count };

enum class InternalFieldSlot { DataSlot, Count };

void freeArrayBuffer(void* data, size_t length, void* deleter_data) {
    RefCountableAutoreleasePool::release(deleter_data);
}

void freeWrappedPointer(void* opaque) {
    RefCountableAutoreleasePool::release(opaque);
}

inline JSValue toValdiJSValue(const IndirectV8Persistent& value) {
    static_assert(sizeof(IndirectV8Persistent) < 128);
    return JSValue(value);
}

inline JSPropertyName toValdiJSPropertyName(const IndirectV8Persistent& value) {
    static_assert(sizeof(IndirectV8Persistent) < 64);
    return JSPropertyName(value);
}

IndirectV8Persistent fromValdiJSValueToIndirect(const JSValue& jsValue) {
    return jsValue.bridge<IndirectV8Persistent>();
}

inline IndirectV8Persistent fromValdiJSPropertyNameToIndirect(const JSPropertyName& value) {
    return value.bridge<IndirectV8Persistent>();
}

inline v8::Local<v8::Value> fromValdiJSValue(v8::Isolate* isolate,
                                             const JSValue& jsValue,
                                             JSExceptionTracker& exceptionTracker) {
    const IndirectV8Persistent indirect = fromValdiJSValueToIndirect(jsValue);
    const v8::Local<v8::Value> retval = indirect.get(isolate);
    if (retval.IsEmpty()) {
        exceptionTracker.onError("Provided JSValue does not contain an underlying value");
        return v8::Local<v8::Value>();
    } else {
        return retval;
    }
}

inline v8::Local<v8::String> fromValdiJSPropertyName(v8::Isolate* isolate,
                                                     const JSPropertyName& value,
                                                     JSExceptionTracker& exceptionTracker) {
    auto indirect = fromValdiJSPropertyNameToIndirect(value);
    auto retval = indirect.get(isolate);
    if (retval.IsEmpty() || !retval->IsString()) {
        exceptionTracker.onError("Failed to get the underlying key for property name");
        return v8::Local<v8::String>();
    } else {
        return v8::Local<v8::String>::Cast(retval);
    }
}

inline JSValueRef V8JavaScriptContext::toRetainedJSValueRef(const IndirectV8Persistent& value) {
    return JSValueRef::makeRetained(*this, toValdiJSValue(value));
}

inline JSValueRef V8JavaScriptContext::toUnretainedJSValueRef(const IndirectV8Persistent& value) {
    return JSValueRef::makeUnretained(*this, toValdiJSValue(value));
}

inline void setObjectPropertyHelper(v8::Local<v8::Context>& context,
                                    v8::Local<v8::Object> obj,
                                    v8::Local<v8::Name> key,
                                    v8::Local<v8::Value> value,
                                    bool enumerable,
                                    JSExceptionTracker& exceptionTracker) {
    v8::PropertyAttribute attr = enumerable ? v8::PropertyAttribute::None : v8::PropertyAttribute::DontEnum;
    auto result = obj->DefineOwnProperty(context, key, value, attr);
    if (result.IsNothing()) {
        exceptionTracker.onError("Failed to set property for object");
    }
}

inline void setObjectPropertyIndexHelper(v8::Local<v8::Context>& context,
                                         v8::Local<v8::Object> obj,
                                         uint32_t key,
                                         v8::Local<v8::Value> value,
                                         bool enumerable,
                                         JSExceptionTracker& exceptionTracker) {
    if (!enumerable) {
        exceptionTracker.onError("non-enumerable property index is not currently supported");
        return;
    }

    if (!obj->Set(context, key, value).FromMaybe(false)) {
        exceptionTracker.onError("Failed to set property index on object");
    }
}

V8JavaScriptContext::V8JavaScriptContext(JavaScriptTaskScheduler* taskScheduler) : IJavaScriptContext(taskScheduler) {
    initializeV8Engine();
    _allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    _params.array_buffer_allocator = _allocator;
    _isolate = v8::Isolate::New(_params);

    SC_ASSERT(_isolate->GetNumberOfDataSlots() >= 1);

    // NOTE(rjaber): An isolate is a VM instance with its own heap.
    v8::HandleScope handleScope(_isolate);
    auto ctx = v8::Context::New(_isolate);
    ctx->Enter();
    _context.Reset(_isolate, ctx);

    auto valdiObjectTemplate = v8::ObjectTemplate::New(_isolate);
    valdiObjectTemplate->SetInternalFieldCount(static_cast<int>(InternalFieldSlot::Count));
    _valdiObjectTemplate.Reset(_isolate, valdiObjectTemplate);
    auto propertyString = v8::String::NewFromUtf8Literal(_isolate, "com.snapinc.valdi.js.v8.function.private_prop");
    auto functionPrivate = v8::Private::New(_isolate, propertyString);
    _functionDataProperty.Reset(_isolate, functionPrivate);
}

V8JavaScriptContext::~V8JavaScriptContext() {
    prepareForTeardown();
    v8::HandleScope handleScope(_isolate);
    _context.Reset();
    // TODO(rjaber): PersistentHandleVisitor seems to be deprecated find alternative
    _isolate->Dispose();
    // v8::V8::Dispose();
    // v8::V8::DisposePlatform();
    delete _allocator;
}

JSValueRef V8JavaScriptContext::getGlobalObject(JSExceptionTracker& exceptionTracker) {
    SC_ABORT("getGlobalObject is not implemented");
    return JSValueRef();
}

JSValueRef V8JavaScriptContext::evaluate(const std::string& script,
                                         const std::string_view& sourceFilename,
                                         JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::TryCatch catcher(_isolate);

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    v8::TryCatch tryCatch(_isolate);

    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> source;
    if (!v8::String::NewFromUtf8(
             _isolate, script.c_str(), v8::NewStringType::kNormal, static_cast<int32_t>(script.length()))
             .ToLocal(&source)) {
        exceptionTracker.onError("Failed to convert script into v8 backed utf8 string");
        return JSValueRef();
    }

    v8::Local<v8::Script> compiledSource;
    if (!v8::Script::Compile(context, source).ToLocal(&compiledSource)) {
        if (catcher.HasCaught()) {
            auto excep = catcher.Exception();
            exceptionTracker.storeException(toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, excep)));
        } else {
            exceptionTracker.onError("Failed to compile source");
        }
        return JSValueRef();
    }

    v8::Local<v8::Value> result;
    if (!compiledSource->Run(context).ToLocal(&result)) {
        if (catcher.HasCaught()) {
            auto excep = catcher.Exception();
            exceptionTracker.storeException(toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, excep)));
        } else {
            exceptionTracker.onError("Failed to run compiled source");
        }
        return JSValueRef();
    }

    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, result));
}

JSValueRef V8JavaScriptContext::evaluatePreCompiled(const BytesView& /*bytes*/,
                                                    const std::string_view& /*sourceFilename*/,
                                                    JSExceptionTracker& exceptionTracker) {
    return JSValueRef();
}

BytesView V8JavaScriptContext::preCompile(const std::string_view& /*script*/,
                                          const std::string_view& /*sourceFilename*/,
                                          JSExceptionTracker& exceptionTracker) {
    return BytesView();
}

JSPropertyNameRef V8JavaScriptContext::newPropertyName(const std::string_view& str) {
    v8::HandleScope handleScope(_isolate);

    auto maybeValue =
        v8::String::NewFromUtf8(_isolate, str.data(), v8::NewStringType::kNormal, static_cast<int32_t>(str.length()));
    v8::Local<v8::String> val;
    if (!maybeValue.ToLocal(&val)) {
        return JSPropertyNameRef();
    }

    return JSPropertyNameRef(this, toValdiJSPropertyName(IndirectV8Persistent::make(_isolate, val)), true);
}

StringBox V8JavaScriptContext::propertyNameToString(const JSPropertyName& propertyName) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);
    auto propertyValue = fromValdiJSPropertyName(_isolate, propertyName, tracker);
    if (!tracker) {
        // TODO(rjaber): Figure out the error handling here
        return StringBox();
    }
    v8::String::Utf8Value propertyValueString(_isolate, propertyValue);

    auto stringData = *propertyValueString;
    auto stringLength = propertyValueString.length();

    // TODO(rjaber): Find a way to avoid the copy, or move it to creation ?
    return StringCache::getGlobal().makeString(stringData, stringLength);
}

JSValueRef V8JavaScriptContext::newObject(JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto val = v8::Object::New(_isolate);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val));
}

static void InvokeCallable(const v8::FunctionCallbackInfo<v8::Value>& info) {
    auto isolate = info.GetIsolate();
    v8::HandleScope handleScope(isolate);

    auto context = unsafeBridge<V8JavaScriptContext>(isolate->GetData(static_cast<int>(IsolateDataSlot::ContextSlot)));

    auto data = v8::Local<v8::External>::Cast(info.Data())->Value();
    if (data == nullptr) {
        isolate->ThrowError("Provided function data was null");
        return;
    }

    auto callable = unsafeBridge<JSFunction>(data);
    JSValueRef outArguments[info.Length()];
    for (int i = 0; i < info.Length(); i++) {
        outArguments[i] =
            JSValueRef::makeUnretained(*context, toValdiJSValue(IndirectV8Persistent::make(isolate, info[i])));
    }

    JSExceptionTracker tracker(*context);
    JSFunctionNativeCallContext callContext(
        *context, outArguments, info.Length(), tracker, callable->getReferenceInfo());
    callContext.setThisValue(toValdiJSValue(IndirectV8Persistent::make(isolate, info.This())));

    if (context->interruptRequested()) {
        context->onInterrupt();
    }

    auto result = (*callable)(callContext);

    if (VALDI_LIKELY(tracker)) {
        auto returnValue = fromValdiJSValue(isolate, result.get(), tracker);
        if (VALDI_LIKELY(tracker)) {
            info.GetReturnValue().Set(returnValue);
            return;
        }
    }

    auto exception = fromValdiJSValue(isolate, tracker.getExceptionAndClear().get(), tracker);
    if (VALDI_LIKELY(tracker)) {
        isolate->ThrowException(exception);
    } else {
        isolate->ThrowError("Something went terriblely wrong and can't unpack underlying exception");
    }
}

JSValueRef V8JavaScriptContext::newFunction(const Ref<JSFunction>& callable, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    auto externalData =
        IndirectV8Persistent::makeWeakExternal(_isolate, unsafeBridgeRetain(callable.get()), freeWrappedPointer);
    auto funcTemplate = v8::FunctionTemplate::New(_isolate, InvokeCallable, externalData);

    v8::Local<v8::Function> func;
    funcTemplate->SetClassName(v8::String::NewFromUtf8Literal(_isolate, "ValdiFunctionObject"));

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

    if (!funcTemplate->GetFunction(context).ToLocal(&func)) {
        exceptionTracker.onError("Failed to create function");
        return JSValueRef();
    }

    if (!callable->getReferenceInfo().getName().isEmpty()) {
        auto callableName = callable->getReferenceInfo().getName().toStringView();
        v8::MaybeLocal<v8::String> maybeValue = v8::String::NewFromUtf8(
            _isolate, callableName.data(), v8::NewStringType::kNormal, static_cast<int32_t>(callableName.length()));

        if (maybeValue.IsEmpty()) {
            exceptionTracker.onError("Failed to convert function name to string");
            return JSValueRef();
        }

        func->SetName(maybeValue.ToLocalChecked());
    }

    func->SetPrivate(context, _functionDataProperty.Get(_isolate), externalData);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, func));
}

JSValueRef V8JavaScriptContext::newStringUTF8(const std::string_view& str, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto maybeValue =
        v8::String::NewFromUtf8(_isolate, str.data(), v8::NewStringType::kNormal, static_cast<int32_t>(str.length()));
    v8::Local<v8::String> val;
    if (!maybeValue.ToLocal(&val)) {
        exceptionTracker.onError("Cannot create utf8 string");
    }
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val));
}

JSValueRef V8JavaScriptContext::newStringUTF16(const std::u16string_view& str, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::MaybeLocal<v8::String> maybeValue = v8::String::NewFromTwoByte(_isolate,
                                                                       reinterpret_cast<const uint16_t*>(str.data()),
                                                                       v8::NewStringType::kNormal,
                                                                       static_cast<int32_t>(str.length()));

    if (maybeValue.IsEmpty()) {
        exceptionTracker.onError("Cannot create utf16 string");
    }
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, maybeValue.ToLocalChecked()));
}

JSValueRef V8JavaScriptContext::newArray(size_t initialSize, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto val = v8::Array::New(_isolate, 0);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val));
}

JSValueRef V8JavaScriptContext::newArrayWithValues(const JSValue* values,
                                                   size_t size,
                                                   JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    auto* valuesUnpacked = static_cast<v8::Local<v8::Value>*>(alloca(sizeof(v8::Local<v8::Value>) * size));

    for (size_t i = 0; i < size; i++) {
        auto val = fromValdiJSValue(_isolate, values[i], exceptionTracker);
        if (!exceptionTracker) {
            return JSValueRef();
        }
        valuesUnpacked[i] = val;
    }

    auto val = v8::Array::New(_isolate, valuesUnpacked, size);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val));
}

JSValueRef V8JavaScriptContext::newArrayBuffer(const BytesView& buffer, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::Local<v8::ArrayBuffer> val;
    if (buffer.empty()) {
        // NOTE(rjaber): Should we cache the object like qjs ?
        val = v8::ArrayBuffer::New(_isolate, 0);
    } else {
        // NOTE(rjaber): There has to be an API for creating a shared backing store QQ
        auto backingStore = v8::ArrayBuffer::NewBackingStore(const_cast<Byte*>(buffer.data()),
                                                             buffer.size(),
                                                             &freeArrayBuffer,
                                                             unsafeBridgeRetain(buffer.getSource().get()));
        val = v8::ArrayBuffer::New(_isolate, std::move(backingStore));
    }

    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val));
};

JSValueRef V8JavaScriptContext::newTypedArrayFromArrayBuffer(const TypedArrayType& type,
                                                             const JSValue& arrayBuffer,
                                                             JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    v8::Local<v8::TypedArray> retval;
    auto local = fromValdiJSValue(_isolate, arrayBuffer, exceptionTracker);
    if (!exceptionTracker) {
        return JSValueRef();
    }

    if (!local->IsArrayBuffer() && !local->IsSharedArrayBuffer()) {
        exceptionTracker.onError("Cannot create typed array since provided object is not arraybuffer");
        return JSValueRef();
    }

    auto arrayBufferUnpacked = v8::Local<v8::ArrayBuffer>::Cast(local);
    const auto byteLength = arrayBufferUnpacked->ByteLength();
    switch (type) {
        case Int8Array: {
            retval = v8::Int8Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(int8_t));
        } break;
        case Int16Array: {
            retval = v8::Int16Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(int16_t));
        } break;
        case Int32Array: {
            retval = v8::Int32Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(int32_t));
        } break;
        case Uint8Array: {
            retval = v8::Uint8Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(uint8_t));
        } break;
        case Uint8ClampedArray: {
            retval = v8::Uint8ClampedArray::New(arrayBufferUnpacked, 0, byteLength / sizeof(uint8_t));
        } break;
        case Uint16Array: {
            retval = v8::Uint16Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(uint16_t));
        } break;
        case Uint32Array: {
            retval = v8::Uint32Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(uint32_t));
        } break;
        case Float32Array: {
            retval = v8::Float32Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(float));
        } break;
        case Float64Array: {
            retval = v8::Float64Array::New(arrayBufferUnpacked, 0, byteLength / sizeof(double));
        } break;
        case ArrayBuffer:
            return JSValueRef::makeRetained(*this, arrayBuffer);
    }

    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, retval));
}

JSValueRef V8JavaScriptContext::newWrappedObject(const Ref<RefCountable>& wrappedObject,
                                                 JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

    auto valdiObjectTemplate = _valdiObjectTemplate.Get(_isolate);
    v8::Local<v8::Object> val;
    if (!valdiObjectTemplate->NewInstance(context).ToLocal(&val)) {
        exceptionTracker.onError("Cannot create wrapper object");
        return JSValueRef();
    }

    auto externalData =
        IndirectV8Persistent::makeWeakExternal(_isolate, unsafeBridgeRetain(wrappedObject.get()), freeWrappedPointer);

    val->SetInternalField(static_cast<int>(InternalFieldSlot::DataSlot), externalData);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val));
}

JSValueRef V8JavaScriptContext::newNativeClass(const Ref<RefCountable>& /*classOpaque*/,
                                               const JSClassDefinition& /*classDefinition*/,
                                               JSExceptionTracker& exceptionTracker) {
    exceptionTracker.onError("Native classes are not supported by V8");
    return JSValueRef();
}

JSValueRef V8JavaScriptContext::newObjectFromNativeClass(const Ref<RefCountable>& /*opaque*/,
                                                         const JSValue& /*cls*/,
                                                         JSExceptionTracker& exceptionTracker) {
    exceptionTracker.onError("Native classes are not supported by V8");
    return JSValueRef();
}

JSValueRef V8JavaScriptContext::newWeakRef(const JSValue& object, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto indirect = fromValdiJSValueToIndirect(object);
    return toUnretainedJSValueRef(indirect);
}

JSValueRef V8JavaScriptContext::derefWeakRef(const JSValue& weakRef, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto indirect = fromValdiJSValueToIndirect(weakRef);
    if (indirect.isEmpty()) {
        exceptionTracker.onError("Object cannot be dereferenced");
        return JSValueRef();
    }
    return toRetainedJSValueRef(indirect);
}

JSValueRef V8JavaScriptContext::getObjectProperty(const JSValue& object,
                                                  const std::string_view& propertyName,
                                                  JSExceptionTracker& exceptionTracker) {
    auto propertyKey = newPropertyName(propertyName);
    return getObjectProperty(object, propertyKey.get(), exceptionTracker);
}

JSValueRef V8JavaScriptContext::getObjectProperty(const JSValue& object,
                                                  const JSPropertyName& propertyName,
                                                  JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, exceptionTracker));
    if (!exceptionTracker) {
        return JSValueRef();
    }

    if (!obj->IsObject()) {
        exceptionTracker.onError("Cannot getObject property on non object value");
        return JSValueRef();
    }

    auto key = fromValdiJSPropertyName(_isolate, propertyName, exceptionTracker);
    if (!exceptionTracker) {
        return JSValueRef();
    }
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

    auto maybeVal = obj->Get(context, key);
    v8::Local<v8::Value> retval;
    if (!maybeVal.ToLocal(&retval)) {
        return newUndefined();
    }
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, retval));
}

JSValueRef V8JavaScriptContext::getObjectPropertyForIndex(const JSValue& object,
                                                          size_t index,
                                                          JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto local = fromValdiJSValue(_isolate, object, exceptionTracker);
    if (!exceptionTracker) {
        return JSValueRef();
    }

    if (!local->IsObject()) {
        exceptionTracker.onError("Cannot getObject property on non object value");
        return JSValueRef();
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    auto val = v8::Local<v8::Object>::Cast(local);

    v8::Local<v8::Value> retval;
    if (!val->Get(context, static_cast<uint32_t>(index)).ToLocal(&retval)) {
        exceptionTracker.onError("Failed to fetch object at requested index");
        return JSValueRef();
    }

    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, retval));
}

bool V8JavaScriptContext::hasObjectProperty(const JSValue& object, const JSPropertyName& propertyName) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);

    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, tracker));
    if (!tracker) {
        tracker.clearError();
        return false;
    }

    if (!obj->IsObject()) {
        return false;
    }

    auto key = fromValdiJSPropertyName(_isolate, propertyName, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    return obj->Has(context, key).FromMaybe(false);
}

void V8JavaScriptContext::setObjectProperty(const JSValue& object,
                                            const std::string_view& propertyName,
                                            const JSValue& propertyValue,
                                            bool enumerable,
                                            JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }
    auto val = fromValdiJSValue(_isolate, propertyValue, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    v8::Local<v8::String> key;
    if (!v8::String::NewFromUtf8(
             _isolate, propertyName.data(), v8::NewStringType::kNormal, static_cast<uint32_t>(propertyName.length()))
             .ToLocal(&key)) {
        exceptionTracker.onError("Failed to create a string key object");
        return;
    }
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    setObjectPropertyHelper(context, obj, key, val, enumerable, exceptionTracker);
}

void V8JavaScriptContext::setObjectProperty(const JSValue& object,
                                            const JSPropertyName& propertyName,
                                            const JSValue& propertyValue,
                                            bool enumerable,
                                            JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }

    if (!obj->IsObject()) {
        exceptionTracker.onError("Cannot set property on non object");
        return;
    }
    auto key = fromValdiJSPropertyName(_isolate, propertyName, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    auto val = fromValdiJSValue(_isolate, propertyValue, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    setObjectPropertyHelper(context, obj, key, val, enumerable, exceptionTracker);
}

void V8JavaScriptContext::setObjectProperty(const JSValue& object,
                                            const JSValue& propertyName,
                                            const JSValue& propertyValue,
                                            bool enumerable,
                                            JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }
    if (!obj->IsObject()) {
        exceptionTracker.onError("Cannot set property on non object");
        return;
    }
    auto key = fromValdiJSValue(_isolate, propertyName, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    auto val = fromValdiJSValue(_isolate, object, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    if (key->IsName()) {
        v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
        auto keyName = v8::Local<v8::Name>::Cast(key);
        setObjectPropertyHelper(context, obj, keyName, val, enumerable, exceptionTracker);
    } else if (key->IsInt32()) {
        v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
        auto index = v8::Local<v8::Int32>::Cast(key)->Value();
        setObjectPropertyIndexHelper(context, obj, index, val, enumerable, exceptionTracker);
    } else {
        exceptionTracker.onError("Failed to set key object because it does not belong to a supported type");
    }
}

void V8JavaScriptContext::setObjectPropertyIndex(const JSValue& object,
                                                 size_t index,
                                                 const JSValue& value,
                                                 JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }
    if (!obj->IsObject()) {
        exceptionTracker.onError("Cannot setObject property on non object value");
        return;
    }

    auto val = fromValdiJSValue(_isolate, value, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    setObjectPropertyIndexHelper(context, obj, static_cast<uint32_t>(index), val, true, exceptionTracker);
}

JSValueRef V8JavaScriptContext::callObjectAsFunction(const JSValue& object, JSFunctionCallContext& callContext) {
    SC_ABORT("callObjectAsFunction is not implemented");
    return JSValueRef();
}

JSValueRef V8JavaScriptContext::callObjectAsConstructor(const JSValue& object, JSFunctionCallContext& callContext) {
    SC_ABORT("callObjectAsConstructor is not implemented");
    return JSValueRef();
}

void V8JavaScriptContext::visitObjectPropertyNames(const JSValue& object,
                                                   JSExceptionTracker& exceptionTracker,
                                                   IJavaScriptPropertyNamesVisitor& visitor) {
    v8::HandleScope handleScope(_isolate);
    auto obj = v8::Local<v8::Object>::Cast(fromValdiJSValue(_isolate, object, exceptionTracker));
    if (!exceptionTracker) {
        return;
    }
    if (!obj->IsObject()) {
        exceptionTracker.onError("Cannot invoke visitObjectPropertyNames on a non object");
        return;
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    v8::Local<v8::Array> properties;
    if (!obj->GetPropertyNames(context).ToLocal(&properties)) {
        exceptionTracker.onError("Cannot get property list from object");
        return;
    }

    bool shouldContinue = true;
    for (uint32_t i = 0; i < properties->Length() && shouldContinue && exceptionTracker; i++) {
        v8::Local<v8::Value> ival;
        if (!properties->Get(context, i).ToLocal(&ival)) {
            // NOTE(rjaber): Should never happen
            assert(false);
        }
        auto propertyName = toValdiJSPropertyName(IndirectV8Persistent::make(_isolate, ival));
        shouldContinue = visitor.visitPropertyName(*this, object, propertyName, exceptionTracker);
    }
}

StringBox V8JavaScriptContext::valueToString(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::Local<v8::Value> val = fromValdiJSValue(_isolate, value, exceptionTracker);
    if (!exceptionTracker) {
        return StringBox();
    }
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    v8::Local<v8::String> string;
    if (!val->ToString(context).ToLocal(&string)) {
        exceptionTracker.onError("Failed to convert value to string");
        return StringBox();
    }

    // TODO: we probably should use WriteUtf8 method instead
    v8::String::Utf8Value stringValue(_isolate, string);

    auto stringData = *stringValue;
    auto stringLength = stringValue.length();

    return StringCache::getGlobal().makeString(stringData, stringLength);
}

Ref<StaticString> V8JavaScriptContext::valueToStaticString(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::Local<v8::Value> val = fromValdiJSValue(_isolate, value, exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    v8::Local<v8::String> string;
    if (!val->ToString(context).ToLocal(&string)) {
        exceptionTracker.onError("Failed to convert value to static string");
        return nullptr;
    }

    const int length = string->Utf8Length(_isolate);
    Ref<StaticString> str = StaticString::makeUTF8(length);
    string->WriteUtf8(_isolate, str->utf8Data(), length);

    return str;
}

bool V8JavaScriptContext::valueToBool(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    auto val_ = fromValdiJSValue(_isolate, value, exceptionTracker);
    if (!exceptionTracker) {
        return false;
    }

    v8::Local<v8::Value> val;

    if (val_->IsBooleanObject()) {
        val = val_->ToBoolean(_isolate);
    } else {
        val = val_;
    }

    return val->BooleanValue(_isolate);
}

double V8JavaScriptContext::valueToDouble(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);

    auto val_ = fromValdiJSValue(_isolate, value, exceptionTracker);
    if (!exceptionTracker) {
        return 0;
    }
    v8::Local<v8::Value> val;

    if (val_->IsNumberObject()) {
        if (!val_->ToNumber(context).ToLocal(&val)) {
            exceptionTracker.onError("Failed to convert NumberObject to Number");
            return 0;
        }
    } else {
        val = val_;
    }

    if (!val->IsNumber()) {
        exceptionTracker.onError("Value is not a number type and cannot be converted");
        return 0;
    }

    double retval;
    if (!val->NumberValue(context).To(&retval)) {
        exceptionTracker.onError("Value could not be converted");
        return 0;
    }
    return retval;
}

int32_t V8JavaScriptContext::valueToInt(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    auto val = v8::Local<v8::Int32>::Cast(fromValdiJSValue(_isolate, value, exceptionTracker));
    if (!exceptionTracker) {
        return 0;
    }

    if (!val->IsInt32()) {
        exceptionTracker.onError("Value is not an int32 type and cannot be converted");
        return 0;
    }

    return val->Value();
}

Ref<RefCountable> V8JavaScriptContext::valueToWrappedObject(const JSValue& value,
                                                            JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);
    auto indirect = fromValdiJSValueToIndirect(value);
    auto objectInfo = indirect.getWithWrapperID(_isolate);
    auto val = v8::Local<v8::Object>::Cast(objectInfo.first);

    if (indirect.isEmpty()) {
        exceptionTracker.onError("Provided js value is already been cleared");
        return Ref<RefCountable>();
    } else if (!val->IsObject()) {
        exceptionTracker.onError("Provided js value is not object");
        return Ref<RefCountable>();
    } else if (objectInfo.second != kWrappedObjectID) {
        exceptionTracker.onError("Provided js value is not a wrapped object");
        return Ref<RefCountable>();
    }

    SC_ASSERT(val->InternalFieldCount() == static_cast<int>(InternalFieldSlot::Count));
    auto external = v8::Local<v8::External>::Cast(val->GetInternalField(static_cast<int>(InternalFieldSlot::DataSlot)));
    if (!external->IsExternal()) {
        exceptionTracker.onError("Internal field value is not of type external");
    }

    return unsafeBridge<RefCountable>(external->Value());
}

JSTypedArray V8JavaScriptContext::valueToTypedArray(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    v8::Local<v8::Value> val = fromValdiJSValue(_isolate, value, exceptionTracker);
    if (!val->IsTypedArray()) {
        exceptionTracker.onError("Value is not TypedArray");
        return JSTypedArray();
    }
    v8::Local<v8::TypedArray> arr = v8::Local<v8::TypedArray>::Cast(val);
    const auto type = getTypedArrayType(val);
    void* data = arr->Buffer()->Data();
    const size_t length = arr->ByteLength();
    return JSTypedArray(type, data, length, toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, val)));
}

Ref<JSFunction> V8JavaScriptContext::valueToFunction(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    v8::HandleScope handleScope(_isolate);

    auto val = v8::Local<v8::Function>::Cast(fromValdiJSValue(_isolate, value, exceptionTracker));
    if (!val->IsFunction()) {
        exceptionTracker.onError("Was not provided with a function value");
        return nullptr;
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    v8::Local<v8::Private> privateProp = _functionDataProperty.Get(_isolate);
    if (val->HasPrivate(context, privateProp).IsNothing()) {
        exceptionTracker.onError("Provided function was not wrapped by the v8 js context");
        return nullptr;
    }

    auto data = v8::Local<v8::External>::Cast(val->GetPrivate(context, privateProp).ToLocalChecked());
    if (!data->IsExternal()) {
        exceptionTracker.onError("Provided function did not contain data of the expected type");
        return nullptr;
    }

    return unsafeBridge<JSFunction>(data->Value());
}

ValueType V8JavaScriptContext::getValueType(const JSValue& value) {
    v8::HandleScope handleScope(_isolate);

    auto indirect = fromValdiJSValueToIndirect(value);
    auto objectInfo = indirect.getWithWrapperID(_isolate);

    auto val = objectInfo.first;
    auto classID = objectInfo.second;

    if (val.IsEmpty()) {
        return ValueType::Undefined;
    }

    if (val->IsUndefined()) {
        return ValueType::Undefined;
    }
    if (val->IsNull()) {
        return ValueType::Null;
    }
    if (val->IsString() || val->IsStringObject()) {
        return ValueType::StaticString;
    }
    if (val->IsNumber() || val->IsNumberObject()) {
        return ValueType::Double;
    }
    if (val->IsBoolean() || val->IsBoolean()) {
        return ValueType::Bool;
    }
    if (val->IsArray()) {
        return ValueType::Array;
    }
    if (val->IsObject() && classID == kWrappedObjectID) {
        return ValueType::ValdiObject;
    }
    if (val->IsFunction()) {
        return ValueType::Function;
    }
    if (val->IsTypedArray() || val->IsArrayBuffer() || val->IsArrayBufferView()) {
        return ValueType::TypedArray;
    }
    if (val->IsNativeError()) { // TODO(rjaber): Check that native error is semantically the as other VMs
        return ValueType::Error;
    }
    if (val->IsSymbol() || val->IsSymbolObject() || val->IsAsyncFunction()) {
        // Symbols are opaque types that we don't support exporting
        return ValueType::Undefined;
    }
    // IMPL(rjaber): Implement a wrapper valdi object
    //  if () {
    //      return ValueType::Long;
    //  }

    return ValueType::Map;
}

bool V8JavaScriptContext::isValueUndefined(const JSValue& value) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);

    auto val = fromValdiJSValue(_isolate, value, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }
    return val->IsUndefined();
}
bool V8JavaScriptContext::isValueNull(const JSValue& value) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);

    auto val = fromValdiJSValue(_isolate, value, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }
    return val->IsNull();
}
bool V8JavaScriptContext::isValueFunction(const JSValue& value) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);

    auto val = fromValdiJSValue(_isolate, value, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }
    return val->IsFunction();
}

bool V8JavaScriptContext::isValueNumber(const JSValue& value) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);

    auto val = fromValdiJSValue(_isolate, value, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }
    return val->IsNumber() || val->IsNumberObject();
}

bool V8JavaScriptContext::isValueLong(const JSValue& value) {
    return false;
}

bool V8JavaScriptContext::isValueEqual(const JSValue& left, const JSValue& right) {
    JSExceptionTracker tracker(*this);
    auto leftVal = fromValdiJSValue(_isolate, left, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }
    auto rightVal = fromValdiJSValue(_isolate, right, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }

    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(_isolate, _context);
    return leftVal->Equals(context, rightVal).FromMaybe(false);
}

bool V8JavaScriptContext::isValueObject(const JSValue& value) {
    v8::HandleScope handleScope(_isolate);
    JSExceptionTracker tracker(*this);

    auto val = fromValdiJSValue(_isolate, value, tracker);
    if (!tracker) {
        tracker.clearError();
        return false;
    }
    return val->IsObject();
}

void V8JavaScriptContext::retainValue(const JSValue& value) {
    fromValdiJSValueToIndirect(value).retain();
}

void V8JavaScriptContext::releaseValue(const JSValue& value) {
    fromValdiJSValueToIndirect(value).release();
}

void V8JavaScriptContext::retainPropertyName(const JSPropertyName& value) {
    fromValdiJSPropertyNameToIndirect(value).retain();
}

void V8JavaScriptContext::releasePropertyName(const JSPropertyName& value) {
    fromValdiJSPropertyNameToIndirect(value).release();
}

void V8JavaScriptContext::garbageCollect() {
    _isolate->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical);
}

JavaScriptContextMemoryStatistics V8JavaScriptContext::dumpMemoryStatistics() {
    JavaScriptContextMemoryStatistics retval;

    v8::HeapStatistics stats;
    _isolate->GetHeapStatistics(&stats);
    retval.memoryUsageBytes = stats.malloced_memory();

    for (size_t i = 0; i < _isolate->NumberOfTrackedHeapObjectTypes(); i++) {
        v8::HeapObjectStatistics statc;
        _isolate->GetHeapObjectStatisticsAtLastGC(&statc, i);
        retval.objectsCount += statc.object_count();
    }

    return retval;
}

JSValueRef V8JavaScriptContext::onNewBool(bool boolean) {
    v8::HandleScope handleScope(_isolate);
    auto result = v8::Boolean::New(_isolate, boolean);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, result));
}

JSValueRef V8JavaScriptContext::onNewNumber(int32_t number) {
    v8::HandleScope handleScope(_isolate);
    auto result = v8::Int32::New(_isolate, number);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, result));
}

JSValueRef V8JavaScriptContext::onNewNumber(double number) {
    v8::HandleScope handleScope(_isolate);
    auto result = v8::Number::New(_isolate, number);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, result));
}

JSValueRef V8JavaScriptContext::onNewNull() {
    v8::HandleScope handleScope(_isolate);
    auto result = v8::Null(_isolate);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, result));
}

JSValueRef V8JavaScriptContext::onNewUndefined() {
    v8::HandleScope handleScope(_isolate);
    auto result = v8::Undefined(_isolate);
    return toRetainedJSValueRef(IndirectV8Persistent::make(_isolate, result));
}

void V8JavaScriptContext::onInitialize(JSExceptionTracker& exceptionTracker) {
    _isolate->SetData(static_cast<int>(IsolateDataSlot::ContextSlot), unsafeBridgeCast(this));
}

void V8JavaScriptContext::enqueueMicrotask(const JSValue& value, JSExceptionTracker& exceptionTracker) {
    // IMPL(rjaber): Needs to be done
}

void V8JavaScriptContext::willEnterVM() {
    // IMPL(rjaber): Needs to be done
}

void V8JavaScriptContext::willExitVM(JSExceptionTracker& exceptionTracker) {
    // IMPL(rjaber): Needs to be done
}

} // namespace Valdi::V8
