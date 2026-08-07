//
//  ValdiShapeLayerClass.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 3/4/22.
//

#pragma once

#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"
#include "valdi/snap_drawing/Layers/ValdiShapeLayer.hpp"

namespace snap::drawing {

class ValdiShapeLayerClass : public ILayerClass {
public:
    explicit ValdiShapeLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass);
    ~ValdiShapeLayerClass() override;

    Valdi::Ref<Layer> instantiate() override;
    void bindAttributes(Valdi::AttributesBindingContext& binder) override;

    DECLARE_UNTYPED_ATTRIBUTE(ValdiShapeLayer, path)

    DECLARE_DOUBLE_ATTRIBUTE(ValdiShapeLayer, strokeWidth)
    DECLARE_COLOR_ATTRIBUTE(ValdiShapeLayer, strokeColor)
    DECLARE_COLOR_ATTRIBUTE(ValdiShapeLayer, fillColor)
    DECLARE_UNTYPED_ATTRIBUTE(ValdiShapeLayer, fillGradient)
    DECLARE_STRING_ATTRIBUTE(ValdiShapeLayer, strokeCap)
    DECLARE_STRING_ATTRIBUTE(ValdiShapeLayer, strokeJoin)
    DECLARE_DOUBLE_ATTRIBUTE(ValdiShapeLayer, strokeStart)
    DECLARE_DOUBLE_ATTRIBUTE(ValdiShapeLayer, strokeEnd)
};

} // namespace snap::drawing
