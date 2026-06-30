//
//  JavaCache.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/14/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JNIConstants.hpp"
#include <vector>

#define IMPLEMENT_JAVA_CLASS(name, classpath)                                                                          \
    JavaClass& JavaCache::get##name##Class() { /*NOLINT*/                                                              \
        if (!name##Class) {                                                                                            \
            name##Class = {JavaClass::resolveOrAbort(JavaEnv(), classpath)};                                           \
        }                                                                                                              \
        return *name##Class;                                                                                           \
    }

#define IMPLEMENT_JAVA_METHOD(class, suffix, method, ...)                                                              \
    JavaMethod<__VA_ARGS__>& JavaCache::get##class##suffix##Method() { /*NOLINT*/                                      \
        static auto kMethod = JavaCache::get().get##class##Class().getMethod<__VA_ARGS__>(method);                     \
        return kMethod;                                                                                                \
    }

#define IMPLEMENT_JAVA_STATIC_METHOD(class, suffix, method, ...)                                                       \
    JavaStaticMethod<__VA_ARGS__>& JavaCache::get##class##suffix##Method() { /*NOLINT*/                                \
        static auto kMethod = JavaCache::get().get##class##Class().getStaticMethod<__VA_ARGS__>(method);               \
        return kMethod;                                                                                                \
    }

namespace ValdiAndroid {

JavaCache::JavaCache() {
    getClassClass(); // fun
    getExceptionClass();
    getThrowableClass();
    getSetClass();
    getMapClass();
    getHashMapClass();
    getObjectClass();
    getNumberClass();
    getStringClass();
    getIntegerClass();
    getLongClass();
    getDoubleClass();
    getFloatClass();
    getBooleanClass();
    getIterableClass();
    getIteratorClass();
    getObjectArrayClass();
    getByteArrayClass();
    getMapEntryClass();
    getWeakRefeferenceClass();
    getWeakHashMapClass();
    getValdiActionClass();
    getValdiMarshallerClass();
    getValdiMarshallerCPPClass();
    getValdiFunctionClass();
    getValdiMarshallableClass();
    getValdiFunctionNativeClass();
    getViewClass();
    getCppObjectWrapperClass();
    getNativeRefClass();
    getNativeHandleWrapperClass();
    getAutoDisposableClass();
    getUndefinedValueClass();
    getValdiExceptionClass();
    getValdiThreadClass();
    getValdiResultClass();
    getViewRefClass();
    getViewFactoryClass();
    getSurfacePresenterManagerClass();
    getBitmapClass();
    getBitmapHandlerClass();
    getValdiImageClass();
    getAssetClass();
    getPromiseClass();
    getPromiseCallbackClass();
    getCppPromiseCallbackClass();
    getCppPromiseClass();
}

JavaCache& JavaCache::get() {
    static auto* kCache = new JavaCache();
    return *kCache;
}

static GlobalRefJavaObject* resolveUndefinedValueClassInstance() {
    auto& clazz = JavaCache::get().getUndefinedValueClass();
    auto* instanceField = JavaEnv::accessEnvRet([&](JNIEnv& env) -> auto {
        return env.GetStaticFieldID(clazz.getClass(), "UNDEFINED", "Lcom/snapchat/client/valdi/UndefinedValue;");
    });
    auto* instance = JavaEnv::accessEnvRet(
        [&](JNIEnv& env) -> auto { return env.GetStaticObjectField(clazz.getClass(), instanceField); });
    return new GlobalRefJavaObject(JavaEnv(), instance, "UndefindValue", false);
}

JavaObject JavaCache::getUndefinedValueClassInstance() {
    static auto* kUndefinedValueClassInstance = resolveUndefinedValueClassInstance();
    return kUndefinedValueClassInstance->toObject();
}

static GlobalRefJavaObject* resolveUnitValueClassInstance() {
    auto& clazz = JavaCache::get().getUndefinedValueClass();
    auto* instanceField = JavaEnv::accessEnvRet(
        [&](JNIEnv& env) -> auto { return env.GetStaticFieldID(clazz.getClass(), "UNIT", "Ljava/lang/Object;"); });
    auto* instance = JavaEnv::accessEnvRet(
        [&](JNIEnv& env) -> auto { return env.GetStaticObjectField(clazz.getClass(), instanceField); });
    return new GlobalRefJavaObject(JavaEnv(), instance, "UnitValue", false);
}

JavaObject JavaCache::getUnitValueClassInstance() {
    static auto* kUnitValueClassInstance = resolveUnitValueClassInstance();
    return kUnitValueClassInstance->toObject();
}

IMPLEMENT_JAVA_CLASS(Class, "java/lang/Class")
IMPLEMENT_JAVA_METHOD(Class, GetName, "getName", String)

IMPLEMENT_JAVA_CLASS(Exception, "java/lang/Exception")

IMPLEMENT_JAVA_CLASS(Throwable, "java/lang/Throwable")
IMPLEMENT_JAVA_METHOD(Throwable, GetMessage, "getMessage", String)

IMPLEMENT_JAVA_CLASS(Set, "java/util/Set")
IMPLEMENT_JAVA_METHOD(Set, ToArray, "toArray", ObjectArray)

IMPLEMENT_JAVA_CLASS(Map, "java/util/Map")
IMPLEMENT_JAVA_METHOD(Map, EntrySet, "entrySet", SetType)
IMPLEMENT_JAVA_METHOD(Map, Size, "size", int32_t)

IMPLEMENT_JAVA_CLASS(HashMap, "java/util/HashMap")
IMPLEMENT_JAVA_METHOD(HashMap, Constructor, kJniSigConstructor, ConstructorType)
IMPLEMENT_JAVA_METHOD(HashMap, Put, "put", JavaObject, JavaObject, JavaObject)

IMPLEMENT_JAVA_CLASS(Object, "java/lang/Object")
IMPLEMENT_JAVA_METHOD(Object, ToString, "toString", String)

IMPLEMENT_JAVA_CLASS(Number, "java/lang/Number")
IMPLEMENT_JAVA_METHOD(Number, IntValue, "intValue", int32_t)
IMPLEMENT_JAVA_METHOD(Number, LongValue, "longValue", int64_t)
IMPLEMENT_JAVA_METHOD(Number, DoubleValue, "doubleValue", double)

IMPLEMENT_JAVA_CLASS(String, "java/lang/String")

IMPLEMENT_JAVA_CLASS(Integer, "java/lang/Integer")
IMPLEMENT_JAVA_METHOD(Integer, Constructor, kJniSigConstructor, ConstructorType, int32_t)

IMPLEMENT_JAVA_CLASS(Long, "java/lang/Long")
IMPLEMENT_JAVA_METHOD(Long, Constructor, kJniSigConstructor, ConstructorType, int64_t)

IMPLEMENT_JAVA_CLASS(Double, "java/lang/Double")
IMPLEMENT_JAVA_METHOD(Double, Constructor, kJniSigConstructor, ConstructorType, double)

IMPLEMENT_JAVA_CLASS(Float, "java/lang/Float")
IMPLEMENT_JAVA_METHOD(Float, Constructor, kJniSigConstructor, ConstructorType, float)

IMPLEMENT_JAVA_CLASS(Boolean, "java/lang/Boolean")
IMPLEMENT_JAVA_METHOD(Boolean, Constructor, kJniSigConstructor, ConstructorType, bool)
IMPLEMENT_JAVA_METHOD(Boolean, BooleanValue, "booleanValue", bool)

IMPLEMENT_JAVA_CLASS(Iterable, "java/lang/Iterable")
IMPLEMENT_JAVA_METHOD(Iterable, Iterator, "iterator", IteratorType)

IMPLEMENT_JAVA_CLASS(Iterator, "java/util/Iterator")
IMPLEMENT_JAVA_METHOD(Iterator, HasNext, "hasNext", bool)
IMPLEMENT_JAVA_METHOD(Iterator, Next, "next", JavaObject)

IMPLEMENT_JAVA_CLASS(ObjectArray, kJniSigObjectArray);
IMPLEMENT_JAVA_CLASS(ByteArray, kJniSigByteArray);

IMPLEMENT_JAVA_CLASS(MapEntry, "java/util/Map$Entry")
IMPLEMENT_JAVA_METHOD(MapEntry, GetKey, "getKey", JavaObject)
IMPLEMENT_JAVA_METHOD(MapEntry, GetValue, "getValue", JavaObject)

IMPLEMENT_JAVA_CLASS(WeakRefeference, "java/lang/ref/WeakReference")
IMPLEMENT_JAVA_METHOD(WeakRefeference, Constructor, kJniSigConstructor, ConstructorType, JavaObject)
IMPLEMENT_JAVA_METHOD(WeakRefeference, Get, "get", JavaObject)

IMPLEMENT_JAVA_CLASS(WeakHashMap, "java/util/WeakHashMap")
IMPLEMENT_JAVA_METHOD(WeakHashMap, Constructor, kJniSigConstructor, ConstructorType)
IMPLEMENT_JAVA_METHOD(WeakHashMap, Get, "get", JavaObject, JavaObject)
IMPLEMENT_JAVA_METHOD(WeakHashMap, Put, "put", JavaObject, JavaObject, JavaObject)

IMPLEMENT_JAVA_CLASS(ValdiAction, "com/snap/valdi/actions/ValdiAction")
IMPLEMENT_JAVA_METHOD(ValdiAction, Perform, "perform", JavaObject, ObjectArray)

IMPLEMENT_JAVA_CLASS(ValdiMarshaller, "com/snap/valdi/utils/ValdiMarshaller")

IMPLEMENT_JAVA_CLASS(ValdiFunction, "com/snap/valdi/callable/ValdiFunction")

IMPLEMENT_JAVA_CLASS(ValdiFunctionNative, "com/snap/valdi/callable/ValdiFunctionNative")
IMPLEMENT_JAVA_METHOD(ValdiFunctionNative, Constructor, kJniSigConstructor, ConstructorType, int64_t)
IMPLEMENT_JAVA_STATIC_METHOD(
    ValdiFunctionNative, PerformFromNative, "performFromNative", VoidType, ValdiFunctionType, int64_t)
IMPLEMENT_JAVA_STATIC_METHOD(
    ValdiFunctionNative, PerformRunnableFromNative, "performRunnableFromNative", VoidType, RunnableType)

IMPLEMENT_JAVA_CLASS(ValdiMarshallable, "com/snap/valdi/utils/ValdiMarshallable")

IMPLEMENT_JAVA_CLASS(ValdiMarshallerCPP, "com/snap/valdi/utils/ValdiMarshallerCPP")
IMPLEMENT_JAVA_STATIC_METHOD(
    ValdiMarshallerCPP, PushMarshallable, "pushMarshallable", int32_t, ValdiMarshallableType, int64_t)
IMPLEMENT_JAVA_STATIC_METHOD(ValdiMarshallerCPP, ArrayToList, "arrayToList", JavaObject, ObjectArray)
IMPLEMENT_JAVA_STATIC_METHOD(ValdiMarshallerCPP, ListToArray, "listToArray", ObjectArray, JavaObject)

IMPLEMENT_JAVA_CLASS(View, "android/view/View")
IMPLEMENT_JAVA_METHOD(View, GetWidth, "getWidth", int32_t)
IMPLEMENT_JAVA_METHOD(View, GetHeight, "getHeight", int32_t)
IMPLEMENT_JAVA_METHOD(View, GetLeft, "getLeft", int32_t)
IMPLEMENT_JAVA_METHOD(View, GetTop, "getTop", int32_t)

IMPLEMENT_JAVA_CLASS(AutoDisposable, "com/snap/valdi/utils/AutoDisposable")
IMPLEMENT_JAVA_METHOD(AutoDisposable, Retain, "retain", VoidType)
IMPLEMENT_JAVA_METHOD(AutoDisposable, Release, "release", VoidType)

IMPLEMENT_JAVA_CLASS(UndefinedValue, "com/snapchat/client/valdi/UndefinedValue")

IMPLEMENT_JAVA_CLASS(ValdiException, "com/snap/valdi/exceptions/ValdiException")
IMPLEMENT_JAVA_METHOD(ValdiException, Constructor, kJniSigConstructor, ConstructorType, String)
IMPLEMENT_JAVA_CLASS(MarshallerException, "com/snap/valdi/exceptions/MarshallerException")

IMPLEMENT_JAVA_CLASS(CppObjectWrapper, "com/snapchat/client/valdi/utils/CppObjectWrapper")
IMPLEMENT_JAVA_METHOD(CppObjectWrapper, Constructor, kJniSigConstructor, ConstructorType, int64_t)

IMPLEMENT_JAVA_CLASS(NativeRef, "com/snap/valdi/utils/NativeRef")
IMPLEMENT_JAVA_METHOD(NativeRef, Constructor, kJniSigConstructor, ConstructorType, int64_t)
IMPLEMENT_JAVA_METHOD(NativeRef, GetNativeHandle, "getNativeHandle", int64_t)

IMPLEMENT_JAVA_CLASS(NativeHandleWrapper, "com/snapchat/client/valdi/utils/NativeHandleWrapper")

IMPLEMENT_JAVA_CLASS(ValdiResult, "com/snap/valdi/utils/ValdiResult")
IMPLEMENT_JAVA_METHOD(ValdiResult, IsSuccess, "isSuccess", bool)
IMPLEMENT_JAVA_METHOD(ValdiResult, IsFailure, "isFailure", bool)
IMPLEMENT_JAVA_METHOD(ValdiResult, GetErrorMessage, "getErrorMessage", String)
IMPLEMENT_JAVA_METHOD(ValdiResult, GetSuccessValue, "getSuccessValue", JavaObject)

IMPLEMENT_JAVA_STATIC_METHOD(ValdiResult, Failure, "failure", ValdiResultType, String)
IMPLEMENT_JAVA_STATIC_METHOD(ValdiResult, Success, "success", ValdiResultType, JavaObject)

IMPLEMENT_JAVA_CLASS(ValdiThread, "com/snap/valdi/utils/ValdiThread")
IMPLEMENT_JAVA_STATIC_METHOD(ValdiThread, Start, "start", ValdiThreadType, String, int32_t, int64_t)
IMPLEMENT_JAVA_METHOD(ValdiThread, Join, "join", VoidType)
IMPLEMENT_JAVA_METHOD(ValdiThread, UpdateQoS, "updateQoS", VoidType, int32_t)

IMPLEMENT_JAVA_CLASS(ViewFactory, "com/snap/valdi/ViewFactoryPrivate")
IMPLEMENT_JAVA_METHOD(ViewFactory, CreateView, "createView", ViewType, JavaObject)
IMPLEMENT_JAVA_METHOD(ViewFactory, BindAttributes, "bindAttributes", VoidType, int64_t)
IMPLEMENT_JAVA_METHOD(ViewFactory, ManagesChildFrames, "managesChildFrames", bool)

IMPLEMENT_JAVA_CLASS(ViewRef, "com/snap/valdi/ViewRef")
IMPLEMENT_JAVA_METHOD(ViewRef, InvalidateLayout, "invalidateLayout", VoidType)
IMPLEMENT_JAVA_METHOD(ViewRef, Measure, "measure", int64_t, int32_t, int32_t, int32_t, int32_t)
IMPLEMENT_JAVA_METHOD(ViewRef, Layout, "layout", VoidType)
IMPLEMENT_JAVA_METHOD(ViewRef, CancelAllAnimations, "cancelAllAnimations", VoidType)
IMPLEMENT_JAVA_METHOD(ViewRef, IsRecyclable, "isRecyclable", bool)
IMPLEMENT_JAVA_METHOD(ViewRef, Snapshot, "snapshot", VoidType, ValdiFunctionType)
IMPLEMENT_JAVA_METHOD(
    ViewRef, Raster, "raster", String, JavaObject, int32_t, int32_t, int32_t, int32_t, float, float, JavaObject)
IMPLEMENT_JAVA_METHOD(ViewRef, GetViewClassName, "getViewClassName", String)
IMPLEMENT_JAVA_METHOD(ViewRef, OnTouchEvent, "onTouchEvent", bool, int64_t, int32_t, float, float, JavaObject)
IMPLEMENT_JAVA_METHOD(ViewRef, CustomizedHitTest, "customizedHitTest", int32_t, float, float)
IMPLEMENT_JAVA_METHOD(ViewRef, RequestFocus, "requestFocus", VoidType)

IMPLEMENT_JAVA_CLASS(SurfacePresenterManager, "com/snap/valdi/snapdrawing/SurfacePresenterManager")
IMPLEMENT_JAVA_METHOD(SurfacePresenterManager,
                      CreatePresenterWithDrawableSurface,
                      "createPresenterWithDrawableSurface",
                      VoidType,
                      int32_t,
                      int32_t)
IMPLEMENT_JAVA_METHOD(SurfacePresenterManager,
                      CreatePresenterForEmbeddedView,
                      "createPresenterForEmbeddedView",
                      VoidType,
                      int32_t,
                      int32_t,
                      ViewType)
IMPLEMENT_JAVA_METHOD(SurfacePresenterManager, SetPresenterZIndex, "setPresenterZIndex", VoidType, int32_t, int32_t)
IMPLEMENT_JAVA_METHOD(SurfacePresenterManager, RemovePresenter, "removePresenter", VoidType, int32_t)
IMPLEMENT_JAVA_METHOD(SurfacePresenterManager,
                      SetEmbeddedViewPresenterState,
                      "setEmbeddedViewPresenterState",
                      VoidType,
                      int32_t,
                      int32_t,
                      int32_t,
                      int32_t,
                      int32_t,
                      float,
                      JavaObject,
                      bool,
                      JavaObject,
                      bool)
IMPLEMENT_JAVA_METHOD(
    SurfacePresenterManager, OnDrawableSurfacePresenterUpdated, "onDrawableSurfacePresenterUpdated", VoidType, int32_t)

IMPLEMENT_JAVA_CLASS(Bitmap, "android/graphics/Bitmap")
IMPLEMENT_JAVA_STATIC_METHOD(Bitmap, CreateBitmap, "createBitmap", BitmapType, int32_t, int32_t, BitmapConfigType)
IMPLEMENT_JAVA_METHOD(Bitmap, SetPremultiplied, "setPremultiplied", VoidType, bool)

IMPLEMENT_JAVA_CLASS(BitmapConfig, "android/graphics/Bitmap$Config")
IMPLEMENT_JAVA_STATIC_METHOD(BitmapConfig, ValueOf, "valueOf", BitmapConfigType, String)

IMPLEMENT_JAVA_CLASS(BitmapHandler, "com/snap/valdi/utils/BitmapHandler")
IMPLEMENT_JAVA_METHOD(BitmapHandler, GetBitmap, "getBitmap", BitmapType)
IMPLEMENT_JAVA_METHOD(BitmapHandler, Retain, "retain", VoidType)
IMPLEMENT_JAVA_METHOD(BitmapHandler, Release, "release", VoidType)

IMPLEMENT_JAVA_CLASS(ValdiImage, "com/snap/valdi/utils/ValdiImage")
IMPLEMENT_JAVA_METHOD(ValdiImage, OnRetrieveContent, "onRetrieveContent", JavaObject, int64_t)

IMPLEMENT_JAVA_CLASS(Asset, "com/snapchat/client/valdi_core/Asset")

IMPLEMENT_JAVA_CLASS(PromiseCallback, "com/snap/valdi/promise/PromiseCallback")
IMPLEMENT_JAVA_METHOD(PromiseCallback, OnSuccess, "onSuccess", VoidType, JavaObject)
IMPLEMENT_JAVA_METHOD(PromiseCallback, OnFailure, "onFailure", VoidType, Throwable)

IMPLEMENT_JAVA_CLASS(Promise, "com/snap/valdi/promise/Promise")
IMPLEMENT_JAVA_METHOD(Promise, OnComplete, "onComplete", VoidType, PromiseCallback)
IMPLEMENT_JAVA_METHOD(Promise, Cancel, "cancel", VoidType)
IMPLEMENT_JAVA_METHOD(Promise, IsCancelable, "isCancelable", bool)

IMPLEMENT_JAVA_CLASS(CppPromiseCallback, "com/snap/valdi/promise/CppPromiseCallback")
IMPLEMENT_JAVA_METHOD(CppPromiseCallback, Constructor, kJniSigConstructor, ConstructorType, int64_t, int64_t)

IMPLEMENT_JAVA_CLASS(CppPromise, "com/snap/valdi/promise/CppPromise")
IMPLEMENT_JAVA_METHOD(CppPromise, Constructor, kJniSigConstructor, ConstructorType, int64_t, int64_t)

} // namespace ValdiAndroid
