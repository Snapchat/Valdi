//
//  PaintMaskLayer.cpp
//  snap_drawing-pc
//
//  Created by Simon Corsin on 10/20/22.
//

#include "snap_drawing/cpp/Layers/Mask/PaintMaskLayer.hpp"
#include "snap_drawing/cpp/Drawing/Mask/PaintMask.hpp"

namespace snap::drawing {

PaintMaskLayer::PaintMaskLayer() = default;
PaintMaskLayer::~PaintMaskLayer() = default;

void PaintMaskLayer::setPath(const Path& path) {
    _path = path;
    _rect = Rect::makeEmpty();
}

void PaintMaskLayer::setRect(const Rect& rect) {
    _rect = rect;

    if (!_path.isEmpty()) {
        _path.reset();
    }
}

void PaintMaskLayer::setColor(Color color) {
    _color = color;
}

Color PaintMaskLayer::getColor() const {
    return _color;
}

void PaintMaskLayer::setBlendMode(SkBlendMode blendMode) {
    _blendMode = blendMode;
}

Rect PaintMaskLayer::getBounds() const {
    auto bounds = _path.getBounds();
    return bounds ? bounds.value() : _rect;
}

Ref<IMask> PaintMaskLayer::createMask(const Rect& /*bounds*/) {
    auto pathBounds = getBounds();
    if (pathBounds.isEmpty()) {
        return nullptr;
    }

    Paint paint;
    // Skia leaves a paint aliased by default, which draws a curved path's edge, such as a ring cut by a maskPath, as
    // jagged pixels. A rect region stays aliased, so it erases whole pixels as before.
    paint.setAntiAlias(!_path.isEmpty());
    onConfigurePaint(paint, pathBounds);

    return Valdi::makeShared<PaintMask>(paint, _path, _rect);
}

MaskLayerPositioning PaintMaskLayer::getPositioning() {
    return _positioning;
}

void PaintMaskLayer::setPositioning(MaskLayerPositioning positioning) {
    _positioning = positioning;
}

void PaintMaskLayer::onConfigurePaint(Paint& paint, const Rect& /*bounds*/) {
    paint.setColor(_color);
    paint.setBlendMode(_blendMode);
}

} // namespace snap::drawing
