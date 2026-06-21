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

namespace snap::drawing {

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
                                           fontManager);

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
    BIND_STRING_ATTRIBUTE(TextLayer, textOverflow, true);

    BIND_INT_ATTRIBUTE(TextLayer, numberOfLines, true);

    BIND_BOOLEAN_ATTRIBUTE(TextLayer, adjustsFontSizeToFitWidth, true);
    BIND_DOUBLE_ATTRIBUTE(TextLayer, minimumScaleFactor, true);

    BIND_DOUBLE_ATTRIBUTE(TextLayer, letterSpacing, true);

    BIND_DOUBLE_ATTRIBUTE(TextLayer, lineHeightMultiple, true);
    BIND_DOUBLE_ATTRIBUTE(TextLayer, lineHeight, true);

    BIND_UNTYPED_ATTRIBUTE(TextLayer, textShadow, true);

    BIND_UNTYPED_ATTRIBUTE(TextLayer, textGradient, false);

    REGISTER_PREPROCESSOR(font, true);
}

IMPLEMENT_TEXT_ATTRIBUTE(
    TextLayer,
    value,
    {
        if (value.isString()) {
            view.setText(value.toStringBox());
        } else if (value.isValdiObject()) {
            auto parseResult = AttributedTextParser::parse(*getResources()->getFontManager(), value);
            if (!parseResult) {
                return parseResult.moveError();
            }

            view.setAttributedText(parseResult.value());
        }

        return Valdi::Void();
    },
    { view.setText(Valdi::StringBox()); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    TextLayer,
    font,
    {
        auto font = Valdi::castOrNull<Font>(value.getValdiObject());

        view.setTextFont(font);

        return Valdi::Void();
    },
    { view.setTextFont(nullptr); })

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

IMPLEMENT_COLOR_ATTRIBUTE(
    TextLayer,
    color,
    {
        view.setTextColor(snapDrawingColorFromValdiColor(value));
        return Valdi::Void();
    },
    { view.setTextColor(Color::black()); })

IMPLEMENT_STRING_ATTRIBUTE(
    TextLayer,
    textAlign,
    {
        if (value == "left") {
            view.setTextAlign(TextAlignLeft);
        } else if (value == "right") {
            view.setTextAlign(TextAlignRight);
        } else if (value == "center") {
            view.setTextAlign(TextAlignCenter);
        } else if (value == "justified") {
            view.setTextAlign(TextAlignJustify);
        } else {
            return Valdi::Error("Invalid textAlign");
        }

        return Valdi::Void();
    },
    { view.setTextAlign(TextAlignLeft); })

IMPLEMENT_STRING_ATTRIBUTE(
    TextLayer,
    textDecoration,
    {
        if (value.isEmpty() || value == "none") {
            view.setTextDecoration(TextDecorationNone);
        } else if (value == "strikethrough") {
            view.setTextDecoration(TextDecorationStrikethrough);
        } else if (value == "underline") {
            view.setTextDecoration(TextDecorationUnderline);
        } else if (value == "dashed-underline") {
            view.setTextDecoration(TextDecorationDashedUnderline);
        } else if (value == "dotted-underline") {
            view.setTextDecoration(TextDecorationDottedUnderline);
        } else {
            return Valdi::Error("Invalid textDecoration");
        }

        return Valdi::Void();
    },
    { view.setTextDecoration(TextDecorationNone); })

IMPLEMENT_STRING_ATTRIBUTE(
    TextLayer,
    textOverflow,
    {
        if (value == "ellipsis") {
            view.setTextOverflow(TextOverflowEllipsis);
        } else if (value == "clip") {
            view.setTextOverflow(TextOverflowClip);
        } else {
            return Valdi::Error("Invalid textOverflow");
        }

        return Valdi::Void();
    },
    { view.setTextOverflow(TextOverflowEllipsis); })

IMPLEMENT_INT_ATTRIBUTE(
    TextLayer,
    numberOfLines,
    {
        view.setNumberOfLines(static_cast<int>(value));
        return Valdi::Void();
    },
    { view.setNumberOfLines(1); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    TextLayer,
    adjustsFontSizeToFitWidth,
    {
        view.setAdjustsFontSizeToFitWidth(value);
        return Valdi::Void();
    },
    { view.setAdjustsFontSizeToFitWidth(false); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    minimumScaleFactor,
    {
        view.setMinimumScaleFactor(value);
        return Valdi::Void();
    },
    { view.setMinimumScaleFactor(0.0); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    lineHeightMultiple,
    {
        view.setLineHeightMultiple(static_cast<Scalar>(value));
        return Valdi::Void();
    },
    { view.setLineHeightMultiple(1.0f); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    lineHeight,
    {
        view.setLineHeight(static_cast<Scalar>(value));
        return Valdi::Void();
    },
    { view.resetLineHeight(); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    TextLayer,
    letterSpacing,
    {
        view.setLetterSpacing(static_cast<Scalar>(value));
        return Valdi::Void();
    },
    { view.setLetterSpacing(0.0f); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    TextLayer,
    textShadow,
    {
        if (value.isNullOrUndefined()) {
            view.resetTextShadow();
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

        view.setTextShadow(
            skColor, static_cast<Scalar>(radius), opacity, static_cast<Scalar>(offsetX), static_cast<Scalar>(offsetY));

        return Valdi::Void();
    },
    { view.resetTextShadow(); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    TextLayer,
    textGradient,
    {
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
            view.setTextRadialGradient(std::move(outLocations), std::move(outColors));
        } else {
            view.setTextLinearGradient(std::move(outLocations), std::move(outColors), orientation);
        }

        return Valdi::Void();
    },
    { view.resetTextGradient(); })

} // namespace snap::drawing

// namespace snap::drawing
