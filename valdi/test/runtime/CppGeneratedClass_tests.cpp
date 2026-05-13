#include "valdi_core/cpp/Marshalling/CppGeneratedClass.hpp"
#include "valdi_core/cpp/Marshalling/CppGeneratedGenericClass.hpp"
#include "valdi_core/cpp/Marshalling/CppMarshaller.hpp"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Utils/Future.hpp"
#include "valdi_core/cpp/Utils/Promise.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include "valdi_modules/test/test.hpp"
#include <cstddef>
#include <future>
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

struct MyCard : public CppGeneratedModel {
    StringBox title;
    std::optional<StringBox> subtitle;
    double width = 0;
    double height = 0;
    std::optional<bool> selected;
    std::optional<Function<bool(StringBox)>> onTap;

    MyCard() : CppGeneratedModel(getRegisteredClass()) {}
    ~MyCard() = default;

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass = CppGeneratedClass::registerSchema(
            "c 'MyCard'{'title': s, 'subtitle': s?, 'width': d, 'height': d, 'selected': b?, 'onTap': f?(s):b}");
        return kRegisteredClass;
    }

    static void marshall(ExceptionTracker& exceptionTracker, const MyCard& value, Value& out) {
        CppMarshaller::marshallTypedObject(exceptionTracker,
                                           *getRegisteredClass(),
                                           out,
                                           value.title,
                                           value.subtitle,
                                           value.width,
                                           value.height,
                                           value.selected,
                                           value.onTap);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, MyCard& out) {
        CppMarshaller::unmarshallTypedObject(exceptionTracker,
                                             *getRegisteredClass(),
                                             value,
                                             out.title,
                                             out.subtitle,
                                             out.width,
                                             out.height,
                                             out.selected,
                                             out.onTap);
    }
};

struct MyCardSection : public CppGeneratedModel {
    std::vector<MyCard> cards;
    std::vector<int64_t> ids;

    MyCardSection() : CppGeneratedModel(getRegisteredClass()) {}
    ~MyCardSection() = default;

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass =
            CppGeneratedClass::registerSchema("c 'MyCardSection'{'cards': a<r:'[0]'>, 'ids': a<l>}",
                                              []() -> TypeReferencesVec { return {MyCard::getRegisteredClass()}; });
        return kRegisteredClass;
    }

    static void marshall(ExceptionTracker& exceptionTracker, const MyCardSection& value, Value& out) {
        CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out, value.cards, value.ids);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, MyCardSection& out) {
        CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value, out.cards, out.ids);
    }
};

struct ICalculator : public CppGeneratedInterface {
    virtual double add(double left, double right) = 0;

    ICalculator() : CppGeneratedInterface(getRegisteredClass()) {}

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass = CppGeneratedClass::registerSchema("c+ 'MyCalculator'{'add': f(d, d): d}");
        return kRegisteredClass;
    }

    static void marshall(ExceptionTracker& exceptionTracker,
                         CppObjectStore* objectStore,
                         const Ref<ICalculator>& value,
                         Value& out);

    static void unmarshall(ExceptionTracker& exceptionTracker,
                           CppObjectStore* objectStore,
                           const Value& value,
                           Ref<ICalculator>& out);
};

struct ICalculatorProxy : public ICalculator {
    Function<double(double, double)> addFn;

    ICalculatorProxy() = default;
    ~ICalculatorProxy() override = default;

    double add(double left, double right) final {
        return addFn(left, right);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Ref<ICalculatorProxy>& out) {
        out = makeShared<ICalculatorProxy>();
        auto& self = *out;
        CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value, self.addFn);
    }
};

void ICalculator::marshall(ExceptionTracker& exceptionTracker,
                           CppObjectStore* objectStore,
                           const Ref<ICalculator>& value,
                           Value& out) {
    CppMarshaller::marshallProxyObject(exceptionTracker, objectStore, *getRegisteredClass(), *value, out, [&]() {
        CppMarshaller::marshallTypedObject(exceptionTracker,
                                           *getRegisteredClass(),
                                           out,
                                           CppMarshaller::methodToFunction<ICalculator, &ICalculator::add>(value));
    });
}

void ICalculator::unmarshall(ExceptionTracker& exceptionTracker,
                             CppObjectStore* objectStore,
                             const Value& value,
                             Ref<ICalculator>& out) {
    CppMarshaller::unmarshallProxyObject<ICalculatorProxy>(
        exceptionTracker, objectStore, *getRegisteredClass(), value, out);
}

template<typename T>
class GenericContainer : public CppGeneratedModel {
public:
    GenericContainer() : CppGeneratedModel(getRegisteredClass()) {}
    explicit GenericContainer(T value) : CppGeneratedModel(getRegisteredClass()), _value(std::move(value)) {}
    ~GenericContainer() = default;

    const T& getValue() const {
        return _value;
    }

    void setValue(T value) {
        _value = std::move(value);
    }

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass = CppGeneratedClass::registerGenericSchema(
            "c 'GenericContainer'{'value': r:0}", CppGeneratedGenericClass::makeTypeArgumentsCallback<T>());
        return kRegisteredClass;
    }

    static void marshall(ExceptionTracker& exceptionTracker, const GenericContainer<T>& value, Value& out) {
        CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out, value._value);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, GenericContainer<T>& out) {
        CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value, out._value);
    }

private:
    T _value = {};
};

class GenericInt : public CppGeneratedClass {
public:
    GenericInt() : CppGeneratedClass(getRegisteredClass()) {}
    ~GenericInt() = default;

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass =
            CppGeneratedClass::registerSchema("c 'GenericInt'{'container': g:'[0]'<i>}", []() -> TypeReferencesVec {
                return {GenericContainer<int32_t>::getRegisteredClass()};
            });
        return kRegisteredClass;
    }

    const GenericContainer<int32_t>& getContainer() const {
        return _container;
    }

    void setContainer(GenericContainer<int32_t> container) {
        _container = std::move(container);
    }

    static void marshall(ExceptionTracker& exceptionTracker, const GenericInt& value, Value& out) {
        CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out, value._container);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, GenericInt& out) {
        CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value, out._container);
    }

private:
    GenericContainer<int32_t> _container;
};

class GenericObject : public CppGeneratedClass {
public:
    GenericObject() : CppGeneratedClass(getRegisteredClass()) {}
    ~GenericObject() = default;

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass = CppGeneratedClass::registerSchema(
            "c 'GenericObject'{'container': g:'[0]'<r:'[1]'>}", []() -> TypeReferencesVec {
                return {GenericContainer<MyCard>::getRegisteredClass(), MyCard::getRegisteredClass()};
            });
        return kRegisteredClass;
    }

    const GenericContainer<MyCard>& getContainer() const {
        return _container;
    }

    void setContainer(GenericContainer<MyCard> container) {
        _container = std::move(container);
    }

    static void marshall(ExceptionTracker& exceptionTracker, const GenericObject& value, Value& out) {
        CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out, value._container);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, GenericObject& out) {
        CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value, out._container);
    }

private:
    GenericContainer<MyCard> _container;
};

class FutureContainer : public CppGeneratedClass {
public:
    FutureContainer() : CppGeneratedClass(getRegisteredClass()) {}
    ~FutureContainer() = default;

    const Future<MyCard>& getFuture() const {
        return _future;
    }

    void setFuture(Future<MyCard> future) {
        _future = std::move(future);
    }

    static RegisteredCppGeneratedClass* getRegisteredClass() {
        static auto* kRegisteredClass =
            CppGeneratedClass::registerSchema("c 'FutureContainer'{'future': p<r:'[0]'>}",
                                              []() -> TypeReferencesVec { return {MyCard::getRegisteredClass()}; });
        return kRegisteredClass;
    }

    static void marshall(ExceptionTracker& exceptionTracker, const FutureContainer& value, Value& out) {
        CppMarshaller::marshallTypedObject(exceptionTracker, *getRegisteredClass(), out, value._future);
    }

    static void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, FutureContainer& out) {
        CppMarshaller::unmarshallTypedObject(exceptionTracker, *getRegisteredClass(), value, out._future);
    }

private:
    Future<MyCard> _future;
};

struct MyTestCalculator : public ICalculator {
    MyTestCalculator() = default;
    ~MyTestCalculator() override = default;

    double add(double left, double right) final {
        return left + right;
    }
};

class TestProxyObject : public ValueTypedProxyObject {
public:
    explicit TestProxyObject(const Ref<ValueTypedObject>& typedObject) : ValueTypedProxyObject(typedObject) {}
    ~TestProxyObject() override = default;

    std::string_view getType() const final {
        return "Test Proxy";
    }
};

TEST(CppGeneratedClass, canMarshallObject) {
    MyCard object;

    object.title = STRING_LITERAL("Hello World");
    object.width = 42;
    object.height = 100;
    object.selected = {true};

    SimpleExceptionTracker exceptionTracker;
    Value out;
    MyCard::marshall(exceptionTracker, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto typedObject = out.getTypedObjectRef();
    ASSERT_TRUE(typedObject != nullptr);

    ASSERT_EQ(Value()
                  .setMapValue("title", Value(STRING_LITERAL("Hello World")))
                  .setMapValue("subtitle", Value::undefined())
                  .setMapValue("width", Value(42.0))
                  .setMapValue("height", Value(100.0))
                  .setMapValue("selected", Value(true))
                  .setMapValue("onTap", Value::undefined()),
              Value(typedObject->toValueMap()));
}

TEST(CppGeneratedClass, canUnmarshallObjectFromMap) {
    MyCard out;
    SimpleExceptionTracker exceptionTracker;
    MyCard::unmarshall(exceptionTracker,
                       Value()
                           .setMapValue("title", Value(STRING_LITERAL("Hello World")))
                           .setMapValue("subtitle", Value(STRING_LITERAL("Nice")))
                           .setMapValue("width", Value(42.0))
                           .setMapValue("height", Value(100.0))
                           .setMapValue("selected", Value(true))
                           .setMapValue("onTap", Value::undefined()),
                       out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(STRING_LITERAL("Hello World"), out.title);
    ASSERT_EQ(std::make_optional(STRING_LITERAL("Nice")), out.subtitle);
    ASSERT_EQ(42.0, out.width);
    ASSERT_EQ(100.0, out.height);
    ASSERT_EQ(std::make_optional(true), out.selected);
    ASSERT_EQ(std::nullopt, out.onTap);
}

TEST(CppGeneratedClass, canUnmarshallObjectFromTypedObject) {
    SimpleExceptionTracker exceptionTracker;
    auto classSchema = MyCard::getRegisteredClass()->getResolvedClassSchema(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto typedObject = ValueTypedObject::make(classSchema);
    typedObject->setProperty(0, Value(STRING_LITERAL("Hello World")));
    typedObject->setProperty(2, Value(42.0));
    typedObject->setProperty(3, Value(100.0));
    typedObject->setProperty(4, Value(false));

    MyCard out;
    MyCard::unmarshall(exceptionTracker, Value(typedObject), out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(STRING_LITERAL("Hello World"), out.title);
    ASSERT_EQ(std::nullopt, out.subtitle);
    ASSERT_EQ(42.0, out.width);
    ASSERT_EQ(100.0, out.height);
    ASSERT_EQ(std::make_optional(false), out.selected);
    ASSERT_EQ(std::nullopt, out.onTap);
}

TEST(CppGeneratedClass, canMarshallFunction) {
    MyCard object;

    size_t callCount = 0;
    object.onTap = {[&](StringBox name) -> bool {
        callCount++;
        if (name == "activate") {
            return true;
        } else {
            return false;
        }
    }};

    SimpleExceptionTracker exceptionTracker;
    Value out;
    MyCard::marshall(exceptionTracker, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto typedObject = out.getTypedObjectRef();
    ASSERT_TRUE(typedObject != nullptr);

    auto onTap =
        typedObject->getPropertyForName(STRING_LITERAL("onTap")).checkedTo<Ref<ValueFunction>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(static_cast<size_t>(0), callCount);

    auto result = (*onTap)({Value(STRING_LITERAL("great"))});

    ASSERT_EQ(static_cast<size_t>(1), callCount);
    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(false), result.value());

    result = (*onTap)({Value(STRING_LITERAL("activate"))});

    ASSERT_EQ(static_cast<size_t>(2), callCount);
    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(true), result.value());
}

TEST(CppGeneratedClass, canUnmarshallFunction) {
    size_t callCount = 0;
    auto onTap = makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext& callContext) -> Value {
        auto str = callContext.getParameterAsString(0);
        callCount++;
        if (str == "activate") {
            return Value(true);
        } else {
            return Value(false);
        }
    });

    MyCard out;
    SimpleExceptionTracker exceptionTracker;
    MyCard::unmarshall(exceptionTracker,
                       Value()
                           .setMapValue("title", Value(STRING_LITERAL("Hello World")))
                           .setMapValue("width", Value(42.0))
                           .setMapValue("height", Value(100.0))
                           .setMapValue("onTap", Value(onTap)),
                       out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(out.onTap);

    ASSERT_EQ(static_cast<size_t>(0), callCount);

    auto result = out.onTap.value()(STRING_LITERAL("great"));

    ASSERT_EQ(static_cast<size_t>(1), callCount);
    ASSERT_EQ(false, result);

    result = out.onTap.value()(STRING_LITERAL("activate"));

    ASSERT_EQ(static_cast<size_t>(2), callCount);
    ASSERT_EQ(true, result);
}

TEST(CppGeneratedClass, canMarshallVector) {
    MyCardSection section;

    MyCard item1;
    item1.title = STRING_LITERAL("Item 1");
    item1.width = 1;
    item1.height = 1;

    MyCard item2;
    item2.title = STRING_LITERAL("Item 2");
    item2.width = 2;
    item2.height = 2;

    section.cards.emplace_back(item1);
    section.cards.emplace_back(item2);

    section.ids.emplace_back(42);
    section.ids.emplace_back(43);

    SimpleExceptionTracker exceptionTracker;
    Value out;
    MyCardSection::marshall(exceptionTracker, section, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto typedObject = out.getTypedObjectRef();
    ASSERT_TRUE(typedObject != nullptr);

    auto cards = typedObject->getProperty(0).checkedTo<Ref<ValueArray>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(static_cast<size_t>(2), cards->size());

    auto convertedItem1 = (*cards)[0].checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto convertedItem2 = (*cards)[1].checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(Value()
                  .setMapValue("title", Value(STRING_LITERAL("Item 1")))
                  .setMapValue("width", Value(1.0))
                  .setMapValue("height", Value(1.0)),
              Value(convertedItem1->toValueMap(true)));

    ASSERT_EQ(Value()
                  .setMapValue("title", Value(STRING_LITERAL("Item 2")))
                  .setMapValue("width", Value(2.0))
                  .setMapValue("height", Value(2.0)),
              Value(convertedItem2->toValueMap(true)));

    ASSERT_EQ(Value(ValueArray::make({Value(42), Value(43)})), typedObject->getProperty(1));
}

TEST(CppGeneratedClass, canUnmarshallVector) {
    auto value =
        Value()
            .setMapValue("cards",
                         Value(ValueArray::make({Value()
                                                     .setMapValue("title", Value(STRING_LITERAL("Title 1")))
                                                     .setMapValue("width", Value(1.0))
                                                     .setMapValue("height", Value(1.0)),
                                                 Value()
                                                     .setMapValue("title", Value(STRING_LITERAL("Title 2")))
                                                     .setMapValue("subtitle", Value(STRING_LITERAL("A subtitle")))
                                                     .setMapValue("width", Value(2.0))
                                                     .setMapValue("height", Value(2.0))})))
            .setMapValue("ids", Value(ValueArray::make({Value(7.0)})));

    SimpleExceptionTracker exceptionTracker;
    MyCardSection out;
    MyCardSection::unmarshall(exceptionTracker, value, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(static_cast<size_t>(2), out.cards.size());

    auto card1 = out.cards[0];
    ASSERT_EQ(STRING_LITERAL("Title 1"), card1.title);
    ASSERT_EQ(std::nullopt, card1.subtitle);
    ASSERT_EQ(1.0, card1.width);
    ASSERT_EQ(1.0, card1.height);
    ASSERT_EQ(std::nullopt, card1.selected);
    ASSERT_EQ(std::nullopt, card1.onTap);

    auto card2 = out.cards[1];
    ASSERT_EQ(STRING_LITERAL("Title 2"), card2.title);
    ASSERT_EQ(std::make_optional(STRING_LITERAL("A subtitle")), card2.subtitle);
    ASSERT_EQ(2.0, card2.width);
    ASSERT_EQ(2.0, card2.height);
    ASSERT_EQ(std::nullopt, card2.selected);
    ASSERT_EQ(std::nullopt, card2.onTap);

    ASSERT_EQ(std::vector<int64_t>({7}), out.ids);
}

TEST(CppGeneratedClass, canMarshallInterface) {
    CppObjectStore objectStore;

    auto object = makeShared<MyTestCalculator>();

    ASSERT_EQ(1, object.use_count());

    SimpleExceptionTracker exceptionTracker;
    Value out;
    ICalculator::marshall(exceptionTracker, &objectStore, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(out.isProxyObject());

    ASSERT_EQ(2, object.use_count());

    auto addValue = out.getProxyObject()->getTypedObject()->getPropertyForName(STRING_LITERAL("add"));

    auto addFunction = addValue.checkedTo<Ref<ValueFunction>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto result = (*addFunction)({Value(7.0), Value(2.0)});

    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(Value(9.0), result.value());

    ASSERT_EQ(2, object.use_count());

    auto lock = objectStore.lock();
    auto instance = objectStore.getObjectProxyForId(out.getProxyObject()->getId());

    ASSERT_EQ(object.get(), instance.get());

    ASSERT_EQ(3, object.use_count());

    instance = nullptr;
    out = Value();

    ASSERT_EQ(1, object.use_count());
}

TEST(CppGeneratedClass, marshallingInterfaceTwiceShouldReturnSameProxy) {
    CppObjectStore objectStore;

    auto object = makeShared<MyTestCalculator>();

    SimpleExceptionTracker exceptionTracker;
    Value out;
    ICalculator::marshall(exceptionTracker, &objectStore, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(out.isProxyObject());
    auto proxy1 = out.getTypedProxyObjectRef();

    out = Value();
    ICalculator::marshall(exceptionTracker, &objectStore, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(out.isProxyObject());
    auto proxy2 = out.getTypedProxyObjectRef();

    ASSERT_EQ(proxy1, proxy2);

    out = Value();
    proxy2 = nullptr;

    // Proxy should be retained weekly
    ASSERT_EQ(1, proxy1.use_count());
    proxy1 = nullptr;

    ICalculator::marshall(exceptionTracker, &objectStore, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(out.isProxyObject());
    proxy1 = out.getTypedProxyObjectRef();
    out = Value();

    ASSERT_EQ(1, proxy1.use_count());
}

TEST(CppGeneratedClass, unmarshallingPreviouslyMarshalledInterfaceShouldReturnSameInstance) {
    CppObjectStore objectStore;

    auto object = makeShared<MyTestCalculator>();

    SimpleExceptionTracker exceptionTracker;
    Value out;
    ICalculator::marshall(exceptionTracker, &objectStore, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();
    ASSERT_TRUE(out.isProxyObject());

    Ref<ICalculator> outCalculator;
    ICalculator::unmarshall(exceptionTracker, &objectStore, out, outCalculator);

    ASSERT_EQ(object, outCalculator);
}

TEST(CppGeneratedClass, callingFunctionOnDeallocatedMarshalledInterfaceShouldFail) {
    CppObjectStore objectStore;

    auto object = makeShared<MyTestCalculator>();

    SimpleExceptionTracker exceptionTracker;
    Value out;
    ICalculator::marshall(exceptionTracker, &objectStore, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(out.isProxyObject());

    auto addValue = out.getProxyObject()->getTypedObject()->getPropertyForName(STRING_LITERAL("add"));

    auto addFunction = addValue.checkedTo<Ref<ValueFunction>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    out = Value();
    ASSERT_EQ(1, object.use_count());
    object = nullptr;

    auto result = (*addFunction)({Value(7.0), Value(2.0)});

    ASSERT_FALSE(result) << result.description();
    ASSERT_TRUE(result.error().toStringBox().contains("object was deallocated"));
}

TEST(CppGeneratedClass, canUnmarshallInterface) {
    CppObjectStore objectStore;

    SimpleExceptionTracker exceptionTracker;
    auto schema = ICalculator::getRegisteredClass()->getResolvedClassSchema(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    size_t callCount = 0;
    auto addFunction = makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext& callContext) -> Value {
        callCount++;
        auto left = callContext.getParameterAsDouble(0);
        auto right = callContext.getParameterAsDouble(1);

        return Value(left + right);
    });

    auto typedObject = ValueTypedObject::make(schema);
    typedObject->setPropertyForName(STRING_LITERAL("add"), Value(addFunction));

    auto testProxyObject = makeShared<TestProxyObject>(typedObject);

    Ref<ICalculator> out;
    ICalculator::unmarshall(exceptionTracker, &objectStore, Value(testProxyObject), out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    // We should have gotten a proxy
    ASSERT_TRUE(castOrNull<ICalculatorProxy>(out) != nullptr);

    auto result = out->add(13, 57);

    ASSERT_EQ(70.0, result);
    ASSERT_EQ(static_cast<size_t>(1), callCount);

    // If we marshall the proxy, we should get the testProxyObject back

    Value outValue;
    ICalculator::marshall(exceptionTracker, &objectStore, out, outValue);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_TRUE(outValue.isProxyObject());
    ASSERT_EQ(testProxyObject, outValue.getTypedProxyObjectRef());
}

TEST(CppGeneratedClass, canMarshallGenericInt) {
    GenericInt object;

    object.setContainer(GenericContainer<int32_t>(42));

    SimpleExceptionTracker exceptionTracker;
    Value out;
    GenericInt::marshall(exceptionTracker, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto typedObject = out.getTypedObjectRef();
    ASSERT_TRUE(typedObject != nullptr);

    auto container =
        typedObject->getPropertyForName(STRING_LITERAL("container")).checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto value = container->getPropertyForName(STRING_LITERAL("value")).toInt();

    ASSERT_EQ(42, value);

    auto unresolvedGenericSchema = ValueSchemaRegistry::sharedInstance()->getSchemaForTypeKey(ValueSchemaRegistryKey(
        ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("GenericContainer")))));

    ASSERT_TRUE(unresolvedGenericSchema.has_value());
    ASSERT_EQ("class 'GenericContainer'{'value': ref:0}", unresolvedGenericSchema.value().toString());

    auto resolvedGenericSchema = ValueSchemaRegistry::sharedInstance()->getSchemaForTypeKey(
        ValueSchemaRegistryKey(ValueSchema::genericTypeReference(
            ValueSchemaTypeReference::named(STRING_LITERAL("GenericContainer")), {ValueSchema::integer()})));
    ASSERT_TRUE(resolvedGenericSchema.has_value());
    ASSERT_EQ("class 'GenericContainer'{'value': int}", resolvedGenericSchema.value().toString());
}

TEST(CppGeneratedClass, canUnmarshallGenericInt) {
    auto value = Value().setMapValue("container", Value(Value().setMapValue("value", Value(42))));

    SimpleExceptionTracker exceptionTracker;
    GenericInt out;
    GenericInt::unmarshall(exceptionTracker, value, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ(42, out.getContainer().getValue());
}

TEST(CppGeneratedClass, canMarshallGenericObject) {
    GenericObject object;

    MyCard card;
    card.title = STRING_LITERAL("Hello World");
    card.width = 42;
    card.height = 100;
    card.selected = {true};

    object.setContainer(GenericContainer<MyCard>(std::move(card)));

    SimpleExceptionTracker exceptionTracker;
    Value out;
    GenericObject::marshall(exceptionTracker, object, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto typedObject = out.getTypedObjectRef();
    ASSERT_TRUE(typedObject != nullptr);

    auto container =
        typedObject->getPropertyForName(STRING_LITERAL("container")).checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto value =
        container->getPropertyForName(STRING_LITERAL("value")).checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    ASSERT_EQ("Hello World", value->getPropertyForName(STRING_LITERAL("title")).toString());
    ASSERT_EQ(42, value->getPropertyForName(STRING_LITERAL("width")).toInt());
    ASSERT_EQ(100, value->getPropertyForName(STRING_LITERAL("height")).toInt());
    ASSERT_EQ(true, value->getPropertyForName(STRING_LITERAL("selected")).toBool());

    auto unresolvedGenericSchema = ValueSchemaRegistry::sharedInstance()->getSchemaForTypeKey(ValueSchemaRegistryKey(
        ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("GenericContainer")))));

    ASSERT_TRUE(unresolvedGenericSchema.has_value());
    ASSERT_EQ("class 'GenericContainer'{'value': ref:0}", unresolvedGenericSchema.value().toString());

    auto resolvedGenericSchema = ValueSchemaRegistry::sharedInstance()->getSchemaForTypeKey(
        ValueSchemaRegistryKey(ValueSchema::genericTypeReference(
            ValueSchemaTypeReference::named(STRING_LITERAL("GenericContainer")),
            {ValueSchema::typeReference(ValueSchemaTypeReference::named(STRING_LITERAL("MyCard")))})));
    ASSERT_TRUE(resolvedGenericSchema.has_value());
    ASSERT_EQ("class 'GenericContainer'{'value': link:ref:'MyCard'}", resolvedGenericSchema.value().toString());
}

TEST(CppGeneratedClass, canUnmarshallGenericObject) {
    auto value =
        Value().setMapValue("container",
                            Value(Value().setMapValue("value",
                                                      Value(Value()
                                                                .setMapValue("title", Value(STRING_LITERAL("Goodbye")))
                                                                .setMapValue("width", Value(24))
                                                                .setMapValue("height", Value(50))
                                                                .setMapValue("selected", Value(false))))));

    SimpleExceptionTracker exceptionTracker;
    GenericObject out;
    GenericObject::unmarshall(exceptionTracker, value, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto card = out.getContainer().getValue();

    ASSERT_EQ("Goodbye", card.title);
    ASSERT_EQ(24, card.width);
    ASSERT_EQ(50, card.height);
    ASSERT_EQ(false, card.selected);
}

TEST(CppGeneratedClass, canMarshallFuture) {
    FutureContainer object;

    TypedPromise<MyCard> promise;
    object.setFuture(promise.getFuture());

    SimpleExceptionTracker exceptionTracker;
    Value out;
    FutureContainer::marshall(exceptionTracker, object, out);
    auto typedObject = out.getTypedObjectRef();
    ASSERT_TRUE(typedObject != nullptr);

    auto future =
        typedObject->getPropertyForName(STRING_LITERAL("future")).checkedToValdiObject<Promise>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    Result<Value> storedResult;
    future->onComplete([&](const Result<Value>& result) { storedResult = result; });

    ASSERT_TRUE(storedResult.empty()) << storedResult.description();

    MyCard card;
    card.title = StringBox::fromCString("Hello World");
    promise.setValue(card);

    ASSERT_FALSE(storedResult.empty()) << storedResult.description();
    ASSERT_TRUE(storedResult.success()) << storedResult.description();

    auto marshalledCard = storedResult.value().checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    auto value = marshalledCard->getPropertyForName(STRING_LITERAL("title"));
    ASSERT_EQ(STRING_LITERAL("Hello World"), value.toStringBox());
}

TEST(CppGeneratedClass, canUnmarshallFuture) {
    auto promise = makeShared<ResolvablePromise>();
    auto value = Value().setMapValue("future", Value(promise));

    SimpleExceptionTracker exceptionTracker;
    FutureContainer out;
    FutureContainer::unmarshall(exceptionTracker, value, out);
    ASSERT_TRUE(exceptionTracker) << exceptionTracker.extractError();

    Result<MyCard> storedResult;
    out.getFuture().then([&](Result<MyCard> result) -> void { storedResult = std::move(result); });

    ASSERT_TRUE(storedResult.empty()) << storedResult.description();

    promise->fulfill(Value()
                         .setMapValue("title", Value(STRING_LITERAL("Hello World")))
                         .setMapValue("width", Value(42))
                         .setMapValue("height", Value(100)));

    ASSERT_FALSE(storedResult.empty()) << storedResult.description();
    ASSERT_TRUE(storedResult.success()) << storedResult.description();
    ASSERT_EQ(STRING_LITERAL("Hello World"), storedResult.value().title);
}

TEST(CppGeneratedClass, canCopyGeneratedModel) {
    auto model = snap::valdi_modules::test::MyModel(42, STRING_LITERAL("Hello World"), true);

    ASSERT_EQ(42, model.getI());
    ASSERT_EQ(STRING_LITERAL("Hello World"), model.getS());
    ASSERT_EQ(true, model.getB());

    // Copy constructor
    auto copiedModel = model;

    ASSERT_EQ(42, copiedModel.getI());
    ASSERT_EQ(STRING_LITERAL("Hello World"), copiedModel.getS());
    ASSERT_EQ(true, copiedModel.getB());

    copiedModel.setI(12);

    copiedModel.setS(STRING_LITERAL("Goodbye"));
    copiedModel.setB(false);

    // Copy assignment
    model = copiedModel;

    ASSERT_EQ(12, model.getI());
    ASSERT_EQ(STRING_LITERAL("Goodbye"), model.getS());
    ASSERT_EQ(false, model.getB());
}

TEST(CppGeneratedClass, canMoveGeneratedModel) {
    auto model = snap::valdi_modules::test::MyModel(42, STRING_LITERAL("Hello World"), true);

    ASSERT_EQ(42, model.getI());
    ASSERT_EQ(STRING_LITERAL("Hello World"), model.getS());
    ASSERT_EQ(true, model.getB());

    // Copy constructor
    auto movedModel = std::move(model);

    ASSERT_EQ(42, movedModel.getI());
    ASSERT_EQ(STRING_LITERAL("Hello World"), movedModel.getS());
    ASSERT_EQ(true, movedModel.getB());

    ASSERT_EQ(42, model.getI());
    ASSERT_EQ(STRING_LITERAL(""), model.getS());
    ASSERT_EQ(true, model.getB());

    movedModel.setI(12);
    movedModel.setB(false);

    // Copy assignment
    model = std::move(movedModel);

    ASSERT_EQ(12, model.getI());
    ASSERT_EQ(STRING_LITERAL("Hello World"), model.getS());
    ASSERT_EQ(false, model.getB());

    ASSERT_EQ(12, movedModel.getI());
    ASSERT_EQ(STRING_LITERAL(""), movedModel.getS());
    ASSERT_EQ(false, movedModel.getB());
}

} // namespace ValdiTest
