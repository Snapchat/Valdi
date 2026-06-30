//
//  TextLayoutBuilder.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 9/21/22.
//

#include "snap_drawing/cpp/Text/TextLayoutBuilder.hpp"
#include "snap_drawing/cpp/Text/CharactersIterator.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/TextShaper.hpp"
#include "snap_drawing/cpp/Text/Unicode.hpp"

#include "snap_drawing/cpp/Utils/UTFUtils.hpp"

#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include <iostream>

namespace snap::drawing {

struct TextLineOffset {
    Scalar horizontalOffset = 0;
    LineMetrics lineMetrics;
};

struct TextLineSize {
    Scalar width = 0;
    LineMetrics lineMetrics;
};

using TextLineOffsetVector = Valdi::SmallVector<TextLineOffset, 2>;

static std::string toUTF8(const ShapedGlyph* glyphsBegin, const ShapedGlyph* glyphsEnd) {
    std::vector<Character> unicode;
    unicode.resize(glyphsEnd - glyphsBegin);

    auto* unicodePtr = unicode.data();

    const auto* it = glyphsBegin;
    while (it != glyphsEnd) {
        *unicodePtr = it->character();
        it++;
        unicodePtr++;
    }

    return unicodeToUtf8(unicode.data(), unicode.size());
}

template<typename T>
static void forEachSegmentsLine(TextLayoutBuilderSegment* begin, TextLayoutBuilderSegment* end, T&& cb) {
    size_t lastLineNumber = 0;
    TextLayoutBuilderSegment* itStartForCurrentLine = nullptr;
    auto* it = begin;
    while (it != end) {
        auto currentLineNumber = it->lineNumber;
        if (currentLineNumber != lastLineNumber) {
            if (itStartForCurrentLine != nullptr) {
                cb(lastLineNumber, itStartForCurrentLine, it);
            }

            itStartForCurrentLine = it;
            lastLineNumber = currentLineNumber;
        }

        it++;
    }

    if (itStartForCurrentLine != nullptr) {
        cb(lastLineNumber, itStartForCurrentLine, it);
    }
}

struct LineBreakResult {
    const ShapedGlyph* end;
    const ShapedGlyph* next;
    bool reachedEndLine;
    bool brokenByNewLineCharacter;

    constexpr LineBreakResult(const ShapedGlyph* end,
                              const ShapedGlyph* next,
                              bool reachedEndLine,
                              bool brokenByNewLineCharacter)
        : end(end), next(next), reachedEndLine(reachedEndLine), brokenByNewLineCharacter(brokenByNewLineCharacter) {}
};

static LineBreakResult lineBreak(const ShapedGlyph* begin,
                                 const ShapedGlyph* end,
                                 LineBreakStrategy strategy,
                                 Scalar containerWidth,
                                 Scalar accumulatedWidth) {
    const auto* it = begin;
    // The last iterator that we can safely break on
    const auto* lastSafeBreakIt = it;
    auto prevWasWhitespace = false;

    while (it < end) {
        if (Unicode::isNewline(it->character())) {
            return LineBreakResult(it, it + 1, true, true);
        }

        accumulatedWidth += it->advanceX;

        auto isWhitespace = Unicode::isBreakingWhitespace(it->character());

        switch (strategy) {
            case LineBreakStrategy::ByWord:
                if (isWhitespace && !prevWasWhitespace) {
                    lastSafeBreakIt = it;
                }
                break;
            case LineBreakStrategy::ByCharacter:
                if (!it->unsafeToBreak() && !prevWasWhitespace) {
                    lastSafeBreakIt = it;
                }
                break;
        }

        if (accumulatedWidth > containerWidth) {
            // Reached the end of our container

            if (!isWhitespace) {
                // If we are not in a whitespace, we move back the iterator
                // down to our first safe breaking point in order to consume
                // all the breaking whitespaces that might be there.
                // This is to set our next iterator on the first word instead
                // of having leading whitespaces.
                it = lastSafeBreakIt;
            }

            // Eat up all the trailing whitespaces
            while (it < end && Unicode::isBreakingWhitespace(it->character())) {
                it++;
            }

            return LineBreakResult(lastSafeBreakIt, it, true, false);
        }

        prevWasWhitespace = isWhitespace;
        it++;
    }

    return LineBreakResult(it, it, false, false);
}

static Scalar computeAdvance(const ShapedGlyph* shapedGlyphs, size_t length) {
    auto advance = 0.0f;

    for (size_t i = 0; i < length; i++) {
        advance += shapedGlyphs[i].advanceX;
    }

    return advance;
}

static size_t appendUniqueColor(std::vector<std::optional<Color>>& colors, const std::optional<Color>& color) {
    for (size_t i = 0; i < colors.size(); i++) {
        if (colors[i] == color) {
            return i;
        }
    }

    auto previousSize = colors.size();
    colors.emplace_back(color);
    return previousSize;
}

static size_t appendUniqueBackgroundStyle(std::vector<TextBackgroundStyle>& styles, const TextBackgroundStyle& style) {
    for (size_t i = 0; i < styles.size(); i++) {
        if (styles[i] == style) {
            return i;
        }
    }

    auto previousSize = styles.size();
    styles.emplace_back(style);
    return previousSize;
}

TextLayoutSpecs TextLayoutSpecs::withFont(const Ref<Font>& newFont) const {
    return TextLayoutSpecs(newFont,
                           attachment,
                           replacementSize,
                           replacementVerticalAlignment,
                           lineHeight,
                           letterSpacing,
                           textDecoration,
                           customUnderlineStyle,
                           colorIndex,
                           backgroundStyleIndex);
}

static LineMetrics resolveReplacementLineMetrics(const LineMetrics& lineMetrics,
                                                 const Size& replacementSize,
                                                 Valdi::InlineViewVerticalAlignment verticalAlignment) {
    switch (verticalAlignment) {
        case Valdi::InlineViewVerticalAlignment::Top:
            return LineMetrics(lineMetrics.ascent, lineMetrics.ascent + replacementSize.height);
        case Valdi::InlineViewVerticalAlignment::Bottom:
            return LineMetrics(lineMetrics.descent - replacementSize.height, lineMetrics.descent);
        case Valdi::InlineViewVerticalAlignment::Baseline:
            return LineMetrics(-replacementSize.height, 0);
        case Valdi::InlineViewVerticalAlignment::Center: {
            auto textCenter = (lineMetrics.ascent + lineMetrics.descent) / 2.0f;
            auto ascent = textCenter - (replacementSize.height / 2.0f);
            return LineMetrics(ascent, ascent + replacementSize.height);
        }
    }
    auto textCenter = (lineMetrics.ascent + lineMetrics.descent) / 2.0f;
    auto ascent = textCenter - (replacementSize.height / 2.0f);
    return LineMetrics(ascent, ascent + replacementSize.height);
}

LineMetrics TextLayoutLineHeight::getLineMetrics(const FontMetrics& fontMetrics) const {
    auto lineHeightMultiple = _value;
    if (_kind == TextLayoutLineHeight::Kind::Absolute) {
        auto fontLineHeight = fontMetrics.descent - fontMetrics.ascent;
        lineHeightMultiple = fontLineHeight > 0 ? _value / fontLineHeight : 1.0f;
    }

    return LineMetrics(fontMetrics.ascent * lineHeightMultiple, fontMetrics.descent * lineHeightMultiple);
}

TextLayoutBuilder::TextLayoutBuilder(TextAlign textAlign,
                                     TextOverflow textOverflow,
                                     Size maxSize,
                                     int maxLinesCount,
                                     const Ref<FontManager>& fontManager,
                                     bool isRightToLeft,
                                     Scalar displayScale,
                                     bool prioritizeFewerFonts)
    : _textAlign(textAlign),
      _textOverflow(textOverflow),
      _maxSize(maxSize),
      _position(Point::makeEmpty()),
      _maxLinesCount(maxLinesCount),
      _isRightToLeft(isRightToLeft),
      _prioritizeFewerFonts(prioritizeFewerFonts),
      _displayScale(displayScale),
      _fontManager(fontManager),
      _shaper(fontManager != nullptr ? fontManager->getTextShaper() : nullptr) {}

TextLayoutBuilder::~TextLayoutBuilder() = default;

void TextLayoutBuilder::appendSegment(
    const TextLayoutSpecs& specs, size_t glyphsStart, size_t glyphsCount, const Rect& bounds, bool isRightToLeft) {
    auto& segment = _segments.emplace_back();
    segment.glyphsStart = glyphsStart;
    segment.glyphsCount = glyphsCount;
    segment.bounds = bounds;
    segment.lineNumber = _currentNumberOfLines;
    segment.drawPosition = _position;
    segment.lineMetrics = _currentLineMetrics;
    segment.font = specs.font;
    segment.attachment = specs.attachment;
    segment.isReplacement = specs.replacementSize.has_value();
    segment.replacementSize = specs.replacementSize;
    segment.colorIndex = specs.colorIndex;
    segment.backgroundStyleIndex = specs.backgroundStyleIndex;
    segment.isRightToLeft = isRightToLeft;
    segment.textDecoration = specs.textDecoration;
    segment.customUnderlineStyle = specs.customUnderlineStyle;
}

[[maybe_unused]] static void outputDebug(const ShapedGlyph* shapedGlyphs,
                                         size_t consumedGlyphCount,
                                         const Point& advance,
                                         const Rect& bounds) {
    auto utf8 = toUTF8(shapedGlyphs, shapedGlyphs + consumedGlyphCount);

    std::cout << std::endl;
    std::cout << "For text: \"" << utf8 << "\"" << std::endl;
    std::cout << "Advance is " << advance << std::endl;
    std::cout << "Bounds is " << bounds << std::endl;
    std::cout << "Width is " << bounds.width() << std::endl;
}

bool TextLayoutBuilder::computeLineMetrics(const TextLayoutSpecs& specs,
                                           const FontMetrics& fontMetrics,
                                           LineMetrics& output) {
    output = _currentLineMetrics;
    auto lineMetrics = specs.lineHeight.getLineMetrics(fontMetrics);

    if (specs.replacementSize) {
        auto replacementLineMetrics =
            resolveReplacementLineMetrics(lineMetrics, specs.replacementSize.value(), specs.replacementVerticalAlignment);
        lineMetrics.join(replacementLineMetrics);
    }

    if (const auto* backgroundStyle = getBackgroundStyle(specs.backgroundStyleIndex)) {
        lineMetrics.ascent -= backgroundStyle->padding.top;
        lineMetrics.descent += backgroundStyle->padding.bottom;
    }

    output.join(lineMetrics);
    return _position.y + output.height() <= _maxSize.height;
}

Rect TextLayoutBuilder::computeChunkBounds(const TextLayoutSpecs& specs,
                                           Scalar advance,
                                           const FontMetrics& fontMetrics) const {
    auto left = static_cast<Scalar>(0);
    auto right = specs.replacementSize ? specs.replacementSize->width : advance;
    auto lineMetrics = specs.lineHeight.getLineMetrics(fontMetrics);

    if (specs.replacementSize) {
        lineMetrics =
            resolveReplacementLineMetrics(lineMetrics, specs.replacementSize.value(), specs.replacementVerticalAlignment);
    }

    if (const auto* backgroundStyle = getBackgroundStyle(specs.backgroundStyleIndex)) {
        lineMetrics.ascent -= backgroundStyle->padding.top;
        right += backgroundStyle->horizontalPadding();
        lineMetrics.descent += backgroundStyle->padding.bottom;
    }

    auto bounds = Rect::makeLTRB(left, lineMetrics.ascent, right, lineMetrics.descent);

    return bounds;
}

void TextLayoutBuilder::removeLastSegment() {
    auto lastIndex = _segments.size() - 1;
    auto& lastSegment = _segments[lastIndex];

    _position = lastSegment.drawPosition;
    _segments.erase(_segments.begin() + lastIndex);

    if (_segments.empty()) {
        _currentLineMetrics.ascent = 0;
        _currentLineMetrics.descent = 0;
        _currentNumberOfLines = 1;
    } else {
        auto& newLastSegment = _segments[_segments.size() - 1];
        _currentLineMetrics = newLastSegment.lineMetrics;
        _currentNumberOfLines = newLastSegment.lineNumber;
    }
}

bool TextLayoutBuilder::removeCharactersUntilSizeFit(Size size) {
    for (;;) {
        if (_position.x + size.width <= _maxSize.width && _position.y + size.height <= _maxSize.height) {
            // Our size now fits
            return true;
        }

        if (_segments.empty()) {
            // No segment to remove, we can't fit our size
            return false;
        }

        auto lastIndex = _segments.size() - 1;

        auto& lastSegment = _segments[lastIndex];

        if (lastSegment.lineNumber != _currentNumberOfLines) {
            // We are processing a segment that is not on the current line.
            // Move back to the line of the segment.
            _currentNumberOfLines = lastSegment.lineNumber;
            _position = lastSegment.drawPosition;
            _position.x += lastSegment.bounds.width();

            continue;
        }

        if (_position.y + std::max(lastSegment.lineMetrics.height(), size.height) > _maxSize.height) {
            // The whole computed line height doesn't fit our container height.
            // We can remove the entire segment
            removeLastSegment();

            continue;
        }

        const auto* glyphsStart = _glyphs.data() + lastSegment.glyphsStart;
        const auto* glyphsEnd = glyphsStart + lastSegment.glyphsCount;

        // Do a line break to compute how much characters of this segment we can fit
        auto lineBreakResult = lineBreak(glyphsStart,
                                         glyphsEnd,
                                         LineBreakStrategy::ByCharacter,
                                         _maxSize.width,
                                         size.width + lastSegment.drawPosition.x);

        size_t newGlyphsCount = lineBreakResult.end - glyphsStart;

        if (newGlyphsCount == 0) {
            // No glyphs can fit, remove the entire segment
            removeLastSegment();
            continue;
        }

        lastSegment.glyphsCount = newGlyphsCount;

        // Update the bounds
        auto advance = computeAdvance(glyphsStart, newGlyphsCount);

        auto newRight = lastSegment.bounds.left + advance;
        SC_ABORT_UNLESS(newRight < lastSegment.bounds.right, "Computed advance should be smaller");
        lastSegment.bounds.right = newRight;

        // Update current position
        _position = lastSegment.drawPosition;
        _position.x += advance;
    }
}

void TextLayoutBuilder::processEllipsis(const TextLayoutSpecs& specs) {
    _reachedMaxLines = true;

    if (_textOverflow != TextOverflowEllipsis) {
        return;
    }

    Character c = 0x2026; // Code point for character '…'

    TextLayoutSpecs resolvedSpecs = specs;

    if (!specs.font->typeface()->supportsCharacter(c)) {
        auto result = _fontManager->getCompatibleFont(specs.font, nullptr, 0, c);
        if (!result) {
            return;
        }
        resolvedSpecs = resolvedSpecs.withFont(result.value());
    }

    auto shapeResult = shape(resolvedSpecs, &c, 1, TextScript::common(), false);

    const auto& fontMetrics = resolvedSpecs.font->metrics();

    auto advance = computeAdvance(shapeResult.glyphsStart, shapeResult.glyphsEnd - shapeResult.glyphsStart);
    auto bounds = computeChunkBounds(resolvedSpecs, advance, fontMetrics);

    if (!removeCharactersUntilSizeFit(bounds.size())) {
        // Cannot fit our ... within the current line
        return;
    }

    appendSegment(resolvedSpecs,
                  shapeResult.glyphsStart - _glyphs.data(),
                  shapeResult.glyphsEnd - shapeResult.glyphsStart,
                  bounds,
                  false);

    _position.x += advance;
}

TextLayoutBuilder::ShapeResult TextLayoutBuilder::shape(
    const TextLayoutSpecs& specs, const Character* characters, size_t charactersLength, TextScript script, bool isRTL) {
    auto glyphsStart = _glyphs.size();
    auto glyphsLength =
        _shaper->shape(characters, charactersLength, *specs.font, isRTL, specs.letterSpacing, script, _glyphs);

    if (isRTL) {
        // Put the glyphs into processing order, so that we process the characters from right to left
        std::reverse(_glyphs.begin() + glyphsStart, _glyphs.end());
    }

    auto* shapedGlyphStart = _glyphs.data();
    auto* shapedGlyphIt = shapedGlyphStart + glyphsStart;
    auto* shapedGlyphEnd = shapedGlyphIt + glyphsLength;

    // Round advances to pixel grid
    for (size_t i = 0; i < glyphsLength; i++) {
        shapedGlyphIt[i].advanceX = roundf(shapedGlyphIt[i].advanceX);
    }

    return TextLayoutBuilder::ShapeResult(shapedGlyphIt, shapedGlyphEnd);
}

bool TextLayoutBuilder::shouldProcessEllipsisAfterLineBreak(const ShapedGlyph* begin,
                                                            const ShapedGlyph* end,
                                                            const ShapedGlyph* next) const {
    if (_maxLinesCount != 0 && _currentNumberOfLines + 1 > _maxLinesCount) {
        // We reached our maximum number of lines
        return true;
    }

    if (_position.x == 0 && begin == end && end == next) {
        // We are at the beginning of the line, and we were not able
        // to process any characters. We should break now as we will not
        // be able to fit any more characters.
        return true;
    }

    return false;
}

bool TextLayoutBuilder::shouldProcessEllipsisAfterLineHeightUpdate() const {
    return _position.y + _currentLineMetrics.height() > _maxSize.height;
}

void TextLayoutBuilder::processUnidirectionalRun(const TextLayoutSpecs& specs,
                                                 const Ref<Font>& originalFont,
                                                 const Character* characters,
                                                 size_t charactersLength,
                                                 TextScript script,
                                                 bool isRTL) {
    if (_reachedMaxLines) {
        return;
    }

    auto shapeResult = shape(specs, characters, charactersLength, script, isRTL);
    if (specs.replacementSize) {
        auto glyphStartIndex = shapeResult.glyphsStart - _glyphs.data();
        auto glyphEndIndex = shapeResult.glyphsEnd - _glyphs.data();
        for (auto glyphIndex = glyphStartIndex; glyphIndex < glyphEndIndex; glyphIndex++) {
            _glyphs[glyphIndex].advanceX =
                glyphIndex == glyphStartIndex ? specs.replacementSize->width : 0.0f;
        }
    }

    const auto* shapedGlyphIt = shapeResult.glyphsStart;
    const auto& fontMetrics = specs.font->metrics();
    const auto* backgroundStyle = getBackgroundStyle(specs.backgroundStyleIndex);
    const auto backgroundHorizontalPadding =
        backgroundStyle != nullptr ? backgroundStyle->horizontalPadding() : static_cast<Scalar>(0);
    LineMetrics lineMetrics;

    // Resolve all the segments of our blob.
    // We emit one segment per line.

    while (shapedGlyphIt < shapeResult.glyphsEnd && !_reachedMaxLines) {
        if (!computeLineMetrics(specs, fontMetrics, lineMetrics)) {
            // We reached the bottom of our container.
            processEllipsis(specs.withFont(originalFont));
            return;
        }

        // Consume the unicode string until we reach the max size or a new line character
        auto result = lineBreak(shapedGlyphIt,
                                shapeResult.glyphsEnd,
                                _lineBreakStrategy,
                                _maxSize.width,
                                _position.x + backgroundHorizontalPadding);

        auto shouldProcessEllipsis = false;
        if (result.reachedEndLine && shouldProcessEllipsisAfterLineBreak(shapedGlyphIt, result.end, result.next)) {
            // We reached en end of our container and we need to append an ellipsis.
            // In this case we actually want to break by character so that we can remove
            // the trailing characters to fit the ellipsis.
            shouldProcessEllipsis = true;
            result = lineBreak(shapedGlyphIt,
                               shapeResult.glyphsEnd,
                               LineBreakStrategy::ByCharacter,
                               _maxSize.width,
                               _position.x + backgroundHorizontalPadding);
        }

        auto consumedGlyphCount = result.end - shapedGlyphIt;

        if (consumedGlyphCount > 0 || result.brokenByNewLineCharacter) {
            // Update the lineHeight if we consumed characters or if we added newlines
            _currentLineMetrics = lineMetrics;
        }

        if (consumedGlyphCount > 0) {
            // Measure the space consumed by our chunk.
            auto advance = computeAdvance(shapedGlyphIt, consumedGlyphCount);
            auto bounds = computeChunkBounds(specs, advance, fontMetrics);
            appendSegment(specs, shapedGlyphIt - _glyphs.data(), consumedGlyphCount, bounds, isRTL);
            _position.x += bounds.width();
        }

        if (result.reachedEndLine) {
            if (shouldProcessEllipsis) {
                processEllipsis(specs.withFont(originalFont));
                return;
            }

            _position.x = 0;
            _position.y += _currentLineMetrics.height();
            _currentLineMetrics.ascent = 0;
            _currentLineMetrics.descent = 0;
            _currentNumberOfLines++;

            if (result.brokenByNewLineCharacter) {
                if (!computeLineMetrics(specs, fontMetrics, lineMetrics)) {
                    // We reached the bottom of our container.
                    processEllipsis(specs.withFont(originalFont));
                    return;
                } else {
                    _currentLineMetrics = lineMetrics;
                }
            }
        }

        shapedGlyphIt = result.next;
    }
}

size_t TextLayoutBuilder::appendColor(const std::optional<Color>& color) {
    return appendUniqueColor(_colors, color);
}

std::optional<size_t> TextLayoutBuilder::appendBackgroundStyle(
    const std::optional<TextBackgroundStyle>& backgroundStyle) {
    if (!backgroundStyle || !backgroundStyle->hasBackground()) {
        return std::nullopt;
    }

    return appendUniqueBackgroundStyle(_backgroundStyles, backgroundStyle.value());
}

const TextBackgroundStyle* TextLayoutBuilder::getBackgroundStyle(std::optional<size_t> backgroundStyleIndex) const {
    if (!backgroundStyleIndex) {
        return nullptr;
    }

    return &_backgroundStyles[backgroundStyleIndex.value()];
}

size_t TextLayoutBuilder::append(const std::string_view& text,
                                 const Ref<Font>& font,
                                 TextLayoutLineHeight lineHeight,
                                 Scalar letterSpacing,
                                 TextDecoration textDecoration,
                                 Ref<Valdi::RefCountable> attachment,
                                 std::optional<Color> color,
                                 std::optional<TextBackgroundStyle> backgroundStyle,
                                 std::optional<TextCustomUnderlineStyle> customUnderlineStyle,
                                 std::optional<Size> replacementSize,
                                 Valdi::InlineViewVerticalAlignment replacementVerticalAlignment) {
    if (font == nullptr || text.empty()) {
        return 0;
    }

    auto unicodeCount = utf8ToUnicode(text, _characters);
    if (unicodeCount == 0) {
        return 0;
    }

    auto charactersStart = _characters.size() - unicodeCount;

    auto& entry = _entries.emplace_back();
    entry.specs.font = font;
    entry.specs.lineHeight = lineHeight;
    entry.specs.letterSpacing = letterSpacing;
    entry.specs.textDecoration = textDecoration;
    entry.specs.customUnderlineStyle = customUnderlineStyle;
    entry.specs.attachment = std::move(attachment);
    entry.specs.replacementSize = replacementSize;
    entry.specs.replacementVerticalAlignment = replacementVerticalAlignment;
    entry.specs.colorIndex = appendColor(color);
    entry.specs.backgroundStyleIndex = appendBackgroundStyle(backgroundStyle);
    entry.charactersStart = charactersStart;
    entry.charactersEnd = charactersStart + unicodeCount;

    return unicodeCount;
}

static void appendLineOffset(TextLineOffsetVector& lineOffsets,
                             size_t lineNumber,
                             TextAlign textAlign,
                             Scalar containerWidth,
                             Scalar lineWidth,
                             LineMetrics lineMetrics) {
    auto index = lineNumber - 1;
    while (index >= lineOffsets.size()) {
        lineOffsets.emplace_back();
    }

    auto& lineOffset = lineOffsets[index];
    lineOffset.lineMetrics = lineMetrics;

    switch (textAlign) {
        case TextAlignLeft:
            lineOffset.horizontalOffset = 0;
            break;
        case TextAlignRight:
            lineOffset.horizontalOffset = containerWidth - lineWidth;
            break;
        case TextAlignCenter:
            lineOffset.horizontalOffset = containerWidth / 2 - lineWidth / 2;
            break;
        case TextAlignJustify:
            lineOffset.horizontalOffset = 0;
            break;
    }
}

static TextLineSize getSegmentsLineSize(const TextLayoutBuilderSegment* begin, const TextLayoutBuilderSegment* end) {
    TextLineSize size;
    const auto* it = begin;
    while (it != end) {
        size.width = std::max(size.width, it->drawPosition.x + it->bounds.width());
        size.lineMetrics.join(it->lineMetrics);
        it++;
    }
    return size;
}

template<typename T>
static void forEachSegmentsLineWidth(TextLayoutBuilderSegment* begin, TextLayoutBuilderSegment* end, T&& cb) {
    forEachSegmentsLine(begin, end, [&](size_t lineNumber, const auto* begin, const auto* end) {
        TextLineSize lineSize = getSegmentsLineSize(begin, end);

        cb(lineNumber, lineSize.width);
    });
}

static Scalar calculateMaxLineWidth(TextLayoutBuilderSegment* begin, TextLayoutBuilderSegment* end) {
    Scalar maxLineWidth = 0;

    forEachSegmentsLineWidth(
        begin, end, [&](size_t /*lineNumber*/, Scalar lineWidth) { maxLineWidth = std::max(lineWidth, maxLineWidth); });

    return maxLineWidth;
}

static Scalar resolveContainerWidth(TextLayoutBuilderSegment* begin,
                                    TextLayoutBuilderSegment* end,
                                    Scalar suggestedContainerWidth) {
    if (suggestedContainerWidth == std::numeric_limits<Scalar>::max()) {
        return calculateMaxLineWidth(begin, end);
    } else {
        return suggestedContainerWidth;
    }
}

static void calculateLineOffsets(TextLineOffsetVector& lineOffsets,
                                 TextLayoutBuilderSegment* begin,
                                 TextLayoutBuilderSegment* end,
                                 TextAlign textAlign,
                                 Scalar containerWidth) {
    forEachSegmentsLine(begin, end, [&](size_t lineNumber, const auto* begin, const auto* end) {
        TextLineSize lineSize = getSegmentsLineSize(begin, end);

        appendLineOffset(lineOffsets, lineNumber, textAlign, containerWidth, lineSize.width, lineSize.lineMetrics);
    });
}

void TextLayoutBuilder::setIncludeSegments(bool includeSegments) {
    _includeSegments = includeSegments;
}

void TextLayoutBuilder::setIncludeTextBlob(bool includeTextBlob) {
    _includeTextBlob = includeTextBlob;
}

void TextLayoutBuilder::setLineBreakStrategy(LineBreakStrategy lineBreakStrategy) {
    _lineBreakStrategy = lineBreakStrategy;
}

void TextLayoutBuilder::outputSegmentToEntry(TextLayoutEntry& entry,
                                             const ShapedGlyph* glyphsBegin,
                                             const ShapedGlyph* glyphsEnd,
                                             const Rect& bounds,
                                             const Ref<Font>& font) {
    auto& segment = entry.segments.emplace_back();
    segment.bounds = bounds;
    segment.font = font;
    segment.characters = toUTF8(glyphsBegin, glyphsEnd);
}

void TextLayoutBuilder::reverseSegmentsHorizontally(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= toIndex) {
        // fromIndex could be larger than toIndex if there were segments removed to fit a "...".
        return;
    }

    forEachSegmentsLine(_segments.data() + fromIndex,
                        _segments.data() + toIndex,
                        [&](size_t /*lineNumber*/, TextLayoutBuilderSegment* begin, TextLayoutBuilderSegment* end) {
                            auto drawPositionX = begin->drawPosition.x;

                            std::reverse(begin, end);

                            auto* it = begin;
                            while (it != end) {
                                it->drawPosition.x = drawPositionX;
                                drawPositionX += it->bounds.width();
                                it++;
                            }
                        });
}

static inline bool entryIncludesCharacter(const TextLayoutBuilderEntry& entry, size_t characterIndex) {
    return characterIndex >= entry.charactersStart && characterIndex < entry.charactersEnd;
}

const TextLayoutBuilderEntry* TextLayoutBuilder::resolveEntryAtCharacter(
    size_t characterIndex, const TextLayoutBuilderEntry* lastEntry) const {
    if (lastEntry != nullptr && entryIncludesCharacter(*lastEntry, characterIndex)) {
        return lastEntry;
    }

    for (const auto& entry : _entries) {
        if (entryIncludesCharacter(entry, characterIndex)) {
            return &entry;
        }
    }

    SC_ASSERT_FAIL("Unable to resolve matching entry for character");
    std::abort();
}

static bool fontSupportsCharacter(const Ref<Font>& font, Character c) {
    return Unicode::isNewline(c) || font->typeface()->supportsCharacter(c);
}

static bool fontSupportsAllCharacters(Ref<Font>& font,
                                      const Character* charactersStart,
                                      const Character* charactersEnd) {
    for (const auto* character = charactersStart; character < charactersEnd; character++) {
        if (!fontSupportsCharacter(font, *character)) {
            return false;
        }
    }
    return true;
}

void processFewerFonts(TextLayoutBuilderResolvedShapeableSegments& output, const Character* characters) {
    Ref<Font> candidateFont = nullptr;
    auto it = output.shapeableSegments.rbegin();
    while (it != output.shapeableSegments.rend()) {
        bool attemptMerge = false;
        if (candidateFont == nullptr) {
            candidateFont = it->resolvedFont;
        } else if (it->resolvedFont == nullptr) {
            candidateFont = nullptr;
        } else if (it->resolvedFont != candidateFont) {
            // Re-evaluate the option to use the candidate font for this shapeable segment
            if (fontSupportsAllCharacters(
                    candidateFont, characters + it->charactersStart, characters + it->charactersEnd)) {
                attemptMerge = true;
                it->resolvedFont = candidateFont;
            } else {
                attemptMerge = false;
                candidateFont = it->resolvedFont;
            }
        } else {
            attemptMerge = true;
        }

        if (attemptMerge) {
            auto& previousShapableSegment = *std::prev(it);

            if (!it->isEndOfParagraphSegment && !it->isEndOfParagraph && it->entry == previousShapableSegment.entry &&
                it->paragraphSegment == previousShapableSegment.paragraphSegment &&
                previousShapableSegment.charactersStart == it->charactersEnd) {
                previousShapableSegment.charactersStart = it->charactersStart;

                // The following erases the item pointed by 'it' while returning an iterator to the next (in reverse
                // order) item. This is a bit convoluted but required as erase() operates on iterator, not reverse
                // iterator
                it = decltype(it)(output.shapeableSegments.erase(std::next(it).base()));
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

static bool characterNeedsFontSupport(Character c) {
    return !(Unicode::isBreakingWhitespace(c) || Unicode::isNewline(c) || Unicode::isUnbreakableMarker(c));
}

void TextLayoutBuilder::resolveShapeableSegmentsInSegmentParagraph(
    const TextParagraph& paragraph,
    const TextSegmentProperties& paragraphSegment,
    const Character* characters,
    size_t start,
    size_t end,
    const TextLayoutBuilderEntry* entry,
    std::vector<TextLayoutBuilderShapeableSegment>& output) const {
    Ref<Font> currentFont;
    Ref<Font> fallbackFont;
    bool currentFontIsEmoji = entry->specs.font->typeface()->isEmoji();
    CharactersIterator iterator(characters, start, end);

    while (!iterator.isAtEnd()) {
        auto c = iterator.current();

        if (characterNeedsFontSupport(c) || (currentFontIsEmoji && Unicode::isBreakingWhitespace(c))) {
            // We attempt to resolve a new font whenever encountering a character that needs font support,
            // or our current font is emoji and that we find a space. The latter is to deal with issues
            // in spaces width in Skia, which end up being very large with emojis.
            Font* resolvedFont;
            if (fontSupportsCharacter(entry->specs.font, c)) {
                resolvedFont = entry->specs.font.get();
            } else if (fallbackFont != nullptr && fontSupportsCharacter(fallbackFont, c)) {
                resolvedFont = fallbackFont.get();
            } else {
                auto matchedFont = _fontManager->getCompatibleFont(entry->specs.font, nullptr, 0, c);
                if (matchedFont) {
                    fallbackFont = matchedFont.moveValue();
                } else {
                    fallbackFont = nullptr;
                }
                resolvedFont = fallbackFont.get();
            }

            if (currentFont.get() != resolvedFont) {
                if (currentFont != nullptr && iterator.hasRange()) {
                    auto& currentShapeableSegment = output.emplace_back(entry, &paragraph, &paragraphSegment);
                    currentShapeableSegment.resolvedFont = currentFont;
                    currentShapeableSegment.charactersStart = iterator.rangeStart();
                    currentShapeableSegment.charactersEnd = iterator.rangeEnd();
                    iterator.resetRange();
                }

                currentFont = resolvedFont;
                currentFontIsEmoji = resolvedFont != nullptr ? resolvedFont->typeface()->isEmoji() : false;
            }
            iterator.advance();

            if (currentFont == nullptr) {
                iterator.resetRange();
            }
        } else {
            iterator.advance();
        }
    }

    if (iterator.hasRange()) {
        auto& currentShapeableSegment = output.emplace_back(entry, &paragraph, &paragraphSegment);
        currentShapeableSegment.charactersStart = iterator.rangeStart();
        currentShapeableSegment.charactersEnd = iterator.rangeEnd();
        currentShapeableSegment.resolvedFont = currentFont != nullptr ? currentFont : entry->specs.font;
    }
}

TextLayoutBuilderResolvedShapeableSegments TextLayoutBuilder::resolveShapeableSegments() const {
    TextLayoutBuilderResolvedShapeableSegments output;

    /**
     Directional layout algorithm is as follow:
     - Resolve the text direction segments using the TextShaper
     - For each directional segment:
         - Resolve the shapeable segments, which are parts of the strings that can be processed by a single font.
         - For each shapeable segment:
           - Shape the segment
           - If the segment is RTL, flip the emitted glyphs, to process them in the logical order
           - Layout the shaped glyphs across lines
         - If the directional segment is RTL, and that the overall layout is LTR, flip the emitted sentences across
     lines.
         - If the directional segment is LTR, and that the overall layout is RTL, flip the emitted sentences across
     lines.
     - If the overall layout is RTL, flip all the emitted sentences across lines.
     - When emitting the final text blob, the glyphs for each emitted RTL segment are added in reverse order, which is
       the natural display order for those segments.

     Example string: "123 He Yo 4567 YO 89"
     The string above is an example of a bidi string in LTR and RTL.
     The numbers represent RTL region and the letters represent LTR regions.
     We make the assumption that "1", "23", "He", "Yo", "456", "7" need different fonts.
     We also simulate a line breaking in the example.

     Algorithm in LTR:
     Direction segments: [123], [He Yo], [4567], [YO], [89]
     Shapeable segments: [1] [23] [He] [Yo] [456] [7] [YO] [89]
     Shape and layout:       [1][23] [He] [Yo]
                             [456][7] [YO] [89]
     Flipping RTL segments:  [23][1] [He] [Yo]
                             [7][456] [YO] [89]
     Text blob output:  321 He Yo
                        7654 YO 98

     Algorithm in RTL:
     Direction segments: [123] [He Yo] [4567] [YO] [89]
     Shapeable segments: [1] [23] [He] [Yo] [456] [7] [YO] [89]
     Shape and layout:       [1][23] [He] [Yo]
                             [456][7] [YO] [89]
     Flipping LTR segments:  [1][23] [Yo] [He]
                             [456][7] [YO] [89]
     Flipping all segments:  [He] [Yo] [23][1]
                             [89] [YO] [7]456]
     Text blob output:       He Yo 321
                             98 YO 7654
     */

    const auto* characters = _characters.data();
    {
        VALDI_TRACE("SnapDrawing.resolveParagraphs");
        output.paragraphList = _shaper->resolveParagraphs(characters, _characters.size(), _isRightToLeft);
    }

    const TextLayoutBuilderEntry* entry = nullptr;

    VALDI_TRACE("SnapDrawing.resolveFontCollection");
    for (const auto& paragraph : output.paragraphList) {
        for (const auto& segment : paragraph.segments) {
            CharactersIterator iterator(characters, segment.start, segment.end);
            const TextLayoutBuilderEntry* lastEntry = nullptr;

            while (!iterator.isAtEnd()) {
                entry = resolveEntryAtCharacter(iterator.position(), entry);
                if (entry != lastEntry) {
                    if (iterator.hasRange()) {
                        resolveShapeableSegmentsInSegmentParagraph(paragraph,
                                                                   segment,
                                                                   characters,
                                                                   iterator.rangeStart(),
                                                                   iterator.rangeEnd(),
                                                                   lastEntry,
                                                                   output.shapeableSegments);
                    }

                    iterator.resetRange();

                    lastEntry = entry;
                }
                iterator.advance();
            }

            if (iterator.hasRange()) {
                resolveShapeableSegmentsInSegmentParagraph(paragraph,
                                                           segment,
                                                           characters,
                                                           iterator.rangeStart(),
                                                           iterator.rangeEnd(),
                                                           lastEntry,
                                                           output.shapeableSegments);
            }
        }
    }

    if (!output.shapeableSegments.empty()) {
        // Compute endOfParagraph and endOfParagraphSegment flags
        const TextParagraph* lastParagraph = nullptr;
        const TextSegmentProperties* lastSegment = nullptr;

        size_t index = output.shapeableSegments.size();
        while (index > 0) {
            index--;
            auto& shapeableSegment = output.shapeableSegments[index];
            if (shapeableSegment.paragraph != lastParagraph) {
                lastParagraph = shapeableSegment.paragraph;
                shapeableSegment.isEndOfParagraph = true;
            }
            if (shapeableSegment.paragraphSegment != lastSegment) {
                lastSegment = shapeableSegment.paragraphSegment;
                shapeableSegment.isEndOfParagraphSegment = true;
            }
        }
    }

    if (_prioritizeFewerFonts) {
        processFewerFonts(output, characters);
    }
    return output;
}

void TextLayoutBuilder::buildSegments() {
    // Preallocate glyphs to reduce chance we will have to allocate during shaping
    _glyphs.reserve(_characters.size());
    auto resolvedShapeableSegments = resolveShapeableSegments();
    auto segmentsStartAtParagraph = _segments.size();
    auto segmentsStartAtParagraphSegment = _segments.size();

    VALDI_TRACE("SnapDrawing.shape");
    for (const auto& shapeableSegment : resolvedShapeableSegments.shapeableSegments) {
        if (shapeableSegment.resolvedFont != nullptr) {
            auto specs = shapeableSegment.entry->specs.withFont(shapeableSegment.resolvedFont);
            if (const auto* resolvedBackgroundStyle = getBackgroundStyle(specs.backgroundStyleIndex)) {
                auto backgroundStyle = *resolvedBackgroundStyle;
                if (shapeableSegment.charactersStart != shapeableSegment.entry->charactersStart) {
                    backgroundStyle.padding.left = 0;
                }
                if (shapeableSegment.charactersEnd != shapeableSegment.entry->charactersEnd) {
                    backgroundStyle.padding.right = 0;
                }
                specs.backgroundStyleIndex = appendBackgroundStyle(backgroundStyle);
            }

            const auto* characters = _characters.data() + shapeableSegment.charactersStart;
            processUnidirectionalRun(specs,
                                     shapeableSegment.entry->specs.font,
                                     characters,
                                     shapeableSegment.charactersEnd - shapeableSegment.charactersStart,
                                     shapeableSegment.paragraphSegment->script,
                                     shapeableSegment.paragraphSegment->isRTL);
        }

        if (shapeableSegment.isEndOfParagraphSegment) {
            if (shapeableSegment.paragraphSegment->isRTL != shapeableSegment.paragraph->baseDirectionIsRightToLeft) {
                reverseSegmentsHorizontally(segmentsStartAtParagraphSegment, _segments.size());
            }
            segmentsStartAtParagraphSegment = _segments.size();
        }

        if (shapeableSegment.isEndOfParagraph) {
            if (shapeableSegment.paragraph->baseDirectionIsRightToLeft) {
                reverseSegmentsHorizontally(segmentsStartAtParagraph, _segments.size());
            }

            segmentsStartAtParagraph = _segments.size();
        }
    }
}

/**
 Contains the index for each TextLayoutBuilderSegment that has whitespace
 following it.
 */
using TextLayoutBuilderWhitespaceIndexes = Valdi::SmallVector<size_t, 16>;

static void appendSegmentWord(const TextLayoutBuilderSegment& segment,
                              const ShapedGlyph* glyphs,
                              const ShapedGlyph* glyphsStart,
                              const ShapedGlyph* glyphsEnd,
                              Point* drawPosition,
                              std::vector<TextLayoutBuilderSegment>& out) {
    auto& newSegment = out.emplace_back(segment);
    if (segment.isReplacement) {
        newSegment.drawPosition = *drawPosition;
        drawPosition->x += segment.bounds.width();
        return;
    }
    newSegment.glyphsStart = glyphsStart - glyphs;
    newSegment.glyphsCount = glyphsEnd - glyphsStart;
    newSegment.drawPosition = *drawPosition;

    auto advance = computeAdvance(glyphsStart, newSegment.glyphsCount);
    // Update the bounds so that they only contain the word chunk
    newSegment.bounds =
        Rect::makeXYWH(newSegment.bounds.x(), newSegment.bounds.y(), advance, newSegment.bounds.height());

    drawPosition->x += advance;
}

static void appendWhitespaceIndexIfNeeded(const TextLayoutBuilderSegment& segment,
                                          const std::vector<TextLayoutBuilderSegment>& segments,
                                          TextLayoutBuilderWhitespaceIndexes& whitespaceIndexes) {
    auto segmentsSize = segments.size();
    if (segmentsSize == 0) {
        // Only insert whitespace index once we have at least once segment
        return;
    }

    auto lastSegmentIndex = segmentsSize - 1;

    if (segments[lastSegmentIndex].lineNumber != segment.lineNumber) {
        // Only insert whitespace for a segment on the current line
        return;
    }

    if (whitespaceIndexes.empty() || whitespaceIndexes[whitespaceIndexes.size() - 1] != lastSegmentIndex) {
        // Append the index of the segment that has whitespace following it
        whitespaceIndexes.emplace_back(lastSegmentIndex);
    }
}

static void breakDownSegmentByWords(const TextLayoutBuilderSegment& segment,
                                    const ShapedGlyph* glyphs,
                                    std::vector<TextLayoutBuilderSegment>& out,
                                    Point* drawPosition,
                                    TextLayoutBuilderWhitespaceIndexes& whitespaceIndexes) {
    /**
     This algorithm will break down each TextLayoutBuilderSegment into one segment per word.
     When encountering a whitespace that can be used to justify words, the index of segment
     following the whitespace is added into the WhitespaceIndexes.
     */
    const auto* glyphsIt = glyphs + segment.glyphsStart;
    const auto* glyphsEnd = glyphsIt + segment.glyphsCount;
    const ShapedGlyph* wordStart = nullptr;

    while (glyphsIt != glyphsEnd) {
        auto uni = glyphsIt->character();

        if (Unicode::isBreakingWhitespace(uni)) {
            if (wordStart != nullptr) {
                appendSegmentWord(segment, glyphs, wordStart, glyphsIt, drawPosition, out);
                wordStart = nullptr;
            }

            appendWhitespaceIndexIfNeeded(segment, out, whitespaceIndexes);
        } else {
            if (wordStart == nullptr) {
                wordStart = glyphsIt;
            }
        }

        glyphsIt++;
    }

    if (wordStart != nullptr) {
        appendSegmentWord(segment, glyphs, wordStart, glyphsIt, drawPosition, out);
    }
}

static void justifySegments(TextLayoutBuilderSegment* segments,
                            size_t segmentsStart,
                            size_t segmentsEnd,
                            Scalar lineWidth,
                            Scalar consumedCharactersWidth,
                            const TextLayoutBuilderWhitespaceIndexes& whitespaceIndexes) {
    if (whitespaceIndexes.empty()) {
        return;
    }

    auto resolvedWhitespaceIndexesSize = whitespaceIndexes.size();
    if (whitespaceIndexes[resolvedWhitespaceIndexesSize - 1] == segmentsEnd - 1) {
        // If the last whitespace index matches the last segment, we have trailing
        // spaces and we should ignore it
        resolvedWhitespaceIndexesSize--;
    }

    // Calculate how much space should be inserted between each segment that has whitespace before it
    auto widthPerWhitespaceIndex =
        (lineWidth - consumedCharactersWidth) / static_cast<Scalar>(resolvedWhitespaceIndexesSize);

    Scalar currentOffset = 0;
    size_t currentWhitespaceIndex = 0;
    // Go over all the segments on the line and update their drawX position to account for the whitespaces.
    // Whenever we find a segment that has whitespace after it, we increment the current drawOffset with
    // our calculated whitespace width.

    for (size_t i = segmentsStart; i < segmentsEnd; i++) {
        auto& segment = segments[i];

        segment.drawPosition.x += currentOffset;

        if (i == whitespaceIndexes[currentWhitespaceIndex]) {
            currentOffset += widthPerWhitespaceIndex;
            if (currentWhitespaceIndex + 1 < whitespaceIndexes.size()) {
                currentWhitespaceIndex++;
            }
        }
    }
}

void TextLayoutBuilder::doJustify(Scalar containerWidth) {
    /**
     The algorithm works as follow:
     - For each segments in each line
       - Break down each segment by words, such that there is at least one segment per word
       - Gather an array containing the indexes of each segment that has whitespace before it.
         This is because there could be multiple segments representing one word, which would be
         the case when rendering a word using multiple fonts or colors for instance.
       - From the whitespaces array and the container width, calculate how much space should be
         uniformly given to each whitespace
       - Update drawX of each segment, such that each segment will be offset by the calculated
         whitespace if needed.
     */

    // Stash the segments away, we are going to split them into words
    auto segments = std::move(_segments);
    // Workaround to fix clang tidy issue
    _segments = {};

    TextLayoutBuilderWhitespaceIndexes whitespaceIndexes;

    forEachSegmentsLine(
        segments.data(),
        segments.data() + segments.size(),
        [&](size_t /*lineNumber*/, TextLayoutBuilderSegment* begin, TextLayoutBuilderSegment* end) {
            whitespaceIndexes.clear();

            auto segmentsStart = _segments.size();

            auto drawPosition = begin->drawPosition;

            const auto* it = begin;
            while (it != end) {
                breakDownSegmentByWords(*it, _glyphs.data(), _segments, &drawPosition, whitespaceIndexes);
                it++;
            }

            auto segmentsEnd = _segments.size();

            justifySegments(
                _segments.data(), segmentsStart, segmentsEnd, containerWidth, drawPosition.x, whitespaceIndexes);
        });
}

void TextLayoutBuilder::appendBackgroundIfNeeded(std::vector<TextLayoutVisualEntry>& visualEntries,
                                                 const TextLayoutBuilderSegment& segment,
                                                 Scalar resolvedSegmentX,
                                                 Scalar /* resolvedSegmentY */) const {
    const auto* backgroundStyle = getBackgroundStyle(segment.backgroundStyleIndex);
    if (backgroundStyle == nullptr) {
        return;
    }

    visualEntries.emplace_back(Rect::makeXYWH(resolvedSegmentX,
                                              segment.drawPosition.y,
                                              segment.bounds.width(),
                                              segment.bounds.height()),
                               true,
                               backgroundStyle->color,
                               TextLayoutVisualEntryKindBackground,
                               TextLayoutDecorationStyleSolid,
                               backgroundStyle->borderRadius);
}

void TextLayoutBuilder::appendDecorationIfNeeded(std::vector<TextLayoutVisualEntry>& visualEntries,
                                                 const TextLayoutBuilderSegment& segment,
                                                 const std::optional<Color>& color,
                                                 Scalar resolvedSegmentX,
                                                 Scalar resolvedSegmentY) const {
    constexpr auto kDottedUnderlineStrokeWidth = 2.0f;
    constexpr auto kDottedUnderlineOffset = 0.5f;

    switch (segment.textDecoration) {
        case TextDecorationNone:
            break;
        case TextDecorationStrikethrough: {
            const auto& metrics = segment.font->metrics();

            auto& visualEntry = visualEntries.emplace_back();
            visualEntry.bounds = Rect::makeXYWH(resolvedSegmentX,
                                                resolvedSegmentY + metrics.strikeoutPosition,
                                                segment.bounds.width(),
                                                metrics.strikeoutThickness);
            visualEntry.predraw = false;
            visualEntry.color = color;
        } break;
        case TextDecorationUnderline:
        case TextDecorationDashedUnderline:
        case TextDecorationDottedUnderline: {
            const auto& metrics = segment.font->metrics();
            auto underlineThickness = segment.textDecoration == TextDecorationDottedUnderline
                ? kDottedUnderlineStrokeWidth * _displayScale
                : metrics.underlineThickness;
            auto underlinePosition = segment.textDecoration == TextDecorationDottedUnderline
                ? underlineThickness + (kDottedUnderlineOffset * _displayScale) - (underlineThickness / 2.0f)
                : metrics.underlinePosition;
            auto customUnderlineStyle = segment.customUnderlineStyle;
            if (customUnderlineStyle) {
                customUnderlineStyle->height *= _displayScale;
                customUnderlineStyle->onWidth *= _displayScale;
                customUnderlineStyle->offWidth *= _displayScale;
                customUnderlineStyle->offset *= _displayScale;
                underlineThickness = customUnderlineStyle->height;
                underlinePosition = (metrics.descent / 2.0f) + customUnderlineStyle->offset - (underlineThickness / 2.0f);
            }

            auto& visualEntry = visualEntries.emplace_back();
            visualEntry.bounds =
                Rect::makeXYWH(resolvedSegmentX, resolvedSegmentY + underlinePosition, segment.bounds.width(), underlineThickness);
            visualEntry.predraw = true;
            visualEntry.color = color;
            visualEntry.customUnderlineStyle = customUnderlineStyle;
            if (segment.textDecoration == TextDecorationDashedUnderline) {
                visualEntry.style = TextLayoutDecorationStyleDashed;
            } else if (segment.textDecoration == TextDecorationDottedUnderline) {
                visualEntry.style = TextLayoutDecorationStyleDotted;
            }
        } break;
    }
}

Ref<TextLayout> TextLayoutBuilder::build() {
    buildSegments();

    auto containerWidth = resolveContainerWidth(_segments.data(), _segments.data() + _segments.size(), _maxSize.width);

    if (_textAlign == TextAlignJustify) {
        doJustify(containerWidth);
    }

    std::vector<TextLayoutEntry> entries;
    entries.reserve(_colors.size());

    std::vector<TextLayoutVisualEntry> visualEntries;
    std::vector<TextLayoutAttachment> attachments;

    /**
     Step 1: Calculate the offsets to apply on every  line which will respect the given TextAlign parameter
     and the baseline.
     */
    TextLineOffsetVector lineOffsets;
    lineOffsets.reserve(static_cast<size_t>(_currentNumberOfLines));
    calculateLineOffsets(
        lineOffsets, _segments.data(), _segments.data() + _segments.size(), _textAlign, containerWidth);

    /**
     Step 2: Generate the SkTextBlob from the pending blobs which will be drawn at the correct location.
     */

    for (size_t colorIndex = 0; colorIndex < _colors.size(); colorIndex++) {
        auto& layoutEntry = entries.emplace_back();
        auto entryBounds = Rect::makeEmpty();
        layoutEntry.bounds = Rect::makeEmpty();
        layoutEntry.color = _colors[colorIndex];

        SkTextBlobBuilder textBlobBuilder;

        for (const auto& segment : _segments) {
            if (segment.colorIndex != colorIndex) {
                continue;
            }

            auto lineOffset = lineOffsets[segment.lineNumber - 1];
            auto baseline = std::max(-lineOffset.lineMetrics.ascent, 0.0f);

            auto resolvedSegmentX = segment.drawPosition.x + lineOffset.horizontalOffset;
            auto resolvedSegmentY = segment.drawPosition.y + baseline;

            appendBackgroundIfNeeded(visualEntries, segment, resolvedSegmentX, resolvedSegmentY);
            appendDecorationIfNeeded(visualEntries, segment, layoutEntry.color, resolvedSegmentX, resolvedSegmentY);

            auto resolvedBounds = Rect::makeXYWH(
                resolvedSegmentX, segment.drawPosition.y, segment.bounds.width(), segment.bounds.height());
            entryBounds.join(resolvedBounds);

            if (segment.attachment != nullptr) {
                auto attachmentBounds = segment.isReplacement
                    ? Rect::makeXYWH(resolvedSegmentX,
                                     resolvedSegmentY + segment.bounds.y(),
                                     segment.bounds.width(),
                                     segment.bounds.height())
                    : resolvedBounds;
                attachments.emplace_back(attachmentBounds, segment.attachment, segment.replacementSize);
            }

            const auto* glyphsIt = _glyphs.data() + segment.glyphsStart;
            const auto* glyphsEnd = glyphsIt + segment.glyphsCount;

            if (_includeSegments) {
                outputSegmentToEntry(layoutEntry, glyphsIt, glyphsEnd, resolvedBounds, segment.font);
            }

            if (_includeTextBlob && !segment.isReplacement) {
                ptrdiff_t itIncrement;

                if (segment.isRightToLeft) {
                    const auto* tmp = glyphsIt;
                    glyphsIt = glyphsEnd - 1;
                    glyphsEnd = tmp - 1;
                    itIncrement = -1;
                } else {
                    itIncrement = 1;
                }

                const auto& run = textBlobBuilder.allocRunPos(
                    segment.font->getSkValue(), static_cast<int>(segment.glyphsCount), &resolvedBounds.getSkValue());

                auto* outPoints = run.points();
                auto* outGlyphs = run.glyphs;

                Scalar drawX = resolvedSegmentX;
                if (const auto* backgroundStyle = getBackgroundStyle(segment.backgroundStyleIndex)) {
                    drawX += backgroundStyle->padding.left;
                }
                Scalar drawY = resolvedSegmentY;

                while (glyphsIt != glyphsEnd) {
                    *outGlyphs = glyphsIt->glyphID;

                    outPoints->fX = drawX + glyphsIt->offsetX;
                    outPoints->fY = drawY + glyphsIt->offsetY;

                    drawX += glyphsIt->advanceX;

                    glyphsIt += itIncrement;
                    outPoints++;
                    outGlyphs++;
                }
            }
        }
        // Take in account trailing whitespace at the end, in case if the string ended with a newline character
        entryBounds.bottom = std::max(entryBounds.bottom, _position.y + _currentLineMetrics.height());

        if (_includeTextBlob) {
            layoutEntry.textBlob = textBlobBuilder.make();
        }
        layoutEntry.bounds = entryBounds;
    }

    return Valdi::makeShared<TextLayout>(
        _maxSize, std::move(entries), std::move(visualEntries), std::move(attachments), !_reachedMaxLines);
}

} // namespace snap::drawing
