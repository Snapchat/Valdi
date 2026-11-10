//
//  TextShaperHarfbuzz.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 3/9/22.
//

#include "snap_drawing/cpp/Text/TextShaperHarfbuzz.hpp"
#include "snap_drawing/cpp/Text/CharactersIterator.hpp"
#include "snap_drawing/cpp/Text/Typeface.hpp"
#include "snap_drawing/cpp/Text/Unicode.hpp"

#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/AutoMalloc.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include "hb-icu.h"
#include "hb-ot.h"
#include "hb.h"

#include "include/core/SkStream.h"
#include "unicode/ubidi.h"
#include "unicode/uchar.h"
#include <algorithm>

namespace snap::drawing {

enum class TextDirectionResolutionResult {
    Complex,
    LeftToRight,
    RightToLeft,
};

TextShaperHarfbuzz::TextShaperHarfbuzz() : _buffer(hb_buffer_create()) {}
TextShaperHarfbuzz::~TextShaperHarfbuzz() = default;

/**
 * Disable certain scripts (mostly those with cursive connection) from having letterspacing
 * applied. See https://github.com/behdad/harfbuzz/issues/64 for more details.
 */
static bool isScriptOkForLetterspacing(hb_script_t script) {
    return !(script == HB_SCRIPT_ARABIC || script == HB_SCRIPT_NKO || script == HB_SCRIPT_PSALTER_PAHLAVI ||
             script == HB_SCRIPT_MANDAIC || script == HB_SCRIPT_MONGOLIAN || script == HB_SCRIPT_PHAGS_PA ||
             script == HB_SCRIPT_DEVANAGARI || script == HB_SCRIPT_BENGALI || script == HB_SCRIPT_GURMUKHI ||
             script == HB_SCRIPT_MODI || script == HB_SCRIPT_SHARADA || script == HB_SCRIPT_SYLOTI_NAGRI ||
             script == HB_SCRIPT_TIRHUTA || script == HB_SCRIPT_OGHAM);
}

static bool isBiDiLevelRightToLeft(UBiDiLevel level) {
    return (level & 0x1) != 0;
}

struct UbiDiWrapper {
    UBiDi* ptr;

    UbiDiWrapper() : ptr(ubidi_open()) {}

    ~UbiDiWrapper() {
        ubidi_close(ptr);
    }
};

static void appendSingleSegment(TextParagraph& paragraph,
                                const CharactersIterator& iterator,
                                bool isRightToLeft,
                                TextScript script) {
    if (!paragraph.segments.empty()) {
        // Join with the previous segment when it continues it, so text is not split at paragraph boundaries.
        auto& previous = paragraph.segments.back();
        if (previous.end == iterator.rangeStart() && previous.isRTL == isRightToLeft &&
            (previous.script == script || !previous.script.isStrong() || !script.isStrong())) {
            previous.end = iterator.rangeEnd();
            if (!previous.script.isStrong()) {
                previous.script = script;
            }
            return;
        }
    }

    paragraph.segments.emplace_back(isRightToLeft, iterator.rangeStart(), iterator.rangeEnd(), script);
}

static void doAppendSegment(
    TextParagraph& paragraph, const Character* unicodeText, size_t start, size_t end, bool isRightToLeft) {
    /**
     Remove control chars from the string. We append one separate segment
     for each uninterrupted sequence of non control characters.
     */

    CharactersIterator iterator(unicodeText, start, end);
    auto currentScript = TextScript::common();

    while (!iterator.isAtEnd()) {
        auto c = iterator.current();

        if (Unicode::isBidiControl(c)) {
            if (iterator.hasRange()) {
                appendSingleSegment(paragraph, iterator, isRightToLeft, currentScript);
            }
            iterator.advance();
            iterator.resetRange();
        } else {
            auto newScript = TextScript::fromCharacter(c);

            if (newScript.isStrong()) {
                if (currentScript != newScript && currentScript.isStrong() && iterator.hasRange()) {
                    // Changing from a strong script to another one, we append a dedicated paragraph segment for it
                    appendSingleSegment(paragraph, iterator, isRightToLeft, currentScript);
                    iterator.resetRange();
                }

                currentScript = newScript;
            }
            iterator.advance();
        }
    }

    if (iterator.hasRange()) {
        appendSingleSegment(paragraph, iterator, isRightToLeft, currentScript);
    }
}

static TextDirectionResolutionResult resolveTextDirection(const Character* unicodeText,
                                                          size_t length,
                                                          bool isRightToLeft) {
    // Use "complex" as initial state, when actually detecting that the text has complex or mixed
    // direction, we will bail out of the function.
    TextDirectionResolutionResult currentDirection = TextDirectionResolutionResult::Complex; // Use mixed to define

    for (size_t i = 0; i < length; i++) {
        auto c = unicodeText[i];

        auto charDirection = u_charDirection(c);

        switch (charDirection) {
            case U_LEFT_TO_RIGHT:
            case U_EUROPEAN_NUMBER:
                if (currentDirection == TextDirectionResolutionResult::RightToLeft) {
                    // mixed LTR/RTL, need full ICU
                    return TextDirectionResolutionResult::Complex;
                }
                currentDirection = TextDirectionResolutionResult::LeftToRight;
                break;
            case U_RIGHT_TO_LEFT:
            case U_RIGHT_TO_LEFT_ARABIC:
            case U_ARABIC_NUMBER:
                if (currentDirection == TextDirectionResolutionResult::LeftToRight) {
                    // mixed LTR/RTL, need full ICU
                    return TextDirectionResolutionResult::Complex;
                }
                currentDirection = TextDirectionResolutionResult::RightToLeft;
                break;
            case U_LEFT_TO_RIGHT_EMBEDDING:
            case U_LEFT_TO_RIGHT_OVERRIDE:
            case U_RIGHT_TO_LEFT_EMBEDDING:
            case U_RIGHT_TO_LEFT_OVERRIDE:
            case U_POP_DIRECTIONAL_FORMAT:
            case U_FIRST_STRONG_ISOLATE:
            case U_LEFT_TO_RIGHT_ISOLATE:
            case U_RIGHT_TO_LEFT_ISOLATE:
            case U_POP_DIRECTIONAL_ISOLATE:
                // bidi markers, need full ICU
                return TextDirectionResolutionResult::Complex;
            case U_EUROPEAN_NUMBER_SEPARATOR:
            case U_EUROPEAN_NUMBER_TERMINATOR:
            case U_COMMON_NUMBER_SEPARATOR:
            case U_BLOCK_SEPARATOR:
            case U_SEGMENT_SEPARATOR:
            case U_WHITE_SPACE_NEUTRAL:
            case U_OTHER_NEUTRAL:
            case U_DIR_NON_SPACING_MARK:
            case U_BOUNDARY_NEUTRAL:
            case U_CHAR_DIRECTION_COUNT:
                // neutral characters
                break;
        }
    }

    if (currentDirection == TextDirectionResolutionResult::Complex) {
        return isRightToLeft ? TextDirectionResolutionResult::RightToLeft : TextDirectionResolutionResult::LeftToRight;
    } else {
        return currentDirection;
    }
}

TextParagraphList TextShaperHarfbuzz::resolveParagraphs(const Character* unicodeText,
                                                        size_t length,
                                                        bool isRightToLeft) {
    auto textDirection = resolveTextDirection(unicodeText, length, isRightToLeft);
    switch (textDirection) {
        case TextDirectionResolutionResult::Complex:
            return resolveParagraphsICU(unicodeText, length, isRightToLeft);
        case TextDirectionResolutionResult::LeftToRight:
            return resolveParagraphsPrimitive(unicodeText, length, false);
        case TextDirectionResolutionResult::RightToLeft:
            return resolveParagraphsPrimitive(unicodeText, length, true);
    }
}

TextParagraphList TextShaperHarfbuzz::resolveParagraphsPrimitive(const Character* unicodeText,
                                                                 size_t length,
                                                                 bool isRightToLeft) {
    TextParagraphList out;

    if (length > 0) {
        auto& paragraph = out.emplace_back();
        paragraph.baseDirectionIsRightToLeft = isRightToLeft;
        doAppendSegment(paragraph, unicodeText, 0, length, isRightToLeft);
    }

    return out;
}

TextParagraphList TextShaperHarfbuzz::resolveParagraphsICU(const Character* unicodeText,
                                                           size_t length,
                                                           bool isRightToLeft) {
    TextParagraphList out;

    Valdi::AutoMalloc<char16_t, 512> utf16Storage;

    auto utf16Length = Valdi::countUtf32ToUtf16(unicodeText, length);
    utf16Storage.resize(utf16Length);
    utf16Length = Valdi::utf32ToUtf16(unicodeText, length, utf16Storage.data(), utf16Length);
    Valdi::UTF16ToUTF32Index utf16Index(utf16Storage.data(), utf16Length, unicodeText, length);

    UbiDiWrapper ubidi;

    UErrorCode status = U_ZERO_ERROR;

    ubidi_setPara(ubidi.ptr,
                  utf16Storage.data(),
                  static_cast<int32_t>(utf16Length),
                  isRightToLeft ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR,
                  nullptr,
                  &status);

    if (U_FAILURE(status) != 0) {
        return out;
    }

    auto paragraphsCount = ubidi_countParagraphs(ubidi.ptr);
    UbiDiWrapper line;

    for (int32_t paragraphIndex = 0; paragraphIndex < paragraphsCount; paragraphIndex++) {
        int32_t paragraphStart = 0;
        int32_t paragraphLimit = 0;
        UBiDiLevel paraLevel;

        ubidi_getParagraphByIndex(ubidi.ptr, paragraphIndex, &paragraphStart, &paragraphLimit, &paraLevel, &status);
        // Resolve runs one paragraph at a time so that runs from different paragraphs never get reordered together.
        ubidi_setLine(ubidi.ptr, paragraphStart, paragraphLimit, line.ptr, &status);
        auto runsCount = ubidi_countRuns(line.ptr, &status);

        if (U_FAILURE(status) != 0) {
            status = U_ZERO_ERROR;
            continue;
        }

        auto paragraphIsRightToLeft = isBiDiLevelRightToLeft(paraLevel);

        if (out.empty() || out[out.size() - 1].baseDirectionIsRightToLeft != paragraphIsRightToLeft) {
            // Append a paragraph if we find our run is in a paragraph that has a different base direction.
            auto& paragraph = out.emplace_back();
            paragraph.baseDirectionIsRightToLeft = paragraphIsRightToLeft;
        }

        auto& paragraph = out[out.size() - 1];

        for (int32_t i = 0; i < runsCount; i++) {
            // The layout builder flips right-to-left paragraphs into visual order itself, so their runs are
            // handed over from the rightmost one.
            auto visualIndex = paragraphIsRightToLeft ? runsCount - 1 - i : i;
            int32_t logicalStart = 0;
            int32_t logicalLength = 0;
            auto direction = ubidi_getVisualRun(line.ptr, visualIndex, &logicalStart, &logicalLength);

            if (logicalLength == 0) {
                continue;
            }

            auto isSegmentRTL = isBiDiLevelRightToLeft(direction);

            auto segmentStart = static_cast<size_t>(paragraphStart + logicalStart);
            auto segmentEnd = static_cast<size_t>(paragraphStart + logicalStart + logicalLength);

            doAppendSegment(paragraph,
                            unicodeText,
                            utf16Index.getUTF32Index(segmentStart),
                            utf16Index.getUTF32Index(segmentEnd),
                            isSegmentRTL);
        }
    }

    return out;
}

size_t TextShaperHarfbuzz::shape(const Character* unicodeText,
                                 size_t length,
                                 Font& font,
                                 bool isRightToLeft,
                                 Scalar letterSpacing,
                                 TextScript script,
                                 std::vector<ShapedGlyph>& out) {
    std::lock_guard<Valdi::Mutex> lock(_mutex);

    auto* buffer = _buffer.get();
    if (buffer == nullptr) {
        return 0;
    }

    auto* hbFont = font.getHBFont().get();
    if (hbFont == nullptr) {
        return 0;
    }

    hb_buffer_set_content_type(buffer, HB_BUFFER_CONTENT_TYPE_UNICODE);
    hb_buffer_add_utf32(buffer, reinterpret_cast<const uint32_t*>(unicodeText), static_cast<int>(length), 0, -1);
    hb_buffer_set_script(buffer, static_cast<hb_script_t>(script.code));
    hb_buffer_set_direction(buffer, isRightToLeft ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_guess_segment_properties(buffer);

    const hb_feature_t* features = nullptr;
    unsigned int featuresSize = 0;
    // Disable default-on non-required ligature features if letter-spacing
    // See http://dev.w3.org/csswg/css-text-3/#letter-spacing-property
    // "When the effective spacing between two characters is not zero (due to
    // either justification or a non-zero value of letter-spacing), user agents
    // should not apply optional ligatures."
    if (letterSpacing != 0.0) {
        static hb_feature_t kNoLigaFeatures[] = {{HB_TAG('l', 'i', 'g', 'a'), 0, 0, ~0u},
                                                 {HB_TAG('c', 'l', 'i', 'g'), 0, 0, ~0u}};
        features = kNoLigaFeatures;
        featuresSize = static_cast<unsigned int>(sizeof(kNoLigaFeatures) / sizeof(hb_feature_t));
    }

    hb_shape(hbFont, buffer, features, featuresSize);

    uint32_t glyphCount = 0;
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    int scaleX;
    int scaleY;
    hb_font_get_scale(hbFont, &scaleX, &scaleY);
    auto fontSize = font.getSkValue().getSize();
    double textSizeY = fontSize / scaleY;
    double textSizeX = fontSize / scaleX * font.getSkValue().getScaleX();
    Scalar advanceOffset = isScriptOkForLetterspacing(hb_buffer_get_script(_buffer.get())) ? letterSpacing : 0.0f;

    auto glyphsLength = static_cast<size_t>(glyphCount);

    auto glyphsStart = out.size();
    out.resize(glyphsStart + glyphsLength);
    auto* glyphs = out.data() + glyphsStart;

    for (size_t i = 0; i < glyphsLength; i++) {
        auto& glyph = glyphs[i];
        auto& info = glyphInfos[i];
        auto& pos = glyphPositions[i];

        glyph.setCharacter(unicodeText[info.cluster], (info.mask & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) != 0);
        glyph.glyphID = info.codepoint;
        glyph.offsetX = pos.x_offset * textSizeX;
        glyph.offsetY = -(pos.y_offset * textSizeY); // HarfBuzz y-up, Skia y-down
        glyph.advanceX = (pos.x_advance * textSizeX) + advanceOffset;
    }

    hb_buffer_clear_contents(buffer);

    return glyphsLength;
}

} // namespace snap::drawing
