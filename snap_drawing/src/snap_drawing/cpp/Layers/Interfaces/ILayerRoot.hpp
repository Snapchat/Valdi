//
//  ILayerRoot.hpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/19/22.
//

#pragma once

#include "snap_drawing/cpp/Events/EventCallback.hpp"
#include "snap_drawing/cpp/Events/EventId.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ILayer.hpp"
#include "snap_drawing/cpp/Utils/Duration.hpp"

namespace snap::drawing {

// The resolution the current frame will ultimately be presented/encoded at, in the root's
// coordinate space. May be smaller than a descendant layer's own frame (e.g. a pinch-zoomed
// sticker), which is what lets ExternalLayer avoid rasterizing its external content at a
// resolution higher than what will ever be visible.
struct OutputSize {
    float width = 0;
    float height = 0;
};

class ILayerRoot : public ILayer {
public:
    virtual EventId enqueueEvent(EventCallback&& eventCallback, Duration after) = 0;
    virtual bool cancelEvent(EventId eventId) = 0;

    virtual LayerId allocateLayerId() = 0;

    Ref<ILayer> getParent() const final {
        return nullptr;
    }

    virtual bool shouldRasterizeExternalSurface() const = 0;

    // Defaults to {0, 0} (unknown), in which case callers should treat output size as
    // unconstrained. Roots that draw straight to a fixed-resolution target (e.g. transcoding)
    // should override this so it always reflects the size of the frame currently being drawn.
    virtual OutputSize getOutputSize() const {
        return {};
    }
};

} // namespace snap::drawing
