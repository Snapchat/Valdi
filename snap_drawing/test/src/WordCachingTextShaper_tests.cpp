#include <gtest/gtest.h>

#include "TestDataUtils.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/WordCachingTextShaper.hpp"
#include "snap_drawing/cpp/Utils/UTFUtils.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

namespace snap::drawing {

struct TextShaperRequest {
    std::string characters;
    Ref<Font> font;
    bool isRightToLeft;
    Scalar letterSpacing;
    TextScript script;
};

struct TestTextShaper : public TextShaper {
    std::vector<TextShaperRequest> shapeRequests;

    TextParagraphList resolveParagraphs(const Character* unicodeText, size_t length, bool isRightToLeft) override {
        return TextParagraphList();
    }

    size_t shape(const Character* unicodeText,
                 size_t length,
                 Font& font,
                 bool isRightToLeft,
                 Scalar letterSpacing,
                 TextScript script,
                 std::vector<ShapedGlyph>& out) override {
        TextShaperRequest request;
        request.characters = unicodeToUtf8(unicodeText, length);
        request.font = Valdi::strongSmallRef(&font);
        request.isRightToLeft = isRightToLeft;
        request.letterSpacing = letterSpacing;
        request.script = script;

        shapeRequests.emplace_back(request);

        auto glyphsStart = out.size();
        out.resize(glyphsStart + length);
        auto* outGlyphs = out.data() + glyphsStart;

        for (size_t i = 0; i < length; i++) {
            auto& glyph = outGlyphs[i];

            glyph.setCharacter(unicodeText[i], false);
            glyph.glyphID = 0;
            glyph.offsetX = 0;
            glyph.offsetY = 0;
            glyph.advanceX = font.resolvedSize();
        }

        return length;
    }
};

class WordCachingTextShaperTest : public ::testing::Test {
protected:
    void SetUp() override {
        fontManager = Valdi::makeShared<FontManager>(Valdi::ConsoleLogger::getLogger());
        fontManager->load();

        avenirNext = registerFont("Montserrat",
                                  FontStyle(FontWidthNormal, FontWeightNormal, FontSlantUpright),
                                  "Montserrat-Regular",
                                  "montserrat_regular");
        testTextShaper = Valdi::makeShared<TestTextShaper>();
    }

    void TearDown() override {
        avenirNext = nullptr;
        fontManager = nullptr;
    }

    Ref<FontManager> fontManager;
    Ref<Font> avenirNext;
    Ref<TestTextShaper> testTextShaper;

private:
    Ref<Font> registerFont(std::string_view fontFamilyName,
                           FontStyle fontStyle,
                           std::string_view fontName,
                           const std::string& filename) {
        auto testData = getTestData(filename + ".ttf");
        SC_ASSERT(testData.success(), testData.description());

        fontManager->registerTypeface(
            Valdi::StringCache::getGlobal().makeString(fontFamilyName), fontStyle, false, testData.value());
        auto font =
            fontManager->getFontWithNameAndSize(Valdi::StringCache::getGlobal().makeString(fontName), 17, 1.0, true);
        SC_ASSERT(font.success(), font.description());

        return font.moveValue();
    }
};

TEST_F(WordCachingTextShaperTest, canShapeByWord) {
    WordCachingTextShaper textShaper(testTextShaper, WordCachingTextShaperStrategy::PrioritizeCorrectness);

    auto unicode = utf8ToUnicode("This is a sentence");

    std::vector<ShapedGlyph> glyphs;

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, false, 1.0f, TextScript::invalid(), glyphs);

    ASSERT_EQ(static_cast<size_t>(4), testTextShaper->shapeRequests.size());

    ASSERT_EQ("This", testTextShaper->shapeRequests[0].characters);
    ASSERT_EQ("is", testTextShaper->shapeRequests[1].characters);
    ASSERT_EQ("a", testTextShaper->shapeRequests[2].characters);
    ASSERT_EQ("sentence", testTextShaper->shapeRequests[3].characters);

    for (const auto& shapeRequest : testTextShaper->shapeRequests) {
        ASSERT_EQ(avenirNext, shapeRequest.font);
        ASSERT_FALSE(shapeRequest.isRightToLeft);
        ASSERT_EQ(1.0f, shapeRequest.letterSpacing);
    }
}

TEST_F(WordCachingTextShaperTest, avoidShapingWhenHittingKnownWords) {
    WordCachingTextShaper textShaper(testTextShaper, WordCachingTextShaperStrategy::PrioritizeCorrectness);

    auto unicode = utf8ToUnicode("word word    word  ");

    std::vector<ShapedGlyph> glyphs;

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, false, 1.0f, TextScript::invalid(), glyphs);

    ASSERT_EQ(static_cast<size_t>(1), testTextShaper->shapeRequests.size());

    ASSERT_EQ("word", testTextShaper->shapeRequests[0].characters);

    for (const auto& shapeRequest : testTextShaper->shapeRequests) {
        ASSERT_EQ(avenirNext, shapeRequest.font);
        ASSERT_FALSE(shapeRequest.isRightToLeft);
        ASSERT_EQ(1.0f, shapeRequest.letterSpacing);
    }

    glyphs.clear();

    testTextShaper->shapeRequests.clear();
    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, false, 1.0f, TextScript::invalid(), glyphs);

    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    // If we change the direction, it should not hit the cache

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, true, 1.0f, TextScript::invalid(), glyphs);

    ASSERT_EQ(static_cast<size_t>(1), testTextShaper->shapeRequests.size());
    testTextShaper->shapeRequests.clear();

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, true, 1.0f, TextScript::invalid(), glyphs);
    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    // If we change the letter spacing, it should also not hit the cache

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, true, 2.0f, TextScript::invalid(), glyphs);

    ASSERT_EQ(static_cast<size_t>(1), testTextShaper->shapeRequests.size());
    testTextShaper->shapeRequests.clear();

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, true, 2.0f, TextScript::invalid(), glyphs);
    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    // Using a different font size should also not hit the cache
    textShaper.shape(unicode.data(),
                     unicode.size(),
                     *avenirNext->withSize(12.0f).value(),
                     true,
                     2.0f,
                     TextScript::invalid(),
                     glyphs);

    ASSERT_EQ(static_cast<size_t>(1), testTextShaper->shapeRequests.size());
    testTextShaper->shapeRequests.clear();

    textShaper.shape(unicode.data(),
                     unicode.size(),
                     *avenirNext->withSize(12.0f).value(),
                     true,
                     2.0f,
                     TextScript::invalid(),
                     glyphs);
    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    // Using a different script should also not hit the cache

    textShaper.shape(
        unicode.data(), unicode.size(), *avenirNext->withSize(12.0f).value(), true, 2.0f, TextScript::common(), glyphs);

    ASSERT_EQ(static_cast<size_t>(1), testTextShaper->shapeRequests.size());
    testTextShaper->shapeRequests.clear();

    textShaper.shape(
        unicode.data(), unicode.size(), *avenirNext->withSize(12.0f).value(), true, 2.0f, TextScript::common(), glyphs);
    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());
}

TEST_F(WordCachingTextShaperTest, canPrioritizeCacheHit) {
    WordCachingTextShaper textShaper(testTextShaper, WordCachingTextShaperStrategy::PrioritizeCacheHit);

    auto unicode = utf8ToUnicode("word word    word  ");

    std::vector<ShapedGlyph> glyphs;

    textShaper.shape(unicode.data(), unicode.size(), *avenirNext, false, 1.0f, TextScript::invalid(), glyphs);

    ASSERT_EQ(static_cast<size_t>(1), testTextShaper->shapeRequests.size());

    ASSERT_EQ("word", testTextShaper->shapeRequests[0].characters);

    for (const auto& shapeRequest : testTextShaper->shapeRequests) {
        ASSERT_NE(avenirNext, shapeRequest.font);
        ASSERT_EQ(avenirNext->typeface(), shapeRequest.font->typeface());
        ASSERT_EQ(12.0f, shapeRequest.font->size());
        ASSERT_FALSE(shapeRequest.isRightToLeft);
        ASSERT_EQ(1.0f, shapeRequest.letterSpacing);
    }

    ASSERT_EQ(static_cast<size_t>(19), glyphs.size());
    ASSERT_EQ(17.0f, glyphs[0].advanceX);
    glyphs.clear();

    testTextShaper->shapeRequests.clear();

    textShaper.shape(unicode.data(),
                     unicode.size(),
                     *avenirNext->withSize(9.0f).value(),
                     false,
                     1.0f,
                     TextScript::invalid(),
                     glyphs);

    // We should get a cache hit even with a font of different size
    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    ASSERT_EQ(static_cast<size_t>(19), glyphs.size());
    ASSERT_EQ(9.0f, glyphs[0].advanceX);
    glyphs.clear();

    textShaper.shape(unicode.data(),
                     unicode.size(),
                     *avenirNext->withSize(12.0f).value(),
                     false,
                     1.0f,
                     TextScript::invalid(),
                     glyphs);

    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    ASSERT_EQ(static_cast<size_t>(19), glyphs.size());
    ASSERT_EQ(12.0f, glyphs[0].advanceX);
    glyphs.clear();

    textShaper.shape(unicode.data(),
                     unicode.size(),
                     *avenirNext->withSize(20.0f).value(),
                     false,
                     1.0f,
                     TextScript::invalid(),
                     glyphs);

    ASSERT_EQ(static_cast<size_t>(0), testTextShaper->shapeRequests.size());

    ASSERT_EQ(static_cast<size_t>(19), glyphs.size());
    ASSERT_EQ(20.0f, glyphs[0].advanceX);
    glyphs.clear();
}

} // namespace snap::drawing
