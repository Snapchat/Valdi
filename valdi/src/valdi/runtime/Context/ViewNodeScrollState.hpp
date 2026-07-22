//
//  ViewNodeScrollState.hpp
//  valdi
//
//  Created by Simon Corsin on 8/6/21.
//

#pragma once

#include "valdi/runtime/Context/RawViewNodeId.hpp"
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

    // Preserve scroll position: when enabled, updateScrollState anchors the first on-screen
    // child and shifts contentOffset.y by exactly the content-height change that occurred
    // ABOVE it, so on-screen content stays in place. Unlike the net content-size delta this
    // replaced, it stays correct when content is added at one end and trimmed at the other in
    // the same pass (windowed message lists). Intended for live-update scenarios where the user
    // is scrolled away from the newest end and shouldn't be bumped when new content lands.
    void setPreserveScrollPosition(bool preserve);
    bool getPreserveScrollPosition() const;

    // Native sticky headers: opt-in per-scroll flag. When true, ViewNode::updateStickyHeaders
    // repositions any descendant with stickyPosition != 0 inside the native scroll pass,
    // skipping the JS round-trip that lags the current JS-side sticky implementation.
    void setNativeStickyEnabled(bool enabled);
    bool getNativeStickyEnabled() const;

    // Pixels of visual overhang above sticky headers. Extends the effective header
    // height in the sticky clamp so a header rendering a bar above its yoga bounds
    // (SectionList.stickyCover) stops sliding before the next section arrives.
    void setNativeStickyCover(float cover);
    float getNativeStickyCover() const;

    // Pixels below scroll viewport top where sticky headers pin. Matches CSS
    // `position: sticky; top: N`. Shifts the pin position down for consumers where
    // a floating page header has visual footprint (gradient/shadow/animated height)
    // extending below its Yoga bounds.
    void setNativeStickyOffset(float offset);
    float getNativeStickyOffset() const;

    // Anchor memory for preserveScrollPosition: the id of the first on-screen child and its
    // screen position (= absolute Y minus content offset) at record time. The anchor is refreshed
    // on every scroll event and layout pass (so it never goes stale), and updateScrollState pins
    // it back to this screen position when the viewport is stationary -- which survives the offset
    // being clamped or scrolled between record and apply.
    bool hasPreserveAnchor() const;
    RawViewNodeId getPreserveAnchorId() const;
    float getPreserveAnchorScreenPos() const;
    void setPreserveAnchor(RawViewNodeId id, float screenPos);
    void clearPreserveAnchor();

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
    bool _preserveScrollPosition = false;
    bool _hasPreserveAnchor = false;
    bool _nativeStickyEnabled = false;
    float _nativeStickyCover = 0.0f;
    float _nativeStickyOffset = 0.0f;
    RawViewNodeId _preserveAnchorId = 0;
    float _preserveAnchorScreenPos = 0.0f;

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
