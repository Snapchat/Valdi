//
//  EditableTextLayerClass.cpp
//  valdi-desktop-apple
//

#include "valdi/snap_drawing/Layers/Classes/EditableTextLayerClass.hpp"

#include "snap_drawing/cpp/Resources.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/snap_drawing/Utils/AttributedTextParser.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

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
    layer->setClosesWhenReturnKeyPressed(closesWhenReturnKeyPressedByDefault());
    layer->setListener(Valdi::makeShared<EditableTextLayerListenerImpl>());
    return layer;
}

Size EditableTextLayerClass::onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft) {
    static const auto kValue = STRING_LITERAL("value");
    static const auto kPlaceholder = STRING_LITERAL("placeholder");
    static const auto kNumberOfLines = STRING_LITERAL("numberOfLines");

    auto value = attributes.getMapValue(kValue);
    auto placeholder = attributes.getMapValue(kPlaceholder);
    auto numberOfLines = attributes.getMapValue(kNumberOfLines);
    auto valueHasContent = textValueHasContent(value);

    if (valueHasContent && numberOfLines.isNumber()) {
        return _textLayerClass->onMeasure(attributes, maxSize, isRightToLeft);
    }

    auto newMap = Valdi::makeShared<Valdi::ValueMap>();
    if (const auto* srcMap = attributes.getMap()) {
        for (const auto& [key, val] : *srcMap) {
            (*newMap)[key] = val;
        }
    }
    auto measureAttributes = Valdi::Value(newMap);

    if (!valueHasContent && placeholder.isString()) {
        measureAttributes.setMapValue(kValue, placeholder);
    }

    if (!numberOfLines.isNumber()) {
        measureAttributes.setMapValue(kNumberOfLines, Valdi::Value(static_cast<int32_t>(_defaultNumberOfLines)));
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

    BIND_DOUBLE_ATTRIBUTE(EditableTextLayer, lineHeight, true);
    BIND_DOUBLE_ATTRIBUTE(EditableTextLayer, lineHeightAbsolute, true);

    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, textShadow, true);

    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, textGradient, false);

    BIND_STRING_ATTRIBUTE(EditableTextLayer, placeholder, true);
    BIND_COLOR_ATTRIBUTE(EditableTextLayer, placeholderColor, false);

    BIND_BOOLEAN_ATTRIBUTE(EditableTextLayer, selectable, false);
    BIND_UNTYPED_ATTRIBUTE(EditableTextLayer, selection, false);
    BIND_FUNCTION_ATTRIBUTE(EditableTextLayer, onSelectionChange);

    BIND_BOOLEAN_ATTRIBUTE(EditableTextLayer, focused, false);
    BIND_BOOLEAN_ATTRIBUTE(EditableTextLayer, enabled, false);
    BIND_BOOLEAN_ATTRIBUTE(EditableTextLayer, closesWhenReturnKeyPressed, false);
    BIND_FUNCTION_ATTRIBUTE(EditableTextLayer, onChange);
    BIND_FUNCTION_ATTRIBUTE(EditableTextLayer, onEditBegin);
    BIND_FUNCTION_ATTRIBUTE(EditableTextLayer, onEditEnd);
    BIND_FUNCTION_ATTRIBUTE(EditableTextLayer, onReturn);

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
    lineHeight,
    { return _textLayerClass->applyLineHeightAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetLineHeightAttribute(view.getTextLayer()); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    EditableTextLayer,
    lineHeightAbsolute,
    { return _textLayerClass->applyLineHeightAbsoluteAttribute(view.getTextLayer(), value); },
    { _textLayerClass->resetLineHeightAbsoluteAttribute(view.getTextLayer()); })

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

// The field's own focus and edits fire these, which happens only under a root with a text input host.
IMPLEMENT_BOOLEAN_ATTRIBUTE(
    EditableTextLayer,
    focused,
    {
        view.setFocused(value);
        return Valdi::Void();
    },
    { view.setFocused(false); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    EditableTextLayer,
    enabled,
    {
        view.setEnabled(value);
        return Valdi::Void();
    },
    { view.setEnabled(true); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    EditableTextLayer,
    closesWhenReturnKeyPressed,
    {
        view.setClosesWhenReturnKeyPressed(value);
        return Valdi::Void();
    },
    { view.setClosesWhenReturnKeyPressed(closesWhenReturnKeyPressedByDefault()); })

bool EditableTextLayerClass::closesWhenReturnKeyPressedByDefault() const {
    return _defaultNumberOfLines == kTextFieldDefaultNumberOfLines;
}

static Valdi::Value makeEditTextEvent(const EditableTextLayer& layer) {
    static auto kText = STRING_LITERAL("text");
    static auto kSelectionStart = STRING_LITERAL("selectionStart");
    static auto kSelectionEnd = STRING_LITERAL("selectionEnd");

    // The cursor stays at the end: the event's selection is the text's length, in UTF-16 code units as JS counts.
    const auto text = layer.getText().toStringView();
    const auto cursor = static_cast<int32_t>(Valdi::utf8ToUtf16(text.data(), text.size()).second);
    Valdi::Value event;
    event.setMapValue(kText, Valdi::Value(layer.getText()));
    event.setMapValue(kSelectionStart, Valdi::Value(cursor));
    event.setMapValue(kSelectionEnd, Valdi::Value(cursor));
    return event;
}

void EditableTextLayerListenerImpl::onEditBegin(EditableTextLayer& layer) {
    reportFocused(layer);
    call(onEditBeginFn, makeEditTextEvent(layer));
}

void EditableTextLayerListenerImpl::onChange(EditableTextLayer& layer) {
    // As the iOS and Android fields do, so a page that sets the value back, or focuses again, isn't deduplicated away.
    if (auto viewNode = valdiViewNodeFromLayer(layer); viewNode != nullptr) {
        viewNode->valueChanged(viewNode->getAttributeIds().getIdForName("value"), Valdi::Value(layer.getText()));
    }
    call(onChangeFn, makeEditTextEvent(layer));
}

void EditableTextLayerListenerImpl::onReturn(EditableTextLayer& layer) {
    call(onReturnFn, makeEditTextEvent(layer));
}

void EditableTextLayerListenerImpl::onEditEnd(EditableTextLayer& layer, EditTextEndReason reason) {
    static auto kReason = STRING_LITERAL("reason");
    reportFocused(layer);
    auto event = makeEditTextEvent(layer);
    event.setMapValue(kReason, Valdi::Value(static_cast<int32_t>(reason)));
    call(onEditEndFn, std::move(event));
}

void EditableTextLayerListenerImpl::reportFocused(EditableTextLayer& layer) {
    if (auto viewNode = valdiViewNodeFromLayer(layer); viewNode != nullptr) {
        viewNode->valueChanged(viewNode->getAttributeIds().getIdForName("focused"), Valdi::Value(layer.isFocused()));
    }
}

void EditableTextLayerListenerImpl::call(const Valdi::Ref<Valdi::ValueFunction>& function, Valdi::Value event) {
    if (function != nullptr) {
        (*function)({std::move(event)});
    }
}

static EditableTextLayerListenerImpl& listenerOf(EditableTextLayer& layer) {
    return *Valdi::castOrNull<EditableTextLayerListenerImpl>(layer.getListener());
}

Valdi::Result<Valdi::Void> EditableTextLayerClass::apply_onChange(EditableTextLayer& editableTextLayer,
                                                                  const Valdi::Ref<Valdi::ValueFunction>& value,
                                                                  const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onChangeFn = value;
    return Valdi::Void();
}

void EditableTextLayerClass::reset_onChange(EditableTextLayer& editableTextLayer, const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onChangeFn = nullptr;
}

Valdi::Result<Valdi::Void> EditableTextLayerClass::apply_onEditBegin(EditableTextLayer& editableTextLayer,
                                                                     const Valdi::Ref<Valdi::ValueFunction>& value,
                                                                     const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onEditBeginFn = value;
    return Valdi::Void();
}

void EditableTextLayerClass::reset_onEditBegin(EditableTextLayer& editableTextLayer,
                                               const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onEditBeginFn = nullptr;
}

Valdi::Result<Valdi::Void> EditableTextLayerClass::apply_onEditEnd(EditableTextLayer& editableTextLayer,
                                                                   const Valdi::Ref<Valdi::ValueFunction>& value,
                                                                   const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onEditEndFn = value;
    return Valdi::Void();
}

void EditableTextLayerClass::reset_onEditEnd(EditableTextLayer& editableTextLayer,
                                             const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onEditEndFn = nullptr;
}

Valdi::Result<Valdi::Void> EditableTextLayerClass::apply_onReturn(EditableTextLayer& editableTextLayer,
                                                                  const Valdi::Ref<Valdi::ValueFunction>& value,
                                                                  const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onReturnFn = value;
    return Valdi::Void();
}

void EditableTextLayerClass::reset_onReturn(EditableTextLayer& editableTextLayer, const AttributeContext& /*context*/) {
    listenerOf(editableTextLayer).onReturnFn = nullptr;
}

} // namespace snap::drawing
