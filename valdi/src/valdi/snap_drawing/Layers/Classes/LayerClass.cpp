//
//  LayerClass.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"

#include "snap_drawing/cpp/Layers/Mask/PaintMaskLayer.hpp"

#include "snap_drawing/cpp/Touches/DoubleTapGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/DragGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/LongPressGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/PinchGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/RotateGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/SingleTapGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/TapGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/TouchGestureRecognizer.hpp"
#include "utils/debugging/Assert.hpp"

namespace snap::drawing {

class ValdiMaskLayer : public PaintMaskLayer {
public:
    ValdiMaskLayer() {
        setPositioning(MaskLayerPositioning::BelowBackground);
        setColor(Color::black());
        setBlendMode(SkBlendMode::kDstOut);
    }

    ~ValdiMaskLayer() override = default;

    void setPathData(const Valdi::Ref<Valdi::ValueTypedArray>& pathData) {
        _pathData = pathData;
        _pathDirty = true;
    }

    bool isEmpty() const {
        return _pathData == nullptr && getColor().getAlpha() == 255;
    }

    void setOpacity(Scalar opacity) {
        setColor(Color::black().withAlphaRatio(opacity));
    }

    Ref<IMask> createMask(const Rect& bounds) override {
        if (_pathData == nullptr) {
            return nullptr;
        }

        if (bounds != _lastBounds) {
            _lastBounds = bounds;
            _pathDirty = true;
        }

        if (_pathDirty) {
            _pathDirty = false;
            setPath(pathFromValdiGeometricPath(_pathData->getBuffer(), bounds.width(), bounds.height()));
        }

        return PaintMaskLayer::createMask(bounds);
    }

private:
    Valdi::Ref<Valdi::ValueTypedArray> _pathData;
    bool _pathDirty = false;
    Rect _lastBounds;
};

LayerClass::LayerClass(const Ref<Resources>& resources)
    : ILayerClass(resources, "SCValdiView", "com.snap.valdi.views.ValdiView", nullptr, false) {}

Valdi::Ref<Layer> LayerClass::instantiate() {
    return makeLayer<Layer>(getResources());
}

void LayerClass::bindFunctionAndPredicateAttribute(
    Valdi::AttributesBindingContext& binder,
    const Valdi::StringBox& attributeName,
    Valdi::Result<Valdi::Void> (LayerClass::*apply)(
        Layer&, const Valdi::Ref<Valdi::ValueFunction>&, const Valdi::Ref<Valdi::ValueFunction>&, const Valdi::Value&),
    void (LayerClass::*reset)(Layer&),
    snap::valdi_core::CompositeAttributePart* additionalPart) {
    auto compositeName = attributeName + "Composite";
    auto predicateName = attributeName + "Predicate";
    auto disabledName = attributeName + "Disabled";
    std::vector<snap::valdi_core::CompositeAttributePart> parts{
        {attributeName, snap::valdi_core::AttributeType::Untyped, false, false},
        {predicateName, snap::valdi_core::AttributeType::Untyped, true, false},
        {disabledName, snap::valdi_core::AttributeType::Boolean, true, false}};
    if (additionalPart != nullptr) {
        parts.emplace_back(*additionalPart);
    }

    auto attribute = makeUntypedAttribute(
        [=](const auto& view, auto value, const auto& context) -> Valdi::Result<Valdi::Void> {
            auto castedView = Valdi::castOrNull<Layer>(view);
            if (castedView == nullptr)
                return Valdi::Void();

            const auto* array = value.getArray();
            if (array == nullptr || array->size() < 3) {
                return Valdi::Error("Expected at least 3 array entries in bindFunctionAndPredicateAttribute");
            }

            auto callbackFunction = (*array)[0].getFunctionRef();
            auto predicateFunction = (*array)[1].getFunctionRef();
            auto disabled = (*array)[2].toBool();
            auto additionalValue = (array->size() > 3) ? (*array)[3] : Valdi::Value::undefinedRef();
            if (!disabled) {
                return (this->*apply)(*castedView, callbackFunction, predicateFunction, additionalValue);
            } else {
                (this->*reset)(*castedView);
                return Valdi::Void();
            }
        },
        [=](const auto& view, const auto& context) {
            auto castedView = Valdi::castOrNull<Layer>(view);
            if (castedView == nullptr)
                return;
            (this->*reset)(*castedView);
        });
    binder.bindCompositeAttribute(compositeName, parts, attribute);
}

void LayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {
    BIND_COLOR_ATTRIBUTE(Layer, backgroundColor, false);
    BIND_COLOR_ATTRIBUTE(Layer, borderColor, false);

    BIND_BORDER_ATTRIBUTE(Layer, borderRadius, false);

    BIND_DOUBLE_ATTRIBUTE(Layer, opacity, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, translationX, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, translationY, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, scaleX, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, scaleY, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, rotation, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, borderWidth, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, onTouchDelayDuration, false);

    BIND_UNTYPED_ATTRIBUTE(Layer, accessibilityId, false);
    BIND_UNTYPED_ATTRIBUTE(Layer, boxShadow, false);
    BIND_UNTYPED_ATTRIBUTE(Layer, border, false);
    BIND_UNTYPED_ATTRIBUTE(Layer, background, false);

    BIND_UNTYPED_ATTRIBUTE(Layer, maskPath, false);
    BIND_DOUBLE_ATTRIBUTE(Layer, maskOpacity, false);
    BIND_UNTYPED_ATTRIBUTE(Layer, maskImage, false);

    BIND_BOOLEAN_ATTRIBUTE(Layer, slowClipping, false);
    BIND_BOOLEAN_ATTRIBUTE(Layer, touchEnabled, false);
    BIND_BOOLEAN_ATTRIBUTE(Layer, filterTouchesWhenObscured, false);

    BIND_FUNCTION_ATTRIBUTE(Layer, onTouch);
    BIND_FUNCTION_ATTRIBUTE(Layer, onTouchStart);
    BIND_FUNCTION_ATTRIBUTE(Layer, onTouchEnd);
    BIND_FUNCTION_ATTRIBUTE(Layer, hitTest);

    snap::valdi_core::CompositeAttributePart longPressDurationPart = {
        STRING_LITERAL("longPressDuration"), snap::valdi_core::AttributeType::Double, true, false};
    bindFunctionAndPredicateAttribute(binder,
                                      STRING_LITERAL("onLongPress"),
                                      &LayerClass::apply_onLongPress,
                                      &LayerClass::reset_onLongPress,
                                      &longPressDurationPart);
    bindFunctionAndPredicateAttribute(
        binder, STRING_LITERAL("onPinch"), &LayerClass::apply_onPinch, &LayerClass::reset_onPinch);
    bindFunctionAndPredicateAttribute(
        binder, STRING_LITERAL("onDrag"), &LayerClass::apply_onDrag, &LayerClass::reset_onDrag);
    bindFunctionAndPredicateAttribute(
        binder, STRING_LITERAL("onRotate"), &LayerClass::apply_onRotate, &LayerClass::reset_onRotate);
    bindFunctionAndPredicateAttribute(
        binder, STRING_LITERAL("onTap"), &LayerClass::apply_onTap, &LayerClass::reset_onTap);
    bindFunctionAndPredicateAttribute(
        binder, STRING_LITERAL("onDoubleTap"), &LayerClass::apply_onDoubleTap, &LayerClass::reset_onDoubleTap);

    std::vector<snap::valdi_core::CompositeAttributePart> parts;
    parts.emplace_back(STRING_LITERAL("touchAreaExtension"), snap::valdi_core::AttributeType::Double, true, false);
    parts.emplace_back(STRING_LITERAL("touchAreaExtensionLeft"), snap::valdi_core::AttributeType::Double, true, false);
    parts.emplace_back(STRING_LITERAL("touchAreaExtensionRight"), snap::valdi_core::AttributeType::Double, true, false);
    parts.emplace_back(STRING_LITERAL("touchAreaExtensionTop"), snap::valdi_core::AttributeType::Double, true, false);
    parts.emplace_back(
        STRING_LITERAL("touchAreaExtensionBottom"), snap::valdi_core::AttributeType::Double, true, false);

    BIND_COMPOSITE_ATTRIBUTE(Layer, touchAreaExtensionComposite, parts);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_backgroundColor(Layer& view,
                                                             int64_t value,
                                                             const AttributeContext& context) {
    context.setAnimatableAttribute(view,
                                   &Layer::getBackgroundColor,
                                   &Layer::setBackgroundColor,
                                   MIN_VISIBLE_CHANGE_COLOR,
                                   snapDrawingColorFromValdiColor(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_backgroundColor(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getBackgroundColor, &Layer::setBackgroundColor, MIN_VISIBLE_CHANGE_COLOR, Color::transparent());
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_borderRadius(Layer& view,
                                                          const BorderRadius& value,
                                                          const AttributeContext& context) {
    context.setAnimatableAttribute(view,
                                   &Layer::getBorderRadius,
                                   &Layer::setBorderRadius,
                                   MIN_VISIBLE_CHANGE_PIXEL,
                                   value,
                                   [](const auto& layer, const auto& from, const auto& to, auto ratio) -> auto {
                                       return interpolateBorderRadius(from, to, layer.getFrame(), ratio);
                                   });
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_borderRadius(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view,
        &Layer::getBorderRadius,
        &Layer::setBorderRadius,
        MIN_VISIBLE_CHANGE_PIXEL,
        BorderRadius(),
        [](const auto& layer, const auto& from, const auto& to, auto ratio) -> auto { return to; });
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_opacity(Layer& view, double opacity, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getOpacity, &Layer::setOpacity, MIN_VISIBLE_CHANGE_COLOR, static_cast<Scalar>(opacity));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_opacity(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(view, &Layer::getOpacity, &Layer::setOpacity, MIN_VISIBLE_CHANGE_COLOR, 1.0f);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_translationX(Layer& view, double value, const AttributeContext& context) {
    auto translationX = resolveDeltaX(view, static_cast<Scalar>(value));
    context.setAnimatableAttribute(
        view, &Layer::getTranslationX, &Layer::setTranslationX, MIN_VISIBLE_CHANGE_PIXEL, translationX);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_translationX(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getTranslationX, &Layer::setTranslationX, MIN_VISIBLE_CHANGE_PIXEL, static_cast<Scalar>(0));
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_translationY(Layer& view, double value, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getTranslationY, &Layer::setTranslationY, MIN_VISIBLE_CHANGE_PIXEL, static_cast<Scalar>(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_translationY(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getTranslationY, &Layer::setTranslationY, MIN_VISIBLE_CHANGE_PIXEL, static_cast<Scalar>(0));
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_scaleX(Layer& view, double value, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getScaleX, &Layer::setScaleX, MIN_VISIBLE_CHANGE_SCALE_RATIO, static_cast<Scalar>(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_scaleX(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getScaleX, &Layer::setScaleX, MIN_VISIBLE_CHANGE_SCALE_RATIO, static_cast<Scalar>(1));
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_scaleY(Layer& view, double value, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getScaleY, &Layer::setScaleY, MIN_VISIBLE_CHANGE_SCALE_RATIO, static_cast<Scalar>(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_scaleY(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(
        view, &Layer::getScaleY, &Layer::setScaleY, MIN_VISIBLE_CHANGE_SCALE_RATIO, static_cast<Scalar>(1));
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_rotation(Layer& view, double value, const AttributeContext& context) {
    auto rotationAngle = resolveDeltaX(view, static_cast<Scalar>(value));
    context.setAnimatableAttribute(
        view, &Layer::getRotation, &Layer::setRotation, MIN_VISIBLE_CHANGE_ROTATION_DEGREES_ANGLE, rotationAngle);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_rotation(Layer& view, const AttributeContext& context) {
    context.setAnimatableAttribute(view,
                                   &Layer::getRotation,
                                   &Layer::setRotation,
                                   MIN_VISIBLE_CHANGE_ROTATION_DEGREES_ANGLE,
                                   static_cast<Scalar>(0));
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_background(Layer& view,
                                                        const Valdi::Value& value,
                                                        const AttributeContext& /*context*/) {
    const auto* array = value.getArray();
    if (array == nullptr || array->size() != 4) {
        return Valdi::Error("Expecting 4 values");
    }

    const auto* colors = (*array)[0].getArray();
    const auto* locations = (*array)[1].getArray();
    auto orientation = static_cast<snap::drawing::LinearGradientOrientation>((*array)[2].toInt());
    auto radial = (*array)[3].toBool();

    if (colors == nullptr || locations == nullptr) {
        return Valdi::Error("Expecting 2 arrays");
    }

    std::vector<Color> outColors;
    outColors.reserve(colors->size());

    for (const auto& color : *colors) {
        outColors.emplace_back(snapDrawingColorFromValdiColor(color.toLong()));
    }

    std::vector<Scalar> outLocations;
    outLocations.reserve(locations->size());

    for (const auto& location : *locations) {
        outLocations.emplace_back(static_cast<Scalar>(location.toDouble()));
    }

    if (radial) {
        view.setBackgroundRadialGradient(std::move(outLocations), std::move(outColors));
    } else {
        view.setBackgroundLinearGradient(std::move(outLocations), std::move(outColors), orientation);
    }

    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_background(Layer& view, const AttributeContext& /*context*/) {
    view.setBackgroundLinearGradient({}, {}, snap::drawing::LinearGradientOrientationTopBottom);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_maskImage(Layer& view,
                                                       const Valdi::Value& value,
                                                       const AttributeContext& /*context*/) {
    const auto* array = value.getArray();
    if (array == nullptr || array->size() != 4) {
        return Valdi::Error("Expecting 4 values");
    }

    const auto* colors = (*array)[0].getArray();
    const auto* locations = (*array)[1].getArray();
    auto orientation = static_cast<snap::drawing::LinearGradientOrientation>((*array)[2].toInt());
    auto radial = (*array)[3].toBool();

    if (colors == nullptr || locations == nullptr) {
        return Valdi::Error("Expecting 2 arrays");
    }

    std::vector<Color> outColors;
    outColors.reserve(colors->size());

    for (const auto& color : *colors) {
        outColors.emplace_back(snapDrawingColorFromValdiColor(color.toLong()));
    }

    std::vector<Scalar> outLocations;
    outLocations.reserve(locations->size());

    for (const auto& location : *locations) {
        outLocations.emplace_back(static_cast<Scalar>(location.toDouble()));
    }

    if (radial) {
        view.setMaskImageRadialGradient(std::move(outLocations), std::move(outColors));
    } else {
        view.setMaskImageLinearGradient(std::move(outLocations), std::move(outColors), orientation);
    }

    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_maskImage(Layer& view, const AttributeContext& /*context*/) {
    view.setMaskImageLinearGradient({}, {}, snap::drawing::LinearGradientOrientationTopBottom);
}

static void callEventHandler(const Valdi::Ref<Valdi::ValueFunction>& value, Valdi::Value& jsEvent) {
    (*value)({std::move(jsEvent)});
}

static void populatePoint(const Point& point, const String& xKey, const String& yKey, Valdi::Value& value) {
    value.setMapValue(xKey, Valdi::Value(static_cast<double>(point.x)));
    value.setMapValue(yKey, Valdi::Value(static_cast<double>(point.y)));
}

static void populateTouchEvent(const GestureRecognizer& gestureRecognizer, Valdi::Value& value) {
    static auto kState = STRING_LITERAL("state");
    static auto kX = STRING_LITERAL("x");
    static auto kY = STRING_LITERAL("y");
    static auto kAbsoluteX = STRING_LITERAL("absoluteX");
    static auto kAbsoluteY = STRING_LITERAL("absoluteY");

    switch (gestureRecognizer.getState()) {
        case GestureRecognizerStatePossible:
        case GestureRecognizerStateFailed:
            break;
        case GestureRecognizerStateBegan:
            value.setMapValue(kState, Valdi::Value(static_cast<int32_t>(0)));
            break;
        case GestureRecognizerStateChanged:
            value.setMapValue(kState, Valdi::Value(static_cast<int32_t>(1)));
            break;
        case GestureRecognizerStateEnded:
            value.setMapValue(kState, Valdi::Value(static_cast<int32_t>(2)));
            break;
    }

    auto location = gestureRecognizer.getLocation();
    auto locationInWindow = gestureRecognizer.getLocationInWindow();

    auto layer = dynamic_cast<Layer*>(gestureRecognizer.getLayer().get());
    if (layer != nullptr) {
        auto viewNode = valdiViewNodeFromLayer(*layer);
        auto valdiPoint = Valdi::Point(location.x, location.y);
        auto adjustedLocation = viewNode->convertPointToRelativeDirectionAgnostic(valdiPoint);
        // Use the relative point when asking the viewNode to calculate, not the absolute point from the gesture.
        auto adjustedAbsoluteLocation = viewNode->convertPointToAbsoluteDirectionAgnostic(valdiPoint);
        location = Point(adjustedLocation.x, adjustedLocation.y);
        locationInWindow = Point(adjustedAbsoluteLocation.x, adjustedAbsoluteLocation.y);
    }

    populatePoint(location, kX, kY, value);
    populatePoint(locationInWindow, kAbsoluteX, kAbsoluteY, value);
}

template<class T>
static T makePressListener(const Valdi::Ref<Valdi::ValueFunction>& value, GestureRecognizerState allowed) {
    return [value, allowed](const auto& gesture, auto state, const auto& /*location*/) {
        if (state == allowed) {
            Valdi::Value jsEvent;
            populateTouchEvent(gesture, jsEvent);
            callEventHandler(value, jsEvent);
        }
    };
}

template<class T>
static T makePressShouldBeginListener(const Valdi::Ref<Valdi::ValueFunction>& value) {
    return [value](const auto& gesture, const Point& point) {
        Valdi::Value jsEvent;
        populateTouchEvent(gesture, jsEvent);

        auto result = value->call(Valdi::ValueFunctionFlagsCallSync, {std::move(jsEvent)});

        if (result && result.value().isBool()) {
            return result.value().toBool();
        } else {
            return false;
        }
    };
}

TapGestureRecognizer::Listener LayerClass::makeTapGestureListener(const Valdi::Ref<Valdi::ValueFunction>& function) {
    return makePressListener<TapGestureRecognizer::Listener>(function, GestureRecognizerStateEnded);
}

template<class T>
static T& getOrCreateGestureRecognizer(const Ref<Resources>& resources, Layer& view) {
    auto index = view.indexOfGestureRecognizerOfType(typeid(T));
    if (index) {
        return dynamic_cast<T&>(*view.getGestureRecognizer(index.value()));
    }
    auto gestureRecognizer = Valdi::makeShared<T>(resources->getGesturesConfiguration());
    view.addGestureRecognizer(gestureRecognizer);
    return *gestureRecognizer;
}

template<class T>
static void removeGestureRecognizerIfNeeded(Layer& view, T& gestureRecognizer) {
    if (!gestureRecognizer.isEmpty()) {
        return;
    }
    view.removeGestureRecognizer(Valdi::strongSmallRef(&gestureRecognizer));
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onTap(Layer& view,
                                                   const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                   const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                   const Valdi::Value& additionalValue) {
    auto& tapGesture = getOrCreateGestureRecognizer<SingleTapGestureRecognizer>(getResources(), view);
    tapGesture.setListener(makeTapGestureListener(callback));
    if (predicate != nullptr) {
        tapGesture.setShouldBeginListener(
            makePressShouldBeginListener<SingleTapGestureRecognizer::ShouldBeginListener>(predicate));
    } else {
        tapGesture.setShouldBeginListener(SingleTapGestureRecognizer::ShouldBeginListener());
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onTap(Layer& view) {
    auto& tapGesture = getOrCreateGestureRecognizer<SingleTapGestureRecognizer>(getResources(), view);
    tapGesture.setListener(SingleTapGestureRecognizer::Listener());
    tapGesture.setShouldBeginListener(SingleTapGestureRecognizer::ShouldBeginListener());
    removeGestureRecognizerIfNeeded<SingleTapGestureRecognizer>(view, tapGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onDoubleTap(Layer& view,
                                                         const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                         const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                         const Valdi::Value& additionalValue) {
    auto& doubleTapGesture = getOrCreateGestureRecognizer<DoubleTapGestureRecognizer>(getResources(), view);
    doubleTapGesture.setListener(makeTapGestureListener(callback));
    if (predicate != nullptr) {
        doubleTapGesture.setShouldBeginListener(
            makePressShouldBeginListener<DoubleTapGestureRecognizer::ShouldBeginListener>(predicate));
    } else {
        doubleTapGesture.setShouldBeginListener(DoubleTapGestureRecognizer::ShouldBeginListener());
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onDoubleTap(Layer& view) {
    auto& doubleTapGesture = getOrCreateGestureRecognizer<DoubleTapGestureRecognizer>(getResources(), view);
    doubleTapGesture.setListener(DoubleTapGestureRecognizer::Listener());
    doubleTapGesture.setShouldBeginListener(DoubleTapGestureRecognizer::ShouldBeginListener());
    removeGestureRecognizerIfNeeded<DoubleTapGestureRecognizer>(view, doubleTapGesture);
}
static void populateDragEvent(const GestureRecognizer& gestureRecognizer, const DragEvent& event, Valdi::Value& value) {
    populateTouchEvent(gestureRecognizer, value);

    static auto kDeltaX = STRING_LITERAL("deltaX");
    static auto kDeltaY = STRING_LITERAL("deltaY");
    static auto kVelocityX = STRING_LITERAL("velocityX");
    static auto kVelocityY = STRING_LITERAL("velocityY");

    auto offsetX = event.offset.width;
    auto velocityX = event.velocity.dx;

    auto layer = dynamic_cast<Layer*>(gestureRecognizer.getLayer().get());
    if (layer != nullptr) {
        offsetX = resolveDeltaX(*layer, offsetX);
        velocityX = resolveDeltaX(*layer, velocityX);
    }

    populatePoint(Point::make(offsetX, event.offset.height), kDeltaX, kDeltaY, value);
    populatePoint(Point::make(velocityX, event.velocity.dy), kVelocityX, kVelocityY, value);
}

static DragGestureRecognizer::Listener makeDragListener(const Valdi::Ref<Valdi::ValueFunction>& value) {
    return [value](const auto& gesture, auto /*state*/, const DragEvent& event) {
        Valdi::Value jsEvent;
        populateDragEvent(gesture, event, jsEvent);
        callEventHandler(value, jsEvent);
    };
}

static RotateGestureRecognizer::Listener makeRotateListener(const Valdi::Ref<Valdi::ValueFunction>& value) {
    return [value](const auto& gesture, auto /*state*/, const RotateEvent& event) {
        Valdi::Value jsEvent;
        populateDragEvent(gesture, event, jsEvent);
        auto rotation = static_cast<double>(event.rotation);
        auto layer = dynamic_cast<Layer*>(gesture.getLayer().get());
        if (layer) {
            rotation = resolveDeltaX(*layer, rotation);
        }
        jsEvent.setMapValue(STRING_LITERAL("rotation"), Valdi::Value(rotation));
        callEventHandler(value, jsEvent);
    };
}

static PinchGestureRecognizer::Listener makePinchListener(const Valdi::Ref<Valdi::ValueFunction>& value) {
    return [value](const auto& gesture, auto /*state*/, const PinchEvent& event) {
        Valdi::Value jsEvent;
        populateDragEvent(gesture, event, jsEvent);
        jsEvent.setMapValue(STRING_LITERAL("scale"), Valdi::Value(static_cast<double>(event.scale)));
        callEventHandler(value, jsEvent);
    };
}

template<class T, class E>
static T makeMoveShouldBeginListener(const Valdi::Ref<Valdi::ValueFunction>& value) {
    return [value](const auto& gesture, const E& event) {
        Valdi::Value jsEvent;
        populateDragEvent(gesture, event, jsEvent);

        auto result = value->call(Valdi::ValueFunctionFlagsCallSync, {std::move(jsEvent)});

        if (result && result.value().isBool()) {
            return result.value().toBool();
        } else {
            return false;
        }
    };
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onDrag(Layer& view,
                                                    const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                    const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                    const Valdi::Value& additionalValue) {
    auto& dragGesture = getOrCreateGestureRecognizer<DragGestureRecognizer>(getResources(), view);
    dragGesture.setListener(makeDragListener(callback));
    if (predicate != nullptr) {
        dragGesture.setShouldBeginListener(
            makeMoveShouldBeginListener<DragGestureRecognizer::ShouldBeginListener, DragEvent>(predicate));
    } else {
        dragGesture.setShouldBeginListener(DragGestureRecognizer::ShouldBeginListener());
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onDrag(Layer& view) {
    auto& dragGesture = getOrCreateGestureRecognizer<DragGestureRecognizer>(getResources(), view);
    dragGesture.setListener(DragGestureRecognizer::Listener());
    dragGesture.setShouldBeginListener(DragGestureRecognizer::ShouldBeginListener());
    removeGestureRecognizerIfNeeded<DragGestureRecognizer>(view, dragGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onLongPress(Layer& view,
                                                         const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                         const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                         const Valdi::Value& onLongPressDuration) {
    auto& longPressGesture = getOrCreateGestureRecognizer<LongPressGestureRecognizer>(getResources(), view);
    longPressGesture.setListener(
        makePressListener<LongPressGestureRecognizer::Listener>(callback, GestureRecognizerStateBegan));
    if (predicate != nullptr) {
        longPressGesture.setShouldBeginListener(
            makePressShouldBeginListener<LongPressGestureRecognizer::ShouldBeginListener>(predicate));
    } else {
        longPressGesture.setShouldBeginListener(LongPressGestureRecognizer::ShouldBeginListener());
    }
    if (onLongPressDuration.isNumber()) {
        longPressGesture.setLongPressTimeout(Duration::fromSeconds(onLongPressDuration.toDouble()));
    } else {
        longPressGesture.setLongPressTimeout(getResources()->getGesturesConfiguration().longPressTimeout);
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onLongPress(Layer& view) {
    auto& longPressGesture = getOrCreateGestureRecognizer<LongPressGestureRecognizer>(getResources(), view);
    longPressGesture.setListener(LongPressGestureRecognizer::Listener());
    longPressGesture.setShouldBeginListener(LongPressGestureRecognizer::ShouldBeginListener());
    longPressGesture.setLongPressTimeout(getResources()->getGesturesConfiguration().longPressTimeout);
    removeGestureRecognizerIfNeeded<LongPressGestureRecognizer>(view, longPressGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onRotate(Layer& view,
                                                      const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                      const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                      const Valdi::Value& additionalValue) {
    auto& rotateGesture = getOrCreateGestureRecognizer<RotateGestureRecognizer>(getResources(), view);
    rotateGesture.setListener(makeRotateListener(callback));
    if (predicate != nullptr) {
        rotateGesture.setShouldBeginListener(
            makeMoveShouldBeginListener<RotateGestureRecognizer::ShouldBeginListener, RotateEvent>(predicate));
    } else {
        rotateGesture.setShouldBeginListener(RotateGestureRecognizer::ShouldBeginListener());
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onRotate(Layer& view) {
    auto& rotateGesture = getOrCreateGestureRecognizer<RotateGestureRecognizer>(getResources(), view);
    rotateGesture.setListener(RotateGestureRecognizer::Listener());
    rotateGesture.setShouldBeginListener(RotateGestureRecognizer::ShouldBeginListener());
    removeGestureRecognizerIfNeeded<RotateGestureRecognizer>(view, rotateGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onPinch(Layer& view,
                                                     const Valdi::Ref<Valdi::ValueFunction>& callback,
                                                     const Valdi::Ref<Valdi::ValueFunction>& predicate,
                                                     const Valdi::Value& additionalValue) {
    auto& pinchGesture = getOrCreateGestureRecognizer<PinchGestureRecognizer>(getResources(), view);
    pinchGesture.setListener(makePinchListener(callback));
    if (predicate != nullptr) {
        pinchGesture.setShouldBeginListener(
            makeMoveShouldBeginListener<PinchGestureRecognizer::ShouldBeginListener, PinchEvent>(predicate));
    } else {
        pinchGesture.setShouldBeginListener(PinchGestureRecognizer::ShouldBeginListener());
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onPinch(Layer& view) {
    auto& pinchGesture = getOrCreateGestureRecognizer<PinchGestureRecognizer>(getResources(), view);
    pinchGesture.setListener(PinchGestureRecognizer::Listener());
    pinchGesture.setShouldBeginListener(PinchGestureRecognizer::ShouldBeginListener());
    removeGestureRecognizerIfNeeded<PinchGestureRecognizer>(view, pinchGesture);
}

static TouchGestureRecognizerListener makeTouchListener(const Valdi::Ref<Valdi::ValueFunction>& value) {
    return [value](const auto& gesture, auto /*state*/, const auto& /*location*/) {
        Valdi::Value jsEvent;
        populateTouchEvent(gesture, jsEvent);
        callEventHandler(value, jsEvent);
    };
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onTouch(Layer& view,
                                                     const Valdi::Ref<Valdi::ValueFunction>& value,
                                                     const AttributeContext& /*context*/) {
    auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
    touchGesture.setListener(makeTouchListener(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onTouch(Layer& view, const AttributeContext& /*context*/) {
    auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
    touchGesture.setListener(TouchGestureRecognizerListener());
    removeGestureRecognizerIfNeeded<TouchGestureRecognizer>(view, touchGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onTouchStart(Layer& view,
                                                          const Valdi::Ref<Valdi::ValueFunction>& value,
                                                          const AttributeContext& /*context*/) {
    auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
    touchGesture.setOnStartListener(makeTouchListener(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onTouchStart(Layer& view, const AttributeContext& /*context*/) {
    auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
    touchGesture.setOnStartListener(TouchGestureRecognizerListener());
    removeGestureRecognizerIfNeeded<TouchGestureRecognizer>(view, touchGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_onTouchEnd(Layer& view,
                                                        const Valdi::Ref<Valdi::ValueFunction>& value,
                                                        const AttributeContext& /*context*/) {
    auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
    touchGesture.setOnEndListener(makeTouchListener(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_onTouchEnd(Layer& view, const AttributeContext& /*context*/) {
    auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
    touchGesture.setOnEndListener(TouchGestureRecognizerListener());
    removeGestureRecognizerIfNeeded<TouchGestureRecognizer>(view, touchGesture);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_hitTest(Layer& view,
                                                     const Valdi::Ref<Valdi::ValueFunction>& value,
                                                     const AttributeContext& /*context*/) {
    // noop
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_hitTest(Layer& view, const AttributeContext& /*context*/) {
    // noop
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_slowClipping(Layer& view,
                                                          bool value,
                                                          const AttributeContext& /*context*/) {
    view.setClipsToBounds(value);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_slowClipping(Layer& view, const AttributeContext& /*context*/) {
    view.setClipsToBounds(view.getClipsToBoundsDefaultValue());
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_touchEnabled(Layer& view,
                                                          bool value,
                                                          const AttributeContext& /*context*/) {
    view.setTouchEnabled(value);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_touchEnabled(Layer& view, const AttributeContext& /*context*/) {
    view.setTouchEnabled(true);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_filterTouchesWhenObscured(Layer& view,
                                                                       bool value,
                                                                       const AttributeContext& /*context*/) {
    // `filterTouchesWhenObscured` is an Android-specific property to prevent tapjacking over sensitive inputs
    // Ref: https://developer.android.com/privacy-and-security/risks/tapjacking
    //
    // This is not yet implemented and for now will always disable the input for security purposes.
    if (value) {
        SC_ASSERT_FAIL("FilterTouchesWhenObscured is not implemented in SnapDrawing yet");
        view.setTouchEnabled(false);
    }
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
// No-op, filterTouchesWhenObscured is an Android only attribute
void LayerClass::reset_filterTouchesWhenObscured(Layer& /*view*/, const AttributeContext& /*context*/) {}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_accessibilityId(Layer& view,
                                                             const Valdi::Value& value,
                                                             const AttributeContext& /*context*/) {
    if (value.isString()) {
        view.setAccessibilityId(value.toStringBox());
    }

    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_accessibilityId(Layer& view, const AttributeContext& /*context*/) {
    view.setAccessibilityId(Valdi::StringBox());
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_boxShadow(Layer& view,
                                                       const Valdi::Value& value,
                                                       const AttributeContext& context) {
    if (value.isNullOrUndefined()) {
        reset_boxShadow(view, context);
        return Valdi::Void();
    }

    const auto* entries = value.getArray();
    if (entries == nullptr || entries->size() < 5) {
        return Valdi::Error("boxShadow components should have 5 entries");
    }

    auto widthOffset = (*entries)[1].toDouble();
    auto heightOffset = (*entries)[2].toDouble();
    auto blur = (*entries)[3].toDouble();
    auto color = (*entries)[4].toLong();

    auto skColor = snapDrawingColorFromValdiColor(color);

    view.setBoxShadow(
        static_cast<Scalar>(widthOffset), static_cast<Scalar>(heightOffset), static_cast<Scalar>(blur), skColor);

    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_boxShadow(Layer& view, const AttributeContext& /*context*/) {
    view.setBoxShadow(0, 0, 0, Color::transparent());
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_border(Layer& view,
                                                    const Valdi::Value& value,
                                                    const AttributeContext& /*context*/) {
    const auto* entries = value.getArray();
    if (entries == nullptr) {
        return Valdi::Error("Missing border components");
    }

    Scalar borderWidth = 0;
    if (!entries->empty()) {
        borderWidth = static_cast<Scalar>((*entries)[0].toDouble());
    }

    auto borderColor = Color::transparent();
    if (entries->size() > 1) {
        borderColor = snapDrawingColorFromValdiColor((*entries)[1].toLong());
    }

    view.setBorderWidth(borderWidth);
    view.setBorderColor(borderColor);

    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_border(Layer& view, const AttributeContext& /*context*/) {
    view.setBorderWidth(0);
    view.setBorderColor(Color::transparent());
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_borderWidth(Layer& view,
                                                         double value,
                                                         const AttributeContext& /*context*/) {
    view.setBorderWidth(static_cast<Scalar>(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_borderWidth(Layer& view, const AttributeContext& /*context*/) {
    view.setBorderWidth(0);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_borderColor(Layer& view,
                                                         int64_t value,
                                                         const AttributeContext& /*context*/) {
    view.setBorderColor(snapDrawingColorFromValdiColor(value));
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_borderColor(Layer& view, const AttributeContext& /*context*/) {
    view.setBorderColor(Color::transparent());
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> LayerClass::apply_touchAreaExtensionComposite(Layer& view,
                                                                         const Valdi::Value& value,
                                                                         const AttributeContext& /*context*/) {
    const auto* values = value.getArray();
    if (values == nullptr || values->size() != 5) {
        return Valdi::Error("Expecting 5 entries");
    }

    Scalar left = 0;
    Scalar right = 0;
    Scalar top = 0;
    Scalar bottom = 0;

    if ((*values)[0].isDouble()) {
        left = static_cast<Scalar>((*values)[0].toDouble());
        right = left;
        top = right;
        bottom = top;
    }

    if ((*values)[1].isDouble()) {
        left = static_cast<Scalar>((*values)[1].toDouble());
    }
    if ((*values)[2].isDouble()) {
        right = static_cast<Scalar>((*values)[2].toDouble());
    }
    if ((*values)[3].isDouble()) {
        top = static_cast<Scalar>((*values)[3].toDouble());
    }
    if ((*values)[4].isDouble()) {
        bottom = static_cast<Scalar>((*values)[4].toDouble());
    }

    view.setTouchAreaExtension(left, right, top, bottom);

    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void LayerClass::reset_touchAreaExtensionComposite(Layer& view, const AttributeContext& /*context*/) {
    view.setTouchAreaExtension(0, 0, 0, 0);
}

template<typename F>
static void updateMaskLayer(Layer& layer, F&& cb) {
    auto maskLayer = Valdi::castOrNull<ValdiMaskLayer>(layer.getMaskLayer());
    if (maskLayer == nullptr) {
        maskLayer = Valdi::makeShared<ValdiMaskLayer>();
        layer.setMaskLayer(maskLayer);
    }

    cb(*maskLayer);

    if (maskLayer->isEmpty()) {
        layer.setMaskLayer(nullptr);
    } else {
        layer.setNeedsDisplay();
    }
}

IMPLEMENT_UNTYPED_ATTRIBUTE(
    Layer,
    maskPath,
    {
        updateMaskLayer(view, [&](ValdiMaskLayer& maskLayer) { maskLayer.setPathData(value.getTypedArrayRef()); });

        return Valdi::Void();
    },
    { updateMaskLayer(view, [&](ValdiMaskLayer& maskLayer) { maskLayer.setPathData(nullptr); }); });

IMPLEMENT_DOUBLE_ATTRIBUTE(
    Layer,
    maskOpacity,
    {
        updateMaskLayer(view, [&](ValdiMaskLayer& maskLayer) { maskLayer.setOpacity(static_cast<Scalar>(value)); });

        return Valdi::Void();
    },
    { updateMaskLayer(view, [&](ValdiMaskLayer& maskLayer) { maskLayer.setOpacity(1.0f); }); });

IMPLEMENT_DOUBLE_ATTRIBUTE(
    Layer,
    onTouchDelayDuration,
    {
        auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
        touchGesture.setOnTouchDelayDuration(Duration::fromSeconds(value));
        return Valdi::Void();
    },
    {
        auto& touchGesture = getOrCreateGestureRecognizer<TouchGestureRecognizer>(getResources(), view);
        touchGesture.setOnTouchDelayDuration(Duration::fromSeconds(0));
        removeGestureRecognizerIfNeeded<TouchGestureRecognizer>(view, touchGesture);
    });
} // namespace snap::drawing
