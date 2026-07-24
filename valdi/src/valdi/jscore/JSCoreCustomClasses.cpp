//
//  JSCoreCustomClasses.cpp
//  ValdiIOS
//
//  Created by Simon Corsin on 5/31/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/jscore/JSCoreCustomClasses.hpp"
#include "valdi/jscore/JSCoreUtils.hpp"
#include "valdi/jscore/JavaScriptCoreContext.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/Utils/RefCountableAutoreleasePool.hpp"
#include "valdi_core/cpp/Constants.hpp"

namespace ValdiJSCore {

static JSValueRef onJsCallError(JSContextRef ctx,
                                Valdi::JSExceptionTracker& exceptionTracker,
                                JSValueRef* exceptionPtr) {
    auto exception = exceptionTracker.getExceptionAndClear();
    *exceptionPtr = fromValdiJSValue(exception.get()).valueRef;
    return JSValueMakeUndefined(ctx);
}

static JSObjectRef onJsConstructorError(Valdi::JSExceptionTracker& exceptionTracker, JSValueRef* exceptionPtr) {
    auto exception = exceptionTracker.getExceptionAndClear();
    *exceptionPtr = fromValdiJSValue(exception.get()).valueRef;
    return nullptr;
}

JSValueRef callAsFunction(JSContextRef ctx,
                          JSObjectRef function,
                          JSObjectRef thisObject,
                          size_t argumentCount,
                          const JSValueRef arguments[],
                          JSValueRef* exception) {
    auto* attachedJsFunctionData = getAttachedJsFunctionData(function);
    auto& jsContext = attachedJsFunctionData->jsContext;

    Valdi::JSValueRef outArguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        outArguments[i] = Valdi::JSValueRef::makeUnretained(
            jsContext, toValdiJSValue(JSCoreRef(arguments[i], JSValueGetType(ctx, arguments[i]))));
    }

    Valdi::JSExceptionTracker exceptionTracker(attachedJsFunctionData->jsContext);
    Valdi::JSFunctionNativeCallContext callContext(attachedJsFunctionData->jsContext,
                                                   outArguments,
                                                   static_cast<size_t>(argumentCount),
                                                   exceptionTracker,
                                                   attachedJsFunctionData->function->getReferenceInfo());
    callContext.setThisValue(toValdiJSValue(JSCoreRef(thisObject, kJSTypeObject)));

    if (attachedJsFunctionData->jsContext.interruptRequested()) {
        attachedJsFunctionData->jsContext.onInterrupt();
    }

    auto result = (*attachedJsFunctionData->function)(callContext);

    if (VALDI_LIKELY(exceptionTracker)) {
        return fromValdiJSValue(result.get()).valueRef;
    } else {
        return onJsCallError(ctx, exceptionTracker, exception);
    }
}

static JSValueRef callNativeClassAsFunction(JSContextRef ctx,
                                            JSObjectRef function,
                                            JSObjectRef /*thisObject*/,
                                            size_t /*argumentCount*/,
                                            const JSValueRef[] /*arguments*/,
                                            JSValueRef* exception) {
    auto functionData = Valdi::Ref(getAttachedNativeClassFunctionData(function));
    auto& jsContext = functionData->jsContext;
    Valdi::JSExceptionTracker exceptionTracker(jsContext);
    exceptionTracker.onError("Native class constructor must be called with new");
    return onJsCallError(ctx, exceptionTracker, exception);
}

static JSObjectRef callNativeClassConstructor(JSContextRef ctx,
                                              JSObjectRef constructorObject,
                                              size_t argumentCount,
                                              const JSValueRef arguments[],
                                              JSValueRef* exception) {
    auto functionData = Valdi::Ref(getAttachedNativeClassFunctionData(constructorObject));
    auto& jsContext = functionData->jsContext;
    Valdi::JSExceptionTracker exceptionTracker(jsContext);

    Valdi::JSValueRef outArguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        outArguments[i] = Valdi::JSValueRef::makeUnretained(
            jsContext, toValdiJSValue(JSCoreRef(arguments[i], JSValueGetType(ctx, arguments[i]))));
    }

    JSValueRef jsException = nullptr;
    auto prototypeValue = JSObjectGetProperty(
        ctx, constructorObject, fromValdiJSPropertyName(jsContext.getPrototypePropertyName()), &jsException);
    if (jsException != nullptr) {
        exceptionTracker.storeException(Valdi::JSValueRef::makeRetained(
            jsContext, toValdiJSValue(JSCoreRef(jsException, JSValueGetType(ctx, jsException)))));
        return onJsConstructorError(exceptionTracker, exception);
    }
    auto prototypeObject = JSValueToObject(ctx, prototypeValue, &jsException);
    if (jsException != nullptr) {
        exceptionTracker.storeException(Valdi::JSValueRef::makeRetained(
            jsContext, toValdiJSValue(JSCoreRef(jsException, JSValueGetType(ctx, jsException)))));
        return onJsConstructorError(exceptionTracker, exception);
    }

    auto object = JSObjectMake(ctx, getWrappedObjectClassRef(), nullptr);
    JSObjectSetPrototype(ctx, object, prototypeObject);

    Valdi::JSFunctionNativeCallContext callContext(jsContext,
                                                   argumentCount == 0 ? nullptr : outArguments,
                                                   argumentCount,
                                                   exceptionTracker,
                                                   functionData->referenceInfo);
    callContext.setThisValue(toValdiJSValue(JSCoreRef(object, kJSTypeObject)));

    auto constructor = functionData->nativeClass->getConstructor();
    if (constructor == nullptr) {
        auto error =
            jsContext.newError("Native class cannot be constructed from JavaScript", std::nullopt, exceptionTracker);
        if (exceptionTracker) {
            exceptionTracker.storeException(std::move(error));
        }
        return onJsConstructorError(exceptionTracker, exception);
    }
    if (jsContext.interruptRequested()) {
        jsContext.onInterrupt();
    }

    auto nativeOpaque = constructor(functionData->nativeClass->getOpaque().get(), callContext);
    if (exceptionTracker && nativeOpaque == nullptr) {
        exceptionTracker.onError("Native class constructor returned a null opaque object");
    }
    if (!exceptionTracker) {
        return onJsConstructorError(exceptionTracker, exception);
    }

    auto instanceData = Valdi::makeShared<Valdi::JSNativeClassInstanceData>(functionData->nativeClass, nativeOpaque);
    auto* retainedInstanceData = Valdi::unsafeBridgeRetain(instanceData.get());
    if (!JSObjectSetPrivate(object, retainedInstanceData)) {
        Valdi::RefCountableAutoreleasePool::release(retainedInstanceData);
        exceptionTracker.onError("Failed to attach native class instance data");
        return onJsConstructorError(exceptionTracker, exception);
    }

    return object;
}

static bool nativeClassHasInstance(JSContextRef ctx,
                                   JSObjectRef constructorObject,
                                   JSValueRef possibleInstance,
                                   JSValueRef* exception) {
    if (!JSValueIsObject(ctx, possibleInstance)) {
        return false;
    }

    auto functionData = Valdi::Ref(getAttachedNativeClassFunctionData(constructorObject));
    auto prototype = JSObjectGetProperty(
        ctx,
        constructorObject,
        fromValdiJSPropertyName(functionData->jsContext.getPrototypePropertyName()),
        exception);
    if (*exception != nullptr) {
        return false;
    }

    auto current = JSValueToObject(ctx, possibleInstance, exception);
    if (*exception != nullptr) {
        return false;
    }

    while (true) {
        auto parent = JSObjectGetPrototype(ctx, current);
        if (JSValueIsStrictEqual(ctx, parent, prototype)) {
            return true;
        }
        if (!JSValueIsObject(ctx, parent)) {
            return false;
        }
        current = JSValueToObject(ctx, parent, exception);
        if (*exception != nullptr) {
            return false;
        }
    }
}

static JSValueRef callNativeClassInstanceMember(JSContextRef ctx,
                                                JSObjectRef function,
                                                JSObjectRef thisObject,
                                                size_t argumentCount,
                                                const JSValueRef arguments[],
                                                JSValueRef* exception) {
    auto functionData = Valdi::Ref(getAttachedNativeClassFunctionData(function));
    auto& jsContext = functionData->jsContext;

    Valdi::JSValueRef outArguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        outArguments[i] = Valdi::JSValueRef::makeUnretained(
            jsContext, toValdiJSValue(JSCoreRef(arguments[i], JSValueGetType(ctx, arguments[i]))));
    }

    Valdi::JSExceptionTracker exceptionTracker(jsContext);
    Valdi::JSFunctionNativeCallContext callContext(jsContext,
                                                   argumentCount == 0 ? nullptr : outArguments,
                                                   argumentCount,
                                                   exceptionTracker,
                                                   functionData->referenceInfo);
    callContext.setThisValue(toValdiJSValue(JSCoreRef(thisObject, kJSTypeObject)));

    auto wrappedObject = Valdi::Ref(getAttachedWrappedObject(thisObject));
    auto instanceData = Valdi::Ref(dynamic_cast<Valdi::JSNativeClassInstanceData*>(wrappedObject.get()));
    if (instanceData == nullptr || instanceData->getNativeClass() != functionData->nativeClass) {
        auto error = jsContext.newError(
            "Native class member called with an incompatible receiver", std::nullopt, exceptionTracker);
        if (exceptionTracker) {
            exceptionTracker.storeException(std::move(error));
        }
        return onJsCallError(ctx, exceptionTracker, exception);
    }

    if (jsContext.interruptRequested()) {
        jsContext.onInterrupt();
    }

    auto result = functionData->callback(instanceData->getOpaque().get(), callContext);
    if (VALDI_LIKELY(exceptionTracker)) {
        return fromValdiJSValue(result.get()).valueRef;
    }
    return onJsCallError(ctx, exceptionTracker, exception);
}

static JSValueRef callNativeClassStaticMember(JSContextRef ctx,
                                              JSObjectRef function,
                                              JSObjectRef thisObject,
                                              size_t argumentCount,
                                              const JSValueRef arguments[],
                                              JSValueRef* exception) {
    auto functionData = Valdi::Ref(getAttachedNativeClassFunctionData(function));
    auto& jsContext = functionData->jsContext;

    Valdi::JSValueRef outArguments[argumentCount];
    for (size_t i = 0; i < argumentCount; i++) {
        outArguments[i] = Valdi::JSValueRef::makeUnretained(
            jsContext, toValdiJSValue(JSCoreRef(arguments[i], JSValueGetType(ctx, arguments[i]))));
    }

    Valdi::JSExceptionTracker exceptionTracker(jsContext);
    Valdi::JSFunctionNativeCallContext callContext(jsContext,
                                                   argumentCount == 0 ? nullptr : outArguments,
                                                   argumentCount,
                                                   exceptionTracker,
                                                   functionData->referenceInfo);
    callContext.setThisValue(toValdiJSValue(JSCoreRef(thisObject, kJSTypeObject)));

    if (jsContext.interruptRequested()) {
        jsContext.onInterrupt();
    }

    auto result = functionData->callback(functionData->nativeClass->getOpaque().get(), callContext);
    if (VALDI_LIKELY(exceptionTracker)) {
        return fromValdiJSValue(result.get()).valueRef;
    }
    return onJsCallError(ctx, exceptionTracker, exception);
}

void finalize(JSObjectRef object) {
    Valdi::RefCountableAutoreleasePool::release(JSObjectGetPrivate(object));
}

JSClassRef getNativeFunctionClassRef() {
    static JSClassRef _nativeFunctionClassRef = nullptr;
    if (_nativeFunctionClassRef == nullptr) {
        auto classDefinition = kJSClassDefinitionEmpty;
        classDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
        classDefinition.callAsFunction = &callAsFunction;
        classDefinition.finalize = &finalize;

        _nativeFunctionClassRef = JSClassCreate(&classDefinition);
    }

    return _nativeFunctionClassRef;
}

JSClassRef getWrappedObjectClassRef() {
    static JSClassRef _wrappedObjectClassRef = nullptr;
    if (_wrappedObjectClassRef == nullptr) {
        auto wrappedObjectClsDefinition = kJSClassDefinitionEmpty;
        wrappedObjectClsDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
        wrappedObjectClsDefinition.finalize = &finalize;

        _wrappedObjectClassRef = JSClassCreate(&wrappedObjectClsDefinition);
    }

    return _wrappedObjectClassRef;
}

static JSClassRef makeNativeClassFunctionClassRef(JSObjectCallAsFunctionCallback callback) {
    auto classDefinition = kJSClassDefinitionEmpty;
    classDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
    classDefinition.callAsFunction = callback;
    classDefinition.finalize = &finalize;
    return JSClassCreate(&classDefinition);
}

JSClassRef JavaScriptCoreContext::getNativeClassConstructorClassRef() {
    static const auto classRef = [] {
        auto classDefinition = kJSClassDefinitionEmpty;
        classDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
        classDefinition.callAsFunction = &callNativeClassAsFunction;
        classDefinition.callAsConstructor = &callNativeClassConstructor;
        classDefinition.hasInstance = &nativeClassHasInstance;
        classDefinition.finalize = &finalize;
        return JSClassCreate(&classDefinition);
    }();
    return classRef;
}

JSClassRef getNativeClassInstanceMemberClassRef() {
    static auto classRef = makeNativeClassFunctionClassRef(&callNativeClassInstanceMember);
    return classRef;
}

JSClassRef getNativeClassStaticMemberClassRef() {
    static auto classRef = makeNativeClassFunctionClassRef(&callNativeClassStaticMember);
    return classRef;
}

JSFunctionData::JSFunctionData(JavaScriptCoreContext& jsContext, const Valdi::Ref<Valdi::JSFunction>& function)
    : jsContext(jsContext), function(function) {}
JSFunctionData::~JSFunctionData() = default;

NativeClassFunctionData::NativeClassFunctionData(JavaScriptCoreContext& jsContext,
                                                 const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass)
    : jsContext(jsContext),
      nativeClass(nativeClass),
      referenceInfo(nativeClass->getConstructorReferenceInfo()) {}

NativeClassFunctionData::NativeClassFunctionData(JavaScriptCoreContext& jsContext,
                                                 const Valdi::Ref<Valdi::JSNativeClassData>& nativeClass,
                                                 const Valdi::StringBox& name)
    : jsContext(jsContext),
      nativeClass(nativeClass),
      referenceInfo(nativeClass->makeMemberReferenceInfo(name)) {}

inline Valdi::RefCountable* getAttachedRefCountable(JSObjectRef objectRef) {
    return reinterpret_cast<Valdi::RefCountable*>(JSObjectGetPrivate(objectRef));
}

JSFunctionData* getAttachedJsFunctionData(JSObjectRef objectRef) {
    return dynamic_cast<JSFunctionData*>(getAttachedRefCountable(objectRef));
}

Valdi::RefCountable* getAttachedWrappedObject(JSObjectRef objectRef) {
    if (objectRef == nullptr) {
        return nullptr;
    }
    auto* refCountable = getAttachedRefCountable(objectRef);
    if (dynamic_cast<JSFunctionData*>(refCountable) != nullptr ||
        dynamic_cast<NativeClassFunctionData*>(refCountable) != nullptr) {
        // Function bridge metadata is not a wrapped object.
        return nullptr;
    }
    return refCountable;
}

NativeClassFunctionData* getAttachedNativeClassFunctionData(JSObjectRef objectRef) {
    return dynamic_cast<NativeClassFunctionData*>(getAttachedRefCountable(objectRef));
}

} // namespace ValdiJSCore
