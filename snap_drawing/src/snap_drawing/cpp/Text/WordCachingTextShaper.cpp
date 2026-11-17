//
//  WordCachingTextShaper.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 11/15/22.
//

#include "snap_drawing/cpp/Text/WordCachingTextShaper.hpp"
#include "snap_drawing/cpp/Text/CharactersIterator.hpp"
#include "snap_drawing/cpp/Text/Typeface.hpp"

namespace snap::drawing {

constexpr size_t kWordCacheSize = 1000;
constexpr Scalar kUniformFontSize = 12;

WordCachingTextShaper::WordCachingTextShaper(const Ref<TextShaper>& innerShaper, WordCachingTextShaperStrategy strategy)
    : _innerShaper(innerShaper), _cache(kWordCacheSize), _strategy(strategy) {}
WordCachingTextShaper::~WordCachingTextShaper() = default;

void WordCachingTextShaper::clearCache() {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    _cache.clear();
}

TextParagraphList WordCachingTextShaper::resolveParagraphs(const Character* unicodeText,
                                                           size_t length,
                                                           bool isRightToLeft) {
    return _innerShaper->resolveParagraphs(unicodeText, length, isRightToLeft);
}

size_t WordCachingTextShaper::shape(const Character* unicodeText,
                                    size_t length,
                                    Font& font,
                                    bool isRightToLeft,
                                    Scalar letterSpacing,
                                    TextScript script,
                                    std::vector<ShapedGlyph>& out) {
    std::lock_guard<Valdi::Mutex> lock(_mutex);

    if (font.typeface()->hasSpaceInLigaturesOrKerning()) {
        return _innerShaper->shape(unicodeText, length, font, isRightToLeft, letterSpacing, script, out);
    }

    switch (_strategy) {
        case WordCachingTextShaperStrategy::DisableCache:
            return _innerShaper->shape(unicodeText, length, font, isRightToLeft, letterSpacing, script, out);
        case WordCachingTextShaperStrategy::PrioritizeCacheHit:
            return shapeUsingUniformFont(unicodeText, length, font, isRightToLeft, letterSpacing, script, out);
        case WordCachingTextShaperStrategy::PrioritizeCorrectness:
            return breakdownByWordAndShape(unicodeText, length, font, isRightToLeft, letterSpacing, script, out);
    }
}

size_t WordCachingTextShaper::shapeUsingUniformFont(const Character* unicodeText,
                                                    size_t length,
                                                    Font& font,
                                                    bool isRightToLeft,
                                                    Scalar letterSpacing,
                                                    TextScript script,
                                                    std::vector<ShapedGlyph>& out) {
    auto uniformFont = getUniformFont(font);

    auto written =
        breakdownByWordAndShape(unicodeText, length, *uniformFont, isRightToLeft, letterSpacing, script, out);

    auto* glyphsIt = &out[out.size() - written];

    // Scale the written shaped glyph to account for the difference in scale
    auto ratio = font.resolvedSize() / uniformFont->resolvedSize();

    for (size_t i = 0; i < written; i++) {
        auto& glyph = glyphsIt[i];

        glyph.advanceX *= ratio;
        glyph.offsetX *= ratio;
        glyph.offsetY *= ratio;
    }

    return written;
}

size_t WordCachingTextShaper::breakdownByWordAndShape(const Character* unicodeText,
                                                      size_t length,
                                                      Font& font,
                                                      bool isRightToLeft,
                                                      Scalar letterSpacing,
                                                      TextScript script,
                                                      std::vector<ShapedGlyph>& out) {
    auto sizeBefore = out.size();

    auto fontId = font.getFontId();

    std::optional<ShapedGlyph> spaceGlyph;

    CharactersIterator iterator(unicodeText, length);

    while (!iterator.isAtEnd()) {
        auto c = iterator.current();

        if (c == static_cast<Character>(' ')) {
            if (iterator.hasRange()) {
                shapeWord(&unicodeText[iterator.rangeStart()],
                          iterator.rangeLength(),
                          fontId,
                          font,
                          isRightToLeft,
                          letterSpacing,
                          script,
                          out);
            }

            iterator.advance();
            iterator.resetRange();

            if (!spaceGlyph) {
                spaceGlyph = {getSpaceGlyphForFont(fontId, font)};
            }

            out.emplace_back(spaceGlyph.value());
        } else {
            iterator.advance();
        }
    }

    if (iterator.hasRange()) {
        shapeWord(&unicodeText[iterator.rangeStart()],
                  iterator.rangeLength(),
                  fontId,
                  font,
                  isRightToLeft,
                  letterSpacing,
                  script,
                  out);
    }

    auto outputSize = out.size();

    if (isRightToLeft) {
        std::reverse(out.data() + sizeBefore, out.data() + outputSize);
    }

    return outputSize - sizeBefore;
}

void WordCachingTextShaper::shapeWord(const Character* unicodeText,
                                      size_t length,
                                      FontId fontId,
                                      Font& font,
                                      bool isRightToLeft,
                                      Scalar letterSpacing,
                                      TextScript script,
                                      std::vector<ShapedGlyph>& out) {
    auto cacheKey = TextShaperCacheKey(fontId, letterSpacing, script, isRightToLeft, unicodeText, length);
    auto cacheResult = _cache.find(cacheKey);

    if (cacheResult) {
        copyGlyphs(cacheResult.value().glyphs, cacheResult.value().length, out);
        return;
    }

    auto writtenGlyphsLength =
        _innerShaper->shape(unicodeText, length, font, isRightToLeft, letterSpacing, script, _tmp);

    auto* writtenGlyphs = _tmp.data();

    if (isRightToLeft) {
        std::reverse(writtenGlyphs, writtenGlyphs + writtenGlyphsLength);
    }

    _cache.insert(cacheKey, writtenGlyphs, writtenGlyphsLength);
    copyGlyphs(writtenGlyphs, writtenGlyphsLength, out);
    _tmp.clear();
}

Ref<Font> WordCachingTextShaper::getUniformFont(const Font& font) {
    auto hinting = font.getSkValue().getHinting();
    auto key = (static_cast<uint64_t>(hinting) << 32) | font.typeface()->getId();
    const auto& it = _uniformFonts.find(key);
    if (it != _uniformFonts.end()) {
        return it->second;
    }

    auto uniformFont =
        Valdi::makeShared<Font>(nullptr, Ref<Typeface>(font.typeface()), kUniformFontSize, 1.0, false, hinting);
    _uniformFonts[key] = uniformFont;

    return uniformFont;
}

void WordCachingTextShaper::copyGlyphs(const ShapedGlyph* glyphs, size_t length, std::vector<ShapedGlyph>& out) {
    auto beforeSize = out.size();
    out.resize(beforeSize + length);

    auto* outGlyphs = &out[beforeSize];
    for (size_t i = 0; i < length; i++) {
        outGlyphs[i] = glyphs[i];
    }
}

ShapedGlyph WordCachingTextShaper::getSpaceGlyphForFont(FontId fontId, Font& font) {
    auto text = static_cast<Character>(' ');
    auto cacheKey = TextShaperCacheKey(fontId, 0.0f, TextScript::common(), false, &text, 1);

    auto cacheResult = _cache.find(cacheKey);
    if (cacheResult && cacheResult.value().length == 1) {
        return cacheResult.value().glyphs[0];
    }

    auto spaceGlyphId = font.getSkValue().unicharToGlyph(static_cast<SkUnichar>(text));
    SkScalar width;
    font.getSkValue().getWidths(&spaceGlyphId, 1, &width);

    ShapedGlyph glyph;
    glyph.glyphID = spaceGlyphId;
    glyph.offsetX = 0;
    glyph.offsetY = 0;
    glyph.advanceX = width;
    glyph.setCharacter(text, false);

    _cache.insert(cacheKey, &glyph, 1);

    return glyph;
}

} // namespace snap::drawing
