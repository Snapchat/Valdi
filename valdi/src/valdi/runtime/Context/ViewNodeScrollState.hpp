//
//  ViewNodeScrollState.hpp
//  valdi
//
//  Created by Simon Corsin on 8/6/21.
//

#pragma once

#include "valdi/runtime/Views/Frame.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <optional>

namespace Valdi {

struct ViewNodeScrollStateUpdateContentSizeResult {
    bool changed = false;
    bool contentOffsetAdjusted = false;
};

class ViewNodeScrollState {
public:
    ViewNodeScrollState();
    ~ViewNodeScrollState();

    bool isInScrollMode() const;
    void setInScrollMode(bool inScrollMode);

    bool isCurrentlyAnimating() const;
    void setCurrentlyAnimating(bool currentlyAnimating);

    bool isScrolling() const;

    bool needsSyncWithView() const;
    void setNeedsSyncWithView(bool needsSyncWithView);

    const Point& getDirectionAgnosticContentOffset() const;

    Point getDirectionDependentContentOffset() const;
    void resolveClipRect(Frame& outClipRect) const;

    const Size& getContentSize() const;

    void setStaticContentWidth(float staticContentWidth);
    float getStaticContentWidth() const;

    void setStaticContentHeight(float staticContentHeight);
    float getStaticContentHeight() const;

    void setCircularRatio(int circularRatio);
    int getCircularRatio() const;

    void setIsHorizontal(bool isHorizontal);
    bool getIsHorizontal() const;

    // In RTL mode with overflow enabled, Yoga lays out from right to left
    // and items that overflow end up having a negative position. This will
    // contain the offset that need to be applied so that items start at x=0
    // instead of x=-rtlOffsetX.
    float getRtlOffsetX() const;

    Point resolveDirectionDependentContentOffset(const Point& directionAgnosticContentOffset) const;
    Point resolveDirectionAgnosticContentOffset(const Point& directionDependentContentOffset) const;

    ViewNodeScrollStateUpdateContentSizeResult updateContentSizeAndRtlOffset(
        const Size& contentSize, const Size& viewportSize, float rtlOffsetX, float pointScale, bool isHorizontal);

    bool updateDirectionDependentContentOffset(const Point& directionDependentContentOffset,
                                               const Point& directionDependentUnclampedContentOffset);
    bool updateDirectionAgnosticContentOffset(const Point& directionAgnosticContentOffset,
                                              const Point& directionAgnosticUnclampedContentOffset);

    void notifyOnScroll(const Point& directionAgnosticContentOffset,
                        const Point& directionAgnosticUnclampedContentOffset,
                        const Point& directionAgnosticVelocity) const;
    void notifyOnScrollEnd(const Point& directionAgnosticContentOffset,
                           const Point& directionAgnosticUnclampedContentOffset);
    void notifyOnDragStart(const Point& directionAgnosticContentOffset,
                           const Point& directionAgnosticUnclampedContentOffset,
                           const Point& directionAgnosticVelocity);
    void notifyOnDragEnd(const Point& directionAgnosticContentOffset,
                         const Point& directionAgnosticUnclampedContentOffset,
                         const Point& directionAgnosticVelocity) const;

    Result<std::optional<Point>> notifyOnDragEnding(const Point& directionAgnosticContentOffset,
                                                    const Point& directionAgnosticUnclampedContentOffset,
                                                    const Point& directionAgnosticVelocity) const;

    void setOnScrollCallback(Ref<ValueFunction>&& onScrollCallback);
    void setOnScrollEndCallback(Ref<ValueFunction>&& onScrollEndCallback);
    void setOnDragStartCallback(Ref<ValueFunction>&& onDragStartCallback);
    void setOnDragEndingCallback(Ref<ValueFunction>&& onDragEndingCallback);
    void setOnDragEndCallback(Ref<ValueFunction>&& onDragEndCallback);
    void setOnContentSizeChangeCallback(Ref<ValueFunction>&& onContentSizeChangeCallback);

    void setViewportExtensionTop(float viewportExtensionTop);
    void setViewportExtensionBottom(float viewportExtensionBottom);
    void setViewportExtensionLeft(float viewportExtensionLeft);
    void setViewportExtensionRight(float viewportExtensionRight);

    bool onScrollCallbackPrefersSyncCalls() const;

    // Scroll anchor: when enabled, updateScrollState will find a descendant with
    // scrollAnchorPosition != 0 and pin the scroll offset to keep it at the specified
    // viewport edge (1=top, 2=bottom). TS toggles this on during pagination and off after layout settles.
    void setMaintainScrollAnchor(bool maintain);
    bool getMaintainScrollAnchor() const;

private:
    Point _directionAgnosticContentOffset;
    Point _directionAgnosticUnclampedContentOffset;
    Size _contentSize;
    Size _viewportSize;

    float _rtlOffsetX = 0.0f;
    float _pointScale = 1.0;
    float _viewportExtensionLeft = 0.0f;
    float _viewportExtensionRight = 0.0f;
    float _viewportExtensionTop = 0.0f;
    float _viewportExtensionBottom = 0.0f;
    float _staticContentWidth = 0.0f;
    float _staticContentHeight = 0.0f;
    int _circularRatio = 0;
    bool _scrolling = false;
    bool _needsSyncWithView = true;
    bool _currentlyAnimating = false;
    bool _inScrollMode = false;
    bool _isHorizontal = false;
    bool _maintainScrollAnchor = false;

    Ref<ValueFunction> _onScrollCallback;
    Ref<ValueFunction> _onScrollEndCallback;
    Ref<ValueFunction> _onDragStartCallback;
    Ref<ValueFunction> _onDragEndingCallback;
    Ref<ValueFunction> _onDragEndCallback;
    Ref<ValueFunction> _onContentSizeChangeCallback;

    static void submitScrollEvent(const Ref<ValueFunction>& callback,
                                  ValueFunctionFlags callFlags,
                                  const Point& directionAgnosticContentOffset,
                                  const Point& directionAgnosticUnclampedContentOffset,
                                  const Point& directionAgnosticVelocity);
    static void submitScrollEndEvent(const Ref<ValueFunction>& callback,
                                     ValueFunctionFlags callFlags,
                                     const Point& directionAgnosticContentOffset,
                                     const Point& directionAgnosticUnclampedContentOffset,
                                     const Point& directionAgnosticVelocity);
    Point resolveContentOffset(const Point& convertedContentOffset, bool directionAgnostic) const;

    void notifyContentSizeChanged() const;
};

} // namespace Valdi
