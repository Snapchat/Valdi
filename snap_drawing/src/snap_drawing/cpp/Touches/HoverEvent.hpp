#pragma once

#include "valdi_core/cpp/Utils/Function.hpp"

#include "snap_drawing/cpp/Utils/Geometry.hpp"

namespace snap::drawing {

class Layer;

enum HoverEventType {
    HoverEventTypeEntered, // the pointer started hovering the layer
    HoverEventTypeMoved,   // the pointer moved while hovering the layer
    HoverEventTypeExited,  // the pointer stopped hovering the layer
};

struct HoverEvent {
    HoverEventType type;
    // In the coordinates of the layer receiving the event. For HoverEventTypeExited, the last location the layer
    // was told about.
    Point location;
    // Only set on an exit caused by clearing the listener.
    bool detached = false;
};

using HoverListener = Valdi::Function<void(Layer&, const HoverEvent&)>;

} // namespace snap::drawing
