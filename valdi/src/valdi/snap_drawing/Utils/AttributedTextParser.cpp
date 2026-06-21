//
//  AttributedTextParser.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#include "valdi/snap_drawing/Utils/AttributedTextParser.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace snap::drawing {

class AttributedTextOnTapAttributeWithValueFunction : public AttributedTextOnTapAttribute {
public:
    explicit AttributedTextOnTapAttributeWithValueFunction(TapGestureRecognizer::Listener&& listener)
        : _listener(std::move(listener)) {}

    ~AttributedTextOnTapAttributeWithValueFunction() override = default;

    void onTap(const GestureRecognizer& gestureRecognizer,
               GestureRecognizerState state,
               const Point& location) override {
        _listener(dynamic_cast<const TapGestureRecognizer&>(gestureRecognizer), state, location);
    }

private:
    TapGestureRecognizer::Listener _listener;
};

static std::optional<TextDecoration> parseTextDecoration(const Valdi::TextDecoration& textDecoration) {
    switch (textDecoration) {
        case Valdi::TextDecoration::Unset:
            return std::nullopt;
        case Valdi::TextDecoration::None:
            return {TextDecorationNone};
        case Valdi::TextDecoration::Underline:
            return {TextDecorationUnderline};
        case Valdi::TextDecoration::DashedUnderline:
            return {TextDecorationDashedUnderline};
        case Valdi::TextDecoration::DottedUnderline:
            return {TextDecorationDottedUnderline};
        case Valdi::TextDecoration::Strikethrough:
            return {TextDecorationStrikethrough};
    }
}

static TextBackgroundPadding snapDrawingPaddingFromValdiPadding(const Valdi::TextBackgroundPadding& padding) {
    return TextBackgroundPadding{padding.left, padding.top, padding.right, padding.bottom};
}

static BorderRadius snapDrawingBorderRadiusFromValdiDimension(const Valdi::Dimension& dimension) {
    return BorderRadius::makeOval(static_cast<Scalar>(dimension.value),
                                  dimension.unit == Valdi::Dimension::Unit::Percent);
}

Valdi::Result<Valdi::Ref<AttributedText>> AttributedTextParser::parse(FontManager& fontManager,
                                                                      const Valdi::Value& value) {
    auto textAttributeValue = value.getTypedRef<Valdi::TextAttributeValue>();
    if (textAttributeValue == nullptr) {
        return Valdi::Error(STRING_FORMAT("Value '{}' is not an AttributedText object", value.toString()));
    }

    AttributedText::Parts parts;

    auto length = textAttributeValue->getPartsSize();
    parts.reserve(length);

    for (size_t i = 0; i < length; i++) {
        auto& part = parts.emplace_back();

        const auto& style = textAttributeValue->getStyleAtIndex(i);

        part.content = textAttributeValue->getContentAtIndex(i);

        if (style.font) {
            auto fontResult = fontManager.getFontForName(style.font.value(), 1.0);
            if (!fontResult) {
                return fontResult.moveError();
            }
            part.style.font = {fontResult.moveValue()};
        }

        if (style.color) {
            part.style.color = {snapDrawingColorFromValdiColor(style.color.value().value)};
        }

        if (style.background != nullptr) {
            if (style.background->color) {
                part.style.backgroundColor = {snapDrawingColorFromValdiColor(style.background->color.value().value)};
            }
            part.style.backgroundPadding = snapDrawingPaddingFromValdiPadding(style.background->padding);
            part.style.backgroundBorderRadius =
                snapDrawingBorderRadiusFromValdiDimension(style.background->borderRadius);
        }

        if (style.onTap != nullptr) {
            part.style.onTap = Valdi::makeShared<AttributedTextOnTapAttributeWithValueFunction>(
                LayerClass::makeTapGestureListener(style.onTap));
        }

        part.style.inlineViewAttachment = style.inlineViewAttachment;
        part.style.animationTransform = style.animationTransform;
        part.style.textDecoration = parseTextDecoration(style.textDecoration);
    }

    return Valdi::makeShared<AttributedText>(std::move(parts));
}

} // namespace snap::drawing
