//
//  SnapDrawingViewManager.cpp
//  valdi-skia-app
//
//  Created by Simon Corsin on 6/28/20.
//

#include "valdi/snap_drawing/SnapDrawingLayerHolder.hpp"

#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

#include "snap_drawing/cpp/Utils/Image.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"

#include "valdi/snap_drawing/Layers/BridgeLayer.hpp"

#include "snap_drawing/cpp/Animations/Animation.hpp"
#include "snap_drawing/cpp/Animations/SpringAnimation.hpp"
#include "snap_drawing/cpp/Animations/ValueInterpolators.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "valdi/snap_drawing/Animations/ValdiAnimator.hpp"

#include "snap_drawing/cpp/Layers/Interfaces/ILoadedAssetLayer.hpp"
#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"
#include "valdi_core/cpp/Views/Measure.hpp"

namespace snap::drawing {

SnapDrawingLayerHolder::SnapDrawingLayerHolder() = default;
SnapDrawingLayerHolder::~SnapDrawingLayerHolder() = default;

VALDI_CLASS_IMPL(SnapDrawingLayerHolder)

void SnapDrawingLayerHolder::setWeak(const Valdi::Ref<Layer>& view) {
    _strong = nullptr;
    _weak = view;
}

void SnapDrawingLayerHolder::setStrong(const Valdi::Ref<Layer>& view) {
    _weak.reset();
    _strong = view;
}

Valdi::Ref<Layer> SnapDrawingLayerHolder::get() const {
    if (_strong != nullptr) {
        return _strong;
    }
    return _weak.lock();
}

snap::valdi_core::Platform SnapDrawingLayerHolder::getPlatform() const {
    return snap::valdi_core::Platform::Skia;
}

} // namespace snap::drawing
