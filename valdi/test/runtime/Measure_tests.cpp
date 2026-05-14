//
//  Measure_tests.cpp
//  valdi-pc
//

#include "valdi/runtime/Views/Measure.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

TEST(Measure, pointsToPixelsRoundsCorrectly) {
    ASSERT_EQ(6, pointsToPixels(2.0f, 3.0f));
    ASSERT_EQ(0, pointsToPixels(0.0f, 2.5f));
    ASSERT_EQ(3, pointsToPixels(1.0f, 2.5f));
}

TEST(Measure, pixelsToPointsDividesCorrectly) {
    ASSERT_FLOAT_EQ(2.0f, pixelsToPoints(6, 3.0f));
    ASSERT_FLOAT_EQ(0.0f, pixelsToPoints(0, 2.5f));
    ASSERT_FLOAT_EQ(4.0f, pixelsToPoints(10, 2.5f));
}

TEST(Measure, roundToPixelGridSnapsToGrid) {
    float scale = 2.0f;
    ASSERT_FLOAT_EQ(1.5f, roundToPixelGrid(1.4f, scale));
    ASSERT_FLOAT_EQ(1.0f, roundToPixelGrid(1.0f, scale));
}

TEST(CoordinateResolver, getPointScaleReturnsConstructionValue) {
    CoordinateResolver resolver(2.75f);
    ASSERT_FLOAT_EQ(2.75f, resolver.getPointScale());
}

TEST(CoordinateResolver, toPixelsConvertsCorrectly) {
    CoordinateResolver resolver(3.0f);
    ASSERT_EQ(9, resolver.toPixels(3.0f));
    ASSERT_EQ(0, resolver.toPixels(0.0f));
}

TEST(CoordinateResolver, toPointsConvertsCorrectly) {
    CoordinateResolver resolver(2.5f);
    ASSERT_FLOAT_EQ(4.0f, resolver.toPoints(10));
    ASSERT_FLOAT_EQ(0.0f, resolver.toPoints(0));
}

TEST(CoordinateResolver, toPixelsFReturnsUnroundedValue) {
    CoordinateResolver resolver(2.5f);
    ASSERT_FLOAT_EQ(3.75f, resolver.toPixelsF(1.5f));
}

TEST(Measure, packUnpackIntPairRoundTrips) {
    auto packed = packIntPair(1080, 1920);
    auto [h, v] = unpackIntPair(packed);
    ASSERT_EQ(1080, h);
    ASSERT_EQ(1920, v);
}

TEST(Measure, packUnpackNegativeValues) {
    auto packed = packIntPair(-1, -2);
    auto [h, v] = unpackIntPair(packed);
    ASSERT_EQ(-1, h);
    ASSERT_EQ(-2, v);
}

TEST(Measure, pointsToPackedPixelsRoundTrips) {
    float scale = 2.5f;
    auto packed = pointsToPackedPixels(10.0f, 20.0f, scale);
    auto [h, v] = unpackIntPair(packed);
    ASSERT_EQ(25, h);
    ASSERT_EQ(50, v);
}

TEST(Measure, sanitizeMeasurementExactly) {
    ASSERT_FLOAT_EQ(100.0f, sanitizeMeasurement(100.0f, 50.0f, 2.0f, MeasureModeExactly));
}

TEST(Measure, sanitizeMeasurementAtMostReturnsSmaller) {
    // 49.3 * 2.0 = 98.6, ceilf = 99.0, /2.0 = 49.5, min(100, 49.5) = 49.5
    ASSERT_FLOAT_EQ(49.5f, sanitizeMeasurement(100.0f, 49.3f, 2.0f, MeasureModeAtMost));
    // When constrained is smaller: min(40, 49.5) = 40
    ASSERT_FLOAT_EQ(40.0f, sanitizeMeasurement(40.0f, 49.3f, 2.0f, MeasureModeAtMost));
}

TEST(Measure, sanitizeMeasurementUnspecifiedReturnsMeasured) {
    // 49.3 * 2.0 = 98.6, ceilf = 99.0, /2.0 = 49.5
    float result = sanitizeMeasurement(100.0f, 49.3f, 2.0f, MeasureModeUnspecified);
    ASSERT_FLOAT_EQ(49.5f, result);
}

} // namespace ValdiTest
