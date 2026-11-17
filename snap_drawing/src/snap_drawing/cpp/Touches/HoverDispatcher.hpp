#pragma once

#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Touches/HoverEvent.hpp"

#include <optional>

namespace snap::drawing {

/**
 Tracks which layer a hovering pointer (a mouse, or a hand ray) is over and notifies the layers' hover listeners.

 The hovered layer is the innermost layer with a hover listener along the path touches would hit: the same
 hitTest and coordinate conversions as TouchDispatcher, so a layer covering another blocks hover as it blocks touches.
 Only that layer is hovered. An ancestor with its own listener is exited when the pointer moves onto it.

 Hover never takes part in gesture recognition: it captures no gesture recognizers and leaves touches untouched.
 */
class HoverDispatcher {
public:
    /**
     Moves the pointer to the given location in rootLayer's coordinates, or takes it away when nullopt.
     */
    void dispatch(std::optional<Point> location, const Valdi::Ref<Layer>& rootLayer);

    /**
     Hit tests the last location again, for when the tree changed under a pointer that didn't move. Returns whether
     any listener was notified, as listeners can change the tree again.
     */
    bool refresh(const Valdi::Ref<Layer>& rootLayer);

    /**
     Whether listeners kept moving the pointer or changing the tree past what one dispatch handles, so it should be
     refreshed again.
     */
    bool hasPendingUpdate() const;

    /**
     Whether a pointer is present, hovering a layer or not.
     */
    bool hasPointer() const;

    Valdi::Ref<Layer> getHoveredLayer() const;

private:
    std::optional<Point> _location;
    Valdi::Weak<Layer> _hoveredLayer;
    Point _hoveredLocation = Point::makeEmpty();
    bool _dispatching = false;
    bool _needsUpdate = false;

    bool update(const Valdi::Ref<Layer>& rootLayer);
};

} // namespace snap::drawing
