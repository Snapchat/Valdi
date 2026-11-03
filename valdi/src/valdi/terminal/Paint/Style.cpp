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
    auto result = rgb(color.red(), color.green(), color.blue());
    result.alpha = color.alpha();
    return result;
}

static constexpr uint8_t kAnsi16Rgb[16][3] = {
    {0, 0, 0},
    {205, 0, 0},
    {0, 205, 0},
    {205, 205, 0},
    {0, 0, 238},
    {205, 0, 205},
    {0, 205, 205},
    {229, 229, 229},
    {127, 127, 127},
    {255, 0, 0},
    {0, 255, 0},
    {255, 255, 0},
    {92, 92, 255},
    {255, 0, 255},
    {0, 255, 255},
    {255, 255, 255},
};

TermColor TermColor::toRgb() const {
    switch (kind) {
        case Kind::Rgb:
            return *this;
        case Kind::Ansi16:
            return rgb(kAnsi16Rgb[code & 15][0], kAnsi16Rgb[code & 15][1], kAnsi16Rgb[code & 15][2]);
        case Kind::Ansi256: {
            if (code < 16) {
                return rgb(kAnsi16Rgb[code][0], kAnsi16Rgb[code][1], kAnsi16Rgb[code][2]);
            }
            if (code >= 232) {
                const int gray = 8 + (code - 232) * 10;
                return rgb(gray, gray, gray);
            }
            static constexpr int kLevels[6] = {0, 95, 135, 175, 215, 255};
            const int index = code - 16;
            return rgb(kLevels[index / 36], kLevels[(index % 36) / 6], kLevels[index % 6]);
        }
    }
    return *this;
}

TermColor TermColor::blendedOver(const std::optional<TermColor>& backdrop) const {
    if (kind != Kind::Rgb || alpha == 255) {
        return *this;
    }
    // Without a backdrop the terminal default is unknown; black is the usual dark-theme assumption.
    const TermColor below = backdrop ? backdrop->toRgb() : rgb(0, 0, 0);
    const auto mix = [this](int source, int destination) {
        return (source * alpha + destination * (255 - alpha) + 127) / 255;
    };
    return rgb(mix(r, below.r), mix(g, below.g), mix(b, below.b));
}

bool TermColor::operator==(const TermColor& other) const {
    if (kind != other.kind) {
        return false;
    }
    if (kind == Kind::Rgb) {
        return r == other.r && g == other.g && b == other.b && alpha == other.alpha;
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
