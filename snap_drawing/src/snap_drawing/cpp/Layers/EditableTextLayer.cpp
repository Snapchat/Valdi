//
//  EditableTextLayer.cpp
//  valdi-skia
//

#include "snap_drawing/cpp/Layers/EditableTextLayer.hpp"

namespace snap::drawing {

EditableTextLayer::EditableTextLayer(const Ref<Resources>& resources)
    : Layer(resources), _textLayer(makeLayer<TextLayer>(resources)) {
    updateDisplayedText();
}

EditableTextLayer::~EditableTextLayer() = default;

void EditableTextLayer::onInitialize() {
    Layer::onInitialize();
    addChild(_textLayer);
    layoutTextLayer();
}

Size EditableTextLayer::sizeThatFits(Size maxSize) {
    return _textLayer->sizeThatFits(maxSize);
}

void EditableTextLayer::setText(const Valdi::StringBox& text) {
    if (_value != text || _attributedValue != nullptr) {
        _value = text;
        _attributedValue = nullptr;
        updateDisplayedText();
    }
}

void EditableTextLayer::setAttributedText(const Valdi::Ref<AttributedText>& attributedText) {
    if (_attributedValue != attributedText) {
        _attributedValue = attributedText;
        _value = Valdi::StringBox();
        updateDisplayedText();
    }
}

TextLayer& EditableTextLayer::getTextLayer() {
    return *_textLayer;
}

void EditableTextLayer::setTextColor(Color textColor) {
    if (_textColor != textColor) {
        _textColor = textColor;
        updateDisplayedText();
    }
}

void EditableTextLayer::setPlaceholder(const Valdi::StringBox& placeholder) {
    if (_placeholder != placeholder) {
        _placeholder = placeholder;
        updateDisplayedText();
    }
}

void EditableTextLayer::setPlaceholderColor(Color placeholderColor) {
    if (_placeholderColor != placeholderColor) {
        _placeholderColor = placeholderColor;
        updateDisplayedText();
    }
}

bool EditableTextLayer::hasValue() const {
    if (_attributedValue != nullptr) {
        return !_attributedValue->isContentEmpty();
    }

    return !_value.isEmpty();
}

void EditableTextLayer::updateDisplayedText() {
    if (hasValue()) {
        _showingPlaceholder = false;
        _textLayer->setTextColor(_textColor);
        if (_attributedValue != nullptr) {
            _textLayer->setAttributedText(_attributedValue);
        } else {
            _textLayer->setText(_value);
        }
    } else {
        _showingPlaceholder = true;
        _textLayer->setTextColor(_placeholderColor);
        _textLayer->setText(_placeholder);
    }
}

void EditableTextLayer::onBoundsChanged() {
    Layer::onBoundsChanged();
    layoutTextLayer();
}

void EditableTextLayer::onLayout() {
    Layer::onLayout();
    layoutTextLayer();
}

void EditableTextLayer::onRightToLeftChanged() {
    Layer::onRightToLeftChanged();
    _textLayer->setRightToLeft(isRightToLeft());
}

void EditableTextLayer::layoutTextLayer() {
    const auto& frame = getFrame();
    _textLayer->setFrame(Rect::makeXYWH(0, 0, frame.width(), frame.height()));
}

} // namespace snap::drawing
