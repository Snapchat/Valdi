//
//  Value_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 11/7/19.
//

#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

class DeallocationTracker : public ValdiObject {
public:
    DeallocationTracker(int& deallocatedPtr) : _deallocatedPtr(deallocatedPtr) {}

    ~DeallocationTracker() override {
        _deallocatedPtr++;
    }

    VALDI_CLASS_HEADER_IMPL(DeallocationTracker)

private:
    int& _deallocatedPtr;
};

TEST(Value, canHandleBooleans) {
    ASSERT_TRUE(Value(true).toBool());
    ASSERT_FALSE(Value(false).toBool());

    ASSERT_EQ("true", Value(true).toString());
    ASSERT_EQ("false", Value(false).toString());
    ASSERT_EQ(Value(true), Value(true));
    ASSERT_EQ(Value(false), Value(false));
    ASSERT_NE(Value(false), Value(true));

    ASSERT_TRUE(Value(true).isBool());
    ASSERT_TRUE(Value(false).isBool());

    auto trueValue = Value(true);
    auto copy = trueValue;
    ASSERT_EQ(copy, trueValue);
    ASSERT_EQ(Value(trueValue), trueValue);

    auto falseValue = Value(false);
    auto copy2 = falseValue;
    ASSERT_EQ(copy2, falseValue);
    ASSERT_EQ(Value(falseValue), falseValue);
}

TEST(Value, canHandleNumbers) {
    ASSERT_EQ(0, Value(0).toInt());
    ASSERT_EQ(0.0, Value(0).toDouble());

    ASSERT_EQ(0, Value(0.0).toInt());
    ASSERT_EQ(0.0, Value(0.0).toDouble());

    ASSERT_EQ(42, Value(42).toInt());
    ASSERT_EQ(42, Value(42.5).toInt());
    ASSERT_EQ(42.5, Value(42.5).toDouble());

    ASSERT_EQ("0", Value(0).toString());
    ASSERT_EQ("0.000000", Value(0.0).toString());
    ASSERT_EQ("42.500000", Value(42.5).toString());
    ASSERT_EQ("42", Value(42).toString());

    ASSERT_EQ(Value(0), Value(0));
    ASSERT_EQ(Value(42), Value(42));
    ASSERT_NE(Value(42), Value(43));
    ASSERT_EQ(Value(42.0), Value(42.0));
    ASSERT_NE(Value(42.0), Value(43.0));
    ASSERT_EQ(Value(42), Value(42.0));
    ASSERT_NE(Value(42), Value(42.1));

    ASSERT_TRUE(Value(42).isNumber());
    ASSERT_TRUE(Value(42.0).isNumber());

    auto number = Value(42.0);
    auto copy = number;
    ASSERT_EQ(copy, number);
    ASSERT_EQ(Value(number), number);

    auto numberInt = Value(42);
    auto copy2 = numberInt;
    ASSERT_EQ(copy2, numberInt);
    ASSERT_EQ(Value(numberInt), numberInt);
}

TEST(Value, canHandleNullAndUndefined) {
    ASSERT_TRUE(Value().isNull());
    ASSERT_TRUE(Value::undefined().isUndefined());
    ASSERT_TRUE(Value().isNullOrUndefined());
    ASSERT_TRUE(Value::undefined().isNullOrUndefined());
    ASSERT_FALSE(Value().isUndefined());
    ASSERT_FALSE(Value::undefined().isNull());

    ASSERT_EQ(Value(), Value());
    ASSERT_EQ(Value::undefined(), Value::undefined());
    ASSERT_NE(Value::undefined(), Value());

    ASSERT_EQ("<null>", Value().toString());
    ASSERT_EQ("<undefined>", Value::undefined().toString());

    auto nullValue = Value();
    auto copy = nullValue;
    ASSERT_EQ(copy, nullValue);
    ASSERT_EQ(Value(nullValue), nullValue);

    auto undefinedValue = Value::undefined();
    auto copy2 = undefinedValue;
    ASSERT_EQ(copy2, undefinedValue);
    ASSERT_EQ(Value(undefinedValue), undefinedValue);
}

TEST(Value, canHandleStrings) {
    ASSERT_EQ(Value(STRING_LITERAL("")), Value(STRING_LITERAL("")));
    ASSERT_EQ(Value(STRING_LITERAL("hello")), Value(STRING_LITERAL("hello")));
    ASSERT_NE(Value(STRING_LITERAL("hello")), Value(STRING_LITERAL("world")));

    ASSERT_TRUE(Value(STRING_LITERAL("")).isString());
    ASSERT_TRUE(Value(STRING_LITERAL("nice")).isString());

    ASSERT_EQ(STRING_LITERAL(""), Value(STRING_LITERAL("")).toStringBox());
    ASSERT_EQ(STRING_LITERAL(""), Value(Ref<InternedStringImpl>()).toStringBox());

    ASSERT_EQ(STRING_LITERAL("hello"), Value(STRING_LITERAL("hello")).toStringBox());

    auto strValue = Value(STRING_LITERAL("hello"));
    auto copy = strValue;
    ASSERT_EQ(copy, strValue);
    ASSERT_EQ(Value(strValue), strValue);
}

TEST(Value, comparesInternedAndStaticStringsWithoutInterning) {
    auto interned = Value(STRING_LITERAL("hello"));
    auto staticUtf8 = Value(StaticString::makeUTF8(std::string_view("hello")));
    std::u16string utf16 = u"hello";
    auto staticUtf16 = Value(StaticString::makeUTF16(utf16.data(), utf16.size()));

    ASSERT_EQ(interned, staticUtf8);
    ASSERT_EQ(staticUtf8, interned);
    ASSERT_EQ(interned, staticUtf16);
    ASSERT_EQ(staticUtf16, interned);
    ASSERT_NE(interned, Value(StaticString::makeUTF8(std::string_view("hellO"))));
    ASSERT_NE(interned, Value(StaticString::makeUTF8(std::string_view("hell"))));
    ASSERT_NE(Value(STRING_LITERAL("")), staticUtf8);
    ASSERT_EQ(Value(STRING_LITERAL("")), Value(StaticString::makeUTF8(std::string_view(""))));

    // A null interned pointer aliases the empty string.
    ASSERT_EQ(Value(Ref<InternedStringImpl>()), Value(STRING_LITERAL("")));
    ASSERT_EQ(Value(STRING_LITERAL("")), Value(Ref<InternedStringImpl>()));
    ASSERT_EQ(Value(Ref<InternedStringImpl>()).hash(), Value(STRING_LITERAL("")).hash());
    ASSERT_EQ(Value(Ref<InternedStringImpl>()), Value(StaticString::makeUTF8(std::string_view(""))));
    ASSERT_NE(Value(Ref<InternedStringImpl>()), staticUtf8);

    ASSERT_EQ(interned.hash(), Value(STRING_LITERAL("hello")).hash());
    ASSERT_EQ(interned.hash(), STRING_LITERAL("hello").hash());
    ASSERT_NE(interned.hash(), Value(STRING_LITERAL("world")).hash());
}

TEST(Value, canHandleArray) {
    auto array = ValueArray::make(2);

    ASSERT_EQ(1, array.use_count());

    auto value = Value(array);

    ASSERT_EQ(2, array.use_count());
    ASSERT_TRUE(value.isArray());
    ASSERT_TRUE(value.getArray() != nullptr);
    ASSERT_EQ(array.get(), value.getArray());
}

TEST(Value, canMoveSharedPointers) {
    auto map = makeShared<ValueMap>();
    ASSERT_EQ(1, map.use_count());

    // Using constructors

    Value copy1(map);
    ASSERT_EQ(2, map.use_count());

    Value move1(std::move(copy1));
    ASSERT_EQ(2, map.use_count());

    move1 = Value();
    ASSERT_EQ(1, map.use_count());
    copy1 = Value();
    ASSERT_EQ(1, map.use_count());

    // Using assignment operator

    auto copy2 = Value(map);
    ASSERT_EQ(2, map.use_count());

    auto move2 = Value(std::move(copy2));
    ASSERT_EQ(2, map.use_count());

    move2 = Value();
    ASSERT_EQ(1, map.use_count());
    copy2 = Value();
    ASSERT_EQ(1, map.use_count());
}

TEST(Value, canCopySharedPointers) {
    auto map = makeShared<ValueMap>();
    ASSERT_EQ(1, map.use_count());

    auto copy1 = Value(map);
    ASSERT_EQ(2, map.use_count());

    auto copy2 = copy1;
    ASSERT_EQ(3, map.use_count());

    Value copy3(copy2);
    ASSERT_EQ(4, map.use_count());

    copy3 = Value();
    ASSERT_EQ(3, map.use_count());
    copy2 = Value();
    ASSERT_EQ(2, map.use_count());
    copy1 = Value();
    ASSERT_EQ(1, map.use_count());
}

TEST(Value, canStoreFunctions) {
    auto array = Valdi::makeShared<std::vector<Value>>();

    Value func(makeShared<ValueFunctionWithCallable>([array](const ValueFunctionCallContext& callContext) -> Value {
        size_t i = callContext.getParametersSize();
        while (i > 0) {
            i--;
            array->emplace_back(callContext.getParameter(i));
        }

        return Value::undefined();
    }));

    ASSERT_TRUE(func.isFunction());

    // Should have been converted to shared
    ASSERT_TRUE(func.getFunctionRef() != nullptr);
    ASSERT_TRUE(func.getFunction() != nullptr);

    auto param = Value(42);
    ASSERT_EQ(static_cast<size_t>(0), array->size());
    (*func.getFunction())(&param, 1);

    ASSERT_EQ(static_cast<size_t>(1), array->size());
    ASSERT_EQ(Value(42), (*array)[0]);

    Value func2(makeShared<ValueFunctionWithCallable>([array](const ValueFunctionCallContext& callContext) -> Value {
        size_t i = callContext.getParametersSize();
        while (i > 0) {
            i--;
            array->emplace_back(callContext.getParameter(i));
        }

        return Value::undefined();
    }));

    ASSERT_TRUE(func2.isFunction());
    ASSERT_TRUE(func2.getFunctionRef() != nullptr);
    ASSERT_TRUE(func2.getFunction() != nullptr);

    ASSERT_NE(func, func2);
    ASSERT_NE(func.getFunction(), func2.getFunction());
}

TEST(Value, canImplicitlyConvertIntToString) {
    auto result = ValueConverter::toString(Value(42));

    ASSERT_TRUE(result.success());
    ASSERT_EQ(STRING_LITERAL("42"), result.value());
}

TEST(Value, canImplicitlyConvertDoubleToString) {
    auto result = ValueConverter::toString(Value(42.5));

    ASSERT_TRUE(result.success());
    ASSERT_EQ(STRING_LITERAL("42.500000"), result.value());
}

TEST(Value, omitsDecimalWhenConvertingDoubleToStringIfDoubleIsIntRepresentable) {
    auto result = ValueConverter::toString(Value(42.0));

    ASSERT_TRUE(result.success());
    ASSERT_EQ(STRING_LITERAL("42"), result.value());
}

TEST(Value, canSerializeValue) {
    auto value = Value()
                     .setMapValue("hello", Value(STRING_LITERAL("world")))
                     .setMapValue("this is", Value(STRING_LITERAL("great")));

    auto result = serializeValue(value);

    ASSERT_EQ("hello=world\nthis is=great\n",
              std::string_view(reinterpret_cast<const char*>(result->data()), result->size()));
}

TEST(Value, canDeserializeValue) {
    std::string_view serializedValue = "hello=world\nthis is=great\n";

    auto result = deserializeValue(reinterpret_cast<const Byte*>(serializedValue.data()), serializedValue.size());

    ASSERT_TRUE(result.isMap());
    ASSERT_EQ(static_cast<size_t>(2), result.getMap()->size());
    ASSERT_EQ(STRING_LITERAL("world"), result.getMapValue("hello").toStringBox());
    ASSERT_EQ(STRING_LITERAL("great"), result.getMapValue("this is").toStringBox());
}

TEST(Value, canSerializeAndDeserializeValueWithSpecialCharacters) {
    auto value = Value().setMapValue("key", Value(STRING_LITERAL("=>,|':===")));

    auto result = serializeValue(value);

    auto deserialized = deserializeValue(result->data(), result->size());

    ASSERT_EQ(value, deserialized);
}

TEST(Value, canReleaseWrappedObject) {
    int deallocatedCount = 0;

    Value value(makeShared<DeallocationTracker>(deallocatedCount));

    ASSERT_EQ(0, deallocatedCount);
    value = Value();
    ASSERT_EQ(1, deallocatedCount);
}

TEST(Value, canReleaseWrappedObjectInMap) {
    int deallocatedCount = 0;

    Value value;
    value.setMapValue("key", Value(makeShared<DeallocationTracker>(deallocatedCount)));

    ASSERT_EQ(0, deallocatedCount);
    value = Value();
    ASSERT_EQ(1, deallocatedCount);
}

TEST(Value, canHoldError) {
    auto error = Error("This is bad").rethrow("Worst thing error");
    auto value = Value(error);

    ASSERT_TRUE(value.isError());

    ASSERT_EQ(error, value.getError());
    ASSERT_EQ("<error: Worst thing error\n[caused by]: This is bad>", value.toString());
}

} // namespace ValdiTest
