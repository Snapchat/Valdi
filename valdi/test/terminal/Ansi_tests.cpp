#include "valdi/terminal/Paint/Ansi.hpp"

#include <gtest/gtest.h>
#include <map>

using namespace Valdi::Terminal;

namespace ValdiTest {

static EnvGetter makeEnv(std::map<std::string, std::string> values) {
    return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
        const auto it = values.find(std::string(name));
        if (it == values.end()) {
            return std::nullopt;
        }
        return it->second;
    };
}

TEST(TerminalAnsi, convertsValdiColors) {
    const auto orange = TermColor::fromValdiColor(Valdi::Color(255, 136, 0, 255));
    ASSERT_TRUE(orange.has_value());
    ASSERT_EQ(TermColor::rgb(255, 136, 0), *orange);
    ASSERT_FALSE(TermColor::fromValdiColor(Valdi::Color(255, 136, 0, 0)).has_value());
}

TEST(TerminalAnsi, emitsNothingWithoutColors) {
    Style style;
    style.bold = true;
    style.fg = TermColor::ansi16(1);
    ASSERT_EQ("", styleToSgr(style, ColorLevel::None));
    ASSERT_EQ("", styleToSgr(Style(), ColorLevel::TrueColor));
}

TEST(TerminalAnsi, downgradesColorsToTheSupportedLevel) {
    Style red;
    red.fg = TermColor::rgb(255, 0, 0);
    ASSERT_EQ("\x1b[38;2;255;0;0m", styleToSgr(red, ColorLevel::TrueColor));
    ASSERT_EQ("\x1b[38;5;" + std::to_string(rgbToAnsi256(255, 0, 0)) + "m", styleToSgr(red, ColorLevel::Ansi256));
    ASSERT_EQ("\x1b[91m", styleToSgr(red, ColorLevel::Basic16));

    Style blueBold;
    blueBold.bg = TermColor::ansi16(4);
    blueBold.bold = true;
    ASSERT_EQ("\x1b[1;44m", styleToSgr(blueBold, ColorLevel::Basic16));
}

TEST(TerminalAnsi, keepsPaletteColorsAtEveryLevel) {
    Style style;
    style.fg = TermColor::ansi256(200);
    ASSERT_EQ("\x1b[38;5;200m", styleToSgr(style, ColorLevel::TrueColor));
    ASSERT_EQ("\x1b[38;5;200m", styleToSgr(style, ColorLevel::Ansi256));
    ASSERT_EQ("\x1b[" + std::to_string(30 + 60 + ansi256ToAnsi16(200) - 8) + "m",
              styleToSgr(style, ColorLevel::Basic16));

    Style bright;
    bright.fg = TermColor::ansi16(9);
    ASSERT_EQ("\x1b[91m", styleToSgr(bright, ColorLevel::TrueColor));
}

TEST(TerminalAnsi, mapsTextAttributes) {
    Style style;
    style.dim = true;
    style.italic = true;
    style.underline = true;
    style.inverse = true;
    style.strikethrough = true;
    ASSERT_EQ("\x1b[2;3;4;7;9m", styleToSgr(style, ColorLevel::Basic16));
}

TEST(TerminalAnsi, wrapsTextInStyles) {
    Style style;
    style.fg = TermColor::ansi16(2);
    ASSERT_EQ("\x1b[32mok\x1b[0m", styledText("ok", style, ColorLevel::Basic16));
    ASSERT_EQ("ok", styledText("ok", style, ColorLevel::None));
}

TEST(TerminalAnsi, convertsRgbToPalettes) {
    ASSERT_EQ(16, rgbToAnsi256(0, 0, 0));
    ASSERT_EQ(231, rgbToAnsi256(255, 255, 255));
    ASSERT_EQ(196, rgbToAnsi256(255, 0, 0));
    ASSERT_EQ(9, rgbToAnsi16(255, 0, 0));
    ASSERT_EQ(0, rgbToAnsi16(0, 0, 0));
    // ansi-styles brightens only saturated colors; 200 is not 255.
    ASSERT_EQ(5, rgbToAnsi16(200, 0, 200));
    ASSERT_EQ(13, rgbToAnsi16(255, 0, 255));
    ASSERT_EQ(7, ansi256ToAnsi16(244));
    ASSERT_EQ(15, ansi256ToAnsi16(231));
    ASSERT_EQ(9, ansi256ToAnsi16(196));
    ASSERT_EQ(3, ansi256ToAnsi16(3));
}

TEST(TerminalAnsi, downgradesMidIntensityColorsToNormalBasicColors) {
    Style magenta;
    magenta.fg = TermColor::rgb(200, 0, 200);
    ASSERT_EQ("\x1b[35m", styleToSgr(magenta, ColorLevel::Basic16));
    Style gray;
    gray.fg = TermColor::rgb(128, 128, 128);
    ASSERT_EQ("\x1b[37m", styleToSgr(gray, ColorLevel::Basic16));
}

TEST(TerminalAnsi, keepsTheAlphaOfValdiColorsAndBlendsOverABackdrop) {
    const auto half = TermColor::fromValdiColor(Valdi::Color(0, 0, 0, 128));
    ASSERT_TRUE(half.has_value());
    ASSERT_EQ(128, half->alpha);
    ASSERT_EQ(TermColor::rgb(127, 127, 127), half->blendedOver(TermColor::rgb(255, 255, 255)));
    ASSERT_EQ(TermColor::rgb(0, 0, 0), half->blendedOver(std::nullopt));
    ASSERT_EQ(TermColor::rgb(0, 205, 0), TermColor::ansi16(2).toRgb());
    ASSERT_EQ(TermColor::rgb(255, 0, 0), TermColor::rgb(255, 0, 0).blendedOver(TermColor::rgb(1, 2, 3)));
}

TEST(TerminalAnsi, detectsColorLevelFromTheEnvironment) {
    ASSERT_EQ(ColorLevel::None, detectColorLevel(makeEnv({{"NO_COLOR", "1"}, {"COLORTERM", "truecolor"}}), true));
    ASSERT_EQ(ColorLevel::TrueColor, detectColorLevel(makeEnv({{"FORCE_COLOR", "3"}}), false));
    ASSERT_EQ(ColorLevel::None, detectColorLevel(makeEnv({{"FORCE_COLOR", "0"}}), true));
    ASSERT_EQ(ColorLevel::Basic16, detectColorLevel(makeEnv({{"FORCE_COLOR", ""}}), false));
    ASSERT_EQ(ColorLevel::None, detectColorLevel(makeEnv({{"TERM", "xterm-256color"}}), false));
    ASSERT_EQ(ColorLevel::TrueColor, detectColorLevel(makeEnv({{"COLORTERM", "truecolor"}}), true));
    ASSERT_EQ(ColorLevel::Ansi256, detectColorLevel(makeEnv({{"TERM", "xterm-256color"}}), true));
    ASSERT_EQ(ColorLevel::Basic16, detectColorLevel(makeEnv({{"TERM", "xterm"}}), true));
    ASSERT_EQ(ColorLevel::None, detectColorLevel(makeEnv({{"TERM", "dumb"}}), true));
}

TEST(TerminalAnsi, erasesLinesUpward) {
    ASSERT_EQ("", Ansi::eraseLines(0));
    ASSERT_EQ("\x1b[2K\x1b[1A\x1b[2K\x1b[G", Ansi::eraseLines(2));
    ASSERT_EQ("", Ansi::cursorUp(0));
    ASSERT_EQ("\x1b[3A", Ansi::cursorUp(3));
}

TEST(TerminalAnsi, stripsEscapeSequences) {
    ASSERT_EQ("bold plain", stripAnsi("\x1b[1mbold\x1b[0m plain"));
    ASSERT_EQ("link", stripAnsi("\x1b]8;;https://example.com\x07link\x1b]8;;\x07"));
    ASSERT_EQ("plain \xc3\xa9", stripAnsi("plain \xc3\xa9"));
    ASSERT_EQ("x",
              stripAnsi("\xc2\x9b"
                        "31mx"));
    ASSERT_EQ("a\x1b", stripAnsi("a\x1b"));
}

} // namespace ValdiTest
