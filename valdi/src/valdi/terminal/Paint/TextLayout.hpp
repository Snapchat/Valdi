#pragma once

#include "valdi/terminal/Paint/Style.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Valdi::Terminal {

/** A user-perceived character (approximate grapheme cluster) and the number of terminal columns it occupies. */
struct Cluster {
    std::string text;
    int width = 0;
};

/** Whether a code point occupies no terminal column of its own (controls, combining marks, joiners). */
bool isZeroWidth(char32_t codePoint);

/** Columns occupied by a single code point: 0, 1 or 2. */
int codePointWidth(char32_t codePoint);

/**
 * Splits UTF-8 text into clusters: a base code point followed by its combining marks, variation
 * selectors, skin-tone modifiers and ZWJ sequences. Regional indicator pairs form one flag cluster.
 */
std::vector<Cluster> clusterize(std::string_view text);

/** Number of terminal columns `text` occupies. ANSI escape sequences are ignored. */
int stringWidth(std::string_view text);

/**
 * Text overflow behavior. `Wrap` breaks between words (and inside words wider than the available
 * width); the truncate modes keep one line per paragraph and replace the overflow with an ellipsis.
 */
enum class WrapMode { Wrap, TruncateEnd, TruncateMiddle, TruncateStart };

/** Parses wrap, truncate, truncate-end, end, truncate-middle, middle, truncate-start or start. */
std::optional<WrapMode> parseWrapMode(std::string_view name);

/** A piece of text sharing one style. */
struct TextRun {
    std::string text;
    Style style;
};

struct StyledSegment {
    std::string text;
    int width = 0;
    Style style;
};

struct StyledLine {
    std::vector<StyledSegment> segments;
    int width = 0;
};

struct LayoutOptions {
    /** Unset means unconstrained: no wrapping and no truncation. */
    std::optional<int> maxWidth;
    WrapMode mode = WrapMode::Wrap;
    /** Zero means unlimited. */
    int maxLines = 0;
    /** Whether text cut by `maxLines` ends with an ellipsis. */
    bool ellipsize = true;
};

/** Lays out styled runs into lines. Hard line breaks in the text are always honored. */
std::vector<StyledLine> layoutText(const std::vector<TextRun>& runs, const LayoutOptions& options);

std::vector<StyledLine> layoutText(const std::vector<TextRun>& runs,
                                   std::optional<int> maxWidth,
                                   WrapMode mode = WrapMode::Wrap);

/** The widest line and the number of lines. */
struct LineMetrics {
    int width = 0;
    int height = 0;
};

LineMetrics measureLines(const std::vector<StyledLine>& lines);

std::vector<std::string> linesToPlainText(const std::vector<StyledLine>& lines);

} // namespace Valdi::Terminal
