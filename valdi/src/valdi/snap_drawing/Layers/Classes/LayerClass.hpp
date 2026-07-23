//
//  LayerClass.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#pragma once

#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Touches/TapGestureRecognizer.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

class LayerClass : public ILayerClass {
public:
    explicit LayerClass(const Ref<Resources>& resources);

    Valdi::Ref<Layer> instantiate() override;
    void bindAttributes(Valdi::AttributesBindingContext& binder) override;

    void bindFunctionAndPredicateAttribute(
        Valdi::AttributesBindingContext& binder,
        const Valdi::StringBox& attributeName,
        Valdi::Result<Valdi::Void> (LayerClass::*apply)(Layer&,
                                                        const Valdi::Ref<Valdi::ValueFunction>&,
                                                        const Valdi::Ref<Valdi::ValueFunction>&,
                                                        const Valdi::Value&),
        void (LayerClass::*reset)(Layer&),
        snap::valdi_core::CompositeAttributePart* additionalPart = nullptr);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_backgroundColor(Layer& view, int64_t value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_backgroundColor(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_borderRadius(Layer& view,
                                                  const BorderRadius& value,
                                                  const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_borderRadius(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_opacity(Layer& view, double opacity, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_opacity(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_transform(Layer& view,
                                               const Valdi::Value& value,
                                               const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_transform(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_slowClipping(Layer& view, bool value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_slowClipping(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_touchEnabled(Layer& view, bool value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_touchEnabled(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_filterTouchesWhenObscured(Layer& view,
                                                               bool value,
                                                               const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_filterTouchesWhenObscured(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onTouch(Layer& view,
                                             const Valdi::Ref<Valdi::ValueFunction>& value,
                                             const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onTouch(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onTouchStart(Layer& view,
                                                  const Valdi::Ref<Valdi::ValueFunction>& value,
                                                  const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onTouchStart(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onTouchEnd(Layer& view,
                                                const Valdi::Ref<Valdi::ValueFunction>& value,
                                                const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onTouchEnd(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_hitTest(Layer& view,
                                             const Valdi::Ref<Valdi::ValueFunction>& value,
                                             const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_hitTest(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onTap(Layer& view,
                                           const Valdi::Ref<Valdi::ValueFunction>& callback,
                                           const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                           const Valdi::Value& additionalValue);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onTap(Layer& view);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onDoubleTap(Layer& view,
                                                 const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                 const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                 const Valdi::Value& additionalValue);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onDoubleTap(Layer& view);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onDrag(Layer& view,
                                            const Valdi::Ref<Valdi::ValueFunction>& callback,
                                            const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                            const Valdi::Value& additionalValue);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onDrag(Layer& view);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onLongPress(Layer& view,
                                                 const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                 const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                 const Valdi::Value& onLongPressDuration);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onLongPress(Layer& view);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onPinch(Layer& view,
                                             const Valdi::Ref<Valdi::ValueFunction>& callback,
                                             const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                             const Valdi::Value& additionalValue);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onPinch(Layer& view);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_onRotate(Layer& view,
                                              const Valdi::Ref<Valdi::ValueFunction>& callback,
                                              const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                              const Valdi::Value& additionalValue);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_onRotate(Layer& view);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_boxShadow(Layer& view, const Valdi::Value& value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_boxShadow(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_border(Layer& view, const Valdi::Value& value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_border(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_borderColor(Layer& view, int64_t value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_borderColor(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_borderWidth(Layer& view, double value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_borderWidth(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_background(Layer& view,
                                                const Valdi::Value& value,
                                                const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_background(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_maskImage(Layer& view, const Valdi::Value& value, const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_maskImage(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_accessibilityId(Layer& view,
                                                     const Valdi::Value& value,
                                                     const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_accessibilityId(Layer& view, const AttributeContext& context);

    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    Valdi::Result<Valdi::Void> apply_touchAreaExtensionComposite(Layer& view,
                                                                 const Valdi::Value& value,
                                                                 const AttributeContext& context);
    // NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
    void reset_touchAreaExtensionComposite(Layer& view, const AttributeContext& context);

    DECLARE_UNTYPED_ATTRIBUTE(Layer, maskPath)

    DECLARE_DOUBLE_ATTRIBUTE(Layer, maskOpacity)

    DECLARE_DOUBLE_ATTRIBUTE(Layer, onTouchDelayDuration)

    static TapGestureRecognizer::Listener makeTapGestureListener(const Valdi::Ref<Valdi::ValueFunction>& function);
};

} // namespace snap::drawing
