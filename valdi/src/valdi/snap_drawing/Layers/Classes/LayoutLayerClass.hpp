//
//  LayoutLayerClass.hpp
//  valdi
//
//  SnapDrawing layer class for Layout (macOS/desktop).
//  Registers "Layout" so it resolves to a layer instead of falling back to base.
//

#pragma once

#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

class LayoutLayerClass : public ILayerClass {
public:
    explicit LayoutLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass);
    ~LayoutLayerClass() override;

    Valdi::Ref<Layer> instantiate() override;
    void bindAttributes(Valdi::AttributesBindingContext& binder) override;
};

} // namespace snap::drawing
