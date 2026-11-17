//
//  SCValdiObjCValueDelegate.h
//  valdi_core-ios
//
//  Created by Simon Corsin on 1/31/23.
//

#import "valdi_core/SCValdiBlockHelper.h"
#import "valdi_core/SCValdiObjCValue.h"
#import "valdi_core/cpp/Utils/PlatformValueDelegate.hpp"
#import <Foundation/Foundation.h>

namespace ValdiIOS {

constexpr bool kStrictNullCheckingEnabled = false;
// TODO(simon): Enable after the release
// constexpr bool kStrictNullCheckingEnabled = snap::isGoldOrDebugBuild();

class ObjCTypesResolver {
public:
    virtual ~ObjCTypesResolver() {}

    virtual Valdi::Ref<Valdi::PlatformObjectClassDelegate<ObjCValue>> getObjectClassDelegateForName(
        const Valdi::StringBox& className) const = 0;

    virtual Valdi::Ref<Valdi::PlatformEnumClassDelegate<ObjCValue>> getEnumClassDelegateForName(
        const Valdi::StringBox& enumName) const = 0;

    virtual Valdi::Ref<ObjCBlockHelper> getBlockHelperForSchema(
        const Valdi::ValueFunctionSchema& functionSchema) const = 0;
};

class ObjCObjectIndirectRef;

class ObjCObjectStore : public Valdi::PlatformObjectStore<ObjCValue> {
public:
    ObjCObjectStore();
    ~ObjCObjectStore() override;

    Valdi::Ref<Valdi::RefCountable> getValueForObjectKey(const ObjCValue& objectKey,
                                                         Valdi::ExceptionTracker& exceptionTracker) final;

    void setValueForObjectKey(const ObjCValue& objectKey,
                              const Valdi::Ref<Valdi::RefCountable>& value,
                              Valdi::ExceptionTracker& exceptionTracker) final;

    std::optional<ObjCValue> getObjectForId(uint32_t objectId, Valdi::ExceptionTracker& exceptionTracker) final;

    void setObjectForId(uint32_t objectId, const ObjCValue& object, Valdi::ExceptionTracker& exceptionTracker) final;

private:
    Valdi::FlatMap<uint32_t, ObjCObjectIndirectRef> _objectById;

    const void* getObjCKey() const;
};

class ObjCValueDelegate : public Valdi::PlatformValueDelegate<ObjCValue> {
public:
    ObjCValueDelegate(const ObjCTypesResolver& typeResolver);

    ~ObjCValueDelegate() override;

    ObjCValue newVoid() final;

    ObjCValue newNull() final;

    ObjCValue newInt(int32_t value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newIntObject(int32_t value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newBigInt(const Valdi::Ref<Valdi::BigInt>& value, Valdi::ExceptionTracker& exceptionTracker) final;
    Valdi::Ref<Valdi::BigInt> valueToBigInt(const ObjCValue& value,
                                            Valdi::ExceptionTracker& exceptionTracker) const final;

    ObjCValue newLong(int64_t value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newLongObject(int64_t value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newDouble(double value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newDoubleObject(double value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newBool(bool value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newBoolObject(bool value, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newStringUTF8(std::string_view str, Valdi::ExceptionTracker& exceptionTracker) final;
    ObjCValue newStringUTF16(std::u16string_view str, Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newByteArray(const Valdi::BytesView& bytes, Valdi::ExceptionTracker& exceptionTracker) final;
    ObjCValue newTypedArray(Valdi::TypedArrayType arrayType,
                            const Valdi::BytesView& bytes,
                            Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newUntyped(const Valdi::Value& value,
                         const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                         Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newMap(size_t capacity, Valdi::ExceptionTracker& exceptionTracker) final;

    size_t getMapEstimatedLength(const ObjCValue& map, Valdi::ExceptionTracker& exceptionTracker) final;

    void setMapEntry(const ObjCValue& map,
                     const ObjCValue& key,
                     const ObjCValue& value,
                     Valdi::ExceptionTracker& exceptionTracker) final;

    void visitMapKeyValues(const ObjCValue& map,
                           Valdi::PlatformMapValueVisitor<ObjCValue>& visitor,
                           Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newES6Collection(Valdi::CollectionType type, Valdi::ExceptionTracker& exceptionTracker) final;
    void setES6CollectionEntry(const ObjCValue& collection,
                               Valdi::CollectionType type,
                               std::initializer_list<ObjCValue> items,
                               Valdi::ExceptionTracker& exceptionTracker) final;
    void visitES6Collection(const ObjCValue& collection,
                            Valdi::PlatformMapValueVisitor<ObjCValue>& visitor,
                            Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newDate(double millisecondsSinceEpoch, Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::PlatformArrayBuilder<ObjCValue> newArrayBuilder(size_t capacity,
                                                           Valdi::ExceptionTracker& exceptionTracker) final;

    void setArrayEntry(const Valdi::PlatformArrayBuilder<ObjCValue>& arrayBuilder,
                       size_t index,
                       const ObjCValue& value,
                       Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue finalizeArray(Valdi::PlatformArrayBuilder<ObjCValue>& value,
                            Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::PlatformArrayIterator<ObjCValue> newArrayIterator(const ObjCValue& array,
                                                             Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue getArrayItem(const Valdi::PlatformArrayIterator<ObjCValue>& arrayIterator,
                           size_t index,
                           Valdi::ExceptionTracker& exceptionTracker) final;

    ObjCValue newBridgedPromise(const Valdi::Ref<Valdi::Promise>& promise,
                                const Valdi::Ref<Valdi::ValueMarshaller<ObjCValue>>& valueMarshaller,
                                Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::Ref<Valdi::PlatformObjectClassDelegate<ObjCValue>> newObjectClass(
        const Valdi::Ref<Valdi::ClassSchema>& schema, Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::Ref<Valdi::PlatformEnumClassDelegate<ObjCValue>> newEnumClass(
        const Valdi::Ref<Valdi::EnumSchema>& schema, Valdi::ExceptionTracker& exceptionTracker) final;

    Valdi::Ref<Valdi::PlatformFunctionClassDelegate<ObjCValue>> newFunctionClass(
        const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>>& trampoline,
        const Valdi::Ref<Valdi::ValueFunctionSchema>& schema,
        Valdi::ExceptionTracker& exceptionTracker) final;

    bool valueIsNull(const ObjCValue& value) const final;

    int32_t valueToInt(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    int64_t valueToLong(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    double valueToDouble(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    bool valueToBool(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    int32_t valueObjectToInt(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    int64_t valueObjectToLong(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    double valueObjectToDouble(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    bool valueObjectToBool(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::Ref<Valdi::StaticString> valueToString(const ObjCValue& value,
                                                  Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::BytesView valueToByteArray(const ObjCValue& value,
                                      const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                      Valdi::ExceptionTracker& exceptionTracker) const final;
    Valdi::Ref<Valdi::ValueTypedArray> valueToTypedArray(const ObjCValue& value,
                                                         const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                                         Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::Ref<Valdi::Promise> valueToPromise(const ObjCValue& value,
                                              const Valdi::Ref<Valdi::ValueMarshaller<ObjCValue>>& valueMarshaller,
                                              const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                              Valdi::ExceptionTracker& exceptionTracker) const final;

    double dateToDouble(const ObjCValue& value, Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::BytesView encodeProto(const ObjCValue& proto,
                                 const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                 Valdi::ExceptionTracker& exceptionTracker) const final;
    ObjCValue decodeProto(const Valdi::BytesView& bytes,
                          const std::vector<std::string_view>& classNameParts,
                          const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                          Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::Value valueToUntyped(const ObjCValue& value,
                                const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                Valdi::ExceptionTracker& exceptionTracker) const final;

    Valdi::PlatformObjectStore<ObjCValue>& getObjectStore() final;

private:
    const ObjCTypesResolver& _typeResolver;
    ObjCObjectStore _objectStore;
};

} // namespace ValdiIOS
