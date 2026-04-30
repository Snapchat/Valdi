#include "valdi_core/cpp/Utils/Future.hpp"
#include <gtest/gtest.h>

namespace Valdi::test {

TEST(Future, notifiesThenOnComplete) {
    TypedPromise<int> promise;
    auto future = promise.getFuture();
    Result<int> outResult;
    future.then([&](Result<int> result) { outResult = result; });

    ASSERT_TRUE(outResult.empty()) << outResult.description();
    promise.setValue(1);
    ASSERT_FALSE(outResult.empty()) << outResult.description();
    ASSERT_TRUE(outResult.success()) << outResult.description();
    ASSERT_EQ(1, outResult.value());
}

TEST(Future, notifiesThenOnError) {
    TypedPromise<int> promise;
    auto future = promise.getFuture();
    Result<int> outResult;
    future.then([&](Result<int> result) { outResult = result; });

    ASSERT_TRUE(outResult.empty()) << outResult.description();
    promise.setError(Error("This is an error"));
    ASSERT_FALSE(outResult.empty()) << outResult.description();
    ASSERT_TRUE(outResult.failure()) << outResult.description();
    ASSERT_EQ("This is an error", outResult.error().toString());
}

TEST(Future, notifiesThenImmediatelyWhenAlreadyCompleted) {
    TypedPromise<int> promise;
    promise.setValue(1);

    auto future = promise.getFuture();
    Result<int> outResult;
    future.then([&](Result<int> result) { outResult = result; });

    ASSERT_FALSE(outResult.empty()) << outResult.description();
    ASSERT_TRUE(outResult.success()) << outResult.description();
    ASSERT_EQ(1, outResult.value());
}

TEST(Future, notifiesMultipleThenOnComplete) {
    TypedPromise<int> promise;

    auto future1 = promise.getFuture();
    auto future2 = promise.getFuture();

    std::vector<Result<int>> outResults;
    future1.then([&](Result<int> result) { outResults.emplace_back(result); });
    future2.then([&](Result<int> result) { outResults.emplace_back(result); });

    future1.then([&](Result<int> result) { outResults.emplace_back(result); });

    ASSERT_EQ(0, outResults.size());

    promise.setValue(1);

    ASSERT_EQ(3, outResults.size());

    for (const auto& result : outResults) {
        ASSERT_TRUE(result.success()) << result.description();
        ASSERT_EQ(1, result.value());
    }
}

} // namespace Valdi::test
