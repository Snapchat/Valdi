#include "valdi_core/cpp/Views/Measure.hpp"

#include "gtest/gtest.h"

#include <cstdint>
#include <limits>

using namespace Valdi;

TEST(MeasureTest, PackUnpackRoundTrip) {
    auto [h, v] = unpackIntPair(packIntPair(100, 200));
    EXPECT_EQ(h, 100);
    EXPECT_EQ(v, 200);
}

TEST(MeasureTest, PackUnpackNegativeValues) {
    auto [h, v] = unpackIntPair(packIntPair(-1, -2));
    EXPECT_EQ(h, -1);
    EXPECT_EQ(v, -2);
}

TEST(MeasureTest, PackUnpackZero) {
    auto [h, v] = unpackIntPair(packIntPair(0, 0));
    EXPECT_EQ(h, 0);
    EXPECT_EQ(v, 0);
}

TEST(MeasureTest, PackUnpackInt32Extremes) {
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();
    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    auto [h, v] = unpackIntPair(packIntPair(kMax, kMin));
    EXPECT_EQ(h, kMax);
    EXPECT_EQ(v, kMin);
}

TEST(MeasureTest, PackUnpackAsymmetric) {
    auto [h, v] = unpackIntPair(packIntPair(42, 0));
    EXPECT_EQ(h, 42);
    EXPECT_EQ(v, 0);

    auto [h2, v2] = unpackIntPair(packIntPair(0, 99));
    EXPECT_EQ(h2, 0);
    EXPECT_EQ(v2, 99);
}
