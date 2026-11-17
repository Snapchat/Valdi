//
//  JavaUtils.hpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/9/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/PlatformResult.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"
#include "valdi_core/jni/JNIConstants.hpp"
#include "valdi_core/jni/JavaEnv.hpp"
#include "valdi_core/jni/JavaObject.hpp"
#include "valdi_core/jni/JavaValue.hpp"
#include <jni.h>
#include <string>
#include <vector>

namespace Valdi {
class ValueSchema;
}

namespace ValdiAndroid {

class JavaEnv;

JavaObject undefinedValueJavaObject(JavaEnv env);
JavaObject toJavaObject(JavaEnv env, int32_t value);
JavaObject toJavaObject(JavaEnv env, int64_t value);
JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::BigInt>& value);
Valdi::Ref<Valdi::BigInt> bigIntFromJavaObject(jobject value);
JavaObject toJavaObject(JavaEnv env, double value);
JavaObject toJavaObject(JavaEnv env, bool value);
String toJavaObject(JavaEnv env, const std::string_view& str);
String toJavaObject(JavaEnv env, const Valdi::StringBox& str);
String toJavaObject(JavaEnv env, const Valdi::StaticString& str);
JavaObject toJavaObject(JavaEnv env, const Valdi::Value& weakValue);
JavaObject toJavaObject(JavaEnv env, const Valdi::Error& error);
JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueMap>& value);
JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueTypedObject>& value);
JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueTypedProxyObject>& value);
ObjectArray toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueArray>& value);
ObjectArray toJavaObject(JavaEnv env, const std::vector<Valdi::Value>& value);
ByteArray toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueTypedArray>& value);
ByteArray toJavaObject(JavaEnv env, const Valdi::BytesView& value);
ByteArray toJavaObject(JavaEnv env, const Valdi::Byte* data, size_t length);
JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueFunction>& value);

JavaValue toJavaValue(JavaEnv env, jobject javaObject);
JavaValue toJavaValue(JavaEnv env, int32_t value);
JavaValue toJavaValue(JavaEnv env, int64_t value);
JavaValue toJavaValue(JavaEnv env, double value);
JavaValue toJavaValue(JavaEnv env, float value);
JavaValue toJavaValue(JavaEnv env, bool value);
JavaValue toJavaValue(JavaEnv env, const Valdi::Value& weakValue);
JavaValue toJavaValue(JavaEnv env, const std::string& str);
JavaValue toJavaValue(JavaEnv env, const JavaObject& object);
JavaValue toJavaValue(JavaEnv env, djinni::LocalRef<jobject> object);
JavaValue toJavaValue(JavaEnv env, VoidType voidObject);

Throwable toJavaThrowable(JavaEnv env, const Valdi::Error& error);

djinni::LocalRef<jobject> newJavaBooleanObject(bool value);
djinni::LocalRef<jobject> newJavaFloatObject(float value);
djinni::LocalRef<jobject> newJavaDoubleObject(double value);
djinni::LocalRef<jobject> newJavaIntObject(int32_t value);
djinni::LocalRef<jobject> newJavaLongObject(int64_t value);
djinni::LocalRef<jstring> newJavaString(const Valdi::StaticString& str);
djinni::LocalRef<jstring> newJavaStringUTF8(const std::string_view& str);
djinni::LocalRef<jstring> newJavaStringUTF16(const std::u16string_view& str);
djinni::LocalRef<jbyteArray> newJavaByteArray(const Valdi::Byte* data, size_t length);
djinni::LocalRef<jobjectArray> newJavaObjectArray(size_t length);

Valdi::Value toValue(JavaEnv env, ::djinni::LocalRef<jobject> object);

Valdi::Value valueFromJavaValdiFunction(JavaEnv env, jobject object);

Valdi::BytesView toByteArray(JavaEnv env, jbyteArray array);
Valdi::Value valueFromJavaByteArray(JavaEnv env, jbyteArray array);

JavaObject toByteBuffer(JavaEnv env, void* data, size_t size);
JavaObject toJavaFloatArray(JavaEnv env, std::initializer_list<float> values);
JavaObject toJavaFloatArray(JavaEnv env, const float* values, size_t size);
JavaObject toJavaLongArray(JavaEnv env, const int64_t* values, size_t size);

std::string toStdString(JavaEnv env, const String& string);
std::string toStdString(JavaEnv env, jstring string);
Valdi::StringBox toInternedString(JavaEnv env, const String& string);
Valdi::StringBox toInternedString(JavaEnv env, jstring string);

Valdi::Ref<Valdi::StaticString> toStaticString(JavaEnv env, jstring string);

Valdi::Marshaller* unwrapMarshaller(JNIEnv* env, jlong ptr);

Valdi::Value valueFromJavaCppHandle(jlong handle);

Valdi::PlatformResult platformResultFromJavaResult(JavaEnv env, jobject javaResult);
JavaObject javaResultFromPlatformResult(JavaEnv env, const Valdi::PlatformResult& platformResult);

jlong createInternedString(JavaEnv env, jstring string);
Valdi::StringBox unwrapInternedString(jlong ptr);
void destroyInternedString(jlong ptr);

JavaObject javaObjectFromValdiObject(const Valdi::SharedValdiObject& nativeObject, bool wrapIfNeeded);
JavaObject javaObjectFromValdiObject(const Valdi::SharedValdiObject& nativeObject);
Valdi::SharedValdiObject valdiObjectFromJavaObject(const JavaObject& javaObject, const char* tag);

JavaObject newJavaObjectWrappingValue(JavaEnv env, const Valdi::Value& valueToWrap);
// We let Java give us the native handle as int to avoid more communication with the JVM
// to retrieve the native handle from the CppObjectWrapper
Valdi::Ref<Valdi::RefCountable> wrappedCppPtrFromNativeHandle(int64_t nativeHandle);

ValdiContext fromValdiContextUserData(const Valdi::SharedValdiObject& userData);
Valdi::SharedValdiObject toValdiContextUserData(const ValdiContext& context);

Valdi::Shared<Valdi::ValueConvertible> javaObjectToLazyValueConvertible(JavaEnv env,
                                                                        jobject javaObject,
                                                                        const char* tag);

djinni::LocalRef<jobject> newWeakReference(JavaEnv env, jobject value);
djinni::LocalRef<jobject> getWeakReferenceValue(JavaEnv env, jobject weakReference);

jlong bridgeRetain(Valdi::RefCountable* nativeReference);
jlong bridgeCast(Valdi::RefCountable* nativeReference);
void bridgeRelease(jlong jNativeReference);

Valdi::Ref<Valdi::ValueTypedProxyObject> newJavaProxy(jobject object,
                                                      const Valdi::Ref<Valdi::ValueTypedObject>& typedObject);

std::string getJavaTypeSignatureFromValueSchema(const Valdi::ValueSchema& schema, bool treatArrayAsList);
std::optional<JavaValueType> schemaToJavaValueType(const Valdi::ValueSchema& schema);

[[noreturn]] void handleFatalError(std::string_view message);

template<typename T>
Valdi::Ref<T> bridge(jlong ptr) {
    return Valdi::unsafeBridge<T>(reinterpret_cast<void*>(ptr));
}

template<typename T>
Valdi::Ref<T> bridgeTransfer(jlong ptr) {
    return Valdi::unsafeBridgeTransfer<T>(reinterpret_cast<void*>(ptr));
}

class JavaIterator {
public:
    JavaIterator(JavaObject&& iterator);
    ~JavaIterator();

    std::optional<JavaObject> next();

    static JavaIterator fromIterable(jobject iterable);

private:
    JavaObject _iterator;
};

class JavaObjectArray {
public:
    JavaObjectArray(jobjectArray array);

    size_t size() const;

    djinni::LocalRef<jobject> getObject(size_t index) const;
    void setObject(size_t index, jobject object);

private:
    jobjectArray _array;
};

} // namespace ValdiAndroid
