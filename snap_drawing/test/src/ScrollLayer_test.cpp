#include <gtest/gtest.h>

#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "snap_drawing/cpp/Layers/Scroll/IOSScroller.hpp"
#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

using namespace Valdi;

namespace snap::drawing {

enum class Velocity { low = 1000, med = 10000, high = 100000, shigh = 1000000 };

class TestScrollPerfLogger : public ScrollPerfLogger {
public:
    int resumeCount = 0;
    int pauseCount = 0;

    void resume() override {
        resumeCount++;
    }

    void pause(bool cancelLogging) override {
        pauseCount++;
    }
};

class ScrollLayerRoot : public LayerRoot {
public:
    int requestFocusCount = 0;

    explicit ScrollLayerRoot(const Ref<Resources>& resources) : LayerRoot(resources) {}

    void requestFocus(ILayer* layer) override {
        requestFocusCount++;
    }
};

class TestScrollLayerListener : public ScrollLayerListener {
public:
    Valdi::Function<std::optional<Point>(Point, Vector)> onScrollCallback;
    Valdi::Function<void(Point)> onScrollEndCallback;
    Valdi::Function<void(Point, Vector)> onDragStartCallback;
    Valdi::Function<std::optional<Point>(Point, Vector)> onDragEndingCallback;

    std::optional<Point> onScroll(const ScrollLayer& scrollLayer, const Point& point, const Vector& velocity) override {
        if (onScrollCallback) {
            return onScrollCallback(point, velocity);
        }

        return std::nullopt;
    }

    void onScrollEnd(const ScrollLayer& scrollLayer, const Point& point) override {
        if (onScrollEndCallback) {
            onScrollEndCallback(point);
        }
    }

    void onDragStart(const ScrollLayer& scrollLayer, const Point& point, const Vector& velocity) override {
        if (onDragStartCallback) {
            onDragStartCallback(point, velocity);
        }
    }

    std::optional<Point> onDragEnding(const ScrollLayer& scrollLayer,
                                      const Point& point,
                                      const Vector& velocity) override {
        if (onDragEndingCallback) {
            return onDragEndingCallback(point, velocity);
        }

        return std::nullopt;
    }
};

class ScrollLayerFixture : public ::testing::TestWithParam<Velocity> {
protected:
    void SetUp() override {
        auto resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());

        _listener = Valdi::makeShared<TestScrollLayerListener>();

        _scrollLayer = makeLayer<ScrollLayer>(resources);
        _scrollLayer->setScroller(Valdi::makeShared<IOSScroller>());
        _scrollLayer->setListener(_listener);
        _scrollLayer->setContentSize(Size(400, 2 * 800));
        _velocity = static_cast<float>(GetParam());

        _layerRoot = makeLayer<ScrollLayerRoot>(resources);
        _layerRoot->setContentLayer(_scrollLayer, ContentLayerSizingModeMatchSize);
        _layerRoot->setSize(Size(400.0f, 800.0f), 1.0f);
    }

    Scalar getMaxContentOffsetYWrapper() {
        return _scrollLayer->getMaxContentOffsetY();
    }

    Scalar getMinContentOffsetYWrapper() {
        return _scrollLayer->getMinContentOffsetY();
    }

    void setAtTop() {
        _scrollLayer->applyContentOffset(0, 0, Vector(0, 0));
    }

    void setAtBottom() {
        _scrollLayer->applyContentOffset(0, _scrollLayer->getMaxContentOffsetY(), Vector(0, 0));
    }

    void scrollAndFlushAnimations(GestureRecognizerState state,
                                  const Point location,
                                  const Size offset,
                                  const Vector velocity) {
        DragEvent event;
        event.location = location;
        event.offset = offset;
        event.velocity = velocity;
        _scrollLayer->onScrollDrag(state, event);

        flushAnimations();
    }

    size_t flushAnimations() {
        size_t frames = 0;
        while (_layerRoot->needsProcessFrame()) {
            _layerRoot->processFrame(TimePoint::fromSeconds(_currentTime));
            _currentTime += 1.0 / 60.0;
            frames++;
        }

        return frames;
    }

    void TearDown() override {}

    Valdi::Ref<ScrollLayer> _scrollLayer;
    Valdi::Ref<TestScrollLayerListener> _listener;
    Ref<ScrollLayerRoot> _layerRoot;
    float _velocity;
    TimeInterval _currentTime = 0.0;
};

TEST(ScrollLayer, reportsContentLayerAsChildInsertionLayer) {
    auto resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());
    auto scrollLayer = makeLayer<ScrollLayer>(resources);

    auto* childInsertionLayerProvider = dynamic_cast<IChildInsertionLayerProvider*>(scrollLayer.get());
    ASSERT_NE(nullptr, childInsertionLayerProvider);
    ASSERT_EQ(scrollLayer->getContentLayer().get(), &childInsertionLayerProvider->getChildInsertionLayer());
}

TEST_P(ScrollLayerFixture, testScrollDownStopsAtMaxOffset) {
    setAtTop();
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 600), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, -200), Vector(0, _velocity));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 200), Size(0, -400), Vector(0, _velocity));

    auto test = _scrollLayer->getContentOffset().y;
    auto limit = getMaxContentOffsetYWrapper();
    ASSERT_EQ(test, limit);
}

TEST_P(ScrollLayerFixture, testScrollUpStopsAtMinOffset) {
    setAtBottom();
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, 200), Vector(0, -_velocity));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 600), Size(0, 400), Vector(0, -_velocity));

    auto test = _scrollLayer->getContentOffset().y;
    auto limit = getMinContentOffsetYWrapper();
    ASSERT_EQ(test, limit);
}

TEST_P(ScrollLayerFixture, notifiesRequestFocusWhenDismissKeyboardOnDragIsEnabled) {
    setAtTop();

    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    ASSERT_EQ(0, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, 200), Vector(0, -_velocity));
    ASSERT_EQ(0, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 600), Size(0, 400), Vector(0, -_velocity));
    ASSERT_EQ(0, _layerRoot->requestFocusCount);

    setAtTop();

    _scrollLayer->setDismissKeyboardOnDrag(true);

    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    ASSERT_EQ(1, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, 200), Vector(0, -_velocity));
    ASSERT_EQ(1, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 600), Size(0, 400), Vector(0, -_velocity));
    ASSERT_EQ(1, _layerRoot->requestFocusCount);

    setAtTop();
    ASSERT_EQ(1, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    ASSERT_EQ(2, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, 200), Vector(0, -_velocity));
    ASSERT_EQ(2, _layerRoot->requestFocusCount);

    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 600), Size(0, 400), Vector(0, -_velocity));
    ASSERT_EQ(2, _layerRoot->requestFocusCount);
}

TEST_P(ScrollLayerFixture, callsIntoScrollPerfLogger) {
    auto scrollPerfLogger = Valdi::makeShared<TestScrollPerfLogger>();

    _scrollLayer->setScrollPerfLogger(scrollPerfLogger);

    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 600), Size(0, 0), Vector(0, 0));

    ASSERT_EQ(1, scrollPerfLogger->resumeCount);
    ASSERT_EQ(0, scrollPerfLogger->pauseCount);

    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, -200), Vector(0, _velocity));

    ASSERT_EQ(1, scrollPerfLogger->resumeCount);
    ASSERT_EQ(0, scrollPerfLogger->pauseCount);

    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 200), Size(0, -400), Vector(0, _velocity));

    ASSERT_EQ(1, scrollPerfLogger->resumeCount);
    ASSERT_EQ(1, scrollPerfLogger->pauseCount);

    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 600), Size(0, 0), Vector(0, 0));

    ASSERT_EQ(2, scrollPerfLogger->resumeCount);
    ASSERT_EQ(1, scrollPerfLogger->pauseCount);

    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 200), Size(0, -400), Vector(0, _velocity));

    ASSERT_EQ(2, scrollPerfLogger->resumeCount);
    ASSERT_EQ(2, scrollPerfLogger->pauseCount);
}

TEST_P(ScrollLayerFixture, canCancelScrollWhileAnimatingScroll) {
    setAtBottom();
    _scrollLayer->setContentOffset(Point::make(0, 0), Vector::makeEmpty(), true);

    _listener->onScrollCallback = [&](auto point, auto velocity) -> std::optional<Point> {
        if (point != Point::make(0, 0)) {
            _scrollLayer->setContentOffset(Point::make(0, 0), Vector::makeEmpty(), false);
        }
        return std::nullopt;
    };

    auto processedFrames = flushAnimations();

    ASSERT_EQ(2, processedFrames);
}

TEST_P(ScrollLayerFixture, onlyPagesToNeighboringHorizontalScreens) {
    _scrollLayer->setContentSize(Size(400 * 5, 800));
    _scrollLayer->setPagingEnabled(true);
    _scrollLayer->setHorizontal(true);
    setAtTop();

    // Standard velocity swipe, one page right
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(0, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 200), Size(-200, 0), Vector(400, 0));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(300, 200), Size(-400, 0), Vector(400, 0));

    auto xLoc1 = _scrollLayer->getContentOffset().x;
    ASSERT_EQ(xLoc1, 400);

    // High velocity swipe, one page right
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(0, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 200), Size(-200, 0), Vector(400, 0));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(300, 200), Size(-400, 0), Vector(8000, 0));

    auto xLoc2 = _scrollLayer->getContentOffset().x;
    ASSERT_EQ(xLoc2, 800);

    // Uncertain swipe, same page
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(300, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 200), Size(-200, 0), Vector(-400, 0));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(300, 200), Size(100, 0), Vector(50, 0));

    auto xLoc3 = _scrollLayer->getContentOffset().x;
    ASSERT_EQ(static_cast<int>(xLoc3), 800);
}

TEST_P(ScrollLayerFixture, onlyPagesToNeighboringVerticalScreens) {
    _scrollLayer->setContentSize(Size(400, 5 * 800));
    _scrollLayer->setPagingEnabled(true);
    setAtTop();

    // Standard velocity swipe, one page down
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, -200), Vector(0, 800));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 600), Size(0, -400), Vector(0, 800));

    auto yLoc1 = _scrollLayer->getContentOffset().y;
    ASSERT_EQ(static_cast<int>(yLoc1), 800);

    // High velocity swipe, one page down
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, -200), Vector(0, 800));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 600), Size(0, -400), Vector(0, 8000));

    auto yLoc2 = _scrollLayer->getContentOffset().y;
    ASSERT_EQ(static_cast<int>(yLoc2), 1600);

    // Uncertain swipe, same page
    scrollAndFlushAnimations(GestureRecognizerStateBegan, Point(200, 200), Size(0, 0), Vector(0, 0));
    scrollAndFlushAnimations(GestureRecognizerStateChanged, Point(200, 400), Size(0, -200), Vector(0, 800));
    scrollAndFlushAnimations(GestureRecognizerStateEnded, Point(200, 200), Size(0, 200), Vector(0, -50));

    auto yLoc3 = _scrollLayer->getContentOffset().y;
    ASSERT_EQ(static_cast<int>(yLoc3), 1600);
}

INSTANTIATE_TEST_SUITE_P(ScrollLayerTests,
                         ScrollLayerFixture,
                         ::testing::Values(Velocity::low, Velocity::med, Velocity::high, Velocity::shigh));

} // namespace snap::drawing
