#import "valdi_core/UIView+ValdiBase.h"
#import "valdi_core/UIView+ValdiObjects.h"
#import "valdi_core/SCValdiTouches.h"

#import <objc/runtime.h>

static BOOL SCValdiLayerHasAnimation(CALayer *layer)
{
    return layer.animationKeys.count > 0;
}

@implementation UIView (ValdiBase)

- (BOOL)clipsToBoundsByDefault
{
    return NO;
}

- (BOOL)requiresShapeLayerForBorderRadius
{
    return NO;
}

- (void)invalidateLayout
{
    [self invalidateLayoutAndMarkFlexBoxDirty:YES];
}

- (void)invalidateLayoutAndMarkFlexBoxDirty:(BOOL)markFlexBoxDirty
{
    [self setNeedsLayout];

    if (markFlexBoxDirty) {
        [self.valdiViewNode markLayoutDirty];
    }
}

- (void)scrollSpecsDidChangeWithContentOffset:(CGPoint)contentOffset contentSize:(CGSize)contentSize animated:(BOOL)animated
{

}

- (BOOL)willEnqueueIntoValdiPool
{
    return self.class == [UIView class];
}

- (void)valdi_prepareForPoolReuse
{
    // Reset the CALayer transform to identity. Valdi animations set the CALayer
    // model value to the animation target before the CAAnimation starts visually.
    // If the animation is cancelled mid-flight the animation key is removed and the
    // layer snaps to that model value. By the time the pool enqueue fires the
    // animation key is gone, so it cannot be detected via animationKeys — the
    // transform must be reset unconditionally here.
    self.layer.transform = CATransform3DIdentity;
}

- (BOOL)requiresLayoutWhenAnimatingBounds
{
    return YES;
}

- (BOOL)valdi_requiresPointConversionOnPresentationLayer
{
    if (self.valdiContext.enableAccurateTouchGesturesInAnimations) {
        return SCValdiLayerHasAnimation(self.layer);
    }

    return NO;
}

- (CGPoint)valdi_convertPoint:(CGPoint)point fromView:(UIView *)view
{
    CALayer *layer = self.layer;
    if ([self valdi_requiresPointConversionOnPresentationLayer]) {
        CALayer *presentationLayer = layer.presentationLayer;
        if (presentationLayer) {
            return [presentationLayer convertPoint:point fromLayer:view.layer.presentationLayer];
        }
    }

    return [layer convertPoint:point fromLayer:view.layer];
}

- (CGPoint)valdi_convertPoint:(CGPoint)point toView:(UIView *)view
{
    CALayer *layer = self.layer;
    if ([self valdi_requiresPointConversionOnPresentationLayer]) {
        CALayer *presentationLayer = layer.presentationLayer;
        if (presentationLayer) {
            return [presentationLayer convertPoint:point toLayer:view.layer.presentationLayer];
        }
    }

    return [layer convertPoint:point toLayer:view.layer];
}

- (id<SCValdiFunction>)valdiHitTest
{
    return objc_getAssociatedObject(self, @selector(valdiHitTest));
}

- (void)setValdiHitTest:(id<SCValdiFunction>)attributeValue
{
    objc_setAssociatedObject(self, @selector(valdiHitTest), attributeValue, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (UIView *)valdi_hitTest:(CGPoint)point withEvent:(UIEvent *)event withCustomHitTest:(id<SCValdiFunction>)customHitTest
{
    if (!self.userInteractionEnabled || [self isHidden] || self.alpha == 0) {
        return nil;
    }

    // we recursively iterate through children only if the current view can be hitTested
    // - child subviews can have an opportunity to recieve touch events
    // - the view with the highest zIndex has priority
    // - children views that return nil aren't considered
    auto hitTestRet = SCValdiCallSyncActionWithUIEventAndView(customHitTest, point, event, self);
    if (hitTestRet) {
        for (UIView *subview in [self.subviews reverseObjectEnumerator]) {
            CGPoint convertedPoint = [subview convertPoint:point fromView:self];
            UIView *hitTestView = [subview hitTest:convertedPoint withEvent:event];
            if (hitTestView) {
                return hitTestView;
            }
        }

        return self;
    }
    return nil;
}

@end
