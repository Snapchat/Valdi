//
//  AttributesBinderUtils.hpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#pragma once

#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/PlatformResult.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include "valdi_core/Animator.hpp"
#include "valdi_core/CompositeAttributePart.hpp"

#include "valdi/runtime/Attributes/AttributePreprocessor.hpp"

#include "snap_drawing/cpp/Animations/Animation.hpp"
#include "snap_drawing/cpp/Animations/ValueInterpolators.hpp"
#include "snap_drawing/cpp/Utils/BorderRadius.hpp"

#include "valdi/snap_drawing/Animations/ValdiAnimator.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

#include "valdi/runtime/Attributes/AttributeHandlerDelegate.hpp"

namespace snap::drawing {

class Layer;

struct AttributeContext {
public:
    Valdi::Shared<ValdiAnimator> animator;
    const Valdi::StringBox& attributeName;

    AttributeContext(const Valdi::Ref<Valdi::Animator>& animator, const Valdi::StringBox& attributeName);
    ~AttributeContext();

    AttributeContext withAttributeName(const Valdi::StringBox& attributeName) const;

    template<typename T, typename V, typename Interpolator>
    void setAnimatableAttribute(T& view,
                                V (T::*getter)() const,
                                void (T::*setter)(V),
                                double minVisibleChange,
                                V value,
                                Interpolator interpolator) const {
        view.removeAnimation(attributeName);

        if (animator == nullptr) {
            (view.*setter)(value);
            return;
        }

        auto from = (view.*getter)();
        auto to = value;

        animator->createAndAppendAnimation(
            view, attributeName, minVisibleChange, [setter, from, to, interpolator](auto& view, double ratio) {
                auto& castedView = dynamic_cast<T&>(view);

                auto value = interpolator(castedView, from, to, ratio);
                (castedView.*setter)(value);
            });
    }

    template<typename T, typename V>
    void setAnimatableAttribute(
        T& view, V (T::*getter)() const, void (T::*setter)(V), double minVisibleChange, V value) const {
        setAnimatableAttribute(view,
                               getter,
                               setter,
                               minVisibleChange,
                               value,
                               [](const auto& /*layer*/, const auto& from, const auto& to, auto ratio) -> auto {
                                   return interpolateValue(from, to, ratio);
                               });
    }

private:
    AttributeContext(const Valdi::Shared<ValdiAnimator>& animator, const Valdi::StringBox& attributeName);
};

template<typename T>
using AttributeApplier =
    Valdi::Function<Valdi::Result<Valdi::Void>(const Valdi::Ref<Layer>&, const T&, const AttributeContext&)>;
using AttributeResetter = Valdi::Function<void(const Valdi::Ref<Layer>&, const AttributeContext&)>;

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeStringAttribute(AttributeApplier<Valdi::StringBox>&& applier,
                                                                AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeDoubleAttribute(AttributeApplier<double>&& applier,
                                                                AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeBooleanAttribute(AttributeApplier<bool>&& applier,
                                                                 AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeColorAttribute(AttributeApplier<int64_t>&& applier,
                                                               AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeIntAttribute(AttributeApplier<int64_t>&& applier,
                                                             AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeBorderAttribute(AttributeApplier<BorderRadius>&& applier,
                                                                AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeTextAttribute(AttributeApplier<const Valdi::Value&>&& applier,
                                                              AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeUntypedAttribute(AttributeApplier<const Valdi::Value&>&& applier,
                                                                 AttributeResetter&& resetter);

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeFunctionAttribute(
    AttributeApplier<const Valdi::Ref<Valdi::ValueFunction>>&& applier, AttributeResetter&& resetter);

} // namespace snap::drawing
