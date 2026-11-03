#include "TerminalTestUtils.hpp"

#include <gtest/gtest.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

using Lines = std::vector<std::string>;

TEST(TerminalPainter, drawsBordersAndNestedTextInParentCoordinates) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 9, 3);
    tree->style().borderWidth = 1;
    tree->insertChild(b.text(1, 1, 7, 1, "hello"), 0);
    auto painted = paintTree(*tree, 9, 3);
    ASSERT_EQ((Lines{"┌───────┐", "│hello  │", "└───────┘"}), painted.grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, supportsPartialBordersAndRoundStyle) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 5, 2);
    tree->style().borderWidth = 1;
    tree->style().borderStyle = "round";
    tree->style().sideEnabled[static_cast<size_t>(TerminalBorderSide::Bottom)] = false;
    ASSERT_EQ((Lines{"╭───╮", "│   │"}), paintTree(*tree, 5, 2).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, drawsNoBorderWithoutAWidth) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 3, 2);
    ASSERT_EQ((Lines{"", ""}), paintTree(*tree, 3, 2).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, fallsBackToTheSingleStyleForUnknownBorderStyles) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 3, 2);
    tree->style().borderWidth = 1;
    tree->style().borderStyle = "nonsense";
    ASSERT_EQ((Lines{"┌─┐", "└─┘"}), paintTree(*tree, 3, 2).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, wrapsTextToItsFrame) {
    TerminalViewBuilder b;
    auto painted = paintTree(*b.text(0, 0, 5, 2, "hello world"), 5, 2);
    ASSERT_EQ((Lines{"hello", "world"}), painted.grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, clipsChildrenWhenOverflowIsHidden) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 3, 1);
    tree->setClipsChildren(true);
    tree->insertChild(b.text(0, 0, 10, 1, "abcdef"), 0);
    ASSERT_EQ((Lines{"abc"}), paintTree(*tree, 10, 1).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, letsChildrenOverflowByDefault) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 3, 1);
    tree->insertChild(b.text(0, 0, 10, 1, "abcdef"), 0);
    ASSERT_EQ((Lines{"abcdef"}), paintTree(*tree, 10, 1).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, rendersStaticContentSeparately) {
    TerminalViewBuilder b;
    auto staticView = b.make("ValdiTerminalStatic", 0, 0, 10, 2);
    staticView->insertChild(b.text(0, 0, 10, 1, "one"), 0);
    staticView->insertChild(b.text(0, 1, 10, 1, "two"), 1);
    auto tree = b.box(0, 0, 10, 1);
    tree->insertChild(staticView, 0);
    tree->insertChild(b.text(0, 0, 10, 1, "live"), 1);

    auto painted = paintTree(*tree, 10, 1);
    ASSERT_EQ((Lines{"live"}), painted.grid.toLines(ColorLevel::None));
    ASSERT_EQ(1u, painted.staticViews.size());
    ASSERT_EQ(staticView.get(), painted.staticViews[0]);

    auto slice = Painter::paintStaticSlice(*staticView, 0);
    ASSERT_EQ(0, slice.firstRow);
    ASSERT_EQ((Lines{"one", "two"}), slice.grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, staticSlicesStartAtTheFirstNewChild) {
    TerminalViewBuilder b;
    auto staticView = b.make("ValdiTerminalStatic", 0, 0, 10, 3);
    staticView->insertChild(b.text(0, 0, 10, 1, "one"), 0);
    staticView->insertChild(b.text(0, 1, 10, 1, "two"), 1);
    staticView->insertChild(b.text(0, 2, 10, 1, "three"), 2);

    auto slice = Painter::paintStaticSlice(*staticView, 1);
    ASSERT_EQ(1, slice.firstRow);
    const auto lines = slice.grid.toLines(ColorLevel::None);
    ASSERT_EQ((Lines{"", "two", "three"}), lines);

    auto empty = Painter::paintStaticSlice(*staticView, 3);
    ASSERT_EQ(0, empty.grid.height());
}

TEST(TerminalPainter, paintsBackgroundsAndSkipsZeroSizedViews) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 3, 1);
    tree->style().backgroundColor = TermColor::ansi16(4);
    auto hidden = b.box(0, 0, 0, 0);
    hidden->style().backgroundColor = TermColor::ansi16(1);
    tree->insertChild(hidden, 0);
    ASSERT_EQ((Lines{"\x1b[44m   \x1b[0m"}), paintTree(*tree, 3, 1).grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalPainter, rendersLabelsWithTheirColor) {
    TerminalViewBuilder b;
    auto label = b.text(0, 0, 5, 1, "label", 1);
    label->label().baseStyle.fg = TermColor::ansi16(1);
    ASSERT_EQ((Lines{"\x1b[31mlabel\x1b[0m"}), paintTree(*label, 5, 1).grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalPainter, appliesBoldFromTheLabelStyle) {
    TerminalViewBuilder b;
    auto label = b.text(0, 0, 3, 1, "hey", 1);
    label->label().baseStyle.bold = true;
    ASSERT_EQ((Lines{"\x1b[1mhey\x1b[0m"}), paintTree(*label, 3, 1).grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalPainter, labelRunStylesOverrideTheLabelStyle) {
    TerminalViewBuilder b;
    auto label = b.text(0, 0, 4, 1, "", 1);
    Style green;
    green.fg = TermColor::ansi16(2);
    label->label().params.runs = {TextRun{"ab", Style()}, TextRun{"cd", green}};
    label->label().baseStyle.fg = TermColor::ansi16(1);
    ASSERT_EQ((Lines{"\x1b[31mab\x1b[0m\x1b[32mcd\x1b[0m"}), paintTree(*label, 4, 1).grid.toLines(ColorLevel::Basic16));
}

TEST(TerminalPainter, alignsLabelText) {
    TerminalViewBuilder b;
    auto center = b.text(0, 0, 7, 1, "abc", 1);
    center->label().align = TerminalTextAlign::Center;
    ASSERT_EQ((Lines{"  abc"}), paintTree(*center, 7, 1).grid.toLines(ColorLevel::None));

    auto right = b.text(0, 0, 7, 1, "abc", 1);
    right->label().align = TerminalTextAlign::Right;
    ASSERT_EQ((Lines{"    abc"}), paintTree(*right, 7, 1).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, truncatesASingleLineLabelWithAnEllipsis) {
    TerminalViewBuilder b;
    auto label = b.text(0, 0, 6, 1, "hello world", 1);
    ASSERT_EQ((Lines{"hello…"}), paintTree(*label, 6, 1).grid.toLines(ColorLevel::None));

    label->label().params.ellipsize = false;
    label->label().invalidate();
    ASSERT_EQ((Lines{"hello"}), paintTree(*label, 6, 1).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, paintsWideCharactersAcrossTwoCells) {
    TerminalViewBuilder b;
    auto label = b.text(0, 0, 4, 1, "日本", 1);
    ASSERT_EQ((Lines{"日本"}), paintTree(*label, 4, 1).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, translatesScrollChildrenByTheContentOffset) {
    TerminalViewBuilder b;
    auto scroll = b.make("SCValdiScrollView", 0, 0, 5, 2);
    for (int i = 0; i < 4; i++) {
        scroll->insertChild(b.text(0, static_cast<float>(i), 5, 1, "row" + std::to_string(i), 1),
                            static_cast<size_t>(i));
    }
    ASSERT_EQ((Lines{"row0", "row1", "", ""}), paintTree(*scroll, 5, 4).grid.toLines(ColorLevel::None));

    scroll->setScrollSpecs(Point(0, 1), Size(5, 4));
    ASSERT_EQ((Lines{"row1", "row2", "", ""}), paintTree(*scroll, 5, 4).grid.toLines(ColorLevel::None));
}

TEST(TerminalPainter, appliesBorderColorsPerSide) {
    TerminalViewBuilder b;
    auto tree = b.box(0, 0, 3, 3);
    tree->style().borderWidth = 1;
    tree->style().borderColor = TermColor::ansi16(2);
    tree->style().sideColors[static_cast<size_t>(TerminalBorderSide::Top)] = TermColor::ansi16(1);
    tree->style().borderDim = true;
    const auto lines = paintTree(*tree, 3, 3).grid.toLines(ColorLevel::Basic16);
    ASSERT_EQ("\x1b[2;31m┌─┐\x1b[0m", lines[0]);
    ASSERT_EQ("\x1b[2;32m│\x1b[0m \x1b[2;32m│\x1b[0m", lines[1]);
    ASSERT_EQ("\x1b[2;32m└─┘\x1b[0m", lines[2]);
}

TEST(TerminalPainter, roundsFractionalFramesToWholeCells) {
    ASSERT_EQ(IntRect(1, 2, 2, 1), Painter::roundRect(Frame(0.6f, 1.6f, 2.6f, 1.0f), 0, 0));
    ASSERT_EQ(IntRect(11, 2, 3, 1), Painter::roundRect(Frame(1.0f, 2.0f, 3.0f, 1.0f), 10, 0));
}

} // namespace ValdiTest
