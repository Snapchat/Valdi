//
//  TextAnimationGroupLayerClass.cpp
//  Valdi
//

#include "valdi/snap_drawing/Layers/Classes/TextAnimationGroupLayerClass.hpp"

namespace snap::drawing {

// TODO: Text animations are not implemented in SnapDrawing yet; this class intentionally renders as a plain view.
TextAnimationGroupLayerClass::TextAnimationGroupLayerClass(const Ref<Resources>& resources,
                                                           const Ref<LayerClass>& parentClass)
    : ILayerClass(
          resources, "SCValdiTextAnimationGroup", "com.snap.valdi.views.ValdiTextAnimationGroup", parentClass, false) {}

TextAnimationGroupLayerClass::~TextAnimationGroupLayerClass() = default;

Valdi::Ref<Layer> TextAnimationGroupLayerClass::instantiate() {
    // TODO: Replace this placeholder once SnapDrawing text animations support grouped timelines.
    return snap::drawing::makeLayer<Layer>(getResources());
}

} // namespace snap::drawing
