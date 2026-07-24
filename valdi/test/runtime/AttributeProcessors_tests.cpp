#include "valdi/runtime/Attributes/DefaultAttributeProcessors.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "gtest/gtest.h"
#include <cmath>
#include <initializer_list>

using namespace Valdi;

namespace ValdiTest {

template<typename ColorsT>
static Value makeGradientValueImpl(const ColorsT& colors,
                                   std::initializer_list<double> locations,
                                   int32_t angle,
                                   bool radial) {
    auto outColors = ValueArray::make(colors.size());
    auto outLocations = ValueArray::make(locations.size());

    size_t i = 0;
    for (const auto& color : colors) {
        outColors->emplace(i++, Value(color));
    }

    i = 0;
    for (auto location : locations) {
        outLocations->emplace(i++, Value(location));
    }

    return Value(ValueArray::make({Value(outColors), Value(outLocations), Value(angle), Value(radial)}));
}

static Value makeGradientValue(std::initializer_list<StringBox> colors,
                               std::initializer_list<double> locations,
                               int32_t angle,
                               bool radial) {
    return makeGradientValueImpl(colors, locations, angle, radial);
}

static Value makeGradientValue(std::initializer_list<int64_t> colors,
                               std::initializer_list<double> locations,
                               int32_t angle,
                               bool radial) {
    return makeGradientValueImpl(colors, locations, angle, radial);
}

static Ref<ColorPalette> makeTestColorPalette() {
    auto colorPalette = makeShared<ColorPalette>(STRING_LITERAL("default"));
    colorPalette->updateColors({
        {STRING_LITERAL("primary"), Color(0x11223344)},
        {STRING_LITERAL("secondary"), Color(0x55667788)},
    });
    return colorPalette;
}

TEST(AttributeProcessor, canParseSimpleBackground) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("red")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue({STRING_LITERAL("red")}, {}, 0, false), result.value());

    auto postprocessed = postprocessGradient(false, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0xFF0000FF}, {}, 0, false), postprocessed.value());
}

TEST(AttributeProcessor, failsOnTrailingInvalidKeyword) {
    auto result = preprocessGradient(Value(STRING_LITERAL("red wtf")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, canParseSimpleLinearGradient) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("linear-gradient(blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      STRING_LITERAL("blue"),
                      STRING_LITERAL("white"),
                      STRING_LITERAL("red"),
                  },
                  {},
                  0,
                  false),
              result.value());

    auto postprocessed = postprocessGradient(false, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0x0000FFFF, 0xFFFFFFFF, 0xFF0000FF}, {}, 0, false), postprocessed.value());
}

TEST(AttributeProcessor, canParseLinearGradientWithLocations) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("linear-gradient(blue 0, white 0.25, red 0.75)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      STRING_LITERAL("blue"),
                      STRING_LITERAL("white"),
                      STRING_LITERAL("red"),
                  },
                  {
                      0.0,
                      0.25,
                      0.75,
                  },
                  0,
                  false),
              result.value());

    auto postprocessed = postprocessGradient(false, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0x0000FFFF, 0xFFFFFFFF, 0xFF0000FF}, {0.0, 0.25, 0.75}, 0, false),
              postprocessed.value());
}

TEST(AttributeProcessor, failsWhenLinearGradientWithLocationsIsNotBalanced) {
    auto result = preprocessGradient(Value(STRING_LITERAL("linear-gradient(blue 0, white 0.25, red)")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, canParseAngleInLinearGradient) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("linear-gradient(45deg, blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      STRING_LITERAL("blue"),
                      STRING_LITERAL("white"),
                      STRING_LITERAL("red"),
                  },
                  {},
                  1,
                  false),
              result.value());

    auto postprocessed = postprocessGradient(true, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0x0000FFFF, 0xFFFFFFFF, 0xFF0000FF}, {}, 7, false), postprocessed.value());
}

TEST(AttributeProcessor, canParseAngleAsRadInLinearGradient) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("linear-gradient(1.6rad, blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      STRING_LITERAL("blue"),
                      STRING_LITERAL("white"),
                      STRING_LITERAL("red"),
                  },
                  {},
                  2,
                  false),
              result.value());

    auto postprocessed = postprocessGradient(true, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0x0000FFFF, 0xFFFFFFFF, 0xFF0000FF}, {}, 6, false), postprocessed.value());
}

TEST(AttributeProcessor, canParseSimpleRadialGradient) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("radial-gradient(blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      STRING_LITERAL("blue"),
                      STRING_LITERAL("white"),
                      STRING_LITERAL("red"),
                  },
                  {},
                  0,
                  true),
              result.value());

    auto postprocessed = postprocessGradient(false, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0x0000FFFF, 0xFFFFFFFF, 0xFF0000FF}, {}, 0, true), postprocessed.value());
}

TEST(AttributeProcessor, canParseRadialGradientWithLocations) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("radial-gradient(blue 0, white 0.25, red 0.75)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      STRING_LITERAL("blue"),
                      STRING_LITERAL("white"),
                      STRING_LITERAL("red"),
                  },
                  {
                      0.0,
                      0.25,
                      0.75,
                  },
                  0,
                  true),
              result.value());

    auto postprocessed = postprocessGradient(false, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed) << postprocessed.description();
    ASSERT_EQ(makeGradientValue({0x0000FFFF, 0xFFFFFFFF, 0xFF0000FF}, {0.0, 0.25, 0.75}, 0, true),
              postprocessed.value());
}

TEST(AttributeProcessor, failsWhenRadialGradientWithLocationsIsNotBalanced) {
    auto result = preprocessGradient(Value(STRING_LITERAL("radial-gradient(blue 0, white 0.25, red)")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, failsWhenRadialGradientHasAngle) {
    auto result = preprocessGradient(Value(STRING_LITERAL("radial-gradient(45deg, blue 0, white 0.25, red)")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, canParseBorderRadiusWithPoints) {
    auto result = ValueConverter::toBorderValues(Value(STRING_LITERAL("0")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(0, false), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(0, false), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(0, false), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(0, false), result.value()->getBottomLeft());

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("42")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(42, false), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(42, false), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(42, false), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(42, false), result.value()->getBottomLeft());

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("42 12")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(42, false), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(12, false), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(42, false), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(12, false), result.value()->getBottomLeft());

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("42 12 100")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(42, false), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(12, false), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(100, false), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(12, false), result.value()->getBottomLeft());

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("42 12 100 1337")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(42, false), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(12, false), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(100, false), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(1337, false), result.value()->getBottomLeft());
}

TEST(AttributeProcessor, canParseBorderRadiusWithPercent) {
    auto result = ValueConverter::toBorderValues(Value(STRING_LITERAL("10%")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(10, true), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(10, true), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(10, true), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(10, true), result.value()->getBottomLeft());

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("42 12% 100% 1337")));
    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(PercentValue(42, false), result.value()->getTopLeft());
    ASSERT_EQ(PercentValue(12, true), result.value()->getTopRight());
    ASSERT_EQ(PercentValue(100, true), result.value()->getBottomRight());
    ASSERT_EQ(PercentValue(1337, false), result.value()->getBottomLeft());
}

TEST(AttributeProcessor, failsParseBorderRadiusWithInvalidValues) {
    auto result = ValueConverter::toBorderValues(Value(STRING_LITERAL("%")));
    ASSERT_FALSE(result) << result.description();

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("42 12 100 1337 9")));
    ASSERT_FALSE(result) << result.description();

    result = ValueConverter::toBorderValues(Value(STRING_LITERAL("this is not a border")));
    ASSERT_FALSE(result) << result.description();
}

TEST(AttributeProcessor, flipsHorizontalBordersOnRTL) {
    auto result = preprocessBorderRadius(Value(STRING_LITERAL("42 12 100 1337")));
    ASSERT_TRUE(result) << result.description();

    auto borderRadius = result.value().getTypedRef<BorderRadius>();
    ASSERT_TRUE(borderRadius != nullptr);

    auto ltrResult = postprocessBorderRadius(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();

    ASSERT_EQ(result.value(), ltrResult.value());
    // Reference should be exactly the same
    ASSERT_EQ(borderRadius, ltrResult.value().getTypedRef<BorderRadius>());

    auto rtlResult = postprocessBorderRadius(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();

    ASSERT_NE(result.value(), rtlResult.value());
    auto rtlBorderRadius = rtlResult.value().getTypedRef<BorderRadius>();
    ASSERT_TRUE(rtlBorderRadius != nullptr);
    // Reference should be different
    ASSERT_NE(borderRadius, rtlBorderRadius);
    // Equality should be different
    ASSERT_NE(*borderRadius, *rtlBorderRadius);

    // Corners should be flipped
    ASSERT_EQ(borderRadius->getTopRight(), rtlBorderRadius->getTopLeft());
    ASSERT_EQ(borderRadius->getTopLeft(), rtlBorderRadius->getTopRight());
    ASSERT_EQ(borderRadius->getBottomLeft(), rtlBorderRadius->getBottomRight());
    ASSERT_EQ(borderRadius->getBottomRight(), rtlBorderRadius->getBottomLeft());
}

TEST(AttributeProcessor, postprocessGradientResolvesColors) {
    auto colorPalette = makeTestColorPalette();

    auto result = preprocessGradient(Value(STRING_LITERAL("linear-gradient(45deg, primary, secondary)")));
    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue({STRING_LITERAL("primary"), STRING_LITERAL("secondary")}, {}, 1, false),
              result.value());

    auto postprocessed = postprocessGradient(false, *colorPalette, result.value());
    ASSERT_TRUE(postprocessed.success()) << postprocessed.description();

    ASSERT_EQ(makeGradientValue({0x11223344, 0x55667788}, {}, 1, false), postprocessed.value());
}

} // namespace ValdiTest
