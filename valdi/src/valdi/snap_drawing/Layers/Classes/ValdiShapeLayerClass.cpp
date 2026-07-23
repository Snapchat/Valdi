//
//  ValdiShapeLayerClass.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 3/4/22.
//

#include "valdi/snap_drawing/Layers/Classes/ValdiShapeLayerClass.hpp"
#include "valdi/snap_drawing/Animations/ValdiAnimator.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"

#include <vector>

namespace snap::drawing {

namespace {

Valdi::Result<Valdi::Void> applyFillGradientAttribute(ShapeLayer& shapeLayer, const Valdi::Value& value) {
    const auto* array = value.getArray();
    if (array == nullptr || array->size() != 4) {
        return Valdi::Error("Expecting 4 values from fillGradient");
    }

    const auto* colors = (*array)[0].getArray();
    const auto* locations = (*array)[1].getArray();
    auto orientation = static_cast<snap::drawing::LinearGradientOrientation>((*array)[2].toInt());
    auto radial = (*array)[3].toBool();

    if (colors == nullptr || locations == nullptr) {
        return Valdi::Error("Expecting 2 arrays: colors and locations");
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
        shapeLayer.setFillRadialGradient(std::move(outLocations), std::move(outColors));
    } else {
        shapeLayer.setFillLinearGradient(std::move(outLocations), std::move(outColors), orientation);
    }

    return Valdi::Void();
}

void resetFillGradientAttribute(ShapeLayer& shapeLayer) {
    shapeLayer.resetFillGradient();
}

} // namespace

ValdiShapeLayerClass::ValdiShapeLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass)
    : ILayerClass(resources, "SCValdiShapeView", "com.snap.valdi.views.ShapeView", parentClass, false) {}
ValdiShapeLayerClass::~ValdiShapeLayerClass() = default;

Valdi::Ref<Layer> ValdiShapeLayerClass::instantiate() {
    return makeLayer<ValdiShapeLayer>(getResources());
}

void ValdiShapeLayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {
    BIND_UNTYPED_ATTRIBUTE(ValdiShapeLayer, path, false);

    BIND_DOUBLE_ATTRIBUTE(ValdiShapeLayer, strokeWidth, false);
    BIND_COLOR_ATTRIBUTE(ValdiShapeLayer, strokeColor, false);
    BIND_COLOR_ATTRIBUTE(ValdiShapeLayer, fillColor, false);
    BIND_UNTYPED_ATTRIBUTE(ValdiShapeLayer, fillGradient, false);
    BIND_STRING_ATTRIBUTE(ValdiShapeLayer, strokeCap, false);
    BIND_STRING_ATTRIBUTE(ValdiShapeLayer, strokeJoin, false);
    BIND_DOUBLE_ATTRIBUTE(ValdiShapeLayer, strokeStart, false);
    BIND_DOUBLE_ATTRIBUTE(ValdiShapeLayer, strokeEnd, false);
}

IMPLEMENT_UNTYPED_ATTRIBUTE(
    ValdiShapeLayer,
    path,
    {
        view.setPathData(value.getTypedArrayRef());
        return Valdi::Void();
    },
    { view.setPathData(nullptr); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    ValdiShapeLayer,
    strokeWidth,
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeWidth,
                                       &ValdiShapeLayer::setStrokeWidth,
                                       MIN_VISIBLE_CHANGE_PIXEL,
                                       static_cast<Scalar>(value));
        return Valdi::Void();
    },
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeWidth,
                                       &ValdiShapeLayer::setStrokeWidth,
                                       MIN_VISIBLE_CHANGE_PIXEL,
                                       1.0f);
    })

IMPLEMENT_COLOR_ATTRIBUTE(
    ValdiShapeLayer,
    strokeColor,
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeColor,
                                       &ValdiShapeLayer::setStrokeColor,
                                       MIN_VISIBLE_CHANGE_COLOR,
                                       snapDrawingColorFromValdiColor(value));
        return Valdi::Void();
    },
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeColor,
                                       &ValdiShapeLayer::setStrokeColor,
                                       MIN_VISIBLE_CHANGE_COLOR,
                                       Color::transparent());
    })

IMPLEMENT_COLOR_ATTRIBUTE(
    ValdiShapeLayer,
    fillColor,
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getFillColor,
                                       &ValdiShapeLayer::setFillColor,
                                       MIN_VISIBLE_CHANGE_COLOR,
                                       snapDrawingColorFromValdiColor(value));
        return Valdi::Void();
    },
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getFillColor,
                                       &ValdiShapeLayer::setFillColor,
                                       MIN_VISIBLE_CHANGE_COLOR,
                                       Color::transparent());
    })

IMPLEMENT_UNTYPED_ATTRIBUTE(
    ValdiShapeLayer,
    fillGradient,
    { return applyFillGradientAttribute(view, value); },
    { resetFillGradientAttribute(view); })

IMPLEMENT_STRING_ATTRIBUTE(
    ValdiShapeLayer,
    strokeCap,
    {
        if (value == "butt") {
            view.setStrokeCap(PaintStrokeCapButt);
        } else if (value == "round") {
            view.setStrokeCap(PaintStrokeCapRound);
        } else if (value == "square") {
            view.setStrokeCap(PaintStrokeCapSquare);
        } else {
            return Valdi::Error("Invalid strokeCap");
        }
        return Valdi::Void();
    },
    { view.setStrokeCap(PaintStrokeCapButt); })

IMPLEMENT_STRING_ATTRIBUTE(
    ValdiShapeLayer,
    strokeJoin,
    {
        if (value == "bevel") {
            view.setStrokeJoin(PaintStrokeJoinBevel);
        } else if (value == "miter") {
            view.setStrokeJoin(PaintStrokeJoinMiter);
        } else if (value == "round") {
            view.setStrokeJoin(PaintStrokeJoinRound);
        } else {
            return Valdi::Error("Invalid strokeJoin");
        }
        return Valdi::Void();
    },
    { view.setStrokeCap(PaintStrokeCapButt); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    ValdiShapeLayer,
    strokeStart,
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeStart,
                                       &ValdiShapeLayer::setStrokeStart,
                                       MIN_VISIBLE_CHANGE_PIXEL,
                                       static_cast<Scalar>(value));
        return Valdi::Void();
    },
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeStart,
                                       &ValdiShapeLayer::setStrokeStart,
                                       MIN_VISIBLE_CHANGE_PIXEL,
                                       0.0f);
    })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    ValdiShapeLayer,
    strokeEnd,
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeEnd,
                                       &ValdiShapeLayer::setStrokeEnd,
                                       MIN_VISIBLE_CHANGE_PIXEL,
                                       static_cast<Scalar>(value));
        return Valdi::Void();
    },
    {
        context.setAnimatableAttribute(static_cast<ShapeLayer&>(view),
                                       &ValdiShapeLayer::getStrokeEnd,
                                       &ValdiShapeLayer::setStrokeEnd,
                                       MIN_VISIBLE_CHANGE_PIXEL,
                                       1.0f);
    })

} // namespace snap::drawing
