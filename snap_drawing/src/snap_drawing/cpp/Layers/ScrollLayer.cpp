//
//  ScrollLayer.cpp
//  valdi-skia
//
//  Created by Simon Corsin on 6/28/20.
//

#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"

#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include "snap_drawing/cpp/Animations/Animation.hpp"
#include "snap_drawing/cpp/Animations/ValueInterpolators.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

#include "snap_drawing/cpp/Layers/Scroll/IScroller.hpp"

namespace snap::drawing {

VALDI_CLASS_IMPL(ScrollPerfLogger);

constexpr Scalar kRubberBandCoefficient = 0.55;

String& ScrollLayer::getScrollAnimationKey() {
    static auto kScrollAnimationKey = STRING_LITERAL("scrollAnimation");
    return kScrollAnimationKey;
}

ScrollLayer::ScrollLayer(const Ref<Resources>& resources) : Layer(resources) {}

ScrollLayer::~ScrollLayer() = default;

void ScrollLayer::onInitialize() {
    Layer::onInitialize();
    _contentLayer = makeLayer<Layer>(getResources());

    addChild(_contentLayer);

    _scrollGestureRecognizer = Valdi::makeShared<ScrollGestureRecognizer>(getResources()->getGesturesConfiguration());
    _scrollGestureRecognizer->setListener(
        [=](const auto& /*gesture*/, auto state, const auto& event) { this->onScrollDrag(state, event); });
    addGestureRecognizer(_scrollGestureRecognizer);

    _wheelGestureRecognizer = Valdi::makeShared<WheelGestureRecognizer>(
        [=](const auto& /*gesture*/, auto state, const auto& wheelEvent) { this->onScrollWheel(state, wheelEvent); });
    addGestureRecognizer(_wheelGestureRecognizer);

    setClipsToBounds(getClipsToBoundsDefaultValue());
    _scrollGestureRecognizer->setHorizontal(_horizontal);
    setCancelsOtherGesturesOnScroll(true);
}

void ScrollLayer::setContentOffset(Point contentOffset, const Vector& velocity, bool animated) {
    if (_contentOffset == contentOffset) {
        return;
    }

    cancelScrollAnimation();

    if (animated && _scroller != nullptr) {
        auto fast = true;
        auto initialOffset = _contentOffset;
        auto finalOffset = contentOffset;

        auto scrollAnimation = _scroller->animate(initialOffset, finalOffset, fast);
        addAnimation(getScrollAnimationKey(), scrollAnimation);
        _scrollGestureRecognizer->setAnimatingScroll(true);
    } else {
        applyContentOffset(contentOffset.x, contentOffset.y, velocity);
    }
}

void ScrollLayer::setContentSize(Size size) {
    _contentSize = size;

    updateContentLayerFrame();

    auto currentOffsetX = _contentOffset.x;
    auto currentOffsetY = _contentOffset.y;
    auto clampedOffsetX = clampContentOffsetX(currentOffsetX);
    auto clampedOffsetY = clampContentOffsetY(currentOffsetY);
    if (currentOffsetX != clampedOffsetX || currentOffsetY != clampedOffsetY) {
        setContentOffset(Point::make(clampedOffsetX, clampedOffsetY), Vector(0, 0), false);
    }
}

Vector ScrollLayer::applyContentOffset(Scalar offsetX, Scalar offsetY, Vector velocity) {
    Vector adjustment = Vector::makeEmpty();
    auto newOffsetX = offsetX;
    auto newOffsetY = offsetY;

    if (_listener != nullptr) {
        auto updatedOffset = _listener->onScroll(*this, Point::make(offsetX, offsetY), velocity);
        if (updatedOffset) {
            newOffsetX = updatedOffset.value().x;
            newOffsetY = updatedOffset.value().y;

            adjustment.dx = newOffsetX - offsetX;
            adjustment.dy = newOffsetY - offsetY;
            _scrollGestureOffset.x += adjustment.dx;
            _scrollGestureOffset.y += adjustment.dy;
        }
    }

    applyContentOffsetInternal(newOffsetX, newOffsetY);

    return adjustment;
}

void ScrollLayer::applyContentOffsetInternal(Scalar offsetX, Scalar offsetY) {
    _contentOffset.x = offsetX;
    _contentOffset.y = offsetY;

    updateContentLayerFrame();
    updateEdgeGradient();
}

void ScrollLayer::onBoundsChanged() {
    updateContentLayerFrame();
    updateEdgeGradient();
}

void ScrollLayer::updateContentLayerFrame() {
    _contentLayer->setFrame(
        Rect::makeXYWH(-_contentOffset.x, -_contentOffset.y, _contentSize.width, _contentSize.height));
}

Scalar ScrollLayer::clampContentOffsetX(Scalar contentOffsetX) const {
    return std::clamp(contentOffsetX, getMinContentOffsetX(), getMaxContentOffsetX());
}

Scalar ScrollLayer::clampContentOffsetY(Scalar contentOffsetY) const {
    return std::clamp(contentOffsetY, getMinContentOffsetY(), getMaxContentOffsetY());
}

void ScrollLayer::cancelScrollAnimation() {
    auto animation = getAnimation(getScrollAnimationKey());
    if (animation != nullptr) {
        removeAnimation(getScrollAnimationKey());
    }
}

void ScrollLayer::onRootChanged(ILayerRoot* root) {
    Layer::onRootChanged(root);

    if (root == nullptr) {
        cancelScrollAnimation();
        if (_scroller != nullptr) {
            _scroller->reset();
        }
    }
}

void ScrollLayer::onScrollDrag(GestureRecognizerState state, const DragEvent& event) {
    cancelScrollAnimation();

    if (state == GestureRecognizerStateBegan) {
        _scrollGestureOffset = _contentOffset;
    }

    if (_scroller != nullptr) {
        _scroller->onDrag(state, event);
    }

    auto dragVelocity = Vector::make(_horizontal ? event.velocity.dx : 0.0f, !_horizontal ? event.velocity.dy : 0.0f);
    auto targetOffset = targetOffsetForInteractiveOffset(
        Point(_scrollGestureOffset.x - event.offset.width, _scrollGestureOffset.y - event.offset.height));

    if (state == GestureRecognizerStateBegan) {
        resumeScrollPerfLogger();
        if (_dismissKeyboardOnDrag) {
            requestFocus(this);
        }
        if (_listener != nullptr) {
            _listener->onDragStart(*this, _contentOffset, dragVelocity);
        }
    } else if (state == GestureRecognizerStateEnded) {
        onScrollEnded(targetOffset, dragVelocity);
    } else {
        setContentOffset(targetOffset, dragVelocity, false);
    }
}

void ScrollLayer::onScrollWheel(GestureRecognizerState /*state*/, const WheelScrollEvent& scrollEvent) {
    auto desiredContentOffset = getContentOffset().makeOffset(scrollEvent.direction.dx, scrollEvent.direction.dy);
    auto finalContentOffset = targetOffsetForInteractiveOffset(desiredContentOffset);
    onScrollEnded(finalContentOffset, scrollEvent.direction);
}

Point ScrollLayer::targetOffsetForInteractiveOffset(const Point& interactiveOffset) {
    Scalar targetOffsetX = interactiveOffset.x;
    Scalar targetOffsetY = interactiveOffset.y;

    Scalar clampedOffsetX = clampContentOffsetX(targetOffsetX);
    Scalar clampedOffsetY = clampContentOffsetY(targetOffsetY);

    if (!_horizontal) {
        targetOffsetX = clampedOffsetX;
    } else {
        targetOffsetY = clampedOffsetY;
    }
    if (!_bounces) {
        if (!_bouncesHorizontalWithSmallContent) {
            targetOffsetX = clampedOffsetX;
        }
        if (!_bouncesVerticalWithSmallContent) {
            targetOffsetY = clampedOffsetY;
        }
    }
    if (targetOffsetX != clampedOffsetX) {
        Scalar frameWidth = getFrame().width();
        targetOffsetX = computeRubberBand(targetOffsetX, clampedOffsetX, frameWidth);
    }
    if (targetOffsetY != clampedOffsetY) {
        Scalar frameHeight = getFrame().height();
        targetOffsetY = computeRubberBand(targetOffsetY, clampedOffsetY, frameHeight);
    }

    return Point::make(targetOffsetX, targetOffsetY);
}

std::optional<Point> ScrollLayer::computePaginatedTargetContentOffset(const Point& contentOffset,
                                                                      const std::optional<Point>& contentOffsetOverride,
                                                                      const Vector& velocity) const {
    if (_scroller == nullptr) {
        return contentOffsetOverride;
    }

    Scalar frameWidth = getFrame().width();
    Scalar frameHeight = getFrame().height();
    auto minOffsetX = std::max<Scalar>(0, floor(_contentOffset.x / frameWidth) * frameWidth);
    auto maxOffsetX =
        std::min<Scalar>(_contentSize.width - frameWidth, ceil(_contentOffset.x / frameWidth) * frameWidth);
    auto minOffsetY = std::max<Scalar>(0, floor(_contentOffset.y / frameHeight) * frameHeight);
    auto maxOffsetY =
        std::min<Scalar>(_contentSize.height - frameHeight, ceil(_contentOffset.y / frameHeight) * frameHeight);

    Point finalOffset;
    if (contentOffsetOverride) {
        finalOffset = contentOffsetOverride.value();
    } else {
        finalOffset =
            _scroller->computeDecelerationFinalOffset(contentOffset, velocity, Size(frameWidth, frameHeight), true);
    }

    auto paginatedFinalOffsetX = std::clamp(round(finalOffset.x / frameWidth) * frameWidth, minOffsetX, maxOffsetX);
    auto paginatedFinalOffsetY = std::clamp(round(finalOffset.y / frameHeight) * frameHeight, minOffsetY, maxOffsetY);
    return Point::make(paginatedFinalOffsetX, paginatedFinalOffsetY);
}

void ScrollLayer::onScrollEnded(const Point& contentOffset, const Vector& velocity) {
    std::optional<Point> targetContentOffsetOverride;
    if (_listener != nullptr) {
        targetContentOffsetOverride = _listener->onDragEnding(*this, contentOffset, velocity);
    }

    if (_pagingEnabled) {
        targetContentOffsetOverride =
            computePaginatedTargetContentOffset(contentOffset, targetContentOffsetOverride, velocity);
    }

    if (targetContentOffsetOverride) {
        setContentOffset(targetContentOffsetOverride.value(), velocity, true);
    } else {
        setContentOffset(contentOffset, velocity, false);

        if (_scroller != nullptr) {
            _scrollGestureRecognizer->setAnimatingScroll(true);

            auto animation = _scroller->fling(contentOffset, velocity, false);
            addAnimation(getScrollAnimationKey(), animation);
        }
    }
}

Scalar ScrollLayer::computeRubberBand(Scalar value, Scalar clamped, Scalar dim) {
    auto coef = kRubberBandCoefficient;
    auto diff = std::abs(value - clamped);
    auto sign = clamped > value ? -1 : 1;
    auto rubber = (1.0 - (1.0 / (diff * coef / dim + 1.0))) * dim;
    return clamped + sign * rubber;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Scalar ScrollLayer::getMinContentOffsetX() const {
    return 0;
}

Scalar ScrollLayer::getMaxContentOffsetX() const {
    return _horizontal ? std::max(_contentSize.width - getFrame().width(), 0.0f) : 0;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Scalar ScrollLayer::getMinContentOffsetY() const {
    return 0;
}

Scalar ScrollLayer::getMaxContentOffsetY() const {
    return _horizontal ? 0 : std::max(_contentSize.height - getFrame().height(), 0.0f);
}

const Point& ScrollLayer::getContentOffset() const {
    return _contentOffset;
}

const Ref<Layer>& ScrollLayer::getContentLayer() const {
    return _contentLayer;
}

bool ScrollLayer::getClipsToBoundsDefaultValue() const {
    return true;
}

void ScrollLayer::setHorizontal(bool horizontal) {
    _horizontal = horizontal;
    _scrollGestureRecognizer->setHorizontal(horizontal);
    updateEdgeGradient();
}

void ScrollLayer::setBounces(bool bounces) {
    _bounces = bounces;
}

void ScrollLayer::setBouncesVerticalWithSmallContent(bool bouncesVerticalWithSmallContent) {
    _bouncesVerticalWithSmallContent = bouncesVerticalWithSmallContent;
}

void ScrollLayer::setBouncesHorizontalWithSmallContent(bool bouncesHorizontalWithSmallContent) {
    _bouncesHorizontalWithSmallContent = bouncesHorizontalWithSmallContent;
}

void ScrollLayer::setPagingEnabled(bool pagingEnabled) {
    _pagingEnabled = pagingEnabled;
}

void ScrollLayer::setDismissKeyboardOnDrag(bool dismissKeyboardOnDrag) {
    _dismissKeyboardOnDrag = dismissKeyboardOnDrag;
}

void ScrollLayer::setFadingEdgeLength(Scalar length) {
    if (length == _fadingEdgeLength) {
        return;
    }
    _fadingEdgeLength = length;
    if (length <= 0) {
        _fadingEdgesMaskLayer = nullptr;
        setMaskLayer(nullptr);
        return;
    }
    if (_fadingEdgesMaskLayer == nullptr) {
        _fadingEdgesMaskLayer = Valdi::makeShared<ScrollLayerFadingEdgesMaskLayer>();
        updateEdgeGradient();
        setMaskLayer(_fadingEdgesMaskLayer);
    } else {
        updateEdgeGradient();
        setNeedsDisplay();
    }
}

void ScrollLayer::setFadingEdgeStart(bool enabled) {
    if (_fadingEdgeStartEnabled == enabled) {
        return;
    }
    _fadingEdgeStartEnabled = enabled;
    updateEdgeGradient();
}

void ScrollLayer::setFadingEdgeEnd(bool enabled) {
    if (_fadingEdgeEndEnabled == enabled) {
        return;
    }
    _fadingEdgeEndEnabled = enabled;
    updateEdgeGradient();
}

void ScrollLayer::updateEdgeGradient() {
    if (_fadingEdgeLength <= 0) {
        return;
    }
    auto offset = getContentOffset();
    auto frame = getFrame();
    auto bounds = Rect::makeXYWH(0, 0, frame.width(), frame.height());
    if (_horizontal) {
        auto leadingEdgeLength = _fadingEdgeStartEnabled ? std::clamp<Scalar>(offset.x, 0, _fadingEdgeLength) : 0;
        auto trailingEdgeLength =
            _fadingEdgeEndEnabled ?
                std::clamp<Scalar>(_contentSize.width - bounds.width() - offset.x, 0, _fadingEdgeLength) :
                0;
        _fadingEdgesMaskLayer->configure(_horizontal, bounds, leadingEdgeLength, trailingEdgeLength);
    } else {
        auto leadingEdgeLength = _fadingEdgeStartEnabled ? std::clamp<Scalar>(offset.y, 0, _fadingEdgeLength) : 0;
        auto trailingEdgeLength =
            _fadingEdgeEndEnabled ?
                std::clamp<Scalar>(_contentSize.height - bounds.height() - offset.y, 0, _fadingEdgeLength) :
                0;
        _fadingEdgesMaskLayer->configure(_horizontal, bounds, leadingEdgeLength, trailingEdgeLength);
    }
}

bool ScrollLayer::prepareForReuse() {
    auto shouldReuse = Layer::prepareForReuse();
    cancelScrollAnimation();
    if (_scroller != nullptr) {
        _scroller->reset();
    }
    applyContentOffsetInternal(0, 0);
    pauseScrollPerfLogger();
    return shouldReuse;
}

void ScrollLayer::onScrollAnimationEnded() {
    _scrollGestureRecognizer->setAnimatingScroll(false);
    if (_listener != nullptr) {
        _listener->onScrollEnd(*this, _contentOffset);
    }
    pauseScrollPerfLogger();
}

void ScrollLayer::resumeScrollPerfLogger() {
    if (!_scrollPerfLoggerStarted) {
        _scrollPerfLoggerStarted = true;
        if (_scrollPerfLogger != nullptr) {
            _scrollPerfLogger->resume();
        }
    }
}

void ScrollLayer::pauseScrollPerfLogger() {
    if (_scrollPerfLoggerStarted) {
        _scrollPerfLoggerStarted = false;
        if (_scrollPerfLogger != nullptr) {
            _scrollPerfLogger->pause(false);
        }
    }
}

void ScrollLayer::setCancelsOtherGesturesOnScroll(bool cancelsTouchesOnScroll) {
    _scrollGestureRecognizer->setShouldCancelOtherGesturesOnStart(cancelsTouchesOnScroll);
}

void ScrollLayer::setListener(const Ref<ScrollLayerListener>& listener) {
    _listener = listener;
}

const Ref<ScrollLayerListener>& ScrollLayer::getListener() const {
    return _listener;
}

void ScrollLayer::setScrollPerfLogger(const Ref<ScrollPerfLogger>& scrollPerfLogger) {
    if (_scrollPerfLogger != scrollPerfLogger) {
        pauseScrollPerfLogger();
        _scrollPerfLogger = scrollPerfLogger;
    }
}

const Ref<ScrollPerfLogger>& ScrollLayer::getScrollPerfLogger() const {
    return _scrollPerfLogger;
}

void ScrollLayer::setScroller(const Ref<IScroller>& scroller) {
    _scroller = scroller;
}

const Ref<IScroller>& ScrollLayer::getScroller() const {
    return _scroller;
}

} // namespace snap::drawing
