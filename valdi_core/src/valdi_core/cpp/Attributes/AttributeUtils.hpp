//
//  AttributeUtils.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include <string>

namespace Valdi {

class ColorPalette;

struct Dimension {
    enum class Unit {
        // Unit not provided. Will be treated as points.
        None,
        // Unit provided as Pixels. The framework currently treats it identically than Points
        Px,
        // Unit provided as Points.
        Pt,
        Percent,
    };

    double value;
    Unit unit;

    constexpr Dimension(double value, Unit unit) : value(value), unit(unit) {}

    constexpr bool operator==(const Dimension& other) const {
        return value == other.value && unit == other.unit;
    }

    constexpr bool operator!=(const Dimension& other) const {
        return !(*this == other);
    }
};

class AttributeParser : public TextParser {
public:
    explicit AttributeParser(std::string_view str);

    std::optional<Color> parseColor(const ColorPalette& colorPalette);
    std::optional<Value> parseColorValue();
    std::optional<double> parseAngle();
    std::optional<Dimension> parseDimension();

private:
    std::optional<int64_t> parseColorComponent(bool isAlpha);
};

bool hasPrefix(const std::string_view& str, size_t index, const char* term);
bool hasSuffix(const std::string_view& str, const char* term);

} // namespace Valdi
