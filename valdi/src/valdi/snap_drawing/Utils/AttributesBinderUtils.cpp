//
//  AttributesBinderUtils.cpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Views/ViewAttributeHandlerDelegate.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

namespace snap::drawing {

AttributeContext::AttributeContext(const Valdi::Ref<Valdi::Animator>& animator, const Valdi::StringBox& attributeName)
    : animator(Valdi::castOrNull<ValdiAnimator>(animator != nullptr ? animator->getNativeAnimator() : nullptr)),
      attributeName(attributeName) {}

AttributeContext::~AttributeContext() = default;

class SnapDrawingAttributeHandlerDelegate : public Valdi::ViewAttributeHandlerDelegate {
public:
    explicit SnapDrawingAttributeHandlerDelegate(AttributeResetter&& resetter) : _resetter(std::move(resetter)) {}

protected:
    Valdi::Result<Valdi::Void> onViewApply(const Valdi::Ref<Valdi::View>& view,
                                           const Valdi::StringBox& name,
                                           const Valdi::Value& value,
                                           const Valdi::Ref<Valdi::Animator>& animator) override {
        AttributeContext context(animator, name);
        auto layer = valdiViewToLayer(view);
        if (layer == nullptr) {
            return Valdi::Void();
        }
        return onLayerApply(layer, value, context);
    }

    void onViewReset(const Valdi::Ref<Valdi::View>& view,
                     const Valdi::StringBox& name,
                     const Valdi::Ref<Valdi::Animator>& animator) override {
        AttributeContext context(animator, name);
        auto layer = valdiViewToLayer(view);
        if (layer == nullptr) {
            return;
        }
        _resetter(layer, context);
    }

    virtual Valdi::Result<Valdi::Void> onLayerApply(const Valdi::Ref<Layer>& view,
                                                    const Valdi::Value& value,
                                                    AttributeContext& contextr) = 0;

private:
    AttributeResetter _resetter;
};

template<typename T>
class GenericAttributeHandlerDelegate : public SnapDrawingAttributeHandlerDelegate {
public:
    GenericAttributeHandlerDelegate(AttributeApplier<T>&& applier, AttributeResetter&& resetter)
        : SnapDrawingAttributeHandlerDelegate(std::move(resetter)), _applier(std::move(applier)) {}

protected:
    Valdi::Result<Valdi::Void> onLayerApply(const Valdi::Ref<Layer>& view,
                                            const Valdi::Value& value,
                                            AttributeContext& context) override {
        return _applier(view, value.to<T>(), context);
    }

private:
    AttributeApplier<T> _applier;
};

class BorderAttributeHandler : public SnapDrawingAttributeHandlerDelegate {
public:
    BorderAttributeHandler(AttributeApplier<BorderRadius>&& applier, AttributeResetter&& resetter)
        : SnapDrawingAttributeHandlerDelegate(std::move(resetter)), _applier(std::move(applier)) {}

    Valdi::Result<Valdi::Void> onLayerApply(const Valdi::Ref<Layer>& view,
                                            const Valdi::Value& value,
                                            AttributeContext& context) override {
        auto result = Valdi::ValueConverter::toBorderValues(value);
        if (!result) {
            return result.moveError();
        }
        auto border = *result.value();
        return _applier(view,
                        BorderRadius(static_cast<Scalar>(border.getTopLeftValue()),
                                     static_cast<Scalar>(border.getTopRightValue()),
                                     static_cast<Scalar>(border.getBottomRightValue()),
                                     static_cast<Scalar>(border.getBottomLeftValue()),
                                     border.isTopLeftPercent(),
                                     border.isTopRightPercent(),
                                     border.isBottomRightPercent(),
                                     border.isBottomLeftPercent()),
                        context);
    }

private:
    AttributeApplier<BorderRadius> _applier;
};

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeStringAttribute(AttributeApplier<Valdi::StringBox>&& applier,
                                                                AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<Valdi::StringBox>>(std::move(applier),
                                                                                std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeDoubleAttribute(AttributeApplier<double>&& applier,
                                                                AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<double>>(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeBooleanAttribute(AttributeApplier<bool>&& applier,
                                                                 AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<bool>>(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeColorAttribute(AttributeApplier<int64_t>&& applier,
                                                               AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<int64_t>>(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeIntAttribute(AttributeApplier<int64_t>&& applier,
                                                             AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<int64_t>>(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeBorderAttribute(AttributeApplier<BorderRadius>&& applier,
                                                                AttributeResetter&& resetter) {
    return Valdi::makeShared<BorderAttributeHandler>(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeTextAttribute(AttributeApplier<const Valdi::Value&>&& applier,
                                                              AttributeResetter&& resetter) {
    return makeUntypedAttribute(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeUntypedAttribute(AttributeApplier<const Valdi::Value&>&& applier,
                                                                 AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<Valdi::Value>>(std::move(applier), std::move(resetter));
}

Valdi::Ref<Valdi::AttributeHandlerDelegate> makeFunctionAttribute(
    AttributeApplier<const Valdi::Ref<Valdi::ValueFunction>&>&& applier, AttributeResetter&& resetter) {
    return Valdi::makeShared<GenericAttributeHandlerDelegate<Valdi::Ref<Valdi::ValueFunction>>>(std::move(applier),
                                                                                                std::move(resetter));
}

} // namespace snap::drawing
