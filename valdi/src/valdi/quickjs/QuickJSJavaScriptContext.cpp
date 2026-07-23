//
//  QuickJSJavaScriptContext.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/17/19.
//

#include "valdi/quickjs/QuickJSJavaScriptContext.hpp"

#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptHeapDumpBuilder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Threading/Thread.hpp"

#include "valdi/quickjs/QuickJSUtils.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/Defer.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "tsn/tsn.h"

namespace ValdiQuickJS {

static Valdi::TypedArrayType toValdiTypedArrayType(JSTypedArrayType type) {
    switch (type) {
        case JS_TYPED_ARRAY_NONE:
            return Valdi::TypedArrayType::ArrayBuffer;
        case JS_TYPED_ARRAY_UINT8C:
            return Valdi::TypedArrayType::Uint8ClampedArray;
        case JS_TYPED_ARRAY_INT8:
            return Valdi::TypedArrayType::Int8Array;
        case JS_TYPED_ARRAY_UINT8:
            return Valdi::TypedArrayType::Uint8Array;
        case JS_TYPED_ARRAY_INT16:
            return Valdi::TypedArrayType::Int16Array;
        case JS_TYPED_ARRAY_UINT16:
            return Valdi::TypedArrayType::Uint16Array;
        case JS_TYPED_ARRAY_INT32:
            return Valdi::TypedArrayType::Int32Array;
        case JS_TYPED_ARRAY_UINT32:
            return Valdi::TypedArrayType::Uint32Array;
        case JS_TYPED_ARRAY_BIG_INT64:
            return Valdi::TypedArrayType::ArrayBuffer;
        case JS_TYPED_ARRAY_BIG_UINT64:
            return Valdi::TypedArrayType::ArrayBuffer;
        case JS_TYPED_ARRAY_FLOAT32:
            return Valdi::TypedArrayType::Float32Array;
        case JS_TYPED_ARRAY_FLOAT64:
            return Valdi::TypedArrayType::Float64Array;
    }
}

void freeArrayBuffer(JSRuntime* /*rt*/, void* opaque, void* /*ptr*/) {
    Valdi::RefCountableAutoreleasePool::release(opaque);
}

void freeByteBuffer(JSRuntime* rt, void* opaque, void* /*ptr*/) {
    auto* byteBuffer = dynamic_cast<Valdi::ByteBuffer*>(reinterpret_cast<Valdi::RefCountable*>(opaque));

    JS_DecreaseExternallyAllocatedMemory(rt, byteBuffer->capacity());
    Valdi::unsafeBridgeRelease(opaque);
}

static void handleRejectedPromise(
    JSContext* ctx, JSValueConst promise, JSValueConst reason, JS_BOOL is_handled, void* opaque) {
    auto& jsContext = *reinterpret_cast<QuickJSJavaScriptContext*>(opaque);

    if (is_handled == 0) {
        jsContext.appendRejectedPromise(promise, reason);
    } else {
        jsContext.removeRejectedPromise(promise);
    }
}

constexpr double kMaxStackSizeRatio = 0.75;

static int handleInterrupt(JSRuntime* rt, void* opaque) {
    auto& jsContext = *reinterpret_cast<QuickJSJavaScriptContext*>(opaque);
    if (jsContext.interruptRequested()) {
        return jsContext.onInterrupt() ? 1 : 0;
    }
    return 0;
}

QuickJSJavaScriptContext::QuickJSJavaScriptContext(Valdi::JavaScriptTaskScheduler* taskScheduler)
    : Valdi::IJavaScriptContext(taskScheduler) {
    _runtime = JS_NewRuntime();
    JS_SetPropertyCacheEnabledRT(_runtime, 2); // 2 = enabled for both get and set
    _context = JS_NewContext(_runtime);
    tsn_load_in_context(_context);
    attachValdiJSContext(_context, this);
    JS_SetHostPromiseRejectionTracker(_runtime, &handleRejectedPromise, this);

    JS_SetInterruptHandler(_runtime, &handleInterrupt, this);
}

QuickJSJavaScriptContext::~QuickJSJavaScriptContext() {
    auto guard = _threadAccessChecker.guard();
    setListener(nullptr);
    notifyRejectedPromises();
    prepareForTeardown();

    JS_FreeValue(_context, _functionPrototype);
    JS_FreeValue(_context, _objectPrototype);
    JS_FreeValue(_context, _emptyArrayBuffer);
    JS_FreeValue(_context, _int8ArrayCtor);
    JS_FreeValue(_context, _int16ArrayCtor);
    JS_FreeValue(_context, _int32ArrayCtor);
    JS_FreeValue(_context, _uint8ArrayCtor);
    JS_FreeValue(_context, _uint8ClampedArrayCtor);
    JS_FreeValue(_context, _uint16ArrayCtor);
    JS_FreeValue(_context, _uint32ArrayCtor);
    JS_FreeValue(_context, _float32ArrayCtor);
    JS_FreeValue(_context, _float64ArrayCtor);
    JS_FreeAtom(_context, _objectFinalizerAtomKey);

    tsn_unload_in_context(_context);
    JS_FreeContext(_context);

    _needsGarbageCollect = true;
    while (_needsGarbageCollect) {
        _needsGarbageCollect = false;
        JS_RunGC(_runtime);
    }

    JS_FreeRuntime(_runtime);
}

void QuickJSJavaScriptContext::onInitialize(Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto globalObject = getGlobalObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto functionValue = getObjectProperty(globalObject.get(), "Function", exceptionTracker);

    if (!exceptionTracker) {
        return;
    }

    _functionPrototype = JS_GetPrototype(_context, fromValdiJSValue(functionValue.get()));

    _int8ArrayCtor = getConstructorFromObject(globalObject.get(), "Int8Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _int16ArrayCtor = getConstructorFromObject(globalObject.get(), "Int16Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _int32ArrayCtor = getConstructorFromObject(globalObject.get(), "Int32Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint8ArrayCtor = getConstructorFromObject(globalObject.get(), "Uint8Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint8ClampedArrayCtor = getConstructorFromObject(globalObject.get(), "Uint8ClampedArray", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint16ArrayCtor = getConstructorFromObject(globalObject.get(), "Uint16Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _uint32ArrayCtor = getConstructorFromObject(globalObject.get(), "Uint32Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _float32ArrayCtor = getConstructorFromObject(globalObject.get(), "Float32Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    _float64ArrayCtor = getConstructorFromObject(globalObject.get(), "Float64Array", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _functionClassID = initializeClass(getBridgedFunctionClassDef(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _wrappedObjectClassID = initializeClass(getWrappedObjectClassDef(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _weakReferenceFinalizerClassID = initializeClass(getWeakRefFinalizerClassDef(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _nativeClassConstructorClassID = initializeClass(getNativeClassConstructorClassDef(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    initializeClass(getNativeClassInstanceMemberClassDef(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    initializeClass(getNativeClassStaticMemberClassDef(), exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    /**
     We capture the Object.prototype property to optimize the visitObjectPropertyNames
     implementation such that we stop the loop through the prototype chain until
     we reach the Object prototype.
     */
    auto objectValue = getObjectProperty(globalObject.get(), "Object", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto objectPrototype = getObjectProperty(objectValue.get(), "prototype", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _objectPrototype = JS_DupValue(_context, fromValdiJSValue(objectPrototype.get()));

    auto emptyArrayBuffer = checkCallAndGetValue(exceptionTracker, JS_NewArrayBufferCopy(_context, nullptr, 0));
    if (!exceptionTracker) {
        return;
    }

    _emptyArrayBuffer = fromValdiJSValue(emptyArrayBuffer.get());
    emptyArrayBuffer.unsafeReleaseValue();

    auto symbolValue = getObjectProperty(globalObject.get(), "Symbol", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    Valdi::JSFunctionCallContext callContext(*this, nullptr, 0, exceptionTracker);
    auto objectFinalizerKey = callObjectAsFunction(symbolValue.get(), callContext);

    if (!exceptionTracker) {
        return;
    }

    _objectFinalizerAtomKey = JS_ValueToAtom(_context, fromValdiJSValue(objectFinalizerKey.get()));
    if (_objectFinalizerAtomKey == JS_ATOM_NULL) {
        setExceptionToTracker(exceptionTracker);
        return;
    }
}

void QuickJSJavaScriptContext::setExceptionToTracker(Valdi::JSExceptionTracker& exceptionTracker) {
    auto exceptionObject = JS_GetException(_context);
    exceptionTracker.storeException(toRetainedJSValueRef(exceptionObject));
}

Valdi::JSValueRef QuickJSJavaScriptContext::checkCallAndGetValue(Valdi::JSExceptionTracker& exceptionTracker,
                                                                 const JSValue& value) {
    if (JS_IsException(value) != 0) {
        setExceptionToTracker(exceptionTracker);
        return Valdi::JSValueRef();
    }

    return toRetainedJSValueRef(value);
}

bool QuickJSJavaScriptContext::checkCall(Valdi::JSExceptionTracker& exceptionTracker, int retValue) {
    if (retValue < 0) {
        setExceptionToTracker(exceptionTracker);
        return false;
    }
    return true;
}

Valdi::JSValueRef QuickJSJavaScriptContext::getGlobalObject(Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker, JS_GetGlobalObject(_context));
}

bool QuickJSJavaScriptContext::supportsPreCompilation() const {
    return true;
}

Valdi::BytesView QuickJSJavaScriptContext::preCompile(const std::string_view& script,
                                                      const std::string_view& sourceFilename,
                                                      Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto formattedScript = Valdi::formatJsModule(script);

    auto evalResult = checkCallAndGetValue(exceptionTracker,
                                           JS_Eval(_context,
                                                   reinterpret_cast<const char*>(formattedScript.data()),
                                                   formattedScript.length(),
                                                   sourceFilename.data(),
                                                   JS_EVAL_FLAG_COMPILE_ONLY));
    if (!exceptionTracker) {
        return Valdi::BytesView();
    }

    size_t objectSize = 0;

    auto* buffer = JS_WriteObject(_context, &objectSize, fromValdiJSValue(evalResult.get()), JS_WRITE_OBJ_BYTECODE);
    if (buffer == nullptr) {
        checkCallAndGetValue(exceptionTracker, JS_ThrowInternalError(_context, "Could not write object"));
        return Valdi::BytesView();
    }

    auto jsModule = Valdi::makePreCompiledJsModule(reinterpret_cast<const Valdi::Byte*>(buffer), objectSize);

    js_free(_context, buffer);

    return jsModule;
}

Valdi::JSValueRef QuickJSJavaScriptContext::evaluatePreCompiled(const Valdi::BytesView& script,
                                                                const std::string_view& /*sourceFilename*/,
                                                                Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto result = checkCallAndGetValue(
        exceptionTracker,
        JS_ReadObject(_context, reinterpret_cast<const uint8_t*>(script.data()), script.size(), JS_READ_OBJ_BYTECODE));

    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    auto retainedResult = JS_DupValue(_context, fromValdiJSValue(result.get()));

    return checkCallAndGetValue(exceptionTracker, JS_EvalFunction(_context, retainedResult));
}

Valdi::JSValueRef QuickJSJavaScriptContext::evaluate(const std::string& script,
                                                     const std::string_view& sourceFilename,
                                                     Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(
        exceptionTracker,
        JS_Eval(_context, reinterpret_cast<const char*>(script.data()), script.size(), sourceFilename.data(), 0));
}

Valdi::JSValueRef QuickJSJavaScriptContext::evaluateNative(const std::string_view& sourceFilename,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker, tsn_load_module(_context, sourceFilename.data()));
}

std::optional<Valdi::IJavaScriptNativeModuleInfo> QuickJSJavaScriptContext::getNativeModuleInfo(
    const std::string_view& sourceFilename) {
    tsn_module_info moduleInfo;
    if (!tsn_get_module_info(sourceFilename.data(), &moduleInfo)) {
        return std::nullopt;
    }

    Valdi::IJavaScriptNativeModuleInfo outputInfo;
    outputInfo.sha256 = Valdi::StringCache::getGlobal().makeString(std::string_view(moduleInfo.sha256));
    outputInfo.name = Valdi::StringCache::getGlobal().makeString(std::string_view(moduleInfo.name));

    return outputInfo;
}

Valdi::JSPropertyNameRef QuickJSJavaScriptContext::newPropertyName(const std::string_view& str) {
    auto guard = _threadAccessChecker.guard();
    JSAtom atom = JS_NewAtomLen(_context, str.data(), str.size());
    return Valdi::JSPropertyNameRef(this, toValdiJSPropertyName(atom), true);
}

Valdi::StringBox QuickJSJavaScriptContext::propertyNameToString(const Valdi::JSPropertyName& propertyName) {
    auto guard = _threadAccessChecker.guard();
    const auto* cString = JS_AtomToCString(_context, fromValdiJSPropertyName(propertyName));
    if (cString == nullptr) {
        return Valdi::StringBox();
    }

    auto convertedString = Valdi::StringCache::makeStringFromCLiteral(cString);

    JS_FreeCString(_context, cString);

    return convertedString;
}

Valdi::JSValueRef QuickJSJavaScriptContext::newObject(Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker, JS_NewObject(_context));
}

Valdi::JSValueRef QuickJSJavaScriptContext::newFunction(const Valdi::Ref<Valdi::JSFunction>& callable,
                                                        Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    // TODO(simon): Use functionName

    auto functionObject =
        checkCallAndGetValue(exceptionTracker, JS_NewObjectProtoClass(_context, _functionPrototype, _functionClassID));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    setObjectCallable(fromValdiJSValue(functionObject.get()), callable);

    if (!callable->getReferenceInfo().getName().isEmpty()) {
        auto value = newStringUTF8(callable->getReferenceInfo().getName().toStringView(), exceptionTracker);
        if (!exceptionTracker) {
            return Valdi::JSValueRef();
        }

        // JS_DefinePropertyValueStr() automatically releases the given value.
        auto retainedName = JS_DupValue(_context, fromValdiJSValue(value.get()));

        checkCall(exceptionTracker,
                  JS_DefinePropertyValueStr(_context, fromValdiJSValue(functionObject.get()), "name", retainedName, 0));

        if (!exceptionTracker) {
            return Valdi::JSValueRef();
        }
    }

    return functionObject;
}

size_t QuickJSJavaScriptContext::associateWeakReference(const JSValue& value,
                                                        Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto existingFinalizer =
        checkCallAndGetValue(exceptionTracker, JS_GetProperty(_context, value, _objectFinalizerAtomKey));
    if (!exceptionTracker) {
        return 0;
    }

    auto weakReferenceId = weakReferenceIdFromJSWeakReferenceFinalizer(fromValdiJSValue(existingFinalizer.get()));
    if (weakReferenceId != 0) {
        return weakReferenceId;
    }

    // Create the finalizer
    auto finalizerObject = checkCallAndGetValue(
        exceptionTracker, JS_NewObjectProtoClass(_context, _objectPrototype, _weakReferenceFinalizerClassID));
    if (!exceptionTracker) {
        return 0;
    }

    weakReferenceId = ++_weakReferenceSequence;

    setWeakReferenceIdToJSWeakReferenceFinalizer(fromValdiJSValue(finalizerObject.get()), weakReferenceId);
    _weakReferences[weakReferenceId] = value;

    if (!checkCall(
            exceptionTracker,
            JS_SetProperty(
                _context, value, _objectFinalizerAtomKey, fromValdiJSValue(finalizerObject.unsafeReleaseValue())))) {
        return 0;
    }

    return weakReferenceId;
}

Valdi::JSValueRef QuickJSJavaScriptContext::newWeakRef(const Valdi::JSValue& object,
                                                       Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    // Insert an an object finalizer detector on the given object, so that can know when the object is
    // about to be GC'd
    auto jsObject = fromValdiJSValue(object);
    auto weakReferenceId = associateWeakReference(jsObject, exceptionTracker);
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    return toUnretainedJSValueRef(JS_NewInt64(_context, static_cast<int64_t>(weakReferenceId)));
}

void QuickJSJavaScriptContext::removeWeakReference(size_t weakReferenceId) {
    auto guard = _threadAccessChecker.guard();
    const auto& it = _weakReferences.find(weakReferenceId);
    if (it != _weakReferences.end()) {
        _weakReferences.erase(it);
    }
}

Valdi::JSValueRef QuickJSJavaScriptContext::derefWeakRef(const Valdi::JSValue& weakRef,
                                                         Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    int64_t number = 0;

    if (!checkCall(exceptionTracker, JS_ToInt64(_context, &number, fromValdiJSValue(weakRef)))) {
        return Valdi::JSValueRef();
    }

    const auto& it = _weakReferences.find(static_cast<size_t>(number));
    if (it == _weakReferences.end()) {
        return newUndefined();
    }

    return toRetainedJSValueRef(JS_DupValue(_context, it->second));
}

Valdi::JSValueRef QuickJSJavaScriptContext::onNewNull() {
    return toUnretainedJSValueRef(JS_NULL);
}

Valdi::JSValueRef QuickJSJavaScriptContext::onNewUndefined() {
    return toUnretainedJSValueRef(JS_UNDEFINED);
}

Valdi::JSValueRef QuickJSJavaScriptContext::onNewBool(bool boolean) {
    if (boolean) {
        return toUnretainedJSValueRef(JS_TRUE);
    } else {
        return toUnretainedJSValueRef(JS_FALSE);
    }
}

Valdi::JSValueRef QuickJSJavaScriptContext::onNewNumber(int32_t number) {
    return toUnretainedJSValueRef(JS_NewInt32(_context, number));
}

Valdi::JSValueRef QuickJSJavaScriptContext::onNewNumber(double number) {
    return toUnretainedJSValueRef(JS_NewFloat64(_context, number));
}

Valdi::JSValueRef QuickJSJavaScriptContext::newStringUTF8(const std::string_view& str,
                                                          Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker, JS_NewStringLen(_context, str.data(), str.length()));
}

Valdi::JSValueRef QuickJSJavaScriptContext::newStringUTF16(const std::u16string_view& str,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(
        exceptionTracker, JS_NewUTF16StringLen(_context, reinterpret_cast<const uint16_t*>(str.data()), str.size()));
}

Valdi::JSValueRef QuickJSJavaScriptContext::newArray(size_t /*initialSize*/,
                                                     Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker, JS_NewArray(_context));
}

Valdi::JSValueRef QuickJSJavaScriptContext::newTypedArrayFromConstructor(const JSValue& ctor,
                                                                         const Valdi::JSValue& arrayBuffer,
                                                                         Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    JSValue argv = fromValdiJSValue(arrayBuffer);

    return checkCallAndGetValue(exceptionTracker, JS_CallConstructor(_context, ctor, 1, &argv));
}

Valdi::JSValueRef QuickJSJavaScriptContext::newArrayBuffer(const Valdi::BytesView& buffer,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    if (buffer.empty()) {
        return toRetainedJSValueRef(JS_DupValue(_context, _emptyArrayBuffer));
    } else {
        JSFreeArrayBufferDataFunc* freeFunction;
        auto* byteBuffer = dynamic_cast<Valdi::ByteBuffer*>(buffer.getSource().get());

        // For byte buffers that we are transferring to JS, we notify the JS engine about the memory
        // usage from them, to increase the likely of a future GC pass.
        if (byteBuffer != nullptr && byteBuffer->retainCount() == 1) {
            freeFunction = &freeByteBuffer;
            JS_IncreaseExternallyAllocatedMemory(_runtime, byteBuffer->capacity());
        } else {
            freeFunction = &freeArrayBuffer;
        }

        return checkCallAndGetValue(exceptionTracker,
                                    JS_NewArrayBuffer(_context,
                                                      const_cast<Valdi::Byte*>(buffer.data()),
                                                      buffer.size(),
                                                      freeFunction,
                                                      Valdi::unsafeBridgeRetain(buffer.getSource().get()),
                                                      0));
    }
}

Valdi::JSValueRef QuickJSJavaScriptContext::newTypedArrayFromArrayBuffer(const Valdi::TypedArrayType& type,
                                                                         const Valdi::JSValue& arrayBuffer,
                                                                         Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    switch (type) {
        case Valdi::Int8Array:
            return newTypedArrayFromConstructor(_int8ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Int16Array:
            return newTypedArrayFromConstructor(_int16ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Int32Array:
            return newTypedArrayFromConstructor(_int32ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint8Array:
            return newTypedArrayFromConstructor(_uint8ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint8ClampedArray:
            return newTypedArrayFromConstructor(_uint8ClampedArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint16Array:
            return newTypedArrayFromConstructor(_uint16ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Uint32Array:
            return newTypedArrayFromConstructor(_uint32ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Float32Array:
            return newTypedArrayFromConstructor(_float32ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::Float64Array:
            return newTypedArrayFromConstructor(_float64ArrayCtor, arrayBuffer, exceptionTracker);
        case Valdi::ArrayBuffer:
            return Valdi::JSValueRef::makeRetained(*this, arrayBuffer);
    }
}

Valdi::JSValueRef QuickJSJavaScriptContext::newWrappedObject(const Valdi::Ref<Valdi::RefCountable>& wrappedObject,
                                                             Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto jsWrappedObject = checkCallAndGetValue(
        exceptionTracker, JS_NewObjectProtoClass(_context, _objectPrototype, _wrappedObjectClassID));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    setObjectWrappedObject(fromValdiJSValue(jsWrappedObject.get()), wrappedObject);
    return jsWrappedObject;
}

static int toQuickJSPropertyFlags(bool writable, bool enumerable, bool configurable) {
    int flags = JS_PROP_THROW;
    if (writable) {
        flags |= JS_PROP_WRITABLE;
    }
    if (enumerable) {
        flags |= JS_PROP_ENUMERABLE;
    }
    if (configurable) {
        flags |= JS_PROP_CONFIGURABLE;
    }
    return flags;
}

Valdi::JSValueRef QuickJSJavaScriptContext::newNativeClassFunction(
    const Valdi::Ref<NativeClassFunctionData>& functionData,
    const JSClassDefWithId* classDef,
    std::string_view name,
    Valdi::JSExceptionTracker& exceptionTracker) {
    auto function = checkCallAndGetValue(
        exceptionTracker, JS_NewObjectProtoClass(_context, _functionPrototype, classDef->classID));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    setNativeClassFunctionData(fromValdiJSValue(function.get()), functionData);
    auto nameValue = newStringUTF8(name, exceptionTracker);
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    checkCall(exceptionTracker,
              JS_DefinePropertyValueStr(_context,
                                        fromValdiJSValue(function.get()),
                                        "name",
                                        JS_DupValue(_context, fromValdiJSValue(nameValue.get())),
                                        JS_PROP_CONFIGURABLE | JS_PROP_THROW));
    return function;
}

Valdi::JSValueRef QuickJSJavaScriptContext::newNativeClass(const Valdi::Ref<Valdi::RefCountable>& classOpaque,
                                                           const Valdi::JSClassDefinition& classDefinition,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto nativeClass = Valdi::makeShared<Valdi::JSNativeClassData>(
        classDefinition.getName(), classOpaque, classDefinition.getConstructor());

    auto prototype = checkCallAndGetValue(exceptionTracker, JS_NewObject(_context));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    auto constructor = checkCallAndGetValue(
        exceptionTracker, JS_NewObjectProtoClass(_context, _functionPrototype, _nativeClassConstructorClassID));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }
    setNativeClassConstructorData(fromValdiJSValue(constructor.get()), nativeClass);
    JS_SetConstructorBit(_context, fromValdiJSValue(constructor.get()), true);

    auto className = newStringUTF8(classDefinition.getName().toStringView(), exceptionTracker);
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }
    if (!checkCall(exceptionTracker,
                   JS_DefinePropertyValueStr(_context,
                                             fromValdiJSValue(constructor.get()),
                                             "name",
                                             JS_DupValue(_context, fromValdiJSValue(className.get())),
                                             JS_PROP_CONFIGURABLE | JS_PROP_THROW))) {
        return Valdi::JSValueRef();
    }

    JS_SetConstructor(_context, fromValdiJSValue(constructor.get()), fromValdiJSValue(prototype.get()));

    auto defineDataProperty = [&](JSValueConst object,
                                  std::string_view name,
                                  JSValue value,
                                  bool writable,
                                  bool enumerable,
                                  bool configurable) -> bool {
        auto atom = JS_NewAtomLen(_context, name.data(), name.size());
        auto result = JS_DefinePropertyValue(
            _context, object, atom, value, toQuickJSPropertyFlags(writable, enumerable, configurable));
        JS_FreeAtom(_context, atom);
        return checkCall(exceptionTracker, result);
    };

    for (const auto& entry : classDefinition.getEntries()) {
        auto object =
            entry.isClassMember() ? fromValdiJSValue(constructor.get()) : fromValdiJSValue(prototype.get());
        auto* memberClassDef =
            entry.isClassMember() ? getNativeClassStaticMemberClassDef() : getNativeClassInstanceMemberClassDef();
        switch (entry.getKind()) {
            case Valdi::JSClassEntryKind::Method: {
                if (entry.getMethodCallback() == nullptr) {
                    exceptionTracker.onError("Native class method callback cannot be null");
                    return Valdi::JSValueRef();
                }
                const auto& propertyName = entry.getName();
                auto functionData = Valdi::makeShared<NativeClassFunctionData>(nativeClass, propertyName);
                functionData->callback = entry.getMethodCallback();
                auto function = newNativeClassFunction(
                    functionData, memberClassDef, propertyName.toStringView(), exceptionTracker);
                if (!exceptionTracker ||
                    !defineDataProperty(object,
                                        propertyName.toStringView(),
                                        JS_DupValue(_context, fromValdiJSValue(function.get())),
                                        entry.isWritable(),
                                        entry.isEnumerable(),
                                        entry.isConfigurable())) {
                    return Valdi::JSValueRef();
                }
                break;
            }
            case Valdi::JSClassEntryKind::Constant:
                if (!defineDataProperty(object,
                                        entry.getName().toStringView(),
                                        JS_DupValue(_context, fromValdiJSValue(entry.getValue().get())),
                                        entry.isWritable(),
                                        entry.isEnumerable(),
                                        entry.isConfigurable())) {
                    return Valdi::JSValueRef();
                }
                break;
            case Valdi::JSClassEntryKind::Accessor: {
                if (entry.getGetterCallback() == nullptr && entry.getSetterCallback() == nullptr) {
                    exceptionTracker.onError("Native class accessor must have a getter or setter");
                    return Valdi::JSValueRef();
                }

                Valdi::JSValueRef getter;
                Valdi::JSValueRef setter;
                const auto& propertyName = entry.getName();
                if (entry.getGetterCallback() != nullptr) {
                    auto functionData = Valdi::makeShared<NativeClassFunctionData>(nativeClass, propertyName);
                    functionData->callback = entry.getGetterCallback();
                    getter = newNativeClassFunction(functionData,
                                                    memberClassDef,
                                                    propertyName.toStringView(),
                                                    exceptionTracker);
                }
                if (exceptionTracker && entry.getSetterCallback() != nullptr) {
                    auto functionData = Valdi::makeShared<NativeClassFunctionData>(nativeClass, propertyName);
                    functionData->callback = entry.getSetterCallback();
                    setter = newNativeClassFunction(functionData,
                                                    memberClassDef,
                                                    propertyName.toStringView(),
                                                    exceptionTracker);
                }
                if (!exceptionTracker) {
                    return Valdi::JSValueRef();
                }

                auto getterValue =
                    getter.empty() ? JS_UNDEFINED : JS_DupValue(_context, fromValdiJSValue(getter.get()));
                auto setterValue =
                    setter.empty() ? JS_UNDEFINED : JS_DupValue(_context, fromValdiJSValue(setter.get()));
                auto propertyNameView = propertyName.toStringView();
                auto atom = JS_NewAtomLen(_context, propertyNameView.data(), propertyNameView.size());
                auto result =
                    JS_DefinePropertyGetSet(_context,
                                            object,
                                            atom,
                                            getterValue,
                                            setterValue,
                                            toQuickJSPropertyFlags(
                                                false, entry.isEnumerable(), entry.isConfigurable()));
                JS_FreeAtom(_context, atom);
                if (!checkCall(exceptionTracker, result)) {
                    return Valdi::JSValueRef();
                }
                break;
            }
        }
    }

    return constructor;
}

Valdi::JSValueRef QuickJSJavaScriptContext::newObjectFromNativeClass(const Valdi::Ref<Valdi::RefCountable>& opaque,
                                                                     const Valdi::JSValue& cls,
                                                                     Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto clsValue = fromValdiJSValue(cls);
    auto nativeClass = getNativeClassConstructorData(clsValue);
    if (nativeClass == nullptr) {
        exceptionTracker.onError("Value is not a native class");
        return Valdi::JSValueRef();
    }
    if (opaque == nullptr) {
        exceptionTracker.onError("Native class opaque object cannot be null");
        return Valdi::JSValueRef();
    }

    auto prototype = checkCallAndGetValue(exceptionTracker, JS_GetPropertyStr(_context, clsValue, "prototype"));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    auto object = checkCallAndGetValue(
        exceptionTracker, JS_NewObjectProtoClass(_context, fromValdiJSValue(prototype.get()), _wrappedObjectClassID));
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    auto instanceData = Valdi::makeShared<Valdi::JSNativeClassInstanceData>(nativeClass, opaque);
    setObjectWrappedObject(fromValdiJSValue(object.get()), instanceData);
    return object;
}

Valdi::JSValueRef QuickJSJavaScriptContext::getObjectProperty(const Valdi::JSValue& object,
                                                              const std::string_view& propertyName,
                                                              Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker,
                                JS_GetPropertyStr(_context, fromValdiJSValue(object), propertyName.data()));
}

Valdi::JSValueRef QuickJSJavaScriptContext::getObjectProperty(const Valdi::JSValue& object,
                                                              const Valdi::JSPropertyName& propertyName,
                                                              Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(
        exceptionTracker, JS_GetProperty(_context, fromValdiJSValue(object), fromValdiJSPropertyName(propertyName)));
}

Valdi::JSValueRef QuickJSJavaScriptContext::getObjectPropertyForIndex(const Valdi::JSValue& object,
                                                                      size_t index,
                                                                      Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    return checkCallAndGetValue(exceptionTracker,
                                JS_GetPropertyUint32(_context, fromValdiJSValue(object), static_cast<uint32_t>(index)));
}

JSValue QuickJSJavaScriptContext::getConstructorFromObject(const Valdi::JSValue& object,
                                                           const std::string_view& propertyName,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto value = getObjectProperty(object, propertyName, exceptionTracker);
    if (!exceptionTracker) {
        return JS_UNDEFINED;
    }
    auto jsValue = fromValdiJSValue(value.get());

    if (JS_IsConstructor(_context, jsValue) == 0) {
        exceptionTracker.onError(Valdi::Error(STRING_FORMAT("{} is not a constructor", propertyName)));
        return JS_UNDEFINED;
    }

    value.unsafeReleaseValue();

    return jsValue;
}

bool QuickJSJavaScriptContext::hasObjectProperty(const Valdi::JSValue& object,
                                                 const Valdi::JSPropertyName& propertyName) {
    auto guard = _threadAccessChecker.guard();
    return JS_HasProperty(_context, fromValdiJSValue(object), fromValdiJSPropertyName(propertyName)) != 0;
}

void QuickJSJavaScriptContext::setObjectProperty(const Valdi::JSValue& object,
                                                 const std::string_view& propertyName,
                                                 const Valdi::JSValue& propertyValue,
                                                 bool enumerable,
                                                 Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    // JS_SetProperty() automatically releases the given value.
    auto retainedJsValue = JS_DupValue(_context, fromValdiJSValue(propertyValue));

    if (VALDI_LIKELY(enumerable)) {
        checkCall(exceptionTracker,
                  JS_SetPropertyStr(_context, fromValdiJSValue(object), propertyName.data(), retainedJsValue));
    } else {
        auto convertedPropertyName = newPropertyName(propertyName);
        setObjectPropertyNonEumerable(object, convertedPropertyName.get(), retainedJsValue, exceptionTracker);
    }
}

void QuickJSJavaScriptContext::setObjectProperty(const Valdi::JSValue& object,
                                                 const Valdi::JSPropertyName& propertyName,
                                                 const Valdi::JSValue& propertyValue,
                                                 bool enumerable,
                                                 Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    // JS_SetProperty() automatically releases the given value.
    auto retainedJsValue = JS_DupValue(_context, fromValdiJSValue(propertyValue));

    if (VALDI_LIKELY(enumerable)) {
        checkCall(
            exceptionTracker,
            JS_SetProperty(_context, fromValdiJSValue(object), fromValdiJSPropertyName(propertyName), retainedJsValue));
    } else {
        setObjectPropertyNonEumerable(object, propertyName, retainedJsValue, exceptionTracker);
    }
}

void QuickJSJavaScriptContext::setObjectPropertyNonEumerable(const Valdi::JSValue& object,
                                                             const Valdi::JSPropertyName& propertyName,
                                                             const JSValue& propertyValue,
                                                             Valdi::JSExceptionTracker& exceptionTracker) {
    auto ret = JS_DefineProperty(_context,
                                 fromValdiJSValue(object),
                                 fromValdiJSPropertyName(propertyName),
                                 propertyValue,
                                 JS_UNDEFINED,
                                 JS_UNDEFINED,
                                 JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE | JS_PROP_HAS_VALUE |
                                     JS_PROP_HAS_CONFIGURABLE | JS_PROP_HAS_WRITABLE | JS_PROP_THROW);
    JS_FreeValue(_context, propertyValue);
    checkCall(exceptionTracker, ret);
}

void QuickJSJavaScriptContext::setObjectProperty(const Valdi::JSValue& object,
                                                 const Valdi::JSValue& propertyName,
                                                 const Valdi::JSValue& propertyValue,
                                                 bool enumerable,
                                                 Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto atom = JS_ValueToAtom(_context, fromValdiJSValue(propertyName));
    if (atom == JS_ATOM_NULL) {
        setExceptionToTracker(exceptionTracker);
        return;
    }

    setObjectProperty(object, toValdiJSPropertyName(atom), propertyValue, enumerable, exceptionTracker);

    JS_FreeAtom(_context, atom);
}

void QuickJSJavaScriptContext::setObjectPropertyIndex(const Valdi::JSValue& object,
                                                      size_t index,
                                                      const Valdi::JSValue& value,
                                                      Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    // JS_SetProperty() automatically releases the given value.
    auto retainedJsValue = JS_DupValue(_context, fromValdiJSValue(value));
    checkCall(exceptionTracker,
              JS_SetPropertyUint32(_context, fromValdiJSValue(object), static_cast<uint32_t>(index), retainedJsValue));
}

Valdi::JSValueRef QuickJSJavaScriptContext::callObjectAsFunction(const Valdi::JSValue& object,
                                                                 Valdi::JSFunctionCallContext& callContext) {
    auto guard = _threadAccessChecker.guard();
    JSValue argv[callContext.getParameterSize()];
    for (size_t i = 0; i < callContext.getParameterSize(); i++) {
        argv[i] = fromValdiJSValue(callContext.getParameter(i));
    }

    return checkCallAndGetValue(callContext.getExceptionTracker(),
                                JS_Call(_context,
                                        fromValdiJSValue(object),
                                        fromValdiJSValue(callContext.getThisValue()),
                                        static_cast<int>(callContext.getParameterSize()),
                                        argv));
}

Valdi::JSValueRef QuickJSJavaScriptContext::callObjectAsConstructor(const Valdi::JSValue& object,
                                                                    Valdi::JSFunctionCallContext& callContext) {
    auto guard = _threadAccessChecker.guard();
    JSValue argv[callContext.getParameterSize()];
    for (size_t i = 0; i < callContext.getParameterSize(); i++) {
        argv[i] = fromValdiJSValue(callContext.getParameter(i));
    }

    return checkCallAndGetValue(
        callContext.getExceptionTracker(),
        JS_CallConstructor(_context, fromValdiJSValue(object), static_cast<int>(callContext.getParameterSize()), argv));
}

void QuickJSJavaScriptContext::visitObjectPropertyNames(const Valdi::JSValue& object,
                                                        Valdi::JSExceptionTracker& exceptionTracker,
                                                        Valdi::IJavaScriptPropertyNamesVisitor& visitor) {
    auto guard = _threadAccessChecker.guard();
    auto jsObject = fromValdiJSValue(object);

    JSPropertyEnum* properties = nullptr;
    uint32_t len = 0;

    for (;;) {
        if (!checkCall(
                exceptionTracker,
                JS_GetOwnPropertyNames(_context, &properties, &len, jsObject, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY))) {
            return;
        }

        for (uint32_t i = 0; i < len; i++) {
            auto propertyAtom = properties[i].atom;

            bool shouldContinue =
                visitor.visitPropertyName(*this, object, toValdiJSPropertyName(propertyAtom), exceptionTracker);
            if (!exceptionTracker || !shouldContinue) {
                JS_FreePropertyEnum(_context, properties, len);
                return;
            }
        }

        JS_FreePropertyEnum(_context, properties, len);

        jsObject = JS_GetPrototype(_context, jsObject);
        JS_FreeValue(_context, jsObject);

        if (JS_IsNull(jsObject) != 0 || JS_VALUE_GET_PTR(jsObject) == JS_VALUE_GET_PTR(_objectPrototype)) {
            break;
        }
        if (JS_IsException(jsObject) != 0) {
            setExceptionToTracker(exceptionTracker);
            break;
        }
    }
}

Valdi::StringBox QuickJSJavaScriptContext::valueToString(const Valdi::JSValue& value,
                                                         Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    size_t len = 0;
    const auto* cString = JS_ToCStringLen(_context, &len, fromValdiJSValue(value));
    if (!checkCall(exceptionTracker, cString == nullptr ? -1 : 0)) {
        return Valdi::StringBox();
    }

    auto str = Valdi::StringCache::getGlobal().makeString(cString, len);
    JS_FreeCString(_context, cString);

    return str;
}

Valdi::Ref<Valdi::StaticString> QuickJSJavaScriptContext::valueToStaticString(
    const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    size_t len = 0;
    const auto* cString = JS_ToCStringLen(_context, &len, fromValdiJSValue(value));
    if (!checkCall(exceptionTracker, cString == nullptr ? -1 : 0)) {
        return nullptr;
    }

    auto str = Valdi::StaticString::makeUTF8(cString, len);
    JS_FreeCString(_context, cString);

    return str;
}

bool QuickJSJavaScriptContext::valueToBool(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    // JS_ToBool returns -1 for exceptions *and* coerces non-zero numbers and non-empty strings to true
    auto result = JS_ToBool(_context, fromValdiJSValue(value));

    if (!checkCall(exceptionTracker, result)) {
        return false;
    }

    return static_cast<bool>(result != 0);
}

double QuickJSJavaScriptContext::valueToDouble(const Valdi::JSValue& value,
                                               Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    double number = 0;

    if (!checkCall(exceptionTracker, JS_ToFloat64(_context, &number, fromValdiJSValue(value)))) {
        return 0;
    }

    return number;
}

int32_t QuickJSJavaScriptContext::valueToInt(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    int32_t number = 0;

    if (!checkCall(exceptionTracker, JS_ToInt32(_context, &number, fromValdiJSValue(value)))) {
        return 0;
    }

    return number;
}

Valdi::Ref<Valdi::RefCountable> QuickJSJavaScriptContext::valueToWrappedObject(
    const Valdi::JSValue& value, Valdi::JSExceptionTracker& /*exceptionTracker*/) {
    auto guard = _threadAccessChecker.guard();
    return Valdi::unwrapNativeClassInstanceData(getObjectWrappedObject(fromValdiJSValue(value)));
}

Valdi::Ref<Valdi::JSFunction> QuickJSJavaScriptContext::valueToFunction(
    const Valdi::JSValue& value, Valdi::JSExceptionTracker& /*exceptionTracker*/) {
    auto guard = _threadAccessChecker.guard();
    return Valdi::Ref(getObjectCallable(fromValdiJSValue(value)));
}

Valdi::JSTypedArray QuickJSJavaScriptContext::valueToTypedArray(const Valdi::JSValue& value,
                                                                Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto jsValue = fromValdiJSValue(value);

    if (JS_IsArrayBuffer(_context, jsValue) != 0) {
        size_t size = 0;
        auto* buffer = JS_GetArrayBuffer(_context, &size, jsValue);

        if (!checkCall(exceptionTracker, buffer == nullptr ? -1 : 0)) {
            return Valdi::JSTypedArray();
        }

        return Valdi::JSTypedArray(Valdi::ArrayBuffer, buffer, size, toUnretainedJSValueRef(jsValue));
    } else {
        auto arrayType = JS_GetTypedArrayType(_context, jsValue);
        if (arrayType == JS_TYPED_ARRAY_NONE) {
            checkCallAndGetValue(exceptionTracker, JS_ThrowTypeError(_context, "value is not a typed array"));
            return Valdi::JSTypedArray();
        }

        auto arrayBuffer = checkCallAndGetValue(exceptionTracker,
                                                JS_GetTypedArrayBuffer(_context, jsValue, nullptr, nullptr, nullptr));
        if (!exceptionTracker) {
            return Valdi::JSTypedArray();
        }

        size_t length = 0;
        size_t bytesLength = 0;
        auto* ptr = JS_GetTypedArrayData(_context, jsValue, &length, &bytesLength);
        if (!checkCall(exceptionTracker, ptr == nullptr ? -1 : 0)) {
            return Valdi::JSTypedArray();
        }

        auto type = toValdiTypedArrayType(arrayType);

        return Valdi::JSTypedArray(type, ptr, static_cast<size_t>(bytesLength), std::move(arrayBuffer));
    }
}

Valdi::ValueType QuickJSJavaScriptContext::getValueType(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    auto jsValue = fromValdiJSValue(value);

    if (JS_IsUndefined(jsValue) != 0) {
        return Valdi::ValueType::Undefined;
    }
    if (JS_IsNull(jsValue) != 0) {
        return Valdi::ValueType::Null;
    }
    if (JS_IsString(jsValue) != 0) {
        return Valdi::ValueType::StaticString;
    }
    if (JS_IsNumber(jsValue) != 0) {
        return Valdi::ValueType::Double;
    }
    if (JS_IsBool(jsValue) != 0) {
        return Valdi::ValueType::Bool;
    }
    if (JS_IsArray(_context, jsValue) != 0) {
        return Valdi::ValueType::Array;
    }
    if (JS_IsObjectOfClass(_context, jsValue, getWrappedObjectClassDef()->classID) != 0) {
        return Valdi::ValueType::ValdiObject;
    }
    if (JS_IsFunction(_context, jsValue) != 0) {
        return Valdi::ValueType::Function;
    }
    if (JS_GetTypedArrayType(_context, jsValue) != JS_TYPED_ARRAY_NONE || JS_IsArrayBuffer(_context, jsValue) != 0) {
        return Valdi::ValueType::TypedArray;
    }
    if (JS_IsError(_context, jsValue) != 0) {
        return Valdi::ValueType::Error;
    }
    if (JS_IsSymbol(jsValue) != 0) {
        // Symbols are opaque types that we don't support exporting
        return Valdi::ValueType::Undefined;
    }
    if (!getLongConstructor().empty() &&
        JS_IsInstanceOf(_context, jsValue, fromValdiJSValue(getLongConstructor().get())) != 0) {
        return Valdi::ValueType::Long;
    }

    // Regular object
    return Valdi::ValueType::Map;
}

bool QuickJSJavaScriptContext::isValueUndefined(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    return JS_IsUndefined(fromValdiJSValue(value)) != 0;
}

bool QuickJSJavaScriptContext::isValueNull(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    return JS_IsNull(fromValdiJSValue(value)) != 0;
}

bool QuickJSJavaScriptContext::isValueFunction(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    return JS_IsFunction(_context, fromValdiJSValue(value)) != 0;
}

bool QuickJSJavaScriptContext::isValueNumber(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    return JS_IsNumber(fromValdiJSValue(value)) != 0;
}

bool QuickJSJavaScriptContext::isValueEqual(const Valdi::JSValue& left, const Valdi::JSValue& right) {
    auto guard = _threadAccessChecker.guard();
    return JS_VALUE_GET_PTR(fromValdiJSValue(left)) == JS_VALUE_GET_PTR(fromValdiJSValue(right));
}

bool QuickJSJavaScriptContext::isValueLong(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    auto jsValue = fromValdiJSValue(value);

    return !getLongConstructor().empty() &&
           JS_IsInstanceOf(_context, jsValue, fromValdiJSValue(getLongConstructor().get())) != 0;
}

bool QuickJSJavaScriptContext::isValueObject(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    auto jsValue = fromValdiJSValue(value);
    return JS_IsObject(jsValue) != 0;
}

inline Valdi::JSValueRef QuickJSJavaScriptContext::toRetainedJSValueRef(const JSValue& value) {
    return Valdi::JSValueRef(this, toValdiJSValue(value), true);
}

inline Valdi::JSValueRef QuickJSJavaScriptContext::toUnretainedJSValueRef(const JSValue& value) {
    return Valdi::JSValueRef(this, toValdiJSValue(value), false);
}

void QuickJSJavaScriptContext::retainValue(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    JS_DupValue(_context, fromValdiJSValue(value));
}

void QuickJSJavaScriptContext::releaseValue(const Valdi::JSValue& value) {
    auto guard = _threadAccessChecker.guard();
    JS_FreeValue(_context, fromValdiJSValue(value));
    _needsGarbageCollect = true;
}

void QuickJSJavaScriptContext::retainPropertyName(const Valdi::JSPropertyName& value) {
    auto guard = _threadAccessChecker.guard();
    JS_DupAtom(_context, fromValdiJSPropertyName(value));
}

void QuickJSJavaScriptContext::releasePropertyName(const Valdi::JSPropertyName& value) {
    auto guard = _threadAccessChecker.guard();
    JS_FreeAtom(_context, fromValdiJSPropertyName(value));
}

void QuickJSJavaScriptContext::garbageCollect() {
    auto guard = _threadAccessChecker.guard();
    JS_RunGC(_runtime);
}

static JSValue flushMicrotask(JSContext* ctx, int argc, JSValueConst* argv) {
    return JS_Call(ctx, argv[0], JS_UNDEFINED, 0, nullptr);
}

void QuickJSJavaScriptContext::enqueueMicrotask(const Valdi::JSValue& value,
                                                Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    auto jsValue = fromValdiJSValue(value);
    JS_EnqueueJob(_context, &flushMicrotask, 1, &jsValue);
}

Valdi::JavaScriptContextMemoryStatistics QuickJSJavaScriptContext::dumpMemoryStatistics() {
    auto guard = _threadAccessChecker.guard();
    JSMemoryUsage memoryUsage;
    JS_ComputeMemoryUsage(_runtime, &memoryUsage);

    Valdi::JavaScriptContextMemoryStatistics out;
    out.memoryUsageBytes = static_cast<size_t>(memoryUsage.memory_used_size);
    out.objectsCount = static_cast<size_t>(memoryUsage.obj_count);

    return out;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"

static inline Valdi::JavaScriptHeapDumpBuilder* getHeapDumpBuilder(JSRuntime* rt) {
    return reinterpret_cast<Valdi::JavaScriptHeapDumpBuilder*>(JS_GetRuntimeOpaque(rt));
}

static void onObjectBegin(JSRuntime* rt, void* gp, const char* name, JSObjectType objectType, size_t bytesSize) {
    auto* heapDumpBuilder = getHeapDumpBuilder(rt);
    auto internedName =
        Valdi::StringCache::getGlobal().makeString(name != nullptr ? std::string_view(name) : "<unnamed>");
    Valdi::JavaScriptHeapDumpNodeType nodeType;

    switch (objectType) {
        case JS_OBJ_TYPE_JS_OBJECT:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::OBJECT;
            break;
        case JS_OBJ_TYPE_JS_NUMBER:
        case JS_OBJ_TYPE_JS_BOOLEAN:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::NUMBER;
            break;
        case JS_OBJ_TYPE_JS_STRING:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::STRING;
            break;
        case JS_OBJ_TYPE_JS_ARRAY:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::ARRAY;
            break;
        case JS_OBJ_TYPE_JS_BIGINT:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::BIGINT;
            break;
        case JS_OBJ_TYPE_JS_SYMBOL:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::SYMBOL;
            break;
        case JS_OBJ_TYPE_FUNCTION_BYTECODE:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::CODE;
            break;
        case JS_OBJ_TYPE_SHAPE:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::OBJECT_SHAPE;
            break;
        case JS_OBJ_TYPE_VAR_REF:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::CLOSURE;
            break;
        case JS_OBJ_TYPE_ASYNC_FUNCTION:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::OBJECT;
            break;
        case JS_OBJ_TYPE_JS_CONTEXT:
            nodeType = Valdi::JavaScriptHeapDumpNodeType::OBJECT;
            internedName = STRING_LITERAL("<context>");
            break;
    }

    heapDumpBuilder->beginNode(nodeType, internedName, gp, bytesSize);
}

static void onObjectEdgeProperty(JSRuntime* rt,
                                 void* objectPtr,
                                 JSEdgeType edgeType,
                                 JSEdgeValueType edgeValueType,
                                 const char* propertyName,
                                 const char* propertyValue,
                                 size_t selfSizeBytes) {
    auto* heapDumpBuilder = getHeapDumpBuilder(rt);

    Valdi::JavaScriptHeapDumpEdgeType resolvedDumpEdgeType;
    Valdi::JavaScriptHeapEdgeIdentifier edgeIdentifier;
    switch (edgeType) {
        case JS_EDGE_TYPE_ELEMENT:
            resolvedDumpEdgeType = Valdi::JavaScriptHeapDumpEdgeType::ELEMENT;
            edgeIdentifier = Valdi::JavaScriptHeapEdgeIdentifier::indexed(atoi(propertyName)); // NOLINT(cert-err34-c)
            break;
        case JS_EDGE_TYPE_PROPERTY:
            resolvedDumpEdgeType = Valdi::JavaScriptHeapDumpEdgeType::PROPERTY;
            edgeIdentifier = Valdi::JavaScriptHeapEdgeIdentifier::named(propertyName);
            break;
        case JS_EDGE_TYPE_INTERNAL:
            resolvedDumpEdgeType = Valdi::JavaScriptHeapDumpEdgeType::INTERNAL;
            edgeIdentifier = Valdi::JavaScriptHeapEdgeIdentifier::named(propertyName);
            break;
        case JS_EDGE_TYPE_HIDDEN:
            resolvedDumpEdgeType = Valdi::JavaScriptHeapDumpEdgeType::HIDDEN;
            edgeIdentifier = Valdi::JavaScriptHeapEdgeIdentifier::named(propertyName);
            break;
    }

    switch (edgeValueType) {
        case JS_EDGE_VALUE_TYPE_REFERENCE:
            heapDumpBuilder->appendEdge(resolvedDumpEdgeType, edgeIdentifier, objectPtr);
            break;
        case JS_EDGE_VALUE_TYPE_STRING:
            heapDumpBuilder->appendEdgeToValue(resolvedDumpEdgeType,
                                               edgeIdentifier,
                                               Valdi::JavaScriptHeapDumpNodeType::STRING,
                                               objectPtr,
                                               propertyValue,
                                               selfSizeBytes);
            break;
        case JS_EDGE_VALUE_TYPE_NUMBER:
            heapDumpBuilder->appendEdgeToValue(resolvedDumpEdgeType,
                                               edgeIdentifier,
                                               Valdi::JavaScriptHeapDumpNodeType::NUMBER,
                                               objectPtr,
                                               propertyValue,
                                               selfSizeBytes);
            break;
        case JS_EDGE_VALUE_TYPE_BOOLEAN:
            heapDumpBuilder->appendEdgeToValue(resolvedDumpEdgeType,
                                               edgeIdentifier,
                                               Valdi::JavaScriptHeapDumpNodeType::NUMBER,
                                               objectPtr,
                                               propertyValue,
                                               selfSizeBytes);
            break;
    }
}

static void onObjectEdge(JSRuntime* rt, JSGCObjectHeader* gp) {
    auto* heapDumpBuilder = getHeapDumpBuilder(rt);

    static auto kInternalEdge = STRING_LITERAL("<ref>");

    auto edgeIdentifier = Valdi::JavaScriptHeapEdgeIdentifier::named(kInternalEdge);
    heapDumpBuilder->appendEdge(Valdi::JavaScriptHeapDumpEdgeType::INTERNAL, edgeIdentifier, gp);
}

#pragma clang diagnostic pop

void QuickJSJavaScriptContext::dumpHeap(Valdi::JavaScriptHeapDumpBuilder& heapDumpBuilder) {
    auto guard = _threadAccessChecker.guard();
    auto* oldOpaque = JS_GetRuntimeOpaque(_runtime);

    // We shove the heap dump builder as an opaque so that we can retrieve it while visiting
    // the heap.
    JS_SetRuntimeOpaque(_runtime, reinterpret_cast<Valdi::JavaScriptHeapDumpBuilder*>(&heapDumpBuilder));
    JS_VisitAllGCObjects(_runtime, &onObjectBegin, &onObjectEdgeProperty, &onObjectEdge);

    auto stashedJSValues = getAllStashedJSValues();
    // Emit the stashed JS values as a separate node
    heapDumpBuilder.beginNode(Valdi::JavaScriptHeapDumpNodeType::HIDDEN,
                              STRING_LITERAL("System / ExportedReferences"),
                              this,
                              sizeof(JSValue) * stashedJSValues.size());

    size_t index = 0;
    for (const auto& jsValue : stashedJSValues) {
        auto indexStr = std::to_string(index);
        JS_VisitObjectEdge(
            _runtime, fromValdiJSValue(jsValue), indexStr.c_str(), JS_EDGE_TYPE_ELEMENT, &onObjectEdgeProperty);
        index++;
    }

    // Restore old opaque
    JS_SetRuntimeOpaque(_runtime, oldOpaque);
}

JSContext* QuickJSJavaScriptContext::getContext() const {
    return _context;
}

JSClassID QuickJSJavaScriptContext::initializeClass(const JSClassDefWithId* classDef,
                                                    Valdi::JSExceptionTracker& exceptionTracker) {
    if (!checkCall(exceptionTracker, JS_NewClass(_runtime, classDef->classID, &classDef->classDef))) {
        return 0;
    }

    return classDef->classID;
}

void QuickJSJavaScriptContext::willEnterVM() {
    auto guard = _threadAccessChecker.guard();
    auto count = ++_enterVmCount;
    if (count == 1) {
        updateCurrentStackLimit();
    }
}

void QuickJSJavaScriptContext::requestExecutionTermination() {
    IJavaScriptContext::requestExecutionTermination();
}

struct StackLimits {
    void* startFrameAddress;
    size_t maxStackSize;

    constexpr StackLimits(void* startFrameAddress, size_t maxStackSize)
        : startFrameAddress(startFrameAddress), maxStackSize(maxStackSize) {}
};

thread_local std::optional<StackLimits> kStackLimits;
static StackLimits getCurrentThreadStackLimits() {
    if (!kStackLimits) {
        auto stackAttrs = Valdi::Thread::getCurrentStackAttrs();
        auto startAddress =
            static_cast<void*>(static_cast<uint8_t*>(stackAttrs.minFrameAddress) + stackAttrs.stackSize);

        kStackLimits = {StackLimits(
            startAddress, static_cast<size_t>(static_cast<double>(stackAttrs.stackSize) * kMaxStackSizeRatio))};
    }

    return kStackLimits.value();
}

void QuickJSJavaScriptContext::updateCurrentStackLimit() {
    auto currentThreadId = std::this_thread::get_id();
    if (currentThreadId != _lastThreadId) {
        _lastThreadId = currentThreadId;
        auto stackLimits = getCurrentThreadStackLimits();

        JS_SetCStackTopPointer(_runtime, stackLimits.startFrameAddress);
        JS_SetMaxStackSize(_runtime, stackLimits.maxStackSize);
    }
}

void QuickJSJavaScriptContext::appendRejectedPromise(JSValueConst promise, JSValueConst reason) {
    auto& rejectedPromise = _rejectedPromises.emplace_back();
    rejectedPromise.promise = JS_DupValue(_context, promise);
    rejectedPromise.reason = JS_DupValue(_context, reason);
}

void QuickJSJavaScriptContext::removeRejectedPromise(JSValueConst promise) {
    auto it = _rejectedPromises.begin();
    while (it != _rejectedPromises.end()) {
        if (JS_VALUE_GET_PTR(it->promise) == JS_VALUE_GET_PTR(promise)) {
            JS_FreeValue(_context, it->promise);
            JS_FreeValue(_context, it->reason);
            it = _rejectedPromises.erase(it);
        } else {
            it++;
        }
    }
}

void QuickJSJavaScriptContext::notifyRejectedPromises() {
    while (!_rejectedPromises.empty()) {
        auto rejectedPromise = _rejectedPromises.front();
        _rejectedPromises.pop_front();

        auto* listener = getListener();
        if (listener != nullptr) {
            listener->onUnhandledRejectedPromise(*this, toValdiJSValue(rejectedPromise.reason));
        }

        JS_FreeValue(_context, rejectedPromise.promise);
        JS_FreeValue(_context, rejectedPromise.reason);
    }
}

void QuickJSJavaScriptContext::willExitVM(Valdi::JSExceptionTracker& exceptionTracker) {
    auto guard = _threadAccessChecker.guard();
    SC_ASSERT(_enterVmCount > 0);
    if (_enterVmCount > 1) {
        --_enterVmCount;
        return;
    }

    Valdi::Defer exitVM([this]() { --_enterVmCount; });
    if (executionTerminationRequested()) {
        return;
    }

    JSContext* context = nullptr;

    for (;;) {
        switch (JS_ExecutePendingJob(_runtime, &context)) {
            case 0:
                notifyRejectedPromises();
                return;
            case 1:
                continue;
            case -1: {
                setExceptionToTracker(exceptionTracker);
                notifyRejectedPromises();
                return;
            }
        }
    }
}

} // namespace ValdiQuickJS
