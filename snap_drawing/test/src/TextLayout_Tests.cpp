#include <gtest/gtest.h>

#include "snap_drawing/cpp/Layers/EditableTextLayer.hpp"
#include "snap_drawing/cpp/Layers/TextLayer.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Text/AttributedText.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/TextLayout.hpp"
#include "snap_drawing/cpp/Text/TextLayoutBuilder.hpp"
#include "snap_drawing/cpp/Text/TextShaper.hpp"
#include "snap_drawing/cpp/Utils/JSONUtils.hpp"
#include "snap_drawing/cpp/Utils/UTFUtils.hpp"
#include "valdi_core/cpp/Attributes/TextInlineAttachment.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include "TestDataUtils.hpp"
#include "TestFontUtils.hpp"
#include "hb.h"

using namespace Valdi;

namespace snap::drawing {

struct TextLayoutTestContainer {
    Ref<FontManager> fontManager;

    Ref<Font> primaryFont;
    Ref<Font> arabicFont;

    TextLayoutTestContainer() {
        fontManager = makeShared<FontManager>(ConsoleLogger::getLogger());
        fontManager->load();

        primaryFont = loadTestFont(fontManager,
                                   "Test Sans",
                                   FontStyle(FontWidthNormal, FontWeightNormal, FontSlantUpright),
                                   "NotoSans-Regular.ttf");

        auto arabicText = utf8ToUnicode("ر");
        SC_ASSERT(arabicText.size() == 1);

        auto arabicFont = fontManager->getCompatibleFont(primaryFont, nullptr, 0, arabicText[0]);
        SC_ASSERT(arabicFont.success(), arabicFont.description());
        this->arabicFont = arabicFont.moveValue();
        SC_ASSERT(this->primaryFont->getDescription() != this->arabicFont->getDescription());
    }

    Ref<Font> registerFont(std::string_view fontFamilyName,
                           FontStyle fontStyle,
                           std::string_view fontName,
                           const std::string& filename,
                           bool canUseAsFallback) {
        auto testData = getTestData(filename + ".ttf");
        SC_ASSERT(testData.success(), testData.description());

        fontManager->registerTypeface(
            Valdi::StringCache::getGlobal().makeString(fontFamilyName), fontStyle, canUseAsFallback, testData.value());

        auto font = fontManager->getFontWithNameAndSize(
            Valdi::StringCache::getGlobal().makeString(fontName), 17, 1.0, true);
        SC_ASSERT(font.success(), font.description());
        return font.moveValue();
    }

    Ref<Font> getAppleEmojiFont() {
        auto font = fontManager->getFontWithNameAndSize(
            Valdi::StringCache::getGlobal().makeString(std::string_view("Apple Color Emoji")), 17, 1.0, true);
        SC_ASSERT(font.success(), font.description());
        return font.moveValue();
    }

    Ref<Font> getDevanagariFont() {
        auto font = fontManager->getFontWithNameAndSize(
            Valdi::StringCache::getGlobal().makeString(std::string_view("Kohinoor Devanagari")), 17, 1.0, true);
        SC_ASSERT(font.success(), font.description());
        return font.moveValue();
    }

    Ref<Font> getSinhalaFont() {
        auto font = fontManager->getFontWithNameAndSize(
            Valdi::StringCache::getGlobal().makeString(std::string_view("Sinhala Sangam MN")), 17, 1.0, true);
        SC_ASSERT(font.success(), font.description());
        return font.moveValue();
    }
};

class MutableInlineAttachment final : public TextInlineAttachment {
public:
    MutableInlineAttachment(size_t childIndex, const Valdi::Size& size) : TextInlineAttachment(childIndex), _size(size) {}

    Valdi::Size getSize() const override {
        return _size;
    }

    void setSize(const Valdi::Size& size) {
        _size = size;
    }

private:
    Valdi::Size _size;
};

Valdi::Value makeTextLayoutJSON(Size maxSize,
                                std::initializer_list<TextLayoutEntry> entries,
                                std::initializer_list<TextLayoutVisualEntry> visualEntries = {}) {
    return TextLayout(maxSize,
                      std::vector<TextLayoutEntry>(entries),
                      std::vector<TextLayoutVisualEntry>(visualEntries),
                      {},
                      true)
        .toJSONValue();
}

struct TextShaperTestContainer {
    Ref<TextShaper> textShaper;
    TextSegmentProperties segment;
    std::vector<TextSegmentProperties> segments;

    TextShaperTestContainer() : segment(false, 0, 0, TextScript::invalid()) {
        textShaper = TextShaper::make(true);
    }

    void resolveSingleSegmentTextProperties(std::string str, bool isRightToLeft, bool expectsResolvedAsRightToLeft) {
        resolveMultipleSegmentsTextProperties(str, isRightToLeft, expectsResolvedAsRightToLeft);
        ASSERT_EQ(static_cast<size_t>(1), segments.size());

        segment = segments[0];
    }

    void resolveMultipleSegmentsTextProperties(std::string str, bool isRightToLeft, bool expectsResolvedAsRightToLeft) {
        segments.clear();

        auto unicode = utf8ToUnicode(str);
        auto paragraphList = textShaper->resolveParagraphs(unicode.data(), unicode.size(), isRightToLeft);

        ASSERT_EQ(static_cast<size_t>(1), paragraphList.size());
        ASSERT_EQ(expectsResolvedAsRightToLeft, paragraphList[0].baseDirectionIsRightToLeft);

        for (const auto& segment : paragraphList[0].segments) {
            segments.emplace_back(segment);
        }
    }
};

TEST(TextShaper, resolvesLTRInLTRContext) {
    TextShaperTestContainer testContainer;
    auto& segment = testContainer.segment;

    testContainer.resolveSingleSegmentTextProperties("hello", false, false);

    ASSERT_FALSE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(5), segment.end);

    testContainer.resolveSingleSegmentTextProperties("hello word", false, false);

    ASSERT_FALSE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(10), segment.end);

    testContainer.resolveSingleSegmentTextProperties("  hello world  ", false, false);

    ASSERT_FALSE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(15), segment.end);
}

TEST(TextShaper, resolvesRTLInLTRContext) {
    TextShaperTestContainer testContainer;
    auto& segment = testContainer.segment;

    testContainer.resolveSingleSegmentTextProperties("مصر", false, true);

    ASSERT_TRUE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(3), segment.end);

    testContainer.resolveSingleSegmentTextProperties("مصر مصر", false, true);

    ASSERT_TRUE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(7), segment.end);

    testContainer.resolveSingleSegmentTextProperties("  مصر مصر  ", false, true);

    ASSERT_TRUE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(11), segment.end);
}

TEST(TextShaper, resolvesLTRinRTLContext) {
    TextShaperTestContainer testContainer;
    auto& segment = testContainer.segment;

    testContainer.resolveSingleSegmentTextProperties("hello", true, false);

    ASSERT_FALSE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(5), segment.end);

    testContainer.resolveSingleSegmentTextProperties("hello world", true, false);

    ASSERT_FALSE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(11), segment.end);

    testContainer.resolveSingleSegmentTextProperties("  hello world  ", true, false);

    ASSERT_FALSE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(15), segment.end);
}

TEST(TextShaper, resolvesRTLInRTLContext) {
    TextShaperTestContainer testContainer;
    auto& segment = testContainer.segment;

    testContainer.resolveSingleSegmentTextProperties("مصر", true, true);

    ASSERT_TRUE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(3), segment.end);

    testContainer.resolveSingleSegmentTextProperties("مصر مصر", true, true);

    ASSERT_TRUE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(7), segment.end);

    testContainer.resolveSingleSegmentTextProperties("  مصر مصر  ", true, true);

    ASSERT_TRUE(segment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), segment.start);
    ASSERT_EQ(static_cast<size_t>(11), segment.end);
}

TEST(TextShaper, resolvesBidiInLTRContext) {
    TextShaperTestContainer testContainer;

    testContainer.resolveMultipleSegmentsTextProperties("bahrain مصر kuwait", false, false);

    ASSERT_EQ(static_cast<size_t>(3), testContainer.segments.size());

    auto firstSegment = testContainer.segments[0];
    auto secondSegment = testContainer.segments[1];
    auto thirdSegment = testContainer.segments[2];

    ASSERT_FALSE(firstSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), firstSegment.start);
    ASSERT_EQ(static_cast<size_t>(8), firstSegment.end);

    ASSERT_TRUE(secondSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(8), secondSegment.start);
    ASSERT_EQ(static_cast<size_t>(11), secondSegment.end);

    ASSERT_FALSE(thirdSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(11), thirdSegment.start);
    ASSERT_EQ(static_cast<size_t>(18), thirdSegment.end);

    testContainer.resolveMultipleSegmentsTextProperties("  bahrain مصر kuwait  ", false, false);

    ASSERT_EQ(static_cast<size_t>(3), testContainer.segments.size());

    firstSegment = testContainer.segments[0];
    secondSegment = testContainer.segments[1];
    thirdSegment = testContainer.segments[2];

    ASSERT_FALSE(firstSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), firstSegment.start);
    ASSERT_EQ(static_cast<size_t>(10), firstSegment.end);

    ASSERT_TRUE(secondSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(10), secondSegment.start);
    ASSERT_EQ(static_cast<size_t>(13), secondSegment.end);

    ASSERT_FALSE(thirdSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(13), thirdSegment.start);
    ASSERT_EQ(static_cast<size_t>(22), thirdSegment.end);
}

TEST(TextShaper, resolvesBidiInRTLContext) {
    TextShaperTestContainer testContainer;

    testContainer.resolveMultipleSegmentsTextProperties("bahrain مصر kuwait", true, false);

    ASSERT_EQ(static_cast<size_t>(3), testContainer.segments.size());

    auto firstSegment = testContainer.segments[0];
    auto secondSegment = testContainer.segments[1];
    auto thirdSegment = testContainer.segments[2];

    ASSERT_FALSE(firstSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), firstSegment.start);
    ASSERT_EQ(static_cast<size_t>(8), firstSegment.end);

    ASSERT_TRUE(secondSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(8), secondSegment.start);
    ASSERT_EQ(static_cast<size_t>(11), secondSegment.end);

    ASSERT_FALSE(thirdSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(11), thirdSegment.start);
    ASSERT_EQ(static_cast<size_t>(18), thirdSegment.end);

    testContainer.resolveMultipleSegmentsTextProperties("  bahrain مصر kuwait  ", true, false);

    ASSERT_EQ(static_cast<size_t>(3), testContainer.segments.size());

    firstSegment = testContainer.segments[0];
    secondSegment = testContainer.segments[1];
    thirdSegment = testContainer.segments[2];

    ASSERT_FALSE(firstSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(0), firstSegment.start);
    ASSERT_EQ(static_cast<size_t>(10), firstSegment.end);

    ASSERT_TRUE(secondSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(10), secondSegment.start);
    ASSERT_EQ(static_cast<size_t>(13), secondSegment.end);

    ASSERT_FALSE(thirdSegment.isRTL);
    ASSERT_EQ(static_cast<size_t>(13), thirdSegment.start);
    ASSERT_EQ(static_cast<size_t>(22), thirdSegment.end);
}

TEST(TestShaper, resolvesParagraphs) {
    TextShaperTestContainer testContainer;
    auto singleLine = utf8ToUnicode("This is a single line");

    auto paragraphs = testContainer.textShaper->resolveParagraphs(singleLine.data(), singleLine.size(), false);

    ASSERT_EQ(static_cast<size_t>(1), paragraphs.size());

    ASSERT_EQ(static_cast<size_t>(1), paragraphs[0].segments.size());
    ASSERT_EQ(static_cast<size_t>(0), paragraphs[0].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(21), paragraphs[0].segments[0].end);

    auto twoLines = utf8ToUnicode("Line1\nLineTwo");
    paragraphs = testContainer.textShaper->resolveParagraphs(twoLines.data(), twoLines.size(), false);

    ASSERT_EQ(static_cast<size_t>(1), paragraphs.size());

    ASSERT_EQ(static_cast<size_t>(1), paragraphs[0].segments.size());
    ASSERT_EQ(static_cast<size_t>(0), paragraphs[0].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(13), paragraphs[0].segments[0].end);

    auto groupedLines = utf8ToUnicode("\n\n\nLine1\n\nLineTwo\n\n\n\n");
    paragraphs = testContainer.textShaper->resolveParagraphs(groupedLines.data(), groupedLines.size(), false);
    ASSERT_EQ(static_cast<size_t>(1), paragraphs.size());

    ASSERT_EQ(static_cast<size_t>(1), paragraphs[0].segments.size());
    ASSERT_EQ(static_cast<size_t>(0), paragraphs[0].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(21), paragraphs[0].segments[0].end);

    auto differentDirectionParagraphs = utf8ToUnicode("\n\n\nLine1\n\nقرأ Wikipedia™ طوال اليوم\n\n\n\n");
    paragraphs = testContainer.textShaper->resolveParagraphs(
        differentDirectionParagraphs.data(), differentDirectionParagraphs.size(), false);
    ASSERT_EQ(static_cast<size_t>(3), paragraphs.size());

    ASSERT_EQ(static_cast<size_t>(1), paragraphs[0].segments.size());
    ASSERT_EQ(static_cast<size_t>(0), paragraphs[0].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(10), paragraphs[0].segments[0].end);

    ASSERT_EQ(static_cast<size_t>(3), paragraphs[1].segments.size());
    ASSERT_EQ(static_cast<size_t>(23), paragraphs[1].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(36), paragraphs[1].segments[0].end);
    ASSERT_EQ(static_cast<size_t>(14), paragraphs[1].segments[1].start);
    ASSERT_EQ(static_cast<size_t>(23), paragraphs[1].segments[1].end);
    ASSERT_EQ(static_cast<size_t>(10), paragraphs[1].segments[2].start);
    ASSERT_EQ(static_cast<size_t>(14), paragraphs[1].segments[2].end);

    ASSERT_EQ(static_cast<size_t>(1), paragraphs[2].segments.size());
    ASSERT_EQ(static_cast<size_t>(36), paragraphs[2].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(39), paragraphs[2].segments[0].end);
}

TEST(TestShaper, separatesParagraphSegmentsByScript) {
    TextShaperTestContainer testContainer;

    auto singleLine = utf8ToUnicode("123 Hello 456 क्‍ष ශ්‍ර");

    auto paragraphs = testContainer.textShaper->resolveParagraphs(singleLine.data(), singleLine.size(), false);

    ASSERT_EQ(static_cast<size_t>(1), paragraphs.size());

    ASSERT_EQ(static_cast<size_t>(3), paragraphs[0].segments.size());
    ASSERT_EQ(HB_SCRIPT_LATIN, paragraphs[0].segments[0].script.code);
    ASSERT_EQ(static_cast<size_t>(0), paragraphs[0].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(14), paragraphs[0].segments[0].end);
    ASSERT_EQ(HB_SCRIPT_DEVANAGARI, paragraphs[0].segments[1].script.code);
    ASSERT_EQ(static_cast<size_t>(14), paragraphs[0].segments[1].start);
    ASSERT_EQ(static_cast<size_t>(19), paragraphs[0].segments[1].end);
    ASSERT_EQ(HB_SCRIPT_SINHALA, paragraphs[0].segments[2].script.code);
    ASSERT_EQ(static_cast<size_t>(19), paragraphs[0].segments[2].start);
    ASSERT_EQ(static_cast<size_t>(23), paragraphs[0].segments[2].end);
}

TEST(TestShaper, associatesWeakScriptWithCommon) {
    TextShaperTestContainer testContainer;

    auto singleLine = utf8ToUnicode("123 456 ");

    auto paragraphs = testContainer.textShaper->resolveParagraphs(singleLine.data(), singleLine.size(), false);

    ASSERT_EQ(static_cast<size_t>(1), paragraphs.size());

    ASSERT_EQ(static_cast<size_t>(1), paragraphs[0].segments.size());
    ASSERT_EQ(HB_SCRIPT_COMMON, paragraphs[0].segments[0].script.code);
    ASSERT_EQ(static_cast<size_t>(0), paragraphs[0].segments[0].start);
    ASSERT_EQ(static_cast<size_t>(8), paragraphs[0].segments[0].end);
}

// Since we rely on the toJSON to simplify checks, we do this test upfront to confirm that the json conversions work
TEST(TextLayout, canBeConvertedToJSON) {
    TextLayoutTestContainer testContainer;

    auto size = Size::make(13, 24);
    auto bounds = Rect::makeXYWH(1, 2, 3, 4);
    auto segmentsBounds = Rect::makeXYWH(4, 3, 2, 1);
    auto decorationBounds = Rect::makeXYWH(9, 8, 7, 6);

    auto segments = ValueArray::make(1);
    segments->emplace(0,
                      Value()
                          .setMapValue("bounds", toJSONValue(segmentsBounds))
                          .setMapValue("characters", Value("Hello world"))
                          .setMapValue("font", Value(testContainer.primaryFont->getDescription())));

    auto visualEntries = ValueArray::make(1);
    visualEntries->emplace(
        0, Value().setMapValue("bounds", toJSONValue(decorationBounds)).setMapValue("predraw", Value(true)));

    auto entries = ValueArray::make(1);

    entries->emplace(0, Value().setMapValue("bounds", toJSONValue(bounds)).setMapValue("segments", Value(segments)));

    auto expectedJSON = Value()
                            .setMapValue("maxSize", toJSONValue(size))
                            .setMapValue("entries", Value(entries))
                            .setMapValue("visualEntries", Value(visualEntries));

    ASSERT_EQ(expectedJSON,
              makeTextLayoutJSON(
                  size,
                  {TextLayoutEntry(bounds,
                                   std::nullopt,
                                   {TextLayoutEntrySegment(segmentsBounds, testContainer.primaryFont, "Hello world")})},
                  {TextLayoutVisualEntry(decorationBounds, true, std::nullopt, TextLayoutDecorationStyleSolid)}));

    ASSERT_NE(
        expectedJSON,
        makeTextLayoutJSON(
            size,
            {TextLayoutEntry(
                bounds, std::nullopt, {TextLayoutEntrySegment(segmentsBounds, testContainer.primaryFont, "Not good")})},
            {TextLayoutVisualEntry(decorationBounds, true, std::nullopt, TextLayoutDecorationStyleSolid)}));

    ASSERT_NE(
        expectedJSON,
        makeTextLayoutJSON(
            size,
            {TextLayoutEntry(bounds,
                             std::nullopt,
                             {TextLayoutEntrySegment(segmentsBounds, testContainer.primaryFont, "Hello world")})}));

    ASSERT_NE(
        expectedJSON,
        makeTextLayoutJSON(
            size,
            {TextLayoutEntry(bounds,
                             std::nullopt,
                             {TextLayoutEntrySegment(segmentsBounds, testContainer.primaryFont, "Hello world")})}));

    ASSERT_NE(expectedJSON,
              makeTextLayoutJSON(size,
                                 {TextLayoutEntry(bounds, std::nullopt, {})},
                                 {TextLayoutVisualEntry(decorationBounds, true, std::nullopt, TextLayoutDecorationStyleSolid)}));

    ASSERT_NE(expectedJSON,
              makeTextLayoutJSON(
                  size,
                  {TextLayoutEntry(Rect::makeXYWH(0, 0, 0, 0),
                                   std::nullopt,
                                   {TextLayoutEntrySegment(segmentsBounds, testContainer.primaryFont, "Hello world")})},
                  {TextLayoutVisualEntry(decorationBounds, true, std::nullopt, TextLayoutDecorationStyleSolid)}));
}

TEST(TextLayout, canLayoutSingleLine) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello World!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    auto expectedBounds = Rect::makeXYWH(0, 0, 97.0, 23.154000);
    auto segmentBounds = expectedBounds;

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(expectedBounds,
                                   std::nullopt,
                                   {TextLayoutEntrySegment(segmentBounds, testContainer.primaryFont, "Hello World!")})}),
              layout->toJSONValue());
}

TEST(TextLayout, canLayoutDashedUnderlineDecoration) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);

    builder.append("Hello", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationDashedUnderline);

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();

    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());
    ASSERT_EQ(TextLayoutVisualEntryKindDecoration, visualEntries[0].kind);
    ASSERT_EQ(TextLayoutDecorationStyleDashed, visualEntries[0].style);
    ASSERT_TRUE(visualEntries[0].predraw);
    ASSERT_NEAR(testContainer.primaryFont->metrics().underlineThickness, visualEntries[0].bounds.height(), 0.0001f);
}

TEST(TextLayout, canLayoutDottedUnderlineDecoration) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);

    builder.append("Hello", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationDottedUnderline);

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();

    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());
    ASSERT_EQ(TextLayoutVisualEntryKindDecoration, visualEntries[0].kind);
    ASSERT_EQ(TextLayoutDecorationStyleDotted, visualEntries[0].style);
    ASSERT_TRUE(visualEntries[0].predraw);
    ASSERT_NEAR(2.0f, visualEntries[0].bounds.height(), 0.0001f);
    ASSERT_NEAR(-testContainer.primaryFont->metrics().ascent + 2.5f,
                visualEntries[0].bounds.y() + visualEntries[0].bounds.height() / 2.0f,
                0.0001f);
}

TEST(TextLayout, canLayoutCustomUnderlineDecoration) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    TextCustomUnderlineStyle style(1.0f, 1.0f, 1.0f, -2.0f);

    builder.append("Hello",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationDottedUnderline,
                   nullptr,
                   std::nullopt,
                   std::nullopt,
                   style);

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();
    const auto& metrics = testContainer.primaryFont->metrics();

    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());
    ASSERT_EQ(TextLayoutVisualEntryKindDecoration, visualEntries[0].kind);
    ASSERT_TRUE(visualEntries[0].customUnderlineStyle.has_value());
    ASSERT_EQ(style, visualEntries[0].customUnderlineStyle.value());
    ASSERT_NEAR(1.0f, visualEntries[0].bounds.height(), 0.0001f);
    ASSERT_NEAR(-metrics.ascent + metrics.descent / 2.0f - 2.0f,
                visualEntries[0].bounds.y() + visualEntries[0].bounds.height() / 2.0f,
                0.0001f);
}

TEST(TextLayout, customUnderlineHeightDoesNotMoveDecorationCenter) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder thinBuilder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    TextLayoutBuilder thickBuilder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    TextCustomUnderlineStyle thinStyle(1.0f, 0.0f, 0.0f, -2.0f);
    TextCustomUnderlineStyle thickStyle(8.0f, 0.0f, 0.0f, -2.0f);

    thinBuilder.append("Hello",
                       testContainer.primaryFont,
                       TextLayoutLineHeight::multiple(1.0),
                       0.0,
                       TextDecorationUnderline,
                       nullptr,
                       std::nullopt,
                       std::nullopt,
                       thinStyle);
    thickBuilder.append("Hello",
                        testContainer.primaryFont,
                        TextLayoutLineHeight::multiple(1.0),
                        0.0,
                        TextDecorationUnderline,
                        nullptr,
                        std::nullopt,
                        std::nullopt,
                        thickStyle);

    auto thinLayout = thinBuilder.build();
    auto thickLayout = thickBuilder.build();
    const auto& thinEntries = thinLayout->getVisualEntries();
    const auto& thickEntries = thickLayout->getVisualEntries();

    ASSERT_EQ(static_cast<size_t>(1), thinEntries.size());
    ASSERT_EQ(static_cast<size_t>(1), thickEntries.size());
    ASSERT_NEAR(1.0f, thinEntries[0].bounds.height(), 0.0001f);
    ASSERT_NEAR(8.0f, thickEntries[0].bounds.height(), 0.0001f);
    ASSERT_NEAR(thinEntries[0].bounds.y() + thinEntries[0].bounds.height() / 2.0f,
                thickEntries[0].bounds.y() + thickEntries[0].bounds.height() / 2.0f,
                0.0001f);
}

TEST(TextLayout, canLayoutCustomSolidUnderlineDecoration) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    TextCustomUnderlineStyle style(1.0f, 0.0f, 0.0f, -2.0f);

    builder.append("Hello",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationUnderline,
                   nullptr,
                   std::nullopt,
                   std::nullopt,
                   style);

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();

    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());
    ASSERT_TRUE(visualEntries[0].customUnderlineStyle.has_value());
    ASSERT_FALSE(visualEntries[0].customUnderlineStyle->isPatterned());
    ASSERT_NEAR(1.0f, visualEntries[0].bounds.height(), 0.0001f);
}

TEST(TextLayout, canLayoutMultipleLinesWithExplicitNewLines) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world\nand welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0, 111.000000, 46.308000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 89.000000, 23.154000), testContainer.primaryFont, "Hello world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 23.154000, 111.000000, 23.154000), testContainer.primaryFont, "and welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, canLayoutWithExplicitTrailingNewLines) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world\n\n\n", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(maxSize,
                                 {TextLayoutEntry(Rect::makeXYWH(0, 0, 89.000000, 92.616000),
                                                  std::nullopt,
                                                  {
                                                      TextLayoutEntrySegment(Rect::makeXYWH(0, 0.0, 89.000000, 23.154000),
                                                                             testContainer.primaryFont,
                                                                             "Hello world"),
                                                  })}),
              layout->toJSONValue());
}

TEST(TextLayout, canLayoutMultipleLinesByWordBreak) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 111.000000, 46.308000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 89.000000, 23.154000), testContainer.primaryFont, "Hello world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 23.154000, 111.000000, 23.154000), testContainer.primaryFont, "and welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, canLayoutMultipleLinesByCharacterBreak) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append(
        "Thisisaverylongtextthatcannotfitinasingleline", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    builder.setLineBreakStrategy(LineBreakStrategy::ByCharacter);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 114.0, 92.616000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 114.000000, 23.154000), testContainer.primaryFont, "Thisisaverylon"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 23.154000, 114.000000, 23.154000),
                        testContainer.primaryFont,
                        "gtextthatcann"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 46.308000, 112.000000, 23.154000), testContainer.primaryFont, "otftinasingleli"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 69.462000, 21.000000, 23.154000), testContainer.primaryFont, "ne"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, addsEllipsisWhenDoesntFit) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(80, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("This text will not fit", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_FALSE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 80.000000, 23.154000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 67.000000, 23.154000), testContainer.primaryFont, "This text"),
                    TextLayoutEntrySegment(Rect::makeXYWH(67.000000, 0.0, 13.000000, 23.154000), testContainer.primaryFont, "…"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, breaksWordByCharacterWhenDoesntFit) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(80, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append(
        "Thisisaverylongtextthatcannotfitinasingleline", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_FALSE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 76.000000, 23.154000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 63.000000, 23.154000), testContainer.primaryFont, "Thisisav"),
                    TextLayoutEntrySegment(Rect::makeXYWH(63.000000, 0.0, 13.000000, 23.154000), testContainer.primaryFont, "…"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsMultipleAppendWithDifferentFonts) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    auto scaledFont = testContainer.primaryFont->withScale(0.5);
    builder.append("Hello ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);
    builder.append("World", scaledFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {
                TextLayoutEntry(
                    Rect::makeXYWH(0, 0.000000, 69.000000, 23.154000),
                    std::nullopt,
                    {
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(0, 0.0, 45.000000, 23.154000), testContainer.primaryFont, "Hello "),
                        TextLayoutEntrySegment(Rect::makeXYWH(45.000000, 0.0, 24.000000, 11.577000), scaledFont, "World"),
                    }),
            }),
        layout->toJSONValue());
}

TEST(TextLayout, supportsRightTextAlignment) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(9.000000, 0.000000, 111.000000, 46.308000),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(31.000000, 0.000000, 89.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "Hello world"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(9.000000, 23.154000, 111.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "and welcome!"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsCenteredTextAlignment) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(4.500000, 0.000000, 111.000000, 46.308000),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(15.500000, 0.000000, 89.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "Hello world"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(4.500000, 23.154000, 111.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "and welcome!"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsJustifiedTextAlignment) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignJustify, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000000, 0.000000, 120.000000, 46.308000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.000000, 0.000000, 41.000000, 23.154000), testContainer.primaryFont, "Hello"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(76.000000, 0.000000, 44.000000, 23.154000), testContainer.primaryFont, "world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.000000, 23.154000, 31.000000, 23.154000), testContainer.primaryFont, "and"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(44.000000, 23.154000, 76.000000, 23.154000), testContainer.primaryFont, "welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsLineHeightMultiple) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(4.500000, 0.0, 111.000000, 69.462000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(15.500000, 0.000000, 89.000000, 34.731000), testContainer.primaryFont, "Hello world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(4.500000, 34.731000, 111.000000, 34.731000), testContainer.primaryFont, "and welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsExplicitLineHeightWithDifferentFontScales) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    auto scaledFont = testContainer.primaryFont->withScale(0.5);
    auto lineHeight = TextLayoutLineHeight::absolute(24.0f);
    builder.append("Hello ", testContainer.primaryFont, lineHeight, 0.0, TextDecorationNone);
    builder.append("World", scaledFont, lineHeight, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 69.000000, 24.000000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 45.000000, 24.000000), testContainer.primaryFont, "Hello "),
                    TextLayoutEntrySegment(Rect::makeXYWH(45.000000, 0.0, 24.000000, 24.000000), scaledFont, "World"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsMaxHeight) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 50);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);
    auto layout = builder.build();

    ASSERT_FALSE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.000000, 0.0, 102.000000, 34.731000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.000000, 0.000000, 89.000000, 34.731000), testContainer.primaryFont, "Hello world"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(89.000000, 0.000000, 13.000000, 34.731000), testContainer.primaryFont, "…"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsFontFallback) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("مصر", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(45.000000, 0.0, 30.000000, 31.264),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(45.000000, 0.00, 30.000000, 31.264),
                                                              testContainer.arabicFont,
                                                              "مصر"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsFontFallbackFromRegisteredFont) {
    TextLayoutTestContainer testContainer;

    auto appleColorEmoji = testContainer.getAppleEmojiFont();

    auto maxSize = Size::make(120, 10000);

    Ref<TextLayout> layout;
    {
        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
        builder.setIncludeSegments(true);

        builder.append("😊", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);

        layout = builder.build();
    }

    ASSERT_TRUE(layout->fitsInMaxSize());

    // Should initially use Apple Color Emoji
    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(0.000000, 0.0, 21.0, 40.359000),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(
                                           Rect::makeXYWH(0.000000, 0.00, 21.0, 40.359000), appleColorEmoji, "😊"),
                                   })}),
              layout->toJSONValue());

    // We now regsiter our custom emoji font and retry
    auto androidEmojiFont = testContainer.registerFont("Noto Color Emoji",
                                                       FontStyle(FontWidthNormal, FontWeightNormal, FontSlantUpright),
                                                       "NotoColorEmoji-Regular",
                                                       "NotoColorEmoji-Regular",
                                                       /* canUseAsFallback */ true);

    {
        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
        builder.setIncludeSegments(true);

        builder.append("😊", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);

        layout = builder.build();
    }

    ASSERT_TRUE(layout->fitsInMaxSize());

    // We should have used the Noto Color Emoji font
    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.000000, 0.0, 21.0, 29.883),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 0.00, 21.0, 29.883), androidEmojiFont, "😊"),
                      })}),
              layout->toJSONValue());
}

// Text incorrect for appleColorEmojiFont
// Expected: 🏳🏳👨👨👨👨
// Actual: 🏳👨👨
TEST(TextLayout, DISABLED_supportsZeroWidthJoiner) {
    TextLayoutTestContainer testContainer;

    auto appleColorEmojiFont = testContainer.getAppleEmojiFont();
    auto devanagariFont = testContainer.getDevanagariFont();
    auto sinhalaFont = testContainer.getSinhalaFont();

    auto maxSize = Size::make(2000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("🏳️‍🌈👨‍🌾👨‍🦰 क्‍ष ශ්‍ර",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.5),
                   0.0,
                   TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000, 0.0, 104.000, 40.359),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.000, 0.00, 63.000000, 40.359000), appleColorEmojiFont, "🏳🏳👨👨👨👨"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(63.000000, 0.00, 4.000000, 34.833), testContainer.primaryFont, " "),
                    TextLayoutEntrySegment(Rect::makeXYWH(67.000000, 0.00, 25.000000, 35.700), devanagariFont, "ककष "),
                    TextLayoutEntrySegment(Rect::makeXYWH(92.000000, 0.00, 12.000000, 34.216000), sinhalaFont, "ශ"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, useProvidedFontForSpacesBetweenEmojis) {
    TextLayoutTestContainer testContainer;

    auto appleColorEmojiFont = testContainer.getAppleEmojiFont();

    auto maxSize = Size::make(2000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("🦊 🦊 🦊 3 foxes ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000000, 0.0, 136.000000, 26.906000),
                std::nullopt,
                {TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 0.00, 21.000000, 26.906000), appleColorEmojiFont, "🦊"),
                 TextLayoutEntrySegment(
                     Rect::makeXYWH(21.000000, 0.00, 4.000000, 23.154000), testContainer.primaryFont, " "),
                 TextLayoutEntrySegment(Rect::makeXYWH(25.000000, 0.00, 21.000000, 26.906000), appleColorEmojiFont, "🦊"),
                 TextLayoutEntrySegment(
                     Rect::makeXYWH(46.000000, 0.00, 4.000000, 23.154000), testContainer.primaryFont, " "),
                 TextLayoutEntrySegment(Rect::makeXYWH(50.000000, 0.00, 21.000000, 26.906000), appleColorEmojiFont, "🦊"),
                 TextLayoutEntrySegment(
                     Rect::makeXYWH(71.000000, 0.00, 65.000000, 23.154000), testContainer.primaryFont, " 3 foxes ")})}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsZeroWidthNonJoiner) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(2000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("می‌خواهم", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(maxSize,
                           {TextLayoutEntry(Rect::makeXYWH(0.000, 0.0, 56.000, 31.264),
                                            std::nullopt,
                                            {
                                                TextLayoutEntrySegment(Rect::makeXYWH(0.000, 0.00, 56.000000, 31.264),
                                                                       testContainer.arabicFont,
                                                                       "می‌خواهم"),
                                            })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsLineBreakingInRightToLeft) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(80, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("مرحبا كيف حالك", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(15.000, 0.0, 65.000, 62.527000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(15.000, 0.00, 65.000, 31.264000), testContainer.arabicFont, "مرحبا كيف"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(52.000000, 31.264000, 28.000000, 31.264000), testContainer.arabicFont, "حالك"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalTextWithLeftToRightBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(200, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("bahrain مصر kuwait", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.0, 0.0, 154.000000, 23.799000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.0, 0.00, 67.000000, 23.154000), testContainer.primaryFont, "bahrain "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(67.000000, 0.0, 30.000000, 20.842000), testContainer.arabicFont, "مصر"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(97.000000, 0.00, 57.000000, 23.154000), testContainer.primaryFont, " kuwait"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalTextWithRightToLeftBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(200, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, true, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("bahrain مصر kuwait", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(46.000000, 0.0, 154.000000, 23.799000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(46.000000, 0.00, 67.000000, 23.154000), testContainer.primaryFont, "bahrain "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(113.000000, 0.0, 30.000000, 20.842000), testContainer.arabicFont, "مصر"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(143.000000, 0.00, 57.000000, 23.154000), testContainer.primaryFont, " kuwait"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalLineBreakingWithLeftToRightBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("قرأ Wikipedia™ طوال اليوم.", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.0, 0.0, 100.000000, 47.597000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.000000, 0.0, 5.000000, 23.154000), testContainer.primaryFont, "."),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(5.000000, 0.0, 63.000000, 20.842000), testContainer.arabicFont, "طوال اليوم"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(68.000000, 0.00, 17.000000, 23.154000), testContainer.primaryFont, "™ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.000000, 23.799000, 23.000000, 20.842000), testContainer.arabicFont, "قرأ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(23.000000, 23.799000, 77.000000, 23.154000), testContainer.primaryFont, "Wikipedia"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalLineBreakingWithRightToLeftBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, true, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("قرأ Wikipedia™ طوال اليوم.", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(20.000000, 0.0, 100.000000, 47.597000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(35.000000, 0.0, 5.000000, 23.154000), testContainer.primaryFont, "."),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(40.000000, 0.0, 63.000000, 20.842000), testContainer.arabicFont, "طوال اليوم"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(103.000000, 0.00, 17.000000, 23.154000), testContainer.primaryFont, "™ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(20.000000, 23.799000, 23.000000, 20.842000), testContainer.arabicFont, "قرأ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(43.000000, 23.799000, 77.000000, 23.154000), testContainer.primaryFont, "Wikipedia"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, emitsSeparateEntriesForEachColor) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(500, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);
    builder.append("world", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone, nullptr, {Color::red()});
    builder.append(" and ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);
    builder.append("welcome", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone, nullptr, {Color::blue()});
    builder.append("!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {
                TextLayoutEntry(
                    Rect::makeXYWH(0.0, 0.0, 204.000000, 23.154000),
                    std::nullopt,
                    {
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(0.000000, 0, 45.000000, 23.154000), testContainer.primaryFont, "Hello "),
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(89.000000, 0.0, 39.000000, 23.154000), testContainer.primaryFont, " and "),
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(199.000000, 0, 5.000000, 23.154000), testContainer.primaryFont, "!"),
                    }),
                TextLayoutEntry(Rect::makeXYWH(45.000000, 0.0, 44.000000, 23.154000),
                                Color::red(),
                                {
                                    TextLayoutEntrySegment(Rect::makeXYWH(45.000000, 0, 44.000000, 23.154000),
                                                           testContainer.primaryFont,
                                                           "world"),
                                }),
                TextLayoutEntry(Rect::makeXYWH(128.000000, 0.0, 71.000000, 23.154000),
                                Color::blue(),
                                {
                                    TextLayoutEntrySegment(Rect::makeXYWH(128.000000, 0, 71.000000, 23.154000),
                                                           testContainer.primaryFont,
                                                           "welcome"),
                                }),
            }),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBackgroundColor) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(500, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    auto backgroundColor = Color::red();
    builder.append(
        "Hello",
        testContainer.primaryFont,
        TextLayoutLineHeight::multiple(1.0),
        0.0,
        TextDecorationNone,
        nullptr,
        std::nullopt,
        TextBackgroundStyle{backgroundColor});

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();
    const auto& segment = layout->getEntries()[0].segments[0];

    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());
    ASSERT_EQ(TextLayoutVisualEntryKindBackground, visualEntries[0].kind);
    ASSERT_EQ(std::make_optional(backgroundColor), visualEntries[0].color);
    ASSERT_TRUE(visualEntries[0].predraw);
    ASSERT_EQ(segment.bounds, visualEntries[0].bounds);
}

TEST(TextLayout, supportsBackgroundColorAcrossLines) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(90, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationNone,
                   nullptr,
                   std::nullopt,
                   TextBackgroundStyle{Color::blue()});

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();
    const auto& segments = layout->getEntries()[0].segments;

    ASSERT_GT(visualEntries.size(), static_cast<size_t>(1));
    ASSERT_EQ(segments.size(), visualEntries.size());
    for (size_t i = 0; i < visualEntries.size(); i++) {
        ASSERT_EQ(TextLayoutVisualEntryKindBackground, visualEntries[i].kind);
        ASSERT_EQ(std::make_optional(Color::blue()), visualEntries[i].color);
        ASSERT_TRUE(visualEntries[i].predraw);
        ASSERT_EQ(segments[i].bounds, visualEntries[i].bounds);
    }
}

TEST(TextLayout, supportsLayoutAffectingBackgroundPaddingAndRadius) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(500, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    TextBackgroundPadding padding;
    padding.left = 4;
    padding.top = 2;
    padding.right = 6;
    padding.bottom = 3;

    builder.append("Hello ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);
    builder.append("World",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationNone,
                   nullptr,
                   std::nullopt,
                   TextBackgroundStyle{Color::red(), padding, BorderRadius::makeOval(5, false)});
    builder.append("!", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();
    const auto& segments = layout->getEntries()[0].segments;
    const auto& visualEntries = layout->getVisualEntries();

    ASSERT_EQ(static_cast<size_t>(3), segments.size());
    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());

    const auto& helloSegment = segments[0];
    const auto& worldSegment = segments[1];
    const auto& bangSegment = segments[2];
    const auto& backgroundEntry = visualEntries[0];

    ASSERT_EQ(helloSegment.bounds.right, worldSegment.bounds.left);
    ASSERT_EQ(worldSegment.bounds.right, bangSegment.bounds.left);
    ASSERT_EQ(static_cast<Scalar>(47 + padding.left + padding.right), worldSegment.bounds.width());
    ASSERT_NEAR(static_cast<Scalar>(23.154 + padding.top + padding.bottom), worldSegment.bounds.height(), 0.0001f);
    ASSERT_EQ(TextLayoutVisualEntryKindBackground, backgroundEntry.kind);
    ASSERT_EQ(std::make_optional(Color::red()), backgroundEntry.color);
    ASSERT_EQ(BorderRadius::makeOval(5, false), backgroundEntry.borderRadius);
    ASSERT_EQ(worldSegment.bounds, backgroundEntry.bounds);
}

TEST(TextLayout, supportsBackgroundPaddingAcrossLines) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(90, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);

    TextBackgroundPadding padding;
    padding.left = 3;
    padding.right = 5;

    builder.append("Hello world and welcome",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationNone,
                   nullptr,
                   std::nullopt,
                   TextBackgroundStyle{Color::blue(), padding, BorderRadius::makeOval(4, false)});

    auto layout = builder.build();
    const auto& segments = layout->getEntries()[0].segments;
    const auto& visualEntries = layout->getVisualEntries();

    ASSERT_GT(segments.size(), static_cast<size_t>(1));
    ASSERT_EQ(segments.size(), visualEntries.size());
    for (size_t i = 0; i < segments.size(); i++) {
        ASSERT_EQ(TextLayoutVisualEntryKindBackground, visualEntries[i].kind);
        ASSERT_EQ(std::make_optional(Color::blue()), visualEntries[i].color);
        ASSERT_EQ(BorderRadius::makeOval(4, false), visualEntries[i].borderRadius);
        ASSERT_EQ(segments[i].bounds, visualEntries[i].bounds);
        ASSERT_GE(segments[i].bounds.width(), padding.left + padding.right);
    }
}

TEST(TextLayout, supportsPercentBackgroundBorderRadius) {
    TextLayoutTestContainer testContainer;

    TextLayoutBuilder builder(TextAlignLeft,
                              TextOverflowEllipsis,
                              Size::make(500, 100),
                              0,
                              testContainer.fontManager,
                              false,
                              1.0f);
    builder.setIncludeSegments(true);

    builder.append("code",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationNone,
                   nullptr,
                   std::nullopt,
                   TextBackgroundStyle{Color::blue(), {}, BorderRadius::makeOval(50, true)});

    auto layout = builder.build();
    const auto& visualEntries = layout->getVisualEntries();

    ASSERT_EQ(static_cast<size_t>(1), visualEntries.size());
    ASSERT_EQ(TextLayoutVisualEntryKindBackground, visualEntries[0].kind);
    ASSERT_EQ(BorderRadius::makeOval(50, true), visualEntries[0].borderRadius);
}

TEST(TextLayout, dontPrioritizeFewerFonts) {
    TextLayoutTestContainer testContainer;
    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter,
                              TextOverflowEllipsis,
                              maxSize,
                              0,
                              testContainer.fontManager,
                              false /*isRightToLeft*/,
                              1.0f,
                              false /*prioritizeLowerFontCount*/);
    builder.setIncludeSegments(true);
    builder.append(" مصر", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);
    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());
    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(42.500000, 0.0, 35.000000, 31.264),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(42.500000, 0.00, 35.000000, 31.264),
                                                              testContainer.arabicFont,
                                                              " مصر"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, prioritizeFewerFontsTwoSegmentsMerging) {
    TextLayoutTestContainer testContainer;
    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter,
                              TextOverflowEllipsis,
                              maxSize,
                              0,
                              testContainer.fontManager,
                              true /*isRightToLeft*/,
                              1.0f,
                              true /*prioritizeLowerFontCount*/);
    builder.setIncludeSegments(true);
    builder.append(" مصر", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);
    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());
    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(42.500000, 0.0, 35.000000, 31.264),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(42.500000, 0.00, 35.000000, 31.264),
                                                              testContainer.arabicFont,
                                                              " مصر"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, prioritizeFewerFontsFiveSegmentsDifferentParagraphs) {
    TextLayoutTestContainer testContainer;
    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter,
                              TextOverflowEllipsis,
                              maxSize,
                              0,
                              testContainer.fontManager,
                              false /*isRightToLeft*/,
                              1.0f,
                              true /*prioritizeLowerFontCount*/);
    builder.setIncludeSegments(true);
    builder.append(" مصر \n سيريا", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.5), 0.0, TextDecorationNone);
    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());
    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(40.000000, 0.0, 40.00000, 62.527),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(40.000000, 0.00, 40.000000, 31.264), testContainer.arabicFont, " مصر "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(40.000000, 31.264, 40.000000, 31.264), testContainer.arabicFont, " سيريا"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidiMarker) {
    TextLayoutTestContainer testContainer;
    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);
    builder.append("the title is \"\u2067مقدمة إلى \u2066C++\u2069\u2069\" in Arabic.",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000000, 0.0, 266.000000, 23.799000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.000000, 0.00, 88.000000, 23.154000), testContainer.primaryFont, "the title is \""),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(88.000000, 0.00, 31.000000, 23.154000), testContainer.primaryFont, "C++"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(119.000000, 0.00, 62.000000, 20.842000), testContainer.arabicFont, "مقدمة إلى "),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(181.000000, 0.00, 85.000000, 23.154000), testContainer.primaryFont, "\" in Arabic."),
                })}),
        layout->toJSONValue());
}

struct Attachment : public SimpleRefCountable {};

TEST(TextLayut, supportsAttachments) {
    auto attachment1 = Valdi::makeShared<Attachment>();
    auto attachment2 = Valdi::makeShared<Attachment>();

    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(300, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);
    builder.append("the URL is ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);
    builder.append("https://www.snapchat.com", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone, attachment1);
    builder.append(" and you can also search on ", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);
    builder.append("https://google.com", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone, attachment2);

    builder.append(" if you so wish.", testContainer.primaryFont, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(0.000000, 0.0, 297.000000, 69.462000),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 0.00, 83.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "the URL is "),
                                       TextLayoutEntrySegment(Rect::makeXYWH(83.000000, 0.00, 214.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "https://www.snapchat.com"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 23.154000, 221.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "and you can also search on "),
                                       TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 46.308000, 151.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              "https://google.com"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(151.000000, 46.308000, 115.000000, 23.154000),
                                                              testContainer.primaryFont,
                                                              " if you so wish."),
                                   })}),
              layout->toJSONValue());

    ASSERT_EQ(static_cast<size_t>(2), layout->getAttachments().size());

    ASSERT_EQ(toJSONValue(Rect::makeXYWH(83.000000, 0.00, 214.000000, 23.154000)),
              toJSONValue(layout->getAttachments()[0].bounds));
    ASSERT_EQ(attachment1, layout->getAttachments()[0].attachment);

    ASSERT_EQ(toJSONValue(Rect::makeXYWH(0.000000, 46.308000, 151.000000, 23.154000)),
              toJSONValue(layout->getAttachments()[1].bounds));
    ASSERT_EQ(attachment2, layout->getAttachments()[1].attachment);
}

TEST(TextLayut, baselineAlignedReplacementAttachmentBottomMatchesBaseline) {
    auto attachment = Valdi::makeShared<Attachment>();

    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(300, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false, 1.0f);
    builder.setIncludeSegments(true);
    builder.append("x",
                   testContainer.primaryFont,
                   TextLayoutLineHeight::multiple(1.0),
                   0.0,
                   TextDecorationNone,
                   attachment,
                   std::nullopt,
                   std::nullopt,
                   std::nullopt,
                   Size::make(18, 40),
                   InlineViewVerticalAlignment::Baseline);

    auto layout = builder.build();
    ASSERT_EQ(static_cast<size_t>(1), layout->getAttachments().size());

    const auto& layoutEntry = layout->getEntries()[0];
    const auto& attachmentBounds = layout->getAttachments()[0].bounds;
    ASSERT_NEAR(layoutEntry.bounds.y(), attachmentBounds.y(), 0.0001f);
    ASSERT_NEAR(layoutEntry.bounds.y() + layoutEntry.bounds.height() - testContainer.primaryFont->metrics().descent,
                attachmentBounds.y() + attachmentBounds.height(),
                0.0001f);
}

TEST(TextLayut, textLayerRebuildsCachedLayoutWhenInlineAttachmentSizeChanges) {
    auto attachment = Valdi::makeShared<MutableInlineAttachment>(0, Valdi::Size(10, 20));

    TextLayoutTestContainer testContainer;
    auto resources = makeShared<Resources>(testContainer.fontManager, 1.0f, ConsoleLogger::getLogger());
    TextLayer textLayer(resources);

    AttributedText::Parts parts;
    auto& part = parts.emplace_back();
    part.content = StringCache::getGlobal().makeString(std::string_view("x"));
    part.style.font = testContainer.primaryFont;
    part.style.inlineViewAttachment = attachment;
    textLayer.setAttributedText(makeShared<AttributedText>(std::move(parts)));

    auto maxSize = Size::make(300, 10000);
    auto initialSize = textLayer.sizeThatFits(maxSize);
    ASSERT_NEAR(10.0f, initialSize.width, 0.0001f);

    attachment->setSize(Valdi::Size(42, 20));

    auto updatedSize = textLayer.sizeThatFits(maxSize);
    ASSERT_NEAR(42.0f, updatedSize.width, 0.0001f);
}

TEST(TextLayut, textLayerClearsPreviouslyLaidOutInlineChildrenThatAreNoLongerReferenced) {
    auto attachment = Valdi::makeShared<MutableInlineAttachment>(0, Valdi::Size(10, 20));

    TextLayoutTestContainer testContainer;
    auto resources = makeShared<Resources>(testContainer.fontManager, 1.0f, ConsoleLogger::getLogger());
    TextLayer textLayer(resources);
    auto childLayer = makeLayer<Layer>(resources);
    textLayer.addChild(childLayer);
    textLayer.setFrame(Rect::makeXYWH(0, 0, 300, 100));

    AttributedText::Parts inlineParts;
    auto& inlinePart = inlineParts.emplace_back();
    inlinePart.content = StringCache::getGlobal().makeString(std::string_view("x"));
    inlinePart.style.font = testContainer.primaryFont;
    inlinePart.style.inlineViewAttachment = attachment;
    textLayer.setAttributedText(makeShared<AttributedText>(std::move(inlineParts)));
    textLayer.layoutInlineChildrenInLayer(textLayer);

    ASSERT_NEAR(10.0f, childLayer->getFrame().width(), 0.0001f);
    ASSERT_NEAR(20.0f, childLayer->getFrame().height(), 0.0001f);

    AttributedText::Parts plainParts;
    auto& plainPart = plainParts.emplace_back();
    plainPart.content = StringCache::getGlobal().makeString(std::string_view("plain"));
    plainPart.style.font = testContainer.primaryFont;
    textLayer.setAttributedText(makeShared<AttributedText>(std::move(plainParts)));
    textLayer.layoutInlineChildrenInLayer(textLayer);

    ASSERT_EQ(Rect::makeXYWH(0, 0, 0, 0), childLayer->getFrame());
}

TEST(TextLayut, editableTextLayerReportsTextLayerAsContentLayerForInlineChildren) {
    auto attachment = Valdi::makeShared<MutableInlineAttachment>(0, Valdi::Size(10, 20));

    TextLayoutTestContainer testContainer;
    auto resources = makeShared<Resources>(testContainer.fontManager, 1.0f, ConsoleLogger::getLogger());
    auto editableTextLayer = makeLayer<EditableTextLayer>(resources);
    auto inlineChildLayer = makeLayer<Layer>(resources);
    auto unreferencedChildLayer = makeLayer<Layer>(resources);
    auto& contentLayer = editableTextLayer->getChildInsertionLayer();
    contentLayer.addChild(inlineChildLayer);
    contentLayer.addChild(unreferencedChildLayer);
    unreferencedChildLayer->setFrame(Rect::makeXYWH(5, 6, 7, 8));
    editableTextLayer->setFrame(Rect::makeXYWH(0, 0, 300, 100));

    AttributedText::Parts inlineParts;
    auto& inlinePart = inlineParts.emplace_back();
    inlinePart.content = StringCache::getGlobal().makeString(std::string_view("x"));
    inlinePart.style.font = testContainer.primaryFont;
    inlinePart.style.inlineViewAttachment = attachment;
    editableTextLayer->setAttributedText(makeShared<AttributedText>(std::move(inlineParts)));
    editableTextLayer->layoutIfNeeded();

    ASSERT_EQ(Rect::makeXYWH(0, 0, 300, 100), editableTextLayer->getTextLayer().getFrame());
    ASSERT_EQ(&editableTextLayer->getTextLayer(), &contentLayer);
    ASSERT_NEAR(10.0f, inlineChildLayer->getFrame().width(), 0.0001f);
    ASSERT_NEAR(20.0f, inlineChildLayer->getFrame().height(), 0.0001f);
    ASSERT_EQ(Rect::makeXYWH(0, 0, 0, 0), unreferencedChildLayer->getFrame());
}

TEST(TextLayut, canQueryAttachmentByPoint) {
    auto attachment1 = Valdi::makeShared<Attachment>();
    auto attachment2 = Valdi::makeShared<Attachment>();

    auto bounds1 = Rect::makeXYWH(80.000000, 0.00, 207.000000, 23.222);
    auto bounds2 = Rect::makeXYWH(0.00000, 46.444, 150.000000, 23.222);

    std::vector<TextLayoutAttachment> attachments;
    attachments.emplace_back(bounds1, attachment1, std::nullopt);
    attachments.emplace_back(bounds2, attachment2, std::nullopt);

    auto maxSize = Size::make(300, 10000);
    TextLayout textLayout(maxSize, {}, {}, std::move(attachments), true);

    auto attachment = textLayout.getAttachmentAtPoint(Point::make(0.0f, 0.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(79.0f, 0.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(80.0f, 0.0f), 0.0f);
    ASSERT_EQ(attachment1, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(183.0f, 0.0f), 0.0f);
    ASSERT_EQ(attachment1, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(287.0f, 0.0f), 0.0f);
    ASSERT_EQ(attachment1, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(288.0f, 0.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(200.0f, 10.0f), 0.0f);
    ASSERT_EQ(attachment1, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(0.0f, 45.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(0.0f, 47.0f), 0.0f);
    ASSERT_EQ(attachment2, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(75.0f, 55.0f), 0.0f);
    ASSERT_EQ(attachment2, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(150.0f, 69.0f), 0.0f);
    ASSERT_EQ(attachment2, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(151.0f, 69.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(150.0f, 70.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);
}

TEST(TextLayut, canQueryAttachmentByPointWithTolerance) {
    auto attachment1 = Valdi::makeShared<Attachment>();
    auto attachment2 = Valdi::makeShared<Attachment>();

    auto bounds1 = Rect::makeXYWH(80.000000, 0.00, 207.000000, 23.222);
    auto bounds2 = Rect::makeXYWH(0.00000, 46.444, 150.000000, 23.222);

    std::vector<TextLayoutAttachment> attachments;
    attachments.emplace_back(bounds1, attachment1, std::nullopt);
    attachments.emplace_back(bounds2, attachment2, std::nullopt);

    auto maxSize = Size::make(300, 10000);
    TextLayout textLayout(maxSize, {}, {}, std::move(attachments), true);

    auto attachment = textLayout.getAttachmentAtPoint(Point::make(79.0f, 0.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(79.0f, 0.0f), 2.0f);
    ASSERT_EQ(attachment1, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(0.0f, 45.0f), 0.0f);
    ASSERT_EQ(nullptr, attachment);

    attachment = textLayout.getAttachmentAtPoint(Point::make(0.0f, 45.0f), 2.0f);
    ASSERT_EQ(attachment2, attachment);
}

} // namespace snap::drawing
