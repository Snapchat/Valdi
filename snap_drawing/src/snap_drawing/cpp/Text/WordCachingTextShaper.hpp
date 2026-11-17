//
//  WordCachingTextShaper.hpp
//  snap_drawing
//
//  Created by Simon Corsin on 11/15/22.
//

#pragma once

#include "snap_drawing/cpp/Text/TextShaper.hpp"
#include "snap_drawing/cpp/Text/TextShaperCache.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"

namespace snap::drawing {

enum class WordCachingTextShaperStrategy { DisableCache, PrioritizeCacheHit, PrioritizeCorrectness };

/**
 * A TextShaper implementation that breaks down shaping by words and use a cache.
 * The given innerShaper will be used to shape the individual words on a cache miss.
 */
class WordCachingTextShaper : public TextShaper {
public:
    WordCachingTextShaper(const Ref<TextShaper>& innerShaper, WordCachingTextShaperStrategy strategy);
    ~WordCachingTextShaper() override;

    void clearCache() override;

    TextParagraphList resolveParagraphs(const Character* unicodeText, size_t length, bool isRightToLeft) override;

    size_t shape(const Character* unicodeText,
                 size_t length,
                 Font& font,
                 bool isRightToLeft,
                 Scalar letterSpacing,
                 TextScript script,
                 std::vector<ShapedGlyph>& out) override;

private:
    Valdi::Mutex _mutex;
    Ref<TextShaper> _innerShaper;
    TextShaperCache _cache;
    WordCachingTextShaperStrategy _strategy;
    Valdi::FlatMap<uint64_t, Ref<Font>> _uniformFonts;
    std::vector<ShapedGlyph> _tmp;

    size_t shapeUsingUniformFont(const Character* unicodeText,
                                 size_t length,
                                 Font& font,
                                 bool isRightToLeft,
                                 Scalar letterSpacing,
                                 TextScript script,
                                 std::vector<ShapedGlyph>& out);

    size_t breakdownByWordAndShape(const Character* unicodeText,
                                   size_t length,
                                   Font& font,
                                   bool isRightToLeft,
                                   Scalar letterSpacing,
                                   TextScript script,
                                   std::vector<ShapedGlyph>& out);

    void shapeWord(const Character* unicodeText,
                   size_t length,
                   FontId fontId,
                   Font& font,
                   bool isRightToLeft,
                   Scalar letterSpacing,
                   TextScript script,
                   std::vector<ShapedGlyph>& out);

    ShapedGlyph getSpaceGlyphForFont(FontId fontId, Font& font);

    Ref<Font> getUniformFont(const Font& font);

    static void copyGlyphs(const ShapedGlyph* glyphs, size_t length, std::vector<ShapedGlyph>& out);
};

} // namespace snap::drawing
