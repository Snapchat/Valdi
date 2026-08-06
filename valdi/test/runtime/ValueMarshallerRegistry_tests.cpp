//
//  ValueMarshallerRegistry_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 24/01/23
//

#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/PlatformFunctionTrampolineUtils.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueConvertible.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueMarshallerRegistry.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

static Value flattenValue(const Value& value) {
    auto json = valueToJson(value);
    auto result = jsonToValue(json->data(), json->size());
    result.ensureSuccess();
    return result.moveValue();
}

class TestObject : public ValdiObject, public ValueConvertible {
public:
    TestObject(const StringBox& className, const Value* propertyValues, size_t propertiesSize) : _className(className) {
        _properties = ValueArray::make(propertyValues, propertyValues + propertiesSize);
    }

    TestObject(const StringBox& className, std::initializer_list<Value> propertyValues)
        : TestObject(className, propertyValues.begin(), propertyValues.size()) {}

    ~TestObject() override = default;

    size_t getPropertiesSize() const {
        return _properties->size();
    }

    const StringBox& getClassName() const final {
        return _className;
    }

    const Value& getProperty(size_t index) const {
        return (*_properties)[index];
    }

    void setProperty(size_t index, const Value& value) {
        (*_properties)[index] = value;
    }

    const Ref<TestObject>& getPrototype() const {
        return _prototype;
    }

    void setPrototype(const Ref<TestObject>& prototype) {
        _prototype = prototype;
    }

    const Ref<RefCountable>& getAttachedObject() const {
        return _attachedObject;
    }

    void setAttachedObject(const Ref<RefCountable>& attachedObject) {
        _attachedObject = attachedObject;
    }

    Value toValue() final {
        return Value().setMapValue("className", Value(_className)).setMapValue("properties", Value(_properties));
    }

private:
    StringBox _className;
    Ref<ValueArray> _properties;
    Ref<TestObject> _prototype;
    Ref<RefCountable> _attachedObject;
};

enum class NumberType {
    Boolean,
    Integer,
    LongInteger,
    Double,
};

class TestNumberObject : public ValdiObject, public ValueConvertible {
public:
    TestNumberObject(const NumberType& numberType, const Value& value) : _numberType(numberType), _value(value) {}

    ~TestNumberObject() override = default;

    const StringBox& getClassName() const final {
        switch (_numberType) {
            case NumberType::Boolean: {
                static auto kClassName = STRING_LITERAL("Boolean");
                return kClassName;
            }
            case NumberType::Integer: {
                static auto kClassName = STRING_LITERAL("Integer");
                return kClassName;
            }
            case NumberType::LongInteger: {
                static auto kClassName = STRING_LITERAL("Long");
                return kClassName;
            }
            case NumberType::Double: {
                static auto kClassName = STRING_LITERAL("Double");
                return kClassName;
            }
        }
    }

    const Value& getValue() const {
        return _value;
    }

    Value toValue() final {
        return Value().setMapValue("className", Value(getClassName())).setMapValue("value", _value);
    }

    static Value unwrap(const Value& value, NumberType expectedNumberType) {
        auto number = value.getTypedRef<TestNumberObject>();
        SC_ASSERT(number != nullptr);
        SC_ASSERT(number->_numberType == expectedNumberType);

        return number->getValue();
    }

private:
    NumberType _numberType;
    Value _value;
};

class TestFunction : public ValdiObject {
public:
    TestFunction() {}
    ~TestFunction() override = default;

    const StringBox& getClassName() const final {
        static auto kClassName = STRING_LITERAL("TestFunction");
        return kClassName;
    }

    virtual Result<Value> call(const Value* parameters, size_t parametersSize) = 0;
};

class TestFunctionWithValueFunction : public TestFunction {
public:
    TestFunctionWithValueFunction(const Ref<PlatformFunctionTrampoline<Value>>& trampoline,
                                  const ReferenceInfo& referenceInfo,
                                  const Ref<ValueFunction>& valueFunction)
        : _trampoline(trampoline), _referenceInfo(referenceInfo), _valueFunction(valueFunction) {}
    ~TestFunctionWithValueFunction() override = default;

    Result<Value> call(const Value* parameters, size_t parametersSize) final {
        SimpleExceptionTracker exceptionTracker;
        auto value = _trampoline->forwardCall(_valueFunction.get(),
                                              parameters,
                                              parametersSize,
                                              ReferenceInfoBuilder(_referenceInfo),
                                              nullptr,
                                              exceptionTracker);
        if (value) {
            return value.value();
        }
        return exceptionTracker.extractError();
    }

private:
    Ref<PlatformFunctionTrampoline<Value>> _trampoline;
    ReferenceInfo _referenceInfo;
    Ref<ValueFunction> _valueFunction;
};

using TestFunctionCallable = Function<Result<Value>(const Value*, size_t)>;

class TestFunctionWithCallable : public TestFunction {
public:
    TestFunctionWithCallable(TestFunctionCallable callable) : _callable(std::move(callable)) {}
    ~TestFunctionWithCallable() override = default;

    Result<Value> call(const Value* parameters, size_t parametersSize) final {
        return _callable(parameters, parametersSize);
    }

private:
    TestFunctionCallable _callable;
};

class ValueFunctionWithTestFunction : public ValueFunction {
public:
    ValueFunctionWithTestFunction(const Ref<PlatformFunctionTrampoline<Value>>& trampoline,
                                  const Ref<TestObject>& receiver,
                                  const Ref<TestFunction>& testFunction,
                                  const ReferenceInfo& referenceInfo,
                                  size_t arity)
        : _trampoline(trampoline),
          _receiver(receiver),
          _testFunction(testFunction),
          _referenceInfo(referenceInfo),
          _arity(arity),
          _hasReceiver(receiver != nullptr) {}

    ~ValueFunctionWithTestFunction() override = default;

    Value operator()(const ValueFunctionCallContext& callContext) noexcept final {
        return handleBridgeCall(_trampoline->getCallQueue(),
                                _trampoline->isPromiseReturnType(),
                                this,
                                callContext,
                                [](auto* self, const auto& callContext) { return self->doCall(callContext); });
    }

    std::string_view getFunctionType() const final {
        return "TestFunction";
    }

private:
    Ref<PlatformFunctionTrampoline<Value>> _trampoline;
    Weak<TestObject> _receiver;
    Ref<TestFunction> _testFunction;
    ReferenceInfo _referenceInfo;
    size_t _arity;
    bool _hasReceiver;

    Value doCall(const ValueFunctionCallContext& callContext) {
        auto referenceInfoBuilder = ReferenceInfoBuilder(_referenceInfo);
        SmallVector<Value, 8> parameters;
        parameters.resize(_arity);

        if (_hasReceiver) {
            parameters[0] = Value(_receiver.lock());
            if (!_trampoline->unmarshallParameters(callContext.getParameters(),
                                                   callContext.getParametersSize(),
                                                   parameters.data() + 1,
                                                   parameters.size() - 1,
                                                   referenceInfoBuilder,
                                                   callContext.getExceptionTracker())) {
                return Value();
            }
        } else {
            if (!_trampoline->unmarshallParameters(callContext.getParameters(),
                                                   callContext.getParametersSize(),
                                                   parameters.data(),
                                                   parameters.size(),
                                                   referenceInfoBuilder,
                                                   callContext.getExceptionTracker())) {
                return Value();
            }
        }

        auto callResult = _testFunction->call(parameters.data(), parameters.size());
        if (!callResult) {
            callContext.getExceptionTracker().onError(callResult.moveError());
            return Value();
        }

        return _trampoline->marshallReturnValue(
            callResult.value(), referenceInfoBuilder, callContext.getExceptionTracker());
    }
};

class TestFunctionClass : public PlatformFunctionClassDelegate<Value> {
public:
    TestFunctionClass(const Ref<PlatformFunctionTrampoline<Value>>& trampoline, const Ref<ValueFunctionSchema>& schema)
        : _trampoline(trampoline), _schema(schema) {}
    ~TestFunctionClass() override = default;

    Value newFunction(const Ref<ValueFunction>& valueFunction,
                      const ReferenceInfoBuilder& referenceInfoBuilder,
                      ExceptionTracker& exceptionTracker) final {
        return Value(
            makeShared<TestFunctionWithValueFunction>(_trampoline, referenceInfoBuilder.build(), valueFunction));
    }

    Ref<ValueFunction> toValueFunction(const Value* receiver,
                                       const Value& function,
                                       const ReferenceInfoBuilder& referenceInfoBuilder,
                                       ExceptionTracker& exceptionTracker) final {
        auto testFunction = function.getTypedRef<TestFunction>();
        if (testFunction == nullptr) {
            exceptionTracker.onError(Error("Unable to retrieve TestFunction"));
            return nullptr;
        }
        if (receiver != nullptr) {
            return makeShared<ValueFunctionWithTestFunction>(_trampoline,
                                                             receiver->getTypedRef<TestObject>(),
                                                             testFunction,
                                                             referenceInfoBuilder.build(),
                                                             _schema->getParametersSize() + 1);
        } else {
            return makeShared<ValueFunctionWithTestFunction>(
                _trampoline, nullptr, testFunction, referenceInfoBuilder.build(), _schema->getParametersSize());
        }
    }

private:
    Ref<PlatformFunctionTrampoline<Value>> _trampoline;
    Ref<ValueFunctionSchema> _schema;
};

class TestMap : public ValdiObject, public ValueConvertible {
public:
    explicit TestMap(size_t capacity) {
        _map.reserve(capacity);
    }
    ~TestMap() override = default;

    const FlatMap<Value, Value>& getMap() const {
        return _map;
    }

    FlatMap<Value, Value>& getMap() {
        return _map;
    }

    const StringBox& getClassName() const override {
        static auto kClassName = STRING_LITERAL("TestMap");
        return kClassName;
    }

    Ref<ValueArray> getKeys() const {
        auto keys = ValueArray::make(_map.size());

        size_t i = 0;
        for (const auto& it : _map) {
            (*keys)[i] = it.first;
            i++;
        }

        std::sort(keys->begin(), keys->end());

        return keys;
    }

    Value toValue() final {
        auto keys = getKeys();

        auto values = ValueArray::make(keys->size());

        size_t i = 0;
        for (const auto& key : *keys) {
            (*values)[i] = _map.find(key)->second;
            i++;
        }

        return Value().setMapValue("keys", Value(keys)).setMapValue("values", Value(values));
    }

private:
    FlatMap<Value, Value> _map;
};

class Test6ESMap : public TestMap {
public:
    Test6ESMap() : TestMap(0) {}
    const StringBox& getClassName() const final {
        static auto kClassName = STRING_LITERAL("Test6ESMap");
        return kClassName;
    }
};

class TestES6Set : public ValdiObject, public ValueConvertible {
public:
    TestES6Set() {}

    ~TestES6Set() override = default;

    const StringBox& getClassName() const final {
        static auto kClassName = STRING_LITERAL("TestES6Set");
        return kClassName;
    }

    void insert(Value value) {
        _entries.emplace(std::move(value));
    }

    Value toValue() final {
        return Value(_entries.build());
    }

    const ValueArrayBuilder& getEntries() const {
        return _entries;
    }

private:
    ValueArrayBuilder _entries;
};

class TestObjectProxy : public ValueTypedProxyObject {
public:
    TestObjectProxy(const Value& object, const Ref<ValueTypedObject>& typedObject)
        : ValueTypedProxyObject(typedObject), _object(object) {}

    std::string_view getType() const final {
        return "Test Proxy";
    }

    const Value& getObject() const {
        return _object;
    }

private:
    Value _object;
};

class TestObjectClass : public PlatformObjectClassDelegate<Value> {
public:
    TestObjectClass(const Ref<ClassSchema>& schema) : _schema(schema) {}

    ~TestObjectClass() override = default;

    Value newObject(const Value* propertyValues, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<TestObject>(_schema->getClassName(), propertyValues, _schema->getPropertiesSize()));
    }

    Value getProperty(const Value& object, size_t propertyIndex, ExceptionTracker& exceptionTracker) final {
        auto testObject = object.getTypedRef<TestObject>();

        if (_schema->isInterface()) {
            auto prototype = testObject->getPrototype();
            SC_ASSERT(prototype != nullptr);
            return prototype->getProperty(propertyIndex);
        } else {
            return testObject->getProperty(propertyIndex);
        }
    }

    Ref<ValueTypedProxyObject> newProxy(const Value& object,
                                        const Ref<ValueTypedObject>& typedObject,
                                        ExceptionTracker& exceptionTracker) final {
        return makeShared<TestObjectProxy>(object, typedObject);
    }

private:
    Ref<ClassSchema> _schema;
};

class TestEnum : public ValdiObject, public ValueConvertible {
public:
    TestEnum(const StringBox& enumName, size_t caseIndex, bool isBoxed)
        : _enumName(enumName), _caseIndex(caseIndex), _boxed(isBoxed) {}
    ~TestEnum() override = default;

    const StringBox& getClassName() const final {
        return _enumName;
    }

    size_t getCaseIndex() const {
        return _caseIndex;
    }

    bool isBoxed() const {
        return _boxed;
    }

    Value toValue() final {
        return Value()
            .setMapValue("enumName", Value(_enumName))
            .setMapValue("enumCase", Value(static_cast<int32_t>(_caseIndex)))
            .setMapValue("boxed", Value(_boxed));
    }

private:
    StringBox _enumName;
    size_t _caseIndex;
    bool _boxed;
};

class TestEnumClass : public PlatformEnumClassDelegate<Value> {
public:
    TestEnumClass(const Ref<EnumSchema>& schema) : _schema(schema) {}

    ~TestEnumClass() override = default;

    Value newEnum(size_t enumCaseIndex, bool asBoxed, ExceptionTracker& exceptionTracker) final {
        auto testEnum = makeShared<TestEnum>(_schema->getName(), enumCaseIndex, asBoxed);
        return Value(testEnum);
    }

    Value enumCaseToValue(const Value& enumeration, bool isBoxed, ExceptionTracker& exceptionTracker) final {
        auto testEnum = enumeration.getTypedRef<TestEnum>();
        SC_ASSERT(isBoxed == testEnum->isBoxed());
        auto index = testEnum->getCaseIndex();
        return _schema->getCase(index).value;
    }

private:
    Ref<EnumSchema> _schema;
};

class TestObjectStore : public PlatformObjectStore<Value> {
public:
    TestObjectStore() = default;
    ~TestObjectStore() override = default;

    Ref<RefCountable> getValueForObjectKey(const Value& objectKey, ExceptionTracker& exceptionTracker) final {
        auto testObject = objectKey.getTypedRef<TestObject>();
        SC_ASSERT(testObject != nullptr);
        return testObject->getAttachedObject();
    }

    void setValueForObjectKey(const Value& objectKey,
                              const Ref<RefCountable>& value,
                              ExceptionTracker& exceptionTracker) final {
        auto testObject = objectKey.getTypedRef<TestObject>();
        SC_ASSERT(testObject != nullptr);
        testObject->setAttachedObject(value);
    }

    std::optional<Value> getObjectForId(uint32_t objectId, ExceptionTracker& exceptionTracker) final {
        const auto& it = _objectById.find(objectId);

        if (it == _objectById.end()) {
            return std::nullopt;
        }

        auto testObject = it->second.lock();
        if (testObject == nullptr) {
            return std::nullopt;
        }

        return Value(testObject);
    }

    void setObjectForId(uint32_t objectId, const Value& object, ExceptionTracker& exceptionTracker) final {
        auto testObject = object.getTypedRef<TestObject>();
        SC_ASSERT(testObject != nullptr);

        _objectById[objectId] = testObject.toWeak();
    }

private:
    FlatMap<uint32_t, Weak<TestObject>> _objectById;
};

class TestPlatformValueDelegate : public PlatformValueDelegate<Value> {
public:
    Value newVoid() final {
        return Value::undefined();
    }

    Value newNull() final {
        return Value();
    }

    Value newInt(int32_t value, ExceptionTracker& exceptionTracker) final {
        return Value(value);
    }

    Value newIntObject(int32_t value, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<TestNumberObject>(NumberType::Integer, Value(value)));
    }

    Value newLong(int64_t value, ExceptionTracker& exceptionTracker) final {
        return Value(value);
    }

    Value newLongObject(int64_t value, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<TestNumberObject>(NumberType::LongInteger, Value(value)));
    }

    Value newDouble(double value, ExceptionTracker& exceptionTracker) final {
        return Value(value);
    }

    Value newDoubleObject(double value, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<TestNumberObject>(NumberType::Double, Value(value)));
    }

    Value newBool(bool value, ExceptionTracker& exceptionTracker) final {
        return Value(value);
    }

    Value newBoolObject(bool value, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<TestNumberObject>(NumberType::Boolean, Value(value)));
    }

    Value newStringUTF8(std::string_view str, ExceptionTracker& exceptionTracker) final {
        return Value(StringCache::getGlobal().makeString(str));
    }

    Value newStringUTF16(std::u16string_view str, ExceptionTracker& exceptionTracker) final {
        return Value(StaticString::makeUTF16(str.data(), str.size()));
    }

    Value newMap(size_t capacity, ExceptionTracker& exceptionTracker) final {
        auto map = makeShared<TestMap>(capacity);

        return Value(map);
    }

    void setMapEntry(const Value& map, const Value& key, const Value& value, ExceptionTracker& exceptionTracker) final {
        auto mapRef = map.getTypedRef<TestMap>();
        mapRef->getMap()[key] = value;
    }

    size_t getMapEstimatedLength(const Value& map, ExceptionTracker& exceptionTracker) final {
        auto mapRef = map.getTypedRef<TestMap>();
        return mapRef->getMap().size();
    }

    void visitMapKeyValues(const Value& map,
                           PlatformMapValueVisitor<Value>& visitor,
                           ExceptionTracker& exceptionTracker) final {
        auto mapRef = map.getTypedRef<TestMap>();

        for (const auto& it : mapRef->getMap()) {
            if (!visitor.visit(it.first, it.second, exceptionTracker)) {
                break;
            }
        }
    }

    Value newES6Collection(CollectionType type, ExceptionTracker& exceptionTracker) final {
        switch (type) {
            case CollectionType::Map:
                return Value(makeShared<Test6ESMap>());
            case CollectionType::Set:
                return Value(makeShared<TestES6Set>());
        }
    }

    void setES6CollectionEntry(const Value& collection,
                               CollectionType type,
                               std::initializer_list<Value> items,
                               ExceptionTracker& exceptionTracker) final {
        switch (type) {
            case CollectionType::Map: {
                auto mapRef = collection.checkedToValdiObject<Test6ESMap>(exceptionTracker);
                if (mapRef == nullptr) {
                    return;
                }
                mapRef->getMap()[*items.begin()] = *(items.begin() + 1);
                break;
            }
            case CollectionType::Set: {
                auto setRef = collection.checkedToValdiObject<TestES6Set>(exceptionTracker);
                if (setRef == nullptr) {
                    return;
                }
                setRef->insert(*items.begin());
                break;
            }
        }
    }

    void visitES6Collection(const Value& collection,
                            PlatformMapValueVisitor<Value>& visitor,
                            ExceptionTracker& exceptionTracker) final {
        auto mapRef = collection.getTypedRef<Test6ESMap>();
        if (mapRef != nullptr) {
            auto keys = mapRef->getKeys();
            for (const auto& key : *keys) {
                auto value = mapRef->getMap().find(key)->second;
                visitor.visit(key, value, exceptionTracker);
            }
            return;
        }

        auto setRef = collection.getTypedRef<TestES6Set>();
        if (setRef != nullptr) {
            for (const auto& it : setRef->getEntries()) {
                visitor.visit(it, it, exceptionTracker);
            }
            return;
        }
    }

    PlatformArrayBuilder<Value> newArrayBuilder(size_t capacity, ExceptionTracker& exceptionTracker) final {
        return PlatformArrayBuilder<Value>(Value(ValueArray::make(capacity)));
    }

    void setArrayEntry(const PlatformArrayBuilder<Value>& arrayBuilder,
                       size_t index,
                       const Value& value,
                       ExceptionTracker& exceptionTracker) final {
        auto arrayRef = arrayBuilder.getBuilder().getArrayRef();
        arrayRef->emplace(index, value);
    }

    Value finalizeArray(PlatformArrayBuilder<Value>& arrayBuilder, ExceptionTracker& exceptionTracker) {
        return std::move(arrayBuilder.getBuilder());
    }

    PlatformArrayIterator<Value> newArrayIterator(const Value& array, ExceptionTracker& exceptionTracker) final {
        return PlatformArrayIterator<Value>(array, array.getArrayRef()->size());
    }

    Value getArrayItem(const PlatformArrayIterator<Value>& arrayIterator,
                       size_t index,
                       ExceptionTracker& exceptionTracker) final {
        return (*arrayIterator.getIterator().getArrayRef())[index];
    }

    Value newBridgedPromise(const Ref<Promise>& promise,
                            const Ref<ValueMarshaller<Value>>& valueMarshaller,
                            ExceptionTracker& exceptionTracker) final {
        auto resolvablePromise = makeShared<ResolvablePromise>();

        promise->onComplete([resolvablePromise, valueMarshaller](const auto& result) {
            if (!result) {
                resolvablePromise->fulfill(result);
                return;
            }

            SimpleExceptionTracker exceptionTracker;
            auto unmarshalledResult =
                valueMarshaller->unmarshall(result.value(), ReferenceInfoBuilder(), exceptionTracker);
            if (!exceptionTracker) {
                resolvablePromise->fulfill(exceptionTracker.extractError());
                return;
            }

            resolvablePromise->fulfill(unmarshalledResult);
        });

        resolvablePromise->setCancelCallback([promise = weakRef(promise.get())]() {
            auto strongPromise = promise.lock();
            if (strongPromise != nullptr) {
                strongPromise->cancel();
            }
        });

        return Value(resolvablePromise);
    }

    Value newByteArray(const BytesView& bytes, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<ValueTypedArray>(TypedArrayType::ArrayBuffer, bytes));
    }
    Value newTypedArray(TypedArrayType arrayType, const BytesView& bytes, ExceptionTracker& exceptionTracker) final {
        return Value(makeShared<ValueTypedArray>(arrayType, bytes));
    }
    Value newUntyped(const Value& value,
                     const ReferenceInfoBuilder& referenceInfoBuilder,
                     ExceptionTracker& exceptionTracker) final {
        return value;
    }

    Ref<PlatformObjectClassDelegate<Value>> newObjectClass(const Ref<ClassSchema>& schema,
                                                           ExceptionTracker& exceptionTracker) final {
        return makeShared<TestObjectClass>(schema);
    }

    Ref<PlatformEnumClassDelegate<Value>> newEnumClass(const Ref<EnumSchema>& schema,
                                                       ExceptionTracker& exceptionTracker) final {
        return makeShared<TestEnumClass>(schema);
    }

    Ref<PlatformFunctionClassDelegate<Value>> newFunctionClass(const Ref<PlatformFunctionTrampoline<Value>>& trampoline,
                                                               const Ref<ValueFunctionSchema>& schema,
                                                               ExceptionTracker& exceptionTracker) final {
        return makeShared<TestFunctionClass>(trampoline, schema);
    }

    bool valueIsNull(const Value& value) const final {
        return value.isNullOrUndefined();
    }

    int32_t valueToInt(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return value.toInt();
    }

    int64_t valueToLong(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return value.toLong();
    }

    double valueToDouble(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return value.toDouble();
    }

    bool valueToBool(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return value.toBool();
    }

    int32_t valueObjectToInt(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return TestNumberObject::unwrap(value, NumberType::Integer).toInt();
    }

    int64_t valueObjectToLong(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return TestNumberObject::unwrap(value, NumberType::LongInteger).toLong();
    }

    double valueObjectToDouble(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return TestNumberObject::unwrap(value, NumberType::Double).toDouble();
    }

    bool valueObjectToBool(const Value& value, ExceptionTracker& exceptionTracker) const final {
        return TestNumberObject::unwrap(value, NumberType::Boolean).toBool();
    }

    Ref<StaticString> valueToString(const Value& value, ExceptionTracker& exceptionTracker) const final {
        if (value.isString()) {
            auto strBox = value.toStringBox();
            return StaticString::makeUTF8(strBox.getCStr(), strBox.length());
        }
        return value.getStaticStringRef();
    }

    BytesView valueToByteArray(const Value& value,
                               const ReferenceInfoBuilder& referenceInfoBuilder,
                               ExceptionTracker& exceptionTracker) const final {
        return value.getTypedArray()->getBuffer();
    }
    Ref<ValueTypedArray> valueToTypedArray(const Value& value,
                                           const ReferenceInfoBuilder& referenceInfoBuilder,
                                           ExceptionTracker& exceptionTracker) const final {
        return value.getTypedArrayRef();
    }

    Ref<Promise> valueToPromise(const Value& value,
                                const Ref<ValueMarshaller<Value>>& valueMarshaller,
                                const ReferenceInfoBuilder& referenceInfoBuilder,
                                ExceptionTracker& exceptionTracker) const final {
        auto promise = value.checkedToValdiObject<Promise>(exceptionTracker);
        if (promise == nullptr) {
            return nullptr;
        }

        auto resolvablePromise = makeShared<ResolvablePromise>();

        promise->onComplete([weakResolvablePromise = weakRef(resolvablePromise.get()),
                             valueMarshaller](const Result<Value>& result) -> void {
            auto resolvablePromise = weakResolvablePromise.lock();
            if (resolvablePromise == nullptr) {
                return;
            }

            if (!result) {
                resolvablePromise->fulfill(result);
                return;
            }

            SimpleExceptionTracker exceptionTracker;
            auto marshalledResult =
                valueMarshaller->marshall(nullptr, result.value(), ReferenceInfoBuilder(), exceptionTracker);
            if (!exceptionTracker) {
                resolvablePromise->fulfill(exceptionTracker.extractError());
                return;
            }

            resolvablePromise->fulfill(marshalledResult);
        });

        resolvablePromise->setCancelCallback([promise]() { promise->cancel(); });

        return resolvablePromise;
    }

    Value newDate(double millisecondsSinceEpoch, ExceptionTracker& exceptionTracker) {
        return Value(millisecondsSinceEpoch);
    }
    double dateToDouble(const Value& value, ExceptionTracker& exceptionTracker) const {
        return value.toDouble();
    }

    BytesView encodeProto(const Value& proto,
                          const ReferenceInfoBuilder& referenceInfoBuilder,
                          ExceptionTracker& exceptionTracker) const {
        return {};
    }
    Value decodeProto(const BytesView& bytes,
                      const std::vector<std::string_view>& classNameParts,
                      const ReferenceInfoBuilder& referenceInfoBuilder,
                      ExceptionTracker& exceptionTracker) const {
        return {};
    }

    Value valueToUntyped(const Value& value,
                         const ReferenceInfoBuilder& referenceInfoBuilder,
                         ExceptionTracker& exceptionTracker) const final {
        return value;
    }

    PlatformObjectStore<Value>& getObjectStore() final {
        return _objectStore;
    }

private:
    TestObjectStore _objectStore;
};

class TestDispatchQueue : public IDispatchQueue {
public:
    TestDispatchQueue() = default;
    ~TestDispatchQueue() override = default;

    void sync(const DispatchFunction& function) final {
        function();
    }

    void async(DispatchFunction function) final {
        _tasks.emplace_back(std::move(function));
    }

    task_id_t asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) final {
        async(std::move(function));
        return 0;
    }

    void cancel(task_id_t taskId) final {}

    size_t getTasksCount() const {
        return _tasks.size();
    }

    void flushTasks() {
        while (!_tasks.empty()) {
            auto task = std::move(_tasks.front());
            _tasks.pop_front();
            task();
        }
    }

private:
    std::deque<DispatchFunction> _tasks;
};

class TestValueMarshallerRegistry : public SimpleRefCountable, public ValueMarshallerRegistry<Value> {
public:
    // withResolverRegistry=false mirrors the JS-side ValueMarshallerRegistry, whose type
    // resolver has no ValueSchemaRegistry and can only reach schemas through the
    // ValueSchemaReferences embedded in the schemas it receives.
    TestValueMarshallerRegistry(const Ref<ValueSchemaRegistry>& schemaRegistry,
                                const Ref<TestDispatchQueue>& testDispatchQueue = nullptr,
                                bool withResolverRegistry = true)
        : ValueMarshallerRegistry<Value>(ValueSchemaTypeResolver(withResolverRegistry ? schemaRegistry.get() : nullptr),
                                         makeShared<TestPlatformValueDelegate>(),
                                         testDispatchQueue,
                                         STRING_LITERAL("Test")),
          _schemaRegistry(schemaRegistry) {}

    Value unmarshall(ValueSchemaRegistrySchemaIdentifier identifier,
                     const Value& value,
                     ExceptionTracker& exceptionTracker) {
        auto valueMarshaller = getValueMarshallerForSchemaIdentifier(identifier, exceptionTracker);
        if (valueMarshaller == nullptr) {
            return Value();
        }

        return valueMarshaller->unmarshall(value, ReferenceInfoBuilder(), exceptionTracker);
    }

    Value marshall(ValueSchemaRegistrySchemaIdentifier identifier,
                   const Value& value,
                   ExceptionTracker& exceptionTracker) {
        auto valueMarshaller = getValueMarshallerForSchemaIdentifier(identifier, exceptionTracker);
        if (valueMarshaller == nullptr) {
            return Value();
        }

        return valueMarshaller->marshall(nullptr, value, ReferenceInfoBuilder(), exceptionTracker);
    }

private:
    Ref<ValueSchemaRegistry> _schemaRegistry;

    Ref<ValueMarshaller<Value>> getValueMarshallerForSchemaIdentifier(ValueSchemaRegistrySchemaIdentifier identifier,
                                                                      ExceptionTracker& exceptionTracker) {
        auto lock = _schemaRegistry->lock();

        auto entry = _schemaRegistry->getSchemaAndKeyForIdentifier(identifier);
        if (!entry) {
            exceptionTracker.onError(Error(STRING_FORMAT("Could not resolve schema for identifier '{}'", identifier)));
            return nullptr;
        }

        return getValueMarshaller(entry.value().schemaKey, entry.value().schema, exceptionTracker).valueMarshaller;
    }
};

TEST(ValueMarshallerRegistry, canMarshallSimpleObject) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("c 'MyObject'{'aString': s, 'aDouble': d}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object =
        Value().setMapValue("aString", Value(STRING_LITERAL("Hello world"))).setMapValue("aDouble", Value(42.0));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto testObject = result.getTypedRef<TestObject>();
    ASSERT_TRUE(testObject != nullptr);

    ASSERT_EQ(static_cast<size_t>(2), testObject->getPropertiesSize());

    ASSERT_EQ(Value(STRING_LITERAL("Hello world")), testObject->getProperty(0));
    ASSERT_EQ(Value(42.0), testObject->getProperty(1));

    // Now attempt marshalling

    testObject->setProperty(0, Value(STRING_LITERAL("Goodbye")));
    testObject->setProperty(1, Value(90.0));

    auto marshalledValue = valueMarshallerRegistry->marshall(identifier, Value(testObject), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value(ValueTypedObject::make(schema.value().getClassRef(),
                                           {Value(StaticString::makeUTF8("Goodbye")), Value(90.0)})),
              marshalledValue);

    ASSERT_EQ(
        Value().setMapValue("aString", Value(StaticString::makeUTF8("Goodbye"))).setMapValue("aDouble", Value(90.0)),
        Value(marshalledValue.getTypedObject()->toValueMap(true)));
}

TEST(ValueMarshallerRegistry, performsTypeValidationWhileUnmarshalling) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("c 'MyObject'{'aString': s, 'aDouble': d}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object =
        Value().setMapValue("aString", Value(42.0)).setMapValue("aDouble", Value(STRING_LITERAL("Not a double")));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_FALSE(exceptionTracker);
    ASSERT_EQ("Failed to unmarshall property 'aString' of class 'MyObject'\n[caused by]: Cannot convert type 'double' "
              "to type 'string'",
              exceptionTracker.extractError().toString());
}

TEST(ValueMarshallerRegistry, canMarshallObjectReferences) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("c 'MyObject'{'items': a<r:'MyOtherObject'>}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    auto items = ValueArray::make({
        Value().setMapValue("title", Value(STRING_LITERAL("Hello"))),
        Value().setMapValue("title", Value(STRING_LITERAL("World"))),
    });

    SimpleExceptionTracker exceptionTracker;
    auto object = Value().setMapValue("items", Value(items));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    // It should initially fail as the MyOtherObject schema is not registered yet.
    ASSERT_FALSE(exceptionTracker);
    ASSERT_TRUE(result.isNullOrUndefined());
    ASSERT_EQ("Could not resolve type references of schema key 'ref:'MyObject''\n[caused by]: Failed to resolve "
              "MyObject.items\n[caused by]: Could not resolve type reference 'MyOtherObject'\n[caused by]: Type not "
              "registered in ValueSchemaRegistry",
              exceptionTracker.extractError().toString());

    // Now register the schema

    auto myOtherSchema = ValueSchema::parse("c 'MyOtherObject'{'title': s}");

    ASSERT_TRUE(myOtherSchema) << myOtherSchema.description();

    auto myOtherSchemaIdentifier = schemaRegistry->registerSchema(myOtherSchema.value());

    // Now try again

    result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto testObject = result.getTypedRef<TestObject>();
    ASSERT_TRUE(testObject != nullptr);

    ASSERT_EQ(static_cast<size_t>(1), testObject->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("MyObject"), testObject->getClassName());

    auto itemsInsideTestObject = testObject->getProperty(0);

    ASSERT_TRUE(itemsInsideTestObject.isArray());
    ASSERT_EQ(static_cast<size_t>(2), itemsInsideTestObject.getArray()->size());

    auto item1 = (*itemsInsideTestObject.getArray())[0].getTypedRef<TestObject>();

    ASSERT_TRUE(item1 != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), item1->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("MyOtherObject"), item1->getClassName());
    ASSERT_EQ(Value(STRING_LITERAL("Hello")), item1->getProperty(0));

    auto item2 = (*itemsInsideTestObject.getArray())[1].getTypedRef<TestObject>();

    ASSERT_TRUE(item2 != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), item2->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("MyOtherObject"), item2->getClassName());
    ASSERT_EQ(Value(STRING_LITERAL("World")), item2->getProperty(0));

    // Now attempt marshalling

    item1->setProperty(0, Value(STRING_LITERAL("Some Title")));
    item2->setProperty(0, Value(STRING_LITERAL("Some other Title")));

    std::initializer_list<Value> properties = {Value(STRING_LITERAL("This is added"))};

    auto item3 = makeShared<TestObject>(STRING_LITERAL("MyOtherObject"), std::move(properties));

    testObject->setProperty(0,
                            Value(ValueArray::make({
                                Value(item1),
                                Value(item2),
                                Value(item3),
                            })));

    auto marshalledValue = valueMarshallerRegistry->marshall(identifier, Value(testObject), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value(ValueTypedObject::make(
                  schemaRegistry->getSchemaForIdentifier(identifier).getClassRef(),
                  {
                      Value(ValueArray::make({
                          Value(ValueTypedObject::make(
                              schemaRegistry->getSchemaForIdentifier(myOtherSchemaIdentifier).getClassRef(),
                              {Value(StaticString::makeUTF8("Some Title"))})),
                          Value(ValueTypedObject::make(
                              schemaRegistry->getSchemaForIdentifier(myOtherSchemaIdentifier).getClassRef(),
                              {Value(StaticString::makeUTF8("Some other Title"))})),
                          Value(ValueTypedObject::make(
                              schemaRegistry->getSchemaForIdentifier(myOtherSchemaIdentifier).getClassRef(),
                              {Value(StaticString::makeUTF8("This is added"))})),
                      })),
                  })),
              marshalledValue);
}

TEST(ValueMarshallerRegistry, supportsGenericTypes) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto observableSchema = ValueSchema::parse("c 'Observable'{'items': a<r:0>}");

    ASSERT_TRUE(observableSchema) << observableSchema.description();

    schemaRegistry->registerSchema(observableSchema.value());

    auto userSchema = ValueSchema::parse("c 'User'{'username': s?}");

    ASSERT_TRUE(userSchema) << userSchema.description();

    schemaRegistry->registerSchema(userSchema.value());

    auto userStoreSchema = ValueSchema::parse("c 'UserStore'{'users': g:'Observable'<r:'User'>}");

    ASSERT_TRUE(userStoreSchema) << userStoreSchema.description();

    auto userStoreSchemaIdentifier = schemaRegistry->registerSchema(userStoreSchema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object =
        Value().setMapValue("users",
                            Value().setMapValue("items",
                                                Value(ValueArray::make({
                                                    Value().setMapValue("username", Value(STRING_LITERAL("ABC"))),
                                                    Value().setMapValue("username", Value(STRING_LITERAL("DEF"))),
                                                }))));

    auto result = valueMarshallerRegistry->unmarshall(userStoreSchemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto testObject = result.getTypedRef<TestObject>();
    ASSERT_TRUE(testObject != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), testObject->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("UserStore"), testObject->getClassName());

    auto usersObservable = testObject->getProperty(0).getTypedRef<TestObject>();
    ASSERT_TRUE(usersObservable != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), usersObservable->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("Observable"), usersObservable->getClassName());

    auto users = usersObservable->getProperty(0).getArrayRef();
    ASSERT_TRUE(users != nullptr);
    ASSERT_EQ(static_cast<size_t>(2), users->size());

    auto user1 = (*users)[0].getTypedRef<TestObject>();

    ASSERT_TRUE(user1 != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), user1->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("User"), user1->getClassName());
    ASSERT_EQ(Value(STRING_LITERAL("ABC")), user1->getProperty(0));

    auto user2 = (*users)[1].getTypedRef<TestObject>();

    ASSERT_TRUE(user2 != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), user2->getPropertiesSize());
    ASSERT_EQ(STRING_LITERAL("User"), user2->getClassName());
    ASSERT_EQ(Value(STRING_LITERAL("DEF")), user2->getProperty(0));

    ASSERT_EQ(
        std::vector<ValueSchema>(
            {ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("UserStore"))),
             ValueSchema::array(ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("User")))),
             ValueSchema::genericTypeReference(
                 ValueSchemaTypeReference::named(STRING_LITERAL("Observable")),
                 {
                     ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("User"))),
                 }),
             ValueSchema::string(),
             ValueSchema::optional(ValueSchema::string()),
             ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("User")))}),
        valueMarshallerRegistry->getValueMarshallerKeys());
}

TEST(ValueMarshallerRegistry, supportsBoxingInGenericTypes) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto wrapperSchema = ValueSchema::parse("c 'Wrapper'{'item': r:0}");

    ASSERT_TRUE(wrapperSchema) << wrapperSchema.description();

    schemaRegistry->registerSchema(wrapperSchema.value());

    auto enumSchema = ValueSchema::parse("e<i> 'StatusCode'{'ok': 200, 'divine': 42}");
    ASSERT_TRUE(enumSchema) << enumSchema.description();

    auto enumSchemaKey =
        ValueSchema::typeReference(Valdi::ValueSchemaTypeReference::named(STRING_LITERAL("StatusCode")));
    schemaRegistry->registerSchema(enumSchemaKey, enumSchema.value());
    schemaRegistry->registerSchema(enumSchemaKey.asBoxed(), enumSchema.value().asBoxed());

    auto userSchema = ValueSchema::parse("c 'User'{'status': g:'Wrapper'<r@:'StatusCode'>}");

    ASSERT_TRUE(userSchema) << userSchema.description();

    auto userSchemaIdentifier = schemaRegistry->registerSchema(userSchema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object = Value().setMapValue("status", Value().setMapValue("item", Value(static_cast<int32_t>(42))));

    auto result = valueMarshallerRegistry->unmarshall(userSchemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(
        Value()
            .setMapValue("className", Value(STRING_LITERAL("User")))
            .setMapValue(
                "properties",
                Value(ValueArray::make(
                    {Value()
                         .setMapValue("className", Value(STRING_LITERAL("Wrapper")))
                         .setMapValue("properties",
                                      Value(ValueArray::make(
                                          {Value()
                                               .setMapValue("boxed", Value(true))
                                               .setMapValue("enumCase", Value(static_cast<int32_t>(1)))
                                               .setMapValue("enumName", Value(STRING_LITERAL("StatusCode")))})))}))),
        flattenValue(result));
}

TEST(ValueMarshallerRegistry, supportsRecursiveTypes) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto userSchema = ValueSchema::parse("c 'User'{'store': r?:'UserStore', 'name': s}");

    ASSERT_TRUE(userSchema) << userSchema.description();

    schemaRegistry->registerSchema(userSchema.value());

    auto userStoreSchema = ValueSchema::parse("c 'UserStore'{'users': a<r:'User'>}");

    ASSERT_TRUE(userStoreSchema) << userStoreSchema.description();

    auto userStoreSchemaIdentifier = schemaRegistry->registerSchema(userStoreSchema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object = Value().setMapValue(
        "users",
        Value(ValueArray::make({
            Value().setMapValue("name", Value(STRING_LITERAL("User1"))),
            Value()
                .setMapValue("name", Value(STRING_LITERAL("User2")))
                .setMapValue("store",
                             Value().setMapValue("users",
                                                 Value(ValueArray::make(
                                                     {Value().setMapValue("name", Value(STRING_LITERAL("User3")))})))),
        })));

    auto result = valueMarshallerRegistry->unmarshall(userStoreSchemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(
        Value()
            .setMapValue("className", Value(STRING_LITERAL("UserStore")))
            .setMapValue(
                "properties",
                Value(ValueArray::make({Value(ValueArray::make({
                    Value()
                        .setMapValue("className", Value(STRING_LITERAL("User")))
                        .setMapValue("properties", Value(ValueArray::make({Value(), Value(STRING_LITERAL("User1"))}))),
                    Value()
                        .setMapValue("className", Value(STRING_LITERAL("User")))
                        .setMapValue(
                            "properties",
                            Value(ValueArray::make(
                                {Value()
                                     .setMapValue("className", Value(STRING_LITERAL("UserStore")))
                                     .setMapValue(
                                         "properties",
                                         Value(ValueArray::make({Value(ValueArray::make(
                                             {Value()
                                                  .setMapValue("className", Value(STRING_LITERAL("User")))
                                                  .setMapValue("properties",
                                                               Value(ValueArray::make(
                                                                   {Value(), Value(STRING_LITERAL("User3"))})))}))}))),
                                 Value(STRING_LITERAL("User2"))}))),
                }))}))),
        flattenValue(result));
}

TEST(ValueMarshallerRegistry, supportsUnmarshallingFunction) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto userSchema = ValueSchema::parse("c 'User'{'firstname': s, 'lastname': s}");

    ASSERT_TRUE(userSchema) << userSchema.description();
    schemaRegistry->registerSchema(userSchema.value());

    auto userUtilsSchema = ValueSchema::parse("c 'UserUtils'{'formatName': f(r:'User'): s}");
    ASSERT_TRUE(userUtilsSchema) << userUtilsSchema.description();
    auto userUtilsSchemaIdentifier = schemaRegistry->registerSchema(userUtilsSchema.value());

    auto nameFormatter =
        makeShared<ValueFunctionWithCallable>([](const ValueFunctionCallContext& callContext) -> Value {
            auto typedObject =
                callContext.getParameter(0).checkedTo<Ref<ValueTypedObject>>(callContext.getExceptionTracker());
            if (!callContext.getExceptionTracker()) {
                return Value();
            }

            auto firstname = typedObject->getProperty(0).checkedTo<StringBox>(callContext.getExceptionTracker());
            if (!callContext.getExceptionTracker()) {
                return Value();
            }

            auto lastname = typedObject->getProperty(1).checkedTo<StringBox>(callContext.getExceptionTracker());
            if (!callContext.getExceptionTracker()) {
                return Value();
            }

            return Value(STRING_FORMAT("{} {}", firstname, lastname));
        });

    SimpleExceptionTracker exceptionTracker;
    auto object = Value().setMapValue("formatName", Value(nameFormatter));

    auto result = valueMarshallerRegistry->unmarshall(userUtilsSchemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto userUtils = result.getTypedRef<TestObject>();
    ASSERT_TRUE(userUtils != nullptr);

    auto unmarshalledNameFormatter = userUtils->getProperty(0).getTypedRef<TestFunction>();

    ASSERT_TRUE(unmarshalledNameFormatter != nullptr);

    // The unmarshalle name formatter takes parameters as TestObject instances

    std::initializer_list<Value> properties = {Value(STRING_LITERAL("Jean Jacque")),
                                               Value(STRING_LITERAL("Delacroix"))};

    auto user = Value(makeShared<TestObject>(STRING_LITERAL("User"), std::move(properties)));

    auto callResult = unmarshalledNameFormatter->call(&user, 1);

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(Value(STRING_LITERAL("Jean Jacque Delacroix")), callResult.value());
}

TEST(ValueMarshallerRegistry, supportsMarshallingFunction) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto userSchema = ValueSchema::parse("c 'User'{'firstname': s, 'lastname': s}");

    ASSERT_TRUE(userSchema) << userSchema.description();
    schemaRegistry->registerSchema(userSchema.value());

    auto userUtilsSchema = ValueSchema::parse("c 'UserUtils'{'formatName': f(r:'User'): s}");
    ASSERT_TRUE(userUtilsSchema) << userUtilsSchema.description();
    auto userUtilsSchemaIdentifier = schemaRegistry->registerSchema(userUtilsSchema.value());

    auto nameFormatter =
        makeShared<TestFunctionWithCallable>([](const Value* parameters, size_t parametersSize) -> Result<Value> {
            if (parametersSize != 1) {
                return Error("Expected 1 parameter");
            }

            auto user = parameters[0].getTypedRef<TestObject>();
            if (user == nullptr || user->getClassName() != STRING_LITERAL("User")) {
                return Error("Expected user object");
            }

            auto firstname = user->getProperty(0).toStringBox();
            auto lastname = user->getProperty(1).toStringBox();

            return Value(STRING_FORMAT("{} {}", firstname, lastname));
        });

    std::initializer_list<Value> properties = {Value(nameFormatter)};

    auto userUtilsObject = Value(makeShared<TestObject>(STRING_LITERAL("UserUtils"), std::move(properties)));

    SimpleExceptionTracker exceptionTracker;
    auto marshalledValue =
        valueMarshallerRegistry->marshall(userUtilsSchemaIdentifier, userUtilsObject, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto typedObject = marshalledValue.getTypedObjectRef();
    auto function = typedObject->getProperty(0).getFunctionRef();

    ASSERT_TRUE(function != nullptr);

    // Now call the function through the marshaller API.
    // It should take care of converting the User into a User object, call the TestFunctionWithCallable, and return the
    // result

    std::initializer_list<Value> parameters = {Value()
                                                   .setMapValue("firstname", Value(STRING_LITERAL("Jean Jacque")))
                                                   .setMapValue("lastname", Value(STRING_LITERAL("Delacroix")))};

    auto result = (*function)(parameters.begin(), parameters.size());

    ASSERT_TRUE(result) << result.description();

    auto callResult = result.value().toStringBox();
    ASSERT_EQ(STRING_LITERAL("Jean Jacque Delacroix"), callResult);
}

TEST(ValueMarshallerRegistry, callsMarshalledFunctionOnCallbackQueue) {
    auto testQueue = makeShared<TestDispatchQueue>();
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry, testQueue);

    auto metricsSchema = ValueSchema::parse("c 'Metrics'{'enqueue': f|w|(s)}");
    ASSERT_TRUE(metricsSchema) << metricsSchema.description();
    auto metricsSchemaIdentifier = schemaRegistry->registerSchema(metricsSchema.value());
    std::vector<StringBox> enqueuedMetrics;

    auto enqueueMethod =
        makeShared<TestFunctionWithCallable>([&](const Value* parameters, size_t parametersSize) -> Result<Value> {
            if (parametersSize != 1) {
                return Error("Expected 1 parameter");
            }

            enqueuedMetrics.emplace_back(parameters[0].toStringBox());

            return Value();
        });

    std::initializer_list<Value> properties = {Value(enqueueMethod)};

    auto metricsObject = Value(makeShared<TestObject>(STRING_LITERAL("Metrics"), std::move(properties)));

    SimpleExceptionTracker exceptionTracker;
    auto marshalledValue = valueMarshallerRegistry->marshall(metricsSchemaIdentifier, metricsObject, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto typedObject = marshalledValue.getTypedObjectRef();
    auto function = typedObject->getProperty(0).getFunctionRef();

    ASSERT_TRUE(function != nullptr);

    auto callResult = (*function)({Value(STRING_LITERAL("My metric!"))});

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(Value::undefined(), callResult.value());

    // The call should to the function should have been enqueued into the task queue
    ASSERT_EQ(static_cast<size_t>(1), testQueue->getTasksCount());
    ASSERT_EQ(static_cast<size_t>(0), enqueuedMetrics.size());

    testQueue->flushTasks();

    ASSERT_EQ(static_cast<size_t>(1), enqueuedMetrics.size());
    ASSERT_EQ(static_cast<size_t>(0), testQueue->getTasksCount());

    ASSERT_EQ(std::vector<StringBox>({STRING_LITERAL("My metric!")}), enqueuedMetrics);
}

TEST(ValueMarshallerRegistry, callsMarshalledFunctionOnCallbackQueueWithPromiseResult) {
    auto testQueue = makeShared<TestDispatchQueue>();
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry, testQueue);

    auto doublerSchema = ValueSchema::parse("c 'Doubler'{'double': f|w|(i): p<i>}");
    ASSERT_TRUE(doublerSchema) << doublerSchema.description();
    auto doublerSchemaIdentifier = schemaRegistry->registerSchema(doublerSchema.value());

    auto doublerMethod =
        makeShared<TestFunctionWithCallable>([](const Value* parameters, size_t parametersSize) -> Result<Value> {
            if (parametersSize != 1) {
                return Error("Expected 1 parameter");
            }

            auto promise = makeShared<ResolvablePromise>();
            promise->fulfill(Value(parameters[0].toInt() * 2));

            return Value(promise);
        });

    std::initializer_list<Value> properties = {Value(doublerMethod)};

    auto doublerObject = Value(makeShared<TestObject>(STRING_LITERAL("Doubler"), std::move(properties)));

    SimpleExceptionTracker exceptionTracker;
    auto marshalledValue = valueMarshallerRegistry->marshall(doublerSchemaIdentifier, doublerObject, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto typedObject = marshalledValue.getTypedObjectRef();
    auto function = typedObject->getProperty(0).getFunctionRef();

    ASSERT_TRUE(function != nullptr);

    auto callResult = (*function)({Value(static_cast<int32_t>(21))});

    ASSERT_TRUE(callResult) << callResult.description();

    auto promise = callResult.value().getTypedRef<Promise>();

    ASSERT_TRUE(promise != nullptr);

    Result<Value> promiseResult;
    promise->onComplete([&](const auto& result) { promiseResult = result; });

    // The call should to the function should have been enqueued into the task queue
    ASSERT_EQ(static_cast<size_t>(1), testQueue->getTasksCount());
    ASSERT_TRUE(promiseResult.empty());

    testQueue->flushTasks();

    ASSERT_EQ(static_cast<size_t>(0), testQueue->getTasksCount());
    ASSERT_TRUE(promiseResult) << promiseResult.description();
    ASSERT_EQ(Value(static_cast<int32_t>(42)), promiseResult.value());
}

TEST(ValueMarshallerRegistry, supportsNumberObjects) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema =
        ValueSchema::parse("c 'MyObject'{'aBoxedDouble': d@, 'anOptionalBoxedBoolean': b@?, 'anArray': a<i@>}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object = Value()
                      .setMapValue("aBoxedDouble", Value(42.0))
                      .setMapValue("anOptionalBoxedBoolean", Value())
                      .setMapValue("anArray", Value(ValueArray::make({Value(static_cast<int32_t>(90))})));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(
        Value()
            .setMapValue("className", Value(STRING_LITERAL("MyObject")))
            .setMapValue("properties",
                         Value(ValueArray::make(
                             {Value()
                                  .setMapValue("className", Value(STRING_LITERAL("Double")))
                                  .setMapValue("value", Value(42.0)),
                              Value(),
                              Value(ValueArray::make({Value()
                                                          .setMapValue("className", Value(STRING_LITERAL("Integer")))
                                                          .setMapValue("value", Value(static_cast<int32_t>(90)))}))

                             }))),
        flattenValue(result));

    // Now attempt marshalling

    std::initializer_list<Value> properties = {
        Value(makeShared<TestNumberObject>(NumberType::Double, Value(1337.0))),
        Value(makeShared<TestNumberObject>(NumberType::Boolean, Value(true))),
        Value(ValueArray::make({
            Value(makeShared<TestNumberObject>(NumberType::Integer, Value(static_cast<int32_t>(42)))),
        }))};
    auto testObject = makeShared<TestObject>(STRING_LITERAL("MyObject"), std::move(properties));

    auto marshalledValue = valueMarshallerRegistry->marshall(identifier, Value(testObject), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("aBoxedDouble", Value(1337.0))
                  .setMapValue("anOptionalBoxedBoolean", Value(true))
                  .setMapValue("anArray", Value(ValueArray::make({Value(static_cast<int32_t>(42))}))),
              Value(marshalledValue.getTypedObjectRef()->toValueMap(true)));
}

TEST(ValueMarshallerRegistry, supportsMap) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("c 'Response'{'headers': m<s, s?>}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object = Value().setMapValue("headers",
                                      Value()
                                          .setMapValue("Content-Encoding", Value(STRING_LITERAL("gzip")))
                                          .setMapValue("Content-Type", Value(STRING_LITERAL("text/html"))));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("className", Value(STRING_LITERAL("Response")))
                  .setMapValue("properties",
                               Value(ValueArray::make({Value()
                                                           .setMapValue("keys",
                                                                        Value(ValueArray::make({
                                                                            Value(STRING_LITERAL("Content-Encoding")),
                                                                            Value(STRING_LITERAL("Content-Type")),
                                                                        })))
                                                           .setMapValue("values",
                                                                        Value(ValueArray::make({
                                                                            Value(STRING_LITERAL("gzip")),
                                                                            Value(STRING_LITERAL("text/html")),
                                                                        })))}))),
              flattenValue(result));

    // Now attempt marshalling

    auto testMap = makeShared<TestMap>(3);
    testMap->getMap()[Value(STRING_LITERAL("Content-Type"))] = Value(STRING_LITERAL("application/javascript"));
    testMap->getMap()[Value(STRING_LITERAL("Server"))] = Value(STRING_LITERAL("Apache"));
    testMap->getMap()[Value(STRING_LITERAL("Vary"))] = Value(STRING_LITERAL("Cookie"));

    std::initializer_list<Value> properties = {Value(testMap)};
    auto response = makeShared<TestObject>(STRING_LITERAL("Response"), std::move(properties));

    auto marshalledValue = valueMarshallerRegistry->marshall(identifier, Value(response), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(
        Value().setMapValue("headers",
                            Value()
                                .setMapValue("Content-Type", Value(StaticString::makeUTF8("application/javascript")))
                                .setMapValue("Server", Value(StaticString::makeUTF8("Apache")))
                                .setMapValue("Vary", Value(StaticString::makeUTF8("Cookie")))),
        Value(marshalledValue.getTypedObjectRef()->toValueMap(true)));
}

TEST(ValueMarshallerRegistry, supportsEnum) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("e<u> 'WeirdEnum'{'ok': 200, 'not_ok': 'What the hell', 'divine': 42}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object = Value(static_cast<int32_t>(200));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("enumName", Value(STRING_LITERAL("WeirdEnum")))
                  .setMapValue("enumCase", Value(static_cast<int32_t>(0)))
                  .setMapValue("boxed", Value(false)),
              flattenValue(result));

    object = Value(static_cast<int32_t>(42));

    result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("enumName", Value(STRING_LITERAL("WeirdEnum")))
                  .setMapValue("enumCase", Value(static_cast<int32_t>(2)))
                  .setMapValue("boxed", Value(false)),
              flattenValue(result));

    object = Value(STRING_LITERAL("What the hell"));

    result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("enumName", Value(STRING_LITERAL("WeirdEnum")))
                  .setMapValue("enumCase", Value(static_cast<int32_t>(1)))
                  .setMapValue("boxed", Value(false)),
              flattenValue(result));

    // Now test marshalling

    auto marshalledValue = valueMarshallerRegistry->marshall(
        identifier, Value(makeShared<TestEnum>(STRING_LITERAL("WeirdEnum"), 0, false)), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value(static_cast<int32_t>(200)), marshalledValue);

    marshalledValue = valueMarshallerRegistry->marshall(
        identifier, Value(makeShared<TestEnum>(STRING_LITERAL("WeirdEnum"), 1, false)), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value(STRING_LITERAL("What the hell")), marshalledValue);

    marshalledValue = valueMarshallerRegistry->marshall(
        identifier, Value(makeShared<TestEnum>(STRING_LITERAL("WeirdEnum"), 2, false)), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value(static_cast<int32_t>(42)), marshalledValue);
}

struct TestProtocolHelper {
    Ref<ValueSchemaRegistry> schemaRegistry;
    Ref<TestValueMarshallerRegistry> valueMarshallerRegistry;
    ValueSchemaRegistrySchemaIdentifier schemaIdentifier;
    Ref<TestObject> sequence;
    Result<ValueSchema> schema;
    Ref<TestObjectProxy> foreignProxy;

    TestProtocolHelper() : schema(ValueSchema::parse("c+ 'Sequence'{'increment': f*(i): i}")) {
        if (!schema) {
            return;
        }

        SC_ASSERT(schema.value().getClass()->isInterface());

        schemaRegistry = makeShared<ValueSchemaRegistry>();
        valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

        schemaIdentifier = schemaRegistry->registerSchema(schema.value());

        createSequenceObject();
        createMarshalledSequenceObject();
    }

private:
    void createSequenceObject() {
        // This simulates an object implementing an interface/protocol that we can pass from platform
        auto increment =
            makeShared<TestFunctionWithCallable>([](const Value* parameters, size_t parametersSize) -> Result<Value> {
                if (parametersSize != 2) {
                    return Error("Expected 2 parameters");
                }
                auto self = parameters[0].getTypedRef<TestObject>();
                if (self == nullptr) {
                    return Error("self not passed in");
                }
                auto increment = parameters[1].toInt();

                auto currentValue = self->getProperty(0).toInt();
                auto newValue = Value(currentValue + increment);
                self->setProperty(0, newValue);

                return newValue;
            });

        std::initializer_list<Value> prototypeProperties = {Value(increment)};
        auto prototype = makeShared<TestObject>(STRING_LITERAL("SequencePrototype"), std::move(prototypeProperties));

        std::initializer_list<Value> sequenceProperties = {Value(0)};
        sequence = makeShared<TestObject>(STRING_LITERAL("Sequence"), std::move(sequenceProperties));
        sequence->setPrototype(prototype);
    }

    void createMarshalledSequenceObject() {
        // This simulates a bridged object which was passed back from another platform

        auto value = makeShared<Value>(static_cast<int32_t>(42));

        auto increment =
            makeShared<ValueFunctionWithCallable>([value](const ValueFunctionCallContext& callContext) -> Value {
                auto increment = callContext.getParameterAsInt(0);
                if (!callContext.getExceptionTracker()) {
                    return Value();
                }

                auto newValue = Value(value->toInt() + increment);
                *value = newValue;

                return newValue;
            });

        std::initializer_list<Value> properties = {Value(increment)};

        foreignProxy = makeShared<TestObjectProxy>(
            Value(), ValueTypedObject::make(schema.value().getClassRef(), std::move(properties)));
    }
};

TEST(ValueMarshallerRegistry, supportsMarshallingProtocol) {
    /**
     * This test is checking that we can convert a TestObject with a protocol schema
     * into a ValueTypedProxyObject with an associated TypedObject, allowing
     * another subsystem to interact with the TestObject
     * Conversion order: TestObject -> Value
     */

    TestProtocolHelper helper;
    ASSERT_TRUE(helper.schema) << helper.schema.description();

    SimpleExceptionTracker exceptionTracker;
    auto object =
        helper.valueMarshallerRegistry->marshall(helper.schemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_TRUE(object.isProxyObject());

    auto proxyObject = object.getTypedProxyObjectRef();

    ASSERT_TRUE(proxyObject != nullptr);

    // Now we try to interact with created typed object
    auto bridgedIncrement = proxyObject->getTypedObject()->getProperty(0);

    ASSERT_EQ(ValueType::Function, bridgedIncrement.getType());

    std::initializer_list<Value> parameters = {Value(static_cast<int32_t>(2))};
    auto result = (*bridgedIncrement.getFunction())(parameters.begin(), parameters.size());

    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(2, result.value().toInt());

    std::initializer_list<Value> newParameters = {Value(static_cast<int32_t>(1))};

    result = (*bridgedIncrement.getFunction())(newParameters.begin(), newParameters.size());

    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(3, result.value().toInt());

    // should have two references: one held in the proxy object, one in the helper
    ASSERT_EQ(static_cast<long>(2), helper.sequence.use_count());

    proxyObject = nullptr;
    object = Value();

    // clearing the proxy object should clear the reference on the sequence
    ASSERT_EQ(static_cast<long>(1), helper.sequence.use_count());
}

TEST(ValueMarshallerRegistry, marshallingProtocolTwiceShouldReturnSameProxyInstance) {
    /**
     * This test is checking that when converting a TestObject with a protocol schema,
     * we get the same ValueTypedProxyObject instance as long as the proxy stays alive.
     * Conversion order: TestObject -> Value
     */

    TestProtocolHelper helper;
    ASSERT_TRUE(helper.schema) << helper.schema.description();

    SimpleExceptionTracker exceptionTracker;
    auto object =
        helper.valueMarshallerRegistry->marshall(helper.schemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto proxyObject = object.getTypedProxyObjectRef();
    ASSERT_TRUE(proxyObject != nullptr);

    object =
        helper.valueMarshallerRegistry->marshall(helper.schemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto newProxyObject = object.getTypedProxyObjectRef();
    ASSERT_TRUE(newProxyObject != nullptr);

    ASSERT_EQ(proxyObject, newProxyObject);

    auto weakProxy = proxyObject.toWeak();

    proxyObject = nullptr;
    newProxyObject = nullptr;
    object = Value();

    ASSERT_TRUE(weakProxy.expired());

    object =
        helper.valueMarshallerRegistry->marshall(helper.schemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    // We should be able to get another proxy object when it becomes deallocated.
    newProxyObject = object.getTypedProxyObjectRef();
    ASSERT_TRUE(newProxyObject != nullptr);
}

TEST(ValueMarshallerRegistry, canMarshallOneInstanceForMultipleProtocols) {
    TestProtocolHelper helper;
    ASSERT_TRUE(helper.schema) << helper.schema.description();

    auto secondSchema = ValueSchema::parse("c+ 'AnotherSequence'{'increment': f*(i): i}");
    ASSERT_TRUE(secondSchema) << secondSchema.description();

    auto secondSchemaIdentifier = helper.schemaRegistry->registerSchema(secondSchema.value());

    SimpleExceptionTracker exceptionTracker;
    auto object =
        helper.valueMarshallerRegistry->marshall(helper.schemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto proxyObject = object.getTypedProxyObjectRef();
    ASSERT_TRUE(proxyObject != nullptr);

    // Marshall using the separate schema identifier
    object = helper.valueMarshallerRegistry->marshall(secondSchemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto secondProxyObject = object.getTypedProxyObjectRef();
    ASSERT_TRUE(secondProxyObject != nullptr);

    // Proxy objects should be different as they come from a different schema
    ASSERT_NE(proxyObject, secondProxyObject);
}

TEST(ValueMarshallerRegistry, unmarshallingPreviouslyMarshalledProtocolShouldReturnUnderlyingInstance) {
    /**
     * This test is checking that when converting a TestObject with a protocol schema,
     * and later converting back the ValueTypedProxyObject, we get the original TestObject
     * Conversion order: TestObject -> Value -> TestObject
     */

    TestProtocolHelper helper;
    ASSERT_TRUE(helper.schema) << helper.schema.description();

    SimpleExceptionTracker exceptionTracker;
    auto object =
        helper.valueMarshallerRegistry->marshall(helper.schemaIdentifier, Value(helper.sequence), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto instance = helper.valueMarshallerRegistry->unmarshall(helper.schemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto sequence = instance.getTypedRef<TestObject>();

    ASSERT_EQ(helper.sequence, sequence);
}

TEST(ValueMarshallerRegistry, supportsUnmarshallingProtocol) {
    /**
     * This test is checking that when we can convert an externally provided ValueTypedProxyObject into
     * a TestObject, and that we can interact with that TestObject.
     * Conversion order: Value -> TestObject
     */

    TestProtocolHelper helper;
    ASSERT_TRUE(helper.schema) << helper.schema.description();

    SimpleExceptionTracker exceptionTracker;
    auto object = Value(helper.foreignProxy);

    auto instance = helper.valueMarshallerRegistry->unmarshall(helper.schemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto sequence = instance.getTypedRef<TestObject>();
    ASSERT_TRUE(sequence != nullptr);
    ASSERT_NE(helper.sequence, sequence);

    auto bridgedIncrement = sequence->getProperty(0);
    ASSERT_EQ(ValueType::ValdiObject, bridgedIncrement.getType());

    auto bridgedIncrementFunction = bridgedIncrement.getTypedRef<TestFunction>();

    ASSERT_TRUE(bridgedIncrementFunction != nullptr);

    // Attempt to call the function, which should bridge to our foreign proxy
    auto parameter = Value(static_cast<int32_t>(1));
    auto callResult = bridgedIncrementFunction->call(&parameter, 1);

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(Value(static_cast<int32_t>(43)), callResult.value());

    parameter = Value(static_cast<int32_t>(3));
    callResult = bridgedIncrementFunction->call(&parameter, 1);

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(Value(static_cast<int32_t>(46)), callResult.value());

    object = Value();

    // We should have two references of the foreign proxy: one in the helper, one in our unmarshalled ValueTypedObject
    ASSERT_EQ(static_cast<long>(2), helper.foreignProxy.use_count());

    // Clearing our object should clear the proxy reference
    instance = Value();
    sequence = nullptr;
    bridgedIncrement = Value();
    bridgedIncrementFunction = nullptr;

    ASSERT_EQ(static_cast<long>(1), helper.foreignProxy.use_count());
}

TEST(ValueMarshallerRegistry, unmarshallingProtocolTwiceShouldReturnSameInstance) {
    /**
     * This test is checking that when converting an externally provided ValueTypedProxyObject twice,
     * we get the same TestObject instance.
     * Conversion order: Value -> TestObject
     */

    TestProtocolHelper helper;
    ASSERT_TRUE(helper.schema) << helper.schema.description();

    SimpleExceptionTracker exceptionTracker;
    auto object = Value(helper.foreignProxy);

    auto instance = helper.valueMarshallerRegistry->unmarshall(helper.schemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto sequence = instance.getTypedRef<TestObject>();
    ASSERT_TRUE(sequence != nullptr);

    instance = helper.valueMarshallerRegistry->unmarshall(helper.schemaIdentifier, object, exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto newSequence = instance.getTypedRef<TestObject>();
    ASSERT_TRUE(newSequence != nullptr);

    ASSERT_EQ(sequence, newSequence);

    instance = Value();
    newSequence = nullptr;
    object = Value();

    ASSERT_EQ(static_cast<long>(1), sequence.use_count());
}

TEST(ValueMarshallerRegistry, supportsMarshallingMap) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("c 'MyObject'{'aMap': %<s, d>}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;

    // Test unmarshalling: Create a Value with ES6 Map data and convert to TestObject
    auto es6Map = makeShared<ES6Map>();
    es6Map->entries.push_back(Value(STRING_LITERAL("key1")));
    es6Map->entries.push_back(Value(10.5));
    es6Map->entries.push_back(Value(STRING_LITERAL("key2")));
    es6Map->entries.push_back(Value(20.5));
    es6Map->entries.push_back(Value(STRING_LITERAL("key3")));
    es6Map->entries.push_back(Value(30.5));

    auto object = Value().setMapValue("aMap", Value(es6Map));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("className", Value(STRING_LITERAL("MyObject")))
                  .setMapValue("properties",
                               Value(ValueArray::make({Value()
                                                           .setMapValue("keys",
                                                                        Value(ValueArray::make({
                                                                            Value(STRING_LITERAL("key1")),
                                                                            Value(STRING_LITERAL("key2")),
                                                                            Value(STRING_LITERAL("key3")),
                                                                        })))
                                                           .setMapValue("values",
                                                                        Value(ValueArray::make({
                                                                            Value(10.5),
                                                                            Value(20.5),
                                                                            Value(30.5),
                                                                        })))}))),
              flattenValue(result));

    // Now attempt marshalling: Create TestObject with ES6 Map and convert back to Value
    auto testMap = makeShared<Test6ESMap>();
    testMap->getMap()[Value(STRING_LITERAL("foo"))] = Value(100.0);
    testMap->getMap()[Value(STRING_LITERAL("bar"))] = Value(200.0);
    testMap->getMap()[Value(STRING_LITERAL("baz"))] = Value(300.0);

    std::initializer_list<Value> properties = {Value(testMap)};
    auto testObject = makeShared<TestObject>(STRING_LITERAL("MyObject"), std::move(properties));

    auto marshalledValue = valueMarshallerRegistry->marshall(identifier, Value(testObject), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto marshalledMap = Value(marshalledValue.getTypedObjectRef()->toValueMap(true));
    auto aMapValue = marshalledMap.getMapValue("aMap");
    auto aMapRef = aMapValue.getTypedRef<ES6Map>();

    ASSERT_TRUE(aMapRef != nullptr);

    // ES6Map entries are stored as [key1, value1, key2, value2, ...]
    // FlatMap iterates in sorted order by key (alphabetically)
    ASSERT_EQ(static_cast<size_t>(6), aMapRef->entries.size());

    // Check first key-value pair: "bar" -> 200.0
    ASSERT_TRUE(aMapRef->entries[0].isString());
    ASSERT_EQ(STRING_LITERAL("bar"), aMapRef->entries[0].toStringBox());
    ASSERT_EQ(200.0, aMapRef->entries[1].toDouble());

    // Check second key-value pair: "baz" -> 300.0
    ASSERT_TRUE(aMapRef->entries[2].isString());
    ASSERT_EQ(STRING_LITERAL("baz"), aMapRef->entries[2].toStringBox());
    ASSERT_EQ(300.0, aMapRef->entries[3].toDouble());

    // Check third key-value pair: "foo" -> 100.0
    ASSERT_TRUE(aMapRef->entries[4].isString());
    ASSERT_EQ(STRING_LITERAL("foo"), aMapRef->entries[4].toStringBox());
    ASSERT_EQ(100.0, aMapRef->entries[5].toDouble());
}

TEST(ValueMarshallerRegistry, supportsMarshallingSet) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();
    auto valueMarshallerRegistry = makeShared<TestValueMarshallerRegistry>(schemaRegistry);

    auto schema = ValueSchema::parse("c 'MyObject'{'aSet': @<s>}");

    ASSERT_TRUE(schema) << schema.description();

    auto identifier = schemaRegistry->registerSchema(schema.value());

    SimpleExceptionTracker exceptionTracker;

    // Test unmarshalling: Create a Value with ES6 Set data and convert to TestObject
    auto es6Set = makeShared<ES6Set>();
    es6Set->entries.push_back(Value(STRING_LITERAL("apple")));
    es6Set->entries.push_back(Value(STRING_LITERAL("banana")));
    es6Set->entries.push_back(Value(STRING_LITERAL("cherry")));

    auto object = Value().setMapValue("aSet", Value(es6Set));

    auto result = valueMarshallerRegistry->unmarshall(identifier, object, exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    ASSERT_EQ(Value()
                  .setMapValue("className", Value(STRING_LITERAL("MyObject")))
                  .setMapValue("properties",
                               Value(ValueArray::make({Value(ValueArray::make({
                                   Value(STRING_LITERAL("apple")),
                                   Value(STRING_LITERAL("banana")),
                                   Value(STRING_LITERAL("cherry")),
                               }))}))),
              flattenValue(result));

    // Now attempt marshalling: Create TestObject with ES6 Set and convert back to Value
    auto testSet = makeShared<TestES6Set>();
    testSet->insert(Value(STRING_LITERAL("red")));
    testSet->insert(Value(STRING_LITERAL("green")));
    testSet->insert(Value(STRING_LITERAL("blue")));

    std::initializer_list<Value> properties = {Value(testSet)};
    auto testObject = makeShared<TestObject>(STRING_LITERAL("MyObject"), std::move(properties));

    auto marshalledValue = valueMarshallerRegistry->marshall(identifier, Value(testObject), exceptionTracker);

    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto marshalledMap = Value(marshalledValue.getTypedObjectRef()->toValueMap(true));
    auto aSetValue = marshalledMap.getMapValue("aSet");
    auto aSetRef = aSetValue.getTypedRef<ES6Set>();

    ASSERT_TRUE(aSetRef != nullptr);

    // ES6Set entries are stored as [value1, value2, value3, ...]
    // TestES6Set uses ValueArrayBuilder which maintains insertion order
    ASSERT_EQ(static_cast<size_t>(3), aSetRef->entries.size());

    // Check entries in insertion order: "red", "green", "blue"
    ASSERT_TRUE(aSetRef->entries[0].isString());
    ASSERT_EQ(STRING_LITERAL("red"), aSetRef->entries[0].toStringBox());

    ASSERT_TRUE(aSetRef->entries[1].isString());
    ASSERT_EQ(STRING_LITERAL("green"), aSetRef->entries[1].toStringBox());

    ASSERT_TRUE(aSetRef->entries[2].isString());
    ASSERT_EQ(STRING_LITERAL("blue"), aSetRef->entries[2].toStringBox());
}

// Regression test for PUSH-10349: a marshaller registry whose type resolver has no
// ValueSchemaRegistry (the JS-side registry) receives schemas as ValueSchemaReferences into a
// platform-owned registry. When the referenced entry still contains unresolved named type
// references — which happens whenever the platform side has not walked the entry yet, e.g.
// with lazy function return-marshalling — the entry must be resolved against the reference's
// owning registry instead of failing with "Cannot resolve schema from type references without
// a registry set".
TEST(ValueMarshallerRegistry, resolvesSchemaReferenceEntryAgainstOwningRegistry) {
    auto schemaRegistry = makeShared<ValueSchemaRegistry>();

    auto payloadSchema = ValueSchema::parse("c 'Payload'{'x': d}");
    ASSERT_TRUE(payloadSchema) << payloadSchema.description();
    schemaRegistry->registerSchema(payloadSchema.value());

    // The function parameter keeps an unresolved named reference inside the registered entry,
    // mirroring the G71.onForceTweakFailed shape from the crash.
    auto sessionSchema = ValueSchema::parse("c 'Session'{'onFail': f(r:'Payload'): v, 'name': s}");
    ASSERT_TRUE(sessionSchema) << sessionSchema.description();
    schemaRegistry->registerSchema(sessionSchema.value());

    auto valueMarshallerRegistry =
        makeShared<TestValueMarshallerRegistry>(schemaRegistry, nullptr, /*withResolverRegistry=*/false);

    auto sessionKey =
        ValueSchemaRegistryKey(ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("Session"))));
    auto sessionReference = schemaRegistry->getSchemaReferenceForTypeKey(sessionKey);
    ASSERT_TRUE(sessionReference != nullptr);

    SimpleExceptionTracker exceptionTracker;
    auto marshaller =
        valueMarshallerRegistry->getValueMarshaller(ValueSchema::schemaReference(sessionReference), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();
    ASSERT_TRUE(marshaller.valueMarshaller != nullptr);

    auto onFail = makeShared<ValueFunctionWithCallable>(
        [](const ValueFunctionCallContext& /*callContext*/) -> Value { return Value(); });
    auto object = Value().setMapValue("onFail", Value(onFail)).setMapValue("name", Value(STRING_LITERAL("hello")));

    auto result = marshaller.valueMarshaller->unmarshall(object, ReferenceInfoBuilder(), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto session = result.getTypedRef<TestObject>();
    ASSERT_TRUE(session != nullptr);
    ASSERT_EQ(Value(STRING_LITERAL("hello")), session->getProperty(1));
}

// End-to-end shape of the PUSH-10349 crash: with lazy function return-marshalling enabled, a
// JS-like registry (no registry on its type resolver) defers the return marshaller of
// Tool.createSession(); the first call resolves the return type Session — whose entry still
// contains a named reference to Payload — through the reference's owning registry.
TEST(ValueMarshallerRegistry, lazyReturnMarshallerResolvesThroughOwningRegistry) {
    lazyFunctionReturnMarshallerFlag().store(true);

    auto schemaRegistry = makeShared<ValueSchemaRegistry>();

    auto payloadSchema = ValueSchema::parse("c 'Payload'{'x': d}");
    ASSERT_TRUE(payloadSchema) << payloadSchema.description();
    schemaRegistry->registerSchema(payloadSchema.value());

    auto sessionSchema = ValueSchema::parse("c 'Session'{'onFail': f(r:'Payload'): v, 'name': s}");
    ASSERT_TRUE(sessionSchema) << sessionSchema.description();
    schemaRegistry->registerSchema(sessionSchema.value());

    auto toolSchema = ValueSchema::parse("c 'Tool'{'createSession': f(): r:'Session'}");
    ASSERT_TRUE(toolSchema) << toolSchema.description();
    schemaRegistry->registerSchema(toolSchema.value());

    auto valueMarshallerRegistry =
        makeShared<TestValueMarshallerRegistry>(schemaRegistry, nullptr, /*withResolverRegistry=*/false);

    auto toolKey =
        ValueSchemaRegistryKey(ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("Tool"))));
    auto toolReference = schemaRegistry->getSchemaReferenceForTypeKey(toolKey);
    ASSERT_TRUE(toolReference != nullptr);

    SimpleExceptionTracker exceptionTracker;
    auto marshaller =
        valueMarshallerRegistry->getValueMarshaller(ValueSchema::schemaReference(toolReference), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();
    ASSERT_TRUE(marshaller.valueMarshaller != nullptr);

    auto createSession =
        makeShared<ValueFunctionWithCallable>([](const ValueFunctionCallContext& /*callContext*/) -> Value {
            auto onFail = makeShared<ValueFunctionWithCallable>(
                [](const ValueFunctionCallContext& /*innerCallContext*/) -> Value { return Value(); });
            return Value().setMapValue("onFail", Value(onFail)).setMapValue("name", Value(STRING_LITERAL("session-1")));
        });
    auto toolObject = Value().setMapValue("createSession", Value(createSession));

    auto unmarshalledTool =
        marshaller.valueMarshaller->unmarshall(toolObject, ReferenceInfoBuilder(), exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError().toString();

    auto tool = unmarshalledTool.getTypedRef<TestObject>();
    ASSERT_TRUE(tool != nullptr);

    auto createSessionFunction = tool->getProperty(0).getTypedRef<TestFunction>();
    ASSERT_TRUE(createSessionFunction != nullptr);

    // First call triggers the lazy return-marshaller resolution; pre-fix this failed with
    // "Lazy ValueMarshaller failed to resolve return type ... without a registry set".
    auto callResult = createSessionFunction->call(nullptr, 0);
    ASSERT_TRUE(callResult) << callResult.description();

    auto session = callResult.value().getTypedRef<TestObject>();
    ASSERT_TRUE(session != nullptr);
    ASSERT_EQ(Value(STRING_LITERAL("session-1")), session->getProperty(1));

    lazyFunctionReturnMarshallerFlag().store(false);
}

} // namespace ValdiTest
