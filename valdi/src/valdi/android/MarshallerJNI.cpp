//
//  MarshallerJNI.cpp
//  valdi-android
//
//  Created by Simon Corsin on 7/24/19.
//

#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"
#include <djinni/jni/djinni_support.hpp>

static int valueTypeToInt(Valdi::ValueType type) {
    // Must match ValdiMarshaller.kt
    switch (type) {
        case Valdi::ValueType::Null:
            return 0;
        case Valdi::ValueType::Undefined:
            return 1;
        case Valdi::ValueType::InternedString:
        case Valdi::ValueType::StaticString:
            return 2;
        case Valdi::ValueType::Int:
            return 3;
        case Valdi::ValueType::Long:
            return 4;
        case Valdi::ValueType::Double:
            return 5;
        case Valdi::ValueType::Bool:
            return 6;
        case Valdi::ValueType::Map:
            return 7;
        case Valdi::ValueType::Array:
            return 8;
        case Valdi::ValueType::TypedArray:
            return 9;
        case Valdi::ValueType::Function:
            return 10;
        case Valdi::ValueType::Error:
            return 11;
        case Valdi::ValueType::TypedObject:
            return 12;
        case Valdi::ValueType::ProxyTypedObject:
            return 13;
        case Valdi::ValueType::ValdiObject:
            return 14;
        case Valdi::ValueType::BigInt:
            return 15;
    }
}

static inline Valdi::Marshaller* unwrap(JNIEnv* env, jlong handle) {
    return ValdiAndroid::unwrapMarshaller(env, handle);
}

static void throwJavaMarshallerException(JNIEnv* env, const Valdi::Error& error) {
    if (env->ExceptionCheck() != 0L) {
        return;
    }

    env->ThrowNew(ValdiAndroid::JavaEnv::getCache().getMarshallerExceptionClass().getClass(), error.toString().c_str());
}

class AndroidMarshaller : public Valdi::Marshaller {
public:
    AndroidMarshaller() : Valdi::Marshaller(_exceptionTracker) {}

    ~AndroidMarshaller() override = default;

private:
    Valdi::SimpleExceptionTracker _exceptionTracker;
};

bool checkMarshaller(JNIEnv* env, Valdi::Marshaller* marshaller) {
    if (!marshaller->getExceptionTracker()) {
        throwJavaMarshallerException(env, marshaller->getExceptionTracker().extractError());
        return false;
    }
    return true;
}

void jniPutMapProperty(JNIEnv* env, Valdi::Marshaller* marshaller, jlong propPtr, jint index) {
    marshaller->putMapProperty(ValdiAndroid::unwrapInternedString(propPtr), static_cast<int>(index));
    checkMarshaller(env, marshaller);
}

static bool jniGetMapProperty(JNIEnv* env, Valdi::Marshaller* marshaller, jlong propPtr, jint index, bool optional) {
    auto propStrCpp = ValdiAndroid::unwrapInternedString(propPtr);
    if (optional) {
        return marshaller->getMapProperty(
            propStrCpp, static_cast<int>(index), Valdi::Marshaller::GetMapPropertyFlags::IgnoreNullOrUndefined);
    } else {
        marshaller->mustGetMapProperty(
            propStrCpp, static_cast<int>(index), Valdi::Marshaller::GetMapPropertyFlags::IgnoreNullOrUndefined);
        return checkMarshaller(env, marshaller);
    }
}

template<typename T>
static std::optional<T> jniGetMapPropertyAndReturnValue(
    JNIEnv* env, jlong ptr, jlong propPtr, jint index, bool optional) {
    auto marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return std::nullopt;
    }

    if (!jniGetMapProperty(env, marshaller, propPtr, index, optional)) {
        return std::nullopt;
    }

    auto value = marshaller->getOrUndefined(-1).checkedTo<T>(marshaller->getExceptionTracker());
    marshaller->pop();
    if (!checkMarshaller(env, marshaller)) {
        return std::nullopt;
    }

    return {std::move(value)};
}

bool checkNotNull(JNIEnv* env, jobject object) {
    if (object == nullptr) {
        throwJavaMarshallerException(env, Valdi::Error("Object is null"));
        return false;
    }
    return true;
}

jlong nativeCreate() {
    Valdi::Marshaller* marshaller = new AndroidMarshaller();
    return reinterpret_cast<jlong>(marshaller);
}

void nativeDestroy(jlong ptr) {
    delete reinterpret_cast<Valdi::Marshaller*>(ptr);
}

jint nativeSize(jlong ptr) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->size());
}

jint nativePushMap(jlong ptr, jint capacity) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->pushMap(static_cast<int>(capacity)));
}

void nativePutMapProperty(JNIEnv* env, jclass /*cls*/, jlong ptr, jstring prop, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    auto cppProp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), prop);
    marshaller->putMapProperty(cppProp, static_cast<int>(index));
    checkMarshaller(env, marshaller);
}

void nativePutMapPropertyInterned(JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedString(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jstring value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }
    marshaller->pushString(ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), value));
    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedDouble(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jdouble value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->pushDouble(static_cast<double>(value));
    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedLong(JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jlong value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->pushLong(static_cast<int64_t>(value));
    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedBoolean(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jboolean value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->pushBool(static_cast<bool>(value));
    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedFunction(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jobject value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }
    if (!checkNotNull(env, value)) {
        return;
    }

    marshaller->pushFunction(ValdiAndroid::valueFromJavaValdiFunction(ValdiAndroid::JavaEnv(), value).getFunctionRef());
    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedOpaque(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jobject value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->pushValdiObject(ValdiAndroid::valdiObjectFromJavaObject(
        ValdiAndroid::JavaObject(ValdiAndroid::JavaEnv(), value), "MarshallerOpaqueObject"));
    jniPutMapProperty(env, marshaller, propPtr, index);
}

void nativePutMapPropertyInternedByteArray(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index, jbyteArray value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->push(ValdiAndroid::valueFromJavaByteArray(ValdiAndroid::JavaEnv(), value));
    jniPutMapProperty(env, marshaller, propPtr, index);
}

jboolean nativeGetMapProperty(JNIEnv* env, jclass /*cls*/, jlong ptr, jlong propPtr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return static_cast<jboolean>(false);
    }

    auto result = marshaller->getMapProperty(ValdiAndroid::unwrapInternedString(propPtr), static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }
    return static_cast<jboolean>(result);
}

jint nativePushArray(jlong ptr, jint capacity) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->pushArray(static_cast<int>(capacity)));
}

void nativeSetArrayItem(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index, jint arrayIndex) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->setArrayItem(static_cast<int>(index), static_cast<int>(arrayIndex));
    checkMarshaller(env, marshaller);
}

jint nativePushByteArray(JNIEnv* env, jclass /*cls*/, jlong ptr, jbyteArray byteArray) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto value = ValdiAndroid::valueFromJavaByteArray(ValdiAndroid::JavaEnv(), byteArray);
    return static_cast<jint>(marshaller->push(std::move(value)));
}

jint nativePushString(JNIEnv* env,

                      jclass /*cls*/,
                      jlong ptr,
                      jstring str) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto convertedStr = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), str);
    return static_cast<jint>(marshaller->push(Valdi::Value(convertedStr)));
}

void nativeSetError(JNIEnv* env,

                    jclass /*cls*/,
                    jlong ptr,
                    jstring error) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    auto convertedStr = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), error);
    marshaller->getExceptionTracker().onError(Valdi::Error(convertedStr));
}

void nativeCheckError(JNIEnv* env,

                      jclass /*cls*/,
                      jlong ptr) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }
    checkMarshaller(env, marshaller);
}

jint nativePushInternedString(jlong ptr, jlong propPtr) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto convertedStr = ValdiAndroid::unwrapInternedString(propPtr);
    return static_cast<jint>(marshaller->push(Valdi::Value(convertedStr)));
}

jint nativePushFunction(JNIEnv* env,

                        jclass /*cls*/,
                        jlong ptr,
                        jobject function) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }
    if (!checkNotNull(env, function)) {
        return 0;
    }

    auto value = ValdiAndroid::valueFromJavaValdiFunction(ValdiAndroid::JavaEnv(), function);
    return static_cast<jint>(marshaller->push(std::move(value)));
}

jint nativePushNull(jlong ptr) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->push(Valdi::Value()));
}

jint nativePushUndefined(jlong ptr) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->push(Valdi::Value::undefined()));
}

jint nativePushBoolean(jlong ptr, jboolean boolean) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->push(Valdi::Value(static_cast<bool>(boolean))));
}

jint nativePushDouble(jlong ptr, jdouble d) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->push(Valdi::Value(static_cast<double>(d))));
}

jint nativePushLong(jlong ptr, jlong l) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jint>(marshaller->pushLong(static_cast<int64_t>(l)));
}

jint nativePushBigInt(JNIEnv* env, jclass /*cls*/, jlong ptr, jobject value) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }
    return static_cast<jint>(marshaller->push(Valdi::Value(ValdiAndroid::bigIntFromJavaObject(value))));
}

jint nativePushOpaqueObject(JNIEnv* env,

                            jclass /*cls*/,
                            jlong ptr,
                            jobject obj) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto valdiObject = ValdiAndroid::valdiObjectFromJavaObject(ValdiAndroid::JavaObject(ValdiAndroid::JavaEnv(), obj),
                                                               "MarshallerOpaqueObject");
    return static_cast<jint>(marshaller->push(Valdi::Value(valdiObject)));
}

jint nativePushCppObject(jlong ptr, jlong cppPtr) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto value = ValdiAndroid::valueFromJavaCppHandle(cppPtr);
    return static_cast<jint>(marshaller->push(std::move(value)));
}

jint nativePushMapIterator(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto result = marshaller->pushMapIterator(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }

    return static_cast<jint>(result);
}

jboolean nativeMapIteratorPushNextAndPopPrevious(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jint index, jboolean popPrevious) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return static_cast<jboolean>(false);
    }

    if (popPrevious != 0) {
        marshaller->popCount(2);
        if (!checkMarshaller(env, marshaller)) {
            return 0;
        }
    }
    auto pushNextEntryResult = marshaller->pushMapIteratorNextEntry(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }

    return static_cast<jboolean>(pushNextEntryResult);
}

jint nativeUnwrapProxy(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return static_cast<jint>(0);
    }
    auto proxyObject = marshaller->getProxyObject(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return static_cast<jint>(0);
    }

    return static_cast<jint>(marshaller->push(Valdi::Value(proxyObject->getTypedObject())));
}

jint nativeGetTypedObjectProperty(JNIEnv* env, jclass /*cls*/, jlong ptr, jint propertyIndex, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return static_cast<jint>(0);
    }
    auto typedObject = marshaller->getTypedObject(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return static_cast<jint>(0);
    }
    return static_cast<jint>(marshaller->push(typedObject->getProperty(static_cast<size_t>(propertyIndex))));
}

jboolean nativeMapIteratorPushNext(JNIEnv* env, jclass cls, jlong ptr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return static_cast<jboolean>(false);
    }

    return nativeMapIteratorPushNextAndPopPrevious(env, cls, ptr, index, 0);
}

jint nativeGetType(jlong ptr, jint index) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return static_cast<jint>(valueTypeToInt(Valdi::ValueType::Undefined));
    }

    return static_cast<jint>(valueTypeToInt(marshaller->getOrUndefined(static_cast<int>(index)).getType()));
}

jboolean nativeGetBoolean(jlong ptr, jint index) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return static_cast<jboolean>(false);
    }

    return static_cast<jboolean>(marshaller->getOrUndefined(static_cast<int>(index)).toBool());
}

jdouble nativeGetDouble(jlong ptr, jint index) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jdouble>(marshaller->getOrUndefined(static_cast<int>(index)).toDouble());
}

jlong nativeGetLong(jlong ptr, jint index) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    return static_cast<jlong>(marshaller->getOrUndefined(static_cast<int>(index)).toLong());
}

jobject nativeGetUntyped(JNIEnv* env,

                         jclass /*cls*/,
                         jlong ptr,
                         jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto getResult = marshaller->get(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }

    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), getResult).releaseObject();
}

jstring nativeGetString(JNIEnv* env,

                        jclass /*cls*/,
                        jlong ptr,
                        jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto indexCpp = static_cast<int>(index);
    if (marshaller->isStaticString(indexCpp)) {
        return ValdiAndroid::newJavaString(*marshaller->getStaticString(indexCpp)).release();
    }

    auto getResult = marshaller->getString(indexCpp);

    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }

    return reinterpret_cast<jstring>(ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), getResult).releaseObject());
}

jlong nativeGetInternedString(JNIEnv* env,

                              jclass /*cls*/,
                              jlong ptr,
                              jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto getResult = marshaller->getString(static_cast<int>(index));

    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }

    return reinterpret_cast<jlong>(getResult.getInternedString().get());
}

jbyteArray nativeGetByteArray(JNIEnv* env,

                              jclass /*cls*/,
                              jlong ptr,
                              jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto typedArray = marshaller->getTypedArray(static_cast<int>(index));

    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }

    return reinterpret_cast<jbyteArray>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), typedArray).releaseObject());
}

jobject JNICALL nativeGetNativeWrapper(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto getResult = marshaller->get(static_cast<int>(index));

    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }
    return ValdiAndroid::newJavaObjectWrappingValue(ValdiAndroid::JavaEnv(), getResult).releaseObject();
}

jobject JNICALL nativeGetOpaqueObject(JNIEnv* env,

                                      jclass /*cls*/,
                                      jlong ptr,
                                      jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto getResult = marshaller->getValdiObject(static_cast<int>(index));

    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }
    return ValdiAndroid::javaObjectFromValdiObject(getResult).releaseObject();
}

jobject nativeGetFunction(JNIEnv* env,

                          jclass /*cls*/,
                          jlong ptr,
                          jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto getResult = marshaller->getFunction(static_cast<int>(index));

    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }
    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), getResult).releaseObject();
}

jint nativeGetArrayLength(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto arrayLength = marshaller->getArrayLength(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }

    return static_cast<jint>(arrayLength);
}

jint nativeGetArrayItem(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index, jint arrayIndex) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    auto getResult = marshaller->getArrayItem(static_cast<int>(index), static_cast<int>(arrayIndex));

    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }
    return static_cast<jint>(getResult);
}

jint nativeGetArrayItemAndPopPrevious(
    JNIEnv* env, jclass /*cls*/, jlong ptr, jint index, jint arrayIndex, jboolean popPrevious) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return 0;
    }

    if (popPrevious != 0) {
        marshaller->pop();
        if (!checkMarshaller(env, marshaller)) {
            return 0;
        }
    }

    auto getResult = marshaller->getArrayItem(static_cast<int>(index), static_cast<int>(arrayIndex));

    if (!checkMarshaller(env, marshaller)) {
        return 0;
    }
    return static_cast<jint>(getResult);
}

void nativePop(JNIEnv* env, jclass /*cls*/, jlong ptr) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->pop();
    checkMarshaller(env, marshaller);
}

void nativePopCount(JNIEnv* env, jclass /*cls*/, jlong ptr, jint count) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return;
    }

    marshaller->popCount(static_cast<int>(count));
    checkMarshaller(env, marshaller);
}

jstring nativeToString(JNIEnv* env,

                       jclass /*cls*/,
                       jlong ptr,
                       jboolean indent) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    std::string str;
    {
        VALDI_TRACE("Valdi.marshallerToString");
        str = marshaller->toString(static_cast<bool>(indent));
    }

    VALDI_TRACE("Valdi.marshallerToJavaString");
    return reinterpret_cast<jstring>(ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), str).releaseObject());
}

jstring nativeToStringAtIndex(JNIEnv* env, jclass /*cls*/, jlong ptr, jint index, jboolean indent) {
    auto* marshaller = unwrap(env, ptr);
    if (marshaller == nullptr) {
        return nullptr;
    }

    auto getResult = marshaller->get(static_cast<int>(index));
    if (!checkMarshaller(env, marshaller)) {
        return nullptr;
    }

    return reinterpret_cast<jstring>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), getResult.toString(static_cast<bool>(indent)))
            .releaseObject());
}

jboolean nativeEquals(jlong ptr, jlong otherPtr) {
    auto* marshaller = unwrap(nullptr, ptr);
    if (marshaller == nullptr) {
        return static_cast<jboolean>(false);
    }
    auto* otherMarshaller = unwrap(nullptr, otherPtr);
    if (otherMarshaller == nullptr) {
        return static_cast<jboolean>(false);
    }

    return static_cast<jboolean>(*marshaller == *otherMarshaller);
}

jdouble nativeGetMapPropertyDouble(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<double>(env, ptr, propPtr, index, false);
    return static_cast<jdouble>(result ? result.value() : 0.0);
}

jlong nativeGetMapPropertyLong(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<int64_t>(env, ptr, propPtr, index, false);
    return static_cast<jlong>(result ? result.value() : 0);
}

jboolean nativeGetMapPropertyBoolean(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<bool>(env, ptr, propPtr, index, false);
    return static_cast<jboolean>(result ? result.value() : false);
}

jstring nativeGetMapPropertyString(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::StringBox>(env, ptr, propPtr, index, false);

    if (!result) {
        return nullptr;
    }

    return reinterpret_cast<jstring>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), result.value()).releaseObject());
}

jstring nativeGetMapPropertyOptionalString(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::StringBox>(env, ptr, propPtr, index, true);

    if (!result) {
        return nullptr;
    }

    return reinterpret_cast<jstring>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), result.value()).releaseObject());
}

jobject nativeGetMapPropertyFunction(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::Ref<Valdi::ValueFunction>>(env, ptr, propPtr, index, false);

    if (!result) {
        return nullptr;
    }
    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), result.value()).releaseObject();
}

jobject nativeGetMapPropertyOptionalFunction(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::Ref<Valdi::ValueFunction>>(env, ptr, propPtr, index, true);

    if (!result) {
        return nullptr;
    }
    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), result.value()).releaseObject();
}

jobject nativeGetMapPropertyOpaque(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::Ref<Valdi::ValdiObject>>(env, ptr, propPtr, index, false);
    if (!result) {
        return nullptr;
    }

    return ValdiAndroid::javaObjectFromValdiObject(result.value(), true).releaseObject();
}

jbyteArray nativeGetMapPropertyByteArray(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::Ref<Valdi::ValueTypedArray>>(env, ptr, propPtr, index, false);
    if (!result) {
        return nullptr;
    }

    return reinterpret_cast<jbyteArray>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), result.value()).releaseObject());
}

jbyteArray nativeGetMapPropertyOptionalByteArray(JNIEnv* env, jclass /*cls */, jlong ptr, jlong propPtr, jint index) {
    auto result = jniGetMapPropertyAndReturnValue<Valdi::Ref<Valdi::ValueTypedArray>>(env, ptr, propPtr, index, true);

    return reinterpret_cast<jbyteArray>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), result.value()).releaseObject());
}

#define ValdiMakeNativeMethod(name) makeNativeMethod(#name, name)
#define ValdiMakeCriticalNativeMethod(name) makeCriticalNativeMethod_DO_NOT_USE_OR_YOU_WILL_BE_FIRED(#name, name)

djinni::JniClassInitializer jvmMarshallerJNIOnLoad([] { // NOLINT(fuchsia-statically-constructed-objects)
    JavaVM* vm = nullptr;
    djinni::jniGetThreadEnv()->GetJavaVM(&vm);

    facebook::jni::initialize(vm, [] {
        facebook::jni::registerNatives("com/snap/valdi/utils/ValdiMarshallerCPP",
                                       {ValdiMakeCriticalNativeMethod(nativeCreate),
                                        ValdiMakeCriticalNativeMethod(nativeDestroy),
                                        ValdiMakeCriticalNativeMethod(nativeSize),

                                        ValdiMakeNativeMethod(nativePutMapProperty),

                                        ValdiMakeNativeMethod(nativePutMapPropertyInterned),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedString),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedDouble),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedLong),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedBoolean),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedFunction),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedByteArray),
                                        ValdiMakeNativeMethod(nativePutMapPropertyInternedOpaque),

                                        ValdiMakeNativeMethod(nativeGetMapProperty),
                                        ValdiMakeNativeMethod(nativePushMapIterator),
                                        ValdiMakeNativeMethod(nativeMapIteratorPushNext),
                                        ValdiMakeNativeMethod(nativeMapIteratorPushNextAndPopPrevious),

                                        ValdiMakeNativeMethod(nativeGetTypedObjectProperty),
                                        ValdiMakeNativeMethod(nativeUnwrapProxy),

                                        ValdiMakeNativeMethod(nativeSetArrayItem),

                                        ValdiMakeNativeMethod(nativePushString),
                                        ValdiMakeNativeMethod(nativeSetError),
                                        ValdiMakeNativeMethod(nativeCheckError),
                                        ValdiMakeNativeMethod(nativePushOpaqueObject),
                                        ValdiMakeNativeMethod(nativePushBigInt),
                                        ValdiMakeNativeMethod(nativePushFunction),
                                        ValdiMakeNativeMethod(nativePushByteArray),

                                        ValdiMakeCriticalNativeMethod(nativePushMap),
                                        ValdiMakeCriticalNativeMethod(nativePushArray),
                                        ValdiMakeCriticalNativeMethod(nativePushInternedString),
                                        ValdiMakeCriticalNativeMethod(nativePushNull),
                                        ValdiMakeCriticalNativeMethod(nativePushUndefined),
                                        ValdiMakeCriticalNativeMethod(nativePushBoolean),
                                        ValdiMakeCriticalNativeMethod(nativePushDouble),
                                        ValdiMakeCriticalNativeMethod(nativePushLong),
                                        ValdiMakeCriticalNativeMethod(nativePushCppObject),

                                        ValdiMakeCriticalNativeMethod(nativeGetType),

                                        ValdiMakeCriticalNativeMethod(nativeGetBoolean),
                                        ValdiMakeCriticalNativeMethod(nativeGetDouble),
                                        ValdiMakeCriticalNativeMethod(nativeGetLong),
                                        ValdiMakeNativeMethod(nativeGetUntyped),
                                        ValdiMakeNativeMethod(nativeGetString),
                                        ValdiMakeNativeMethod(nativeGetInternedString),
                                        ValdiMakeNativeMethod(nativeGetNativeWrapper),
                                        ValdiMakeNativeMethod(nativeGetOpaqueObject),
                                        ValdiMakeNativeMethod(nativeGetByteArray),
                                        ValdiMakeNativeMethod(nativeGetFunction),

                                        ValdiMakeNativeMethod(nativeGetArrayLength),
                                        ValdiMakeNativeMethod(nativeGetArrayItem),
                                        ValdiMakeNativeMethod(nativeGetArrayItemAndPopPrevious),

                                        ValdiMakeCriticalNativeMethod(nativeEquals),

                                        ValdiMakeNativeMethod(nativePop),
                                        ValdiMakeNativeMethod(nativePopCount),

                                        ValdiMakeNativeMethod(nativeGetMapPropertyDouble),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyLong),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyString),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyBoolean),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyOptionalString),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyFunction),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyOptionalFunction),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyOpaque),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyByteArray),
                                        ValdiMakeNativeMethod(nativeGetMapPropertyOptionalByteArray),

                                        ValdiMakeNativeMethod(nativeToString),

                                        ValdiMakeNativeMethod(nativeToStringAtIndex)});
    });
});
