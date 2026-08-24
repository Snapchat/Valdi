//
//  BridgeLayer.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/20/20.
//

#include "valdi/snap_drawing/Layers/BridgeLayer.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

#include "snap_drawing/cpp/Touches/GestureRecognizer.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"

#include "utils/time/StopWatch.hpp"

#include <limits>

namespace snap::drawing {

class BridgeGestureRecognizer : public GestureRecognizer {
public:
    BridgeGestureRecognizer() {
        setShouldCancelOtherGesturesOnStart(true);
        // Replicates behavior of Android's TouchDispatcher
        setShouldProcessBeforeOtherGestures(true);
    }

    ~BridgeGestureRecognizer() override = default;

    bool canRecognizeSimultaneously(const GestureRecognizer& /*other*/) const override {
        return false;
    }

protected:
    void onUpdate(const TouchEvent& event) override {
        auto bridgeLayer = Valdi::castOrNull<BridgeLayer>(getLayer());
        _lastForwardedEvent = {event};

        if (event.getType() == TouchEventTypeNone) {
            transitionToState(GestureRecognizerStateFailed);
            return;
        }

        auto shouldConsumeEvent = bridgeLayer->forwardTouchEvent(event, event.getType(), false);

        if (event.getType() == TouchEventTypeUp) {
            if (isActive() || shouldConsumeEvent) {
                transitionToState(GestureRecognizerStateEnded);
            } else {
                transitionToState(GestureRecognizerStateFailed);
            }
        } else if (shouldConsumeEvent && getState() == GestureRecognizerStatePossible) {
            // Trigger a start of the gesture, so that the other gestures will be canceled
            transitionToState(GestureRecognizerStateBegan);
        }
    }

    bool shouldBegin() override {
        return true;
    }

    void onProcess() override {}

    void onReset() override {
        auto bridgeLayer = Valdi::castOrNull<BridgeLayer>(getLayer());
        if (bridgeLayer != nullptr && _lastForwardedEvent) {
            auto event = std::move(_lastForwardedEvent.value());
            _lastForwardedEvent = std::nullopt;

            bridgeLayer->forwardTouchEvent(event, TouchEventTypeNone, false);
        }
    }

    std::string_view getTypeName() const final {
        return "bridge";
    }

private:
    std::optional<TouchEvent> _lastForwardedEvent;
};

BridgeLayer::BridgeLayer(const Ref<Resources>& resources) : ExternalLayer(resources) {}

BridgeLayer::~BridgeLayer() = default;

void BridgeLayer::onInitialize() {
    Layer::onInitialize();
    addGestureRecognizer(Valdi::makeShared<BridgeGestureRecognizer>());
    setNeedsLayout();
}

void BridgeLayer::setAttachedData(const Ref<Valdi::RefCountable>& attachedData) {
    Layer::setAttachedData(attachedData);

    auto viewNode = valdiViewNodeFromLayer(*this);
    if (viewNode != nullptr) {
        auto bridgedView = getBridgedView();
        if (bridgedView != nullptr) {
            bridgedView->getViewTransaction(viewNode->getViewNodeTree())
                .moveViewToTree(bridgedView->getView(), viewNode->getViewNodeTree(), viewNode.get());
            // Views created without a tree (preloaded, pooled) get their manager here, so the
            // pre-raster fence applies once a node adopts them.
            bridgedView->setMainThreadManager(viewNode->getViewNodeTree()->getMainThreadManager());
        }
    }
}

bool BridgeLayer::prepareForReuse() {
    auto shouldReuse = Layer::prepareForReuse();

    auto viewNode = valdiViewNodeFromLayer(*this);
    auto bridgedView = getBridgedView();
    if (viewNode != nullptr && bridgedView != nullptr) {
        bridgedView->getViewTransaction(viewNode->getViewNodeTree())
            .willEnqueueViewToPool(bridgedView->getView(), [](auto& /*view*/) {});
    }

    setNeedsLayout();

    return shouldReuse;
}

void BridgeLayer::onLayout() {
    ExternalLayer::onLayout();

    if (shouldRasterizeExternalSurface()) {
        auto viewNode = valdiViewNodeFromLayer(*this);
        auto bridgedView = getBridgedView();
        if (bridgedView != nullptr && viewNode != nullptr) {
            auto viewTransaction = bridgedView->getViewManager().createViewTransaction(
                viewNode->getViewNodeTree()->getMainThreadManager(), false);
            viewTransaction->setViewFrame(
                bridgedView->getView(), Valdi::Frame(0, 0, getFrame().width(), getFrame().height()), false, nullptr);
            viewTransaction->flush(/* sync */ false);
        }
    }
}

Valdi::Ref<BridgedView> BridgeLayer::getBridgedView() const {
    return Valdi::castOrNull<BridgedView>(getExternalSurface());
}

Valdi::Ref<Valdi::View> BridgeLayer::getView() const {
    auto bridgedView = getBridgedView();
    if (bridgedView == nullptr) {
        return nullptr;
    }
    return bridgedView->getView();
}

void BridgeLayer::setBridgedView(const Valdi::Ref<BridgedView>& bridgedView) {
    setExternalSurface(bridgedView);
}

bool BridgeLayer::forwardTouchEvent(const TouchEvent& currentEvent, TouchEventType type, bool isTouchOwner) const {
    auto view = getView();
    if (view == nullptr) {
        return false;
    }

    auto currentEventLocation = currentEvent.getLocation();
    auto currentEventDirection = currentEvent.getDirection();

    return view->onTouchEvent(static_cast<long>(currentEvent.getOffsetSinceSource().milliseconds()),
                              static_cast<int>(type),
                              static_cast<float>(currentEvent.getLocationInWindow().x),
                              static_cast<float>(currentEvent.getLocationInWindow().y),
                              static_cast<float>(currentEventLocation.x),
                              static_cast<float>(currentEventLocation.y),
                              static_cast<float>(currentEventDirection.dx),
                              static_cast<float>(currentEventDirection.dy),
                              isTouchOwner,
                              currentEvent.getSource());
}

bool BridgeLayer::hitTest(const Point& point) const {
    auto view = getView();
    if (view == nullptr) {
        return false;
    }

    switch (view->hitTest(point.x, point.y)) {
        case Valdi::ViewHitTestResultUseDefault:
            return ExternalLayer::hitTest(point);
        case Valdi::ViewHitTestResultHit:
            return true;
        case Valdi::ViewHitTestResultNotHit:
            return false;
    }
}

} // namespace snap::drawing
