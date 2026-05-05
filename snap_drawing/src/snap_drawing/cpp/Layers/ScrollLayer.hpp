//
//  ScrollLayer.hpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#pragma once

#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Layers/Mask/ScrollLayerFadingEdgesMaskLayer.hpp"
#include "snap_drawing/cpp/Touches/ScrollGestureRecognizer.hpp"
#include "snap_drawing/cpp/Touches/WheelGestureRecognizer.hpp"
#include "snap_drawing/cpp/Utils/VelocityTracker.hpp"

#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace snap::drawing {

class ScrollLayerClass;
class ScrollLayerFixture;
class ScrollLayer;
class IScroller;

class ScrollLayerListener : public Valdi::SimpleRefCountable {
public:
    virtual std::optional<Point> onScroll(const ScrollLayer& scrollLayer,
                                          const Point& point,
                                          const Vector& velocity) = 0;

    virtual void onScrollEnd(const ScrollLayer& scrollLayer, const Point& point) = 0;

    virtual void onDragStart(const ScrollLayer& scrollLayer, const Point& point, const Vector& velocity) = 0;

    virtual std::optional<Point> onDragEnding(const ScrollLayer& scrollLayer,
                                              const Point& point,
                                              const Vector& velocity) = 0;
};

class ScrollPerfLogger : public Valdi::ValdiObject {
public:
    virtual void resume() = 0;
    virtual void pause(bool cancelLogging) = 0;

    VALDI_CLASS_HEADER(ScrollPerfLogger)
};

class BaseScrollLayerAnimation;

class ScrollLayer : public Layer {
public:
    explicit ScrollLayer(const Ref<Resources>& resources);
    ~ScrollLayer() override;

    void onInitialize() override;

    void setContentOffset(Point contentOffset, const Vector& velocity, bool animated);
    void setContentSize(Size size);

    const Ref<Layer>& getContentLayer() const;

    bool prepareForReuse() override;

    const Point& getContentOffset() const;

    void setHorizontal(bool horizontal);

    void setCancelsOtherGesturesOnScroll(bool cancelsTouchesOnScroll);

    void setBounces(bool bounces);
    void setBouncesVerticalWithSmallContent(bool bouncesVerticalWithSmallContent);
    void setBouncesHorizontalWithSmallContent(bool bouncesHorizontalWithSmallContent);

    void setDismissKeyboardOnDrag(bool dismissKeyboardOnDrag);

    void setFadingEdgeLength(Scalar length);
    void setFadingEdgeStart(bool enabled);
    void setFadingEdgeEnd(bool enabled);

    void setPagingEnabled(bool pagingEnabled);

    bool getClipsToBoundsDefaultValue() const override;

    void setListener(const Ref<ScrollLayerListener>& listener);
    const Ref<ScrollLayerListener>& getListener() const;

    void setScrollPerfLogger(const Ref<ScrollPerfLogger>& scrollPerfLogger);
    const Ref<ScrollPerfLogger>& getScrollPerfLogger() const;

    /**
     Set a Scroller instance which will be responsible for calculating the scroll physics.
     */
    void setScroller(const Ref<IScroller>& scroller);
    const Ref<IScroller>& getScroller() const;

protected:
    void onBoundsChanged() override;
    void onRootChanged(ILayerRoot* root) override;

private:
    friend ScrollLayerClass;
    friend ScrollLayerFixture;
    friend BaseScrollLayerAnimation;

    Ref<Layer> _contentLayer;

    Point _contentOffset = Point::makeEmpty();
    Size _contentSize = Size::makeEmpty();

    Point _scrollGestureOffset = Point::makeEmpty();

    bool _horizontal = false;
    bool _bounces = true;
    bool _bouncesVerticalWithSmallContent = false;
    bool _bouncesHorizontalWithSmallContent = false;
    bool _pagingEnabled = false;
    bool _scrollPerfLoggerStarted = false;
    bool _dismissKeyboardOnDrag = false;

    Ref<ScrollGestureRecognizer> _scrollGestureRecognizer;
    Ref<WheelGestureRecognizer> _wheelGestureRecognizer;
    Ref<ScrollLayerListener> _listener;
    Ref<ScrollPerfLogger> _scrollPerfLogger;
    Ref<IScroller> _scroller;

    Ref<ScrollLayerFadingEdgesMaskLayer> _fadingEdgesMaskLayer;
    Scalar _fadingEdgeLength = 0;
    bool _fadingEdgeStartEnabled = true;
    bool _fadingEdgeEndEnabled = true;

    Scalar getMinContentOffsetX() const;
    Scalar getMinContentOffsetY() const;
    Scalar getMaxContentOffsetX() const;
    Scalar getMaxContentOffsetY() const;

    Scalar clampContentOffsetX(Scalar contentOffsetX) const;
    Scalar clampContentOffsetY(Scalar contentOffsetY) const;

    Vector applyContentOffset(Scalar offsetX, Scalar offsetY, Vector velocity);
    void applyContentOffsetInternal(Scalar offsetX, Scalar offsetY);

    std::optional<Point> computePaginatedTargetContentOffset(const Point& contentOffset,
                                                             const std::optional<Point>& contentOffsetOverride,
                                                             const Vector& velocity) const;

    void cancelScrollAnimation();

    void onScrollAnimationEnded();

    void updateContentLayerFrame();
    void updateEdgeGradient();

    void onScrollDrag(GestureRecognizerState state, const DragEvent& event);
    void onScrollWheel(GestureRecognizerState state, const WheelScrollEvent& scrollEvent);

    Point targetOffsetForInteractiveOffset(const Point& interactiveOffset);
    void onScrollEnded(const Point& contentOffset, const Vector& velocity);

    void resumeScrollPerfLogger();
    void pauseScrollPerfLogger();

    static Scalar computeRubberBand(Scalar value, Scalar clamped, Scalar dim);

    // For Testing Only
    static String& getScrollAnimationKey();
};

} // namespace snap::drawing
