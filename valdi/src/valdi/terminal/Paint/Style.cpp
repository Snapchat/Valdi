#include "valdi/terminal/Paint/Style.hpp"

#include <algorithm>

namespace Valdi::Terminal {

static uint8_t clampChannel(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

TermColor TermColor::ansi16(int code) {
    TermColor color;
    color.kind = Kind::Ansi16;
    color.code = static_cast<uint8_t>(std::clamp(code, 0, 15));
    return color;
}

TermColor TermColor::ansi256(int code) {
    TermColor color;
    color.kind = Kind::Ansi256;
    color.code = clampChannel(code);
    return color;
}

TermColor TermColor::rgb(int r, int g, int b) {
    TermColor color;
    color.kind = Kind::Rgb;
    color.r = clampChannel(r);
    color.g = clampChannel(g);
    color.b = clampChannel(b);
    return color;
}

std::optional<TermColor> TermColor::fromValdiColor(const Color& color) {
    if (color.alpha() == 0) {
        return std::nullopt;
    }
    return rgb(color.red(), color.green(), color.blue());
}

bool TermColor::operator==(const TermColor& other) const {
    if (kind != other.kind) {
        return false;
    }
    if (kind == Kind::Rgb) {
        return r == other.r && g == other.g && b == other.b;
    }
    return code == other.code;
}

bool TermColor::operator!=(const TermColor& other) const {
    return !(*this == other);
}

bool Style::isEmpty() const {
    return !fg && !bg && !bold && !dim && !italic && !underline && !strikethrough && !inverse;
}

bool Style::operator==(const Style& other) const {
    return fg == other.fg && bg == other.bg && bold == other.bold && dim == other.dim && italic == other.italic &&
           underline == other.underline && strikethrough == other.strikethrough && inverse == other.inverse;
}

bool Style::operator!=(const Style& other) const {
    return !(*this == other);
}

Style Style::merge(const Style& base, const Style& overlay) {
    Style out = base;
    if (overlay.fg) {
        out.fg = overlay.fg;
    }
    if (overlay.bg) {
        out.bg = overlay.bg;
    }
    out.bold = out.bold || overlay.bold;
    out.dim = out.dim || overlay.dim;
    out.italic = out.italic || overlay.italic;
    out.underline = out.underline || overlay.underline;
    out.strikethrough = out.strikethrough || overlay.strikethrough;
    out.inverse = out.inverse || overlay.inverse;
    return out;
}

} // namespace Valdi::Terminal
