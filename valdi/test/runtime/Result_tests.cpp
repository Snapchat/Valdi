//
//  Result_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 2/24/20.
//

#include "valdi_core/cpp/Utils/Result.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

TEST(Result, canBeEmpty) {
    Result<int> result;
    ASSERT_TRUE(result.empty());
    ASSERT_FALSE(result.success());
    ASSERT_FALSE(result.failure());
}

TEST(Result, canHoldValue) {
    Result<int> result = 42;
    ASSERT_FALSE(result.empty());
    ASSERT_TRUE(result.success());
    ASSERT_FALSE(result.failure());

    ASSERT_EQ(42, result.value());
}

TEST(Result, canHoldFailure) {
    Result<bool> result = Error("Bad");

    ASSERT_FALSE(result.empty());
    ASSERT_FALSE(result.success());
    ASSERT_TRUE(result.failure());

    ASSERT_EQ("Bad", result.error().toString());
}

TEST(Result, canMoveValue) {
    Result<std::string> result = std::string("Hello");

    ASSERT_EQ("Hello", result.value());

    auto message = result.moveValue();
    ASSERT_EQ("Hello", message);
    ASSERT_EQ("", result.value());
}

TEST(Result, canMoveError) {
    Result<bool> result = Error("Bad");

    ASSERT_EQ("Bad", result.error().toString());

    auto error = result.moveError();
    ASSERT_EQ("Bad", error.toString());
    ASSERT_EQ("Empty Error", result.error().toString());
}

TEST(Result, canBeCopied) {
    auto ptr = makeShared<std::string>();

    {
        Result<Shared<std::string>> result(ptr);

        ASSERT_EQ(2, ptr.use_count());
        ASSERT_TRUE(result.success());

        Result<Shared<std::string>> result2(result);

        ASSERT_EQ(3, ptr.use_count());
        ASSERT_TRUE(result.success());
        ASSERT_TRUE(result2.success());
        ASSERT_EQ(ptr, result2.value());

        Result<Shared<std::string>> result3;
        result3 = result2;

        ASSERT_EQ(4, ptr.use_count());
        ASSERT_TRUE(result2.success());
        ASSERT_TRUE(result3.success());
        ASSERT_EQ(ptr, result3.value());
    }

    ASSERT_EQ(1, ptr.use_count());
}

TEST(Result, canBeMoved) {
    auto ptr = makeShared<std::string>();

    {
        Result<Shared<std::string>> result(ptr);

        ASSERT_EQ(2, ptr.use_count());
        ASSERT_TRUE(result.success());

        Result<Shared<std::string>> result2(std::move(result));

        ASSERT_EQ(2, ptr.use_count());
        ASSERT_TRUE(result2.success());
        ASSERT_FALSE(result.success());

        Result<Shared<std::string>> result3;
        result3 = std::move(result2);

        ASSERT_EQ(2, ptr.use_count());
        ASSERT_TRUE(result3.success());
        ASSERT_FALSE(result2.success());
    }

    ASSERT_EQ(1, ptr.use_count());
}

// Polymorphic like ValdiModuleArchive, and remembers its own address the way an arm64e vtable pointer
// signed with the object address does, so a bitwise move of the Result storage is detected on any platform.
class AddressBoundValue {
public:
    AddressBoundValue() = default;
    AddressBoundValue(const AddressBoundValue& /*other*/) {}
    AddressBoundValue(AddressBoundValue&& /*other*/) noexcept {}

    virtual ~AddressBoundValue() {
        EXPECT_EQ(this, _self);
    }

    bool isAtOwnAddress() const {
        return _self == this;
    }

private:
    const AddressBoundValue* _self = this;
};

static Result<AddressBoundValue> makeAddressBoundResult() {
    return AddressBoundValue();
}

TEST(Result, moveConstructionKeepsValueBoundToItsAddress) {
    Result<AddressBoundValue> result = makeAddressBoundResult();
    Result<AddressBoundValue> result2(std::move(result));

    ASSERT_TRUE(result2.success());
    ASSERT_TRUE(result2.value().isAtOwnAddress());
    ASSERT_TRUE(result.empty());
}

TEST(Result, moveAssignmentKeepsValueBoundToItsAddress) {
    Result<AddressBoundValue> result;
    result = makeAddressBoundResult();

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.value().isAtOwnAddress());

    Result<AddressBoundValue> result2 = Error("Bad");
    result2 = std::move(result);

    ASSERT_TRUE(result2.success());
    ASSERT_TRUE(result2.value().isAtOwnAddress());
    ASSERT_TRUE(result.empty());
}

TEST(Result, isCompact) {
    Result<bool> result1;

    ASSERT_EQ(sizeof(void*) + sizeof(size_t), sizeof(result1));

    Result<std::string> result2;
    ASSERT_EQ(sizeof(std::string) + sizeof(size_t), sizeof(result2));
}

} // namespace ValdiTest
