//
//  JavaScriptValueDelegate.hpp
//  valdi
//
//  Created by Simon Corsin on 2/3/23.
//

#pragma once

#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi_core/cpp/Context/ContextId.hpp"
#include "valdi_core/cpp/Utils/PlatformValueDelegate.hpp"

namespace Valdi {

class IJavaScriptContext;
class PromiseExecutor;

class JavaScriptObjectStore : public PlatformObjectStore<JSValueRef> {
public:
    JavaScriptObjectStore(IJavaScriptContext& jsContext, bool disabled, JSExceptionTracker& exceptionTracker);
    ~JavaScriptObjectStore() override;

    void teardown();

    Ref<RefCountable> getValueForObjectKey(const JSValueRef& objectKey, ExceptionTracker& exceptionTracker) final;
    Ref<RefCountable> getValueForObjectKey(const JSValue& objectKey, JSExceptionTracker& exceptionTracker);
    bool hasValueForObjectKey(const JSValue& objectKey);

    void setValueForObjectKey(const JSValueRef& objectKey,
                              const Ref<RefCountable>& value,
                              ExceptionTracker& exceptionTracker) final;

    std::optional<JSValueRef> getObjectForId(uint32_t objectId, ExceptionTracker& exceptionTracker) final;

    void setObjectForId(uint32_t objectId, const JSValueRef& object, ExceptionTracker& exceptionTracker) final;

    struct ObjectStoreId {
        uint32_t objectId;
        ContextId contextId;

        bool operator==(const ObjectStoreId& other) const;
        bool operator!=(const ObjectStoreId& other) const;
    };

private:
    IJavaScriptContext* _jsContext;
    JSPropertyNameRef _attachedProxyName;
    FlatMap<ObjectStoreId, JSValueRef> _objectById;
    bool _disabled;

    static ObjectStoreId makeObjectStoreId(uint32_t objectId);
};

/**
 Implementation of PlatformValueDelegate on JSValueRef that allows the ValueMarshallerRegistry
 to generically create and manipulate JS values.
 */
class JavaScriptValueDelegate : public PlatformValueDelegate<JSValueRef> {
public:
    JavaScriptValueDelegate(IJavaScriptContext& jsContext,
                            bool disableProxyObjectStore,
                            JSExceptionTracker& exceptionTracker);
    ~JavaScriptValueDelegate() override;

    void teardown();

    JSValueRef newVoid() final;
    JSValueRef newNull() final;

    JSValueRef newInt(int32_t value, ExceptionTracker& exceptionTracker) final;
    JSValueRef newIntObject(int32_t value, ExceptionTracker& exceptionTracker) final;

    JSValueRef newLong(int64_t value, ExceptionTracker& exceptionTracker) final;
    JSValueRef newLongObject(int64_t value, ExceptionTracker& exceptionTracker) final;

    JSValueRef newDouble(double value, ExceptionTracker& exceptionTracker) final;
    JSValueRef newDoubleObject(double value, ExceptionTracker& exceptionTracker) final;

    JSValueRef newBool(bool value, ExceptionTracker& exceptionTracker) final;
    JSValueRef newBoolObject(bool value, ExceptionTracker& exceptionTracker) final;

    JSValueRef newStringUTF8(std::string_view str, ExceptionTracker& exceptionTracker) final;
    JSValueRef newStringUTF16(std::u16string_view str, ExceptionTracker& exceptionTracker) final;
    JSValueRef newString(const StaticString& str, ExceptionTracker& exceptionTracker) final;

    JSValueRef newByteArray(const BytesView& bytes, ExceptionTracker& exceptionTracker) final;
    JSValueRef newTypedArray(TypedArrayType arrayType,
                             const BytesView& bytes,
                             ExceptionTracker& exceptionTracker) final;

    JSValueRef newUntyped(const Value& value,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) final;

    JSValueRef newMap(size_t capacity, ExceptionTracker& exceptionTracker) final;
    void setMapEntry(const JSValueRef& map,
                     const JSValueRef& key,
                     const JSValueRef& value,
                     ExceptionTracker& exceptionTracker) final;
    size_t getMapEstimatedLength(const JSValueRef& map, ExceptionTracker& exceptionTracker) final;
    void visitMapKeyValues(const JSValueRef& map,
                           PlatformMapValueVisitor<JSValueRef>& visitor,
                           ExceptionTracker& exceptionTracker) final;

    JSValueRef newES6Collection(CollectionType type, ExceptionTracker& exceptionTracker) final;
    void setES6CollectionEntry(const JSValueRef& collection,
                               CollectionType type,
                               std::initializer_list<JSValueRef> items,
                               ExceptionTracker& exceptionTracker) final;
    void visitES6Collection(const JSValueRef& collection,
                            PlatformMapValueVisitor<JSValueRef>& visitor,
                            ExceptionTracker& exceptionTracker) final;

    JSValueRef newDate(double millisecondsSinceEpoch, ExceptionTracker& exceptionTracker) final;

    PlatformArrayBuilder<JSValueRef> newArrayBuilder(size_t capacity, ExceptionTracker& exceptionTracker) final;
    void setArrayEntry(const PlatformArrayBuilder<JSValueRef>& arrayBuilder,
                       size_t index,
                       const JSValueRef& value,
                       ExceptionTracker& exceptionTracker) final;
    JSValueRef finalizeArray(PlatformArrayBuilder<JSValueRef>& arrayBuilder, ExceptionTracker& exceptionTracker) final;

    PlatformArrayIterator<JSValueRef> newArrayIterator(const JSValueRef& array,
                                                       ExceptionTracker& exceptionTracker) final;
    JSValueRef getArrayItem(const PlatformArrayIterator<JSValueRef>& arrayIterator,
                            size_t index,
                            ExceptionTracker& exceptionTracker) final;

    JSValueRef newBridgedPromise(const Ref<Promise>& promise,
                                 const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                 ExceptionTracker& exceptionTracker) final;

    Ref<PlatformObjectClassDelegate<JSValueRef>> newObjectClass(const Ref<ClassSchema>& schema,
                                                                ExceptionTracker& exceptionTracker) final;

    Ref<PlatformEnumClassDelegate<JSValueRef>> newEnumClass(const Ref<EnumSchema>& schema,
                                                            ExceptionTracker& exceptionTracker) final;

    Ref<PlatformFunctionClassDelegate<JSValueRef>> newFunctionClass(
        const Ref<PlatformFunctionTrampoline<JSValueRef>>& trampoline,
        const Ref<ValueFunctionSchema>& schema,
        ExceptionTracker& exceptionTracker) final;

    bool valueIsNull(const JSValueRef& value) const final;
    int32_t valueToInt(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    int64_t valueToLong(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    double valueToDouble(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    bool valueToBool(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;

    int32_t valueObjectToInt(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    int64_t valueObjectToLong(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    double valueObjectToDouble(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    bool valueObjectToBool(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;

    Ref<StaticString> valueToString(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;
    BytesView valueToByteArray(const JSValueRef& value,
                               const ReferenceInfoBuilder& referenceInfoBuilder,
                               ExceptionTracker& exceptionTracker) const final;
    Ref<ValueTypedArray> valueToTypedArray(const JSValueRef& value,
                                           const ReferenceInfoBuilder& referenceInfoBuilder,
                                           ExceptionTracker& exceptionTracker) const final;

    Ref<Promise> valueToPromise(const JSValueRef& value,
                                const Ref<ValueMarshaller<JSValueRef>>& valueMarshaller,
                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                ExceptionTracker& exceptionTracker) const final;

    double dateToDouble(const JSValueRef& value, ExceptionTracker& exceptionTracker) const final;

    BytesView encodeProto(const JSValueRef& proto,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) const final;
    JSValueRef decodeProto(const BytesView& bytes,
                           const std::vector<std::string_view>& classNameParts,
                           const ReferenceInfoBuilder& referenceInfoBuilder,
                           ExceptionTracker& exceptionTracker) const final;

    Value valueToUntyped(const JSValueRef& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) const final;

    PlatformObjectStore<JSValueRef>& getObjectStore() final;
    JavaScriptObjectStore& getJsObjectStore();

private:
    IJavaScriptContext* _jsContext;
    JavaScriptObjectStore _objectStore;
    Ref<PromiseExecutor> _promiseExecutor;
    JSValueRef _promiseExecutorJS;
};

} // namespace Valdi

namespace std {

template<>
struct hash<Valdi::JavaScriptObjectStore::ObjectStoreId> {
    std::size_t operator()(const Valdi::JavaScriptObjectStore::ObjectStoreId& k) const noexcept;
};

} // namespace std
