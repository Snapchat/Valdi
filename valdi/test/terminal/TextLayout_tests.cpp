#include "valdi/terminal/Paint/TextLayout.hpp"

#include <gtest/gtest.h>

using namespace Valdi::Terminal;

namespace ValdiTest {

using Lines = std::vector<std::string>;

static std::vector<TextRun> plain(const std::string& text) {
    return {TextRun{text, Style()}};
}

static Lines layoutPlain(const std::string& text, std::optional<int> width, WrapMode mode = WrapMode::Wrap) {
    return linesToPlainText(layoutText(plain(text), width, mode));
}

TEST(TerminalTextWidth, countsAsciiAsOneColumnEach) {
    ASSERT_EQ(5, stringWidth("hello"));
    ASSERT_EQ(0, stringWidth(""));
}

TEST(TerminalTextWidth, countsCjkAndEmojiAsTwoColumns) {
    ASSERT_EQ(4, stringWidth("日本"));
    ASSERT_EQ(2, stringWidth("😀"));
    ASSERT_EQ(4, stringWidth("a😀b"));
}

TEST(TerminalTextWidth, ignoresCombiningMarksAndEscapeSequences) {
    ASSERT_EQ(1, stringWidth("é"));
    ASSERT_EQ(1, stringWidth("é"));
    ASSERT_EQ(3, stringWidth("\x1b[31mred\x1b[0m"));
}

TEST(TerminalTextWidth, keepsZwjSequencesAndFlagsAsOneCluster) {
    const auto family = clusterize("\U0001F468‍\U0001F469‍\U0001F467");
    ASSERT_EQ(1u, family.size());
    ASSERT_EQ(2, family[0].width);

    const auto flag = clusterize("\U0001F1EB\U0001F1F7x");
    ASSERT_EQ(2u, flag.size());
    ASSERT_EQ(2, flag[0].width);
    ASSERT_EQ("x", flag[1].text);
}

TEST(TerminalTextWidth, widensTextSymbolsWithEmojiPresentation) {
    ASSERT_EQ(1, stringWidth("❤"));
    ASSERT_EQ(2, stringWidth("❤️"));
}

TEST(TerminalTextWidth, treatsControlCharactersAsZeroWidth) {
    ASSERT_EQ(0, stringWidth("\u0007"));
}

TEST(TerminalTextWidth, replacesInvalidUtf8WithASingleColumnCharacter) {
    ASSERT_EQ(3, stringWidth(std::string("a\xff") + "b"));
}

TEST(TerminalTextSanitizing, dropsControlCharactersSoTheyNeverReachTheTerminal) {
    ASSERT_EQ((Lines{"a[31mb"}), layoutPlain("a\x1b[31mb", 20));
    ASSERT_EQ((Lines{"ab"}), layoutPlain(std::string("a\x07") + "b", 20));
    ASSERT_EQ((Lines{"ab"}),
              layoutPlain("a\xc2\x9b"
                          "b",
                          20));
    ASSERT_EQ((Lines{"ab"}), layoutPlain(std::string("a\x7f") + "b", 20));
    ASSERT_EQ((Lines{"a b"}), layoutPlain("a\tb", 20));
    const auto clusters = clusterize("a\x1b");
    ASSERT_EQ(1u, clusters.size());
    ASSERT_EQ("a", clusters[0].text);
}

TEST(TerminalTextSanitizing, rejectsOverlongAndOutOfRangeUtf8) {
    const std::string replacement = "\xef\xbf\xbd";
    // Overlong encoding of ESC: every offending byte becomes U+FFFD.
    ASSERT_EQ((Lines{replacement + replacement + replacement}), layoutPlain("\xe0\x80\x9b", 20));
    ASSERT_EQ((Lines{replacement + replacement + replacement + replacement}), layoutPlain("\xf0\x80\x80\x9b", 20));
    ASSERT_EQ((Lines{replacement + replacement + replacement + replacement}), layoutPlain("\xf8\x80\x80\x80", 20));
    ASSERT_EQ((Lines{replacement + replacement}), layoutPlain("\xc0\x9b", 20));
    ASSERT_EQ((Lines{replacement + replacement + replacement}), layoutPlain("\xed\xa0\x80", 20));
    ASSERT_EQ((Lines{replacement + replacement + replacement + replacement}), layoutPlain("\xf4\x90\x80\x80", 20));
    // Valid encodings next to the boundaries still decode.
    ASSERT_EQ((Lines{"\xe0\xa0\x80"}), layoutPlain("\xe0\xa0\x80", 20));
    ASSERT_EQ((Lines{"\xf0\x90\x80\x80"}), layoutPlain("\xf0\x90\x80\x80", 20));
}

TEST(TerminalWrap, returnsASingleLineWhenUnconstrained) {
    const auto lines = layoutText(plain("hello world"), std::nullopt);
    ASSERT_EQ((Lines{"hello world"}), linesToPlainText(lines));
    const auto metrics = measureLines(lines);
    ASSERT_EQ(11, metrics.width);
    ASSERT_EQ(1, metrics.height);
}

TEST(TerminalWrap, wrapsBetweenWords) {
    const auto lines = layoutText(plain("the quick brown fox"), 10);
    ASSERT_EQ((Lines{"the quick", "brown fox"}), linesToPlainText(lines));
    const auto metrics = measureLines(lines);
    ASSERT_EQ(9, metrics.width);
    ASSERT_EQ(2, metrics.height);
}

TEST(TerminalWrap, hardBreaksWordsWiderThanTheLine) {
    ASSERT_EQ((Lines{"abcd", "efgh", "ij"}), layoutPlain("abcdefghij", 4));
}

TEST(TerminalWrap, honorsHardLineBreaks) {
    ASSERT_EQ((Lines{"a", "b", "c"}), layoutPlain("a\nb\r\nc", 10));
    const auto empty = measureLines(layoutText(plain(""), 10));
    ASSERT_EQ(0, empty.width);
    ASSERT_EQ(1, empty.height);
    const auto newline = measureLines(layoutText(plain("\n"), 10));
    ASSERT_EQ(0, newline.width);
    ASSERT_EQ(2, newline.height);
}

TEST(TerminalWrap, doesNotSplitWideCharacters) {
    ASSERT_EQ((Lines{"日本", "語"}), layoutPlain("日本語", 4));
}

TEST(TerminalWrap, putsAClusterWiderThanTheLineOnItsOwnLineAndMeasuresItsRealWidth) {
    const auto lines = layoutText(plain("日本"), 1);
    ASSERT_EQ((Lines{"日", "本"}), linesToPlainText(lines));
    ASSERT_EQ(2, measureLines(lines).width);
    ASSERT_EQ(2, measureLines(lines).height);
    ASSERT_EQ((Lines{"a", "日"}), layoutPlain("a日", 1));
}

TEST(TerminalWrap, clipsInsteadOfEllipsizingWhenEllipsizeIsOff) {
    LayoutOptions options;
    options.maxWidth = 8;
    options.mode = WrapMode::TruncateEnd;
    options.ellipsize = false;
    ASSERT_EQ((Lines{"hello wo"}), linesToPlainText(layoutText(plain("hello wonderful world"), options)));
    options.mode = WrapMode::TruncateStart;
    ASSERT_EQ((Lines{"ul world"}), linesToPlainText(layoutText(plain("hello wonderful world"), options)));
}

TEST(TerminalWrap, truncatesWithAnEllipsis) {
    const std::string text = "hello wonderful world";
    ASSERT_EQ((Lines{"hello won…"}), layoutPlain(text, 10, WrapMode::TruncateEnd));
    ASSERT_EQ((Lines{"…ful world"}), layoutPlain(text, 10, WrapMode::TruncateStart));
    ASSERT_EQ((Lines{"hello…orld"}), layoutPlain(text, 10, WrapMode::TruncateMiddle));
    ASSERT_EQ((Lines{"…"}), layoutPlain(text, 1, WrapMode::TruncateEnd));
    ASSERT_EQ((Lines{"short"}), layoutPlain("short", 10, WrapMode::TruncateEnd));
}

TEST(TerminalWrap, keepsStylesPerSegmentAndMergesEqualNeighbors) {
    Style red;
    red.fg = TermColor::ansi16(1);
    const auto lines = layoutText({TextRun{"a", red}, TextRun{"b", red}, TextRun{"c", Style()}}, 10);
    ASSERT_EQ(1u, lines.size());
    ASSERT_EQ(2u, lines[0].segments.size());
    ASSERT_EQ("ab", lines[0].segments[0].text);
    ASSERT_EQ("c", lines[0].segments[1].text);
    ASSERT_EQ(red, lines[0].segments[0].style);
    ASSERT_TRUE(lines[0].segments[1].style.isEmpty());
}

TEST(TerminalWrap, wrapsAcrossRunsWithDifferentStyles) {
    Style green;
    green.fg = TermColor::ansi16(2);
    const auto lines = layoutText({TextRun{"Status: ", Style()}, TextRun{"all good", green}}, 10);
    ASSERT_EQ((Lines{"Status:", "all good"}), linesToPlainText(lines));
}

TEST(TerminalWrap, producesEmptyLinesForZeroWidth) {
    const auto metrics = measureLines(layoutText(plain("abc"), 0));
    ASSERT_EQ(0, metrics.width);
    ASSERT_EQ(1, metrics.height);
}

TEST(TerminalWrap, capsWrappedTextAtMaxLinesWithAnEllipsis) {
    LayoutOptions options;
    options.maxWidth = 8;
    options.maxLines = 2;
    ASSERT_EQ((Lines{"the", "quick b…"}), linesToPlainText(layoutText(plain("the quick brown fox"), options)));

    options.ellipsize = false;
    ASSERT_EQ((Lines{"the", "quick"}), linesToPlainText(layoutText(plain("the quick brown fox"), options)));
}

TEST(TerminalWrap, appendsAnEllipsisWhenMaxLinesCutsAtAParagraphBoundary) {
    LayoutOptions options;
    options.maxWidth = 10;
    options.maxLines = 1;
    ASSERT_EQ((Lines{"first…"}), linesToPlainText(layoutText(plain("first\nsecond"), options)));
}

TEST(TerminalWrap, ellipsisInheritsTheStyleOfTheTruncatedText) {
    Style red;
    red.fg = TermColor::ansi16(1);
    const auto lines = layoutText({TextRun{"abcdefgh", red}}, 5, WrapMode::TruncateEnd);
    ASSERT_EQ(1u, lines.size());
    ASSERT_EQ(1u, lines[0].segments.size());
    ASSERT_EQ("abcd…", lines[0].segments[0].text);
}

TEST(TerminalWrap, parsesWrapModes) {
    ASSERT_EQ(WrapMode::Wrap, *parseWrapMode("wrap"));
    ASSERT_EQ(WrapMode::TruncateEnd, *parseWrapMode("truncate"));
    ASSERT_EQ(WrapMode::TruncateEnd, *parseWrapMode("truncate-end"));
    ASSERT_EQ(WrapMode::TruncateMiddle, *parseWrapMode("middle"));
    ASSERT_EQ(WrapMode::TruncateStart, *parseWrapMode("truncate-start"));
    ASSERT_FALSE(parseWrapMode("nope").has_value());
}

} // namespace ValdiTest
