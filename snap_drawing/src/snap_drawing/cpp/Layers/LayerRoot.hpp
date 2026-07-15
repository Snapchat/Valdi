//
//  LayerRoot.hpp
//  valdi-skia-apple
//
//  Created by Simon Corsin on 6/27/20.
//

#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"

#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Interfaces/IMainThreadDispatcher.hpp"

#include "snap_drawing/cpp/Events/EventQueue.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ILayerRoot.hpp"
#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Touches/TouchDispatcher.hpp"
#include "snap_drawing/cpp/Touches/TouchEvent.hpp"
#include "snap_drawing/cpp/Utils/TimePoint.hpp"

#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"
#include <cstdint>
#include <optional>

namespace snap::drawing {

class LayerRoot;
class DrawableSurfaceCanvas;
class CompositorPlaneList;

class LayerRootListener {
public:
    virtual ~LayerRootListener() = default;

    /**
     Called whenever the layer tree requires a call to processFrame().
     */
    virtual void onNeedsProcessFrame(LayerRoot& root) = 0;

    /**
     Called whenever a frame was drawn during a processFrame() call.
     The given DisplayList contains the draw operations for the whole tree and
     can be drawn into a RenderTarget.
     */
    virtual void onDidDraw(LayerRoot& root,
                           const Ref<DisplayList>& displayList,
                           const CompositorPlaneList* planeList) = 0;
};

enum ContentLayerSizingMode {
    // ContentLayer will have the minimum size possible
    // for the given size constraints
    ContentLayerSizingModeMinSize,
    // ContentLayer will have the same size as the given
    // size constraints
    ContentLayerSizingModeMatchSize,
};

struct GestureTypes {
    bool hasTap = false;
    bool hasScroll = false;
    bool hasDrag = false;
};

class LayerRoot : public ILayerRoot {
public:
    explicit LayerRoot(const Ref<Resources>& resources);
    ~LayerRoot() override;

    void onInitialize() override;

    void setContentLayer(const Valdi::Ref<Layer>& contentLayer, ContentLayerSizingMode sizingMode);
    const Valdi::Ref<Layer>& getContentLayer() const;

    bool dispatchTouchEvent(const TouchEvent& event);
    GestureTypes getGesturesTypesForTouchEvent(const TouchEvent& event) const;
    bool refreshTouches(const TimePoint& currentTime);

    void setSize(Size size, Scalar scale);

    // The visible sub-rect of the content, in content coordinates. When the content is laid out
    // much larger than what is actually on screen (e.g. a pinch-zoomed layer), the host sets this
    // to the on-screen region so the drawable is sized to it instead of to the full content. Empty
    // (default) renders the whole content. Only affects rasterization -- layout and touch handling
    // continue to operate in full content coordinates.
    void setRenderViewport(const Rect& viewport);

    void setListener(LayerRootListener* listener);
    LayerRootListener* getListener() const;

    void setChildNeedsDisplay() override;
    bool needsDisplay() const;

    void requestLayout(ILayer* layer) override;
    EventId enqueueEvent(EventCallback&& eventCallback, Duration after) override;
    bool cancelEvent(EventId eventId) override;

    void requestFocus(ILayer* layer) override;

    uint64_t allocateLayerId() override;

    TouchDispatcher& getTouchDispatcher();

    void destroy();

    Scalar getScale() const;

    void processFrame(TimePoint absoluteFrameTime);

    const std::optional<TimePoint>& getLastAbsoluteFrameTime() const;
    const Ref<DisplayList>& getLastDrawnFrame() const;

    Ref<DisplayList> draw();

    void drawInCanvas(DrawableSurfaceCanvas& canvas);

    bool needsProcessFrame() const;

    const Ref<Resources>& getResources() const;

    bool shouldRasterizeExternalSurface() const override;

    inline Scalar sanitizeCoordinate(Scalar value) const {
        return snap::drawing::sanitizeScalarFromScale(value, _scale);
    }

    TimePoint getFrameTimeForAbsoluteFrameTime(TimePoint absoluteFrameTime) const;

private:
    Ref<Resources> _resources;
    LayerRootListener* _listener = nullptr;
    TouchDispatcher _touchDispatcher;
    Valdi::Ref<Layer> _contentLayer;
    EventQueue _eventQueue;
    Size _size = Size::makeEmpty();
    // Unset -> render the full content. Set (possibly empty) -> only that region is rasterized;
    // an empty value means nothing is on screen. See DisplayList::setViewport.
    std::optional<Rect> _renderViewport;
    Scalar _scale = 1;
    uint64_t _layerIdSequence = 0;
    bool _needsDisplay = false;
    bool _needsLayout = true;
    bool _didEnqueueFrame = false;
    bool _destroyed = false;
    bool _processingFrame = false;
    ContentLayerSizingMode _sizingMode = ContentLayerSizingModeMinSize;
    std::optional<TimePoint> _initialAbsoluteFrameTime;
    std::optional<TimePoint> _lastAbsoluteFrameTime;
    std::unique_ptr<CompositorPlaneList> _planeList;
    Ref<DisplayList> _lastDrawnFrame;

    bool needsLayout() const;

    void enqueueFrame();

    void layoutIfNeeded();

    bool canEnqueueFrame() const;

    Ref<DisplayList> doDraw(DrawMetrics& metrics);

    TimePoint updateFrameTime(TimePoint absoluteFrameTime);
};

} // namespace snap::drawing
