#include "valdi/terminal/Paint/CellGrid.hpp"

#include <gtest/gtest.h>

using namespace Valdi::Terminal;

namespace ValdiTest {

static Style fgStyle(const TermColor& color) {
    Style style;
    style.fg = color;
    return style;
}

using Lines = std::vector<std::string>;

TEST(TerminalCellGrid, writesTextAndTrimsTrailingBlanks) {
    CellGrid grid(10, 2);
    grid.writeText(1, 0, "hi", Style(), grid.bounds());
    ASSERT_EQ((Lines{" hi", ""}), grid.toLines(ColorLevel::None));
}

TEST(TerminalCellGrid, clipsToTheGivenRectangle) {
    CellGrid grid(10, 1);
    grid.writeText(0, 0, "abcdef", Style(), IntRect(0, 0, 3, 1));
    ASSERT_EQ((Lines{"abc"}), grid.toLines(ColorLevel::None));
}

TEST(TerminalCellGrid, ignoresWritesOutsideTheGrid) {
    CellGrid grid(3, 1);
    grid.writeText(-2, 0, "abcdef", Style(), IntRect(-10, -10, 100, 100));
    grid.writeText(0, 5, "zzz", Style(), IntRect(-10, -10, 100, 100));
    ASSERT_EQ((Lines{"cde"}), grid.toLines(ColorLevel::None));
}

TEST(TerminalCellGrid, handlesWideCharactersAndPartialOverwrites) {
    CellGrid grid(6, 1);
    grid.writeText(0, 0, "日本", Style(), grid.bounds());
    ASSERT_EQ((Lines{"日本"}), grid.toLines(ColorLevel::None));
    grid.writeText(1, 0, "x", Style(), grid.bounds());
    ASSERT_EQ((Lines{" x本"}), grid.toLines(ColorLevel::None));
}

TEST(TerminalCellGrid, replacesAWideCharacterThatDoesNotFitTheClipWithASpace) {
    CellGrid grid(3, 1);
    grid.writeText(2, 0, "日", Style(), grid.bounds());
    ASSERT_EQ("   ", grid.rowText(0, 0, 3));
}

TEST(TerminalCellGrid, attachesZeroWidthClustersToThePreviousCell) {
    CellGrid grid(4, 1);
    grid.writeText(0, 0, "éx", Style(), grid.bounds());
    ASSERT_EQ((Lines{"éx"}), grid.toLines(ColorLevel::None));
}

TEST(TerminalCellGrid, aLaterOpaqueFillReplacesTheBackgroundAndCoversText) {
    CellGrid grid(5, 1);
    grid.writeText(0, 0, "hello", Style(), grid.bounds());
    grid.fillBackground(IntRect(0, 0, 5, 1), TermColor::rgb(255, 0, 0), grid.bounds());
    ASSERT_EQ("     ", grid.rowText(0, 0, 5));
    grid.fillBackground(IntRect(1, 0, 2, 1), TermColor::rgb(0, 0, 255), grid.bounds());
    ASSERT_EQ((Lines{"\x1b[48;2;255;0;0m \x1b[0m\x1b[48;2;0;0;255m  \x1b[0m\x1b[48;2;255;0;0m  \x1b[0m"}),
              grid.toLines(ColorLevel::TrueColor));
}

TEST(TerminalCellGrid, fillCoveringHalfAWideCharacterLeavesABlankInsteadOfAHalf) {
    CellGrid grid(4, 1);
    grid.writeText(0, 0, "日本", Style(), grid.bounds());
    grid.fillBackground(IntRect(1, 0, 2, 1), TermColor::rgb(1, 2, 3), grid.bounds());
    ASSERT_EQ("    ", grid.rowText(0, 0, 4));
}

TEST(TerminalCellGrid, aTranslucentFillTintsTheBackgroundAndKeepsTheText) {
    CellGrid grid(3, 1);
    grid.fillBackground(IntRect(0, 0, 3, 1), TermColor::rgb(255, 255, 255), grid.bounds());
    grid.writeText(0, 0, "ab", Style(), grid.bounds());
    grid.fillBackground(IntRect(0, 0, 3, 1), *TermColor::fromValdiColor(Valdi::Color(0, 0, 0, 128)), grid.bounds());
    ASSERT_EQ("ab ", grid.rowText(0, 0, 3));
    ASSERT_EQ((Lines{"\x1b[48;2;127;127;127mab \x1b[0m"}), grid.toLines(ColorLevel::TrueColor));
}

TEST(TerminalCellGrid, blendsTranslucentTextColorsAgainstTheCellBackground) {
    CellGrid grid(2, 1);
    grid.fillBackground(IntRect(0, 0, 2, 1), TermColor::rgb(255, 255, 255), grid.bounds());
    Style style;
    style.fg = TermColor::fromValdiColor(Valdi::Color(0, 0, 0, 128));
    grid.writeText(0, 0, "x", style, grid.bounds());
    ASSERT_EQ((Lines{"\x1b[38;2;127;127;127;48;2;255;255;255mx\x1b[0m\x1b[48;2;255;255;255m \x1b[0m"}),
              grid.toLines(ColorLevel::TrueColor));
}

TEST(TerminalCellGrid, translucentColorsWithoutABackdropBlendAgainstBlack) {
    CellGrid grid(1, 1);
    const auto translucentWhite = *TermColor::fromValdiColor(Valdi::Color(255, 255, 255, 128));
    grid.fillBackground(IntRect(0, 0, 1, 1), translucentWhite, grid.bounds());
    ASSERT_EQ((Lines{"\x1b[48;2;128;128;128m \x1b[0m"}), grid.toLines(ColorLevel::TrueColor));
}

TEST(TerminalCellGrid, leadingZeroWidthClustersJoinTheNextCellInsteadOfTheLeftNeighbor) {
    CellGrid grid(4, 1);
    grid.writeText(0, 0, "ab", Style(), grid.bounds());
    grid.writeText(2,
                   0,
                   "\xcc\x81"
                   "c",
                   Style(),
                   grid.bounds());
    ASSERT_EQ("b", grid.rowText(0, 1, 1));
    ASSERT_EQ("\xcc\x81"
              "c",
              grid.rowText(0, 2, 1));
    grid.writeText(3, 0, "\xcc\x81", Style(), grid.bounds());
    ASSERT_EQ(" ", grid.rowText(0, 3, 1));
}

TEST(TerminalCellGrid, emitsMinimalSgrChanges) {
    const auto red = fgStyle(TermColor::ansi16(1));
    CellGrid grid(6, 1);
    grid.writeText(0, 0, "ab", red, grid.bounds());
    grid.writeText(2, 0, "cd", red, grid.bounds());
    grid.writeText(4, 0, "e", Style(), grid.bounds());
    ASSERT_EQ((Lines{"\x1b[31mabcd\x1b[0me"}), grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalCellGrid, textWrittenAfterAFillKeepsTheFillBackground) {
    CellGrid grid(4, 1);
    grid.fillBackground(IntRect(0, 0, 4, 1), TermColor::ansi16(4), grid.bounds());
    grid.writeText(0, 0, "ab", fgStyle(TermColor::ansi16(1)), grid.bounds());
    ASSERT_EQ((Lines{"\x1b[31;44mab\x1b[0m\x1b[44m  \x1b[0m"}), grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalCellGrid, keepsTheBackgroundWhenTextIsWrittenOverIt) {
    CellGrid grid(3, 1);
    grid.fillBackground(IntRect(0, 0, 3, 1), TermColor::ansi16(4), grid.bounds());
    grid.writeText(0, 0, "a", fgStyle(TermColor::ansi16(2)), grid.bounds());
    ASSERT_EQ((Lines{"\x1b[32;44ma\x1b[0m\x1b[44m  \x1b[0m"}), grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalCellGrid, keepsStyledBlanksOnlyWhenColorsAreEnabled) {
    CellGrid grid(3, 1);
    grid.fillBackground(IntRect(0, 0, 3, 1), TermColor::ansi16(4), grid.bounds());
    ASSERT_EQ((Lines{""}), grid.toLines(ColorLevel::None));
    ASSERT_EQ((Lines{"\x1b[44m   \x1b[0m"}), grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalCellGrid, rowTextCountsWideCharactersOnceAndEmptyCellsAsSpaces) {
    CellGrid grid(6, 1);
    grid.writeText(1, 0, "日", Style(), grid.bounds());
    ASSERT_EQ(" 日   ", grid.rowText(0, 0, 6));
    ASSERT_EQ("", grid.rowText(3, 0, 6));
}

} // namespace ValdiTest
