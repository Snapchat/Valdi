//
//  EditableTextLayer.cpp
//  valdi-skia
//

#include "snap_drawing/cpp/Layers/EditableTextLayer.hpp"

#include "snap_drawing/cpp/Layers/Interfaces/ILayerRoot.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Touches/TapGestureRecognizer.hpp"

#include <string_view>

namespace snap::drawing {

namespace {

// A type of its own, so a JS onTap on the field, which reuses the layer's SingleTapGestureRecognizer, doesn't replace
// it.
class FocusTapGestureRecognizer final : public TapGestureRecognizer {
public:
    explicit FocusTapGestureRecognizer(const GesturesConfiguration& gesturesConfiguration)
        : TapGestureRecognizer(1, gesturesConfiguration) {}

protected:
    std::string_view getTypeName() const final {
        return "focus tap";
    }
};

} // namespace

TextInputSession::TextInputSession(const Valdi::Weak<EditableTextLayer>& layer, const Valdi::StringBox& text)
    : _layer(layer), _initialText(text) {}

TextInputSession::~TextInputSession() = default;

const Valdi::StringBox& TextInputSession::getInitialText() const {
    return _initialText;
}

Valdi::Ref<EditableTextLayer> TextInputSession::activeLayer() const {
    return _active ? Valdi::strongRef(_layer) : nullptr;
}

void TextInputSession::replaceText(const Valdi::StringBox& text) {
    if (auto layer = activeLayer(); layer != nullptr) {
        layer->onSessionReplace(text);
    }
}

void TextInputSession::commit() {
    if (auto layer = activeLayer(); layer != nullptr) {
        layer->onSessionCommit();
    }
}

void TextInputSession::cancel() {
    if (auto layer = activeLayer(); layer != nullptr) {
        layer->endEditing(EditTextEndReason::DismissKeyPress);
    }
}

EditableTextLayer::EditableTextLayer(const Ref<Resources>& resources)
    : Layer(resources), _textLayer(makeLayer<TextLayer>(resources)) {
    updateDisplayedText();
}

EditableTextLayer::~EditableTextLayer() {
    if (_session != nullptr) {
        _session->_active = false;
        _sessionHost->endTextInput(_session);
    }
}

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

Layer& EditableTextLayer::getChildInsertionLayer() {
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

const Valdi::StringBox& EditableTextLayer::getText() const {
    return _value;
}

bool EditableTextLayer::isFocused() const {
    return _session != nullptr;
}

bool EditableTextLayer::setFocused(bool focused) {
    _focusPending = focused && _enabled && _session == nullptr;
    if (_focusPending) {
        beginEditing();
    } else if (!focused && _session != nullptr) {
        endEditing(EditTextEndReason::Unknown);
    }
    return isFocused();
}

void EditableTextLayer::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        _focusPending = false;
        if (_session != nullptr) {
            endEditing(EditTextEndReason::Unknown);
        }
    }
}

void EditableTextLayer::setClosesWhenReturnKeyPressed(bool closesWhenReturnKeyPressed) {
    _closesWhenReturnKeyPressed = closesWhenReturnKeyPressed;
}

void EditableTextLayer::setListener(const Ref<EditableTextLayerListener>& listener) {
    _listener = listener;
}

const Ref<EditableTextLayerListener>& EditableTextLayer::getListener() const {
    return _listener;
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

void EditableTextLayer::onRootChanged(ILayerRoot* root) {
    if (_session != nullptr && root != getRoot()) {
        endEditing(EditTextEndReason::Unknown);
    }
    Layer::onRootChanged(root);

    // Only roots that can edit get the tap, so the field takes no touches anywhere else.
    if (_focusTap == nullptr && root != nullptr && root->getTextInputHost() != nullptr) {
        auto focusTap = Valdi::makeShared<FocusTapGestureRecognizer>(getResources()->getGesturesConfiguration());
        auto weakSelf = Valdi::weakRef(this);
        focusTap->setShouldBeginListener([weakSelf](const auto& /*gesture*/, const auto& /*location*/) {
            auto self = Valdi::strongRef(weakSelf);
            // A tap on an already focused field is taken too, or it falls through to a parent that may dismiss the
            // keyboard.
            return self != nullptr && self->_enabled;
        });
        focusTap->setListener([weakSelf](const auto& /*gesture*/, auto state, const auto& /*location*/) {
            if (state != GestureRecognizerStateEnded) {
                return;
            }
            if (auto self = Valdi::strongRef(weakSelf); self != nullptr) {
                self->setFocused(true);
            }
        });
        _focusTap = focusTap;
        addGestureRecognizer(focusTap);
    } else if (_focusTap != nullptr && (root == nullptr || root->getTextInputHost() == nullptr)) {
        removeGestureRecognizer(_focusTap);
        _focusTap = nullptr;
    }
    if (_focusPending) {
        beginEditing();
    }
}

void EditableTextLayer::layoutTextLayer() {
    const auto& frame = getFrame();
    _textLayer->setFrame(Rect::makeXYWH(0, 0, frame.width(), frame.height()));
}

void EditableTextLayer::beginEditing() {
    auto* root = getRoot();
    auto* host = root != nullptr ? root->getTextInputHost() : nullptr;
    if (!_enabled || host == nullptr) {
        return;
    }
    _focusPending = false;
    _session = Valdi::makeShared<TextInputSession>(Valdi::weakRef(this), _value);
    _sessionHost = Valdi::Ref<ITextInputHost>(host);
    _sessionHost->beginTextInput(_session);
    if (_listener != nullptr) {
        _listener->onEditBegin(*this);
    }
}

void EditableTextLayer::endEditing(EditTextEndReason reason) {
    auto session = std::move(_session);
    auto host = std::move(_sessionHost);
    if (session == nullptr) {
        return;
    }
    session->_active = false;
    host->endTextInput(session);
    if (_listener != nullptr) {
        _listener->onEditEnd(*this, reason);
    }
}

void EditableTextLayer::onSessionReplace(const Valdi::StringBox& text) {
    _value = text;
    _attributedValue = nullptr;
    updateDisplayedText();
    if (_listener != nullptr) {
        _listener->onChange(*this);
    }
}

void EditableTextLayer::onSessionCommit() {
    if (_listener != nullptr) {
        _listener->onReturn(*this);
    }
    if (_closesWhenReturnKeyPressed) {
        endEditing(EditTextEndReason::ReturnKeyPress);
    }
}

} // namespace snap::drawing
