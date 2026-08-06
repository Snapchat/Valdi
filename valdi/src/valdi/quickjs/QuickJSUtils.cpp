//
//  QuickJSUtils.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/19/19.
//

#include "valdi/quickjs/QuickJSUtils.hpp"

#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi/quickjs/QuickJSJavaScriptContext.hpp"

#include <mutex>

namespace ValdiQuickJS {

static JSValue onJsCallError(JSContext* context, Valdi::JSExceptionTracker& exceptionTracker) {
    auto exception = exceptionTracker.getExceptionAndClear();
    return JS_Throw(context, JS_DupValue(context, fromValdiJSValue(exception.get())));
}

NativeClassFunctionData::NativeClassFunctionData(const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass,
                                                 const Valdi::StringBox& name)
    : nativeClass(nativeClass),
      referenceInfo(nativeClass->makeMemberReferenceInfo(name)) {}

static JSValue callNativeClassInstanceMember(JSContext* context,
                                             JSValueConst funcObject,
                                             JSValueConst thisValue,
                                             int argc,
                                             JSValueConst* argv,
                                             int /*flags*/) {
    auto& valdiJsContext = *getValdiJSContext(context);
    auto* opaque = JS_GetOpaque(funcObject, getNativeClassInstanceMemberClassDef()->classID);
    auto functionData = Valdi::unsafeBridge<NativeClassFunctionData>(opaque);

    Valdi::JSValueRef arguments[argc];
    for (int i = 0; i < argc; i++) {
        arguments[i] = Valdi::JSValueRef::makeUnretained(valdiJsContext, toValdiJSValue(argv[i]));
    }

    Valdi::JSExceptionTracker exceptionTracker(valdiJsContext);
    Valdi::JSFunctionNativeCallContext callContext(
        valdiJsContext, arguments, static_cast<size_t>(argc), exceptionTracker, functionData->referenceInfo);
    callContext.setThisValue(toValdiJSValue(thisValue));

    auto wrappedObject = getObjectWrappedObject(thisValue);
    auto* instanceData = dynamic_cast<Valdi::JSNativeClassInstanceData*>(wrappedObject.get());
    if (instanceData == nullptr || instanceData->getNativeClass() != functionData->nativeClass) {
        return JS_ThrowTypeError(context, "Native class member called with an incompatible receiver");
    }

    if (valdiJsContext.interruptRequested()) {
        valdiJsContext.onInterrupt();
    }

    auto result = functionData->callback(instanceData->getOpaque().get(), callContext);
    if (VALDI_LIKELY(exceptionTracker)) {
        return JS_DupValue(context, fromValdiJSValue(result.get()));
    }
    return onJsCallError(context, exceptionTracker);
}

static JSValue callNativeClassStaticMember(JSContext* context,
                                           JSValueConst funcObject,
                                           JSValueConst thisValue,
                                           int argc,
                                           JSValueConst* argv,
                                           int /*flags*/) {
    auto& valdiJsContext = *getValdiJSContext(context);
    auto* opaque = JS_GetOpaque(funcObject, getNativeClassStaticMemberClassDef()->classID);
    auto functionData = Valdi::unsafeBridge<NativeClassFunctionData>(opaque);

    Valdi::JSValueRef arguments[argc];
    for (int i = 0; i < argc; i++) {
        arguments[i] = Valdi::JSValueRef::makeUnretained(valdiJsContext, toValdiJSValue(argv[i]));
    }

    Valdi::JSExceptionTracker exceptionTracker(valdiJsContext);
    Valdi::JSFunctionNativeCallContext callContext(
        valdiJsContext, arguments, static_cast<size_t>(argc), exceptionTracker, functionData->referenceInfo);
    callContext.setThisValue(toValdiJSValue(thisValue));

    if (valdiJsContext.interruptRequested()) {
        valdiJsContext.onInterrupt();
    }

    auto result = functionData->callback(functionData->nativeClass->getOpaque().get(), callContext);
    if (VALDI_LIKELY(exceptionTracker)) {
        return JS_DupValue(context, fromValdiJSValue(result.get()));
    }
    return onJsCallError(context, exceptionTracker);
}

static JSValue callNativeClassConstructor(
    JSContext* context, JSValueConst funcObject, JSValueConst newTarget, int argc, JSValueConst* argv, int flags) {
    if ((flags & JS_CALL_FLAG_CONSTRUCTOR) == 0) {
        return JS_ThrowTypeError(context, "Native class constructor must be called with new");
    }
    if (JS_VALUE_GET_PTR(funcObject) != JS_VALUE_GET_PTR(newTarget)) {
        return JS_ThrowTypeError(context, "Native class subclassing is not supported");
    }

    auto& valdiJsContext = *getValdiJSContext(context);
    auto nativeClass = getNativeClassConstructorData(funcObject);
    auto constructor = nativeClass->getConstructor();
    if (constructor == nullptr) {
        return JS_ThrowTypeError(context, "Native class cannot be constructed from JavaScript");
    }

    Valdi::JSValueRef arguments[argc];
    for (int i = 0; i < argc; i++) {
        arguments[i] = Valdi::JSValueRef::makeUnretained(valdiJsContext, toValdiJSValue(argv[i]));
    }

    Valdi::JSExceptionTracker exceptionTracker(valdiJsContext);
    const auto& referenceInfo = nativeClass->getConstructorReferenceInfo();
    Valdi::JSFunctionNativeCallContext callContext(
        valdiJsContext, arguments, static_cast<size_t>(argc), exceptionTracker, referenceInfo);

    auto prototype = JS_GetPropertyStr(context, funcObject, "prototype");
    if (JS_IsException(prototype)) {
        return prototype;
    }
    auto object = JS_NewObjectProtoClass(context, prototype, getWrappedObjectClassDef()->classID);
    JS_FreeValue(context, prototype);
    if (JS_IsException(object)) {
        return object;
    }
    callContext.setThisValue(toValdiJSValue(object));

    if (valdiJsContext.interruptRequested()) {
        valdiJsContext.onInterrupt();
    }

    auto nativeOpaque = constructor(nativeClass->getOpaque().get(), callContext);
    if (exceptionTracker && nativeOpaque == nullptr) {
        exceptionTracker.onError("Native class constructor returned a null opaque object");
    }

    if (!exceptionTracker) {
        JS_FreeValue(context, object);
        return onJsCallError(context, exceptionTracker);
    }

    auto instanceData = Valdi::makeShared<Valdi::JSNativeClassInstanceData>(nativeClass, nativeOpaque);
    setObjectWrappedObject(object, instanceData);
    return object;
}

JSValue jsCall(
    JSContext* context, JSValueConst funcObject, JSValueConst thisValue, int argc, JSValueConst* argv, int /*flags*/) {
    auto& valdiJsContext = *getValdiJSContext(context);
    auto* function = getObjectCallable(funcObject);

    // Note: for even faster call performance, we could have Valdi::JSValue being the exact
    // same size as QuickJS's JSValue, and just pass the argv pointer directly.
    Valdi::JSValueRef arguments[argc];
    for (int i = 0; i < argc; i++) {
        arguments[i] = Valdi::JSValueRef::makeUnretained(valdiJsContext, toValdiJSValue(argv[i]));
    }

    Valdi::JSExceptionTracker exceptionTracker(valdiJsContext);
    Valdi::JSFunctionNativeCallContext callContext(
        valdiJsContext, arguments, static_cast<size_t>(argc), exceptionTracker, function->getReferenceInfo());
    callContext.setThisValue(toValdiJSValue(thisValue));

    if (valdiJsContext.interruptRequested()) {
        valdiJsContext.onInterrupt();
    }

    auto result = (*function)(callContext);

    if (VALDI_LIKELY(exceptionTracker)) {
        return JS_DupValue(context, fromValdiJSValue(result.get()));
    } else {
        return onJsCallError(context, exceptionTracker);
    }
}

void jsWrappedObjectFinalize(JSRuntime* /*tr*/, JSValue value) {
    setObjectWrappedObject(value, nullptr);
}

void jsFunctionFinalize(JSRuntime* /*tr*/, JSValue value) {
    setObjectCallable(value, nullptr);
}

void jsWeakRefFinalizerFinalize(JSRuntime* tr, JSValue value) {
    auto* jsContext = getValdiJSContext(tr);
    if (jsContext != nullptr) {
        auto weakReferenceId = weakReferenceIdFromJSWeakReferenceFinalizer(value);
        jsContext->removeWeakReference(weakReferenceId);
    }
}

JSClassID newClassID() {
    static auto* kMutex = new std::mutex();

    auto lockGuard = std::lock_guard<std::mutex>(*kMutex);

    JSClassID classID = 0;
    return JS_NewClassID(&classID);
}

JSClassDefWithId* newClassDefWithId(const char* className) {
    auto* classDefWithId = new JSClassDefWithId();
    std::memset(classDefWithId, 0, sizeof(JSClassDefWithId));

    classDefWithId->classID = newClassID();
    classDefWithId->classDef.class_name = className;

    return classDefWithId;
}

JSClassDefWithId* makeBridgedFunctionClassDef() {
    auto* classDefWithId = newClassDefWithId("NativeBridgedFunction");

    classDefWithId->classDef.call = &jsCall;
    classDefWithId->classDef.finalizer = &jsFunctionFinalize;

    return classDefWithId;
}

const JSClassDefWithId* getBridgedFunctionClassDef() {
    static auto* kClassDef = makeBridgedFunctionClassDef();

    return kClassDef;
}

JSClassDefWithId* makeWrappedObjectClassDef() {
    auto* classDefWithId = newClassDefWithId("WrappedNativeObject");

    classDefWithId->classDef.finalizer = &jsWrappedObjectFinalize;

    return classDefWithId;
}

const JSClassDefWithId* getWrappedObjectClassDef() {
    static auto* kClassDef = makeWrappedObjectClassDef();

    return kClassDef;
}

JSClassDefWithId* makeWeakRefFinalizerClassDef() {
    auto* classDefWithId = newClassDefWithId("WeakRefFinalizer");

    classDefWithId->classDef.finalizer = &jsWeakRefFinalizerFinalize;

    return classDefWithId;
}

void nativeClassConstructorFinalize(JSRuntime* /*rt*/, JSValue value) {
    setNativeClassConstructorData(value, nullptr);
}

static void releaseNativeClassFunctionData(const JSValue& value, const JSClassDefWithId* classDef) {
    auto* opaque = JS_GetOpaque(value, classDef->classID);
    Valdi::RefCountableAutoreleasePool::release(opaque);
}

void nativeClassInstanceMemberFinalize(JSRuntime* /*rt*/, JSValue value) {
    releaseNativeClassFunctionData(value, getNativeClassInstanceMemberClassDef());
}

void nativeClassStaticMemberFinalize(JSRuntime* /*rt*/, JSValue value) {
    releaseNativeClassFunctionData(value, getNativeClassStaticMemberClassDef());
}

JSClassDefWithId* makeNativeClassConstructorClassDef() {
    auto* classDefWithId = newClassDefWithId("NativeClassConstructor");
    classDefWithId->classDef.call = &callNativeClassConstructor;
    classDefWithId->classDef.finalizer = &nativeClassConstructorFinalize;
    return classDefWithId;
}

const JSClassDefWithId* getNativeClassConstructorClassDef() {
    static auto* kClassDef = makeNativeClassConstructorClassDef();
    return kClassDef;
}

JSClassDefWithId* makeNativeClassFunctionClassDef(const char* name,
                                                 JSClassCall* call,
                                                 JSClassFinalizer* finalizer) {
    auto* classDefWithId = newClassDefWithId(name);
    classDefWithId->classDef.call = call;
    classDefWithId->classDef.finalizer = finalizer;
    return classDefWithId;
}

const JSClassDefWithId* getNativeClassInstanceMemberClassDef() {
    static auto* kClassDef = makeNativeClassFunctionClassDef(
        "NativeClassInstanceMember", &callNativeClassInstanceMember, &nativeClassInstanceMemberFinalize);
    return kClassDef;
}

const JSClassDefWithId* getNativeClassStaticMemberClassDef() {
    static auto* kClassDef = makeNativeClassFunctionClassDef(
        "NativeClassStaticMember", &callNativeClassStaticMember, &nativeClassStaticMemberFinalize);
    return kClassDef;
}

const JSClassDefWithId* getWeakRefFinalizerClassDef() {
    static auto* kClassDef = makeWeakRefFinalizerClassDef();

    return kClassDef;
}

void setObjectWrappedObject(const JSValue& value, const Valdi::Ref<Valdi::RefCountable>& wrappedObject) {
    auto* opaque = JS_GetOpaque(value, getWrappedObjectClassDef()->classID);
    if (opaque != nullptr) {
        Valdi::RefCountableAutoreleasePool::release(opaque);
    }

    JS_SetOpaque(value, Valdi::unsafeBridgeRetain(wrappedObject.get()));
}

Valdi::Ref<Valdi::RefCountable> getObjectWrappedObject(const JSValue& value) {
    auto* opaque = JS_GetOpaque(value, getWrappedObjectClassDef()->classID);

    return Valdi::unsafeBridge<Valdi::RefCountable>(opaque);
}

void setNativeClassConstructorData(const JSValue& value, const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass) {
    auto* opaque = JS_GetOpaque(value, getNativeClassConstructorClassDef()->classID);
    if (opaque != nullptr) {
        Valdi::RefCountableAutoreleasePool::release(opaque);
    }
    JS_SetOpaque(value, Valdi::unsafeBridgeRetain(nativeClass.get()));
}

Valdi::Ref<Valdi::JSNativeClassData> getNativeClassConstructorData(const JSValue& value) {
    auto* opaque = JS_GetOpaque(value, getNativeClassConstructorClassDef()->classID);
    return Valdi::unsafeBridge<Valdi::JSNativeClassData>(opaque);
}

void setNativeClassFunctionData(const JSValue& value, const Valdi::Ref<NativeClassFunctionData>& functionData) {
    JS_SetOpaque(value, Valdi::unsafeBridgeRetain(functionData.get()));
}

size_t weakReferenceIdFromJSWeakReferenceFinalizer(const JSValue& jsWeakReferenceFinalizer) {
    auto* opaque = JS_GetOpaque(jsWeakReferenceFinalizer, getWeakRefFinalizerClassDef()->classID);
    return reinterpret_cast<size_t>(opaque);
}

void setWeakReferenceIdToJSWeakReferenceFinalizer(const JSValue& jsWeakReferenceFinalizer, size_t weakReferenceId) {
    JS_SetOpaque(jsWeakReferenceFinalizer, reinterpret_cast<void*>(weakReferenceId));
}

} // namespace ValdiQuickJS
