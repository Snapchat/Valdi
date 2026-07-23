#include "valdi/runtime/Attributes/DefaultAttributeProcessors.hpp"
#include "valdi/runtime/Attributes/TransformAttributes.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "gtest/gtest.h"
#include <cmath>

using namespace Valdi;

namespace ValdiTest {

static auto kColorPalette = makeShared<ColorPalette>();

static Value makeGradientValue(std::vector<Color> colors, std::vector<double> locations, int32_t angle, bool radial) {
    auto outColors = ValueArray::make(colors.size());
    auto outLocations = ValueArray::make(locations.size());

    for (size_t i = 0; i < colors.size(); i++) {
        outColors->emplace(i, Value(colors[i].value));
    }
    for (size_t i = 0; i < locations.size(); i++) {
        outLocations->emplace(i, Value(locations[i]));
    }

    return Value(ValueArray::make({Value(outColors), Value(outLocations), Value(angle), Value(radial)}));
}

static Value makeTransformValue(const Value& translationX,
                                const Value& translationY,
                                const Value& scaleX,
                                const Value& scaleY,
                                const Value& rotation,
                                const Value& transformOrigin) {
    return Value(ValueArray::make({transformOrigin,
                                   Value::undefinedRef(),
                                   translationX,
                                   translationY,
                                   scaleX,
                                   scaleY,
                                   rotation}));
}

static Value makeTransformValue(double translationX,
                                double translationY,
                                double scaleX,
                                double scaleY,
                                double rotation,
                                const Value& transformOrigin = Value::undefinedRef()) {
    return makeTransformValue(Value(translationX),
                              Value(translationY),
                              Value(scaleX),
                              Value(scaleY),
                              Value(rotation),
                              transformOrigin);
}

static Value makeTransformStringValue(const Value& transform, const Value& transformOrigin = Value::undefinedRef()) {
    return Value(ValueArray::make({transformOrigin,
                                   transform,
                                   Value::undefinedRef(),
                                   Value::undefinedRef(),
                                   Value::undefinedRef(),
                                   Value::undefinedRef(),
                                   Value::undefinedRef()}));
}

static void expectTransformValues(const Value& value,
                                  double translationX,
                                  double translationY,
                                  double scaleX,
                                  double scaleY,
                                  double rotation) {
    const auto* values = value.getArray();
    ASSERT_NE(values, nullptr);
    ASSERT_EQ(values->size(), 5);
    EXPECT_NEAR((*values)[0].toDouble(), translationX, 0.00001);
    EXPECT_NEAR((*values)[1].toDouble(), translationY, 0.00001);
    EXPECT_NEAR((*values)[2].toDouble(), scaleX, 0.00001);
    EXPECT_NEAR((*values)[3].toDouble(), scaleY, 0.00001);
    EXPECT_NEAR((*values)[4].toDouble(), rotation, 0.00001);
}

TEST(AttributeProcessor, canParseSimpleBackground) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("red")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue({Color::rgba(255, 0, 0, 1.0)}, {}, 0, false), result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(result.value(), rtlResult.value());
}

TEST(AttributeProcessor, failsOnTrailingInvalidKeyword) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("red wtf")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, canParseSimpleLinearGradient) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("linear-gradient(blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {},
                  0,
                  false),
              result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(result.value(), rtlResult.value());
}

TEST(AttributeProcessor, canParseLinearGradientWithLocations) {
    auto result =
        preprocessGradient(kColorPalette, Value(STRING_LITERAL("linear-gradient(blue 0, white 0.25, red 0.75)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {
                      0.0,
                      0.25,
                      0.75,
                  },
                  0,
                  false),
              result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(result.value(), rtlResult.value());
}

TEST(AttributeProcessor, failsWhenLinearGradientWithLocationsIsNotBalanced) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("linear-gradient(blue 0, white 0.25, red)")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, canParseAngleInLinearGradient) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("linear-gradient(45deg, blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {},
                  1,
                  false),
              result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {},
                  7,
                  false),
              rtlResult.value());
}

TEST(AttributeProcessor, canParseAngleAsRadInLinearGradient) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("linear-gradient(1.6rad, blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {},
                  2,
                  false),
              result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {},
                  6,
                  false),
              rtlResult.value());
}

TEST(AttributeProcessor, canParseSimpleRadialGradient) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("radial-gradient(blue, white, red)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {},
                  0,
                  true),
              result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(result.value(), rtlResult.value());
}

TEST(AttributeProcessor, canParseRadialGradientWithLocations) {
    auto result =
        preprocessGradient(kColorPalette, Value(STRING_LITERAL("radial-gradient(blue 0, white 0.25, red 0.75)")));

    ASSERT_TRUE(result.success()) << result.description();

    ASSERT_EQ(makeGradientValue(
                  {
                      Color::rgba(0, 0, 255, 1.0),
                      Color::rgba(255, 255, 255, 1.0),
                      Color::rgba(255, 0, 0, 1.0),
                  },
                  {
                      0.0,
                      0.25,
                      0.75,
                  },
                  0,
                  true),
              result.value());

    auto ltrResult = postprocessGradient(false, result.value());
    ASSERT_TRUE(ltrResult) << ltrResult.description();
    ASSERT_EQ(result.value(), ltrResult.value());

    auto rtlResult = postprocessGradient(true, result.value());
    ASSERT_TRUE(rtlResult) << rtlResult.description();
    ASSERT_EQ(result.value(), rtlResult.value());
}

TEST(AttributeProcessor, failsWhenRadialGradientWithLocationsIsNotBalanced) {
    auto result = preprocessGradient(kColorPalette, Value(STRING_LITERAL("radial-gradient(blue 0, white 0.25, red)")));

    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, failsWhenRadialGradientHasAngle) {
    auto result =
        preprocessGradient(kColorPalette, Value(STRING_LITERAL("radial-gradient(45deg, blue 0, white 0.25, red)")));

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
    auto result = preprocessBorderRadius(nullptr, Value(STRING_LITERAL("42 12 100 1337")));
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

TEST(AttributeProcessor, transformAttributesPostprocessKeepsCenterOriginTransforms) {
    auto result = TransformAttributes::postprocess(100, 80, false, makeTransformValue(10, 20, 2, 3, 0.5));
    ASSERT_TRUE(result.success()) << result.description();

    expectTransformValues(result.value(), 10, 20, 2, 3, 0.5);
}

TEST(AttributeProcessor, transformAttributesPostprocessResolvesKeywordOrigins) {
    auto result =
        TransformAttributes::postprocess(100,
                                         80,
                                         false,
                                         makeTransformValue(0, 0, 2, 3, 0, Value(STRING_LITERAL("top left"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), 50, 80, 2, 3, 0);

    result =
        TransformAttributes::postprocess(100,
                                         80,
                                         false,
                                         makeTransformValue(0, 0, 2, 2, 0, Value(STRING_LITERAL("right bottom"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), -50, -40, 2, 2, 0);
}

TEST(AttributeProcessor, transformAttributesPostprocessResolvesLengthAndPercentOrigins) {
    auto result =
        TransformAttributes::postprocess(100,
                                         80,
                                         false,
                                         makeTransformValue(0, 0, 2, 2, 0, Value(STRING_LITERAL("50px 70px"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), 0, -30, 2, 2, 0);

    result =
        TransformAttributes::postprocess(100,
                                         80,
                                         false,
                                         makeTransformValue(0, 0, 2, 2, 0, Value(STRING_LITERAL("25% 75%"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), 25, -20, 2, 2, 0);
}

TEST(AttributeProcessor, transformAttributesPostprocessResolvesPercentOriginAgainstFrame) {
    auto result =
        TransformAttributes::postprocess(200,
                                         80,
                                         false,
                                         makeTransformValue(0, 0, 1.5, 0.5, 0, Value(STRING_LITERAL("10% 25%"))));
    ASSERT_TRUE(result.success()) << result.description();

    expectTransformValues(result.value(), 40, -10, 1.5, 0.5, 0);
}

TEST(AttributeProcessor, transformAttributesPostprocessResolvesTranslationPercentagesAgainstFrame) {
    auto result = TransformAttributes::postprocess(100,
                                                   80,
                                                   false,
                                                   makeTransformValue(Value(STRING_LITERAL("10%")),
                                                                      Value(STRING_LITERAL("25%")),
                                                                      Value(1.0),
                                                                      Value(1.0),
                                                                      Value(0.0),
                                                                      Value::undefinedRef()));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), 10, 20, 1, 1, 0);
}

TEST(AttributeProcessor, transformAttributesPostprocessParsesWebTransformStrings) {
    auto result = TransformAttributes::postprocess(
        100,
        80,
        false,
        makeTransformStringValue(Value(STRING_LITERAL("translate(50%, -25%) translateX(10px) translateY(5pt)"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), 60, -15, 1, 1, 0);

    result = TransformAttributes::postprocess(
        100,
        80,
        false,
        makeTransformStringValue(Value(STRING_LITERAL("translate(10px, 20px) rotate(90deg) scale(2, 3)"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), 10, 20, 2, 3, M_PI_2);
}

TEST(AttributeProcessor, transformAttributesPostprocessAppliesOriginToWebTransformStrings) {
    auto result = TransformAttributes::postprocess(
        100,
        100,
        false,
        makeTransformStringValue(Value(STRING_LITERAL("rotate(90deg)")), Value(STRING_LITERAL("top left"))));
    ASSERT_TRUE(result.success()) << result.description();

    expectTransformValues(result.value(), -100, 0, 1, 1, M_PI_2);
}

TEST(AttributeProcessor, transformAttributesPostprocessAppliesRtlToWebTransformStrings) {
    auto result =
        TransformAttributes::postprocess(
            100, 80, true, makeTransformStringValue(Value(STRING_LITERAL("translateX(50%) rotate(90deg)"))));
    ASSERT_TRUE(result.success()) << result.description();
    expectTransformValues(result.value(), -50, 0, 1, 1, -M_PI_2);
}

TEST(AttributeProcessor, transformAttributesPostprocessRejectsInvalidWebTransformStrings) {
    auto result = TransformAttributes::postprocess(
        100, 80, false, makeTransformStringValue(Value(STRING_LITERAL("translateX(50%) nope(1)"))));
    ASSERT_FALSE(result.success()) << result.description();

    result = TransformAttributes::postprocess(
        100, 80, false, makeTransformStringValue(Value(STRING_LITERAL("translateX(10px"))));
    ASSERT_FALSE(result.success()) << result.description();
}

TEST(AttributeProcessor, transformAttributesPostprocessFoldsRotationAroundNonCenterOrigin) {
    auto result = TransformAttributes::postprocess(
        100, 100, false, makeTransformValue(0, 0, 1, 1, M_PI_2, Value(STRING_LITERAL("top left"))));
    ASSERT_TRUE(result.success()) << result.description();

    expectTransformValues(result.value(), -100, 0, 1, 1, M_PI_2);
}

TEST(AttributeProcessor, transformAttributesPostprocessPreservesExistingRtlBehavior) {
    auto result = TransformAttributes::postprocess(100, 100, true, makeTransformValue(10, 20, 1, 1, M_PI_2));
    ASSERT_TRUE(result.success()) << result.description();

    expectTransformValues(result.value(), -10, 20, 1, 1, -M_PI_2);
}

TEST(AttributeProcessor, transformAttributesPostprocessRejectsInvalidOrigins) {
    auto result =
        TransformAttributes::postprocess(100,
                                         100,
                                         false,
                                         makeTransformValue(0, 0, 1, 1, 0, Value(STRING_LITERAL("top bottom"))));
    ASSERT_FALSE(result.success()) << result.description();

    result = TransformAttributes::postprocess(100,
                                              100,
                                              false,
                                              makeTransformValue(0, 0, 1, 1, 0, Value(STRING_LITERAL("10px 20px 0"))));
    ASSERT_FALSE(result.success()) << result.description();

    result = TransformAttributes::postprocess(100,
                                              100,
                                              false,
                                              makeTransformValue(0, 0, 1, 1, 0, Value(STRING_LITERAL("10 20"))));
    ASSERT_FALSE(result.success()) << result.description();
}

} // namespace ValdiTest
