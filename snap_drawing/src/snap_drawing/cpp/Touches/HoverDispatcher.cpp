#include "snap_drawing/cpp/Touches/HoverDispatcher.hpp"

namespace snap::drawing {

// Bounds how many times listeners that move the pointer again can make a single dispatch start over.
static constexpr int kMaxNestedUpdates = 8;

void HoverDispatcher::dispatch(std::optional<Point> location, const Valdi::Ref<Layer>& rootLayer) {
    _location = location;
    refresh(rootLayer);
}

bool HoverDispatcher::refresh(const Valdi::Ref<Layer>& rootLayer) {
    if (_dispatching) {
        // A listener moved the pointer or changed the tree; finish notifying before starting over.
        _needsUpdate = true;
        return false;
    }

    _dispatching = true;
    auto notified = false;
    auto updates = 0;
    do {
        _needsUpdate = false;
        notified = update(rootLayer) || notified;
    } while (_needsUpdate && ++updates < kMaxNestedUpdates);
    _dispatching = false;
    return notified;
}

static Valdi::Ref<Layer> findHoverTarget(const Valdi::Ref<Layer>& rootLayer, const Point& location) {
    auto layer = rootLayer->getLayerAtPoint(location);
    while (layer != nullptr) {
        if (layer->hasHoverListener()) {
            return layer;
        }
        if (layer == rootLayer) {
            break;
        }
        layer = Valdi::castOrNull<Layer>(layer->getParent());
    }
    return nullptr;
}

bool HoverDispatcher::update(const Valdi::Ref<Layer>& rootLayer) {
    Valdi::Ref<Layer> target;
    auto targetLocation = Point::makeEmpty();
    if (_location && rootLayer != nullptr) {
        target = findHoverTarget(rootLayer, _location.value());
        if (target != nullptr) {
            targetLocation = rootLayer->convertPointToLayer(_location.value(), target).value_or(targetLocation);
        }
    }

    Valdi::Ref<Layer> previous = _hoveredLayer.lock();
    if (previous == target) {
        if (target != nullptr && targetLocation != _hoveredLocation) {
            _hoveredLocation = targetLocation;
            target->notifyHover(HoverEvent{HoverEventTypeMoved, targetLocation});
            return true;
        }
        return false;
    }

    auto previousLocation = _hoveredLocation;
    _hoveredLayer = target;
    _hoveredLocation = targetLocation;

    if (previous != nullptr) {
        previous->notifyHover(HoverEvent{HoverEventTypeExited, previousLocation});
    }
    if (target != nullptr) {
        target->notifyHover(HoverEvent{HoverEventTypeEntered, targetLocation});
    }
    return true;
}

bool HoverDispatcher::hasPendingUpdate() const {
    return _needsUpdate;
}

bool HoverDispatcher::hasPointer() const {
    return _location.has_value();
}

Valdi::Ref<Layer> HoverDispatcher::getHoveredLayer() const {
    return _hoveredLayer.lock();
}

} // namespace snap::drawing
