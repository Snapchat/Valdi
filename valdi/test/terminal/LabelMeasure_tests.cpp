#include "TerminalTestUtils.hpp"

#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"

#include <gtest/gtest.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

namespace {

class LabelMeasureFixture : public ::testing::Test {
protected:
    TerminalViewManager manager;
    Ref<ITerminalViewClass> labelClass = manager.getViewClass(STRING_LITERAL("SCValdiLabel"));

    Ref<ValueMap> attributes(const Value& value) {
        auto map = makeShared<ValueMap>();
        (*map)[STRING_LITERAL("value")] = value;
        return map;
    }

    Ref<ValueMap> attributes(const std::string& value, int numberOfLines) {
        auto map = attributes(Value(StringCache::getGlobal().makeString(value)));
        (*map)[STRING_LITERAL("numberOfLines")] = Value(static_cast<int32_t>(numberOfLines));
        return map;
    }

    Size measure(const Ref<ValueMap>& map, float width, MeasureMode mode) {
        return labelClass->onMeasure(map, width, mode, 0, MeasureModeUnspecified, false);
    }
};

} // namespace

TEST_F(LabelMeasureFixture, clippedSingleLinesMeasureTheClippedWidth) {
    auto map = attributes("hello wonderful world", 1);
    (*map)[STRING_LITERAL("textOverflow")] = Value(STRING_LITERAL("clip"));
    ASSERT_EQ(Size(8, 1), measure(map, 8, MeasureModeExactly));
}

TEST_F(LabelMeasureFixture, usesTheWidestLineWhenWidthIsUnspecified) {
    ASSERT_EQ(Size(11, 1), measure(attributes("hello world", 0), 0, MeasureModeUnspecified));
    ASSERT_EQ(Size(5, 2), measure(attributes("hello\nworld", 0), 0, MeasureModeUnspecified));
}

TEST_F(LabelMeasureFixture, wrapsAtTheAvailableWidth) {
    ASSERT_EQ(Size(5, 2), measure(attributes("hello world", 0), 5, MeasureModeAtMost));
    ASSERT_EQ(Size(5, 2), measure(attributes("hello world", 0), 5.9f, MeasureModeExactly));
    ASSERT_EQ(Size(9, 2), measure(attributes("the quick brown fox", 0), 10, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, defaultsToASingleEllipsizedLine) {
    auto map = attributes(Value(STRING_LITERAL("hello wonderful world")));
    ASSERT_EQ(Size(10, 1), measure(map, 10, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, limitsLinesWithNumberOfLines) {
    ASSERT_EQ(Size(8, 2), measure(attributes("the quick brown fox jumps", 2), 8, MeasureModeAtMost));
    ASSERT_EQ(Size(8, 3), measure(attributes("the quick brown fox jumps", 3), 8, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, zeroNumberOfLinesIsUnlimited) {
    ASSERT_EQ(Size(5, 5), measure(attributes("aaaaa bbbbb ccccc ddddd eeeee", 0), 5, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, measuresWideCharactersInCells) {
    ASSERT_EQ(Size(6, 1), measure(attributes("日本語", 0), 0, MeasureModeUnspecified));
    ASSERT_EQ(Size(4, 2), measure(attributes("日本語", 0), 4, MeasureModeAtMost));
    ASSERT_EQ(Size(4, 1), measure(attributes("a😀b", 0), 0, MeasureModeUnspecified));
}

TEST_F(LabelMeasureFixture, clipsInsteadOfEllipsizingWhenAsked) {
    auto map = attributes("the quick brown fox", 2);
    (*map)[STRING_LITERAL("textOverflow")] = Value(STRING_LITERAL("clip"));
    ASSERT_EQ(Size(5, 2), measure(map, 8, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, honorsAnExplicitTextWrap) {
    auto map = attributes("hello wonderful world", 0);
    (*map)[STRING_LITERAL("textWrap")] = Value(STRING_LITERAL("truncate-middle"));
    ASSERT_EQ(Size(10, 1), measure(map, 10, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, measuresAttributedText) {
    TextAttributeValue::Parts parts;
    TextAttributeValueStyle bold;
    bold.font = STRING_LITERAL("AvenirNext-Bold 16");
    parts.emplace_back(
        TextAttributeValuePart<TextAttributeValueStyle>{STRING_LITERAL("Status: "), TextAttributeValueStyle()});
    parts.emplace_back(TextAttributeValuePart<TextAttributeValueStyle>{STRING_LITERAL("all good"), bold});
    auto text = makeShared<TextAttributeValue>(std::move(parts));
    auto map = attributes(Value(Ref<ValdiObject>(text)));
    (*map)[STRING_LITERAL("numberOfLines")] = Value(static_cast<int32_t>(0));
    ASSERT_EQ(Size(16, 1), measure(map, 0, MeasureModeUnspecified));
    ASSERT_EQ(Size(8, 2), measure(map, 10, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, emptyTextIsOneRowTall) {
    ASSERT_EQ(Size(0, 1), measure(attributes("", 0), 10, MeasureModeAtMost));
}

TEST_F(LabelMeasureFixture, onlyLabelsAreMeasurable) {
    ASSERT_TRUE(labelClass->canBeMeasured());
    ASSERT_FALSE(manager.getViewClass(STRING_LITERAL("SCValdiView"))->canBeMeasured());
}

TEST(TerminalLabelFont, derivesWeightAndSlantFromFontNames) {
    ASSERT_TRUE(LabelClass::styleFromFontName("AvenirNext-Bold 16").bold);
    ASSERT_TRUE(LabelClass::styleFromFontName("AvenirNext-DemiBold 16").bold);
    ASSERT_TRUE(LabelClass::styleFromFontName("Helvetica-Heavy 12").bold);
    ASSERT_TRUE(LabelClass::styleFromFontName("Helvetica-Light 12").dim);
    ASSERT_TRUE(LabelClass::styleFromFontName("Helvetica-UltraLight 12").dim);
    ASSERT_TRUE(LabelClass::styleFromFontName("Georgia-Italic 12").italic);
    ASSERT_TRUE(LabelClass::styleFromFontName("Helvetica-BoldItalic 12").bold);
    ASSERT_TRUE(LabelClass::styleFromFontName("Helvetica-BoldItalic 12").italic);
    ASSERT_TRUE(LabelClass::styleFromFontName("Helvetica 12").isEmpty());
}

} // namespace ValdiTest
