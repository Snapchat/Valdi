#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaParser.hpp"
#include "valdi_core/cpp/Utils/BigInt.hpp"
#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>

namespace ValdiTest {
using namespace Valdi;

TEST(BigInt, exactInt64Boundaries) {
    for (auto value : {int64_t(0),
                       int64_t(-1),
                       int64_t(9007199254740993),
                       std::numeric_limits<int64_t>::min(),
                       std::numeric_limits<int64_t>::max()}) {
        auto number = BigInt::make(value);
        EXPECT_EQ(value, number->toInt64());
        SimpleExceptionTracker tracker;
        auto parsed = BigInt::make(number->toString(), tracker);
        ASSERT_TRUE(tracker);
        EXPECT_EQ(value, parsed->toInt64());
        EXPECT_EQ(number->toString(), parsed->toString());
        std::ostringstream stream;
        stream << std::hex << *number;
        EXPECT_EQ(number->toString(), stream.str());
    }
}

TEST(BigInt, preservesLargeValuesAndPrints) {
    for (const auto* decimal : {"9223372036854775808",
                                "-9223372036854775809",
                                "18446744073709551616",
                                "-999999999999999999999999999999999999999999999999999999999999"}) {
        SimpleExceptionTracker tracker;
        auto number = BigInt::make(decimal, tracker);
        ASSERT_TRUE(tracker);
        EXPECT_FALSE(number->toInt64());
        EXPECT_EQ(decimal, number->toString());
        std::ostringstream stream;
        stream << *number;
        EXPECT_EQ(decimal, stream.str());
        EXPECT_EQ("\"" + std::string(decimal) + "\"", valueToJsonString(Value(number)));
    }
}

TEST(BigInt, canonicalizesAndRejectsMalformedInput) {
    for (const auto* zero : {"0", "-0", "+000", "-000"}) {
        SimpleExceptionTracker tracker;
        auto number = BigInt::make(zero, tracker);
        ASSERT_TRUE(tracker);
        EXPECT_EQ(0, number->toInt64());
        EXPECT_EQ("0", number->toString());
    }
    SimpleExceptionTracker tracker;
    EXPECT_EQ("9223372036854775808", BigInt::make("+0009223372036854775808", tracker)->toString());
    for (const auto* invalid : {"", "+", "-", " 1", "1 ", "1.0", "0x10", "1n", "1e3", "--1"}) {
        SimpleExceptionTracker error;
        EXPECT_EQ(nullptr, BigInt::make(invalid, error));
        ASSERT_FALSE(error);
        EXPECT_EQ("Invalid BigInt decimal string", error.extractError().getMessage().toStringView());
    }
}

TEST(BigInt, comparisonOperatorsOrderSignedValuesExactly) {
    constexpr std::array<std::string_view, 13> decimals = {
        "-100000000000000000001",
        "-100000000000000000000",
        "-9223372036854775809",
        "-9223372036854775808",
        "-1",
        "0",
        "1",
        "9007199254740993",
        "9223372036854775807",
        "9223372036854775808",
        "9223372036854775809",
        "100000000000000000000",
        "100000000000000000001",
    };
    std::array<Ref<BigInt>, decimals.size()> integers;
    SimpleExceptionTracker tracker;
    for (size_t i = 0; i < decimals.size(); i++) {
        integers[i] = BigInt::make(decimals[i], tracker);
        ASSERT_TRUE(tracker);
    }
    for (size_t i = 0; i < integers.size(); i++) {
        for (size_t j = 0; j < integers.size(); j++) {
            SCOPED_TRACE(std::string(decimals[i]) + " compared to " + std::string(decimals[j]));
            EXPECT_EQ(i == j, *integers[i] == *integers[j]);
            EXPECT_EQ(i != j, *integers[i] != *integers[j]);
            EXPECT_EQ(i < j, *integers[i] < *integers[j]);
            EXPECT_EQ(i <= j, *integers[i] <= *integers[j]);
            EXPECT_EQ(i > j, *integers[i] > *integers[j]);
            EXPECT_EQ(i >= j, *integers[i] >= *integers[j]);
            EXPECT_EQ(i < j, Value(integers[i]) < Value(integers[j]));
        }
    }
}

TEST(BigInt, canonicalValuesShareEqualityAndHash) {
    for (const auto* decimal : {"-000",
                                "+001",
                                "-001",
                                "+009223372036854775807",
                                "-009223372036854775808",
                                "+009223372036854775808",
                                "-009223372036854775809"}) {
        SimpleExceptionTracker tracker;
        auto number = BigInt::make(decimal, tracker);
        ASSERT_TRUE(tracker);
        auto canonical = BigInt::make(number->toString(), tracker);
        ASSERT_TRUE(tracker);
        EXPECT_EQ(*number, *canonical);
        EXPECT_EQ(number->hash(), canonical->hash());
        EXPECT_EQ(std::hash<BigInt>()(*number), number->hash());
        EXPECT_EQ(std::hash<BigInt>()(*canonical), Value(canonical).hash());
    }
}

TEST(BigInt, valueSemanticsAndLifetime) {
    SimpleExceptionTracker tracker;
    auto number = BigInt::make("9223372036854775808", tracker);
    EXPECT_EQ(1, number->retainCount());
    {
        Value value(number);
        EXPECT_TRUE(value.isBigInt());
        EXPECT_FALSE(value.isNumber());
        EXPECT_EQ(2, number->retainCount());
        Value copy(value);
        EXPECT_EQ(3, number->retainCount());
        auto equal = Value(BigInt::make("+009223372036854775808", tracker));
        EXPECT_EQ(value, equal);
        EXPECT_EQ(value.hash(), equal.hash());
        EXPECT_LT(Value(BigInt::make(std::numeric_limits<int64_t>::max())), value);
        EXPECT_NE(Value(BigInt::make(1)), Value(1));
        EXPECT_EQ(number, value.checkedTo<Ref<BigInt>>(tracker));
    }
    EXPECT_EQ(1, number->retainCount());
    EXPECT_LT(Value(BigInt::make("-9223372036854775809", tracker)),
              Value(BigInt::make(std::numeric_limits<int64_t>::min())));
    EXPECT_LT(Value(BigInt::make("-100000000000000000000", tracker)),
              Value(BigInt::make("-9223372036854775809", tracker)));
    SimpleExceptionTracker mismatch;
    EXPECT_EQ(nullptr, Value(1).checkedTo<Ref<BigInt>>(mismatch));
    EXPECT_FALSE(mismatch);
    mismatch.clearError();
}

TEST(BigInt, schemaRoundTrip) {
    auto schema = ValueSchema::bigInt();
    EXPECT_TRUE(schema.isBigInt());
    EXPECT_EQ("bigint", schema.toString());
    EXPECT_EQ("B", schema.toString(ValueSchemaPrintFormat::Short, false));
    for (const auto* alias : {"B", "bigint"}) {
        auto parsed = ValueSchemaParser::parse(alias);
        ASSERT_TRUE(parsed) << parsed.description();
        EXPECT_EQ(schema, parsed.value());
        EXPECT_EQ(schema.asOptional(), ValueSchemaParser::parse(std::string(alias) + "?").value());
        EXPECT_EQ(schema.asArray(), ValueSchemaParser::parse("a<" + std::string(alias) + ">").value());
        EXPECT_EQ(ValueSchema::boxed(schema.asOptional()), ValueSchemaParser::parse(std::string(alias) + "@?").value());
    }
    auto function = ValueSchemaParser::parse("f(B, b): bigint");
    ASSERT_TRUE(function) << function.description();
    EXPECT_EQ("func(bigint, bool): bigint", function.value().toString());
    EXPECT_EQ(ValueSchema::boolean(), ValueSchemaParser::parse("b").value());
    EXPECT_EQ(ValueSchema::boolean().asOptional(), ValueSchemaParser::parse("b?").value());
    EXPECT_EQ(ValueSchema::longInteger(), ValueSchemaParser::parse("l").value());
    for (const auto* invalid : {"bi", "big", "bigintx", "bigint64", "B0", "BigInt", "bool"}) {
        EXPECT_FALSE(ValueSchemaParser::parse(invalid)) << invalid;
    }
}

} // namespace ValdiTest
