#pragma once

#include "valdi_core/cpp/Attributes/ColorPalette.hpp"

#include <cstdint>
#include <optional>

namespace Valdi::Terminal {

/**
 * A color a terminal can display. Palette colors keep their index so named ANSI colors emit the
 * short SGR codes instead of being converted through RGB.
 */
struct TermColor {
    enum class Kind : uint8_t { Ansi16, Ansi256, Rgb };

    Kind kind = Kind::Rgb;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t code = 0;
    /** Opacity of an Rgb color; translucent colors are blended against the cell below when written to a grid. */
    uint8_t alpha = 255;

    static TermColor ansi16(int code);
    static TermColor ansi256(int code);
    static TermColor rgb(int r, int g, int b);

    /**
     * Converts a Valdi RGBA color, keeping its alpha. Fully transparent colors have no terminal
     * equivalent and yield nullopt.
     */
    static std::optional<TermColor> fromValdiColor(const Color& color);

    /** The color as RGB, using the default xterm values for palette colors. */
    TermColor toRgb() const;

    /** Source-over composite of this color on `backdrop`; the result is opaque. Opaque colors are returned as is. */
    TermColor blendedOver(const std::optional<TermColor>& backdrop) const;

    bool operator==(const TermColor& other) const;
    bool operator!=(const TermColor& other) const;
};

/** Visual attributes of a run of terminal text. */
struct Style {
    std::optional<TermColor> fg;
    std::optional<TermColor> bg;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool inverse = false;

    /** True when the style changes nothing about how a cell renders. */
    bool isEmpty() const;

    bool operator==(const Style& other) const;
    bool operator!=(const Style& other) const;

    /** Overlays `overlay` on `base`: colors set on the overlay win, boolean attributes accumulate. */
    static Style merge(const Style& base, const Style& overlay);
};

} // namespace Valdi::Terminal
