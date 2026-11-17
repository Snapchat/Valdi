//
//  EditableTextLayer.hpp
//  valdi-skia
//

#pragma once

#include "snap_drawing/cpp/Layers/Interfaces/IChildInsertionLayerProvider.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ITextInputHost.hpp"
#include "snap_drawing/cpp/Layers/TextLayer.hpp"

namespace snap::drawing {

class TapGestureRecognizer;

/** Why a field stopped editing, with the values of valdi_tsx's EditTextUnfocusReason. */
enum class EditTextEndReason {
    Unknown = 0,
    ReturnKeyPress = 1,
    DismissKeyPress = 2,
};

class EditableTextLayer;

/** Hears a field's focus and edits, which happen only under a root with an ITextInputHost. */
class EditableTextLayerListener : public Valdi::SimpleRefCountable {
public:
    virtual void onEditBegin(EditableTextLayer& layer) = 0;
    virtual void onChange(EditableTextLayer& layer) = 0;
    virtual void onReturn(EditableTextLayer& layer) = 0;
    virtual void onEditEnd(EditableTextLayer& layer, EditTextEndReason reason) = 0;
};

/**
 * A textfield or textview. It is editable only under a root with an ITextInputHost: a tap on it, or setFocused(true),
 * opens a TextInputSession with that host, whose edits change the text and reach the listener. Under any other root
 * it only displays its text.
 */
class EditableTextLayer : public Layer, public IChildInsertionLayerProvider {
public:
    explicit EditableTextLayer(const Ref<Resources>& resources);
    ~EditableTextLayer() override;

    void onInitialize() override;
    Size sizeThatFits(Size maxSize) override;

    void setText(const Valdi::StringBox& text);
    void setAttributedText(const Valdi::Ref<AttributedText>& attributedText);

    TextLayer& getTextLayer();
    Layer& getChildInsertionLayer() override;

    void setTextColor(Color textColor);

    void setPlaceholder(const Valdi::StringBox& placeholder);

    void setPlaceholderColor(Color placeholderColor);

    /** The plain text, which edits replace an attributed value with. */
    const Valdi::StringBox& getText() const;

    bool isFocused() const;
    /**
     * Returns whether the field is focused afterwards. Asked before the field is in a tree with a text input host, it
     * focuses once it is; it can't while disabled.
     */
    bool setFocused(bool focused);
    /** A disabled field blurs, and ignores taps and setFocused(true). */
    void setEnabled(bool enabled);
    /** Whether the return key ends editing, as it does for a textfield by default. */
    void setClosesWhenReturnKeyPressed(bool closesWhenReturnKeyPressed);

    void setListener(const Ref<EditableTextLayerListener>& listener);
    const Ref<EditableTextLayerListener>& getListener() const;

protected:
    void onBoundsChanged() override;
    void onLayout() override;
    void onRightToLeftChanged() override;
    void onRootChanged(ILayerRoot* root) override;

private:
    friend TextInputSession;

    Ref<TextLayer> _textLayer;
    Valdi::StringBox _value;
    Valdi::Ref<AttributedText> _attributedValue;
    Valdi::StringBox _placeholder;
    Color _textColor = Color::black();
    Color _placeholderColor = Color::makeARGB(0xFF, 0xB3, 0xB3, 0xB3);
    bool _showingPlaceholder = false;

    bool _enabled = true;
    bool _closesWhenReturnKeyPressed = true;
    bool _focusPending = false;
    Ref<TextInputSession> _session;
    // The host the session was opened with, which it ends with even after the field left the tree.
    Ref<ITextInputHost> _sessionHost;
    Ref<TapGestureRecognizer> _focusTap;
    Ref<EditableTextLayerListener> _listener;

    bool hasValue() const;
    void updateDisplayedText();
    void layoutTextLayer();

    void beginEditing();
    void endEditing(EditTextEndReason reason);

    void onSessionReplace(const Valdi::StringBox& text);
    void onSessionCommit();
};

} // namespace snap::drawing
