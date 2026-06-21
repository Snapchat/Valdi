//
//  TextLayoutBuilder.hpp
//  snap_drawing
//
//  Created by Simon Corsin on 9/21/22.
//

#pragma once

#include "snap_drawing/cpp/Text/Font.hpp"
#include "snap_drawing/cpp/Text/TextLayout.hpp"
#include "snap_drawing/cpp/Text/TextShaper.hpp"
#include "snap_drawing/cpp/Utils/Aliases.hpp"
#include "snap_drawing/cpp/Utils/Color.hpp"

#include "snap_drawing/cpp/Utils/Geometry.hpp"

#include "include/core/SkTextBlob.h"

#include <memory>
#include <optional>
#include <string>

namespace snap::drawing {

struct ShapedGlyph;
class FontManager;

struct TextBackgroundStyle {
    std::optional<Color> color;
    TextBackgroundPadding padding;
    BorderRadius borderRadius;

    constexpr bool hasBackground() const {
        return color.has_value();
    }

    constexpr Scalar horizontalPadding() const {
        return padding.left + padding.right;
    }

    bool operator==(const TextBackgroundStyle& other) const {
        return color == other.color && padding == other.padding && borderRadius == other.borderRadius;
    }
};

struct TextLayoutSpecs {
    Ref<Font> font;
    Ref<Valdi::RefCountable> attachment;
    TextLayoutLineHeight lineHeight;
    Scalar letterSpacing;
    TextDecoration textDecoration;
    std::optional<TextCustomUnderlineStyle> customUnderlineStyle;
    size_t colorIndex;
    std::optional<size_t> backgroundStyleIndex;

    TextLayoutSpecs() = default;
    inline TextLayoutSpecs(const Ref<Font>& font,
                           const Ref<Valdi::RefCountable>& attachment,
                           TextLayoutLineHeight lineHeight,
                           Scalar letterSpacing,
                           TextDecoration textDecoration,
                           std::optional<TextCustomUnderlineStyle> customUnderlineStyle,
                           size_t colorIndex,
                           std::optional<size_t> backgroundStyleIndex)
        : font(font),
          attachment(attachment),
          lineHeight(lineHeight),
          letterSpacing(letterSpacing),
          textDecoration(textDecoration),
          customUnderlineStyle(customUnderlineStyle),
          colorIndex(colorIndex),
          backgroundStyleIndex(backgroundStyleIndex) {}

    TextLayoutSpecs withFont(const Ref<Font>& newFont) const;
};

struct TextLayoutBuilderEntry {
    TextLayoutSpecs specs;
    size_t charactersStart;
    size_t charactersEnd;
};

struct TextLayoutBuilderSegment {
    // Where to find the glyphs for this segment
    size_t glyphsStart;

    // How many glyphs should be displayed from glyphs pointer
    size_t glyphsCount;

    // The draw position, within the TextLayout, not taking in account
    // text alignment or vertical offset
    Point drawPosition;

    // The computed bounds of the glyphs, not taking in
    // account the drawPosition.
    Rect bounds;

    // The line number that this segment is drawn into
    size_t lineNumber;

    // The total line metrics that includes the ascent of this segment within the line
    // and all prior segments within the same line.
    LineMetrics lineMetrics;

    // The decoration type to draw on top of the segment
    TextDecoration textDecoration;

    // Optional custom underline geometry for underline decorations.
    std::optional<TextCustomUnderlineStyle> customUnderlineStyle;

    // The resolved font of the segment
    Ref<Font> font;

    // The attachment that was passed in through the append() call
    Ref<Valdi::RefCountable> attachment;

    // The color on which the segment should be drawn
    size_t colorIndex;

    // The background style with which the segment should be drawn
    std::optional<size_t> backgroundStyleIndex;

    bool isRightToLeft;
};

/**
 * TextLayoutBuilderShapeableSegment contains a single resolved font
 * with a resolved text direction.
 */
struct TextLayoutBuilderShapeableSegment {
    // Holds a reference on the TextLayoutBuilderEntry, which was created
    // by the TextLayoutBuilder::append() call.
    const TextLayoutBuilderEntry* entry;
    // Holds a reference on the resolved TextParagraph, which tells what base
    // direction this segment is part of.
    const TextParagraph* paragraph;
    // Holds a reference on the paragraph segment within the paragraph, which tells
    // what direction this shapeable segment should be shaped in.
    const TextSegmentProperties* paragraphSegment;
    // Contains the index where the characters represented by this shapeable segment
    // start within the unicode characters array.
    size_t charactersStart = 0;
    // Contains the index where the characters represented by this shapeable segment
    // end within the unicode characters array.
    size_t charactersEnd = 0;
    // Whether this shapeable segment is the last entry before entering a new paragraph.
    // This could mean that the base text direction will switch from LTR to RTL or
    // vice versa after processing this shapeable segment.
    bool isEndOfParagraph = false;
    // Whether this shapeable segment is the last entry before entering a new paragraph segment.
    bool isEndOfParagraphSegment = false;

    // The resolved font that should be used to shape this segment.
    // Can be null, if no suitable fonts were found for this segment.
    Ref<Font> resolvedFont;

    inline TextLayoutBuilderShapeableSegment(const TextLayoutBuilderEntry* entry,
                                             const TextParagraph* paragraph,
                                             const TextSegmentProperties* paragraphSegment)
        : entry(entry), paragraph(paragraph), paragraphSegment(paragraphSegment) {}
};

/**
 * The TextLayoutBuilderResolvedShapeableSegments contains the list of resolved paragraphs,
 * where each paragraph has a single base direction, and can contain a list of segments that
 * can be either LTR or RTL. A paragraph reads in a logical direction, which is defined
 * by the base direction. The TextLayoutBuilderResolvedShapeableSegments contains the list
 * of segments that can be shaped from the paragraph list, where each shapeable segment
 * is associated with a single font, in a single direction.
 */
struct TextLayoutBuilderResolvedShapeableSegments {
    TextParagraphList paragraphList;
    std::vector<TextLayoutBuilderShapeableSegment> shapeableSegments;
};

/**
 This defines the strategy to use to break sentences into multiple lines.
 */
enum class LineBreakStrategy {
    /**
     Line break will happen between word boundaries.
     */
    ByWord,
    /**
     Line break will happen between any safe to break character.
     */
    ByCharacter

};

class TextLayoutBuilder {
public:
    TextLayoutBuilder(TextAlign textAlign,
                      TextOverflow textOverflow,
                      Size maxSize,
                      int maxLinesCount,
                      const Ref<FontManager>& fontManager,
                      bool isRightToLeft,
                      Scalar displayScale,
                      bool prioritizeFewerFonts = false);
    ~TextLayoutBuilder();

    /**
     * Append a UTF-8 encoded string to the builder, associated with the given layout specs.
     * Returns how many UTF-32 Unicode characters have been added to the builder.
     */
    size_t append(const std::string_view& text,
                  const Ref<Font>& font,
                  TextLayoutLineHeight lineHeight,
                  Scalar letterSpacing,
                  TextDecoration textDecoration,
                  Ref<Valdi::RefCountable> attachment = nullptr,
                  std::optional<Color> color = std::nullopt,
                  std::optional<TextBackgroundStyle> backgroundStyle = std::nullopt,
                  std::optional<TextCustomUnderlineStyle> customUnderlineStyle = std::nullopt);

    void setIncludeSegments(bool includeSegments);

    /**
     Whether the output TextLayout should include an SkTextBlob.
     Without an SkTextBlob, the TextLayout will not be drawable, so disabling
     textBlob output is only suitable when measuring text.
     */
    void setIncludeTextBlob(bool includeTextBlob);

    void setLineBreakStrategy(LineBreakStrategy lineBreakStrategy);

    /**
     * Resolves the Shapeable text segments by processing the entries that were
     * added to the builder. Each shapeable segment defines a set of characters
     * associated with a single font in a single direction (LTR or RTL).
     */
    TextLayoutBuilderResolvedShapeableSegments resolveShapeableSegments() const;

    Ref<TextLayout> build();

private:
    TextAlign _textAlign;
    TextOverflow _textOverflow;
    Size _maxSize;
    Point _position;
    LineMetrics _currentLineMetrics;
    size_t _maxLinesCount;
    size_t _currentNumberOfLines = 1;
    bool _reachedMaxLines = false;
    bool _includeSegments = false;
    bool _includeTextBlob = false;
    bool _isRightToLeft;
    bool _prioritizeFewerFonts;
    Scalar _displayScale;

    Ref<FontManager> _fontManager;

    // The characters from all the append calls converted into utf32
    std::vector<Character> _characters;
    // The entries corresponding to each append call
    std::vector<TextLayoutBuilderEntry> _entries;
    // The combined shaped glyphs from all the shape calls
    std::vector<ShapedGlyph> _glyphs;
    // The resolved segments, there will be one segment per
    // directional run, resolved font, line number and decoration
    std::vector<TextLayoutBuilderSegment> _segments;
    // The list of colors used for the append calls.
    // There will be one entry per unique color, as each color
    // needs to be drawn individually.
    std::vector<std::optional<Color>> _colors;
    std::vector<TextBackgroundStyle> _backgroundStyles;
    Ref<TextShaper> _shaper;
    LineBreakStrategy _lineBreakStrategy = LineBreakStrategy::ByWord;

    struct ShapeResult {
        const ShapedGlyph* glyphsStart;
        const ShapedGlyph* glyphsEnd;

        constexpr ShapeResult(const ShapedGlyph* glyphsStart, const ShapedGlyph* glyphsEnd)
            : glyphsStart(glyphsStart), glyphsEnd(glyphsEnd) {}
    };

    Rect computeChunkBounds(const TextLayoutSpecs& specs, Scalar advance, const FontMetrics& fontMetrics) const;

    /**
     Compute the current line metrics into the given LineMetrics output.
     Returns whether the computed line height fits within the constraints of our container.
     */
    bool computeLineMetrics(const TextLayoutSpecs& specs, const FontMetrics& fontMetrics, LineMetrics& output);

    size_t appendColor(const std::optional<Color>& color);
    std::optional<size_t> appendBackgroundStyle(const std::optional<TextBackgroundStyle>& backgroundStyle);
    const TextBackgroundStyle* getBackgroundStyle(std::optional<size_t> backgroundStyleIndex) const;

    void appendSegment(
        const TextLayoutSpecs& specs, size_t glyphsStart, size_t glyphsCount, const Rect& bounds, bool isRightToLeft);

    void processUnidirectionalRun(const TextLayoutSpecs& specs,
                                  const Ref<Font>& originalFont,
                                  const Character* characters,
                                  size_t charactersLength,
                                  TextScript script,
                                  bool isRTL);

    void buildSegments();
    void doJustify(Scalar containerWidth);
    void processEllipsis(const TextLayoutSpecs& specs);
    void removeLastSegment();
    bool removeCharactersUntilSizeFit(Size size);
    bool shouldProcessEllipsisAfterLineBreak(const ShapedGlyph* begin,
                                             const ShapedGlyph* end,
                                             const ShapedGlyph* next) const;
    bool shouldProcessEllipsisAfterLineHeightUpdate() const;

    ShapeResult shape(const TextLayoutSpecs& specs,
                      const Character* characters,
                      size_t charactersLength,
                      TextScript script,
                      bool isRTL);

    const TextLayoutBuilderEntry* resolveEntryAtCharacter(size_t characterIndex,
                                                          const TextLayoutBuilderEntry* lastEntry) const;

    static void outputSegmentToEntry(TextLayoutEntry& entry,
                                     const ShapedGlyph* glyphsBegin,
                                     const ShapedGlyph* glyphsEnd,
                                     const Rect& bounds,
                                     const Ref<Font>& font);

    void reverseSegmentsHorizontally(size_t fromIndex, size_t toIndex);

    void appendBackgroundIfNeeded(std::vector<TextLayoutVisualEntry>& visualEntries,
                                  const TextLayoutBuilderSegment& segment,
                                  Scalar resolvedSegmentX,
                                  Scalar resolvedSegmentY) const;

    void appendDecorationIfNeeded(std::vector<TextLayoutVisualEntry>& visualEntries,
                                  const TextLayoutBuilderSegment& segment,
                                  const std::optional<Color>& color,
                                  Scalar resolvedSegmentX,
                                  Scalar resolvedSegmentY) const;

    void resolveShapeableSegmentsInSegmentParagraph(const TextParagraph& paragraph,
                                                    const TextSegmentProperties& paragraphSegment,
                                                    const Character* characters,
                                                    size_t start,
                                                    size_t end,
                                                    const TextLayoutBuilderEntry* entry,
                                                    std::vector<TextLayoutBuilderShapeableSegment>& output) const;
};

} // namespace snap::drawing
