//
//  TextAnimationGroupLayerClass.hpp
//  Valdi
//

#pragma once

#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Layers/Interfaces/ILayerClass.hpp"

namespace snap::drawing {

class TextAnimationGroupLayerClass : public ILayerClass {
public:
    TextAnimationGroupLayerClass(const Ref<Resources>& resources, const Ref<LayerClass>& parentClass);
    ~TextAnimationGroupLayerClass() override;

    Valdi::Ref<Layer> instantiate() override;
};

} // namespace snap::drawing
