//
//  JavaValueDelegate.cpp
//  valdi-android
//
//  Created by Simon Corsin on 2/14/23.
//

#include "valdi/android/JavaValueDelegate.hpp"
#include "valdi/android/JavaFunctionClassDelegate.hpp"
#include "valdi/android/JavaMethodClassDelegate.hpp"
#include "valdi_core/cpp/Utils/Promise.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"
#include "valdi_core/jni/IndirectJavaGlobalRef.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaFunctionTrampolineHelper.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

namespace ValdiAndroid {

class JavaPromise : public Valdi::Promise {
public:
    JavaPromise(jobject javaPromise, const Valdi::Ref<Valdi::ValueMarshaller<JavaValue>>& valueMarshaller)
        : _javaPromise(JavaEnv(), javaPromise, "Java Promise"), _valueMarshaller(valueMarshaller) {}
    ~JavaPromise() override = default;

    void onComplete(const Valdi::Ref<Valdi::PromiseCallback>& callback) final {
        auto& javaCache = JavaCache::get();
        auto javaCallback =
            javaCache.getCppPromiseCallbackClass().newObject(javaCache.getCppPromiseCallbackConstructorMethod(),
                                                             bridgeRetain(callback.get()),
                                                             bridgeRetain(_valueMarshaller.get()));
        javaCache.getPromiseOnCompleteMethod().call(_javaPromise.toObject(),
                                                    PromiseCallback(JavaEnv(), std::move(javaCallback)));
    }

    void cancel() final {
        JavaCache::get().getPromiseCancelMethod().call(_javaPromise.toObject());
    }

    bool isCancelable() const final {
        return JavaCache::get().getPromiseIsCancelableMethod().call(_javaPromise.toObject());
    }

private:
    GlobalRefJavaObjectBase _javaPromise;
    Valdi::Ref<Valdi::ValueMarshaller<JavaValue>> _valueMarshaller;
};

static JavaObject javaValueToObject(const JavaValue& obj) {
    return JavaObject(JavaEnv(), obj.getObject());
}

JavaObjectStore::JavaObjectStore() {
    auto& cache = JavaCache::get();
    auto weakMap = cache.getWeakHashMapClass().newObject(cache.getWeakHashMapConstructorMethod());
    _weakMap = IndirectJavaGlobalRef::make(weakMap.get(), "JavaObjectStoreWeakMap");
}

JavaObjectStore::~JavaObjectStore() = default;

Valdi::Ref<Valdi::RefCountable> JavaObjectStore::getValueForObjectKey(const JavaValue& objectKey,
                                                                      Valdi::ExceptionTracker& exceptionTracker) {
    auto& cache = JavaCache::get();
    auto result =
        cache.getWeakHashMapGetMethod().call(JavaObject(JavaEnv(), _weakMap.get()), javaValueToObject(objectKey));

    if (result.isNull()) {
        return nullptr;
    }

    auto nativeHandle = cache.getNativeRefGetNativeHandleMethod().call(result);

    return Valdi::unsafeBridge<Valdi::RefCountable>(reinterpret_cast<void*>(nativeHandle));
}

void JavaObjectStore::setValueForObjectKey(const JavaValue& objectKey,
                                           const Valdi::Ref<Valdi::RefCountable>& value,
                                           Valdi::ExceptionTracker& exceptionTracker) {
    auto& cache = JavaCache::get();
    auto nativeRef = cache.getNativeRefClass().newObject(
        cache.getNativeRefConstructorMethod(), reinterpret_cast<int64_t>(Valdi::unsafeBridgeRetain(value.get())));
    cache.getWeakHashMapPutMethod().call(
        JavaObject(JavaEnv(), _weakMap.get()), javaValueToObject(objectKey), JavaObject(JavaEnv(), nativeRef.get()));
}

std::optional<JavaValue> JavaObjectStore::getObjectForId(uint32_t objectId, Valdi::ExceptionTracker& exceptionTracker) {
    const auto& it = _objectById.find(objectId);
    if (it == _objectById.end()) {
        return std::nullopt;
    }

    auto weakRef = it->second.get();
    auto result = getWeakReferenceValue(JavaEnv(), weakRef.get());

    if (result == nullptr) {
        _objectById.erase(it);
        return std::nullopt;
    }

    return JavaValue::makeObject(std::move(result));
}

void JavaObjectStore::setObjectForId(uint32_t objectId,
                                     const JavaValue& object,
                                     Valdi::ExceptionTracker& exceptionTracker) {
    auto weakRef = newWeakReference(JavaEnv(), object.getObject());

    _objectById[objectId] = IndirectJavaGlobalRef::make(weakRef.get(), "JavaObjectStoreItem");
}

JavaValueDelegate::JavaValueDelegate(JavaTypesResolver& typesResolver) : _typesResolver(typesResolver) {
    // Initialize immediately in case something is wrong
    JavaFunctionTrampolineHelper::get();
}

JavaValueDelegate::~JavaValueDelegate() = default;

JavaValue JavaValueDelegate::newVoid() {
    return newNull();
}

JavaValue JavaValueDelegate::newNull() {
    return JavaValue::unsafeMakeObject(nullptr);
}

JavaValue JavaValueDelegate::newInt(int32_t value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeInt(value);
}

JavaValue JavaValueDelegate::newIntObject(int32_t value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaIntObject(value));
}

JavaValue JavaValueDelegate::newBigInt(const Valdi::Ref<Valdi::BigInt>& value,
                                       Valdi::ExceptionTracker& exceptionTracker) {
    return std::move(toJavaObject(JavaEnv(), value).getValue());
}

Valdi::Ref<Valdi::BigInt> JavaValueDelegate::valueToBigInt(const JavaValue& value,
                                                           Valdi::ExceptionTracker& exceptionTracker) const {
    auto object = value.getObject();
    if (!JavaEnv::instanceOf(object, JavaEnv::getCache().getBigIntClass())) {
        exceptionTracker.onError("Expected com.snap.valdi.BigInt");
        return nullptr;
    }
    return bigIntFromJavaObject(object);
}

JavaValue JavaValueDelegate::newLong(int64_t value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeLong(value);
}

JavaValue JavaValueDelegate::newLongObject(int64_t value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaLongObject(value));
}

JavaValue JavaValueDelegate::newDouble(double value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeDouble(value);
}

JavaValue JavaValueDelegate::newDoubleObject(double value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaDoubleObject(value));
}

JavaValue JavaValueDelegate::newBool(bool value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeBoolean(value ? 1 : 0);
}

JavaValue JavaValueDelegate::newBoolObject(bool value, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaBooleanObject(value));
}

JavaValue JavaValueDelegate::newStringUTF8(std::string_view str, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaStringUTF8(str));
}

JavaValue JavaValueDelegate::newStringUTF16(std::u16string_view str, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaStringUTF16(str));
}

JavaValue JavaValueDelegate::newByteArray(const Valdi::BytesView& bytes, Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(newJavaByteArray(bytes.data(), bytes.size()));
}

JavaValue JavaValueDelegate::newUntyped(const Valdi::Value& value,
                                        const Valdi::ReferenceInfoBuilder& /*referenceInfoBuilder*/,
                                        Valdi::ExceptionTracker& exceptionTracker) {
    return std::move(ValdiAndroid::toJavaObject(JavaEnv(), value).getValue());
}

JavaValue JavaValueDelegate::newMap(size_t capacity, Valdi::ExceptionTracker& exceptionTracker) {
    auto& cache = JavaEnv::getCache();
    return JavaValue::makeObject(cache.getHashMapClass().newObject(cache.getHashMapConstructorMethod()));
}

void JavaValueDelegate::setMapEntry(const JavaValue& map,
                                    const JavaValue& key,
                                    const JavaValue& value,
                                    Valdi::ExceptionTracker& exceptionTracker) {
    JavaEnv::getCache().getHashMapPutMethod().call(
        javaValueToObject(map), javaValueToObject(key), javaValueToObject(value));
}

size_t JavaValueDelegate::getMapEstimatedLength(const JavaValue& map, Valdi::ExceptionTracker& exceptionTracker) {
    auto size = JavaEnv::getCache().getMapSizeMethod().call(javaValueToObject(map));
    return static_cast<size_t>(size);
}

void JavaValueDelegate::visitMapKeyValues(const JavaValue& map,
                                          Valdi::PlatformMapValueVisitor<JavaValue>& visitor,
                                          Valdi::ExceptionTracker& exceptionTracker) {
    auto entrySet = JavaCache::get().getMapEntrySetMethod().call(map.getObject());
    auto iterator = JavaIterator::fromIterable(entrySet.getUnsafeObject());

    for (;;) {
        auto next = iterator.next();
        if (!next) {
            break;
        }

        auto key = JavaCache::get().getMapEntryGetKeyMethod().call(next.value());
        auto value = JavaCache::get().getMapEntryGetValueMethod().call(next.value());

        if (!visitor.visit(JavaValue::unsafeMakeObject(key.getUnsafeObject()),
                           JavaValue::unsafeMakeObject(value.getUnsafeObject()),
                           exceptionTracker)) {
            break;
        }
    }
}

Valdi::PlatformArrayBuilder<JavaValue> JavaValueDelegate::newArrayBuilder(size_t capacity,
                                                                          Valdi::ExceptionTracker& exceptionTracker) {
    return Valdi::PlatformArrayBuilder<JavaValue>(JavaValue::makeObject(newJavaObjectArray(capacity)));
}

void JavaValueDelegate::setArrayEntry(const Valdi::PlatformArrayBuilder<JavaValue>& arrayBuilder,
                                      size_t index,
                                      const JavaValue& value,
                                      Valdi::ExceptionTracker& exceptionTracker) {
    JavaEnv::accessEnv([&](JNIEnv& jniEnv) {
        jniEnv.SetObjectArrayElement(
            arrayBuilder.getBuilder().getObjectArray(), static_cast<jsize>(index), value.getObject());
    });
}

JavaValue JavaValueDelegate::finalizeArray(Valdi::PlatformArrayBuilder<JavaValue>& builder,
                                           Valdi::ExceptionTracker& exceptionTracker) {
    auto value = JavaCache::get().getValdiMarshallerCPPArrayToListMethod().call(
        JavaCache::get().getValdiMarshallerCPPClass().getClass(),
        ObjectArray(JavaEnv(), builder.getBuilder().getObject()));
    return std::move(value.getValue());
}

Valdi::PlatformArrayIterator<JavaValue> JavaValueDelegate::newArrayIterator(const JavaValue& array,
                                                                            Valdi::ExceptionTracker& exceptionTracker) {
    auto value = JavaCache::get().getValdiMarshallerCPPListToArrayMethod().call(
        JavaCache::get().getValdiMarshallerCPPClass().getClass(), JavaObject(JavaEnv(), array.getObject()));
    auto arraySize = JavaObjectArray(value.getValue().getObjectArray()).size();
    return Valdi::PlatformArrayIterator<JavaValue>(std::move(value.getValue()), arraySize);
}

JavaValue JavaValueDelegate::getArrayItem(const Valdi::PlatformArrayIterator<JavaValue>& arrayIterator,
                                          size_t index,
                                          Valdi::ExceptionTracker& exceptionTracker) {
    auto object = JavaObjectArray(arrayIterator.getIterator().getObjectArray()).getObject(index);

    return JavaValue::makeObject(std::move(object));
}

JavaValue JavaValueDelegate::newBridgedPromise(const Valdi::Ref<Valdi::Promise>& promise,
                                               const Valdi::Ref<Valdi::ValueMarshaller<JavaValue>>& valueMarshaller,
                                               Valdi::ExceptionTracker& exceptionTracker) {
    return JavaValue::makeObject(
        JavaEnv::getCache().getCppPromiseClass().newObject(JavaEnv::getCache().getCppPromiseConstructorMethod(),
                                                           bridgeRetain(promise.get()),
                                                           bridgeRetain(valueMarshaller.get())));
}

Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> JavaValueDelegate::newObjectClass(
    const Valdi::Ref<Valdi::ClassSchema>& schema, Valdi::ExceptionTracker& exceptionTracker) {
    if (schema->isInterface()) {
        return _typesResolver.getInterfaceClassDelegateForName(schema->getClassName());
    } else {
        return _typesResolver.getObjectClassDelegateForName(schema->getClassName());
    }
}

Valdi::Ref<Valdi::PlatformEnumClassDelegate<JavaValue>> JavaValueDelegate::newEnumClass(
    const Valdi::Ref<Valdi::EnumSchema>& schema, Valdi::ExceptionTracker& exceptionTracker) {
    return _typesResolver.getEnumClassDelegateForName(schema->getName());
}

Valdi::Ref<Valdi::PlatformFunctionClassDelegate<JavaValue>> JavaValueDelegate::newFunctionClass(
    const Valdi::Ref<Valdi::PlatformFunctionTrampoline<JavaValue>>& trampoline,
    const Valdi::Ref<Valdi::ValueFunctionSchema>& schema,
    Valdi::ExceptionTracker& exceptionTracker) {
    if (schema->getAttributes().isMethod()) {
        return Valdi::makeShared<JavaMethodClassDelegate>(trampoline);
    } else {
        return Valdi::makeShared<JavaFunctionClassDelegate>(trampoline);
    }
}

bool JavaValueDelegate::valueIsNull(const JavaValue& value) const {
    return value.isNull();
}

int32_t JavaValueDelegate::valueToInt(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return value.getInt();
}

int64_t JavaValueDelegate::valueToLong(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return value.getLong();
}

double JavaValueDelegate::valueToDouble(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return value.getDouble();
}

bool JavaValueDelegate::valueToBool(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return value.getBoolean() != 0u;
}

int32_t JavaValueDelegate::valueObjectToInt(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return JavaEnv::getCache().getNumberIntValueMethod().call(javaValueToObject(value));
}

int64_t JavaValueDelegate::valueObjectToLong(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return JavaEnv::getCache().getNumberLongValueMethod().call(javaValueToObject(value));
}

double JavaValueDelegate::valueObjectToDouble(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return JavaEnv::getCache().getNumberDoubleValueMethod().call(javaValueToObject(value));
}

bool JavaValueDelegate::valueObjectToBool(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return JavaEnv::getCache().getBooleanBooleanValueMethod().call(javaValueToObject(value));
}

Valdi::Ref<Valdi::StaticString> JavaValueDelegate::valueToString(const JavaValue& value,
                                                                 Valdi::ExceptionTracker& exceptionTracker) const {
    return ValdiAndroid::toStaticString(JavaEnv(), value.getString());
}

Valdi::BytesView JavaValueDelegate::valueToByteArray(const JavaValue& value,
                                                     const Valdi::ReferenceInfoBuilder& /*referenceInfoBuilder*/,
                                                     Valdi::ExceptionTracker& exceptionTracker) const {
    // TODO (4554): Clean up null check block
    if (value.isNull() || value.getByteArray() == nullptr) {
        return Valdi::BytesView();
    }
    return ValdiAndroid::toByteArray(JavaEnv(), value.getByteArray());
}

Valdi::Ref<Valdi::Promise> JavaValueDelegate::valueToPromise(
    const JavaValue& value,
    const Valdi::Ref<Valdi::ValueMarshaller<JavaValue>>& valueMarshaller,
    const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
    Valdi::ExceptionTracker& exceptionTracker) const {
    return Valdi::makeShared<JavaPromise>(value.getObject(), valueMarshaller);
}

Valdi::Value JavaValueDelegate::valueToUntyped(const JavaValue& value,
                                               const Valdi::ReferenceInfoBuilder& /*referenceInfoBuilder*/,
                                               Valdi::ExceptionTracker& exceptionTracker) const {
    return ValdiAndroid::toValue(JavaEnv(), JavaEnv::newLocalRef(value.getObject()));
}

Valdi::PlatformObjectStore<JavaValue>& JavaValueDelegate::getObjectStore() {
    return _objectStore;
}

JavaValue JavaValueDelegate::newTypedArray(Valdi::TypedArrayType arrayType,
                                           const Valdi::BytesView& bytes,
                                           Valdi::ExceptionTracker& exceptionTracker) {
    // TODO: preserve type
    return newByteArray(bytes, exceptionTracker);
}

JavaValue JavaValueDelegate::newES6Collection(Valdi::CollectionType type, Valdi::ExceptionTracker& exceptionTracker) {
    return {};
}

void JavaValueDelegate::setES6CollectionEntry(const JavaValue& collection,
                                              Valdi::CollectionType type,
                                              std::initializer_list<JavaValue> items,
                                              Valdi::ExceptionTracker& exceptionTracker) {}

void JavaValueDelegate::visitES6Collection(const JavaValue& collection,
                                           Valdi::PlatformMapValueVisitor<JavaValue>& visitor,
                                           Valdi::ExceptionTracker& exceptionTracker) {}

JavaValue JavaValueDelegate::newDate(double millisecondsSinceEpoch, Valdi::ExceptionTracker& exceptionTracker) {
    return {};
}

Valdi::Ref<Valdi::ValueTypedArray> JavaValueDelegate::valueToTypedArray(
    const JavaValue& value,
    const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
    Valdi::ExceptionTracker& exceptionTracker) const {
    // TODO preserve type
    return Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType,
                                                     valueToByteArray(value, referenceInfoBuilder, exceptionTracker));
}

double JavaValueDelegate::dateToDouble(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const {
    return 0.0;
}

Valdi::BytesView JavaValueDelegate::encodeProto(const JavaValue& proto,
                                                const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                                Valdi::ExceptionTracker& exceptionTracker) const {
    return {};
}

JavaValue JavaValueDelegate::decodeProto(const Valdi::BytesView& bytes,
                                         const std::vector<std::string_view>& classNameParts,
                                         const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                         Valdi::ExceptionTracker& exceptionTracker) const {
    return {};
}

} // namespace ValdiAndroid
