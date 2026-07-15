//
//  DrawOperation.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 2/16/22.
//

#include "snap_drawing/cpp/Drawing/DrawOperation.hpp"
#include "snap_drawing/cpp/Drawing/Surface/SurfacePresenterManager.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace snap::drawing {

DrawOperation::DrawOperation(const Ref<DisplayList>& displayList,
                             const Ref<SurfacePresenterManager>& surfacePresenterManager,
                             SurfacePresenterList&& surfacePresenters)
    : _displayList(displayList),
      _surfacePresenterManager(surfacePresenterManager),
      _surfacePresenters(std::move(surfacePresenters)),
      _current(_surfacePresenters.begin()) {
    advance();
}

DrawOperation::~DrawOperation() = default;

bool DrawOperation::drawForPresenterId(SurfacePresenterId presenterId, DrawableSurfaceCanvas& canvas) {
    auto* presenter = _surfacePresenters.getForId(presenterId);
    if (_displayList == nullptr || presenter == nullptr) {
        return false;
    }

    _displayList->draw(canvas, presenter->getDisplayListPlaneIndex(), /* shouldClearCanvas */ true);

    return true;
}

Valdi::Result<snap::drawing::GraphicsContext*> DrawOperation::drawNext() {
    const auto& surfacePresenter = *_current;
    _current++;

    advance();

    auto drawableSurface = Ref<DrawableSurface>(surfacePresenter.getDrawableSurface());

    if (drawableSurface == nullptr) {
        return nullptr;
    }

    // A set-but-empty viewport means nothing is on screen (fully scrolled off); the drawable is
    // zero-sized, so skip before preparing a canvas that would just fail every frame.
    if (_displayList->getViewport().isEmpty()) {
        return nullptr;
    }

    VALDI_TRACE("SnapDrawing.drawSurface");

    auto canvasResult = drawableSurface->prepareCanvas();

    if (!canvasResult) {
        return canvasResult.error().rethrow("Failed to prepare canvas on GraphicsContext");
    }

    // Scale against the viewport (the content sub-rect this drawable covers), not the full content
    // size, so a drawable sized to just the visible region renders at native resolution and the
    // translation applied in DisplayList::draw lines the region up. Equals the full-content scale
    // when no viewport is set.
    auto viewport = _displayList->getViewport();
    auto scaleX = static_cast<Scalar>(canvasResult.value().getWidth()) / viewport.width();
    auto scaleY = static_cast<Scalar>(canvasResult.value().getHeight()) / viewport.height();

    _displayList->draw(canvasResult.value(),
                       surfacePresenter.getDisplayListPlaneIndex(),
                       scaleX,
                       scaleY,
                       /* shouldClearCanvas */ true);

    drawableSurface->flush();

    _surfacePresenterManager->onDrawableSurfacePresenterUpdated(surfacePresenter.getId());

    return drawableSurface->getGraphicsContext();
}

bool DrawOperation::hasNext() {
    return _current != _surfacePresenters.end();
}

void DrawOperation::advance() {
    while (_current != _surfacePresenters.end() && !_current->isDrawable()) {
        _current++;
    }
}

} // namespace snap::drawing
