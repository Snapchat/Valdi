#include <gtest/gtest.h>

#include "TestGestureUtils.hpp"

#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"

#include <memory>
#include <utility>
#include <vector>

using namespace Valdi;

namespace snap::drawing {

struct HoverRecord {
    const Layer* layer;
    HoverEventType type;
    Point location;

    bool operator==(const HoverRecord& other) const {
        return layer == other.layer && type == other.type && location == other.location;
    }
};

static void PrintTo(const HoverRecord& record, std::ostream* os) {
    static constexpr const char* kTypes[] = {"Entered", "Moved", "Exited"};
    *os << kTypes[record.type] << " on " << record.layer << " at (" << record.location.x << ", " << record.location.y
        << ")";
}

static HoverRecord entered(const Ref<Layer>& layer, Scalar x, Scalar y) {
    return HoverRecord{layer.get(), HoverEventTypeEntered, Point::make(x, y)};
}

static HoverRecord moved(const Ref<Layer>& layer, Scalar x, Scalar y) {
    return HoverRecord{layer.get(), HoverEventTypeMoved, Point::make(x, y)};
}

static HoverRecord exited(const Ref<Layer>& layer, Scalar x, Scalar y) {
    return HoverRecord{layer.get(), HoverEventTypeExited, Point::make(x, y)};
}

class CountingLayerRootListener : public LayerRootListener {
public:
    int frameRequests = 0;

    void onNeedsProcessFrame(LayerRoot& /*root*/) override {
        frameRequests++;
    }

    void onDidDraw(LayerRoot& /*root*/,
                   const Ref<DisplayList>& /*displayList*/,
                   const CompositorPlaneList* /*planeList*/) override {}
};

class HoverDispatcherTest : public ::testing::Test {
protected:
    CountingLayerRootListener rootListener;
    Ref<Resources> resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());
    Ref<LayerRoot> root = makeShared<LayerRoot>(resources);
    Ref<Layer> rootView = createView(0, 0, 100, 100);
    std::vector<HoverRecord> events;

    void SetUp() override {
        root->setContentLayer(rootView, ContentLayerSizingModeMatchSize);
        root->setSize(Size::make(100, 100), 1);
        root->setListener(&rootListener);
        settle();
    }

    void TearDown() override {
        root->setListener(nullptr);
    }

    void listen(const Ref<Layer>& layer) {
        layer->setHoverListener([this](Layer& hovered, const HoverEvent& event) {
            events.emplace_back(HoverRecord{&hovered, event.type, event.location});
        });
    }

    void hover(Scalar x, Scalar y) {
        root->dispatchHoverEvent(Point::make(x, y));
    }

    void leave() {
        root->dispatchHoverEvent(std::nullopt);
    }

    void processFrame() {
        root->processFrame(TimePoint::fromSeconds(0));
    }

    // Processes frames until none is requested.
    void settle() {
        for (int i = 0; i < 10; i++) {
            processFrame();
            if (!root->needsProcessFrame()) {
                return;
            }
        }
        FAIL() << "frames kept being requested";
    }

    std::vector<HoverRecord> takeEvents() {
        return std::exchange(events, {});
    }
};

TEST_F(HoverDispatcherTest, entersMovesAndExits) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), takeEvents());
    ASSERT_EQ(child, root->getHoverDispatcher().getHoveredLayer());

    hover(16, 20);
    ASSERT_EQ(std::vector<HoverRecord>({moved(child, 6, 10)}), takeEvents());

    hover(16, 20);
    ASSERT_TRUE(takeEvents().empty());

    hover(80, 80);
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 6, 10)}), takeEvents());
    ASSERT_EQ(nullptr, root->getHoverDispatcher().getHoveredLayer());

    hover(15, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), takeEvents());

    leave();
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), takeEvents());
    ASSERT_FALSE(root->getHoverDispatcher().hasPointer());

    processFrame();
    ASSERT_TRUE(takeEvents().empty());
}

TEST_F(HoverDispatcherTest, topmostSiblingWins) {
    auto bottom = createView(10, 10, 50, 50);
    auto top = createView(10, 10, 50, 50);
    rootView->addChild(bottom);
    rootView->addChild(top);
    listen(bottom);
    listen(top);

    hover(20, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(top, 10, 10)}), takeEvents());
}

TEST_F(HoverDispatcherTest, coveringSiblingBlocksHoverUnlessTouchDisabled) {
    auto bottom = createView(10, 10, 50, 50);
    auto top = createView(10, 10, 50, 50);
    rootView->addChild(bottom);
    rootView->addChild(top);
    listen(bottom);

    hover(20, 20);
    ASSERT_TRUE(takeEvents().empty());

    top->setTouchEnabled(false);
    processFrame();
    ASSERT_EQ(std::vector<HoverRecord>({entered(bottom, 10, 10)}), takeEvents());

    top->setTouchEnabled(true);
    processFrame();
    ASSERT_EQ(std::vector<HoverRecord>({exited(bottom, 10, 10)}), takeEvents());
}

TEST_F(HoverDispatcherTest, onlyInnermostListenerIsHovered) {
    auto parent = createView(10, 10, 60, 60);
    auto child = createView(10, 10, 20, 20);
    rootView->addChild(parent);
    parent->addChild(child);
    listen(parent);
    listen(child);

    hover(15, 15);
    ASSERT_EQ(std::vector<HoverRecord>({entered(parent, 5, 5)}), takeEvents());

    hover(25, 25);
    ASSERT_EQ(std::vector<HoverRecord>({exited(parent, 5, 5), entered(child, 5, 5)}), takeEvents());

    hover(26, 25);
    ASSERT_EQ(std::vector<HoverRecord>({moved(child, 6, 5)}), takeEvents());

    hover(15, 15);
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 6, 5), entered(parent, 5, 5)}), takeEvents());
}

TEST_F(HoverDispatcherTest, hoversNearestAncestorWithListener) {
    auto parent = createView(10, 10, 60, 60);
    auto child = createView(10, 10, 20, 20);
    rootView->addChild(parent);
    parent->addChild(child);
    listen(parent);

    hover(25, 25);
    ASSERT_EQ(std::vector<HoverRecord>({entered(parent, 15, 15)}), takeEvents());

    hover(26, 25);
    ASSERT_EQ(std::vector<HoverRecord>({moved(parent, 16, 15)}), takeEvents());
}

TEST_F(HoverDispatcherTest, followsTranslation) {
    auto child = createView(10, 10, 20, 20);
    child->setTranslationX(5);
    child->setTranslationY(5);
    rootView->addChild(child);
    listen(child);

    hover(12, 12);
    ASSERT_TRUE(takeEvents().empty());

    hover(30, 30);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 15, 15)}), takeEvents());
}

TEST_F(HoverDispatcherTest, followsScale) {
    // Scaled 2x about its center (40, 40), so it covers (0, 0, 80, 80) on screen.
    auto scaled = createView(20, 20, 40, 40);
    scaled->setScaleX(2);
    scaled->setScaleY(2);
    auto leaf = createView(5, 5, 10, 10);
    rootView->addChild(scaled);
    scaled->addChild(leaf);
    listen(scaled);

    // Outside the unscaled frame, inside the scaled one.
    hover(10, 10);
    ASSERT_EQ(std::vector<HoverRecord>({entered(scaled, 5, 5)}), takeEvents());

    listen(leaf);
    hover(20, 20);
    ASSERT_EQ(std::vector<HoverRecord>({exited(scaled, 5, 5), entered(leaf, 5, 5)}), takeEvents());

    hover(90, 90);
    ASSERT_EQ(std::vector<HoverRecord>({exited(leaf, 5, 5)}), takeEvents());
}

TEST_F(HoverDispatcherTest, followsScrollingUnderStillPointerAndClipsToScrollBounds) {
    auto scroll = makeLayer<ScrollLayer>(resources);
    scroll->setFrame(Rect::makeXYWH(0, 0, 100, 50));
    scroll->setContentSize(Size::make(100, 200));
    rootView->addChild(scroll);

    std::vector<Ref<Layer>> rows;
    for (int i = 0; i < 8; i++) {
        auto row = createView(0, static_cast<Scalar>(i * 25), 100, 25);
        scroll->getContentLayer()->addChild(row);
        listen(row);
        rows.emplace_back(row);
    }

    hover(50, 10);
    ASSERT_EQ(std::vector<HoverRecord>({entered(rows[0], 50, 10)}), takeEvents());

    scroll->setContentOffset(Point::make(0, 50), Vector::makeEmpty(), false);
    processFrame();
    ASSERT_EQ(std::vector<HoverRecord>({exited(rows[0], 50, 10), entered(rows[2], 50, 10)}), takeEvents());

    // Below the scroll's bounds, where the scrolled content continues but can't be seen or touched.
    hover(50, 60);
    ASSERT_EQ(std::vector<HoverRecord>({exited(rows[2], 50, 10)}), takeEvents());
}

TEST_F(HoverDispatcherTest, skipsInvisibleLayers) {
    auto bottom = createView(10, 10, 50, 50);
    auto top = createView(10, 10, 50, 50);
    rootView->addChild(bottom);
    rootView->addChild(top);
    listen(bottom);
    listen(top);

    hover(20, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(top, 10, 10)}), takeEvents());

    top->setOpacity(0);
    processFrame();
    ASSERT_EQ(std::vector<HoverRecord>({exited(top, 10, 10), entered(bottom, 10, 10)}), takeEvents());
}

TEST_F(HoverDispatcherTest, exitsLayerRemovedWhileHovered) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), takeEvents());

    child->removeFromParent();
    processFrame();
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), takeEvents());
    ASSERT_EQ(nullptr, root->getHoverDispatcher().getHoveredLayer());
}

TEST_F(HoverDispatcherTest, forgetsLayerReleasedWhileHovered) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    takeEvents();

    child->removeFromParent();
    child = nullptr;
    processFrame();
    ASSERT_TRUE(takeEvents().empty());
    ASSERT_EQ(nullptr, root->getHoverDispatcher().getHoveredLayer());
}

TEST_F(HoverDispatcherTest, releasesListenerOfLayerFreedWhileHovered) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    auto token = std::make_shared<int>(0);
    child->setHoverListener([token](Layer& /*hovered*/, const HoverEvent& /*event*/) {});

    hover(15, 20);
    ASSERT_EQ(2, token.use_count());

    child->removeFromParent();
    child = nullptr;
    ASSERT_EQ(1, token.use_count());
}

TEST_F(HoverDispatcherTest, entersOnlyTheListenerTheTargetHasAfterTheExit) {
    auto left = createView(10, 10, 20, 20);
    auto right = createView(50, 10, 20, 20);
    rootView->addChild(left);
    rootView->addChild(right);
    listen(right);
    std::vector<HoverRecord> replacementEvents;
    left->setHoverListener([&](Layer& hovered, const HoverEvent& event) {
        events.emplace_back(HoverRecord{&hovered, event.type, event.location});
        if (event.type == HoverEventTypeExited) {
            right->setHoverListener([&](Layer& replaced, const HoverEvent& replacedEvent) {
                replacementEvents.emplace_back(HoverRecord{&replaced, replacedEvent.type, replacedEvent.location});
            });
        }
    });

    hover(15, 15);
    takeEvents();
    hover(55, 15);
    settle();
    // right's first listener was replaced before it could be entered, so it hears nothing.
    ASSERT_EQ(std::vector<HoverRecord>({exited(left, 5, 5)}), takeEvents());
    ASSERT_EQ(std::vector<HoverRecord>({entered(right, 5, 5)}), replacementEvents);

    leave();
    ASSERT_EQ(std::vector<HoverRecord>({entered(right, 5, 5), exited(right, 5, 5)}), replacementEvents);
}

TEST_F(HoverDispatcherTest, movesToAncestorWhenListenerIsCleared) {
    auto parent = createView(10, 10, 60, 60);
    auto child = createView(10, 10, 20, 20);
    rootView->addChild(parent);
    parent->addChild(child);
    listen(parent);
    listen(child);

    hover(25, 25);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 5)}), takeEvents());

    child->setHoverListener(HoverListener());
    processFrame();
    // The cleared listener heard the pointer arrive, so it hears it leave.
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 5), entered(parent, 15, 15)}), takeEvents());
}

TEST_F(HoverDispatcherTest, exitsClearedListenerWhenPointerLeaves) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    takeEvents();

    child->setHoverListener(HoverListener());
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), takeEvents());

    leave();
    ASSERT_TRUE(takeEvents().empty());
}

TEST_F(HoverDispatcherTest, replacingListenerKeepsHoverUnderStillPointer) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), takeEvents());

    std::vector<HoverRecord> replacementEvents;
    child->setHoverListener([&](Layer& layer, const HoverEvent& event) {
        replacementEvents.emplace_back(HoverRecord{&layer, event.type, event.location});
    });
    settle();
    ASSERT_TRUE(takeEvents().empty());
    ASSERT_TRUE(replacementEvents.empty());

    leave();
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), replacementEvents);
}

TEST_F(HoverDispatcherTest, entersNewListenerOfReusedLayerUnderStillPointer) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), takeEvents());

    // As a view pool hands the layer to a new element within one render: reset, then the new element's listener.
    child->setHoverListener(HoverListener());
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), takeEvents());
    std::vector<HoverRecord> newListenerEvents;
    child->setHoverListener([&](Layer& hovered, const HoverEvent& event) {
        newListenerEvents.emplace_back(HoverRecord{&hovered, event.type, event.location});
    });
    settle();
    ASSERT_TRUE(takeEvents().empty());
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), newListenerEvents);

    leave();
    ASSERT_TRUE(takeEvents().empty());
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10), exited(child, 5, 10)}), newListenerEvents);
}

TEST_F(HoverDispatcherTest, entersLayerGainingListenerUnderStillPointer) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);

    hover(15, 20);
    ASSERT_TRUE(takeEvents().empty());

    listen(child);
    processFrame();
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10)}), takeEvents());
}

TEST_F(HoverDispatcherTest, listenerMovingPointerDuringDispatchIsAppliedAfter) {
    auto left = createView(0, 0, 50, 100);
    auto right = createView(50, 0, 50, 100);
    rootView->addChild(left);
    rootView->addChild(right);
    listen(right);
    left->setHoverListener([this](Layer& hovered, const HoverEvent& event) {
        events.emplace_back(HoverRecord{&hovered, event.type, event.location});
        if (event.type == HoverEventTypeEntered) {
            hover(60, 10);
        }
    });

    hover(10, 10);
    ASSERT_EQ(std::vector<HoverRecord>({entered(left, 10, 10), exited(left, 10, 10), entered(right, 10, 10)}),
              takeEvents());
}

TEST_F(HoverDispatcherTest, requestsFrameWhenHitTestChangesUnderPointer) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    settle();
    takeEvents();
    rootListener.frameRequests = 0;

    child->setTouchEnabled(false);
    ASSERT_EQ(1, rootListener.frameRequests);
    settle();
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), takeEvents());

    child->setTouchEnabled(true);
    settle();
    takeEvents();
    rootListener.frameRequests = 0;

    child->setHoverListener(HoverListener());
    ASSERT_EQ(1, rootListener.frameRequests);

    leave();
    settle();
    rootListener.frameRequests = 0;

    listen(child);
    child->setTouchEnabled(false);
    ASSERT_EQ(0, rootListener.frameRequests);
}

TEST_F(HoverDispatcherTest, followsTouchAreaExtension) {
    auto child = createView(10, 10, 20, 20);
    rootView->addChild(child);
    listen(child);

    hover(5, 5);
    ASSERT_TRUE(takeEvents().empty());

    child->setTouchAreaExtension(10, 10, 10, 10);
    settle();
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, -5, -5)}), takeEvents());
}

TEST_F(HoverDispatcherTest, retargetsAfterListenerChangesTreeDuringFrame) {
    auto bottom = createView(0, 0, 50, 50);
    auto mover = createView(60, 0, 40, 40);
    rootView->addChild(bottom);
    rootView->addChild(mover);
    listen(bottom);
    mover->setHoverListener([this](Layer& hovered, const HoverEvent& event) {
        events.emplace_back(HoverRecord{&hovered, event.type, event.location});
        if (event.type == HoverEventTypeEntered) {
            hovered.setOpacity(0);
        }
    });

    hover(10, 10);
    ASSERT_EQ(std::vector<HoverRecord>({entered(bottom, 10, 10)}), takeEvents());

    mover->setTranslationX(-60);
    settle();
    ASSERT_EQ(std::vector<HoverRecord>(
                  {exited(bottom, 10, 10), entered(mover, 10, 10), exited(mover, 10, 10), entered(bottom, 10, 10)}),
              takeEvents());
}

TEST_F(HoverDispatcherTest, exitsWhenRootIsDestroyed) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    takeEvents();

    root->destroy();
    ASSERT_EQ(std::vector<HoverRecord>({exited(child, 5, 10)}), takeEvents());
}

TEST_F(HoverDispatcherTest, neverBecomesATouchCandidate) {
    auto child = createView(10, 10, 30, 30);
    rootView->addChild(child);
    listen(child);

    hover(15, 20);
    ASSERT_TRUE(root->getTouchDispatcher().isEmpty());
    ASSERT_TRUE(root->getTouchDispatcher()
                    .getGestureCandidatesForEvent(createTouchEvent(TouchEventTypeDown, 15, 20), rootView)
                    .empty());
}

TEST_F(HoverDispatcherTest, leavesTapsUntouched) {
    auto child = createView(25, 25, 25, 25);
    rootView->addChild(child);
    listen(child);
    auto tap = addSingleTapGesture(child);

    hover(30, 35);
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeDown, 30, 35));
    hover(31, 35);
    hover(80, 80);
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeUp, 30, 35));

    ASSERT_EQ(GestureRecognizerStateEnded, tap->state);
    ASSERT_TRUE(root->getTouchDispatcher().isEmpty());
    ASSERT_EQ(std::vector<HoverRecord>({entered(child, 5, 10), moved(child, 6, 10), exited(child, 6, 10)}),
              takeEvents());
}

TEST_F(HoverDispatcherTest, leavesDragsUntouched) {
    auto child = createView(25, 25, 25, 25);
    rootView->addChild(child);
    listen(child);
    auto drag = addDragGesture(child);
    auto slop = GesturesConfiguration::getDefault().dragTouchSlop;

    hover(30, 35);
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeDown, 30, 35));
    ASSERT_EQ(GestureRecognizerStatePossible, drag->state);

    hover(30 + slop, 35);
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeMoved, 30 + slop, 35));
    ASSERT_EQ(GestureRecognizerStateBegan, drag->state);
    ASSERT_EQ(30 + slop - 25, drag->location.x);

    leave();
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeMoved, 30 + slop, 35));
    ASSERT_EQ(GestureRecognizerStateChanged, drag->state);

    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeUp, 30, 35));
    ASSERT_EQ(GestureRecognizerStateEnded, drag->state);
    ASSERT_TRUE(root->getTouchDispatcher().isEmpty());
}

TEST_F(HoverDispatcherTest, leavesLongPressesUntouched) {
    auto child = createView(25, 25, 25, 25);
    rootView->addChild(child);
    listen(child);
    auto longPress = addLongPressGesture(child);

    hover(30, 35);
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeDown, 30, 35));
    hover(31, 35);
    root->dispatchTouchEvent(
        createTouchEvent(TouchEventTypeMoved, 30, 35, 0, 0, 1, GesturesConfiguration::getDefault().longPressTimeout));
    ASSERT_EQ(GestureRecognizerStateBegan, longPress->state);

    leave();
    root->dispatchTouchEvent(createTouchEvent(TouchEventTypeUp, 30, 35));
    ASSERT_EQ(GestureRecognizerStateEnded, longPress->state);
    ASSERT_TRUE(root->getTouchDispatcher().isEmpty());
}

} // namespace snap::drawing
