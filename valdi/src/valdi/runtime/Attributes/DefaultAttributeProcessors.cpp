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

namespace Valdi {

static Error parseError(AttributeParser& parser, std::string_view name) {
    parser.prependError(fmt::format("Failed to parse {}", name));
    return parser.getError();
}

Result<Value> preprocessBorder(const Ref<ColorPalette>& colorPalette, const Value& in) {
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

        auto color = parser.parseColor(*colorPalette);
        if (!color) {
            return parseError(parser, "border color");
        }

        parser.tryParseWhitespaces();

        if (!parser.ensureIsAtEnd()) {
            return parser.getError();
        }

        border = ValueArray::make({Value(borderWidth.value().value), Value(color.value().value)});

    } else {
        border = ValueArray::make({Value(borderWidth.value().value)});
    }

    return Value(border);
}

Result<Value> preprocessBoxShadow(const Ref<ColorPalette>& colorPalette, const Value& in) {
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

    auto color = parser.parseColor(*colorPalette);
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
                                       Value(color.value().value)});

    return Value(boxShadow);
}

Result<Value> preprocessTextShadow(const Ref<ColorPalette>& colorPalette, const Value& in) {
    auto stringBox = in.toStringBox();
    static StringBox none = STRING_LITERAL("none");
    if (stringBox == none) {
        return Value::undefined();
    }

    AttributeParser parser(stringBox.toStringView());

    auto color = parser.parseColor(*colorPalette);
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

    const auto textShadow = ValueArray::make({Value(color.value().value),
                                              Value(radius.value()),
                                              Value(opacity.value()),
                                              Value(hOffset.value()),
                                              Value(vOffset.value())});

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

Result<Value> preprocessGradient(const Ref<ColorPalette>& colorPalette, const Value& in) {
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
                auto color = parser.parseColor(*colorPalette);
                if (!color) {
                    return parseError(parser, "gradient color");
                }

                colors.emplace(color.value().value);

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

        auto singleColor = parser.parseColor(*colorPalette);
        if (!singleColor) {
            return parser.getError();
        }

        colorArray = ValueArray::make({Value(singleColor.value().value)});

        locationArray = ValueArray::make(0);
    }

    if (!parser.ensureIsAtEnd()) {
        return parser.getError();
    }

    const auto gradient =
        ValueArray::make({Value(colorArray), Value(locationArray), Value(angleEnum), Value(isRadialGradient)});

    return Value(gradient);
}

Result<Value> preprocessBorderRadius(const Ref<ColorPalette>& /*colorPalette*/, const Value& in) {
    auto borderRadius = ValueConverter::toBorderValues(in);
    if (!borderRadius) {
        return borderRadius.moveError();
    }

    return Value(borderRadius.value());
}

Result<Value> postprocessBoxShadow(ViewNode& viewNode, const Value& in) {
    return postprocessBoxShadow(viewNode.isRightToLeft(), in);
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

Result<Value> makeBackgroundWithAngle(const ValueArray* background, LinearGradientAngle angle) {
    auto newBackground = background->clone();
    newBackground->emplace(kAngleIndex, Value(angle));
    return Value(newBackground);
}

Result<Value> postprocessGradient(ViewNode& viewNode, const Value& in) {
    return postprocessGradient(viewNode.isRightToLeft(), in);
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

    switch (angle) {
        case LinearGradientAngleTopBottom:
            return in;
        case LinearGradientAngleTopRightBottomLeft:
            return makeBackgroundWithAngle(background, LinearGradientAngleTopLeftBottomRight);
        case LinearGradientAngleRightLeft:
            return makeBackgroundWithAngle(background, LinearGradientAngleLeftRight);
        case LinearGradientAngleBottomRightTopLeft:
            return makeBackgroundWithAngle(background, LinearGradientAngleBottomLeftTopRight);
        case LinearGradientAngleBottomTop:
            return in;
        case LinearGradientAngleBottomLeftTopRight:
            return makeBackgroundWithAngle(background, LinearGradientAngleBottomRightTopLeft);
        case LinearGradientAngleLeftRight:
            return makeBackgroundWithAngle(background, LinearGradientAngleRightLeft);
        case LinearGradientAngleTopLeftBottomRight:
            return makeBackgroundWithAngle(background, LinearGradientAngleTopRightBottomLeft);
    }

    return in;
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
    auto fillGradientAttributeId = attributesManager.getAttributeIds().getIdForName("fillGradient");

    attributesManager.registerPreprocessor(borderAttributeId, &preprocessBorder);
    attributesManager.registerPreprocessor(boxShadowAttributeId, &preprocessBoxShadow);
    attributesManager.registerPreprocessor(textShadowAttributeId, &preprocessTextShadow);
    attributesManager.registerPreprocessor(backgroundAttributeId, &preprocessGradient);
    attributesManager.registerPreprocessor(borderRadiusAttributeId, &preprocessBorderRadius);
    attributesManager.registerPreprocessor(textGradientAttributeId, &preprocessGradient);
    attributesManager.registerPreprocessor(maskImageAttributeId, &preprocessGradient);
    attributesManager.registerPreprocessor(fillGradientAttributeId, &preprocessGradient);

    attributesManager.registerPostprocessor(boxShadowAttributeId, &postprocessBoxShadow);
    attributesManager.registerPostprocessor(backgroundAttributeId, &postprocessGradient);
    attributesManager.registerPostprocessor(borderRadiusAttributeId, &postprocessBorderRadius);
    attributesManager.registerPostprocessor(textGradientAttributeId, &postprocessGradient);
    attributesManager.registerPostprocessor(maskImageAttributeId, &postprocessGradient);
    attributesManager.registerPostprocessor(fillGradientAttributeId, &postprocessGradient);
}

} // namespace Valdi
