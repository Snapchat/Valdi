//
//  LayerRoot.cpp
//  valdi-skia-apple
//
//  Created by Simon Corsin on 6/27/20.
//

#include "snap_drawing/cpp/Layers/LayerRoot.hpp"

#include "LayerRoot.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

#include "snap_drawing/cpp/Drawing/Composition/Compositor.hpp"
#include "snap_drawing/cpp/Drawing/Composition/CompositorPlaneList.hpp"
#include "snap_drawing/cpp/Drawing/Surface/DrawableSurfaceCanvas.hpp"

#include "snap_drawing/cpp/Touches/DragGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/ScrollGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/TapGestureRecognizer.hpp"

#include "utils/time/StopWatch.hpp"
#include <cstdint>

namespace snap::drawing {

constexpr double kFrameWarningThresholdMs = 100;
constexpr double kTouchRefreshMs = 10;

LayerRoot::LayerRoot(const Ref<Resources>& resources)
    : _resources(resources),
      _touchDispatcher(resources->getLogger(), resources->getGesturesConfiguration().debugGestures),
      _eventQueue(TimePoint(0.0)) {}

LayerRoot::~LayerRoot() = default;

void LayerRoot::onInitialize() {}

void LayerRoot::setListener(LayerRootListener* listener) {
    _listener = listener;
}

LayerRootListener* LayerRoot::getListener() const {
    return _listener;
}

void LayerRoot::setContentLayer(const Valdi::Ref<Layer>& contentLayer, ContentLayerSizingMode sizingMode) {
    if (_contentLayer != contentLayer || _sizingMode != sizingMode) {
        _touchDispatcher.cancelAllGestures();

        if (_contentLayer != nullptr) {
            _contentLayer->onParentChanged(nullptr);
        }

        _contentLayer = contentLayer;
        _sizingMode = sizingMode;

        if (_contentLayer != nullptr) {
            _contentLayer->onParentChanged(Valdi::strongSmallRef(this));
        }

        setChildNeedsDisplay();
        requestLayout(this);
    }
}

const Valdi::Ref<Layer>& LayerRoot::getContentLayer() const {
    return _contentLayer;
}

void LayerRoot::destroy() {
    _listener = nullptr;
    _destroyed = true;
    setContentLayer(nullptr, _sizingMode);
    _eventQueue.clear();
}

void LayerRoot::setSize(Size size, Scalar scale) {
    if (_size != size) {
        _size = size;
        _needsLayout = true;
    }

    if (_scale != scale) {
        _scale = scale;
        setChildNeedsDisplay();
    }

    layoutIfNeeded();
}

void LayerRoot::setRenderViewport(const Rect& viewport) {
    if (_renderViewport != viewport) {
        _renderViewport = viewport;
        setChildNeedsDisplay();
    }
}

void LayerRoot::drawInCanvas(DrawableSurfaceCanvas& canvas) {
    if (_size.width == 0 || _size.height == 0) {
        return;
    }

    auto displayList = draw();

    // Scale against the viewport (the region the canvas covers), consistent with the DrawOperation
    // path. Equals _size when no render viewport is set.
    auto viewport = displayList->getViewport();
    if (viewport.isEmpty()) {
        // Nothing visible -> skip (also avoids dividing by a zero viewport extent below).
        return;
    }
    auto scaleWidth = static_cast<Scalar>(canvas.getWidth()) / viewport.width();
    auto scaleHeight = static_cast<Scalar>(canvas.getHeight()) / viewport.height();

    displayList->draw(canvas, kDisplayListAllPlaneIndexes, scaleWidth, scaleHeight, /* shouldClearCanvas */ true);
}

Ref<DisplayList> LayerRoot::draw() {
    VALDI_TRACE("SnapDrawing.draw");

    snap::utils::time::StopWatch sw;
    sw.start();

    DrawMetrics metrics;
    auto displayList = doDraw(metrics);

    auto elapsed = sw.elapsed();
    if (elapsed.milliseconds() >= kFrameWarningThresholdMs) {
        VALDI_WARN(_resources->getLogger(),
                   "Spent {} to render frame (draw cache hit {}, draw cache miss {})",
                   elapsed.toString(),
                   metrics.visitedLayers - metrics.drawCacheMiss,
                   metrics.drawCacheMiss);
    }

    return displayList;
}

Ref<DisplayList> LayerRoot::doDraw(DrawMetrics& metrics) {
    auto displayList =
        Valdi::makeShared<DisplayList>(_size, _lastAbsoluteFrameTime ? _lastAbsoluteFrameTime.value() : TimePoint(0.0));

    if (_contentLayer != nullptr) {
        _contentLayer->draw(*displayList, metrics);
    }

    if (_planeList == nullptr) {
        _planeList = std::make_unique<CompositorPlaneList>();
    } else {
        _planeList->clear();
    }

    Compositor compositor(_resources->getLogger());
    auto composedDisplayList = compositor.performComposition(*displayList, *_planeList);

    // Propagate the visible viewport so the drawable is rasterized for the on-screen region only.
    // Only when the host has set one -- otherwise the display list stays unset (full content), so
    // paths that never set a viewport (e.g. offscreen raster) are unchanged.
    if (_renderViewport.has_value()) {
        composedDisplayList->setViewport(*_renderViewport);
    }

    return composedDisplayList;
}

void LayerRoot::setChildNeedsDisplay() {
    if (!_needsDisplay) {
        _needsDisplay = true;
        enqueueFrame();
    }
}

bool LayerRoot::needsDisplay() const {
    return _needsDisplay;
}

void LayerRoot::requestLayout(ILayer* /*layer*/) {
    if (!_needsLayout) {
        _needsLayout = true;
        enqueueFrame();
    }
}

void LayerRoot::requestFocus(ILayer* /*layer*/) {
    // no-op by default
}

bool LayerRoot::dispatchTouchEvent(const TouchEvent& event) {
    if (_contentLayer == nullptr || _touchDispatcher.isDispatchingEvent()) {
        return false;
    }

    auto processed = _touchDispatcher.dispatchEvent(event, _contentLayer);

    if (!_touchDispatcher.isEmpty()) {
        enqueueFrame();
    }

    return processed;
}

GestureTypes LayerRoot::getGesturesTypesForTouchEvent(const TouchEvent& event) const {
    if (_contentLayer == nullptr) {
        return GestureTypes();
    }

    auto gestureCandidates = _touchDispatcher.getGestureCandidatesForEvent(event, _contentLayer);

    GestureTypes types;

    for (const auto& gesture : gestureCandidates) {
        if (Valdi::castOrNull<TapGestureRecognizer>(gesture) != nullptr) {
            types.hasTap = true;
        } else if (Valdi::castOrNull<ScrollGestureRecognizer>(gesture) != nullptr) {
            types.hasScroll = true;
        } else if (Valdi::castOrNull<DragGestureRecognizer>(gesture) != nullptr) {
            types.hasDrag = true;
        }
    }

    return types;
}

bool LayerRoot::refreshTouches(const TimePoint& currentTime) {
    if (_touchDispatcher.isEmpty()) {
        return false;
    }
    if (!_touchDispatcher.getLastEvent()) {
        return false;
    }

    const auto& lastEvent = _touchDispatcher.getLastEvent().value();

    auto offsetSinceLastEvent = currentTime - lastEvent.getTime();

    if (offsetSinceLastEvent.milliseconds() < kTouchRefreshMs) {
        // Only refresh gestures if there wasn't another event that came over 10ms ago
        return false;
    }

    const auto wasInteraction =
        (lastEvent.getType() == TouchEventTypeDown || lastEvent.getType() == TouchEventTypeMoved ||
         lastEvent.getType() == TouchEventTypeIdle || lastEvent.getType() == TouchEventTypePointerDown ||
         lastEvent.getType() == TouchEventTypePointerUp);
    const auto newType = wasInteraction ? TouchEventTypeIdle : TouchEventTypeNone;

    dispatchTouchEvent(TouchEvent(newType,
                                  lastEvent.getLocationInWindow(),
                                  lastEvent.getLocation(),
                                  lastEvent.getDirection(),
                                  lastEvent.getPointerCount(),
                                  lastEvent.getActionIndex(),
                                  lastEvent.getPointerLocations(),
                                  currentTime,
                                  lastEvent.getOffsetSinceSource() + offsetSinceLastEvent,
                                  lastEvent.getSource()));
    return true;
}

EventId LayerRoot::enqueueEvent(EventCallback&& eventCallback, Duration after) {
    auto eventId = _eventQueue.enqueue(after, std::move(eventCallback));
    enqueueFrame();
    return eventId;
}

bool LayerRoot::cancelEvent(EventId eventId) {
    return _eventQueue.cancel(eventId);
}

void LayerRoot::enqueueFrame() {
    if (canEnqueueFrame() && _listener != nullptr) {
        _didEnqueueFrame = true;
        _listener->onNeedsProcessFrame(*this);
    }
}

void LayerRoot::layoutIfNeeded() {
    if (needsLayout()) {
        VALDI_TRACE("SnapDrawing.layout");
        _needsLayout = false;

        Size resolvedSize;

        if (_sizingMode == ContentLayerSizingModeMinSize) {
            resolvedSize = _contentLayer->sizeThatFits(_size);
        } else {
            resolvedSize = _size;
        }

        _contentLayer->setFrame(Rect::makeXYWH(0, 0, resolvedSize.width, resolvedSize.height));
        _contentLayer->layoutIfNeeded();
    }
}

TimePoint LayerRoot::updateFrameTime(TimePoint absoluteFrameTime) {
    if (!_initialAbsoluteFrameTime) {
        _initialAbsoluteFrameTime = {absoluteFrameTime};
    }
    _lastAbsoluteFrameTime = {absoluteFrameTime};

    return TimePoint((absoluteFrameTime - _initialAbsoluteFrameTime.value()).seconds());
}

TimePoint LayerRoot::getFrameTimeForAbsoluteFrameTime(TimePoint absoluteFrameTime) const {
    if (!_initialAbsoluteFrameTime) {
        return TimePoint(0.0);
    }

    return TimePoint((absoluteFrameTime - _initialAbsoluteFrameTime.value()).seconds());
}

uint64_t LayerRoot::allocateLayerId() {
    return ++_layerIdSequence;
}

void LayerRoot::processFrame(TimePoint absoluteFrameTime) {
    if (_destroyed) {
        return;
    }

    _processingFrame = true;

    VALDI_TRACE("SnapDrawing.processFrame");

    auto frameTime = updateFrameTime(absoluteFrameTime);

    layoutIfNeeded();

    {
        VALDI_TRACE("SnapDrawing.flushEvents");
        refreshTouches(frameTime);
        _eventQueue.flush(frameTime);
    }

    Ref<DisplayList> displayList;

    if (_needsDisplay) {
        _needsDisplay = false;
        displayList = draw();
        _lastDrawnFrame = displayList;
    }

    _didEnqueueFrame = false;
    _processingFrame = false;

    if (displayList != nullptr && _listener != nullptr) {
        _listener->onDidDraw(*this, displayList, _planeList.get());
    }

    if (needsProcessFrame()) {
        enqueueFrame();
    }
}

bool LayerRoot::needsProcessFrame() const {
    return _didEnqueueFrame || _needsDisplay || needsLayout() || !_eventQueue.isEmpty() || !_touchDispatcher.isEmpty();
}

bool LayerRoot::needsLayout() const {
    return _needsLayout && _contentLayer != nullptr;
}

bool LayerRoot::canEnqueueFrame() const {
    return !_didEnqueueFrame && !_processingFrame && !_destroyed;
}

const Ref<DisplayList>& LayerRoot::getLastDrawnFrame() const {
    return _lastDrawnFrame;
}

const std::optional<TimePoint>& LayerRoot::getLastAbsoluteFrameTime() const {
    return _lastAbsoluteFrameTime;
}

TouchDispatcher& LayerRoot::getTouchDispatcher() {
    return _touchDispatcher;
}

Scalar LayerRoot::getScale() const {
    return _scale;
}

const Ref<Resources>& LayerRoot::getResources() const {
    return _resources;
}

bool LayerRoot::shouldRasterizeExternalSurface() const {
    return false;
}

} // namespace snap::drawing
