//
//  TextLayerClass.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#include "valdi/snap_drawing/Layers/Classes/TextLayerClass.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "valdi/snap_drawing/Utils/AttributedTextParser.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include <array>
#include <cmath>

namespace snap::drawing {

class TextCustomUnderlineStyleValue : public Valdi::ValdiObject {
public:
    explicit TextCustomUnderlineStyleValue(TextCustomUnderlineStyle style) : style(style) {}

    TextCustomUnderlineStyle style;

    VALDI_CLASS_HEADER_IMPL(TextCustomUnderlineStyleValue)
};

static Valdi::Result<TextCustomUnderlineStyle> parseTextCustomUnderlineStyle(const Valdi::StringBox& styleString) {
    Valdi::TextParser parser(styleString.toStringView());
    auto height = parser.parseDouble();
    if (!height) {
        return parser.getError();
    }

    auto onWidth = parser.parseDouble();
    if (!onWidth) {
        return parser.getError();
    }

    auto offWidth = parser.parseDouble();
    if (!offWidth) {
        return parser.getError();
    }

    auto offset = parser.parseDouble();
    if (!offset) {
        return parser.getError();
    }

    parser.tryParseWhitespaces();
    if (!parser.ensureIsAtEnd()) {
        return parser.getError();
    }

    std::array<double, 4> values = {height.value(), onWidth.value(), offWidth.value(), offset.value()};
    for (auto value : values) {
        if (!std::isfinite(value)) {
            return Valdi::Error("customUnderlineStyle values must be finite numbers");
        }
    }

    if (values[0] <= 0) {
        return Valdi::Error("customUnderlineStyle height must be positive");
    }

    auto solid = values[1] == 0 && values[2] == 0;
    auto patterned = values[1] > 0 && values[2] > 0;
    if (!solid && !patterned) {
        return Valdi::Error("customUnderlineStyle onWidth and offWidth must both be positive, or both be 0");
    }

    return TextCustomUnderlineStyle(static_cast<Scalar>(values[0]),
                                    static_cast<Scalar>(values[1]),
                                    static_cast<Scalar>(values[2]),
                                    static_cast<Scalar>(values[3]));
}

TextLayerClass::TextLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass)
    : ILayerClass(resources, "SCValdiLabel", "com.snap.valdi.views.ValdiTextView", parentClass, true) {}

TextLayerClass::~TextLayerClass() = default;

Valdi::Ref<Layer> TextLayerClass::instantiate() {
    return snap::drawing::makeLayer<snap::drawing::TextLayer>(getResources());
}

Size TextLayerClass::onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft) {
    auto text = attributes.getMapValue("value");
    auto font = Valdi::castOrNull<Font>(attributes.getMapValue("font").getValdiObject());
    auto numberOfLines = attributes.getMapValue("numberOfLines");
    auto lineHeightMultiple = attributes.getMapValue("lineHeightMultiple");
    auto lineHeight = attributes.getMapValue("lineHeight");
    auto letterSpacing = attributes.getMapValue("letterSpacing");
    auto adjustsFontSizeToFitWidth = attributes.getMapValue("adjustsFontSizeToFitWidth");
    auto minimumScaleFactor = attributes.getMapValue("minimumScaleFactor");
    auto textOverflowStr = attributes.getMapValue("textOverflow");

    auto respectDynamicType = getResources()->getRespectDynamicType();
    auto displayScale = getResources()->getDisplayScale();
    auto dynamicTypeScale = getResources()->getDynamicTypeScale();
    const auto& fontManager = getResources()->getFontManager();

    String resolvedText;
    Ref<AttributedText> resolvedAttributedText;

    if (text.isString()) {
        resolvedText = text.toStringBox();
    } else if (text.isValdiObject()) {
        auto attributedText = AttributedTextParser::parse(*fontManager, text);
        if (attributedText) {
            resolvedAttributedText = attributedText.moveValue();
        } else {
            VALDI_ERROR(getResources()->getLogger(),
                        "Failed to parse attributed text: {}",
                        attributedText.error().getMessage());
        }
    }

    auto resolvedNumberOfLines = numberOfLines.isNumber() ? numberOfLines.toInt() : 1;
    auto resolvedLineHeight = lineHeightMultiple.isNumber()
        ? TextLayoutLineHeight::multiple(static_cast<Scalar>(lineHeightMultiple.toDouble()))
        : TextLayoutLineHeight::multiple(1.0f);
    if (lineHeight.isNumber()) {
        resolvedLineHeight = TextLayoutLineHeight::absolute(static_cast<Scalar>(lineHeight.toDouble() * displayScale));
    }
    auto resolvedLetterSpacing = letterSpacing.isNumber() ? letterSpacing.toDouble() : 0.0;

    auto scaledMaxSize = Size::make(maxSize.width * displayScale, maxSize.height * displayScale);
    auto textOverflow = TextOverflowEllipsis;

    if (textOverflowStr.isString() && textOverflowStr.toStringBox() == "clip") {
        textOverflow = TextOverflowClip;
    }

    auto textSize = TextLayer::measureText(scaledMaxSize,
                                           resolvedText,
                                           resolvedAttributedText,
                                           font,
                                           TextAlignLeft,
                                           TextDecorationNone,
                                           textOverflow,
                                           resolvedNumberOfLines,
                                           resolvedLineHeight,
                                           static_cast<Scalar>(resolvedLetterSpacing),
                                           false,
                                           adjustsFontSizeToFitWidth.toBool(),
                                           minimumScaleFactor.toDouble(),
                                           respectDynamicType,
                                           displayScale,
                                           dynamicTypeScale,
                                           fontManager,
                                           std::nullopt);

    return Size::make(textSize.width / displayScale, textSize.height / displayScale);
}

void TextLayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {
    std::vector<snap::valdi_core::CompositeAttributePart> parts;
    parts.emplace_back(STRING_LITERAL("fontSize"), snap::valdi_core::AttributeType::Double, true, true);

    BIND_TEXT_ATTRIBUTE(TextLayer, value, true);
    BIND_COMPOSITE_ATTRIBUTE(TextLayer, font, parts);
    BIND_COLOR_ATTRIBUTE(TextLayer, color, false);

    BIND_STRING_ATTRIBUTE(TextLayer, textAlign, false);
    BIND_STRING_ATTRIBUTE(TextLayer, textDecoration, false);
    BIND_UNTYPED_ATTRIBUTE(TextLayer, customUnderlineStyle, false);
    BIND_STRING_ATTRIBUTE(TextLayer, textOverflow, true);

    BIND_INT_ATTRIBUTE(TextLayer, numberOfLines, true);

    BIND_BOOLEAN_ATTRIBUTE(TextLayer, adjustsFontSizeToFitWidth, true);
    BIND_DOUBLE_ATTRIBUTE(TextLayer, minimumScaleFactor, true);

    BIND_DOUBLE_ATTRIBUTE(TextLayer, letterSpacing, true);

    BIND_DOUBLE_ATTRIBUTE(TextLayer, lineHeightMultiple, true);
    BIND_DOUBLE_ATTRIBUTE(TextLayer, lineHeight, true);

    BIND_UNTYPED_ATTRIBUTE(TextLayer, textShadow, true);

    BIND_UNTYPED_ATTRIBUTE(TextLayer, textGradient, false);

    BIND_BOOLEAN_ATTRIBUTE(TextLayer, selectable, false);
    BIND_UNTYPED_ATTRIBUTE(TextLayer, selection, false);
    BIND_FUNCTION_ATTRIBUTE(TextLayer, onSelectionChange);

    REGISTER_PREPROCESSOR(font, true);
    REGISTER_PREPROCESSOR(customUnderlineStyle, true);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyTextAttribute(TextLayer& textLayer, const Valdi::Value& value) {
    if (value.isString()) {
        textLayer.setText(value.toStringBox());
    } else if (value.isValdiObject()) {
        auto parseResult = AttributedTextParser::parse(*getResources()->getFontManager(), value);
        if (!parseResult) {
            return parseResult.moveError();
        }

        textLayer.setAttributedText(parseResult.value());
    }

    return Valdi::Void();
}

void TextLayerClass::resetTextAttribute(TextLayer& textLayer) {
    textLayer.setText(Valdi::StringBox());
}

Valdi::Result<Valdi::Void> TextLayerClass::applyFontAttribute(TextLayer& textLayer, const Valdi::Value& value) {
    auto font = Valdi::castOrNull<Font>(value.getValdiObject());

    textLayer.setTextFont(font);

    return Valdi::Void();
}

void TextLayerClass::resetFontAttribute(TextLayer& textLayer) {
    textLayer.setTextFont(nullptr);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyColorAttribute(TextLayer& textLayer, int64_t value) {
    textLayer.setTextColor(snapDrawingColorFromValdiColor(value));
    return Valdi::Void();
}

void TextLayerClass::resetColorAttribute(TextLayer& textLayer) {
    textLayer.setTextColor(Color::black());
}

Valdi::Result<Valdi::Void> TextLayerClass::applyTextAlignAttribute(TextLayer& textLayer, const String& value) {
    if (value == "left") {
        textLayer.setTextAlign(TextAlignLeft);
    } else if (value == "right") {
        textLayer.setTextAlign(TextAlignRight);
    } else if (value == "center") {
        textLayer.setTextAlign(TextAlignCenter);
    } else if (value == "justified") {
        textLayer.setTextAlign(TextAlignJustify);
    } else {
        return Valdi::Error("Invalid textAlign");
    }

    return Valdi::Void();
}

void TextLayerClass::resetTextAlignAttribute(TextLayer& textLayer) {
    textLayer.setTextAlign(TextAlignLeft);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyTextDecorationAttribute(TextLayer& textLayer, const String& value) {
    if (value.isEmpty() || value == "none") {
        textLayer.setTextDecoration(TextDecorationNone);
    } else if (value == "strikethrough") {
        textLayer.setTextDecoration(TextDecorationStrikethrough);
    } else if (value == "underline") {
        textLayer.setTextDecoration(TextDecorationUnderline);
    } else if (value == "dashed-underline") {
        textLayer.setTextDecoration(TextDecorationDashedUnderline);
    } else if (value == "dotted-underline") {
        textLayer.setTextDecoration(TextDecorationDottedUnderline);
    } else {
        return Valdi::Error("Invalid textDecoration");
    }

    return Valdi::Void();
}

void TextLayerClass::resetTextDecorationAttribute(TextLayer& textLayer) {
    textLayer.setTextDecoration(TextDecorationNone);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyCustomUnderlineStyleAttribute(TextLayer& textLayer,
                                                                              const Valdi::Value& value) {
    auto styleValue = Valdi::castOrNull<TextCustomUnderlineStyleValue>(value.getValdiObject());
    if (styleValue == nullptr) {
        return Valdi::Error("Invalid customUnderlineStyle");
    }

    textLayer.setCustomUnderlineStyle(styleValue->style);
    return Valdi::Void();
}

void TextLayerClass::resetCustomUnderlineStyleAttribute(TextLayer& textLayer) {
    textLayer.setCustomUnderlineStyle(std::nullopt);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyTextOverflowAttribute(TextLayer& textLayer, const String& value) {
    if (value == "ellipsis") {
        textLayer.setTextOverflow(TextOverflowEllipsis);
    } else if (value == "clip") {
        textLayer.setTextOverflow(TextOverflowClip);
    } else {
        return Valdi::Error("Invalid textOverflow");
    }

    return Valdi::Void();
}

void TextLayerClass::resetTextOverflowAttribute(TextLayer& textLayer) {
    textLayer.setTextOverflow(TextOverflowEllipsis);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyNumberOfLinesAttribute(TextLayer& textLayer, int64_t value) {
    textLayer.setNumberOfLines(static_cast<int>(value));
    return Valdi::Void();
}

void TextLayerClass::resetNumberOfLinesAttribute(TextLayer& textLayer) {
    textLayer.setNumberOfLines(1);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyAdjustsFontSizeToFitWidthAttribute(TextLayer& textLayer, bool value) {
    textLayer.setAdjustsFontSizeToFitWidth(value);
    return Valdi::Void();
}

void TextLayerClass::resetAdjustsFontSizeToFitWidthAttribute(TextLayer& textLayer) {
    textLayer.setAdjustsFontSizeToFitWidth(false);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyMinimumScaleFactorAttribute(TextLayer& textLayer, double value) {
    textLayer.setMinimumScaleFactor(value);
    return Valdi::Void();
}

void TextLayerClass::resetMinimumScaleFactorAttribute(TextLayer& textLayer) {
    textLayer.setMinimumScaleFactor(0.0);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyLineHeightMultipleAttribute(TextLayer& textLayer, double value) {
    textLayer.setLineHeightMultiple(static_cast<Scalar>(value));
    return Valdi::Void();
}

void TextLayerClass::resetLineHeightMultipleAttribute(TextLayer& textLayer) {
    textLayer.setLineHeightMultiple(1.0f);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyLineHeightAttribute(TextLayer& textLayer, double value) {
    textLayer.setLineHeight(static_cast<Scalar>(value));
    return Valdi::Void();
}

void TextLayerClass::resetLineHeightAttribute(TextLayer& textLayer) {
    textLayer.resetLineHeight();
}

Valdi::Result<Valdi::Void> TextLayerClass::applyLetterSpacingAttribute(TextLayer& textLayer, double value) {
    textLayer.setLetterSpacing(static_cast<Scalar>(value));
    return Valdi::Void();
}

void TextLayerClass::resetLetterSpacingAttribute(TextLayer& textLayer) {
    textLayer.setLetterSpacing(0.0f);
}

Valdi::Result<Valdi::Void> TextLayerClass::applyTextShadowAttribute(TextLayer& textLayer, const Valdi::Value& value) {
    if (value.isNullOrUndefined()) {
        textLayer.resetTextShadow();
        return Valdi::Void();
    }

    const auto* entries = value.getArray();
    if (entries == nullptr || entries->size() < 5) {
        return Valdi::Error("textShadow components should have 5 entries");
    }

    auto displayScale = getResources()->getDisplayScale();

    auto color = (*entries)[0].toLong();
    auto radius = (*entries)[1].toDouble() * displayScale;
    auto opacity = (*entries)[2].toDouble();
    auto offsetX = (*entries)[3].toDouble() * displayScale;
    auto offsetY = (*entries)[4].toDouble() * displayScale;

    auto skColor = snapDrawingColorFromValdiColor(color);

    textLayer.setTextShadow(
        skColor, static_cast<Scalar>(radius), opacity, static_cast<Scalar>(offsetX), static_cast<Scalar>(offsetY));

    return Valdi::Void();
}

void TextLayerClass::resetTextShadowAttribute(TextLayer& textLayer) {
    textLayer.resetTextShadow();
}

Valdi::Result<Valdi::Void> TextLayerClass::applyTextGradientAttribute(TextLayer& textLayer, const Valdi::Value& value) {
    const auto* array = value.getArray();
    if (array == nullptr || array->size() != 4) {
        return Valdi::Error("Expecting 4 values from textGradient");
    }

    const auto* colors = (*array)[0].getArray();
    const auto* locations = (*array)[1].getArray();
    auto orientation = static_cast<snap::drawing::LinearGradientOrientation>((*array)[2].toInt());
    auto radial = (*array)[3].toBool();

    if (colors == nullptr || locations == nullptr) {
        return Valdi::Error("Expecting 2 arrays: colors and locations");
    }

    std::vector<Color> outColors;
    outColors.reserve(colors->size());

    for (const auto& color : *colors) {
        outColors.emplace_back(snapDrawingColorFromValdiColor(color.toLong()));
    }

    std::vector<Scalar> outLocations;
    outLocations.reserve(locations->size());

    for (const auto& location : *locations) {
        outLocations.emplace_back(static_cast<Scalar>(location.toDouble()));
    }

    if (radial) {
        textLayer.setTextRadialGradient(std::move(outLocations), std::move(outColors));
    } else {
        textLayer.setTextLinearGradient(std::move(outLocations), std::move(outColors), orientation);
    }

    return Valdi::Void();
}

void TextLayerClass::resetTextGradientAttribute(TextLayer& textLayer) {
    textLayer.resetTextGradient();
}

IMPLEMENT_TEXT_ATTRIBUTE(TextLayer, value, { return applyTextAttribute(view, value); }, { resetTextAttribute(view); })

IMPLEMENT_UNTYPED_ATTRIBUTE(TextLayer, font, { return applyFontAttribute(view, value); }, { resetFontAttribute(view); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    TextLayer,
    customUnderlineStyle,
    { return applyCustomUnderlineStyleAttribute(view, value); },
    { resetCustomUnderlineStyleAttribute(view); })

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Value> TextLayerClass::preprocess_font(const Valdi::Value& value) {
    auto displayScale = getResources()->getDisplayScale();
    auto fontManager = getResources()->getFontManager();
    if (fontManager == nullptr) {
        return Valdi::Error("No font manager loaded");
    }

    if (value.isString()) {
        auto result = fontManager->getFontForName(value.toStringBox(), displayScale);
        if (!result) {
            return result.moveError();
        }
        return Valdi::Value(result.value());
    }

    if (value.isArray()) {
        auto result = fontManager->getDefaultFont(static_cast<Scalar>((*value.getArray())[0].toDouble()), displayScale);
        if (!result) {
            return result.moveError();
        }

        return Valdi::Value(result.value());
    }

    return Valdi::Value();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Value> TextLayerClass::preprocess_customUnderlineStyle(const Valdi::Value& value) {
    if (!value.isString()) {
        return Valdi::Error("customUnderlineStyle must be a string");
    }

    auto style = parseTextCustomUnderlineStyle(value.toStringBox());
    if (!style) {
        return style.moveError();
    }

    return Valdi::Value(Valdi::makeShared<TextCustomUnderlineStyleValue>(style.moveValue()));
}

IMPLEMENT_COLOR_ATTRIBUTE(
    TextLayer, color, { return applyColorAttribute(view, value); }, { resetColorAttribute(view); })

IMPLEMENT_STRING_ATTRIBUTE(
    TextLayer, textAlign, { return applyTextAlignAttribute(view, value); }, { resetTextAlignAttribute(view); })

IMPLEMENT_STRING_ATTRIBUTE(
    TextLayer,
    textDecoration,
    { return applyTextDecorationAttribute(view, value); },
    { resetTextDecorationAttribute(view); })

IMPLEMENT_STRING_ATTRIBUTE(
    TextLayer, textOverflow, { return applyTextOverflowAttribute(view, value); }, { resetTextOverflowAttribute(view); })

IMPLEMENT_INT_ATTRIBUTE(
    TextLayer,
    numberOfLines,
    { return applyNumberOfLinesAttribute(view, value); },
    { resetNumberOfLinesAttribute(view); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    TextLayer,
    adjustsFontSizeToFitWidth,
    { return applyAdjustsFontSizeToFitWidthAttribute(view, value); },
    { resetAdjustsFontSizeToFitWidthAttribute(view); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    minimumScaleFactor,
    { return applyMinimumScaleFactorAttribute(view, value); },
    { resetMinimumScaleFactorAttribute(view); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    lineHeightMultiple,
    { return applyLineHeightMultipleAttribute(view, value); },
    { resetLineHeightMultipleAttribute(view); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer, lineHeight, { return applyLineHeightAttribute(view, value); }, { resetLineHeightAttribute(view); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    letterSpacing,
    { return applyLetterSpacingAttribute(view, value); },
    { resetLetterSpacingAttribute(view); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    TextLayer, textShadow, { return applyTextShadowAttribute(view, value); }, { resetTextShadowAttribute(view); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    TextLayer, textGradient, { return applyTextGradientAttribute(view, value); }, { resetTextGradientAttribute(view); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(TextLayer, selectable, { return Valdi::Void(); }, {})

IMPLEMENT_UNTYPED_ATTRIBUTE(TextLayer, selection, { return Valdi::Void(); }, {})

Valdi::Result<Valdi::Void> TextLayerClass::apply_onSelectionChange(
    TextLayer& /*textLayer*/,
    const Valdi::Ref<Valdi::ValueFunction>& /*value*/,
    const AttributeContext& /*context*/) {
    return Valdi::Void();
}

void TextLayerClass::reset_onSelectionChange(TextLayer& /*textLayer*/, const AttributeContext& /*context*/) {}

} // namespace snap::drawing

// namespace snap::drawing
