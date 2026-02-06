//
//  JavaScriptCoreContext.cpp
//  ValdiIOS
//
//  Created by Simon Corsin on 5/30/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/jscore/JavaScriptCoreContext.hpp"

#include "valdi/jscore/JSCoreCustomClasses.hpp"
#include "valdi/jscore/JSCoreDebuggerProxy.hpp"
#include "valdi/jscore/JSCoreUtils.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/Utils/RefCountableAutoreleasePool.hpp"
#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"

#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include <JavaScriptCore/JavaScriptCore.h>

#include "utils/platform/BuildOptions.hpp"
#include "utils/platform/TargetPlatform.hpp"

#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

JS_EXPORT void JSRemoteInspectorStart(uint16_t port);
using JSRunLoopScheduleCallback = void (*)(void* opaque, uint32_t delayMs);
JS_EXPORT void JSRunLoopFlush();
JS_EXPORT void JSRunLoopSetScheduleCallback(void* opaque, JSRunLoopScheduleCallback callback);
JS_EXPORT void JSSynchronousGarbageCollectForDebugging(JSContextRef ctx);

/*!
@function
@abstract Produces an object with various statistics about current memory usage.
@param ctx The execution context to use.
@result An object containing GC heap status data.
@discussion Specifically, the result object has the following integer-valued fields:
 heapSize: current size of heap
 heapCapacity: current capacity of heap
 extraMemorySize: amount of non-GC memory referenced by GC objects (included in heap size / capacity)
 objectCount: current count of GC objects
 protectedObjectCount: current count of protected GC objects
 globalObjectCount: current count of global GC objects
 protectedGlobalObjectCount: current count of protected global GC objects
*/
JS_EXPORT JSObjectRef JSGetMemoryUsageStatistics(JSContextRef ctx) __attribute__((weak));

JS_EXPORT void JSGlobalContextSetUnhandledRejectionCallback(JSGlobalContextRef ctx,
                                                            JSObjectRef function,
                                                            JSValueRef* exception) __attribute__((weak));

#ifdef __cplusplus
}
#endif

namespace ValdiJSCore {

static JSTypedArrayType jsTypedArrayTypeFromTypedArrayType(const Valdi::TypedArrayType& type) {
    switch (type) {
        case Valdi::TypedArrayType::Int8Array:
            return kJSTypedArrayTypeInt8Array;
        case Valdi::TypedArrayType::Int16Array:
            return kJSTypedArrayTypeInt16Array;
        case Valdi::TypedArrayType::Int32Array:
            return kJSTypedArrayTypeInt32Array;
        case Valdi::TypedArrayType::Uint8Array:
            return kJSTypedArrayTypeUint8Array;
        case Valdi::TypedArrayType::Uint8ClampedArray:
            return kJSTypedArrayTypeUint8ClampedArray;
        case Valdi::TypedArrayType::Uint16Array:
            return kJSTypedArrayTypeUint16Array;
        case Valdi::TypedArrayType::Uint32Array:
            return kJSTypedArrayTypeUint32Array;
        case Valdi::TypedArrayType::Float32Array:
            return kJSTypedArrayTypeFloat32Array;
        case Valdi::TypedArrayType::Float64Array:
            return kJSTypedArrayTypeFloat64Array;
        case Valdi::TypedArrayType::ArrayBuffer:
            return kJSTypedArrayTypeArrayBuffer;
    }
}

static Valdi::ReferenceInfo makePromiseCallbackReferenceInfo() {
    auto key = STRING_LITERAL("UnhandledPromiseCallback");
    return Valdi::ReferenceInfoBuilder().withObject(key).build();
}

class JavaScriptCoreUnhandledPromiseCallback : public Valdi::JSFunction {
public:
    JavaScriptCoreUnhandledPromiseCallback() : _referenceInfo(makePromiseCallbackReferenceInfo()) {}

    ~JavaScriptCoreUnhandledPromiseCallback() override = default;

    const Valdi::ReferenceInfo& getReferenceInfo() const override {
        return _referenceInfo;
    }

    Valdi::JSValueRef operator()(Valdi::JSFunctionNativeCallContext& callContext) noexcept override {
        auto* listener = callContext.getContext().getListener();
        if (listener != nullptr && callContext.getParameterSize() > 1) {
            listener->onUnhandledRejectedPromise(callContext.getContext(), callContext.getParameter(1));
        }

        return callContext.getContext().newUndefined();
    }

private:
    Valdi::ReferenceInfo _referenceInfo;
};

JavaScriptCoreContext::JavaScriptCoreContext(Valdi::JavaScriptTaskScheduler* taskScheduler,
                                             [[maybe_unused]] Valdi::ILogger& logger)
    : IJavaScriptContext(taskScheduler), _logger(logger) {
    _contextGroup = JSContextGroupCreate();
    _globalContext = JSGlobalContextCreateInGroup(_contextGroup, nullptr);

#if (TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MAX_ALLOWED >= 160400) ||                                                 \
    (TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED >= 130300)
    if constexpr (snap::kIsGoldBuild || snap::kIsDevBuild) {
        if (__builtin_available(macOS 13.3, iOS 16.4, *)) {
            JSGlobalContextSetInspectable(_globalContext, true);
        }
    }
#endif

    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
    auto str = newPropertyName("Valdi JS Context");
    JSGlobalContextSetName(_globalContext, fromValdiJSPropertyName(str.get()));

    static_assert(kJSTypeUndefined == 0, "Undefined type must be set to 0");
}

JavaScriptCoreContext::~JavaScriptCoreContext() {
#if !TARGET_OS_IPHONE && !TARGET_OS_OSX
    JSRunLoopSetScheduleCallback(nullptr, nullptr);
#endif

    while (!_microtasks.empty()) {
        auto microtask = _microtasks.front();
        _microtasks.pop_front();
        JSValueUnprotect(_globalContext, microtask);
    }

    prepareForTeardown();

    auto globalContext = _globalContext;
    auto contextGroup = _contextGroup;

    _globalContext = nullptr;
    _contextGroup = nullptr;

    JSGlobalContextRelease(globalContext);
    JSContextGroupRelease(contextGroup);
}

void JavaScriptCoreContext::onInitialize(Valdi::JSExceptionTracker& exceptionTracker) {
    auto globalRef = getGlobalObject(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto errorConstructor = getObjectProperty(globalRef.get(), "Error", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _errorConstructor = fromValdiJSValue(errorConstructor.get()).asObjectRef();
    JSValueProtect(getJSGlobalContext(), _errorConstructor);

    auto functionConstructor = getObjectProperty(globalRef.get(), "Function", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    _functionPrototype =
        JSObjectGetPrototype(_globalContext, fromValdiJSValue(functionConstructor.get()).asObjectRef());
    JSValueProtect(_globalContext, _functionPrototype);

    auto arrayBufferConstructor = getObjectProperty(globalRef.get(), "ArrayBuffer", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    Valdi::JSFunctionCallContext callContext(*this, nullptr, 0, exceptionTracker);
    auto emptyArrayBuffer = callObjectAsConstructor(arrayBufferConstructor.get(), callContext);
    if (!exceptionTracker) {
        return;
    }
    _emptyArrayBuffer = fromValdiJSValue(emptyArrayBuffer.get()).asObjectRef();
    JSValueProtect(_globalContext, _emptyArrayBuffer);

    auto weakRefConstructor = getObjectProperty(globalRef.get(), "WeakRef", exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    if (!isValueUndefined(weakRefConstructor.get())) {
        // Our JSCore version supports WeakRef
        _weakRefConstructor = fromValdiJSValue(weakRefConstructor.get()).asObjectRef();
        JSValueProtect(_globalContext, _weakRefConstructor);

        auto weakRefPrototype = getObjectProperty(weakRefConstructor.get(), "prototype", exceptionTracker);
        if (!exceptionTracker) {
            return;
        }

        auto derefFunction = getObjectProperty(weakRefPrototype.get(), "deref", exceptionTracker);
        if (!exceptionTracker) {
            return;
        }

        _weakRefDerefFunction = fromValdiJSValue(derefFunction.get()).asObjectRef();
        JSValueProtect(_globalContext, _weakRefDerefFunction);
    }

    if constexpr (!snap::kIsAppstoreBuild || !snap::isIos()) {
        // JSGlobalContextSetUnhandledRejectionCallback() is currently a private symbol, so we use it
        // only on Android or non production iOS.

        auto callback = newFunction(Valdi::makeShared<JavaScriptCoreUnhandledPromiseCallback>(), exceptionTracker);

        if (!exceptionTracker) {
            return;
        }

        if (JSGlobalContextSetUnhandledRejectionCallback) {
            JSGlobalContextSetUnhandledRejectionCallback(
                _globalContext, fromValdiJSValue(callback.get()).asObjectRef(), nullptr);
        }
    }
}

inline JSContextRef JavaScriptCoreContext::getJSGlobalContext() const {
    return reinterpret_cast<JSContextRef>(_globalContext);
}

Valdi::JSValueRef JavaScriptCoreContext::getGlobalObject(Valdi::JSExceptionTracker& exceptionTracker) {
    auto globalObject = JSContextGetGlobalObject(_globalContext);
    return returnJSValueRef(globalObject, kJSTypeObject);
}

Valdi::BytesView JavaScriptCoreContext::preCompile(const std::string_view& script,
                                                   const std::string_view& sourceFilename,
                                                   Valdi::JSExceptionTracker& exceptionTracker) {
    exceptionTracker.onError(Valdi::Error("JavaScriptCore does not support precompilation"));
    return Valdi::BytesView();
}

Valdi::JSValueRef JavaScriptCoreContext::evaluate(const std::string& script,
                                                  const std::string_view& sourceFilename,
                                                  Valdi::JSExceptionTracker& exceptionTracker) {
    auto scriptRef = newPropertyName(script);

    Valdi::JSPropertyNameRef sourceURL;
    if (!sourceFilename.empty()) {
        sourceURL = newPropertyName(sourceFilename);
    }

    JSValueRef exception = nullptr;

    auto value = JSEvaluateScript(getJSGlobalContext(),
                                  fromValdiJSPropertyName(scriptRef.get()),
                                  nullptr,
                                  fromValdiJSPropertyName(sourceURL.get()),
                                  0,
                                  &exception);
    return checkCallAndGetValue(exceptionTracker, value, exception);
}

Valdi::JSValueRef JavaScriptCoreContext::evaluatePreCompiled(const Valdi::BytesView& script,
                                                             const std::string_view& sourceFilename,
                                                             Valdi::JSExceptionTracker& exceptionTracker) {
    exceptionTracker.onError(Valdi::Error("JavaScriptCore does not support precompilation"));
    return Valdi::JSValueRef();
}

void JavaScriptCoreContext::garbageCollect() {
    if (_isGarbageCollecting) {
        _needGarbageCollect = true;
        return;
    }

    _isGarbageCollecting = true;
    _needGarbageCollect = true;

    while (_needGarbageCollect) {
        _needGarbageCollect = false;

#if TARGET_OS_IPHONE
        JSGarbageCollect(_globalContext);
#else
        JSSynchronousGarbageCollectForDebugging(_globalContext);
#endif
    }
}

Valdi::JavaScriptContextMemoryStatistics JavaScriptCoreContext::dumpMemoryStatistics() {
    Valdi::JavaScriptContextMemoryStatistics out;

    if constexpr (!snap::kIsAppstoreBuild || !snap::isIos()) {
        // JSGetMemoryUsageStatistics() is currently a private symbol, so we use it
        // only on Android/Desktop or non production iOS.

        if (JSGetMemoryUsageStatistics) {
            auto memoryUsageJs = JSGetMemoryUsageStatistics(_globalContext);

            Valdi::JSExceptionTracker exceptionTracker(*this);
            auto memoryUsage = Valdi::jsValueToValue(
                *this,
                toValdiJSValue(JSCoreRef(memoryUsageJs, JSValueGetType(_globalContext, memoryUsageJs))),
                Valdi::ReferenceInfoBuilder(),
                exceptionTracker);

            exceptionTracker.clearError();

            out.memoryUsageBytes = static_cast<size_t>(memoryUsage.getMapValue("heapCapacity").toLong());
            out.objectsCount = static_cast<size_t>(memoryUsage.getMapValue("objectCount").toLong());
        }
    }

    return out;
}

Valdi::JSValueRef JavaScriptCoreContext::newObject(Valdi::JSExceptionTracker& exceptionTracker) {
    auto context = getJSGlobalContext();
    return returnJSValueRef(JSObjectMake(context, nullptr, nullptr), kJSTypeObject);
}

Valdi::JSValueRef JavaScriptCoreContext::newFunction(const Valdi::Ref<Valdi::JSFunction>& callable,
                                                     Valdi::JSExceptionTracker& exceptionTracker) {
    auto context = getJSGlobalContext();

    auto functionData = Valdi::makeShared<JSFunctionData>(*this, callable);
    auto object = JSObjectMake(context, getNativeFunctionClassRef(), Valdi::unsafeBridgeRetain(functionData.get()));

    if (!callable->getReferenceInfo().getName().isEmpty()) {
        static auto kName = JSStringCreateWithUTF8CString("name");
        auto value = newStringUTF8(callable->getReferenceInfo().getName().toStringView(), exceptionTracker);
        if (!exceptionTracker) {
            return Valdi::JSValueRef();
        }
        JSObjectSetProperty(
            context, object, kName, fromValdiJSValue(value.get()).valueRef, kJSPropertyAttributeNone, nullptr);
    }

    JSObjectSetPrototype(context, object, _functionPrototype);
    return returnJSValueRef(object, kJSTypeObject);
}

Valdi::JSValueRef JavaScriptCoreContext::onNewBool(bool boolean) {
    return returnJSValueRef(JSValueMakeBoolean(getJSGlobalContext(), boolean), kJSTypeBoolean);
}

Valdi::JSValueRef JavaScriptCoreContext::onNewNumber(int32_t number) {
    return onNewNumber(static_cast<double>(number));
}

Valdi::JSValueRef JavaScriptCoreContext::onNewNumber(double number) {
    auto context = getJSGlobalContext();
    return returnJSValueRef(JSValueMakeNumber(context, number), kJSTypeNumber);
}

Valdi::JSValueRef JavaScriptCoreContext::newStringUTF8(const std::string_view& str,
                                                       Valdi::JSExceptionTracker& exceptionTracker) {
    auto jsString = newPropertyName(str);

    return returnJSValueRef(JSValueMakeString(getJSGlobalContext(), fromValdiJSPropertyName(jsString.get())),
                            kJSTypeString);
}

Valdi::JSValueRef JavaScriptCoreContext::newStringUTF16(const std::u16string_view& str,
                                                        Valdi::JSExceptionTracker& exceptionTracker) {
    if (utf16Disabled()) {
        auto utf8 = Valdi::utf16ToUtf8(str.data(), str.size());
        return newStringUTF8(std::string_view(utf8.first, utf8.second), exceptionTracker);
    }

    auto jsString = Valdi::JSPropertyNameRef(
        this,
        toValdiJSPropertyName(JSStringCreateWithCharacters(reinterpret_cast<const JSChar*>(str.data()), str.size())),
        true);

    return returnJSValueRef(JSValueMakeString(getJSGlobalContext(), fromValdiJSPropertyName(jsString.get())),
                            kJSTypeString);
}

Valdi::JSValueRef JavaScriptCoreContext::onNewNull() {
    return returnJSValueRef(JSValueMakeNull(getJSGlobalContext()), kJSTypeNull);
}

Valdi::JSValueRef JavaScriptCoreContext::onNewUndefined() {
    return returnJSValueRef(JSValueMakeUndefined(getJSGlobalContext()), kJSTypeUndefined);
}

Valdi::JSValueRef JavaScriptCoreContext::newArray(size_t /*initialSize*/, Valdi::JSExceptionTracker& exceptionTracker) {
    JSValueRef exception = nullptr;
    auto array = JSObjectMakeArray(getJSGlobalContext(), 0, nullptr, &exception);
    return checkCallAndGetValue(exceptionTracker, array, exception);
}

Valdi::JSValueRef JavaScriptCoreContext::newArrayWithValues(const Valdi::JSValue* values,
                                                            size_t size,
                                                            Valdi::JSExceptionTracker& exceptionTracker) {
    Valdi::SmallVector<JSValueRef, 16> temp;
    for (size_t i = 0; i < size; i++) {
        temp.emplace_back(fromValdiJSValue(values[i]).valueRef);
    }

    JSValueRef exception = nullptr;
    auto array = JSObjectMakeArray(getJSGlobalContext(), size, temp.data(), &exception);
    return checkCallAndGetValue(exceptionTracker, array, exception);
}

void bufferDeallocator(void* bytes, void* deallocatorContext) {
    Valdi::RefCountableAutoreleasePool::release(deallocatorContext);
}

Valdi::JSValueRef JavaScriptCoreContext::newArrayBuffer(const Valdi::BytesView& buffer,
                                                        Valdi::JSExceptionTracker& exceptionTracker) {
    auto context = getJSGlobalContext();
    JSValueRef exception = nullptr;
    JSObjectRef arrayBuffer = nullptr;
    if (buffer.empty()) {
        arrayBuffer = _emptyArrayBuffer;
    } else {
        arrayBuffer = JSObjectMakeArrayBufferWithBytesNoCopy(context,
                                                             const_cast<Valdi::Byte*>(buffer.data()),
                                                             buffer.size(),
                                                             &bufferDeallocator,
                                                             Valdi::unsafeBridgeRetain(buffer.getSource().get()),
                                                             &exception);
        if (exception != nullptr) {
            // The bufferDeallocator will always be called if an exception is thrown
            // No need to worry about deleting the buffer here
            return checkCallAndGetValue(exceptionTracker, arrayBuffer, exception);
        }
    }

    return returnJSValueRef(arrayBuffer, kJSTypeObject);
}

Valdi::JSValueRef JavaScriptCoreContext::newTypedArrayFromArrayBuffer(const Valdi::TypedArrayType& type,
                                                                      const Valdi::JSValue& arrayBuffer,
                                                                      Valdi::JSExceptionTracker& exceptionTracker) {
    if (type == Valdi::TypedArrayType::ArrayBuffer) {
        return Valdi::JSValueRef::makeUnretained(*this, arrayBuffer);
    }

    auto arrayBufferRef = fromValdiJSValue(arrayBuffer).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return newUndefined();
    }

    auto context = getJSGlobalContext();
    JSValueRef exception = nullptr;
    auto value = JSObjectMakeTypedArrayWithArrayBuffer(
        context, jsTypedArrayTypeFromTypedArrayType(type), arrayBufferRef, &exception);
    return checkCallAndGetValue(exceptionTracker, value, exception);
}

Valdi::JSValueRef JavaScriptCoreContext::newWrappedObject(const Valdi::Ref<Valdi::RefCountable>& wrappedObject,
                                                          Valdi::JSExceptionTracker& exceptionTracker) {
    auto context = getJSGlobalContext();
    auto object = JSObjectMake(context, getWrappedObjectClassRef(), Valdi::unsafeBridgeRetain(wrappedObject.get()));

    return returnJSValueRef(object, kJSTypeObject);
}

Valdi::JSValueRef JavaScriptCoreContext::newWeakRef(const Valdi::JSValue& object,
                                                    Valdi::JSExceptionTracker& exceptionTracker) {
    if (_weakRefConstructor == nullptr) {
        // No support for weak refs
        auto jsValue = fromValdiJSValue(object);
        return returnJSValueRef(jsValue.valueRef, jsValue.type);
    } else {
        auto ctor = toUnretainedJSValueRef(_weakRefConstructor, kJSTypeObject);
        auto parameter = Valdi::JSValueRef::makeUnretained(*this, object);
        Valdi::JSFunctionCallContext callContext(*this, &parameter, 1, exceptionTracker);
        return callObjectAsConstructor(ctor.get(), callContext);
    }
}

Valdi::JSValueRef JavaScriptCoreContext::derefWeakRef(const Valdi::JSValue& weakRef,
                                                      Valdi::JSExceptionTracker& exceptionTracker) {
    if (_weakRefConstructor == nullptr) {
        // No support for weak refs
        auto jsValue = fromValdiJSValue(weakRef);
        return returnJSValueRef(jsValue.valueRef, jsValue.type);
    } else {
        auto derefFn = toUnretainedJSValueRef(_weakRefDerefFunction, kJSTypeObject);

        Valdi::JSFunctionCallContext callContext(*this, nullptr, 0, exceptionTracker);
        callContext.setThisValue(weakRef);

        return callObjectAsFunction(derefFn.get(), callContext);
    }
}

Valdi::StringBox JavaScriptCoreContext::valueToString(const Valdi::JSValue& value,
                                                      Valdi::JSExceptionTracker& exceptionTracker) {
    JSValueRef exception = nullptr;
    auto stringRef = JSValueToStringCopy(getJSGlobalContext(), fromValdiJSValue(value).valueRef, &exception);
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
        return Valdi::StringBox();
    }

    auto cppString = jsStringtoStdString(stringRef);
    JSStringRelease(stringRef);
    return cppString;
}

Valdi::Ref<Valdi::StaticString> JavaScriptCoreContext::valueToStaticString(
    const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    JSValueRef exception = nullptr;
    auto stringRef = JSValueToStringCopy(getJSGlobalContext(), fromValdiJSValue(value).valueRef, &exception);
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
        return nullptr;
    }

    Valdi::Ref<Valdi::StaticString> cppString;
    if (utf16Disabled()) {
        auto utf8 = jsStringToUTF8(stringRef);
        cppString = Valdi::StaticString::makeUTF8(utf8.first, utf8.second);
    } else {
        cppString = Valdi::StaticString::makeUTF16(
            reinterpret_cast<const char16_t*>(JSStringGetCharactersPtr(stringRef)), JSStringGetLength(stringRef));
    }

    JSStringRelease(stringRef);
    return cppString;
}

bool JavaScriptCoreContext::valueToBool(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    return JSValueToBoolean(getJSGlobalContext(), fromValdiJSValue(value).valueRef);
}

double JavaScriptCoreContext::valueToDouble(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    JSValueRef exception = nullptr;
    auto number = JSValueToNumber(getJSGlobalContext(), fromValdiJSValue(value).valueRef, &exception);
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
        return 0.0;
    }
    return number;
}

int32_t JavaScriptCoreContext::valueToInt(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    auto d = valueToDouble(value, exceptionTracker);
    if (isnan(d)) {
        return 0;
    } else if (std::isinf(d)) {
        if (d > 0) {
            return std::numeric_limits<int32_t>::max();
        } else {
            return std::numeric_limits<int32_t>::min();
        }
    }
    return static_cast<int32_t>(d);
}

Valdi::Ref<Valdi::RefCountable> JavaScriptCoreContext::valueToWrappedObject(
    const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(value).asObjectRef();
    if (objectRef == nullptr) {
        return nullptr;
    }
    return Valdi::Ref(getAttachedWrappedObject(objectRef));
}

Valdi::Ref<Valdi::JSFunction> JavaScriptCoreContext::valueToFunction(const Valdi::JSValue& value,
                                                                     Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(value).asObjectRef();
    if (objectRef == nullptr) {
        return nullptr;
    }

    auto* functionData = getAttachedJsFunctionData(objectRef);
    if (functionData == nullptr) {
        return nullptr;
    }

    return functionData->function;
}

Valdi::JSTypedArray JavaScriptCoreContext::valueToTypedArray(const Valdi::JSValue& value,
                                                             Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(value).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return Valdi::JSTypedArray();
    }

    auto contextRef = getJSGlobalContext();
    JSValueRef exception = nullptr;
    auto type = JSValueGetTypedArrayType(contextRef, objectRef, &exception);
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
        return Valdi::JSTypedArray();
    }

    if (type == kJSTypedArrayTypeNone) {
        exceptionTracker.onError(Valdi::Error("Object ref must point to a typed array"));
        return Valdi::JSTypedArray();
    }

    auto arrayBufferRef = objectRef;
    if (type != kJSTypedArrayTypeArrayBuffer) {
        arrayBufferRef = JSObjectGetTypedArrayBuffer(contextRef, objectRef, &exception);
        if (exception != nullptr) {
            storeException(exceptionTracker, exception);
            return Valdi::JSTypedArray();
        }
    }

    void* tempBuffer = JSObjectGetArrayBufferBytesPtr(contextRef, arrayBufferRef, &exception);
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
        return Valdi::JSTypedArray();
    }

    size_t bufferSize = 0;
    size_t byteOffset = 0;
    if (type != kJSTypedArrayTypeArrayBuffer) {
        bufferSize = JSObjectGetTypedArrayByteLength(contextRef, objectRef, &exception);
        if (exception != nullptr) {
            storeException(exceptionTracker, exception);
            return Valdi::JSTypedArray();
        }

        byteOffset = JSObjectGetTypedArrayByteOffset(contextRef, objectRef, &exception);
        if (exception != nullptr) {
            storeException(exceptionTracker, exception);
            return Valdi::JSTypedArray();
        }
    } else {
        bufferSize = JSObjectGetArrayBufferByteLength(contextRef, arrayBufferRef, &exception);
        if (exception != nullptr) {
            storeException(exceptionTracker, exception);
            return Valdi::JSTypedArray();
        }
    }

    // The Valdi TypedArray enum matches JSCore's kJSTypedArrayType
    auto arrayType = static_cast<Valdi::TypedArrayType>(type);

    auto bufferStart = reinterpret_cast<Valdi::Byte*>(tempBuffer) + byteOffset;

    return Valdi::JSTypedArray(
        arrayType, bufferStart, bufferSize, toUnretainedJSValueRef(arrayBufferRef, kJSTypeObject));
}

bool JavaScriptCoreContext::isValueUndefined(const Valdi::JSValue& value) {
    return fromValdiJSValue(value).type == kJSTypeUndefined;
}

bool JavaScriptCoreContext::isValueNull(const Valdi::JSValue& value) {
    return fromValdiJSValue(value).type == kJSTypeNull;
}

bool JavaScriptCoreContext::isValueFunction(const Valdi::JSValue& value) {
    auto objectRef = fromValdiJSValue(value).asObjectRef();
    if (objectRef == nullptr) {
        return false;
    }

    return JSObjectIsFunction(getJSGlobalContext(), objectRef);
}

bool JavaScriptCoreContext::isValueNumber(const Valdi::JSValue& value) {
    return fromValdiJSValue(value).type == kJSTypeNumber;
}

bool JavaScriptCoreContext::isValueLong(const Valdi::JSValue& value) {
    auto objectRef = fromValdiJSValue(value).asObjectRef();
    if (objectRef == nullptr) {
        return false;
    }

    return VALDI_UNLIKELY(!getLongConstructor().empty()) &&
           JSValueIsInstanceOfConstructor(
               getJSGlobalContext(), objectRef, fromValdiJSValue(getLongConstructor().get()).asObjectRef(), nullptr);
}

bool JavaScriptCoreContext::isValueObject(const Valdi::JSValue& value) {
    return fromValdiJSValue(value).type == kJSTypeObject;
}

bool JavaScriptCoreContext::isValueEqual(const Valdi::JSValue& left, const Valdi::JSValue& right) {
    return fromValdiJSValue(left).asObjectRef() == fromValdiJSValue(right).asObjectRef();
}

Valdi::ValueType JavaScriptCoreContext::getValueType(const Valdi::JSValue& value) {
    auto contextRef = getJSGlobalContext();
    auto jsCoreValue = fromValdiJSValue(value);
    switch (jsCoreValue.type) {
        case kJSTypeUndefined:
            return Valdi::ValueType::Undefined;
        case kJSTypeNull:
            return Valdi::ValueType::Null;
        case kJSTypeBoolean:
            return Valdi::ValueType::Bool;
        case kJSTypeNumber:
            return Valdi::ValueType::Double;
        case kJSTypeString:
            return Valdi::ValueType::StaticString;
        case kJSTypeObject: {
            if (JSValueIsArray(contextRef, jsCoreValue.valueRef)) {
                return Valdi::ValueType::Array;
            }

            if (JSValueIsObjectOfClass(contextRef, jsCoreValue.valueRef, getWrappedObjectClassRef())) {
                return Valdi::ValueType::ValdiObject;
            }

            if (JSObjectIsFunction(contextRef, jsCoreValue.asObjectRef())) {
                return Valdi::ValueType::Function;
            }

            auto typedArrayType = JSValueGetTypedArrayType(contextRef, jsCoreValue.valueRef, nullptr);
            if (typedArrayType != kJSTypedArrayTypeNone) {
                return Valdi::ValueType::TypedArray;
            }

            if (_errorConstructor != nullptr &&
                JSValueIsInstanceOfConstructor(contextRef, jsCoreValue.valueRef, _errorConstructor, nullptr)) {
                return Valdi::ValueType::Error;
            }

            if (VALDI_UNLIKELY(!getLongConstructor().empty()) &&
                JSValueIsInstanceOfConstructor(contextRef,
                                               jsCoreValue.valueRef,
                                               fromValdiJSValue(getLongConstructor().get()).asObjectRef(),
                                               nullptr)) {
                return Valdi::ValueType::Long;
            }

            // Regular object
            return Valdi::ValueType::Map;
        }
        case kJSTypeSymbol:
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000 || (TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_15_0)
        case kJSTypeBigInt:
#endif
            return Valdi::ValueType::Undefined;
    }
}

Valdi::JSValueRef JavaScriptCoreContext::callObjectAsFunction(const Valdi::JSValue& object,
                                                              Valdi::JSFunctionCallContext& callContext) {
    auto functionObject = fromValdiJSValue(object).asObjectRefOrThrow(*this, callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return Valdi::JSValueRef();
    }

    JSValueRef arguments[callContext.getParameterSize()];
    for (size_t i = 0; i < callContext.getParameterSize(); i++) {
        arguments[i] = fromValdiJSValue(callContext.getParameter(i)).valueRef;
    }

    JSObjectRef thisObject = fromValdiJSValue(callContext.getThisValue()).asObjectRef();

    JSValueRef exception = nullptr;
    auto result = JSObjectCallAsFunction(
        getJSGlobalContext(), functionObject, thisObject, callContext.getParameterSize(), &arguments[0], &exception);

    return checkCallAndGetValue(callContext.getExceptionTracker(), result, exception);
}

Valdi::JSValueRef JavaScriptCoreContext::callObjectAsConstructor(const Valdi::JSValue& object,
                                                                 Valdi::JSFunctionCallContext& callContext) {
    auto functionObject = fromValdiJSValue(object).asObjectRefOrThrow(*this, callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return Valdi::JSValueRef();
    }

    JSValueRef arguments[callContext.getParameterSize()];
    for (size_t i = 0; i < callContext.getParameterSize(); i++) {
        arguments[i] = fromValdiJSValue(callContext.getParameter(i)).valueRef;
    }

    JSValueRef exception = nullptr;
    auto result = JSObjectCallAsConstructor(
        getJSGlobalContext(), functionObject, callContext.getParameterSize(), arguments, &exception);
    return checkCallAndGetValue(callContext.getExceptionTracker(), result, exception);
}

Valdi::JSValueRef JavaScriptCoreContext::getObjectProperty(const Valdi::JSValue& object,
                                                           const std::string_view& propertyName,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto jsPropertyName = newPropertyName(propertyName);

    return getObjectProperty(object, jsPropertyName.get(), exceptionTracker);
}

Valdi::JSValueRef JavaScriptCoreContext::getObjectProperty(const Valdi::JSValue& object,
                                                           const Valdi::JSPropertyName& propertyName,
                                                           Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(object).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    JSValueRef exception = nullptr;
    auto property =
        JSObjectGetProperty(getJSGlobalContext(), objectRef, fromValdiJSPropertyName(propertyName), &exception);
    return checkCallAndGetValue(exceptionTracker, property, exception);
}

Valdi::JSValueRef JavaScriptCoreContext::getObjectPropertyForIndex(const Valdi::JSValue& object,
                                                                   size_t index,
                                                                   Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(object).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return Valdi::JSValueRef();
    }

    JSValueRef exception = nullptr;
    auto value =
        JSObjectGetPropertyAtIndex(getJSGlobalContext(), objectRef, static_cast<unsigned int>(index), &exception);
    return checkCallAndGetValue(exceptionTracker, value, exception);
}

bool JavaScriptCoreContext::hasObjectProperty(const Valdi::JSValue& object, const Valdi::JSPropertyName& propertyName) {
    auto objectRef = fromValdiJSValue(object).asObjectRef();
    if (objectRef == nullptr) {
        return false;
    }

    return JSObjectHasProperty(getJSGlobalContext(), objectRef, fromValdiJSPropertyName(propertyName));
}

void JavaScriptCoreContext::setObjectProperty(const Valdi::JSValue& object,
                                              const std::string_view& propertyName,
                                              const Valdi::JSValue& propertyValue,
                                              bool enumerable,
                                              Valdi::JSExceptionTracker& exceptionTracker) {
    auto jsPropertyName = newPropertyName(propertyName);

    setObjectProperty(object, jsPropertyName.get(), propertyValue, enumerable, exceptionTracker);
}

inline static JSPropertyAttributes toPropertyAttributes(bool enumerable) {
    if (enumerable) {
        return kJSPropertyAttributeNone;
    } else {
        return kJSPropertyAttributeDontEnum;
    }
}

void JavaScriptCoreContext::setObjectProperty(const Valdi::JSValue& object,
                                              const Valdi::JSPropertyName& propertyName,
                                              const Valdi::JSValue& propertyValue,
                                              bool enumerable,
                                              Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(object).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    JSValueRef exception = nullptr;
    JSObjectSetProperty(getJSGlobalContext(),
                        objectRef,
                        fromValdiJSPropertyName(propertyName),
                        fromValdiJSValue(propertyValue).valueRef,
                        toPropertyAttributes(enumerable),
                        &exception);

    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
    }
}

void JavaScriptCoreContext::setObjectProperty(const Valdi::JSValue& object,
                                              const Valdi::JSValue& propertyName,
                                              const Valdi::JSValue& propertyValue,
                                              bool enumerable,
                                              Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(object).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }
    auto propertyValueRef = fromValdiJSValue(propertyValue).valueRef;

    auto jsPropertyName = fromValdiJSValue(propertyName);
    JSValueRef exception = nullptr;

    if (__builtin_available(macOS 10.15, iOS 13.0, *)) {
        JSObjectSetPropertyForKey(getJSGlobalContext(),
                                  objectRef,
                                  jsPropertyName.valueRef,
                                  propertyValueRef,
                                  toPropertyAttributes(enumerable),
                                  &exception);
    } else {
        if (jsPropertyName.type == kJSTypeNumber) {
            auto index = valueToInt(propertyName, exceptionTracker);
            if (!exceptionTracker) {
                return;
            }
            setObjectPropertyIndex(object, index, propertyValue, exceptionTracker);
        } else {
            auto stringRef = JSValueToStringCopy(getJSGlobalContext(), jsPropertyName.valueRef, &exception);
            if (exception != nullptr) {
                storeException(exceptionTracker, exception);
                return;
            }

            JSObjectSetProperty(getJSGlobalContext(),
                                objectRef,
                                stringRef,
                                propertyValueRef,
                                toPropertyAttributes(enumerable),
                                &exception);

            JSStringRelease(stringRef);
        }
    }

    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
    }
}

void JavaScriptCoreContext::setObjectPropertyIndex(const Valdi::JSValue& object,
                                                   size_t index,
                                                   const Valdi::JSValue& value,
                                                   Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(object).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    JSValueRef exception = nullptr;
    JSObjectSetPropertyAtIndex(getJSGlobalContext(),
                               objectRef,
                               static_cast<unsigned int>(index),
                               fromValdiJSValue(value).valueRef,
                               &exception);
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
    }
}

void JavaScriptCoreContext::visitObjectPropertyNames(const Valdi::JSValue& object,
                                                     Valdi::JSExceptionTracker& exceptionTracker,
                                                     Valdi::IJavaScriptPropertyNamesVisitor& visitor) {
    auto objectRef = fromValdiJSValue(object).asObjectRefOrThrow(*this, exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto propertyNames = JSObjectCopyPropertyNames(getJSGlobalContext(), objectRef);

    auto count = JSPropertyNameArrayGetCount(propertyNames);
    bool shouldContinue = true;
    for (size_t i = 0; i < count && shouldContinue && exceptionTracker; i++) {
        auto propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        shouldContinue =
            visitor.visitPropertyName(*this, object, toValdiJSPropertyName(propertyName), exceptionTracker);
    }

    JSPropertyNameArrayRelease(propertyNames);
}

void JavaScriptCoreContext::startDebugger([[maybe_unused]] bool isWorker) {
#if !TARGET_OS_IPHONE && !TARGET_OS_OSX
    auto taskScheduler = getTaskScheduler();
    if (taskScheduler != nullptr) {
        JSRunLoopSetScheduleCallback(taskScheduler, [](void* opaque, uint32_t delayMs) {
            auto taskScheduler = reinterpret_cast<Valdi::JavaScriptTaskScheduler*>(opaque);
            taskScheduler->dispatchOnJsThreadAsyncAfter(nullptr, delayMs, [](auto& /*jsEntry*/) { JSRunLoopFlush(); });
        });
    }

    static std::atomic_bool kRemoteInspectorStarted = false;

    auto* debugger = DebuggerProxy::start();
    if (debugger != nullptr) {
        if (!kRemoteInspectorStarted.exchange(true)) {
            JSRemoteInspectorStart(debugger->getInternalPort());
        }
        _debuggerPort = debugger->getExternalPort();
    }
#endif
}

Valdi::JSPropertyNameRef JavaScriptCoreContext::newPropertyName(const std::string_view& str) {
    auto strSize = str.size();

    /**
     JSCore uses strlen() in its JSStringCreateWithUTF8CString API, so we need to make sure we have a null terminated
     string.
     */

    Valdi::SmallVector<char, 1024> buf;
    buf.resize(strSize + 1);

    auto* data = buf.data();
    std::memcpy(data, str.data(), strSize);
    data[strSize] = '\0';

    auto out = JSStringCreateWithUTF8CString(data);

    return Valdi::JSPropertyNameRef(this, toValdiJSPropertyName(out), true);
}

Valdi::StringBox JavaScriptCoreContext::propertyNameToString(const Valdi::JSPropertyName& propertyName) {
    return jsStringtoStdString(fromValdiJSPropertyName(propertyName));
}

void JavaScriptCoreContext::retainValue(const Valdi::JSValue& value) {
    JSValueProtect(getJSGlobalContext(), fromValdiJSValue(value).valueRef);
}

void JavaScriptCoreContext::releaseValue(const Valdi::JSValue& value) {
    if (getJSGlobalContext() != nullptr) {
        JSValueUnprotect(getJSGlobalContext(), fromValdiJSValue(value).valueRef);
    }
    _needGarbageCollect = true;
}

void JavaScriptCoreContext::retainPropertyName(const Valdi::JSPropertyName& value) {
    JSStringRetain(fromValdiJSPropertyName(value));
}

void JavaScriptCoreContext::releasePropertyName(const Valdi::JSPropertyName& value) {
    JSStringRelease(fromValdiJSPropertyName(value));
}

void JavaScriptCoreContext::enqueueMicrotask(const Valdi::JSValue& value, Valdi::JSExceptionTracker& exceptionTracker) {
    auto objectRef = fromValdiJSValue(value).asObjectRefOrThrow(*this, exceptionTracker);
    if (objectRef == nullptr) {
        return;
    }

    JSValueProtect(_globalContext, objectRef);
    _microtasks.emplace_back(objectRef);
}

void JavaScriptCoreContext::willEnterVM() {
    _enterVmCount++;
}

void JavaScriptCoreContext::willExitVM(Valdi::JSExceptionTracker& exceptionTracker) {
    auto enterVMCount = --_enterVmCount;
    if (enterVMCount == 0) {
        while (!_microtasks.empty()) {
            auto microtask = _microtasks.front();
            _microtasks.pop_front();

            JSValueRef exception = nullptr;
            JSObjectCallAsFunction(getJSGlobalContext(), microtask, nullptr, 0, nullptr, &exception);

            JSValueUnprotect(_globalContext, microtask);

            if (exception != nullptr) {
                storeException(exceptionTracker, exception);
                return;
            }
        }
    }
}

inline Valdi::JSValueRef JavaScriptCoreContext::toUnretainedJSValueRef(const JSValueRef& value, JSType type) {
    return Valdi::JSValueRef(nullptr, toValdiJSValue(JSCoreRef(value, type)), false);
}

inline Valdi::JSValueRef JavaScriptCoreContext::toRetainedJSValueRef(const JSValueRef& value, JSType type) {
    return Valdi::JSValueRef::makeRetained(*this, toValdiJSValue(JSCoreRef(value, type)));
}

Valdi::JSValueRef JavaScriptCoreContext::returnJSValueRef(const JSValueRef& value, JSType type) {
    if (Valdi::forceRetainJsObjects) {
        return toRetainedJSValueRef(value, type);
    } else {
        return toUnretainedJSValueRef(value, type);
    }
}

Valdi::JSValueRef JavaScriptCoreContext::checkCallAndGetValue(Valdi::JSExceptionTracker& exceptionTracker,
                                                              const JSValueRef& value,
                                                              const JSValueRef& exception) {
    if (exception != nullptr) {
        storeException(exceptionTracker, exception);
        return newUndefined();
    }

    return returnJSValueRef(value, JSValueGetType(getJSGlobalContext(), value));
}

void JavaScriptCoreContext::storeException(Valdi::JSExceptionTracker& exceptionTracker, const JSValueRef& exception) {
    exceptionTracker.storeException(
        toValdiJSValue(JSCoreRef(exception, JSValueGetType(getJSGlobalContext(), exception))));
}

} // namespace ValdiJSCore
