#include "valdi/terminal/Paint/Ansi.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Valdi::Terminal {

namespace Ansi {

std::string cursorUp(int count) {
    if (count <= 0) {
        return {};
    }
    return "\x1b[" + std::to_string(count) + "A";
}

std::string eraseLines(int count) {
    std::string out;
    for (int i = 0; i < count; i++) {
        out += kEraseLine;
        if (i < count - 1) {
            out += cursorUp(1);
        }
    }
    if (count > 0) {
        out += kCursorLeft;
    }
    return out;
}

} // namespace Ansi

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAlnum(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isIntroducerPrefix(char c) {
    return c == '[' || c == ']' || c == '(' || c == ')' || c == '#' || c == ';' || c == '?';
}

static bool isOscParameterChar(char c) {
    if (isAlnum(c)) {
        return true;
    }
    switch (c) {
        case '-':
        case '/':
        case '#':
        case '&':
        case '.':
        case ':':
        case '=':
        case '?':
        case '%':
        case '@':
        case '~':
        case '_':
            return true;
        default:
            return false;
    }
}

// Final bytes of ansi-regex's `[\dA-PR-TZcf-ntqry=><~]` class.
static bool isFinalByte(char c) {
    if (isDigit(c)) {
        return true;
    }
    if ((c >= 'A' && c <= 'P') || (c >= 'R' && c <= 'T') || c == 'Z') {
        return true;
    }
    if (c == 'c' || (c >= 'f' && c <= 'n') || c == 't' || c == 'q' || c == 'r' || c == 'y') {
        return true;
    }
    return c == '=' || c == '>' || c == '<' || c == '~';
}

// Returns the length of the escape sequence starting at `start`, or 0 when there is none.
static size_t matchAnsiAt(std::string_view s, size_t start) {
    size_t p = start;
    if (s[p] == '\x1b') {
        p += 1;
    } else if (static_cast<unsigned char>(s[p]) == 0xC2 && p + 1 < s.size() &&
               static_cast<unsigned char>(s[p + 1]) == 0x9B) {
        p += 2;
    } else {
        return 0;
    }

    while (p < s.size() && isIntroducerPrefix(s[p])) {
        p++;
    }

    // OSC style sequence terminated by BEL.
    size_t q = p;
    while (q < s.size() && isAlnum(s[q])) {
        q++;
    }
    while (q < s.size() && s[q] == ';') {
        q++;
        while (q < s.size() && isOscParameterChar(s[q])) {
            q++;
        }
    }
    if (q < s.size() && s[q] == '\x07') {
        return q + 1 - start;
    }

    // CSI style sequence: optional `\d{1,4}(;\d{0,4})*` parameters followed by a final byte.
    q = p;
    size_t digits = 0;
    while (q < s.size() && isDigit(s[q]) && digits < 4) {
        q++;
        digits++;
    }
    if (digits > 0) {
        while (q < s.size() && s[q] == ';') {
            q++;
            size_t count = 0;
            while (q < s.size() && isDigit(s[q]) && count < 4) {
                q++;
                count++;
            }
        }
    }
    // Any prefix of a valid parameter list is itself valid, so back off until a final byte fits.
    for (size_t end = q + 1; end > p; end--) {
        const size_t candidate = end - 1;
        if (candidate < s.size() && isFinalByte(s[candidate])) {
            return candidate + 1 - start;
        }
    }
    return 0;
}

std::string stripAnsi(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0x1b || c == 0xC2) {
            const size_t length = matchAnsiAt(text, i);
            if (length > 0) {
                i += length;
                continue;
            }
        }
        out.push_back(text[i]);
        i++;
    }
    return out;
}

static int roundHalfUp(double value) {
    return static_cast<int>(std::floor(value + 0.5));
}

int rgbToAnsi256(int r, int g, int b) {
    if (r == g && g == b) {
        if (r < 8) {
            return 16;
        }
        if (r > 248) {
            return 231;
        }
        return roundHalfUp(((r - 8) / 247.0) * 24) + 232;
    }
    return 16 + 36 * roundHalfUp((r / 255.0) * 5) + 6 * roundHalfUp((g / 255.0) * 5) + roundHalfUp((b / 255.0) * 5);
}

int rgbToAnsi16(int r, int g, int b) {
    const int maxChannel = std::max({r, g, b});
    if (maxChannel == 0) {
        return 0;
    }
    int code = (roundHalfUp(b / 255.0) << 2) | (roundHalfUp(g / 255.0) << 1) | roundHalfUp(r / 255.0);
    // ansi-styles brightens only when a channel is saturated, so mid-intensity colors and grays stay normal.
    if (maxChannel == 255) {
        code += 8;
    }
    return code;
}

int ansi256ToAnsi16(int code) {
    if (code < 16) {
        return code;
    }
    if (code >= 232) {
        const int gray = ((code - 232) * 10 + 8) & 0xff;
        return rgbToAnsi16(gray, gray, gray);
    }
    const int index = code - 16;
    const int r = (index / 36) * 51;
    const int g = ((index % 36) / 6) * 51;
    const int b = (index % 6) * 51;
    return rgbToAnsi16(r, g, b);
}

std::string colorParameters(const TermColor& color, ColorLevel level, bool background) {
    const int base = background ? 40 : 30;
    const int extended = background ? 48 : 38;
    TermColor resolved = color;
    if (level < ColorLevel::TrueColor && resolved.kind == TermColor::Kind::Rgb) {
        resolved = TermColor::ansi256(rgbToAnsi256(resolved.r, resolved.g, resolved.b));
    }
    if (level < ColorLevel::Ansi256 && resolved.kind == TermColor::Kind::Ansi256) {
        resolved = TermColor::ansi16(ansi256ToAnsi16(resolved.code));
    }
    switch (resolved.kind) {
        case TermColor::Kind::Rgb:
            return std::to_string(extended) + ";2;" + std::to_string(resolved.r) + ";" + std::to_string(resolved.g) +
                   ";" + std::to_string(resolved.b);
        case TermColor::Kind::Ansi256:
            return std::to_string(extended) + ";5;" + std::to_string(resolved.code);
        case TermColor::Kind::Ansi16:
            return std::to_string(resolved.code < 8 ? base + resolved.code : base + 60 + resolved.code - 8);
    }
    return {};
}

std::string styleToSgr(const Style& style, ColorLevel level) {
    if (level == ColorLevel::None) {
        return {};
    }
    std::string codes;
    const auto append = [&codes](const std::string& code) {
        if (!codes.empty()) {
            codes += ';';
        }
        codes += code;
    };
    if (style.bold) {
        append("1");
    }
    if (style.dim) {
        append("2");
    }
    if (style.italic) {
        append("3");
    }
    if (style.underline) {
        append("4");
    }
    if (style.inverse) {
        append("7");
    }
    if (style.strikethrough) {
        append("9");
    }
    if (style.fg) {
        append(colorParameters(*style.fg, level, false));
    }
    if (style.bg) {
        append(colorParameters(*style.bg, level, true));
    }
    if (codes.empty()) {
        return {};
    }
    return "\x1b[" + codes + "m";
}

std::string styledText(std::string_view text, const Style& style, ColorLevel level) {
    auto open = styleToSgr(style, level);
    if (open.empty()) {
        return std::string(text);
    }
    open.append(text);
    open.append(Ansi::kReset);
    return open;
}

static bool endsWith(const std::string& text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool startsWithAny(const std::string& text, std::initializer_list<std::string_view> prefixes) {
    for (const auto& prefix : prefixes) {
        if (text.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

ColorLevel detectColorLevel(const EnvGetter& env, bool isTTY) {
    const auto noColor = env("NO_COLOR");
    if (noColor && !noColor->empty()) {
        return ColorLevel::None;
    }

    const auto force = env("FORCE_COLOR");
    if (force) {
        if (force->empty() || *force == "true") {
            return ColorLevel::Basic16;
        }
        if (*force == "false") {
            return ColorLevel::None;
        }
        char* end = nullptr;
        const long level = std::strtol(force->c_str(), &end, 10);
        if (end != force->c_str()) {
            return static_cast<ColorLevel>(std::clamp(level, 0L, 3L));
        }
    }

    if (!isTTY) {
        return ColorLevel::None;
    }

    const std::string term = env("TERM").value_or("");
    if (term == "dumb") {
        return ColorLevel::None;
    }

    const std::string colorTerm = env("COLORTERM").value_or("");
    if (colorTerm == "truecolor" || colorTerm == "24bit") {
        return ColorLevel::TrueColor;
    }

    const std::string program = env("TERM_PROGRAM").value_or("");
    if (program == "iTerm.app" || program == "vscode" || program == "WezTerm" || program == "ghostty") {
        return ColorLevel::TrueColor;
    }
    if (program == "Apple_Terminal" || endsWith(term, "-256") || endsWith(term, "-256color")) {
        return ColorLevel::Ansi256;
    }
    if (startsWithAny(term, {"xterm", "screen", "tmux", "vt100", "vt220", "rxvt", "linux", "cygwin", "ansi"})) {
        return ColorLevel::Basic16;
    }
    return term.empty() ? ColorLevel::None : ColorLevel::Basic16;
}

EnvGetter processEnvGetter() {
    return [](std::string_view name) -> std::optional<std::string> {
        const char* value = std::getenv(std::string(name).c_str());
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string(value);
    };
}

} // namespace Valdi::Terminal
