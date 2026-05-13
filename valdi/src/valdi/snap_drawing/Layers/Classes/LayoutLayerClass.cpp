//
//  LayoutLayerClass.cpp
//  valdi
//

#include "valdi/snap_drawing/Layers/Classes/LayoutLayerClass.hpp"
#include "snap_drawing/cpp/Layers/Layer.hpp"

namespace snap::drawing {

LayoutLayerClass::LayoutLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass)
    : ILayerClass(resources, "Layout", "com.snap.valdi.views.ValdiView", parentClass, false) {}

LayoutLayerClass::~LayoutLayerClass() = default;

Valdi::Ref<Layer> LayoutLayerClass::instantiate() {
    return makeLayer<Layer>(getResources());
}

void LayoutLayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {
    getParent()->bindAttributes(binder);
}

} // namespace snap::drawing
