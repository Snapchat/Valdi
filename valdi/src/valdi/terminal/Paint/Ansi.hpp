//
//  Ansi.hpp
//
//  The RGB to ANSI 256 / ANSI 16 conversions are ported from ansi-styles, and the escape sequence
//  matcher from ansi-regex. Both are distributed under the MIT license:
//
//  MIT License
//
//  Copyright (c) Sindre Sorhus <sindresorhus@gmail.com> (https://sindresorhus.com)
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
//  associated documentation files (the "Software"), to deal in the Software without restriction,
//  including without limitation the rights to use, copy, modify, merge, publish, distribute,
//  sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all copies or
//  substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//  NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include "valdi/terminal/Paint/Style.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Valdi::Terminal {

/** How many colors the output supports. */
enum class ColorLevel : int {
    None = 0,
    Basic16 = 1,
    Ansi256 = 2,
    TrueColor = 3,
};

namespace Ansi {

inline constexpr std::string_view kCursorHide = "\x1b[?25l";
inline constexpr std::string_view kCursorShow = "\x1b[?25h";
inline constexpr std::string_view kCursorLeft = "\x1b[G";
inline constexpr std::string_view kEraseLine = "\x1b[2K";
inline constexpr std::string_view kEraseDown = "\x1b[J";
inline constexpr std::string_view kClearTerminal = "\x1b[2J\x1b[3J\x1b[H";
inline constexpr std::string_view kReset = "\x1b[0m";

/** Moves the cursor up by `count` lines. Empty when count is not positive. */
std::string cursorUp(int count);

/** Erases `count` lines ending at the cursor line and leaves the cursor at the start of the first one. */
std::string eraseLines(int count);

} // namespace Ansi

/** Removes ANSI escape sequences from `text`. */
std::string stripAnsi(std::string_view text);

int rgbToAnsi256(int r, int g, int b);
int rgbToAnsi16(int r, int g, int b);
int ansi256ToAnsi16(int code);

/** The SGR parameter list (without the CSI prefix and `m` suffix) for `color` downgraded to `level`. */
std::string colorParameters(const TermColor& color, ColorLevel level, bool background);

/** The SGR sequence that enables `style`, or an empty string when nothing is set or colors are disabled. */
std::string styleToSgr(const Style& style, ColorLevel level);

/** `text` wrapped in the SGR sequence for `style` and a reset. */
std::string styledText(std::string_view text, const Style& style, ColorLevel level);

/** Looks up an environment variable; returns nullopt when it is not set. */
using EnvGetter = std::function<std::optional<std::string>(std::string_view name)>;

/** Decides the color level from NO_COLOR, FORCE_COLOR, COLORTERM, TERM_PROGRAM and TERM. */
ColorLevel detectColorLevel(const EnvGetter& env, bool isTTY);

/** An EnvGetter backed by the process environment. */
EnvGetter processEnvGetter();

} // namespace Valdi::Terminal
