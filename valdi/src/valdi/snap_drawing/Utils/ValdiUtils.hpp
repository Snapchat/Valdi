//
//  ValdiUtils.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#pragma once

#include "valdi/runtime/Views/View.hpp"

#include "snap_drawing/cpp/Layers/Layer.hpp"

namespace Valdi {
class ViewNode;
}

namespace snap::drawing {

class DrawableSurfaceCanvas;

Valdi::Ref<Valdi::View> layerToValdiView(const Valdi::Ref<Layer>& layer, bool makeStrongRef);
Valdi::Ref<Layer> valdiViewToLayer(const Valdi::Ref<Valdi::View>& view);

struct ValdiLayerData : public Valdi::SimpleRefCountable {
    Valdi::Weak<Valdi::ViewNode> viewNode;
    bool deliveredHoverEnter = false;
};

Valdi::Ref<ValdiLayerData> getValdiLayerData(const Layer& layer);
Valdi::Ref<Valdi::ViewNode> valdiViewNodeFromLayer(const Layer& layer);
void setValdiViewNodeToLayer(Layer& layer, Valdi::ViewNode* viewNode);

Valdi::Ref<Valdi::View> bridgeViewFromView(const Valdi::Ref<Valdi::View>& view);

// Adjusts `deltaX` according to whether the layer is RTL.
Scalar resolveDeltaX(const Layer& layer, Scalar deltaX);

Color snapDrawingColorFromValdiColor(int64_t color);

Path pathFromValdiGeometricPath(const Valdi::BytesView& pathData, double width, double height);

void drawLayerInCanvas(const Ref<Layer>& layer, DrawableSurfaceCanvas& canvas);

} // namespace snap::drawing
