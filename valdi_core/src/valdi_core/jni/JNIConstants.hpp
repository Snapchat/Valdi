//
//  JNIConstants.hpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/12/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Utils/JNISignatures.hpp"
#include "valdi_core/jni/JavaObject.hpp"
#include <jni.h>
#include <string>

#define JNI_TYPE(Type, jniType)                                                                                        \
    template<>                                                                                                         \
    struct GetJNIType<Type> {                                                                                          \
        const char* operator()() const {                                                                               \
            return jniType;                                                                                            \
        }                                                                                                              \
    }; // NOLINT

#define JAVA_OBJECT_TYPE(Type, jniType)                                                                                \
    class Type : public JavaObject { /*NOLINT*/                                                                        \
    public:                                                                                                            \
        explicit Type(JavaEnv env, ::djinni::LocalRef<jobject> object) : JavaObject(env, std::move(object)) {}         \
        explicit Type(JavaEnv env, jobject object) : JavaObject(env, object) {}                                        \
        explicit Type(JavaEnv env) : JavaObject(env) {}                                                                \
    };                                                                                                                 \
    JNI_TYPE(Type, jniType)

namespace ValdiAndroid {

class VoidType {
public:
    VoidType();
};

class ConstructorType {};

template<typename T>
struct GetJNIType {};

JNI_TYPE(jobject, kJniSigObject)
JNI_TYPE(jclass, kJniSigClass)
JNI_TYPE(jstring, kJniSigString)
JNI_TYPE(std::string, kJniSigString)
JNI_TYPE(bool, kJniSigBool)
JNI_TYPE(float, kJniSigFloat)
JNI_TYPE(double, kJniSigDouble)
JNI_TYPE(int32_t, kJniSigInt)
JNI_TYPE(int64_t, "J")
JNI_TYPE(VoidType, kJniSigVoid)
JNI_TYPE(jobjectArray, kJniSigObjectArray)
JNI_TYPE(jbooleanArray, JAVA_ARRAY("Z"))

JNI_TYPE(ConstructorType, kJniSigConstructorRetType)
JNI_TYPE(JavaObject, JAVA_CLASS("java/lang/Object"))

JAVA_OBJECT_TYPE(ByteArray, JAVA_ARRAY("B"))
JAVA_OBJECT_TYPE(ObjectArray, JAVA_ARRAY(JAVA_CLASS("java/lang/Object")))
JAVA_OBJECT_TYPE(String, JAVA_CLASS("java/lang/String"))
JAVA_OBJECT_TYPE(Throwable, JAVA_CLASS("java/lang/Throwable"))

JAVA_OBJECT_TYPE(Class, JAVA_CLASS("java/lang/Class"))

JAVA_OBJECT_TYPE(SetType, JAVA_CLASS("java/util/Set"))
JAVA_OBJECT_TYPE(MapType, JAVA_CLASS("java/util/Map"))
JAVA_OBJECT_TYPE(IteratorType, JAVA_CLASS("java/util/Iterator"))
JAVA_OBJECT_TYPE(BitmapType, JAVA_CLASS("android/graphics/Bitmap"))
JAVA_OBJECT_TYPE(BitmapConfigType, JAVA_CLASS("android/graphics/Bitmap$Config"))

JAVA_OBJECT_TYPE(ValdiContext, JAVA_CLASS("com/snap/valdi/context/ValdiContext"))

JAVA_OBJECT_TYPE(Unit, JAVA_CLASS("kotlin/Unit"))
JAVA_OBJECT_TYPE(AttributesBindingContext, JAVA_CLASS("com/snapchat/client/valdi/AttributesBindingContext"))

JAVA_OBJECT_TYPE(ViewType, JAVA_CLASS("com/snap/valdi/ViewRef"))
JAVA_OBJECT_TYPE(ValdiMarshallerType, JAVA_CLASS("com/snap/valdi/utils/ValdiMarshaller"))
JAVA_OBJECT_TYPE(ValdiFunctionType, JAVA_CLASS("com/snap/valdi/callable/ValdiFunction"))
JAVA_OBJECT_TYPE(ValdiMarshallableType, JAVA_CLASS("com/snap/valdi/utils/ValdiMarshallable"))
JAVA_OBJECT_TYPE(ValdiThreadType, JAVA_CLASS("com/snap/valdi/utils/ValdiThread"))
JAVA_OBJECT_TYPE(ValdiResultType, JAVA_CLASS("com/snap/valdi/utils/ValdiResult"))
JAVA_OBJECT_TYPE(RunnableType, JAVA_CLASS("java/lang/Runnable"))

JAVA_OBJECT_TYPE(Promise, kJniSigPromise)
JAVA_OBJECT_TYPE(PromiseCallback, JAVA_CLASS("com/snap/valdi/promise/PromiseCallback"))

} // namespace ValdiAndroid
