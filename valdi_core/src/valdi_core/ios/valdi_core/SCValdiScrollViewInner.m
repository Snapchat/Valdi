#import "valdi_core/SCValdiScrollViewInner.h"

@implementation SCValdiScrollViewInner

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        self.horizontalScroll = NO;
        self.bouncesFromDragAtStart = YES;
        self.bouncesFromDragAtEnd = YES;
        self.panGestureRecognizerEnabled = YES;
        self.stopScrollingOnTouch = NO;
    }
    return self;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.stopScrollingOnTouch && self.isDecelerating && [self pointInside:point withEvent:event]) {
        [self setContentOffset:self.contentOffset animated:NO];
        [self.layer removeAllAnimations];
    }
    return [super hitTest:point withEvent:event];
}

- (void)setPanGestureRecognizerEnabled:(BOOL)panGestureRecognizerEnabled
{
    _panGestureRecognizerEnabled = panGestureRecognizerEnabled;
    self.panGestureRecognizer.enabled = _panGestureRecognizerEnabled;
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    // panGestureRecognizer's state gets reset when its parent view is moved around so we
    // reinitialize it here.
    self.panGestureRecognizer.enabled = _panGestureRecognizerEnabled;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    // We only care about the panGestureRecognizer here
    if (self.panGestureRecognizer != gestureRecognizer) {
        return [super gestureRecognizerShouldBegin:gestureRecognizer];
    }
    // If no special behavior was registered
    if (self.bouncesFromDragAtStart && self.bouncesFromDragAtEnd) {
        return [super gestureRecognizerShouldBegin:gestureRecognizer];
    }
    // Read the necessary current states
    CGPoint fingerVelocity = [self.panGestureRecognizer velocityInView:self];
    CGSize boundsSize = self.bounds.size;
    CGPoint contentOffset = self.contentOffset;
    CGSize contentSize = self.contentSize;
    // Vertical mode, we only care about Y
    CGFloat fingerVelocityValue = fingerVelocity.y;
    CGFloat boundsSizeValue = boundsSize.height;
    CGFloat contentOffsetValue = contentOffset.y;
    CGFloat contentSizeValue = contentSize.height;
    // Horizontal mode, we only care about X
    if (self.horizontalScroll) {
        fingerVelocityValue = fingerVelocity.x;
        boundsSizeValue = boundsSize.width;
        contentOffsetValue = contentOffset.x;
        contentSizeValue = contentSize.width;
    }
    // Only needed if we want to be able to ignore a backward scroll
    if (!self.bouncesFromDragAtStart) {
        // Scrolling backward (finger moving forward), check if we're already at the origin
        if (fingerVelocityValue > 0 && contentOffsetValue == 0) {
            return NO;
        }
        // Bouncing at lower limit (finger velocity may be innacurate due to bounce, must still forbid gesture)
        else if (contentOffsetValue < 0) {
            return NO;
        }
    }
    // Only needed if we want to be able to ignore a forward scroll
    if (!self.bouncesFromDragAtEnd) {
        // The current limit of the visible content
        CGFloat contentLimitValue = contentOffsetValue + boundsSizeValue;
        // Scrolling forward (finger moving backward), check if we're already at the max content size limit
        if (fingerVelocityValue < 0 && contentLimitValue == contentSizeValue) {
            return NO;
        }
        // Bouncing at upper limit (finger velocity may be innacurate due to bounce, must still forbid gesture)
        else if (contentLimitValue > contentSizeValue) {
            return NO;
        }
    }
    // If we didn't swallow the drag
    return [super gestureRecognizerShouldBegin:gestureRecognizer];
}

@end
