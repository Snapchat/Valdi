//
//  EditableTextLayerClass.hpp
//  valdi-desktop-apple
//

#pragma once

#include "snap_drawing/cpp/Layers/EditableTextLayer.hpp"
#include "valdi/snap_drawing/Layers/Classes/TextLayerClass.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

/**
 * Reports a field's focus and edits to Valdi: its `value` and `focused` attributes, and the JS handlers the element
 * sets, of which it holds the ones set.
 */
class EditableTextLayerListenerImpl : public EditableTextLayerListener {
public:
    void onEditBegin(EditableTextLayer& layer) override;
    void onChange(EditableTextLayer& layer) override;
    void onReturn(EditableTextLayer& layer) override;
    void onEditEnd(EditableTextLayer& layer, EditTextEndReason reason) override;

    Valdi::Ref<Valdi::ValueFunction> onEditBeginFn;
    Valdi::Ref<Valdi::ValueFunction> onChangeFn;
    Valdi::Ref<Valdi::ValueFunction> onReturnFn;
    Valdi::Ref<Valdi::ValueFunction> onEditEndFn;

private:
    static void reportFocused(EditableTextLayer& layer);
    static void call(const Valdi::Ref<Valdi::ValueFunction>& function, Valdi::Value event);
};

class EditableTextLayerClass : public ILayerClass {
public:
    static Ref<EditableTextLayerClass> makeForTextField(const Ref<Resources>& resources,
                                                        const Ref<TextLayerClass>& parentClass);

    static Ref<EditableTextLayerClass> makeForTextView(const Ref<Resources>& resources,
                                                       const Ref<TextLayerClass>& parentClass);

    EditableTextLayerClass(const Ref<Resources>& resources,
                           const Ref<TextLayerClass>& parentClass,
                           const char* iosClassName,
                           const char* androidClassName,
                           int defaultNumberOfLines,
                           bool managesChildFrames,
                           TextVerticalAlignment textVerticalAlignment);
    ~EditableTextLayerClass() override;

    bool isFallback() const override {
        return true;
    }

    bool managesChildFrames() const override;

    Valdi::Ref<Layer> instantiate() override;

    Size onMeasure(const Valdi::Value& attributes, Size maxSize, bool isRightToLeft) override;

    void bindAttributes(Valdi::AttributesBindingContext& binder) override;

    DECLARE_TEXT_ATTRIBUTE(EditableTextLayer, value)

    DECLARE_INT_ATTRIBUTE(EditableTextLayer, numberOfLines)

    DECLARE_COLOR_ATTRIBUTE(EditableTextLayer, color)

    DECLARE_UNTYPED_ATTRIBUTE(EditableTextLayer, font)

    DECLARE_STRING_ATTRIBUTE(EditableTextLayer, textAlign)

    DECLARE_STRING_ATTRIBUTE(EditableTextLayer, textDecoration)

    DECLARE_UNTYPED_ATTRIBUTE(EditableTextLayer, customUnderlineStyle)

    DECLARE_STRING_ATTRIBUTE(EditableTextLayer, textOverflow)

    DECLARE_BOOLEAN_ATTRIBUTE(EditableTextLayer, adjustsFontSizeToFitWidth)

    DECLARE_DOUBLE_ATTRIBUTE(EditableTextLayer, minimumScaleFactor)

    DECLARE_DOUBLE_ATTRIBUTE(EditableTextLayer, lineHeight)

    DECLARE_DOUBLE_ATTRIBUTE(EditableTextLayer, lineHeightAbsolute)

    DECLARE_DOUBLE_ATTRIBUTE(EditableTextLayer, letterSpacing)

    DECLARE_UNTYPED_ATTRIBUTE(EditableTextLayer, textShadow)

    DECLARE_UNTYPED_ATTRIBUTE(EditableTextLayer, textGradient)

    DECLARE_STRING_ATTRIBUTE(EditableTextLayer, placeholder)

    DECLARE_COLOR_ATTRIBUTE(EditableTextLayer, placeholderColor)

    DECLARE_BOOLEAN_ATTRIBUTE(EditableTextLayer, selectable)

    DECLARE_UNTYPED_ATTRIBUTE(EditableTextLayer, selection)

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onSelectionChange(EditableTextLayer& editableTextLayer,
                                                       const Valdi::Ref<Valdi::ValueFunction>& value,
                                                       const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onSelectionChange(EditableTextLayer& editableTextLayer, const AttributeContext& context);

    DECLARE_BOOLEAN_ATTRIBUTE(EditableTextLayer, focused)

    DECLARE_BOOLEAN_ATTRIBUTE(EditableTextLayer, enabled)

    DECLARE_BOOLEAN_ATTRIBUTE(EditableTextLayer, closesWhenReturnKeyPressed)

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onChange(EditableTextLayer& editableTextLayer,
                                              const Valdi::Ref<Valdi::ValueFunction>& value,
                                              const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onChange(EditableTextLayer& editableTextLayer, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onEditBegin(EditableTextLayer& editableTextLayer,
                                                 const Valdi::Ref<Valdi::ValueFunction>& value,
                                                 const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onEditBegin(EditableTextLayer& editableTextLayer, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onEditEnd(EditableTextLayer& editableTextLayer,
                                               const Valdi::Ref<Valdi::ValueFunction>& value,
                                               const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onEditEnd(EditableTextLayer& editableTextLayer, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onReturn(EditableTextLayer& editableTextLayer,
                                              const Valdi::Ref<Valdi::ValueFunction>& value,
                                              const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onReturn(EditableTextLayer& editableTextLayer, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Value> preprocess_font(const Valdi::Value& value);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Value> preprocess_customUnderlineStyle(const Valdi::Value& value);

private:
    Ref<TextLayerClass> _textLayerClass;
    int _defaultNumberOfLines;
    bool _managesChildFrames;
    TextVerticalAlignment _textVerticalAlignment;

    // A textfield ends editing on return; a textview keeps it.
    bool closesWhenReturnKeyPressedByDefault() const;
};

} // namespace snap::drawing
