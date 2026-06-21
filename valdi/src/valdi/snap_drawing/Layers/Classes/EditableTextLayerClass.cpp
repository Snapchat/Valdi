//
//  EditableTextLayerClass.cpp
//  valdi-desktop-apple
//

#include "valdi/snap_drawing/Layers/Classes/EditableTextLayerClass.hpp"

#include "snap_drawing/cpp/Resources.hpp"
#include "valdi/snap_drawing/Utils/AttributedTextParser.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

namespace snap::drawing {

static constexpr Color kDefaultPlaceholderColor = Color::makeARGB(0xFF, 0xB3, 0xB3, 0xB3);
static constexpr const char* kTextFieldIOSClassName = "SCValdiTextField";
static constexpr const char* kTextFieldAndroidClassName = "com.snap.valdi.views.ValdiEditText";
static constexpr const char* kTextViewIOSClassName = "SCValdiTextView";
static constexpr const char* kTextViewAndroidClassName = "com.snap.valdi.views.ValdiEditTextMultiline";
static constexpr int kTextFieldDefaultNumberOfLines = 1;
static constexpr int kTextViewDefaultNumberOfLines = 0;

static bool textValueHasContent(const Valdi::Value& value) {
    if (value.isString()) {
        return !value.toStringBox().isEmpty();
    }

    if (value.isValdiObject()) {
        auto textAttributeValue = value.getTypedRef<Valdi::TextAttributeValue>();
        if (textAttributeValue == nullptr) {
            return true;
        }

        return !textAttributeValue->isContentEmpty();
    }

    return false;
}

Ref<EditableTextLayerClass> EditableTextLayerClass::makeForTextField(const Ref<Resources>& resources,
                                                                     const Ref<TextLayerClass>& parentClass) {
    return Valdi::makeShared<EditableTextLayerClass>(resources,
                                                     parentClass,
                                                     kTextFieldIOSClassName,
                                                     kTextFieldAndroidClassName,
                                                     kTextFieldDefaultNumberOfLines,
                                                     false,
                                                     TextVerticalAlignmentCenter);
}

Ref<EditableTextLayerClass> EditableTextLayerClass::makeForTextView(const Ref<Resources>& resources,
                                                                    const Ref<TextLayerClass>& parentClass) {
    return Valdi::makeShared<EditableTextLayerClass>(resources,
                                                     parentClass,
                                                     kTextViewIOSClassName,
                                                     kTextViewAndroidClassName,
                                                     kTextViewDefaultNumberOfLines,
                                                     true,
                                                     TextVerticalAlignmentTop);
}

EditableTextLayerClass::EditableTextLayerClass(const Ref<Resources>& resources,
                                               const Ref<TextLayerClass>& parentClass,
                                               const char* iosClassName,
                                               const char* androidClassName,
                                               int defaultNumberOfLines,
                                               bool managesChildFrames,
                                               TextVerticalAlignment textVerticalAlignment)
    : ILayerClass(resources, iosClassName, androidClassName, parentClass, true),
      _textLayerClass(parentClass),
      _defaultNumberOfLines(defaultNumberOfLines),
      _managesChildFrames(managesChildFrames),
      _textVerticalAlignment(textVerticalAlignment) {}

EditableTextLayerClass::~EditableTextLayerClass() = default;

bool EditableTextLayerClass::managesChildFrames() const {
    return _managesChildFrames;
}

Valdi::Ref<Layer> EditableTextLayerClass::instantiate() {
    auto layer = snap::drawing::makeLayer<snap::drawing::EditableTextLayer>(getResources());
    layer->getTextLayer().setNumberOfLines(_defaultNumberOfLines);
    layer->getTextLayer().setTextVerticalAlignment(_textVerticalAlignment);
    return layer;
}

Size EditableTextLayerClass::onMeasure(const Valdi::Value& attributes,
                                       Size maxSize,
                                       bool isRightToLeft) {
    auto value = attributes.getMapValue("value");
    auto placeholder = attributes.getMapValue("placeholder");
    auto numberOfLines = attributes.getMapValue("numberOfLines");
    auto valueHasContent = textValueHasContent(value);

    if (valueHasContent && numberOfLines.isNumber()) {
        return _textLayerClass->onMeasure(attributes, maxSize, isRightToLeft);
    }

    auto measureAttributes = attributes;

    if (!valueHasContent && placeholder.isString()) {
        measureAttributes.setMapValue(STRING_LITERAL("value"), placeholder);
    }

    if (!numberOfLines.isNumber()) {
        measureAttributes.setMapValue(STRING_LITERAL("numberOfLines"),
                                      Valdi::Value(static_cast<int32_t>(_defaultNumberOfLines)));
    }

    return _textLayerClass->onMeasure(measureAttributes, maxSize, isRightToLeft);
}

void EditableTextLayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {
    std::vector<snap::valdi_core::CompositeAttributePart> fontParts;
    fontParts.emplace_back(STRING_LITERAL("fontSize"), snap::valdi_core::AttributeType::Double, true, true);

    BIND_TEXT_ATTRIBUTE(EditableTextLayer, value, true);
    BIND_COMPOSITE_ATTRIBUTE(EditableTextLayer, font, fontParts);
    BIND_COLOR_ATTRIBUTE(EditableTextLayer, color, false);

    BIND_STRING_ATTRIBUTE(EditableTextLayer, textAlign, false);
    BIND_STRING_ATTRIBUTE(EditableTextLayer, textDecoration, false);
    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, customUnderlineStyle, false);
    BIND_STRING_ATTRIBUTE(EditableTextLayer, textOverflow, true);

    BIND_INT_ATTRIBUTE(EditableTextLayer, numberOfLines, true);

    BIND_BOOLEAN_ATTRIBUTE(EditableTextLayer, adjustsFontSizeToFitWidth, true);
    BIND_DOUBLE_ATTRIBUTE(EditableTextLayer, minimumScaleFactor, true);

    BIND_DOUBLE_ATTRIBUTE(EditableTextLayer, letterSpacing, true);

    BIND_DOUBLE_ATTRIBUTE(EditableTextLayer, lineHeightMultiple, true);
    BIND_DOUBLE_ATTRIBUTE(EditableTextLayer, lineHeight, true);

    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, textShadow, true);

    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, textGradient, false);

    BIND_STRING_ATTRIBUTE(EditableTextLayer, placeholder, true);
    BIND_COLOR_ATTRIBUTE(EditableTextLayer, placeholderColor, false);

    BIND_BOOLEAN_ATTRIBUTE(EditableTextLayer, selectable, false);
    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, selection, false);
    BIND_FUNCTION_ATTRIBUTE(EditableTextLayer, onSelectionChange);

    REGISTER_PREPROCESSOR(font, true);
    REGISTER_PREPROCESSOR(customUnderlineStyle, true);
}

IMPLEMENT_TEXT_ATTRIBUTE(
    EditableTextLayer,
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
    EditableTextLayer,
    font,
    { return _textLayerClass->applyFontAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetFontAttribute(view.getTextLayer()); })

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Value> EditableTextLayerClass::preprocess_font(const Valdi::Value& value) {
    return _textLayerClass->preprocess_font(value);
}

IMPLEMENT_UNTYPED_ATTRIBUTE(
    EditableTextLayer,
    customUnderlineStyle,
    { return _textLayerClass->applyCustomUnderlineStyleAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetCustomUnderlineStyleAttribute(view.getTextLayer()); })

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Value> EditableTextLayerClass::preprocess_customUnderlineStyle(const Valdi::Value& value) {
    return _textLayerClass->preprocess_customUnderlineStyle(value);
}

IMPLEMENT_COLOR_ATTRIBUTE(
    EditableTextLayer,
    color,
    {
        view.setTextColor(snapDrawingColorFromValdiColor(value));
        return Valdi::Void();
    },
    { view.setTextColor(Color::black()); })

IMPLEMENT_STRING_ATTRIBUTE(
    EditableTextLayer,
    textAlign,
    { return _textLayerClass->applyTextAlignAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetTextAlignAttribute(view.getTextLayer()); })

IMPLEMENT_STRING_ATTRIBUTE(
    EditableTextLayer,
    textDecoration,
    { return _textLayerClass->applyTextDecorationAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetTextDecorationAttribute(view.getTextLayer()); })

IMPLEMENT_STRING_ATTRIBUTE(
    EditableTextLayer,
    textOverflow,
    { return _textLayerClass->applyTextOverflowAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetTextOverflowAttribute(view.getTextLayer()); })

IMPLEMENT_INT_ATTRIBUTE(
    EditableTextLayer,
    numberOfLines,
    { return _textLayerClass->applyNumberOfLinesAttribute(view.getTextLayer(), value); },
    { view.getTextLayer().setNumberOfLines(_defaultNumberOfLines); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    EditableTextLayer,
    adjustsFontSizeToFitWidth,
    { return _textLayerClass->applyAdjustsFontSizeToFitWidthAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetAdjustsFontSizeToFitWidthAttribute(view.getTextLayer()); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    EditableTextLayer,
    minimumScaleFactor,
    { return _textLayerClass->applyMinimumScaleFactorAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetMinimumScaleFactorAttribute(view.getTextLayer()); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    EditableTextLayer,
    lineHeightMultiple,
    { return _textLayerClass->applyLineHeightMultipleAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetLineHeightMultipleAttribute(view.getTextLayer()); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    EditableTextLayer,
    lineHeight,
    { return _textLayerClass->applyLineHeightAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetLineHeightAttribute(view.getTextLayer()); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    EditableTextLayer,
    letterSpacing,
    { return _textLayerClass->applyLetterSpacingAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetLetterSpacingAttribute(view.getTextLayer()); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    EditableTextLayer,
    textShadow,
    { return _textLayerClass->applyTextShadowAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetTextShadowAttribute(view.getTextLayer()); })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    EditableTextLayer,
    textGradient,
    { return _textLayerClass->applyTextGradientAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetTextGradientAttribute(view.getTextLayer()); })

IMPLEMENT_STRING_ATTRIBUTE(
    EditableTextLayer,
    placeholder,
    {
        view.setPlaceholder(value);
        return Valdi::Void();
    },
    { view.setPlaceholder(Valdi::StringBox()); })

IMPLEMENT_COLOR_ATTRIBUTE(
    EditableTextLayer,
    placeholderColor,
    {
        view.setPlaceholderColor(snapDrawingColorFromValdiColor(value));
        return Valdi::Void();
    },
    { view.setPlaceholderColor(kDefaultPlaceholderColor); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(EditableTextLayer, selectable, { return Valdi::Void(); }, {})

IMPLEMENT_UNTYPED_ATTRIBUTE(EditableTextLayer, selection, { return Valdi::Void(); }, {})

Valdi::Result<Valdi::Void> EditableTextLayerClass::apply_onSelectionChange(
    EditableTextLayer& /*editableTextLayer*/,
    const Valdi::Ref<Valdi::ValueFunction>& /*value*/,
    const AttributeContext& /*context*/) {
    return Valdi::Void();
}

void EditableTextLayerClass::reset_onSelectionChange(EditableTextLayer& /*editableTextLayer*/,
                                                     const AttributeContext& /*context*/) {}

} // namespace snap::drawing
