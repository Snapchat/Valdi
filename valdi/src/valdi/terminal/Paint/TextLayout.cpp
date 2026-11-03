#include "valdi/terminal/Paint/TextLayout.hpp"
#include "valdi/terminal/Paint/Ansi.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace Valdi::Terminal {

using CodePointRange = std::pair<char32_t, char32_t>;

static constexpr std::array kZeroWidthRanges = {
    CodePointRange{0x0300, 0x036f},   CodePointRange{0x0483, 0x0489}, CodePointRange{0x0591, 0x05bd},
    CodePointRange{0x0610, 0x061a},   CodePointRange{0x064b, 0x065f}, CodePointRange{0x0e31, 0x0e31},
    CodePointRange{0x0e34, 0x0e3a},   CodePointRange{0x0e47, 0x0e4e}, CodePointRange{0x1ab0, 0x1aff},
    CodePointRange{0x1dc0, 0x1dff},   CodePointRange{0x200b, 0x200f}, CodePointRange{0x2028, 0x202e},
    CodePointRange{0x2060, 0x2064},   CodePointRange{0x20d0, 0x20ff}, CodePointRange{0xfe00, 0xfe0f},
    CodePointRange{0xfe20, 0xfe2f},   CodePointRange{0xfeff, 0xfeff}, CodePointRange{0x1f3fb, 0x1f3ff},
    CodePointRange{0xe0100, 0xe01ef},
};

static constexpr std::array kWideRanges = {
    CodePointRange{0x1100, 0x115f},   CodePointRange{0x231a, 0x231b},   CodePointRange{0x2329, 0x232a},
    CodePointRange{0x23e9, 0x23ec},   CodePointRange{0x23f0, 0x23f0},   CodePointRange{0x23f3, 0x23f3},
    CodePointRange{0x25fd, 0x25fe},   CodePointRange{0x2614, 0x2615},   CodePointRange{0x2648, 0x2653},
    CodePointRange{0x267f, 0x267f},   CodePointRange{0x2693, 0x2693},   CodePointRange{0x26a1, 0x26a1},
    CodePointRange{0x26aa, 0x26ab},   CodePointRange{0x26bd, 0x26be},   CodePointRange{0x26c4, 0x26c5},
    CodePointRange{0x26ce, 0x26ce},   CodePointRange{0x26d4, 0x26d4},   CodePointRange{0x26ea, 0x26ea},
    CodePointRange{0x26f2, 0x26f3},   CodePointRange{0x26f5, 0x26f5},   CodePointRange{0x26fa, 0x26fa},
    CodePointRange{0x26fd, 0x26fd},   CodePointRange{0x2705, 0x2705},   CodePointRange{0x270a, 0x270b},
    CodePointRange{0x2728, 0x2728},   CodePointRange{0x274c, 0x274c},   CodePointRange{0x274e, 0x274e},
    CodePointRange{0x2753, 0x2755},   CodePointRange{0x2757, 0x2757},   CodePointRange{0x2795, 0x2797},
    CodePointRange{0x27b0, 0x27b0},   CodePointRange{0x27bf, 0x27bf},   CodePointRange{0x2b1b, 0x2b1c},
    CodePointRange{0x2b50, 0x2b50},   CodePointRange{0x2b55, 0x2b55},   CodePointRange{0x2e80, 0x303e},
    CodePointRange{0x3041, 0x33ff},   CodePointRange{0x3400, 0x4dbf},   CodePointRange{0x4e00, 0x9fff},
    CodePointRange{0xa000, 0xa4cf},   CodePointRange{0xa960, 0xa97f},   CodePointRange{0xac00, 0xd7a3},
    CodePointRange{0xf900, 0xfaff},   CodePointRange{0xfe10, 0xfe19},   CodePointRange{0xfe30, 0xfe6f},
    CodePointRange{0xff00, 0xff60},   CodePointRange{0xffe0, 0xffe6},   CodePointRange{0x1f004, 0x1f004},
    CodePointRange{0x1f0cf, 0x1f0cf}, CodePointRange{0x1f18e, 0x1f18e}, CodePointRange{0x1f191, 0x1f19a},
    CodePointRange{0x1f200, 0x1f251}, CodePointRange{0x1f300, 0x1f64f}, CodePointRange{0x1f680, 0x1f6ff},
    CodePointRange{0x1f7e0, 0x1f7eb}, CodePointRange{0x1f90c, 0x1f9ff}, CodePointRange{0x1fa70, 0x1faff},
    CodePointRange{0x20000, 0x3fffd},
};

template<size_t N>
static bool inRanges(char32_t codePoint, const std::array<CodePointRange, N>& ranges) {
    size_t low = 0;
    size_t high = N;
    while (low < high) {
        const size_t mid = (low + high) / 2;
        if (codePoint < ranges[mid].first) {
            high = mid;
        } else if (codePoint > ranges[mid].second) {
            low = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

static constexpr char32_t kZeroWidthJoiner = 0x200d;
static constexpr char32_t kVariationSelector16 = 0xfe0f;

bool isZeroWidth(char32_t codePoint) {
    return codePoint < 0x20 || (codePoint >= 0x7f && codePoint < 0xa0) || codePoint == kZeroWidthJoiner ||
           inRanges(codePoint, kZeroWidthRanges);
}

int codePointWidth(char32_t codePoint) {
    if (isZeroWidth(codePoint)) {
        return 0;
    }
    return inRanges(codePoint, kWideRanges) ? 2 : 1;
}

static bool isRegionalIndicator(char32_t codePoint) {
    return codePoint >= 0x1f1e6 && codePoint <= 0x1f1ff;
}

static void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Invalid sequences decode to U+FFFD, one per offending byte.
static std::vector<char32_t> decodeUtf8(std::string_view text) {
    std::vector<char32_t> out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        size_t length = 1;
        char32_t cp = lead;
        if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            cp = lead & 0x07;
        } else if (lead >= 0xE0) {
            length = 3;
            cp = lead & 0x0F;
        } else if (lead >= 0xC2) {
            length = 2;
            cp = lead & 0x1F;
        } else if (lead >= 0x80) {
            out.push_back(0xFFFD);
            i++;
            continue;
        }
        if (length > 1) {
            bool valid = i + length <= text.size();
            for (size_t k = 1; valid && k < length; k++) {
                const auto next = static_cast<unsigned char>(text[i + k]);
                if ((next & 0xC0) != 0x80) {
                    valid = false;
                } else {
                    cp = (cp << 6) | (next & 0x3F);
                }
            }
            if (!valid || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
                out.push_back(0xFFFD);
                i++;
                continue;
            }
        }
        out.push_back(cp);
        i += length;
    }
    return out;
}

std::vector<Cluster> clusterize(std::string_view text) {
    std::vector<Cluster> clusters;
    std::string current;
    int currentWidth = 0;
    bool joinNext = false;
    bool pendingRegional = false;

    const auto flush = [&]() {
        if (!current.empty()) {
            clusters.push_back(Cluster{std::move(current), currentWidth});
        }
        current.clear();
        currentWidth = 0;
        pendingRegional = false;
    };

    for (const char32_t cp : decodeUtf8(text)) {
        if (cp == kZeroWidthJoiner) {
            if (current.empty()) {
                continue;
            }
            appendUtf8(current, cp);
            joinNext = true;
            continue;
        }
        if (joinNext) {
            appendUtf8(current, cp);
            joinNext = false;
            continue;
        }
        if (isRegionalIndicator(cp) && pendingRegional) {
            appendUtf8(current, cp);
            currentWidth = 2;
            pendingRegional = false;
            continue;
        }
        if (isZeroWidth(cp) && !current.empty()) {
            appendUtf8(current, cp);
            if (cp == kVariationSelector16 && currentWidth == 1) {
                // Emoji presentation requested for a text-default symbol; terminals render it in two cells.
                currentWidth = 2;
            }
            continue;
        }
        flush();
        appendUtf8(current, cp);
        currentWidth = codePointWidth(cp);
        pendingRegional = isRegionalIndicator(cp);
    }
    flush();
    return clusters;
}

int stringWidth(std::string_view text) {
    int width = 0;
    for (const auto& cluster : clusterize(stripAnsi(text))) {
        width += cluster.width;
    }
    return width;
}

std::optional<WrapMode> parseWrapMode(std::string_view name) {
    if (name == "wrap") {
        return WrapMode::Wrap;
    }
    if (name == "end" || name == "truncate" || name == "truncate-end") {
        return WrapMode::TruncateEnd;
    }
    if (name == "middle" || name == "truncate-middle") {
        return WrapMode::TruncateMiddle;
    }
    if (name == "start" || name == "truncate-start") {
        return WrapMode::TruncateStart;
    }
    return std::nullopt;
}

namespace {

struct StyledCluster {
    std::string text;
    int width = 0;
    Style style;
};

using ClusterLine = std::vector<StyledCluster>;

struct LineInfo {
    ClusterLine clusters;
    size_t paragraph = 0;
    // Index of the line's first cluster within its paragraph.
    size_t start = 0;
};

StyledCluster makeEllipsis(const Style& style) {
    return StyledCluster{"…", 1, style};
}

bool isWhitespace(const StyledCluster& cluster) {
    return cluster.text == " " || cluster.text == "\t";
}

StyledLine toStyledLine(const ClusterLine& clusters) {
    StyledLine line;
    StyledSegment current;
    bool hasSegment = false;
    for (const auto& cluster : clusters) {
        if (hasSegment && current.style != cluster.style) {
            line.segments.push_back(std::move(current));
            current = StyledSegment();
        }
        hasSegment = true;
        current.style = cluster.style;
        current.text += cluster.text;
        current.width += cluster.width;
        line.width += cluster.width;
    }
    if (hasSegment) {
        line.segments.push_back(std::move(current));
    }
    return line;
}

std::vector<ClusterLine> splitParagraphs(const std::vector<TextRun>& runs) {
    std::vector<ClusterLine> paragraphs(1);
    for (const auto& run : runs) {
        std::string normalized;
        normalized.reserve(run.text.size());
        for (size_t i = 0; i < run.text.size(); i++) {
            const char c = run.text[i];
            if (c == '\r') {
                if (i + 1 < run.text.size() && run.text[i + 1] == '\n') {
                    i++;
                }
                normalized.push_back('\n');
            } else if (c == '\t') {
                normalized.push_back(' ');
            } else {
                normalized.push_back(c);
            }
        }

        size_t partStart = 0;
        while (true) {
            const size_t newline = normalized.find('\n', partStart);
            const auto part =
                std::string_view(normalized)
                    .substr(partStart, newline == std::string::npos ? std::string::npos : newline - partStart);
            for (auto& cluster : clusterize(part)) {
                paragraphs.back().push_back(StyledCluster{std::move(cluster.text), cluster.width, run.style});
            }
            if (newline == std::string::npos) {
                break;
            }
            paragraphs.emplace_back();
            partStart = newline + 1;
        }
    }
    return paragraphs;
}

struct WrappedLine {
    ClusterLine clusters;
    size_t start = 0;
};

std::vector<WrappedLine> wrapParagraph(const ClusterLine& clusters, int maxWidth) {
    std::vector<WrappedLine> lines;
    ClusterLine line;
    size_t lineStart = 0;
    int lineWidth = 0;

    const auto pushCluster = [&](size_t index) {
        if (line.empty()) {
            lineStart = index;
        }
        line.push_back(clusters[index]);
        lineWidth += clusters[index].width;
    };

    const auto pushLine = [&]() {
        // Whitespace before a wrap point is invisible; dropping it keeps measured widths tight.
        while (!line.empty() && isWhitespace(line.back())) {
            line.pop_back();
        }
        lines.push_back(WrappedLine{std::move(line), lineStart});
        line.clear();
        lineWidth = 0;
    };

    size_t index = 0;
    while (index < clusters.size()) {
        const bool whitespace = isWhitespace(clusters[index]);
        size_t end = index;
        int tokenWidth = 0;
        while (end < clusters.size() && isWhitespace(clusters[end]) == whitespace) {
            tokenWidth += clusters[end].width;
            end++;
        }
        const size_t tokenStart = index;
        index = end;

        if (whitespace) {
            // Trailing whitespace that does not fit is dropped rather than wrapped; it would be invisible.
            for (size_t i = tokenStart; i < end; i++) {
                if (lineWidth + clusters[i].width > maxWidth) {
                    break;
                }
                pushCluster(i);
            }
            continue;
        }

        if (lineWidth + tokenWidth <= maxWidth) {
            for (size_t i = tokenStart; i < end; i++) {
                pushCluster(i);
            }
            continue;
        }

        if (!line.empty()) {
            pushLine();
        }
        if (tokenWidth <= maxWidth) {
            for (size_t i = tokenStart; i < end; i++) {
                pushCluster(i);
            }
            continue;
        }
        for (size_t i = tokenStart; i < end; i++) {
            if (lineWidth + clusters[i].width > maxWidth && !line.empty()) {
                pushLine();
            }
            pushCluster(i);
        }
    }
    lines.push_back(WrappedLine{std::move(line), lineStart});
    return lines;
}

ClusterLine takeFromStart(const ClusterLine& clusters, size_t from, int maxWidth) {
    ClusterLine out;
    int width = 0;
    for (size_t i = from; i < clusters.size(); i++) {
        if (width + clusters[i].width > maxWidth) {
            break;
        }
        out.push_back(clusters[i]);
        width += clusters[i].width;
    }
    return out;
}

ClusterLine takeFromEnd(const ClusterLine& clusters, int maxWidth) {
    ClusterLine out;
    int width = 0;
    for (size_t i = clusters.size(); i > 0; i--) {
        const auto& cluster = clusters[i - 1];
        if (width + cluster.width > maxWidth) {
            break;
        }
        out.push_back(cluster);
        width += cluster.width;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

int totalWidth(const ClusterLine& clusters) {
    int width = 0;
    for (const auto& cluster : clusters) {
        width += cluster.width;
    }
    return width;
}

ClusterLine truncateParagraph(const ClusterLine& clusters, int maxWidth, WrapMode mode) {
    if (totalWidth(clusters) <= maxWidth) {
        return clusters;
    }
    if (maxWidth <= 0) {
        return {};
    }
    const Style anchor =
        clusters.empty() ? Style() : (mode == WrapMode::TruncateStart ? clusters.back().style : clusters.front().style);
    if (maxWidth == 1) {
        return {makeEllipsis(anchor)};
    }
    const int available = maxWidth - 1;
    ClusterLine out;
    switch (mode) {
        case WrapMode::TruncateStart: {
            out.push_back(makeEllipsis(anchor));
            for (auto& cluster : takeFromEnd(clusters, available)) {
                out.push_back(std::move(cluster));
            }
            return out;
        }
        case WrapMode::TruncateMiddle: {
            const int headWidth = (available + 1) / 2;
            out = takeFromStart(clusters, 0, headWidth);
            const int used = totalWidth(out);
            out.push_back(makeEllipsis(anchor));
            for (auto& cluster : takeFromEnd(clusters, available - used)) {
                out.push_back(std::move(cluster));
            }
            return out;
        }
        default: {
            out = takeFromStart(clusters, 0, available);
            out.push_back(makeEllipsis(out.empty() ? anchor : out.back().style));
            return out;
        }
    }
}

// Ends a line that was cut at a paragraph boundary with an ellipsis, trimming to make room for it.
ClusterLine appendEllipsis(ClusterLine clusters, std::optional<int> maxWidth) {
    const Style style = clusters.empty() ? Style() : clusters.back().style;
    if (maxWidth) {
        if (*maxWidth <= 0) {
            return {};
        }
        int width = totalWidth(clusters);
        while (!clusters.empty() && width + 1 > *maxWidth) {
            width -= clusters.back().width;
            clusters.pop_back();
        }
    }
    clusters.push_back(makeEllipsis(style));
    return clusters;
}

} // namespace

std::vector<StyledLine> layoutText(const std::vector<TextRun>& runs, const LayoutOptions& options) {
    std::optional<int> width;
    if (options.maxWidth) {
        width = std::max(0, *options.maxWidth);
    }

    const auto paragraphs = splitParagraphs(runs);
    std::vector<LineInfo> lines;
    for (size_t p = 0; p < paragraphs.size(); p++) {
        const auto& paragraph = paragraphs[p];
        if (!width) {
            lines.push_back(LineInfo{paragraph, p, 0});
        } else if (options.mode == WrapMode::Wrap) {
            if (*width == 0) {
                lines.push_back(LineInfo{{}, p, 0});
                continue;
            }
            for (auto& wrapped : wrapParagraph(paragraph, *width)) {
                lines.push_back(LineInfo{std::move(wrapped.clusters), p, wrapped.start});
            }
        } else {
            lines.push_back(LineInfo{truncateParagraph(paragraph, *width, options.mode), p, 0});
        }
    }

    const auto maxLines = static_cast<size_t>(std::max(0, options.maxLines));
    if (maxLines > 0 && lines.size() > maxLines) {
        const bool continuesParagraph = lines[maxLines].paragraph == lines[maxLines - 1].paragraph;
        lines.resize(maxLines);
        if (options.ellipsize) {
            auto& last = lines.back();
            if (continuesParagraph && width) {
                const auto& paragraph = paragraphs[last.paragraph];
                const ClusterLine tail(paragraph.begin() + static_cast<std::ptrdiff_t>(last.start), paragraph.end());
                last.clusters = truncateParagraph(tail, *width, WrapMode::TruncateEnd);
            } else {
                last.clusters = appendEllipsis(std::move(last.clusters), width);
            }
        }
    }

    std::vector<StyledLine> out;
    out.reserve(lines.size());
    for (const auto& line : lines) {
        out.push_back(toStyledLine(line.clusters));
    }
    return out;
}

std::vector<StyledLine> layoutText(const std::vector<TextRun>& runs, std::optional<int> maxWidth, WrapMode mode) {
    LayoutOptions options;
    options.maxWidth = maxWidth;
    options.mode = mode;
    return layoutText(runs, options);
}

LineMetrics measureLines(const std::vector<StyledLine>& lines) {
    LineMetrics metrics;
    for (const auto& line : lines) {
        metrics.width = std::max(metrics.width, line.width);
    }
    metrics.height = static_cast<int>(lines.size());
    return metrics;
}

std::vector<std::string> linesToPlainText(const std::vector<StyledLine>& lines) {
    std::vector<std::string> out;
    out.reserve(lines.size());
    for (const auto& line : lines) {
        std::string text;
        for (const auto& segment : line.segments) {
            text += segment.text;
        }
        out.push_back(std::move(text));
    }
    return out;
}

} // namespace Valdi::Terminal
