//
//  ViewNodeScrollState.cpp
//  valdi
//
//  Created by Simon Corsin on 8/6/21.
//

#include "valdi/runtime/Context/ViewNodeScrollState.hpp"
#include "valdi/runtime/Views/Measure.hpp"
#include "valdi_core/cpp/Events/TouchEvents.hpp"

#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace Valdi {

ViewNodeScrollState::ViewNodeScrollState() = default;

ViewNodeScrollState::~ViewNodeScrollState() = default;

bool ViewNodeScrollState::isInScrollMode() const {
    return _inScrollMode;
}

void ViewNodeScrollState::setInScrollMode(bool inScrollMode) {
    _inScrollMode = inScrollMode;
}

bool ViewNodeScrollState::isCurrentlyAnimating() const {
    return _currentlyAnimating;
}

void ViewNodeScrollState::setCurrentlyAnimating(bool currentlyAnimating) {
    _currentlyAnimating = currentlyAnimating;
}

bool ViewNodeScrollState::needsSyncWithView() const {
    return _needsSyncWithView;
}

void ViewNodeScrollState::setNeedsSyncWithView(bool needsSyncWithView) {
    _needsSyncWithView = needsSyncWithView;
}

const Point& ViewNodeScrollState::getDirectionAgnosticContentOffset() const {
    return _directionAgnosticContentOffset;
}

Point ViewNodeScrollState::getDirectionDependentContentOffset() const {
    return resolveDirectionDependentContentOffset(_directionAgnosticContentOffset);
}

void ViewNodeScrollState::resolveClipRect(Frame& outClipRect) const {
    auto directionDependentContentOffset = getDirectionDependentContentOffset();

    auto initialClipX = outClipRect.x + directionDependentContentOffset.x;
    auto initialClipY = outClipRect.y + directionDependentContentOffset.y;

    auto viewportLeft = initialClipX - _viewportExtensionLeft;
    auto viewportRight = initialClipX + outClipRect.width + _viewportExtensionRight;
    auto viewportTop = initialClipY - _viewportExtensionTop;
    auto viewportBottom = initialClipY + outClipRect.height + _viewportExtensionBottom;

    outClipRect.x = viewportLeft;
    outClipRect.width = std::max(viewportRight - viewportLeft, 0.0f);
    outClipRect.y = viewportTop;
    outClipRect.height = std::max(viewportBottom - viewportTop, 0.0f);
}

const Size& ViewNodeScrollState::getContentSize() const {
    return _contentSize;
}

void ViewNodeScrollState::setCircularRatio(int circularRatio) {
    _circularRatio = circularRatio;
}

int ViewNodeScrollState::getCircularRatio() const {
    return _circularRatio;
}

void ViewNodeScrollState::setStaticContentWidth(float staticContentWidth) {
    _staticContentWidth = staticContentWidth;
}

float ViewNodeScrollState::getStaticContentWidth() const {
    return _staticContentWidth;
}

void ViewNodeScrollState::setStaticContentHeight(float staticContentHeight) {
    _staticContentHeight = staticContentHeight;
}

float ViewNodeScrollState::getStaticContentHeight() const {
    return _staticContentHeight;
}

void ViewNodeScrollState::setIsHorizontal(bool isHorizontal) {
    _isHorizontal = isHorizontal;
}

bool ViewNodeScrollState::getIsHorizontal() const {
    return _isHorizontal;
}

bool ViewNodeScrollState::isScrolling() const {
    return _scrolling;
}

// In RTL mode with overflow enabled, Yoga lays out from right to left
// and items that overflow end up having a negative position. This will
// contain the offset that need to be applied so that items start at x=0
// instead of x=-rtlOffsetX.
float ViewNodeScrollState::getRtlOffsetX() const {
    return _rtlOffsetX;
}

static float applyCircularScroll(
    int circularRatioInt, float contentOffset, float contentSize, float viewportSize, float pointScale) {
    auto circularRatio = static_cast<float>(circularRatioInt);
    auto resolvedPageSize = contentSize / circularRatio;
    auto halfPageSize = resolvedPageSize / 2;

    float newContentOffset = contentOffset;
    float minContentOffset = halfPageSize;
    float maxContentOffset = contentSize - viewportSize - halfPageSize;

    if (resolvedPageSize != 0.0) {
        while (newContentOffset < minContentOffset) {
            newContentOffset += resolvedPageSize;
        }
        while (newContentOffset > maxContentOffset) {
            newContentOffset -= resolvedPageSize;
        }

        if (pointScale != 0.0f) {
            newContentOffset = Valdi::roundToPixelGrid(newContentOffset, pointScale);
        }
    }

    return newContentOffset;
}

bool ViewNodeScrollState::updateDirectionDependentContentOffset(const Point& directionDependentContentOffset,
                                                                const Point& directionDependentUnclampedContentOffset) {
    return updateDirectionAgnosticContentOffset(
        resolveDirectionAgnosticContentOffset(directionDependentContentOffset),
        resolveDirectionAgnosticContentOffset(directionDependentUnclampedContentOffset));
}

bool ViewNodeScrollState::updateDirectionAgnosticContentOffset(const Point& directionAgnosticContentOffset,
                                                               const Point& directionAgnosticUnclampedContentOffset) {
    auto outputDirectionAgnosticContentOffset = directionAgnosticContentOffset;
    auto outputDirectionAgnosticUnclampedContentOffset = directionAgnosticUnclampedContentOffset;

    // In a circular situation, a content offset is only valid between a specific range
    if (VALDI_UNLIKELY(_circularRatio > 1)) {
        if (_isHorizontal) {
            outputDirectionAgnosticContentOffset.x = applyCircularScroll(
                _circularRatio, directionAgnosticContentOffset.x, _contentSize.width, _viewportSize.width, _pointScale);
        } else {
            outputDirectionAgnosticContentOffset.y = applyCircularScroll(_circularRatio,
                                                                         directionAgnosticContentOffset.y,
                                                                         _contentSize.height,
                                                                         _viewportSize.height,
                                                                         _pointScale);
        }
    }

    // If the content offset changed, make sure we keep the new value stuck in the proper direction
    if (_isHorizontal) {
        outputDirectionAgnosticContentOffset.y = 0;
    } else {
        outputDirectionAgnosticContentOffset.x = 0;
    }

    // Do not proceed to update if there was no change to the contentOffset in any way
    bool sameAsLast = outputDirectionAgnosticContentOffset == _directionAgnosticContentOffset;
    bool sameAsInput = outputDirectionAgnosticContentOffset == directionAgnosticContentOffset;
    bool sameAsUnclamped = outputDirectionAgnosticUnclampedContentOffset == _directionAgnosticUnclampedContentOffset;
    if (sameAsLast && sameAsInput && sameAsUnclamped) {
        return false;
    }

    // Actually update the known content offset
    _directionAgnosticContentOffset = outputDirectionAgnosticContentOffset;
    _directionAgnosticUnclampedContentOffset = outputDirectionAgnosticUnclampedContentOffset;

    return true;
}

static float calculateOverscroll(float contentSize, float viewportSize, float unclampedContentOffset) {
    return std::max(unclampedContentOffset - std::max(contentSize - viewportSize, 0.0f), 0.0f);
}

ViewNodeScrollStateUpdateContentSizeResult ViewNodeScrollState::updateContentSizeAndRtlOffset(
    const Size& contentSize, const Size& viewportSize, float rtlOffsetX, float pointScale, bool isHorizontal) {
    ViewNodeScrollStateUpdateContentSizeResult result;
    bool shouldNotifyContentSizeChangeCallback = false;

    if (_rtlOffsetX != rtlOffsetX) {
        _rtlOffsetX = rtlOffsetX;
        result.changed = true;
    }

    if (isHorizontal != _isHorizontal) {
        _isHorizontal = isHorizontal;
        result.changed = true;
    }

    // Maybe updatecontent size with static size
    const float contentSizeWidth = _staticContentWidth > 0 ? _staticContentWidth : contentSize.width;
    const float contentSizeHeight = _staticContentHeight > 0 ? _staticContentHeight : contentSize.height;

    Size contentSizeWithStaticSize = Size(contentSizeWidth, contentSizeHeight);

    if (_contentSize != contentSizeWithStaticSize) {
        auto overscrollX =
            calculateOverscroll(_contentSize.width, _viewportSize.width, _directionAgnosticContentOffset.x);
        auto overscrollY =
            calculateOverscroll(_contentSize.height, _viewportSize.height, _directionAgnosticContentOffset.y);
        auto newOverscrollX =
            calculateOverscroll(contentSizeWithStaticSize.width, viewportSize.width, _directionAgnosticContentOffset.x);
        auto newOverscrollY = calculateOverscroll(
            contentSizeWithStaticSize.height, viewportSize.height, _directionAgnosticContentOffset.y);

        _contentSize = contentSizeWithStaticSize;
        result.changed = true;
        shouldNotifyContentSizeChangeCallback = true;

        if (newOverscrollX > overscrollX || newOverscrollY > overscrollY) {
            // Our new contentSize is causing us to overscroll more
            // Adjust the contentOffset so that we retain the same amount of overscroll
            result.contentOffsetAdjusted = true;

            auto offsetX = overscrollX - newOverscrollX;
            auto offsetY = overscrollY - newOverscrollY;
            _directionAgnosticContentOffset.x += offsetX;
            _directionAgnosticContentOffset.y += offsetY;
            _directionAgnosticUnclampedContentOffset.x += offsetX;
            _directionAgnosticUnclampedContentOffset.y += offsetY;
        }
    }

    _viewportSize = viewportSize;
    _pointScale = pointScale;

    if (result.changed) {
        setNeedsSyncWithView(true);

        if (shouldNotifyContentSizeChangeCallback) {
            notifyContentSizeChanged();
        }
    }

    return result;
}

Point ViewNodeScrollState::resolveDirectionDependentContentOffset(const Point& directionAgnosticContentOffset) const {
    return resolveContentOffset(directionAgnosticContentOffset, false);
}

Point ViewNodeScrollState::resolveDirectionAgnosticContentOffset(const Point& directionDependentContentOffset) const {
    return resolveContentOffset(directionDependentContentOffset, true);
}

Point ViewNodeScrollState::resolveContentOffset(const Point& convertedContentOffset,
                                                [[maybe_unused]] bool directionAgnostic) const {
    // Note directionAgnostic is currently unused, since (x = rtlOffsetX - y) and (y = rtlOffset - x) are equivalent
    if (_rtlOffsetX != 0.0f) {
        return Point(_rtlOffsetX - convertedContentOffset.x, convertedContentOffset.y);
    } else {
        return convertedContentOffset;
    }
}

void ViewNodeScrollState::submitScrollEvent(const Ref<ValueFunction>& callback,
                                            ValueFunctionFlags callFlags,
                                            const Point& directionAgnosticContentOffset,
                                            const Point& directionAgnosticUnclampedContentOffset,
                                            const Point& directionAgnosticVelocity) {
    auto touchEvent = TouchEvents::makeScrollEvent(static_cast<double>(directionAgnosticContentOffset.x),
                                                   static_cast<double>(directionAgnosticContentOffset.y),
                                                   static_cast<double>(directionAgnosticUnclampedContentOffset.x),
                                                   static_cast<double>(directionAgnosticUnclampedContentOffset.y),
                                                   static_cast<double>(directionAgnosticVelocity.x),
                                                   static_cast<double>(directionAgnosticVelocity.y));

    (*callback)(callFlags, {std::move(touchEvent)});
}

void ViewNodeScrollState::submitScrollEndEvent(const Ref<ValueFunction>& callback,
                                               ValueFunctionFlags callFlags,
                                               const Point& directionAgnosticContentOffset,
                                               const Point& directionAgnosticUnclampedContentOffset,
                                               const Point& directionAgnosticVelocity) {
    auto touchEvent = TouchEvents::makeScrollEvent(static_cast<double>(directionAgnosticContentOffset.x),
                                                   static_cast<double>(directionAgnosticContentOffset.y),
                                                   static_cast<double>(directionAgnosticUnclampedContentOffset.x),
                                                   static_cast<double>(directionAgnosticUnclampedContentOffset.y),
                                                   static_cast<double>(directionAgnosticVelocity.x),
                                                   static_cast<double>(directionAgnosticVelocity.y));

    (*callback)(callFlags, {std::move(touchEvent)});
}

void ViewNodeScrollState::notifyOnScroll(const Point& directionAgnosticContentOffset,
                                         const Point& directionAgnosticUnclampedContentOffset,
                                         const Point& directionAgnosticVelocity) const {
    if (_onScrollCallback != nullptr) {
        VALDI_TRACE("Valdi.notifyOnScroll")
        submitScrollEvent(_onScrollCallback,
                          ValueFunctionFlagsAllowThrottling,
                          directionAgnosticContentOffset,
                          directionAgnosticUnclampedContentOffset,
                          directionAgnosticVelocity);
    }
}

void ViewNodeScrollState::notifyOnDragStart(const Point& directionAgnosticContentOffset,
                                            const Point& directionAgnosticUnclampedContentOffset,
                                            const Point& directionAgnosticVelocity) {
    _scrolling = true;

    if (_onDragStartCallback != nullptr) {
        VALDI_TRACE("Valdi.notifyOnDragStart")
        submitScrollEvent(_onDragStartCallback,
                          ValueFunctionFlagsNone,
                          directionAgnosticContentOffset,
                          directionAgnosticUnclampedContentOffset,
                          directionAgnosticVelocity);
    }
}

void ViewNodeScrollState::notifyOnScrollEnd(const Point& directionAgnosticContentOffset,
                                            const Point& directionAgnosticUnclampedContentOffset) {
    _scrolling = false;
    if (_onScrollEndCallback != nullptr) {
        VALDI_TRACE("Valdi.notifyOnScrollEnd")
        static Point velocity = Point(0, 0);
        submitScrollEndEvent(_onScrollEndCallback,
                             ValueFunctionFlagsNone,
                             directionAgnosticContentOffset,
                             directionAgnosticUnclampedContentOffset,
                             velocity);
    }
}

static Value makeScrollEvent(const Point& directionAgnosticContentOffset,
                             const Point& directionAgnosticUnclampedContentOffset,
                             const Point& directionAgnosticVelocity) {
    return TouchEvents::makeScrollEvent(static_cast<double>(directionAgnosticContentOffset.x),
                                        static_cast<double>(directionAgnosticContentOffset.y),
                                        static_cast<double>(directionAgnosticUnclampedContentOffset.x),
                                        static_cast<double>(directionAgnosticUnclampedContentOffset.y),
                                        static_cast<double>(directionAgnosticVelocity.x),
                                        static_cast<double>(directionAgnosticVelocity.y));
}

void ViewNodeScrollState::notifyOnDragEnd(const Point& directionAgnosticContentOffset,
                                          const Point& directionAgnosticUnclampedContentOffset,
                                          const Point& directionAgnosticVelocity) const {
    if (_onDragEndCallback == nullptr) {
        return;
    }

    VALDI_TRACE("Valdi.notifyOnDragEnd");
    auto scrollEvent = makeScrollEvent(
        directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset, directionAgnosticVelocity);
    _onDragEndCallback->call(ValueFunctionFlagsNone, &scrollEvent, 1);
}

Result<std::optional<Point>> ViewNodeScrollState::notifyOnDragEnding(
    const Point& directionAgnosticContentOffset,
    const Point& directionAgnosticUnclampedContentOffset,
    const Point& directionAgnosticVelocity) const {
    if (_onDragEndingCallback == nullptr) {
        return {std::nullopt};
    }
    VALDI_TRACE("Valdi.notifyOnDragEnding")

    auto scrollEvent = makeScrollEvent(
        directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset, directionAgnosticVelocity);

    constexpr auto kMaxOnDragEndingDuration = std::chrono::milliseconds(100);
    auto result = _onDragEndingCallback->callSyncWithDeadline(kMaxOnDragEndingDuration, &scrollEvent, 1);
    if (!result) {
        return result.moveError();
    }

    if (result.value().isNullOrUndefined()) {
        return {std::nullopt};
    }

    if (!result.value().isMap()) {
        return Error("Expecting Point object for onDragEnding");
    }

    return {Point(*result.value().getMap())};
}

void ViewNodeScrollState::notifyContentSizeChanged() const {
    if (_onContentSizeChangeCallback == nullptr) {
        return;
    }

    Value object;
    object.setMapValue("width", Value(static_cast<double>(_contentSize.width)));
    object.setMapValue("height", Value(static_cast<double>(_contentSize.height)));

    (*_onContentSizeChangeCallback)({std::move(object)});
}

void ViewNodeScrollState::setOnScrollCallback(Ref<ValueFunction>&& onScrollCallback) {
    _onScrollCallback = std::move(onScrollCallback);
}

void ViewNodeScrollState::setOnScrollEndCallback(Ref<ValueFunction>&& onScrollEndCallback) {
    _onScrollEndCallback = std::move(onScrollEndCallback);
}

void ViewNodeScrollState::setOnDragStartCallback(Ref<ValueFunction>&& onDragStartCallback) {
    _onDragStartCallback = std::move(onDragStartCallback);
}

void ViewNodeScrollState::setOnDragEndingCallback(Ref<ValueFunction>&& onDragEndingCallback) {
    _onDragEndingCallback = std::move(onDragEndingCallback);
}

void ViewNodeScrollState::setOnContentSizeChangeCallback(Ref<ValueFunction>&& onContentSizeChangeCallback) {
    _onContentSizeChangeCallback = std::move(onContentSizeChangeCallback);
}

void ViewNodeScrollState::setOnDragEndCallback(Ref<ValueFunction>&& onDragEndCallback) {
    _onDragEndCallback = std::move(onDragEndCallback);
}

void ViewNodeScrollState::setViewportExtensionTop(float viewportExtensionTop) {
    _viewportExtensionTop = viewportExtensionTop;
}

void ViewNodeScrollState::setViewportExtensionBottom(float viewportExtensionBottom) {
    _viewportExtensionBottom = viewportExtensionBottom;
}

void ViewNodeScrollState::setViewportExtensionLeft(float viewportExtensionLeft) {
    _viewportExtensionLeft = viewportExtensionLeft;
}

void ViewNodeScrollState::setViewportExtensionRight(float viewportExtensionRight) {
    _viewportExtensionRight = viewportExtensionRight;
}

bool ViewNodeScrollState::onScrollCallbackPrefersSyncCalls() const {
    return false;
}

void ViewNodeScrollState::setMaintainScrollAnchor(bool maintain) {
    _maintainScrollAnchor = maintain;
}

bool ViewNodeScrollState::getMaintainScrollAnchor() const {
    return _maintainScrollAnchor;
}

} // namespace Valdi
