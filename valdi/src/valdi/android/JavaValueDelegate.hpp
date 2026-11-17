//
//  JavaValueDelegate.hpp
//  valdi-android
//
//  Created by Simon Corsin on 2/14/23.
//

#pragma once

#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/PlatformValueDelegate.hpp"
#include "valdi_core/jni/IndirectJavaGlobalRef.hpp"
#include "valdi_core/jni/JavaValue.hpp"

namespace ValdiAndroid {

class JavaTypesResolver {
public:
    virtual ~JavaTypesResolver() = default;

    virtual Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> getObjectClassDelegateForName(
        const Valdi::StringBox& className) = 0;

    virtual Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> getInterfaceClassDelegateForName(
        const Valdi::StringBox& className) = 0;

    virtual Valdi::Ref<Valdi::PlatformEnumClassDelegate<JavaValue>> getEnumClassDelegateForName(
        const Valdi::StringBox& className) = 0;
};

class JavaObjectStore : public Valdi::PlatformObjectStore<JavaValue> {
public:
    JavaObjectStore();
    ~JavaObjectStore() override;

    Valdi::Ref<Valdi::RefCountable> getValueForObjectKey(const JavaValue& objectKey,
                                                         Valdi::ExceptionTracker& exceptionTracker) final;

    void setValueForObjectKey(const JavaValue& objectKey,
                              const Valdi::Ref<Valdi::RefCountable>& value,
                              Valdi::ExceptionTracker& exceptionTracker) final;

    std::optional<JavaValue> getObjectForId(uint32_t objectId, Valdi::ExceptionTracker& exceptionTracker) final;

    void setObjectForId(uint32_t objectId, const JavaValue& object, Valdi::ExceptionTracker& exceptionTracker) final;

private:
    IndirectJavaGlobalRef _weakMap;
    Valdi::FlatMap<uint32_t, IndirectJavaGlobalRef> _objectById;
};

class JavaValueDelegate : public Valdi::PlatformValueDelegate<JavaValue> {
public:
    explicit JavaValueDelegate(JavaTypesResolver& typesResolver);
    ~JavaValueDelegate() override;

    JavaValue newVoid() final;
    JavaValue newNull() final;

    JavaValue newInt(int32_t value, Valdi::ExceptionTracker& exceptionTracker) final;
    JavaValue newIntObject(int32_t value, Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newBigInt(const Valdi::Ref<Valdi::BigInt>& value, Valdi::ExceptionTracker& exceptionTracker) final;
    Valdi::Ref<Valdi::BigInt> valueToBigInt(const JavaValue& value,
                                            Valdi::ExceptionTracker& exceptionTracker) const final;

    JavaValue newLong(int64_t value, Valdi::ExceptionTracker& exceptionTracker) final;
    JavaValue newLongObject(int64_t value, Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newDouble(double value, Valdi::ExceptionTracker& exceptionTracker) final;
    JavaValue newDoubleObject(double value, Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newBool(bool value, Valdi::ExceptionTracker& exceptionTracker) final;
    JavaValue newBoolObject(bool value, Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newStringUTF8(std::string_view str, Valdi::ExceptionTracker& exceptionTracker) final;
    JavaValue newStringUTF16(std::u16string_view str, Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newByteArray(const Valdi::BytesView& bytes, Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newTypedArray(Valdi::TypedArrayType arrayType,
                            const Valdi::BytesView& bytes,
                            Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newUntyped(const Valdi::Value& value,
                         const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                         Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newMap(size_t capacity, Valdi::ExceptionTracker& exceptionTracker) final;
    void setMapEntry(const JavaValue& map,
                     const JavaValue& key,
                     const JavaValue& value,
                     Valdi::ExceptionTracker& exceptionTracker) final;
    size_t getMapEstimatedLength(const JavaValue& map, Valdi::ExceptionTracker& exceptionTracker) final;
    void visitMapKeyValues(const JavaValue& map,
                           Valdi::PlatformMapValueVisitor<JavaValue>& visitor,
                           Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newES6Collection(Valdi::CollectionType type, Valdi::ExceptionTracker& exceptionTracker) final;
    void setES6CollectionEntry(const JavaValue& collection,
                               Valdi::CollectionType type,
                               std::initializer_list<JavaValue> items,
                               Valdi::ExceptionTracker& exceptionTracker) final;
    void visitES6Collection(const JavaValue& collection,
                            Valdi::PlatformMapValueVisitor<JavaValue>& visitor,
                            Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newDate(double millisecondsSinceEpoch, Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::PlatformArrayBuilder<JavaValue> newArrayBuilder(size_t capacity,
                                                           Valdi::ExceptionTracker& exceptionTracker) final;
    void setArrayEntry(const Valdi::PlatformArrayBuilder<JavaValue>& arrayBuilder,
                       size_t index,
                       const JavaValue& value,
                       Valdi::ExceptionTracker& exceptionTracker) final;
    JavaValue finalizeArray(Valdi::PlatformArrayBuilder<JavaValue>& builder,
                            Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::PlatformArrayIterator<JavaValue> newArrayIterator(const JavaValue& array,
                                                             Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue getArrayItem(const Valdi::PlatformArrayIterator<JavaValue>& arrayIterator,
                           size_t index,
                           Valdi::ExceptionTracker& exceptionTracker) final;

    JavaValue newBridgedPromise(const Valdi::Ref<Valdi::Promise>& promise,
                                const Valdi::Ref<Valdi::ValueMarshaller<JavaValue>>& valueMarshaller,
                                Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> newObjectClass(
        const Valdi::Ref<Valdi::ClassSchema>& schema, Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::Ref<Valdi::PlatformEnumClassDelegate<JavaValue>> newEnumClass(
        const Valdi::Ref<Valdi::EnumSchema>& schema, Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::Ref<Valdi::PlatformFunctionClassDelegate<JavaValue>> newFunctionClass(
        const Valdi::Ref<Valdi::PlatformFunctionTrampoline<JavaValue>>& trampoline,
        const Valdi::Ref<Valdi::ValueFunctionSchema>& schema,
        Valdi::ExceptionTracker& exceptionTracker) final;

    bool valueIsNull(const JavaValue& value) const final;
    int32_t valueToInt(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;
    int64_t valueToLong(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;
    double valueToDouble(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;
    bool valueToBool(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    int32_t valueObjectToInt(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;
    int64_t valueObjectToLong(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;
    double valueObjectToDouble(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;
    bool valueObjectToBool(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::Ref<Valdi::StaticString> valueToString(const JavaValue& value,
                                                  Valdi::ExceptionTracker& exceptionTracker) const final;
    Valdi::BytesView valueToByteArray(const JavaValue& value,
                                      const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                      Valdi::ExceptionTracker& exceptionTracker) const final;
    Valdi::Ref<Valdi::ValueTypedArray> valueToTypedArray(const JavaValue& value,
                                                         const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                                         Valdi::ExceptionTracker& exceptionTracker) const final;
    Valdi::Ref<Valdi::Promise> valueToPromise(const JavaValue& value,
                                              const Valdi::Ref<Valdi::ValueMarshaller<JavaValue>>& valueMarshaller,
                                              const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                              Valdi::ExceptionTracker& exceptionTracker) const final;

    double dateToDouble(const JavaValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::BytesView encodeProto(const JavaValue& proto,
                                 const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                 Valdi::ExceptionTracker& exceptionTracker) const final;
    JavaValue decodeProto(const Valdi::BytesView& bytes,
                          const std::vector<std::string_view>& classNameParts,
                          const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                          Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::Value valueToUntyped(const JavaValue& value,
                                const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::PlatformObjectStore<JavaValue>& getObjectStore() final;

private:
    JavaTypesResolver& _typesResolver;
    JavaObjectStore _objectStore;
};

} // namespace ValdiAndroid
