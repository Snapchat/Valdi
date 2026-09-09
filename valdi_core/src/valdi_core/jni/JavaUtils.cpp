//
//  JavaUtils.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/9/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/jni/JavaUtils.hpp"
#include "djinni/jni/djinni_support.hpp"
#include "valdi_core/NativeAsset.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Resources/Asset.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/LazyValueConvertible.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include "valdi_core/jni/GlobalRefJavaObject.hpp"
#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaClass.hpp"
#include "valdi_core/jni/JavaFunctionTrampolineHelper.hpp"
#include "valdi_core/jni/JavaObject.hpp"
#include "valdi_core/jni/ValueFunctionWithJavaFunction.hpp"

#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include <fbjni/detail/utf8.h>

namespace ValdiAndroid {

class JavaProxyObject : public Valdi::ValueTypedProxyObject {
public:
    JavaProxyObject(jobject object, const Valdi::Ref<Valdi::ValueTypedObject>& typedObject)
        : ValueTypedProxyObject(typedObject), _value(IndirectJavaGlobalRef::make(object, "Java Proxy")) {}

    ~JavaProxyObject() override = default;

    std::string_view getType() const final {
        return "Java Proxy";
    }

    djinni::LocalRef<jobject> get() const {
        return _value.get();
    }

private:
    IndirectJavaGlobalRef _value;
};

template<typename T>
T typedNewJavaObjectWrappingValue(JavaEnv env, const Valdi::Value& valueToWrap) {
    auto* wrappedValue = new Valdi::Value(valueToWrap);
    auto object = JavaEnv::getCache().getCppObjectWrapperClass().newObject(
        JavaEnv::getCache().getCppObjectWrapperConstructorMethod(), reinterpret_cast<int64_t>(wrappedValue));

    return T(env, std::move(object));
}

JavaObject newJavaObjectWrappingValue(JavaEnv env, const Valdi::Value& valueToWrap) {
    return typedNewJavaObjectWrappingValue<JavaObject>(env, valueToWrap);
}

template<class T>
T typedJavaObjectFromValdiObject(const Valdi::SharedValdiObject& object, bool wrapIfNeeded) {
    auto* globalRefObject = dynamic_cast<GlobalRefJavaObjectBase*>(object.get());
    if (globalRefObject == nullptr) {
        auto asset = Valdi::castOrNull<Valdi::Asset>(object);
        if (asset != nullptr) {
            auto assetJavaRef = djinni_generated_client::valdi_core::NativeAsset::fromCpp(
                ValdiAndroid::JavaEnv::getUnsafeEnv(), asset.toShared());
            return T(JavaEnv(), std::move(assetJavaRef));
        }

        if (wrapIfNeeded && object != nullptr) {
            // This is a non Java object, wrap it
            return typedNewJavaObjectWrappingValue<T>(JavaEnv(), Valdi::Value(object));
        }

        return T(JavaEnv(), nullptr);
    }

    auto ref = globalRefObject->get();
    return T(globalRefObject->getEnv(), std::move(ref));
}

jfieldID getNativeHandleFieldId() {
    return JavaEnv::getCache().getNativeHandleWrapperClass().getFieldId<int64_t>("mNativeHandle");
}

jlong getNativeHandleFromWrapper(JavaEnv /*javaEnv*/, jobject nativeHandleWrapper) {
    static auto* kNativeHandleFieldId = getNativeHandleFieldId();

    return JavaEnv::accessEnvRet(
        [&](JNIEnv& env) -> jlong { return env.GetLongField(nativeHandleWrapper, kNativeHandleFieldId); });
}

JavaValue toJavaValue(JavaEnv /*env*/, int32_t value) {
    return JavaValue::makeInt(value);
}

JavaValue toJavaValue(JavaEnv /*env*/, int64_t value) {
    return JavaValue::makeLong(value);
}

JavaValue toJavaValue(JavaEnv /*env*/, float value) {
    return JavaValue::makeFloat(value);
}

JavaValue toJavaValue(JavaEnv /*env*/, double value) {
    return JavaValue::makeDouble(value);
}

JavaValue toJavaValue(JavaEnv /*env*/, bool value) {
    return JavaValue::makeBoolean(value ? 1 : 0);
}

JavaValue toJavaValue(JavaEnv env, const Valdi::Value& weakValue) {
    auto outObject = toJavaObject(env, weakValue);
    return std::move(outObject.getValue());
}

JavaValue toJavaValue(JavaEnv env, const std::string& str) {
    auto outObject = toJavaObject(env, str);
    return std::move(outObject.getValue());
}

JavaValue toJavaValue(JavaEnv /*env*/, jobject javaObject) {
    return JavaValue::unsafeMakeObject(javaObject);
}

JavaValue toJavaValue(JavaEnv env, const JavaObject& object) {
    return toJavaValue(env, object.getUnsafeObject());
}

JavaValue toJavaValue(JavaEnv /*env*/, djinni::LocalRef<jobject> object) {
    return JavaValue::makeObject(std::move(object));
}

JavaValue toJavaValue(JavaEnv /*env*/, VoidType /*voidObject*/) {
    return JavaValue::unsafeMakeObject(nullptr);
}

JavaObject toJavaObject(JavaEnv env, const Valdi::Value& weakValue) {
    switch (weakValue.getType()) {
        case Valdi::ValueType::Null:
            return JavaObject(env);
        case Valdi::ValueType::Undefined:
            return undefinedValueJavaObject(env);
        case Valdi::ValueType::InternedString:
            return toJavaObject(env, weakValue.toStringBox());
        case Valdi::ValueType::StaticString:
            return toJavaObject(env, *weakValue.getStaticString());
        case Valdi::ValueType::Int:
            return toJavaObject(env, weakValue.toInt());
        case Valdi::ValueType::Long:
            return toJavaObject(env, weakValue.toLong());
        case Valdi::ValueType::Double:
            return toJavaObject(env, weakValue.toDouble());
        case Valdi::ValueType::Bool:
            return toJavaObject(env, weakValue.toBool());
        case Valdi::ValueType::Map:
            return toJavaObject(env, weakValue.getMapRef());
        case Valdi::ValueType::Array:
            return toJavaObject(env, weakValue.getArrayRef());
        case Valdi::ValueType::TypedArray:
            return toJavaObject(env, weakValue.getTypedArrayRef());
        case Valdi::ValueType::Function:
            return toJavaObject(env, weakValue.getFunctionRef());
        case Valdi::ValueType::TypedObject:
            return toJavaObject(env, weakValue.getTypedObjectRef());
        case Valdi::ValueType::ProxyTypedObject:
            return toJavaObject(env, weakValue.getTypedProxyObjectRef());
        case Valdi::ValueType::ValdiObject:
            return javaObjectFromValdiObject(weakValue.getValdiObject(), true);
        case Valdi::ValueType::Error:
            return toJavaObject(env, weakValue.getError());
    }
}

JavaObject undefinedValueJavaObject(JavaEnv /* env */) {
    return JavaCache::getUndefinedValueClassInstance();
}

JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueMap>& value) {
    if (value == nullptr) {
        return JavaObject(env, nullptr);
    }

    auto& cache = JavaEnv::getCache();
    auto out = cache.getHashMapClass().newObject(cache.getHashMapConstructorMethod());

    for (const auto& it : *value) {
        auto convertedKey = toJavaObject(env, it.first);
        auto convertedValue = toJavaObject(env, it.second);
        cache.getHashMapPutMethod().call(out, convertedKey, convertedValue);
    }

    return JavaObject(env, std::move(out));
}

JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueTypedObject>& value) {
    if (value == nullptr) {
        return JavaObject(env, nullptr);
    }

    auto& cache = JavaEnv::getCache();
    auto out = cache.getHashMapClass().newObject(cache.getHashMapConstructorMethod());

    for (const auto& it : *value) {
        auto convertedKey = toJavaObject(env, it.name);
        auto convertedValue = toJavaObject(env, it.value);
        cache.getHashMapPutMethod().call(out, convertedKey, convertedValue);
    }

    return JavaObject(env, std::move(out));
}

JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueTypedProxyObject>& value) {
    auto* javaProxy = dynamic_cast<JavaProxyObject*>(value.get());
    if (javaProxy == nullptr) {
        return JavaObject(env);
    }

    return JavaObject(env, javaProxy->get());
}

JavaObject toJavaObject(JavaEnv env, int32_t value) {
    return JavaObject(env, newJavaIntObject(value));
}

JavaObject toJavaObject(JavaEnv env, int64_t value) {
    return JavaObject(env, newJavaLongObject(value));
}

JavaObject toJavaObject(JavaEnv env, double value) {
    return JavaObject(env, newJavaDoubleObject(value));
}

JavaObject toJavaObject(JavaEnv env, bool value) {
    return JavaObject(env, newJavaBooleanObject(value));
}

static djinni::LocalRef<jobject> newValdiException(JavaEnv env, const Valdi::Error& error) {
    auto convertedMessage = toJavaObject(env, error.getMessage());
    // Carry the code when there is one, so a cancellation stays distinguishable from a genuine
    // failure on the Kotlin side (see Throwable.valdiErrorCode) instead of only the message
    // surviving. Errors without a code keep using the message-only constructor.
    const auto errorCode = error.getErrorCode();
    if (errorCode != 0) {
        return JavaEnv::getCache().getValdiExceptionClass().newObject(
            JavaEnv::getCache().getValdiExceptionConstructorWithErrorCodeMethod(),
            std::move(convertedMessage),
            errorCode);
    }
    return JavaEnv::getCache().getValdiExceptionClass().newObject(
        JavaEnv::getCache().getValdiExceptionConstructorMethod(), std::move(convertedMessage));
}

JavaObject toJavaObject(JavaEnv env, const Valdi::Error& error) {
    return JavaObject(env, newValdiException(env, error));
}

Throwable toJavaThrowable(JavaEnv env, const Valdi::Error& error) {
    return Throwable(env, newValdiException(env, error));
}

String toJavaObject(JavaEnv env, const Valdi::StringBox& str) {
    return toJavaObject(env, str.toStringView());
}

String toJavaObject(JavaEnv env, const Valdi::StaticString& str) {
    auto stringObject = newJavaString(str);
    return String(env, ::djinni::LocalRef<jobject>(reinterpret_cast<jobject>(stringObject.release())));
}

String toJavaObject(JavaEnv env, const std::string_view& str) {
    auto stringObject = newJavaStringUTF8(str);

    return String(env, ::djinni::LocalRef<jobject>(reinterpret_cast<jobject>(stringObject.release())));
}

djinni::LocalRef<jstring> newJavaString(const Valdi::StaticString& str) {
    switch (str.encoding()) {
        case Valdi::StaticString::Encoding::UTF8:
            return newJavaStringUTF8(str.utf8StringView());
        case Valdi::StaticString::Encoding::UTF16:
            return newJavaStringUTF16(str.utf16StringView());
        case Valdi::StaticString::Encoding::UTF32: {
            auto storage = str.utf16Storage();
            return newJavaStringUTF16(storage.toStringView());
        }
    }
}

djinni::LocalRef<jstring> newJavaStringUTF8(const std::string_view& str) {
    size_t length;
    auto modifiedLength = facebook::jni::detail::modifiedLength(reinterpret_cast<const uint8_t*>(str.data()), &length);

    if (modifiedLength == str.size()) {
        return JavaEnv::accessEnvRetRef([&](JNIEnv& env) { return env.NewStringUTF(str.data()); });
    } else {
        Valdi::SmallVector<uint8_t, 128> buffer;
        // Need additional byte for \0 at the end
        buffer.resize(modifiedLength + 1);

        facebook::jni::detail::utf8ToModifiedUTF8(
            reinterpret_cast<const uint8_t*>(str.data()), str.size(), buffer.data(), buffer.size());

        return JavaEnv::accessEnvRetRef(
            [&](JNIEnv& env) { return env.NewStringUTF(reinterpret_cast<const char*>(buffer.data())); });
    }
}

djinni::LocalRef<jstring> newJavaStringUTF16(const std::u16string_view& str) {
    return JavaEnv::accessEnvRetRef(
        [&](JNIEnv& env) { return env.NewString(reinterpret_cast<const jchar*>(str.data()), str.size()); });
}

ObjectArray toJavaObject(JavaEnv env, const Valdi::Value* values, size_t size) {
    auto outArray = newJavaObjectArray(size);

    auto* jniEnv = JavaEnv::getUnsafeEnv();

    for (size_t i = 0; i < size; i++) {
        const auto& value = values[i];
        if (value.isNull()) {
            continue;
        }
        auto objectElement = toJavaObject(env, value);
        jniEnv->SetObjectArrayElement(outArray, static_cast<jsize>(i), objectElement.getUnsafeObject());
    }

    return ObjectArray(env, djinni::LocalRef<jobject>(reinterpret_cast<jobject>(outArray.release())));
}

ObjectArray toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueArray>& values) {
    if (values == nullptr) {
        return ObjectArray(env, nullptr);
    }

    return toJavaObject(env, values->begin(), values->size());
}

ObjectArray toJavaObject(JavaEnv env, const std::vector<Valdi::Value>& value) {
    return toJavaObject(env, value.data(), value.size());
}

ByteArray toJavaObject(JavaEnv env, const Valdi::BytesView& value) {
    return toJavaObject(env, value.data(), value.size());
}

ByteArray toJavaObject(JavaEnv env, const Valdi::Byte* data, size_t length) {
    auto byteArray = newJavaByteArray(data, length);
    return ByteArray(env, ::djinni::LocalRef<jobject>(byteArray.release()));
}

ByteArray toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueTypedArray>& value) {
    if (value == nullptr) {
        return ByteArray(env, nullptr);
    }

    return toJavaObject(env, value->getBuffer());
}

JavaObject toJavaObject(JavaEnv env, const Valdi::Ref<Valdi::ValueFunction>& value) {
    if (value == nullptr) {
        return JavaObject(env, nullptr);
    }

    auto javaFunction = Valdi::castOrNull<ValueFunctionWithJavaFunction>(value);
    if (javaFunction != nullptr) {
        // unwrap the underlying Java object
        return javaFunction->toObject();
    }

    auto* functionCopy = new Valdi::Value(value);

    auto actionObject = JavaEnv::getCache().getValdiFunctionNativeClass().newObject(
        JavaEnv::getCache().getValdiFunctionNativeConstructorMethod(), reinterpret_cast<int64_t>(functionCopy));
    return JavaObject(env, std::move(actionObject));
}

djinni::LocalRef<jobject> newJavaBooleanObject(bool value) {
    auto& cache = JavaCache::get();
    return cache.getBooleanClass().newObject(cache.getBooleanConstructorMethod(), value);
}

djinni::LocalRef<jobject> newJavaFloatObject(float value) {
    auto& cache = JavaCache::get();
    return cache.getFloatClass().newObject(cache.getFloatConstructorMethod(), value);
}

djinni::LocalRef<jobject> newJavaDoubleObject(double value) {
    auto& cache = JavaCache::get();
    return cache.getDoubleClass().newObject(cache.getDoubleConstructorMethod(), value);
}

djinni::LocalRef<jobject> newJavaIntObject(int32_t value) {
    auto& cache = JavaCache::get();
    return cache.getIntegerClass().newObject(cache.getIntegerConstructorMethod(), value);
}

djinni::LocalRef<jobject> newJavaLongObject(int64_t value) {
    auto& cache = JavaCache::get();
    return cache.getLongClass().newObject(cache.getLongConstructorMethod(), value);
}

djinni::LocalRef<jbyteArray> newJavaByteArray(const Valdi::Byte* data, size_t length) {
    const auto bufferSize = static_cast<jsize>(length);

    return JavaEnv::accessEnvRetRef([&](JNIEnv& env) -> jbyteArray {
        auto* byteArray = env.NewByteArray(bufferSize);

        if (byteArray != nullptr) {
            env.SetByteArrayRegion(byteArray, 0, bufferSize, reinterpret_cast<jbyte const*>(data));
        }

        return byteArray;
    });
}

djinni::LocalRef<jobjectArray> newJavaObjectArray(size_t length) {
    auto& cache = JavaEnv::getCache();
    return JavaEnv::accessEnvRetRef([&](JNIEnv& env) -> jobjectArray {
        return env.NewObjectArray(static_cast<jsize>(length), cache.getObjectClass().getClass(), nullptr);
    });
}

Valdi::StringBox toInternedString(JavaEnv env, const String& string) {
    return toInternedString(env, reinterpret_cast<jstring>(string.getUnsafeObject()));
}

Valdi::StringBox toInternedString(JavaEnv env, jstring string) {
    return Valdi::StringCache::getGlobal().makeString(toStdString(env, string));
}

std::string toStdString(JavaEnv env, const String& string) {
    return toStdString(env, reinterpret_cast<jstring>(string.getUnsafeObject()));
}

std::string toStdString(JavaEnv /*env*/, jstring string) {
    if (string == nullptr) {
        return "";
    }

    JNIEnv* jni = JavaEnv::getUnsafeEnv();
    auto length = static_cast<size_t>(jni->GetStringLength(string));
    const auto* jchars = jni->GetStringChars(string, nullptr);

    auto utf8String = facebook::jni::detail::utf16toUTF8(reinterpret_cast<const uint16_t*>(jchars), length);

    jni->ReleaseStringChars(string, jchars);

    return utf8String;
}

Valdi::Ref<Valdi::StaticString> toStaticString(JavaEnv /*env*/, jstring string) {
    if (string == nullptr) {
        return Valdi::StaticString::makeUTF8("");
    }

    JNIEnv* jni = JavaEnv::getUnsafeEnv();
    auto length = jni->GetStringLength(string);
    auto out = Valdi::StaticString::makeUTF16(static_cast<size_t>(length));

    jni->GetStringRegion(string, 0, length, reinterpret_cast<jchar*>(out->utf16Data()));

    return out;
}

jlong createInternedString(JavaEnv env, jstring string) {
    auto stringBox = toInternedString(env, string);
    if (stringBox.isEmpty()) {
        return 0;
    }

    return reinterpret_cast<jlong>(Valdi::unsafeBridgeRetain(stringBox.getInternedString().get()));
}

void destroyInternedString(jlong ptr) {
    Valdi::unsafeBridgeRelease(reinterpret_cast<void*>(ptr));
}

Valdi::StringBox unwrapInternedString(jlong ptr) {
    if (ptr == 0) {
        return Valdi::StringBox();
    }

    return Valdi::StringBox(Valdi::unsafeBridge<Valdi::InternedStringImpl>(reinterpret_cast<void*>(ptr)));
}

Valdi::BytesView toByteArray(JavaEnv /*env*/, jbyteArray array) {
    auto jLength = JavaEnv::accessEnvRet([&](JNIEnv& env) -> jint { return env.GetArrayLength(array); });

    auto bytes = Valdi::makeShared<Valdi::Bytes>();
    bytes->resize(static_cast<size_t>(jLength));

    JavaEnv::accessEnv([&](JNIEnv& env) {
        return env.GetByteArrayRegion(array, 0, jLength, reinterpret_cast<jbyte*>(bytes->data()));
    });

    return Valdi::BytesView(bytes);
}

JavaObject toByteBuffer(JavaEnv env, void* data, size_t size) {
    auto object = JavaEnv::accessEnvRetRef([&](JNIEnv& env) -> auto { return env.NewDirectByteBuffer(data, size); });

    return JavaObject(env, std::move(object));
}

JavaObject toJavaFloatArray(JavaEnv env, std::initializer_list<float> values) {
    return toJavaFloatArray(env, values.begin(), values.size());
}

JavaObject toJavaFloatArray(JavaEnv env, const float* values, size_t size) {
    auto object = JavaEnv::accessEnvRetRef([&](JNIEnv& env) -> auto {
        auto* array = env.NewFloatArray(static_cast<jsize>(size));
        if (array != nullptr) {
            env.SetFloatArrayRegion(array, 0, static_cast<jsize>(size), values);
        }
        return reinterpret_cast<jobject>(array);
    });

    return JavaObject(env, std::move(object));
}

JavaObject toJavaLongArray(JavaEnv env, const int64_t* values, size_t size) {
    auto object = JavaEnv::accessEnvRetRef([&](JNIEnv& env) -> auto {
        auto* array = env.NewLongArray(static_cast<jsize>(size));
        if (array != nullptr) {
            env.SetLongArrayRegion(array, 0, static_cast<jsize>(size), values);
        }
        return reinterpret_cast<jobject>(array);
    });

    return JavaObject(env, std::move(object));
}

Valdi::Value valueFromJavaByteArray(JavaEnv env, jbyteArray array) {
    if (array == nullptr) {
        return Valdi::Value();
    }

    auto bytes = toByteArray(env, array);
    return Valdi::Value(Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType, bytes));
}

template<typename T>
void iterateThrough(JavaEnv /* env */, ::djinni::LocalRef<jobject> iterable, T&& func) {
    auto iterator = JavaIterator::fromIterable(iterable.get());
    iterable.reset();

    for (;;) {
        auto next = iterator.next();
        if (!next) {
            break;
        }
        func(next.value().stealLocalRef());
    }
}

Valdi::Value valueFromJavaValdiFunction(JavaEnv env, jobject object) {
    return Valdi::Value(Valdi::makeShared<ValueFunctionWithJavaFunction>(env, object));
}

Valdi::Value valueFromJavaCppHandle(jlong handle) {
    auto* value = reinterpret_cast<Valdi::Value*>(handle);
    if (value != nullptr) {
        return *value;
    } else {
        return Valdi::Value();
    }
}

Valdi::Value toValue(JavaEnv env, ::djinni::LocalRef<jobject> object) {
    if (object == nullptr) {
        return Valdi::Value();
    }

    auto& cache = JavaEnv::getCache();

    if (JavaEnv::instanceOf(object, cache.getStringClass())) {
        auto string = toInternedString(env, reinterpret_cast<jstring>(object.get()));
        return Valdi::Value(string);
    } else if (JavaEnv::instanceOf(object, cache.getBooleanClass())) {
        auto result = cache.getBooleanBooleanValueMethod().call(object);
        return Valdi::Value(result);
    } else if (JavaEnv::instanceOf(object, cache.getNumberClass())) {
        auto result = cache.getNumberDoubleValueMethod().call(object);
        return Valdi::Value(result);
    } else if (JavaEnv::instanceOf(object, cache.getMapClass())) {
        auto weakValueMap = Valdi::makeShared<Valdi::ValueMap>();

        auto entrySet = cache.getMapEntrySetMethod().call(object);
        object.reset();

        iterateThrough(env, entrySet.stealLocalRef(), [&](::djinni::LocalRef<jobject> obj) {
            auto key = cache.getMapEntryGetKeyMethod().call(obj);
            auto value = cache.getMapEntryGetValueMethod().call(obj);
            obj.reset();

            auto keyValue = toValue(env, key.stealLocalRef());
            auto valueValue = toValue(env, value.stealLocalRef());

            (*weakValueMap)[keyValue.toStringBox()] = valueValue;
        });

        return Valdi::Value(weakValueMap);
    } else if (JavaEnv::instanceOf(object, cache.getIterableClass())) {
        std::vector<Valdi::Value> weakValues;

        iterateThrough(env, std::move(object), [&](::djinni::LocalRef<jobject> obj) {
            weakValues.emplace_back(toValue(env, std::move(obj)));
        });

        return Valdi::Value(Valdi::ValueArray::make(weakValues.data(), weakValues.data() + weakValues.size()));
    } else if (JavaEnv::instanceOf(object, cache.getCppObjectWrapperClass())) {
        // This needs need to be done before the ValdiFunctionClass, as
        // ValdiFunctionNative inherits from CppObjectWrapper, so that
        // we can directly take the function wrapped in the value and
        // avoid the unnecessary bridging.
        auto handle = getNativeHandleFromWrapper(env, object.get());
        object.reset();

        return valueFromJavaCppHandle(handle);
    } else if (JavaEnv::instanceOf(object, cache.getValdiFunctionClass())) {
        return valueFromJavaValdiFunction(env, object.get());
    } else if (JavaEnv::instanceOf(object, cache.getObjectArrayClass())) {
        JavaObjectArray array(reinterpret_cast<jobjectArray>(object.get()));
        auto length = array.size();
        auto out = Valdi::ValueArray::make(length);

        for (size_t i = 0; i < length; i++) {
            out->emplace(i, toValue(env, array.getObject(i)));
        }

        return Valdi::Value(out);
    } else if (JavaEnv::instanceOf(object, cache.getByteArrayClass())) {
        auto* byteArray = reinterpret_cast<jbyteArray>(object.get());
        return valueFromJavaByteArray(env, byteArray);
    } else if (JavaEnv::instanceOf(object, cache.getUndefinedValueClass())) {
        return Valdi::Value::undefined();
    } else if (JavaEnv::instanceOf(object, cache.getValdiMarshallableClass())) {
        Valdi::SimpleExceptionTracker exceptionTracker;
        Valdi::Marshaller marshaller(exceptionTracker);
        auto itemIndex =
            cache.getValdiMarshallerCPPPushMarshallableMethod().call(cache.getValdiMarshallerCPPClass().getClass(),
                                                                     ValdiMarshallableType(env, object.get()),
                                                                     reinterpret_cast<int64_t>(&marshaller));

        return marshaller.get(static_cast<int>(itemIndex));
    } else {
        if (JavaEnv::instanceOf(object, cache.getAssetClass())) {
            auto asset = Valdi::castOrNull<Valdi::Asset>(
                djinni_generated_client::valdi_core::NativeAsset::toCpp(ValdiAndroid::JavaEnv::getUnsafeEnv(), object));
            if (asset != nullptr) {
                return Valdi::Value(asset);
            }
        }

        auto nativeObject = valdiObjectFromJavaObject(JavaObject(env, object.get()), "OpaqueJavaObject");
        return Valdi::Value(nativeObject);
    }
}

JavaObject javaObjectFromValdiObject(const Valdi::SharedValdiObject& object) {
    return javaObjectFromValdiObject(object, false);
}

JavaObject javaObjectFromValdiObject(const Valdi::SharedValdiObject& object, bool wrapIfNeeded) {
    return typedJavaObjectFromValdiObject<JavaObject>(object, wrapIfNeeded);
}

Valdi::Ref<Valdi::RefCountable> wrappedCppPtrFromNativeHandle(int64_t nativeHandle) {
    auto* wrappedValue = reinterpret_cast<Valdi::Value*>(nativeHandle);
    if (wrappedValue == nullptr) {
        return nullptr;
    }

    return wrappedValue->getUnsafeTypedRefPointer<Valdi::RefCountable>();
}

Valdi::SharedValdiObject valdiObjectFromJavaObject(const JavaObject& javaObject, const char* tag) {
    if (javaObject.isNull()) {
        return nullptr;
    }

    return Valdi::makeShared<GlobalRefJavaObject>(javaObject.getEnv(), javaObject.getUnsafeObject(), tag);
}

Valdi::PlatformResult platformResultFromJavaResult(JavaEnv env, jobject javaResult) {
    if (javaResult == nullptr) {
        return Valdi::Value();
    }

    auto isSuccess = JavaEnv::getCache().getValdiResultIsSuccessMethod().call(javaResult);
    if (isSuccess) {
        auto successValue = JavaEnv::getCache().getValdiResultGetSuccessValueMethod().call(javaResult);
        return toValue(env, successValue.stealLocalRef());
    } else {
        auto errorMessage = JavaEnv::getCache().getValdiResultGetErrorMessageMethod().call(javaResult);
        return Valdi::Error(toInternedString(env, errorMessage));
    }
}

JavaObject javaResultFromPlatformResult(JavaEnv env, const Valdi::PlatformResult& platformResult) {
    if (platformResult) {
        if (platformResult.value().isNullOrUndefined()) {
            // Success with no values are represented as null objects to avoid JNI calls in the good cases.
            return JavaObject(env);
        }

        auto convertedValue = toJavaObject(env, platformResult.value());
        return JavaEnv::getCache().getValdiResultSuccessMethod().call(
            JavaEnv::getCache().getValdiResultClass().getClass(), convertedValue);
    } else {
        auto convertedError = toJavaObject(env, platformResult.error().getMessage());
        return JavaEnv::getCache().getValdiResultFailureMethod().call(
            JavaEnv::getCache().getValdiResultClass().getClass(), convertedError);
    }
}

ValdiContext fromValdiContextUserData(const Valdi::SharedValdiObject& userData) {
    return typedJavaObjectFromValdiObject<ValdiContext>(userData, false);
}

Valdi::SharedValdiObject toValdiContextUserData(const ValdiContext& context) {
    return valdiObjectFromJavaObject(context, "ValdiContext");
}

Valdi::Marshaller* unwrapMarshaller(JNIEnv* env, jlong ptr) {
    auto* marshaller = reinterpret_cast<Valdi::Marshaller*>(ptr);
    if (VALDI_UNLIKELY(marshaller == nullptr)) {
        if (env != nullptr) {
            throwJavaValdiException(env, "Cannot use Marshaller once it has been destroyed.");
        }
    }

    return marshaller;
}

Valdi::Shared<Valdi::ValueConvertible> javaObjectToLazyValueConvertible(JavaEnv env,
                                                                        jobject javaObject,
                                                                        const char* tag) {
    if (javaObject == nullptr) {
        return nullptr;
    }

    auto globalRef = Valdi::makeShared<ValdiAndroid::GlobalRefJavaObject>(env, javaObject, tag, false);

    return Valdi::makeShared<Valdi::LazyValueConvertible>([globalRef, tag]() -> Valdi::Value {
        (void)tag; // [[ maybe_unused ]] is not allowed for lambda captures
        VALDI_TRACE_META("Valdi.convertJavaObject", tag);
        return globalRef->toValue();
    });
}

djinni::LocalRef<jobject> newWeakReference(JavaEnv env, jobject value) {
    return JavaCache::get().getWeakRefeferenceClass().newObject(JavaCache::get().getWeakRefeferenceConstructorMethod(),
                                                                JavaObject(env, value));
}

djinni::LocalRef<jobject> getWeakReferenceValue(JavaEnv /*env*/, jobject weakReference) {
    auto result = JavaCache::get().getWeakRefeferenceGetMethod().call(JavaObject(JavaEnv(), weakReference));

    return result.stealLocalRef();
}

jlong bridgeRetain(Valdi::RefCountable* nativeReference) {
    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(Valdi::unsafeBridgeRetain(nativeReference)));
}

jlong bridgeCast(Valdi::RefCountable* nativeReference) {
    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(Valdi::unsafeBridgeCast(nativeReference)));
}

void bridgeRelease(jlong jNativeReference) {
    Valdi::unsafeBridgeRelease(reinterpret_cast<void*>(jNativeReference));
}

Valdi::Ref<Valdi::ValueTypedProxyObject> newJavaProxy(jobject object,
                                                      const Valdi::Ref<Valdi::ValueTypedObject>& typedObject) {
    return Valdi::makeShared<JavaProxyObject>(object, typedObject);
}

std::string getJavaTypeSignatureFromTypeName(const std::string_view& typeName) {
    std::string out = "L";
    out += typeName;
    out += ";";

    return out;
}

std::string getJavaTypeSignatureFromClassName(const Valdi::StringBox& typeName) {
    return getJavaTypeSignatureFromTypeName(typeName.replacing('.', '/').toStringView());
}

std::string getJavaTypeSignatureFromFunctionValueSchema(const Valdi::ValueFunctionSchema& functionSchema) {
    return getJavaTypeSignatureFromClassName(
        JavaFunctionTrampolineHelper::get().getFunctionClassNameForArity(functionSchema.getParametersSize()));
}

std::string getJavaTypeSignatureFromValueSchema(const Valdi::ValueSchema& schema, bool treatArrayAsList) {
    if (schema.isUntyped()) {
        return kJniSigObject;
    } else if (schema.isVoid()) {
        return kJniSigVoid;
    } else if (schema.isString()) {
        return kJniSigString;
    } else if (schema.isInteger()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Integer");
        } else {
            return kJniSigInt;
        }
    } else if (schema.isLongInteger()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Long");
        } else {
            return kJniSigLong;
        }
    } else if (schema.isDouble()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Double");
        } else {
            return kJniSigDouble;
        }
    } else if (schema.isBoolean()) {
        if (schema.isBoxed()) {
            return JAVA_CLASS("java/lang/Boolean");
        } else {
            return kJniSigBool;
        }
    } else if (schema.isFunction()) {
        return getJavaTypeSignatureFromFunctionValueSchema(*schema.getFunction());
    } else if (schema.isClass()) {
        return getJavaTypeSignatureFromClassName(schema.getClass()->getClassName());
    } else if (schema.isEnum()) {
        return getJavaTypeSignatureFromClassName(schema.getEnum()->getName());
    } else if (schema.isValueTypedArray()) {
        return kJniSigByteArray;
    } else if (schema.isArray()) {
        if (treatArrayAsList) {
            return JAVA_CLASS("java/util/List");
        } else {
            std::string out = "[";
            out += getJavaTypeSignatureFromValueSchema(schema.getArrayItemSchema(), treatArrayAsList);

            return out;
        }
    } else if (schema.isMap()) {
        return JAVA_CLASS("java/util/Map");
    } else if (schema.isTypeReference()) {
        auto typeReference = schema.getTypeReference();
        SC_ASSERT(typeReference.isNamed());
        return getJavaTypeSignatureFromClassName(typeReference.getName());
    } else if (schema.isGenericTypeReference()) {
        const auto& typeReference = schema.getGenericTypeReference()->getType();
        SC_ASSERT(typeReference.isNamed());
        return getJavaTypeSignatureFromClassName(typeReference.getName());
    } else if (schema.isSchemaReference()) {
        return getJavaTypeSignatureFromValueSchema(schema.getReferencedSchema(), treatArrayAsList);
    } else if (schema.isPromise()) {
        return std::string(GetJNIType<Promise>()());
    } else {
        std::abort();
    }
}

std::optional<JavaValueType> schemaToJavaValueType(const Valdi::ValueSchema& schema) {
    if (schema.isInteger() && !schema.isBoxed()) {
        return JavaValueType::Int;
    } else if (schema.isLongInteger() && !schema.isBoxed()) {
        return JavaValueType::Long;
    } else if (schema.isDouble() && !schema.isBoxed()) {
        return JavaValueType::Double;
    } else if (schema.isBoolean() && !schema.isBoxed()) {
        return JavaValueType::Boolean;
    } else if (schema.isVoid()) {
        return std::nullopt;
    } else {
        return JavaValueType::Object;
    }
}

[[noreturn]] void handleFatalError(std::string_view message) {
    SC_ABORT(message.data());
    JavaEnv::accessEnv([&](JNIEnv& env) { throwJavaValdiException(&env, message.data()); });
    std::abort();
}

JavaIterator::JavaIterator(JavaObject&& iterator) : _iterator(std::move(iterator)) {}
JavaIterator::~JavaIterator() = default;

std::optional<JavaObject> JavaIterator::next() {
    auto& cache = JavaEnv::getCache();
    auto hasNext = cache.getIteratorHasNextMethod().call(_iterator);
    if (!hasNext) {
        return std::nullopt;
    }

    return std::make_optional(cache.getIteratorNextMethod().call(_iterator));
}

JavaIterator JavaIterator::fromIterable(jobject iterable) {
    return JavaIterator(JavaEnv::getCache().getIterableIteratorMethod().call(iterable));
}

JavaObjectArray::JavaObjectArray(jobjectArray array) : _array(array) {}

size_t JavaObjectArray::size() const {
    return static_cast<size_t>(JavaEnv::getUnsafeEnv()->GetArrayLength(_array));
}

djinni::LocalRef<jobject> JavaObjectArray::getObject(size_t index) const {
    return JavaEnv::accessEnvRetRef(
        [&](JNIEnv& jni) { return jni.GetObjectArrayElement(_array, static_cast<jsize>(index)); });
}

void JavaObjectArray::setObject(size_t index, jobject object) {
    JavaEnv::accessEnv(
        [&](JNIEnv& jni) { return jni.SetObjectArrayElement(_array, static_cast<jsize>(index), object); });
}

} // namespace ValdiAndroid
