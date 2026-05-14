#include <gtest/gtest.h>

#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/TextLayout.hpp"
#include "snap_drawing/cpp/Text/TextLayoutBuilder.hpp"
#include "snap_drawing/cpp/Text/TextShaper.hpp"
#include "snap_drawing/cpp/Utils/JSONUtils.hpp"
#include "snap_drawing/cpp/Utils/UTFUtils.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include "TestDataUtils.hpp"
#include "hb.h"

using namespace Valdi;

namespace snap::drawing {

struct TextLayoutTestContainer {
    Ref<FontManager> fontManager;

    Ref<Font> avenirNext;
    Ref<Font> arabicFont;

    TextLayoutTestContainer() {
        fontManager = makeShared<FontManager>(ConsoleLogger::getLogger());
        fontManager->load();

        avenirNext = registerFont("Montserrat",
                                  FontStyle(FontWidthNormal, FontWeightNormal, FontSlantUpright),
                                  "Montserrat-Regular",
                                  "montserrat_regular",
                                  false);

        auto arabicText = utf8ToUnicode("ر");
        SC_ASSERT(arabicText.size() == 1);

        auto arabicFont = fontManager->getCompatibleFont(avenirNext, nullptr, 0, arabicText[0]);
        SC_ASSERT(arabicFont.success(), arabicFont.description());
        this->arabicFont = arabicFont.moveValue();
        SC_ASSERT(this->avenirNext->getDescription() != this->arabicFont->getDescription());
    }

    Ref<Font> getFontWithName(std::string_view fontName) {
        auto font =
            fontManager->getFontWithNameAndSize(Valdi::StringCache::getGlobal().makeString(fontName), 17, 1.0, true);
        SC_ASSERT(font.success(), font.description());

        return font.moveValue();
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

        return getFontWithName(fontName);
    }

    Ref<Font> getAppleEmojiFont() {
        return getFontWithName("Apple Color Emoji");
    }

    Ref<Font> getDevanagariFont() {
        return getFontWithName("Kohinoor Devanagari");
    }

    Ref<Font> getSinhalaFont() {
        return getFontWithName("Sinhala Sangam MN");
    }
};

Valdi::Value makeTextLayoutJSON(Size maxSize,
                                std::initializer_list<TextLayoutEntry> entries,
                                std::initializer_list<TextLayoutDecorationEntry> decorations = {}) {
    return TextLayout(maxSize,
                      std::vector<TextLayoutEntry>(entries),
                      std::vector<TextLayoutDecorationEntry>(decorations),
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
                          .setMapValue("font", Value("Montserrat 17 x1")));

    auto decorations = ValueArray::make(1);
    decorations->emplace(
        0, Value().setMapValue("bounds", toJSONValue(decorationBounds)).setMapValue("predraw", Value(true)));

    auto entries = ValueArray::make(1);

    entries->emplace(0, Value().setMapValue("bounds", toJSONValue(bounds)).setMapValue("segments", Value(segments)));

    auto expectedJSON = Value()
                            .setMapValue("maxSize", toJSONValue(size))
                            .setMapValue("entries", Value(entries))
                            .setMapValue("decorations", Value(decorations));

    ASSERT_EQ(expectedJSON,
              makeTextLayoutJSON(
                  size,
                  {TextLayoutEntry(bounds,
                                   std::nullopt,
                                   {TextLayoutEntrySegment(segmentsBounds, testContainer.avenirNext, "Hello world")})},
                  {TextLayoutDecorationEntry(decorationBounds, true, std::nullopt)}));

    ASSERT_NE(
        expectedJSON,
        makeTextLayoutJSON(
            size,
            {TextLayoutEntry(
                bounds, std::nullopt, {TextLayoutEntrySegment(segmentsBounds, testContainer.avenirNext, "Not good")})},
            {TextLayoutDecorationEntry(decorationBounds, true, std::nullopt)}));

    ASSERT_NE(
        expectedJSON,
        makeTextLayoutJSON(
            size,
            {TextLayoutEntry(bounds,
                             std::nullopt,
                             {TextLayoutEntrySegment(segmentsBounds, testContainer.avenirNext, "Hello world")})}));

    ASSERT_NE(
        expectedJSON,
        makeTextLayoutJSON(
            size,
            {TextLayoutEntry(bounds,
                             std::nullopt,
                             {TextLayoutEntrySegment(segmentsBounds, testContainer.avenirNext, "Hello world")})}));

    ASSERT_NE(expectedJSON,
              makeTextLayoutJSON(size,
                                 {TextLayoutEntry(bounds, std::nullopt, {})},
                                 {TextLayoutDecorationEntry(decorationBounds, true, std::nullopt)}));

    ASSERT_NE(expectedJSON,
              makeTextLayoutJSON(
                  size,
                  {TextLayoutEntry(Rect::makeXYWH(0, 0, 0, 0),
                                   std::nullopt,
                                   {TextLayoutEntrySegment(segmentsBounds, testContainer.avenirNext, "Hello world")})},
                  {TextLayoutDecorationEntry(decorationBounds, true, std::nullopt)}));
}

TEST(TextLayout, canLayoutSingleLine) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello World!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    auto expectedBounds = Rect::makeXYWH(0, 0, 97.0, 23.222000);
    auto segmentBounds = expectedBounds;

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(expectedBounds,
                                   std::nullopt,
                                   {TextLayoutEntrySegment(segmentBounds, testContainer.avenirNext, "Hello World!")})}),
              layout->toJSONValue());
}

TEST(TextLayout, canLayoutMultipleLinesWithExplicitNewLines) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world\nand welcome!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0, 111.000000, 46.444000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 88.000000, 23.222), testContainer.avenirNext, "Hello world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 23.222000, 111.000000, 23.222), testContainer.avenirNext, "and welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, canLayoutWithExplicitTrailingNewLines) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world\n\n\n", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(maxSize,
                                 {TextLayoutEntry(Rect::makeXYWH(0, 0, 88.000000, 92.888000),
                                                  std::nullopt,
                                                  {
                                                      TextLayoutEntrySegment(Rect::makeXYWH(0, 0.0, 88.000000, 23.222),
                                                                             testContainer.avenirNext,
                                                                             "Hello world"),
                                                  })}),
              layout->toJSONValue());
}

TEST(TextLayout, canLayoutMultipleLinesByWordBreak) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 111.000000, 46.444000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 88.000000, 23.222), testContainer.avenirNext, "Hello world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 23.222000, 111.000000, 23.222), testContainer.avenirNext, "and welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, canLayoutMultipleLinesByCharacterBreak) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append(
        "Thisisaverylongtextthatcannotfitinasingleline", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    builder.setLineBreakStrategy(LineBreakStrategy::ByCharacter);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 120.0, 69.666000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 120, 23.222000), testContainer.avenirNext, "Thisisaverylong"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 23.222, 119, 23.222),
                        testContainer.avenirNext,
                        "textthatcannotf"), /* Note: the 'i' after the 'f' should have been merged as fi */
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 46.444000, 103.000000, 23.222), testContainer.avenirNext, "tinasingleline"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, addsEllipsisWhenDoesntFit) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(80, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("This text will not fit", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_FALSE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 76.000, 23.222),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 59.000, 23.222), testContainer.avenirNext, "This tex"),
                    TextLayoutEntrySegment(Rect::makeXYWH(59.000, 0.0, 17.0, 23.222), testContainer.avenirNext, "…"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, breaksWordByCharacterWhenDoesntFit) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(80, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 1, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append(
        "Thisisaverylongtextthatcannotfitinasingleline", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_FALSE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0, 0.000000, 78.000, 23.222),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0, 0.0, 61.000, 23.222), testContainer.avenirNext, "Thisisav"),
                    TextLayoutEntrySegment(Rect::makeXYWH(61.000, 0.0, 17.0, 23.222), testContainer.avenirNext, "…"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsMultipleAppendWithDifferentFonts) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(10000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    auto scaledFont = testContainer.avenirNext->withScale(0.5);
    builder.append("Hello ", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);
    builder.append("World", scaledFont, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {
                TextLayoutEntry(
                    Rect::makeXYWH(0, 0.000000, 67.000, 23.222),
                    std::nullopt,
                    {
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(0, 0.0, 44.000000, 23.222000), testContainer.avenirNext, "Hello "),
                        TextLayoutEntrySegment(Rect::makeXYWH(44.000000, 0.0, 23.000, 11.611000), scaledFont, "World"),
                    }),
            }),
        layout->toJSONValue());
}

TEST(TextLayout, supportsRightTextAlignment) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(9.000000, 0.000000, 111.000000, 46.444000),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(32.000000, 0.000000, 88.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              "Hello world"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(9.000000, 23.222000, 111.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              "and welcome!"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsCenteredTextAlignment) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(4.500000, 0.000000, 111.000000, 46.444000),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(16.000000, 0.000000, 88.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              "Hello world"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(4.500000, 23.222, 111.0000, 23.222),
                                                              testContainer.avenirNext,
                                                              "and welcome!"),
                                   })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsJustifiedTextAlignment) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignJustify, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000000, 0.000000, 120.000000, 46.444000),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.000000, 0.000000, 40.00000, 23.222), testContainer.avenirNext, "Hello"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(76.000000, 0.000000, 44.000000, 23.222), testContainer.avenirNext, "world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.00000, 23.222, 30.0000, 23.222), testContainer.avenirNext, "and"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(43.000000, 23.222, 77.0000, 23.222), testContainer.avenirNext, "welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsLineHeightMultiple) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(4.500000, 0.0, 111.000000, 69.666),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(16.000000, 0, 88.000000, 34.8333), testContainer.avenirNext, "Hello world"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(4.500000, 34.8333, 111.000, 34.8333), testContainer.avenirNext, "and welcome!"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsMaxHeight) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 50);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello world and welcome!", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);
    auto layout = builder.build();

    ASSERT_FALSE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.000000, 0.0, 105.000000, 34.833000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.000000, 0, 88.000000, 34.8333), testContainer.avenirNext, "Hello world"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(88.000000, 0, 17.000, 34.8333), testContainer.avenirNext, "…"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsFontFallback) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignCenter, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("مصر", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);

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
        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
        builder.setIncludeSegments(true);

        builder.append("😊", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);

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
        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
        builder.setIncludeSegments(true);

        builder.append("😊", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);

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
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("🏳️‍🌈👨‍🌾👨‍🦰 क्‍ष ශ්‍ර",
                   testContainer.avenirNext,
                   1.5,
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
                        Rect::makeXYWH(63.000000, 0.00, 4.000000, 34.833), testContainer.avenirNext, " "),
                    TextLayoutEntrySegment(Rect::makeXYWH(67.000000, 0.00, 25.000000, 35.700), devanagariFont, "ककष "),
                    TextLayoutEntrySegment(Rect::makeXYWH(92.000000, 0.00, 12.000000, 34.216000), sinhalaFont, "ශ"),
                })}),
        layout->toJSONValue());
}

TEST(TextLayout, useProvidedFontForSpacesBetweenEmojis) {
    TextLayoutTestContainer testContainer;

    auto appleColorEmojiFont = testContainer.getAppleEmojiFont();

    auto maxSize = Size::make(2000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("🦊 🦊 🦊 3 foxes ", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000, 0.0, 134.000, 26.906),
                std::nullopt,
                {TextLayoutEntrySegment(Rect::makeXYWH(0.000, 0.00, 21.000000, 26.906), appleColorEmojiFont, "🦊"),
                 TextLayoutEntrySegment(
                     Rect::makeXYWH(21.000000, 0.00, 4.000000, 23.222), testContainer.avenirNext, " "),
                 TextLayoutEntrySegment(Rect::makeXYWH(25.000, 0.00, 21.000000, 26.906), appleColorEmojiFont, "🦊"),
                 TextLayoutEntrySegment(
                     Rect::makeXYWH(46.000000, 0.00, 4.000000, 23.222), testContainer.avenirNext, " "),
                 TextLayoutEntrySegment(Rect::makeXYWH(50.000, 0.00, 21.000000, 26.906), appleColorEmojiFont, "🦊"),
                 TextLayoutEntrySegment(
                     Rect::makeXYWH(71.000000, 0.00, 63.000000, 23.222), testContainer.avenirNext, " 3 foxes ")})}),
        layout->toJSONValue());
}

TEST(TextLayout, supportsZeroWidthNonJoiner) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(2000, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("می‌خواهم", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);

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
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("مرحبا كيف حالك", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);

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
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("bahrain مصر kuwait", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.0, 0.0, 147.000, 23.222000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.0, 0.00, 63.000, 23.222000), testContainer.avenirNext, "bahrain "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(63.000, 0.0, 30.000000, 20.842000), testContainer.arabicFont, "مصر"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(93.000, 0.00, 54.000, 23.222000), testContainer.avenirNext, " kuwait"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalTextWithRightToLeftBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(200, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, true);
    builder.setIncludeSegments(true);

    builder.append("bahrain مصر kuwait", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(53.000, 0.0, 147.000, 23.222000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(53.000, 0.00, 63.000, 23.222000), testContainer.avenirNext, "bahrain "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(116.000, 0.0, 30.000000, 20.842000), testContainer.arabicFont, "مصر"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(146.000, 0.00, 54.000, 23.222000), testContainer.avenirNext, " kuwait"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalLineBreakingWithLeftToRightBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("قرأ Wikipedia™ طوال اليوم.", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(0.0, 0.0, 102.000, 46.444000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.000, 0.0, 4.000000, 23.222000), testContainer.avenirNext, "."),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(4.0, 0.0, 63.000, 20.842000), testContainer.arabicFont, "طوال اليوم"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(67.000, 0.00, 21.000000, 23.222000), testContainer.avenirNext, "™ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(0.0, 23.222, 23.000, 20.842000), testContainer.arabicFont, "قرأ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(23.000, 23.222, 79.000, 23.222000), testContainer.avenirNext, "Wikipedia"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, supportsBidirectionalLineBreakingWithRightToLeftBaseDirection) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(120, 10000);
    TextLayoutBuilder builder(TextAlignRight, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, true);
    builder.setIncludeSegments(true);

    builder.append("قرأ Wikipedia™ طوال اليوم.", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(
                      Rect::makeXYWH(18.0, 0.0, 102.000, 46.444000),
                      std::nullopt,
                      {
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(32.000, 0.0, 4.000000, 23.222000), testContainer.avenirNext, "."),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(36.0, 0.0, 63.000, 20.842000), testContainer.arabicFont, "طوال اليوم"),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(99.000, 0.00, 21.000000, 23.222000), testContainer.avenirNext, "™ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(18.0, 23.222, 23.000, 20.842000), testContainer.arabicFont, "قرأ "),
                          TextLayoutEntrySegment(
                              Rect::makeXYWH(41.000, 23.222, 79.000, 23.222000), testContainer.avenirNext, "Wikipedia"),
                      })}),
              layout->toJSONValue());
}

TEST(TextLayout, emitsSeparateEntriesForEachColor) {
    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(500, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);

    builder.append("Hello ", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);
    builder.append("world", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone, nullptr, {Color::red()});
    builder.append(" and ", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);
    builder.append("welcome", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone, nullptr, {Color::blue()});
    builder.append("!", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {
                TextLayoutEntry(
                    Rect::makeXYWH(0.0, 0.0, 203.000000, 23.222000),
                    std::nullopt,
                    {
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(0.000000, 0, 44.000000, 23.222000), testContainer.avenirNext, "Hello "),
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(88.000000, 0.0, 38.000000, 23.222000), testContainer.avenirNext, " and "),
                        TextLayoutEntrySegment(
                            Rect::makeXYWH(197.000000, 0, 6.000000, 23.222000), testContainer.avenirNext, "!"),
                    }),
                TextLayoutEntry(Rect::makeXYWH(44.000000, 0.0, 44.000000, 23.222000),
                                Color::red(),
                                {
                                    TextLayoutEntrySegment(Rect::makeXYWH(44.000000, 0, 44.000000, 23.222000),
                                                           testContainer.avenirNext,
                                                           "world"),
                                }),
                TextLayoutEntry(Rect::makeXYWH(126.000000, 0.0, 71.000000, 23.222000),
                                Color::blue(),
                                {
                                    TextLayoutEntrySegment(Rect::makeXYWH(126.000000, 0, 71.000000, 23.222000),
                                                           testContainer.avenirNext,
                                                           "welcome"),
                                }),
            }),
        layout->toJSONValue());
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
                              false /*prioritizeLowerFontCount*/);
    builder.setIncludeSegments(true);
    builder.append(" مصر", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);
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
                              true /*prioritizeLowerFontCount*/);
    builder.setIncludeSegments(true);
    builder.append(" مصر", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);
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
                              true /*prioritizeLowerFontCount*/);
    builder.setIncludeSegments(true);
    builder.append(" مصر \n سيريا", testContainer.avenirNext, 1.5, 0.0, TextDecorationNone);
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
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);
    builder.append("the title is \"\u2067مقدمة إلى \u2066C++\u2069\u2069\" in Arabic.",
                   testContainer.avenirNext,
                   1.0,
                   0.0,
                   TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(
        makeTextLayoutJSON(
            maxSize,
            {TextLayoutEntry(
                Rect::makeXYWH(0.000000, 0.0, 264.00000, 23.222),
                std::nullopt,
                {
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(0.000000, 0.00, 84.0, 23.222000), testContainer.avenirNext, "the title is \""),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(84.000000, 0.00, 34.000000, 23.222), testContainer.avenirNext, "C++"),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(118.000000, 0.00, 62.000000, 20.842), testContainer.arabicFont, "مقدمة إلى "),
                    TextLayoutEntrySegment(
                        Rect::makeXYWH(180.000000, 0.00, 84.000000, 23.222), testContainer.avenirNext, "\" in Arabic."),
                })}),
        layout->toJSONValue());
}

struct Attachment : public SimpleRefCountable {};

TEST(TextLayut, supportsAttachments) {
    auto attachment1 = Valdi::makeShared<Attachment>();
    auto attachment2 = Valdi::makeShared<Attachment>();

    TextLayoutTestContainer testContainer;

    auto maxSize = Size::make(300, 10000);
    TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, maxSize, 0, testContainer.fontManager, false);
    builder.setIncludeSegments(true);
    builder.append("the URL is ", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);
    builder.append("https://www.snapchat.com", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone, attachment1);
    builder.append(" and you can also search on ", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);
    builder.append("https://google.com", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone, attachment2);

    builder.append(" if you so wish.", testContainer.avenirNext, 1.0, 0.0, TextDecorationNone);

    auto layout = builder.build();

    ASSERT_TRUE(layout->fitsInMaxSize());

    ASSERT_EQ(makeTextLayoutJSON(
                  maxSize,
                  {TextLayoutEntry(Rect::makeXYWH(0.000000, 0.0, 287.00000, 69.666),
                                   std::nullopt,
                                   {
                                       TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 0.00, 80.0, 23.222000),
                                                              testContainer.avenirNext,
                                                              "the URL is "),
                                       TextLayoutEntrySegment(Rect::makeXYWH(80.000000, 0.00, 207.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              "https://www.snapchat.com"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(0.000000, 23.222, 213.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              "and you can also search on "),
                                       TextLayoutEntrySegment(Rect::makeXYWH(0.00000, 46.444, 150.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              "https://google.com"),
                                       TextLayoutEntrySegment(Rect::makeXYWH(150.00000, 46.444, 110.000000, 23.222),
                                                              testContainer.avenirNext,
                                                              " if you so wish."),
                                   })}),
              layout->toJSONValue());

    ASSERT_EQ(static_cast<size_t>(2), layout->getAttachments().size());

    ASSERT_EQ(toJSONValue(Rect::makeXYWH(80.000000, 0.00, 207.000000, 23.222)),
              toJSONValue(layout->getAttachments()[0].bounds));
    ASSERT_EQ(attachment1, layout->getAttachments()[0].attachment);

    ASSERT_EQ(toJSONValue(Rect::makeXYWH(0.00000, 46.444, 150.000000, 23.222)),
              toJSONValue(layout->getAttachments()[1].bounds));
    ASSERT_EQ(attachment2, layout->getAttachments()[1].attachment);
}

TEST(TextLayut, canQueryAttachmentByPoint) {
    auto attachment1 = Valdi::makeShared<Attachment>();
    auto attachment2 = Valdi::makeShared<Attachment>();

    auto bounds1 = Rect::makeXYWH(80.000000, 0.00, 207.000000, 23.222);
    auto bounds2 = Rect::makeXYWH(0.00000, 46.444, 150.000000, 23.222);

    std::vector<TextLayoutAttachment> attachments;
    attachments.emplace_back(bounds1, attachment1);
    attachments.emplace_back(bounds2, attachment2);

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
    attachments.emplace_back(bounds1, attachment1);
    attachments.emplace_back(bounds2, attachment2);

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
