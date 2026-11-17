//
//  ITextInputHost.hpp
//  snap_drawing
//

#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace snap::drawing {

class EditableTextLayer;

/**
 * One focus of an editable text layer, which the root's ITextInputHost feeds with what its input method types. Every
 * call must come from the thread the layer tree runs on, and does nothing once the session ended: when the field
 * blurred, left the tree, or the host committed or cancelled it. That makes calls from a request the host already
 * replaced harmless.
 */
class TextInputSession : public Valdi::SimpleRefCountable {
public:
    TextInputSession(const Valdi::Weak<EditableTextLayer>& layer, const Valdi::StringBox& text);
    ~TextInputSession() override;

    /** The field's text when it took focus. */
    const Valdi::StringBox& getInitialText() const;

    /** Replaces the whole text. */
    void replaceText(const Valdi::StringBox& text);
    /** The return key: ends the session when the field closes on return, which a textfield does by default. */
    void commit();
    /** The input method went away without a return: ends the session. */
    void cancel();

private:
    friend EditableTextLayer;

    Valdi::Weak<EditableTextLayer> _layer;
    Valdi::StringBox _initialText;
    bool _active = true;

    Valdi::Ref<EditableTextLayer> activeLayer() const;
};

/**
 * Shows an input method for the focused editable text layer. A LayerRoot without one leaves its editable layers
 * display-only, as they were before: focusing them does nothing.
 */
class ITextInputHost : public Valdi::SharedPtrRefCountable {
public:
    /**
     * `session`'s field took focus. Another field's session may still be open: a host with one input method cancels it,
     * so that only one field is focused.
     */
    virtual void beginTextInput(const Valdi::Ref<TextInputSession>& session) = 0;
    /** `session`'s field blurred or left the tree, or the host itself committed or cancelled it. */
    virtual void endTextInput(const Valdi::Ref<TextInputSession>& session) = 0;
};

} // namespace snap::drawing
