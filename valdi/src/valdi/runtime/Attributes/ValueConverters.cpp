//
//  ValueConverters.hpp
//  Valdi
//
//  Created by Simon Corsin on 7/8/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <fmt/format.h>
#include <fmt/ostream.h>

namespace Valdi {

Error ValueConverter::invalidTypeFailure(const Valdi::Value& value, Valdi::ValueType expectedType) {
    return Error(STRING_FORMAT("Invalid value type {} with value '{}', expected type {}",
                               valueTypeToString(value.getType()),
                               value.toStringBox(),
                               valueTypeToString(expectedType)));
}

Result<StringBox> ValueConverter::toString(const Value& value) {
    if (value.isString()) {
        return value.toStringBox();
    } else if (value.isNumber()) {
        if (value.isDouble()) {
            // Special case for double, avoid decimals if the number is
            // int representable
            auto d = value.toDouble();
            auto i = static_cast<int32_t>(d);
            if (d == static_cast<double>(i)) {
                return Value(i).toStringBox();
            }
        }

        return value.toStringBox();
    } else {
        return invalidTypeFailure(value, ValueType::StaticString);
    }
}

Result<int32_t> ValueConverter::toInt(const Value& value) {
    if (value.isNumber() || value.isString()) {
        return value.toInt();
    } else {
        return invalidTypeFailure(value, ValueType::Int);
    }
}

template<typename T>
std::optional<T> ensureParserAtEnd(AttributeParser& parser, std::optional<T>&& result) {
    if (!result) {
        return std::nullopt;
    }

    parser.tryParseWhitespaces();
    if (!parser.ensureIsAtEnd()) {
        return std::nullopt;
    }

    return std::move(result);
}

Result<Value> ValueConverter::toColor(const ColorPalette& colorPalette, const Value& value) {
    if (value.isString()) {
        auto colorName = value.toStringBox();
        auto color = colorPalette.getColorForName(colorName);
        if (!color) {
            return Error(STRING_FORMAT("Invalid color name '{}'", colorName));
        }
        return Value(color.value().value);
    }

    return value;
}

Result<Value> ValueConverter::toColorValue(const Value& value) {
    if (value.isNumber()) {
        return Value(value.toLong());
    } else if (value.isString()) {
        auto strBox = value.toStringBox();
        AttributeParser parser(strBox.toStringView());
        auto color = ensureParserAtEnd(parser, parser.parseColorValue());
        if (!color) {
            return parser.getError();
        }
        return color.value();
    } else {
        return invalidTypeFailure(value, ValueType::StaticString);
    }
}

Result<double> ValueConverter::toDouble(const Value& value) {
    if (value.isNumber()) {
        return value.toDouble();
    } else if (value.isString()) {
        auto strBox = value.toStringBox();
        AttributeParser parser(strBox.toStringView());
        auto d = ensureParserAtEnd(parser, parser.parseDimension());
        if (!d) {
            return parser.getError();
        }

        return d.value().value;
    } else {
        return invalidTypeFailure(value, ValueType::Double);
    }
}

Result<bool> ValueConverter::toBool(const Value& value) {
    if (value.isNumber() || value.isString()) {
        return value.toBool();
    }
    return invalidTypeFailure(value, ValueType::Bool);
}

std::optional<PercentValue> ValueConverter::parsePercent(AttributeParser& parser) {
    auto d = parser.parseDimension();
    if (!d) {
        return std::nullopt;
    }

    if (d.value().unit == Dimension::Unit::Percent) {
        return PercentValue(d.value().value, true);
    } else {
        return PercentValue(d.value().value, false);
    }
}

Result<PercentValue> ValueConverter::toPercent(const Value& value) {
    if (value.isNumber()) {
        return PercentValue(value.toDouble(), false);
    } else if (value.isString()) {
        auto strBox = value.toStringBox();
        AttributeParser parser(strBox.toStringView());
        auto percent = ensureParserAtEnd(parser, parsePercent(parser));
        if (!percent) {
            return parser.getError();
        }

        return percent.value();
    } else {
        return invalidTypeFailure(value, ValueType::Double);
    }
}

Result<Ref<BorderRadius>> ValueConverter::toBorderValues(const Value& value) {
    if (value.isValdiObject()) {
        auto borderRadius = castOrNull<BorderRadius>(value.getValdiObject());
        if (borderRadius == nullptr) {
            return invalidTypeFailure(value, ValueType::StaticString);
        }
        return borderRadius;
    }

    if (!value.isString()) {
        auto singlePercent = toPercent(value);
        if (!singlePercent) {
            return singlePercent.moveError();
        }

        auto percent = singlePercent.value();

        return Ref(makeShared<BorderRadius>(percent));
    }

    SmallVector<PercentValue, 4> values;

    auto strBox = value.toStringBox();

    AttributeParser parser(strBox.toStringView());
    while (!parser.isAtEnd()) {
        auto percent = parsePercent(parser);
        if (!percent) {
            return parser.getError();
        }

        values.emplace_back(percent.value());

        parser.tryParseWhitespaces();
    }

    if (values.empty() || values.size() > 4) {
        return Error(STRING_FORMAT(
            "Border values should have between 1 to 4 components, found {} in {}", values.size(), value.toStringBox()));
    }

    auto borders = makeShared<BorderRadius>();

    if (values.size() == 1) {
        borders->setAll(values[0]);
    } else if (values.size() == 2) {
        borders->setTopLeft(values[0]);
        borders->setTopRight(values[1]);
        borders->setBottomRight(values[0]);
        borders->setBottomLeft(values[1]);
    } else if (values.size() == 3) {
        borders->setTopLeft(values[0]);
        borders->setTopRight(values[1]);
        borders->setBottomRight(values[2]);
        borders->setBottomLeft(values[1]);
    } else {
        borders->setTopLeft(values[0]);
        borders->setTopRight(values[1]);
        borders->setBottomRight(values[2]);
        borders->setBottomLeft(values[3]);
    }

    return Ref<BorderRadius>(borders);
}

} // namespace Valdi
