//
//  JavaCache.hpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/14/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/jni/GlobalRefJavaObject.hpp"
#include "valdi_core/jni/JNIConstants.hpp"
#include "valdi_core/jni/JavaClass.hpp"
#include "valdi_core/jni/JavaEnv.hpp"

#include <optional>

#define DECLARE_JAVA_CLASS(name)                                                                                       \
private:                                                                                                               \
    std::optional<JavaClass> name##Class;                                                                              \
                                                                                                                       \
public:                                                                                                                \
    JavaClass& get##name##Class();

#define DECLARE_JAVA_METHOD(class, suffix, ...)                                                                        \
public:                                                                                                                \
    JavaMethod<__VA_ARGS__>& get##class##suffix##Method();

#define DECLARE_JAVA_STATIC_METHOD(class, suffix, ...)                                                                 \
public:                                                                                                                \
    JavaStaticMethod<__VA_ARGS__>& get##class##suffix##Method();

namespace ValdiAndroid {

class JavaCache {
public:
    static JavaCache& get();
    static JavaObject getUndefinedValueClassInstance();
    static JavaObject getUnitValueClassInstance();

    DECLARE_JAVA_CLASS(Class)
    DECLARE_JAVA_METHOD(Class, GetName, String)

    DECLARE_JAVA_CLASS(Throwable)
    DECLARE_JAVA_METHOD(Throwable, GetMessage, String)

    DECLARE_JAVA_CLASS(Exception)

    DECLARE_JAVA_CLASS(Set)
    DECLARE_JAVA_METHOD(Set, ToArray, ObjectArray)

    DECLARE_JAVA_CLASS(Map)
    DECLARE_JAVA_METHOD(Map, EntrySet, SetType)
    DECLARE_JAVA_METHOD(Map, Size, int32_t)

    DECLARE_JAVA_CLASS(HashMap)
    DECLARE_JAVA_METHOD(HashMap, Constructor, ConstructorType)
    DECLARE_JAVA_METHOD(HashMap, Put, JavaObject, JavaObject, JavaObject)

    DECLARE_JAVA_CLASS(Object)
    DECLARE_JAVA_METHOD(Object, ToString, String)

    DECLARE_JAVA_CLASS(Number)
    DECLARE_JAVA_METHOD(Number, IntValue, int32_t)
    DECLARE_JAVA_METHOD(Number, LongValue, int64_t)
    DECLARE_JAVA_METHOD(Number, DoubleValue, double)

    DECLARE_JAVA_CLASS(String)

    DECLARE_JAVA_CLASS(Integer)
    DECLARE_JAVA_METHOD(Integer, Constructor, ConstructorType, int32_t)

    DECLARE_JAVA_CLASS(Long)
    DECLARE_JAVA_METHOD(Long, Constructor, ConstructorType, int64_t)

    DECLARE_JAVA_CLASS(Double)
    DECLARE_JAVA_METHOD(Double, Constructor, ConstructorType, double)

    DECLARE_JAVA_CLASS(Float)
    DECLARE_JAVA_METHOD(Float, Constructor, ConstructorType, float)

    DECLARE_JAVA_CLASS(Boolean)
    DECLARE_JAVA_METHOD(Boolean, Constructor, ConstructorType, bool)
    DECLARE_JAVA_METHOD(Boolean, BooleanValue, bool)

    DECLARE_JAVA_CLASS(Iterable)
    DECLARE_JAVA_METHOD(Iterable, Iterator, IteratorType)

    DECLARE_JAVA_CLASS(Iterator)
    DECLARE_JAVA_METHOD(Iterator, HasNext, bool)
    DECLARE_JAVA_METHOD(Iterator, Next, JavaObject)

    DECLARE_JAVA_CLASS(ObjectArray);
    DECLARE_JAVA_CLASS(ByteArray);

    DECLARE_JAVA_CLASS(MapEntry)
    DECLARE_JAVA_METHOD(MapEntry, GetKey, JavaObject)
    DECLARE_JAVA_METHOD(MapEntry, GetValue, JavaObject)

    DECLARE_JAVA_CLASS(WeakRefeference)
    DECLARE_JAVA_METHOD(WeakRefeference, Constructor, ConstructorType, JavaObject)
    DECLARE_JAVA_METHOD(WeakRefeference, Get, JavaObject)

    DECLARE_JAVA_CLASS(WeakHashMap)
    DECLARE_JAVA_METHOD(WeakHashMap, Constructor, ConstructorType)
    DECLARE_JAVA_METHOD(WeakHashMap, Get, JavaObject, JavaObject)
    DECLARE_JAVA_METHOD(WeakHashMap, Put, JavaObject, JavaObject, JavaObject)

    DECLARE_JAVA_CLASS(ValdiAction)
    DECLARE_JAVA_METHOD(ValdiAction, Perform, JavaObject, ObjectArray)

    DECLARE_JAVA_CLASS(ValdiMarshaller)

    DECLARE_JAVA_CLASS(ValdiMarshallerCPP)
    DECLARE_JAVA_STATIC_METHOD(ValdiMarshallerCPP, PushMarshallable, int32_t, ValdiMarshallableType, int64_t)
    DECLARE_JAVA_STATIC_METHOD(ValdiMarshallerCPP, ArrayToList, JavaObject, ObjectArray)
    DECLARE_JAVA_STATIC_METHOD(ValdiMarshallerCPP, ListToArray, ObjectArray, JavaObject)

    DECLARE_JAVA_CLASS(ValdiFunction)

    DECLARE_JAVA_CLASS(ValdiMarshallable)

    DECLARE_JAVA_CLASS(ValdiFunctionNative)
    DECLARE_JAVA_METHOD(ValdiFunctionNative, Constructor, ConstructorType, int64_t)
    DECLARE_JAVA_STATIC_METHOD(ValdiFunctionNative, PerformFromNative, VoidType, ValdiFunctionType, int64_t)
    DECLARE_JAVA_STATIC_METHOD(ValdiFunctionNative, PerformRunnableFromNative, VoidType, RunnableType)

    DECLARE_JAVA_CLASS(View)
    DECLARE_JAVA_METHOD(View, GetWidth, int32_t)
    DECLARE_JAVA_METHOD(View, GetHeight, int32_t)
    DECLARE_JAVA_METHOD(View, GetLeft, int32_t)
    DECLARE_JAVA_METHOD(View, GetTop, int32_t)

    DECLARE_JAVA_CLASS(CppObjectWrapper)
    DECLARE_JAVA_METHOD(CppObjectWrapper, Constructor, ConstructorType, int64_t)

    DECLARE_JAVA_CLASS(NativeRef)
    DECLARE_JAVA_METHOD(NativeRef, Constructor, ConstructorType, int64_t)
    DECLARE_JAVA_METHOD(NativeRef, GetNativeHandle, int64_t)

    DECLARE_JAVA_CLASS(NativeHandleWrapper)

    DECLARE_JAVA_CLASS(AutoDisposable)
    DECLARE_JAVA_METHOD(AutoDisposable, Retain, VoidType)
    DECLARE_JAVA_METHOD(AutoDisposable, Release, VoidType)

    DECLARE_JAVA_CLASS(UndefinedValue)

    DECLARE_JAVA_CLASS(ValdiException)
    DECLARE_JAVA_METHOD(ValdiException, Constructor, ConstructorType, String)
    DECLARE_JAVA_CLASS(MarshallerException)

    DECLARE_JAVA_CLASS(ValdiThread)
    DECLARE_JAVA_STATIC_METHOD(ValdiThread, Start, ValdiThreadType, String, int32_t, int64_t)
    DECLARE_JAVA_METHOD(ValdiThread, Join, VoidType)
    DECLARE_JAVA_METHOD(ValdiThread, UpdateQoS, VoidType, int32_t)

    DECLARE_JAVA_CLASS(ValdiResult)
    DECLARE_JAVA_METHOD(ValdiResult, IsSuccess, bool)
    DECLARE_JAVA_METHOD(ValdiResult, IsFailure, bool)
    DECLARE_JAVA_METHOD(ValdiResult, GetErrorMessage, String)
    DECLARE_JAVA_METHOD(ValdiResult, GetSuccessValue, JavaObject)
    DECLARE_JAVA_STATIC_METHOD(ValdiResult, Failure, ValdiResultType, String)
    DECLARE_JAVA_STATIC_METHOD(ValdiResult, Success, ValdiResultType, JavaObject)

    DECLARE_JAVA_CLASS(ViewFactory)
    DECLARE_JAVA_METHOD(ViewFactory, CreateView, ViewType, JavaObject)
    DECLARE_JAVA_METHOD(ViewFactory, BindAttributes, VoidType, int64_t)
    DECLARE_JAVA_METHOD(ViewFactory, ManagesChildFrames, bool)

    DECLARE_JAVA_CLASS(ViewRef)
    DECLARE_JAVA_METHOD(ViewRef, InvalidateLayout, VoidType)
    DECLARE_JAVA_METHOD(ViewRef, Measure, int64_t, int32_t, int32_t, int32_t, int32_t)
    DECLARE_JAVA_METHOD(ViewRef, Layout, VoidType)
    DECLARE_JAVA_METHOD(ViewRef, CancelAllAnimations, VoidType)
    DECLARE_JAVA_METHOD(ViewRef, IsRecyclable, bool)
    DECLARE_JAVA_METHOD(ViewRef, OnTouchEvent, bool, int64_t, int32_t, float, float, JavaObject)
    DECLARE_JAVA_METHOD(ViewRef, CustomizedHitTest, int32_t, float, float)
    DECLARE_JAVA_METHOD(
        ViewRef, Raster, String, JavaObject, int32_t, int32_t, int32_t, int32_t, float, float, JavaObject)
    DECLARE_JAVA_METHOD(ViewRef, Snapshot, VoidType, ValdiFunctionType)
    DECLARE_JAVA_METHOD(ViewRef, GetViewClassName, String)
    DECLARE_JAVA_METHOD(ViewRef, RequestFocus, VoidType)

    DECLARE_JAVA_CLASS(SurfacePresenterManager)
    DECLARE_JAVA_METHOD(SurfacePresenterManager, CreatePresenterWithDrawableSurface, VoidType, int32_t, int32_t)
    DECLARE_JAVA_METHOD(SurfacePresenterManager, CreatePresenterForEmbeddedView, VoidType, int32_t, int32_t, ViewType)
    DECLARE_JAVA_METHOD(SurfacePresenterManager, SetPresenterZIndex, VoidType, int32_t, int32_t)
    DECLARE_JAVA_METHOD(SurfacePresenterManager, RemovePresenter, VoidType, int32_t)
    DECLARE_JAVA_METHOD(SurfacePresenterManager,
                        SetEmbeddedViewPresenterState,
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
    DECLARE_JAVA_METHOD(SurfacePresenterManager, OnDrawableSurfacePresenterUpdated, VoidType, int32_t)

    DECLARE_JAVA_CLASS(Bitmap)
    DECLARE_JAVA_STATIC_METHOD(Bitmap, CreateBitmap, BitmapType, int32_t, int32_t, BitmapConfigType)
    DECLARE_JAVA_METHOD(Bitmap, SetPremultiplied, VoidType, bool)

    DECLARE_JAVA_CLASS(BitmapConfig)
    DECLARE_JAVA_STATIC_METHOD(BitmapConfig, ValueOf, BitmapConfigType, String)

    DECLARE_JAVA_CLASS(BitmapHandler)
    DECLARE_JAVA_METHOD(BitmapHandler, GetBitmap, BitmapType)
    DECLARE_JAVA_METHOD(BitmapHandler, Retain, VoidType)
    DECLARE_JAVA_METHOD(BitmapHandler, Release, VoidType)

    DECLARE_JAVA_CLASS(ValdiImage)
    DECLARE_JAVA_METHOD(ValdiImage, OnRetrieveContent, JavaObject, int64_t)

    DECLARE_JAVA_CLASS(Asset)

    DECLARE_JAVA_CLASS(PromiseCallback)
    DECLARE_JAVA_METHOD(PromiseCallback, OnSuccess, VoidType, JavaObject)
    DECLARE_JAVA_METHOD(PromiseCallback, OnFailure, VoidType, Throwable)

    DECLARE_JAVA_CLASS(Promise)
    DECLARE_JAVA_METHOD(Promise, OnComplete, VoidType, PromiseCallback)
    DECLARE_JAVA_METHOD(Promise, Cancel, VoidType)
    DECLARE_JAVA_METHOD(Promise, IsCancelable, bool)

    DECLARE_JAVA_CLASS(CppPromiseCallback)
    DECLARE_JAVA_METHOD(CppPromiseCallback, Constructor, ConstructorType, int64_t, int64_t)

    DECLARE_JAVA_CLASS(CppPromise)
    DECLARE_JAVA_METHOD(CppPromise, Constructor, ConstructorType, int64_t, int64_t)

private:
    JavaCache();
};

} // namespace ValdiAndroid
