//
//  DefaultAttributeProcessors.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/27/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Attributes/DefaultAttributeProcessors.hpp"

#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"
#include <fmt/format.h>
#include <optional>

namespace Valdi {

static Error parseError(AttributeParser& parser, std::string_view name) {
    parser.prependError(fmt::format("Failed to parse {}", name));
    return parser.getError();
}

Result<Value> preprocessBorder(const Value& in) {
    auto stringBox = in.toStringBox();

    AttributeParser parser(stringBox.toStringView());

    auto borderWidth = parser.parseDimension();
    if (!borderWidth) {
        return parseError(parser, "border width");
    }

    parser.tryParseWhitespaces();

    Ref<ValueArray> border;

    if (!parser.isAtEnd()) {
        // Ignoring border style since we can only support solid
        auto identifierResult = parser.parseIdentifier();
        if (!identifierResult) {
            return parseError(parser, "border style");
        }

        auto color = parser.parseColorValue();
        if (!color) {
            return parseError(parser, "border color");
        }

        parser.tryParseWhitespaces();

        if (!parser.ensureIsAtEnd()) {
            return parser.getError();
        }

        border = ValueArray::make({Value(borderWidth.value().value), color.value()});

    } else {
        border = ValueArray::make({Value(borderWidth.value().value)});
    }

    return Value(border);
}

Result<Value> preprocessBoxShadow(const Value& in) {
    auto stringBox = in.toStringBox();
    static StringBox none = STRING_LITERAL("none");
    if (stringBox == none) {
        return Value::undefined();
    }

    AttributeParser parser(stringBox.toStringView());

    auto isComplex = parser.tryParse("complex");

    auto hOffset = parser.parseDimension();
    if (!hOffset) {
        return parseError(parser, "boxShadow hOffset");
    }

    auto vOffset = parser.parseDimension();
    if (!vOffset) {
        return parseError(parser, "boxShadow vOffset");
    }

    auto blur = parser.parseDimension();
    if (!blur) {
        return parseError(parser, "boxShadow blur");
    }

    auto color = parser.parseColorValue();
    if (!color) {
        return parseError(parser, "boxShadow color");
    }
    if (!parser.ensureIsAtEnd()) {
        return parser.getError();
    }

    auto boxShadow = ValueArray::make({Value(isComplex),
                                       Value(hOffset.value().value),
                                       Value(vOffset.value().value),
                                       Value(blur.value().value),
                                       color.value()});

    return Value(boxShadow);
}

Result<Value> preprocessTextShadow(const Value& in) {
    auto stringBox = in.toStringBox();
    static StringBox none = STRING_LITERAL("none");
    if (stringBox == none) {
        return Value::undefined();
    }

    AttributeParser parser(stringBox.toStringView());

    auto color = parser.parseColorValue();
    if (!color) {
        return parseError(parser, "Failed to parse text shadow color: ");
    }
    auto radius = parser.parseDouble();
    if (!radius) {
        return parseError(parser, "Failed to parse radius: ");
    }
    auto opacity = parser.parseDouble();
    if (!opacity) {
        return parseError(parser, "Failed to parse opacity: ");
    }
    auto hOffset = parser.parseDouble();
    if (!hOffset) {
        return parseError(parser, "Failed to parse hOffset: ");
    }
    auto vOffset = parser.parseDouble();
    if (!vOffset) {
        return parseError(parser, "Failed to parse vOffset: ");
    }

    auto isAtEnd = parser.ensureIsAtEnd();
    if (!isAtEnd) {
        return parser.getError();
    }

    const auto textShadow = ValueArray::make(
        {color.value(), Value(radius.value()), Value(opacity.value()), Value(hOffset.value()), Value(vOffset.value())});

    return Value(textShadow);
}

enum LinearGradientAngle {
    LinearGradientAngleTopBottom,
    LinearGradientAngleTopRightBottomLeft,
    LinearGradientAngleRightLeft,
    LinearGradientAngleBottomRightTopLeft,
    LinearGradientAngleBottomTop,
    LinearGradientAngleBottomLeftTopRight,
    LinearGradientAngleLeftRight,
    LinearGradientAngleTopLeftBottomRight,
};

static LinearGradientAngle angleRadToAngleEnum(double angleRad) {
    int32_t angleEnum = 0;

    while (angleRad >= M_PI_4 && angleEnum < 7) {
        angleRad -= M_PI_4;
        angleEnum++;
    }

    return static_cast<LinearGradientAngle>(angleEnum);
}

Result<Value> preprocessGradient(const Value& in) {
    auto stringBox = in.toStringBox();

    Ref<ValueArray> colorArray;
    Ref<ValueArray> locationArray;
    LinearGradientAngle angleEnum = LinearGradientAngleTopBottom;

    AttributeParser parser(stringBox.toStringView());

    auto isLinearGradient = parser.tryParse("linear-gradient(");
    auto isRadialGradient = !isLinearGradient && parser.tryParse("radial-gradient(");

    if (isLinearGradient || isRadialGradient) {
        ValueArrayBuilder colors;
        ValueArrayBuilder locations;

        parser.tryParseWhitespaces();

        auto shouldParseColorComponents = true;

        if (!isRadialGradient) {
            auto angleParser = parser;
            auto angleResult = angleParser.parseAngle();
            if (angleResult) {
                parser = angleParser;
                angleEnum = angleRadToAngleEnum(angleResult.value());

                parser.tryParseWhitespaces();

                if (!parser.tryParse(',')) {
                    shouldParseColorComponents = false;
                }
            }
        }

        if (shouldParseColorComponents) {
            while (!parser.isAtEnd()) {
                parser.tryParseWhitespaces();
                auto color = parser.parseColorValue();
                if (!color) {
                    return parseError(parser, "gradient color");
                }

                colors.emplace(color.value());

                parser.tryParseWhitespaces();

                if (!parser.isAtEnd() && parser.peekPredicate([](auto c) { return isdigit(c) != 0; })) {
                    auto locationResult = parser.parseDimension();
                    if (!locationResult) {
                        return parseError(parser, "gradient location");
                    }
                    auto location = locationResult.value();
                    if (location.unit == Dimension::Unit::Percent) {
                        // Convert percentages to decimal notation
                        location.value /= 100.0;
                    }
                    locations.emplace(location.value);

                    parser.tryParseWhitespaces();
                }

                if (!parser.tryParse(',')) {
                    break;
                }
            }
        }

        colorArray = colors.build();
        locationArray = locations.build();

        if (!locationArray->empty() && locationArray->size() != colorArray->size()) {
            return Error("Mismatched locations and colors for gradient");
        }

        if (!parser.parse(')')) {
            return parser.getError();
        }
    } else {
        parser.tryParseWhitespaces();

        auto singleColor = parser.parseColorValue();
        if (!singleColor) {
            return parser.getError();
        }

        colorArray = ValueArray::make({singleColor.value()});

        locationArray = ValueArray::make(0);
    }

    if (!parser.ensureIsAtEnd()) {
        return parser.getError();
    }

    const auto gradient =
        ValueArray::make({Value(colorArray), Value(locationArray), Value(angleEnum), Value(isRadialGradient)});

    return Value(gradient);
}

Result<Value> preprocessBorderRadius(const Value& in) {
    auto borderRadius = ValueConverter::toBorderValues(in);
    if (!borderRadius) {
        return borderRadius.moveError();
    }

    return Value(borderRadius.value());
}

static Result<Value> resolveColorValue(const ColorPalette& colorPalette, const Value& in) {
    return ValueConverter::toColor(colorPalette, in);
}

static Result<Value> resolveColorValue(ViewNode& viewNode, const Value& in) {
    if (!in.isString()) {
        return in;
    }

    const auto& colorPalette = viewNode.getResolvedColorPalette();
    if (colorPalette == nullptr) {
        return Error("ViewNode has no resolved ColorPalette");
    }
    return resolveColorValue(*colorPalette, in);
}

static Result<Value> resolveColorAtIndex(ViewNode& viewNode, const Value& in, size_t colorIndex) {
    if (!in.isArray()) {
        return in;
    }

    const auto* array = in.getArray();
    if (array->size() <= colorIndex || (*array)[colorIndex].isUndefined()) {
        return in;
    }

    auto resolvedColor = resolveColorValue(viewNode, (*array)[colorIndex]);
    if (!resolvedColor) {
        return resolvedColor.moveError();
    }

    auto out = array->clone();
    out->emplace(colorIndex, resolvedColor.moveValue());
    return Value(out);
}

static Result<Ref<ValueArray>> resolveColorAtIndexInArray(ViewNode& viewNode, const Value& in, size_t colorIndex) {
    const auto* array = in.getArray();
    if (array == nullptr) {
        return Error("Invalid array value");
    }

    auto out = array->clone();
    if (out->size() > colorIndex && !(*out)[colorIndex].isUndefined()) {
        auto resolvedColor = resolveColorValue(viewNode, (*out)[colorIndex]);
        if (!resolvedColor) {
            return resolvedColor.moveError();
        }
        out->emplace(colorIndex, resolvedColor.moveValue());
    }

    return out;
}

Result<Value> postprocessBorder(ViewNode& viewNode, const Value& in) {
    constexpr size_t kBorderColorIndex = 1;
    return resolveColorAtIndex(viewNode, in, kBorderColorIndex);
}

static Result<Value> postprocessBoxShadow(bool isRightToLeft, Ref<ValueArray> boxShadow) {
    if (boxShadow->size() != 5) {
        return Error("Invalid boxShadow value");
    }

    if (!isRightToLeft) {
        return Value(boxShadow);
    }

    constexpr size_t kHOffsetIndex = 1;

    auto hOffset = (*boxShadow)[kHOffsetIndex].toDouble();
    if (hOffset != 0.0) {
        boxShadow->emplace(kHOffsetIndex, Value(hOffset * -1));
    }

    return Value(boxShadow);
}

Result<Value> postprocessBoxShadow(ViewNode& viewNode, const Value& in) {
    constexpr size_t kBoxShadowColorIndex = 4;
    auto resolvedBoxShadow = resolveColorAtIndexInArray(viewNode, in, kBoxShadowColorIndex);
    if (!resolvedBoxShadow) {
        return resolvedBoxShadow.moveError();
    }

    return postprocessBoxShadow(viewNode.isRightToLeft(), resolvedBoxShadow.moveValue());
}

Result<Value> postprocessTextShadow(ViewNode& viewNode, const Value& in) {
    constexpr size_t kTextShadowColorIndex = 0;
    return resolveColorAtIndex(viewNode, in, kTextShadowColorIndex);
}

Result<Value> postprocessBoxShadow(bool isRightToLeft, const Value& in) {
    if (!isRightToLeft || !in.isArray()) {
        return in;
    }

    const auto* boxShadow = in.getArray();
    if (boxShadow->size() != 5) {
        return Error("Invalid boxShadow value");
    }

    constexpr size_t kHOffsetIndex = 1;

    auto hOffset = (*boxShadow)[kHOffsetIndex].toDouble();
    if (hOffset == 0.0) {
        // Nothing to do if the horizontal offset is empty
        return in;
    }

    auto flippedBoxShadow = boxShadow->clone();

    // Flip the hOffset of the boxShadow
    flippedBoxShadow->emplace(kHOffsetIndex, Value(hOffset * -1));

    return Value(flippedBoxShadow);
}

constexpr size_t kAngleIndex = 2;

static std::optional<LinearGradientAngle> flippedGradientAngle(LinearGradientAngle angle) {
    switch (angle) {
        case LinearGradientAngleTopBottom:
        case LinearGradientAngleBottomTop:
            return std::nullopt;
        case LinearGradientAngleTopRightBottomLeft:
            return LinearGradientAngleTopLeftBottomRight;
        case LinearGradientAngleRightLeft:
            return LinearGradientAngleLeftRight;
        case LinearGradientAngleBottomRightTopLeft:
            return LinearGradientAngleBottomLeftTopRight;
        case LinearGradientAngleBottomLeftTopRight:
            return LinearGradientAngleBottomRightTopLeft;
        case LinearGradientAngleLeftRight:
            return LinearGradientAngleRightLeft;
        case LinearGradientAngleTopLeftBottomRight:
            return LinearGradientAngleTopRightBottomLeft;
    }

    return std::nullopt;
}

static Result<Value> postprocessGradient(bool isRightToLeft, Ref<ValueArray> background) {
    if (background->size() != 4) {
        return Error("Invalid background value");
    }

    if (!isRightToLeft) {
        return Value(background);
    }

    auto angle = static_cast<LinearGradientAngle>((*background)[kAngleIndex].toInt());
    auto flippedAngle = flippedGradientAngle(angle);
    if (flippedAngle) {
        background->emplace(kAngleIndex, Value(flippedAngle.value()));
    }

    return Value(background);
}

Result<Value> postprocessGradient(ViewNode& viewNode, const Value& in) {
    if (!in.isArray()) {
        return in;
    }

    const auto* background = in.getArray();
    if (background->size() != 4) {
        return Error("Invalid background value");
    }

    constexpr size_t kColorsIndex = 0;
    const auto* colors = (*background)[kColorsIndex].getArray();
    if (colors == nullptr) {
        return Error("Invalid background colors value");
    }

    auto resolvedColors = colors->clone();
    for (size_t i = 0; i < colors->size(); ++i) {
        auto resolvedColor = resolveColorValue(viewNode, (*colors)[i]);
        if (!resolvedColor) {
            return resolvedColor.moveError();
        }
        resolvedColors->emplace(i, resolvedColor.moveValue());
    }

    auto resolvedBackground = background->clone();
    resolvedBackground->emplace(kColorsIndex, Value(resolvedColors));

    return postprocessGradient(viewNode.isRightToLeft(), std::move(resolvedBackground));
}

Result<Value> postprocessGradient(bool isRightToLeft, const ColorPalette& colorPalette, const Value& in) {
    if (!in.isArray()) {
        return in;
    }

    const auto* background = in.getArray();
    if (background->size() != 4) {
        return Error("Invalid background value");
    }

    constexpr size_t kColorsIndex = 0;
    const auto* colors = (*background)[kColorsIndex].getArray();
    if (colors == nullptr) {
        return Error("Invalid background colors value");
    }

    auto resolvedColors = colors->clone();
    for (size_t i = 0; i < colors->size(); ++i) {
        auto resolvedColor = resolveColorValue(colorPalette, (*colors)[i]);
        if (!resolvedColor) {
            return resolvedColor.moveError();
        }
        resolvedColors->emplace(i, resolvedColor.moveValue());
    }

    auto resolvedBackground = background->clone();
    resolvedBackground->emplace(kColorsIndex, Value(resolvedColors));

    return postprocessGradient(isRightToLeft, std::move(resolvedBackground));
}

Result<Value> postprocessGradient(bool isRightToLeft, const Value& in) {
    if (!isRightToLeft || !in.isArray()) {
        return in;
    }

    const auto* background = in.getArray();
    if (background->size() != 4) {
        return Error("Invalid background value");
    }

    auto angle = static_cast<LinearGradientAngle>((*background)[kAngleIndex].toInt());
    auto flippedAngle = flippedGradientAngle(angle);
    if (!flippedAngle) {
        return in;
    }

    auto flippedBackground = background->clone();
    flippedBackground->emplace(kAngleIndex, Value(flippedAngle.value()));
    return Value(flippedBackground);
}

Result<Value> postprocessBorderRadius(ViewNode& viewNode, const Value& in) {
    return postprocessBorderRadius(viewNode.isRightToLeft(), in);
}

Result<Value> postprocessBorderRadius(bool isRightToLeft, const Value& in) {
    auto borderRadiusResult = ValueConverter::toBorderValues(in);
    if (!borderRadiusResult) {
        return borderRadiusResult.moveError();
    }

    auto borderRadius = borderRadiusResult.moveValue();
    if (!isRightToLeft || borderRadius->areBordersEqual()) {
        // Nothing to do if the ViewNode is LTR or if all the borders are equal
        return Value(borderRadius);
    }

    // For RTL, we flip the borders horizontally

    auto out = makeShared<BorderRadius>();
    out->setTopLeft(borderRadius->getTopRight());
    out->setTopRight(borderRadius->getTopLeft());
    out->setBottomRight(borderRadius->getBottomLeft());
    out->setBottomLeft(borderRadius->getBottomRight());

    return Value(out);
}

void registerDefaultProcessors(AttributesManager& attributesManager) {
    auto borderAttributeId = attributesManager.getAttributeIds().getIdForName("border");
    auto boxShadowAttributeId = attributesManager.getAttributeIds().getIdForName("boxShadow");
    auto textShadowAttributeId = attributesManager.getAttributeIds().getIdForName("textShadow");
    auto backgroundAttributeId = attributesManager.getAttributeIds().getIdForName("background");
    auto borderRadiusAttributeId = attributesManager.getAttributeIds().getIdForName("borderRadius");
    auto textGradientAttributeId = attributesManager.getAttributeIds().getIdForName("textGradient");
    auto maskImageAttributeId = attributesManager.getAttributeIds().getIdForName("maskImage");

    attributesManager.registerPreprocessor(borderAttributeId, &preprocessBorder);
    attributesManager.registerPreprocessor(boxShadowAttributeId, &preprocessBoxShadow);
    attributesManager.registerPreprocessor(textShadowAttributeId, &preprocessTextShadow);
    attributesManager.registerPreprocessor(backgroundAttributeId, &preprocessGradient);
    attributesManager.registerPreprocessor(borderRadiusAttributeId, &preprocessBorderRadius);
    attributesManager.registerPreprocessor(textGradientAttributeId, &preprocessGradient);
    attributesManager.registerPreprocessor(maskImageAttributeId, &preprocessGradient);

    attributesManager.registerPostprocessor(borderAttributeId, &postprocessBorder);
    attributesManager.registerPostprocessor(boxShadowAttributeId, &postprocessBoxShadow);
    attributesManager.registerPostprocessor(textShadowAttributeId, &postprocessTextShadow);
    attributesManager.registerPostprocessor(backgroundAttributeId, &postprocessGradient);
    attributesManager.registerPostprocessor(borderRadiusAttributeId, &postprocessBorderRadius);
    attributesManager.registerPostprocessor(textGradientAttributeId, &postprocessGradient);
    attributesManager.registerPostprocessor(maskImageAttributeId, &postprocessGradient);
}

} // namespace Valdi
