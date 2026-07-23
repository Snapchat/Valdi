//
//  AttributeUtils.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <cstdlib>
#include <string_view>

#include <cmath>

namespace Valdi {

AttributeParser::AttributeParser(std::string_view str) : TextParser(str) {}

std::optional<int64_t> AttributeParser::parseColorComponent(bool isAlpha) {
    auto isPercent = false;
    auto component = parseDouble();
    if (!component) {
        return std::nullopt;
    }

    if (tryParse('%')) {
        isPercent = true;
    }

    tryParseWhitespaces();

    if (isAlpha) {
        if (isPercent) {
            return static_cast<int64_t>((component.value() / 100.0) * 255.0);
        } else {
            return static_cast<int64_t>(component.value() * 255.0);
        }
    } else {
        if (isPercent) {
            return static_cast<int64_t>(255.0 * component.value() / 100.0);
        } else {
            return static_cast<int64_t>(component.value());
        }
    }
}

std::optional<Color> AttributeParser::parseColor(const ColorPalette& colorPalette) {
    auto colorValue = parseColorValue();
    if (!colorValue) {
        return std::nullopt;
    }

    if (colorValue->isNumber()) {
        return Color(colorValue->toLong());
    }

    auto colorName = colorValue->toStringBox();
    auto color = colorPalette.getColorForName(colorName);
    if (!color) {
        setErrorAtCurrentPosition(Error(STRING_FORMAT("Invalid color name '{}'", colorName)));
    }
    return color;
}

std::optional<Value> AttributeParser::parseColorValue() {
    tryParseWhitespaces();

    if (tryParse("rgba(")) {
        auto r = parseColorComponent(false);
        if (!r) {
            return std::nullopt;
        }

        if (!parse(',')) {
            return std::nullopt;
        }

        auto g = parseColorComponent(false);
        if (!g) {
            return std::nullopt;
        }
        if (!parse(',')) {
            return std::nullopt;
        }

        auto b = parseColorComponent(false);
        if (!b) {
            return std::nullopt;
        }

        if (!parse(',')) {
            return std::nullopt;
        }

        auto a = parseColorComponent(true);
        if (!a) {
            return std::nullopt;
        }

        tryParseWhitespaces();

        if (!parse(')')) {
            return std::nullopt;
        }

        return Value(Color(r.value(), g.value(), b.value(), a.value()).value);
    } else {
        if (tryParse('#')) {
            auto currentPosition = position();
            auto colorValue = parseHexLong();
            if (!colorValue) {
                return std::nullopt;
            }
            auto length = position() - currentPosition;

            auto color = static_cast<int64_t>(colorValue.value());

            // Expand shorthand hex to 6 digit
            if (length == 3) {
                color = ((color & 0xF00) << 8) | ((color & 0x0F0) << 4) | (color & 0x00F);
                color = color | (color << 4);
                length = 6;
            }

            // If no alpha component, set it to 0xFF
            if (length <= 6) {
                color = 0xFF | color << 8;
            }

            return Value(color);
        } else {
            tryParseWhitespaces();
            auto keyword = parseIdentifier();
            if (!keyword || keyword.value().empty()) {
                return std::nullopt;
            }
            return Value(StringCache::getGlobal().makeString(keyword.value()));
        }
    }
}

std::optional<double> AttributeParser::parseAngle() {
    auto angleValue = parseDouble();
    if (!angleValue) {
        return std::nullopt;
    }

    if (tryParse("rad")) {
        return angleValue.value();
    } else if (tryParse("deg")) {
        return angleValue.value() * M_PI / 180.0;
    } else {
        setErrorAtCurrentPosition("Unrecognized angle unit");
        return std::nullopt;
    }
}

std::optional<Dimension> AttributeParser::parseDimension() {
    tryParseWhitespaces();
    auto d = parseDouble();
    if (!d) {
        return std::nullopt;
    }

    Dimension::Unit unit = Dimension::Unit::None;
    if (tryParse("px")) {
        unit = Dimension::Unit::Px;
    } else if (tryParse("pt")) {
        unit = Dimension::Unit::Pt;
    } else if (tryParse('%')) {
        unit = Dimension::Unit::Percent;
    }

    return Dimension(d.value(), unit);
}

bool hasPrefix(const std::string_view& str, size_t index, const char* term) {
    if (index >= str.size()) {
        return false;
    }

    const auto* cStr = str.data() + index;
    const auto* current = term;
    while (*current != '\0') {
        if (*cStr != *current) {
            return false;
        }

        cStr++;
        current++;
    }

    return true;
}

bool hasSuffix(const std::string_view& str, const char* term) {
    auto termLength = strlen(term);
    if (termLength > str.size()) {
        return false;
    }

    return str.compare(str.size() - termLength, termLength, term) == 0;
}

} // namespace Valdi
