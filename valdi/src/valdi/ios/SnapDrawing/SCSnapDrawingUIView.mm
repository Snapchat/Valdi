//
//  SCSnapDrawingUIView.m
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/22.
//

#import "SCSnapDrawingUIView.h"
#import "SCSnapDrawingUIView+CPP.h"
#import "valdi_core/SCSnapDrawingRuntime.h"

#include "snap_drawing/cpp/Drawing/DrawLooper.hpp"

#include "snap_drawing/cpp/Drawing/Surface/SurfacePresenterManager.hpp"

#import "valdi/ios/SnapDrawing/SCValdiSnapDrawingSurfacePresenterManager.h"
#import "valdi/ios/SnapDrawing/SCValdiSurfacePresenterView.h"

#import <QuartzCore/CAMetalLayer.h>

static CGFloat resolveContentsScale() {
    // For some reasons, we don't see the contentScale being propagated properly
    // through the UIKit level.
    return [[UIScreen mainScreen] nativeScale];
}

@interface SCSnapDrawingUIView () <SCSnapDrawingMetalSurfacePresenterManagerDelegate, SCValdiSurfacePresenterViewDelegate>
- (void)onViewportDisplayLinkTick;
@end

// CADisplayLink retains its target, so target it at a weak proxy to avoid keeping the view alive.
@interface SCSnapDrawingViewportDisplayLinkProxy : NSObject
@property (nonatomic, weak) SCSnapDrawingUIView *view;
@end

@implementation SCSnapDrawingViewportDisplayLinkProxy
- (void)tick { [self.view onViewportDisplayLinkTick]; }
@end

@implementation SCSnapDrawingUIView {
    id<SCSnapDrawingRuntime> _runtime;
    CADisplayLink *_viewportDisplayLink;
    CGRect _lastVisibleRect;
}

- (instancetype)initWithRuntime:(id<SCSnapDrawingRuntime>)runtime
{
    self = [super init];

    if (self) {
        _runtime = runtime;

        auto cppRuntime = [self cppRuntime];

        _layerRoot = snap::drawing::makeLayer<snap::drawing::LayerRoot>(cppRuntime->getResources());

        auto graphicsContext = Valdi::castOrNull<snap::drawing::MetalGraphicsContext>(cppRuntime->getGraphicsContext());
        auto presenterManager = Valdi::makeShared<snap::drawing::IOSSurfacePresenterManager>(self, graphicsContext);

        cppRuntime->getDrawLooper()->addLayerRoot(_layerRoot, presenterManager, false);
    }

    return self;
}

- (void)dealloc
{
    [self stopViewportTracking];

    auto *cppRuntime = [self cppRuntime];
    if (cppRuntime == nullptr) {
        return;
    }

    cppRuntime->getDrawLooper()->removeLayerRoot(_layerRoot);
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    _layerRoot->setSize(snap::drawing::Size::make(self.bounds.size.width, self.bounds.size.height), resolveContentsScale());
    [self refreshRenderViewport];
}

// The on-screen portion of this view, in its own (content) coordinate space. When the view is laid
// out far larger than the screen (e.g. a pinch-zoomed sticker), this is a small sub-rect of bounds,
// so we render/allocate only that region instead of a drawable for the full content.
- (CGRect)visibleContentRect
{
    CGRect bounds = self.bounds;
    UIWindow *window = self.window;
    if (window == nil || CGRectIsEmpty(bounds)) {
        return bounds;
    }
    // Convert through the *presentation* layers, not -convertRect:fromView: (which uses the model
    // geometry). During an animated move the model jumps to the destination immediately while the
    // presentation layer is what's actually on screen, so model geometry would clamp to the
    // destination mid-flight and reveal the edge. Presentation layers reflect the current on-screen
    // (interpolated) position; they fall back to the model layer when nothing is animating, so this
    // is also correct for static/gesture cases. Bounds don't animate for a move, so the model
    // bounds are the right region to intersect.
    CALayer *selfLayer = self.layer.presentationLayer ?: self.layer;
    CALayer *windowLayer = window.layer.presentationLayer ?: window.layer;
    CGRect windowInSelf = [selfLayer convertRect:window.bounds fromLayer:windowLayer];
    CGRect visible = CGRectIntersection(bounds, windowInSelf);
    if (CGRectIsNull(visible)) {
        return CGRectZero;
    }
    return visible;
}

- (void)refreshRenderViewport
{
    CGRect visible = [self visibleContentRect];
    _lastVisibleRect = visible;
    [self applyVisibleContentRect:visible];
    [self updateViewportTracking:visible];
}

- (void)applyVisibleContentRect:(CGRect)visible
{
    _layerRoot->setRenderViewport(
        snap::drawing::Rect::makeXYWH(visible.origin.x, visible.origin.y, visible.size.width, visible.size.height));

    CGRect bounds = self.bounds;
    for (UIView *subview in self.subviews) {
        // Only Skia-backed drawable surfaces (CAMetalLayer) are clamped to the visible region --
        // that is what caps GPU memory. Embedded native-view presenters keep the full bounds and
        // are positioned by their own frame/transform via setEmbeddedViewFrame. CAMetalLayer is
        // iOS 13+, matching the surface presenters; pre-13 there are no metal surfaces to clamp.
        BOOL isMetalDrawable = NO;
        if (@available(iOS 13.0, *)) {
            isMetalDrawable = [subview.layer isKindOfClass:[CAMetalLayer class]];
        }
        subview.frame = isMetalDrawable ? visible : bounds;
        [subview layoutIfNeeded];
    }
}

// A pan/scroll moves an ancestor, which never notifies this view (layoutSubviews only fires on
// size changes). So while the view is clamped -- i.e. it extends beyond the visible region -- run a
// display link to poll the on-screen position each frame and re-clamp when it moves. This is
// agnostic to which view actually moved. Stopped entirely once the view fully fits on screen (no
// clamping needed) or leaves the window, so there is no cost for normal-sized content.
- (void)updateViewportTracking:(CGRect)visible
{
    BOOL clamping = (self.window != nil) && !CGRectEqualToRect(visible, self.bounds);
    if (clamping) {
        if (_viewportDisplayLink == nil) {
            SCSnapDrawingViewportDisplayLinkProxy *proxy = [SCSnapDrawingViewportDisplayLinkProxy new];
            proxy.view = self;
            _viewportDisplayLink = [CADisplayLink displayLinkWithTarget:proxy selector:@selector(tick)];
            [_viewportDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
        }
    } else {
        [self stopViewportTracking];
    }
}

- (void)stopViewportTracking
{
    [_viewportDisplayLink invalidate];
    _viewportDisplayLink = nil;
}

- (void)onViewportDisplayLinkTick
{
    CGRect visible = [self visibleContentRect];
    if (!CGRectEqualToRect(visible, _lastVisibleRect)) {
        _lastVisibleRect = visible;
        [self applyVisibleContentRect:visible];
        [self updateViewportTracking:visible];
    }
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    if (self.window == nil) {
        [self stopViewportTracking];
    } else {
        [self refreshRenderViewport];
    }
}

- (snap::drawing::Runtime *)cppRuntime
{
    return Valdi::unsafeBridgeUnretained<snap::drawing::Runtime>([_runtime handle]);
}

- (id)createEmbeddedPresenterForView:(id)view
{
    return [SCValdiSurfacePresenterView presenterViewWithEmbeddedView:view];
}

- (id)createMetalPresenter:(inout CAMetalLayer *__autoreleasing *)metalLayer API_AVAILABLE(ios(13.0)) {
    SCValdiSurfacePresenterView *presenterView = [SCValdiSurfacePresenterView presenterViewWithMetalLayer:metalLayer contentsScale:resolveContentsScale()];
    presenterView.delegate = self;

    return presenterView;
}

- (void)removePresenter:(id)presenter
{
    [(UIView *)presenter removeFromSuperview];
}

- (void)setFrame:(CGRect)frame transform:(CATransform3D)transform opacity:(CGFloat)opacity clipPath:(CGPathRef)clipPath clipHasChanged:(BOOL)clipHasChanged forEmbeddedPresenter:(id)presenter
{
    [(SCValdiSurfacePresenterView *)presenter
     setEmbeddedViewFrame:frame
     transform:transform
     opacity:opacity
     clipPath:clipPath
     clipHasChanged:clipHasChanged];
}

- (void)setZIndex:(NSUInteger)zIndex forPresenter:(id)presenter
{
    SCValdiSurfacePresenterView *presenterView = presenter;

    [presenterView removeFromSuperview];

    [self insertSubview:presenterView atIndex:zIndex];

    // Set the new presenter's frame together with the current viewport so the drawable size and the
    // display list's viewport stay consistent (a stale viewport would render at the wrong scale).
    [self refreshRenderViewport];
}

- (void)surfacePresenterView:(SCValdiSurfacePresenterView *)surfaceView willResizeDrawableWithBlock:(SCValdiSurfacePresenterViewResizeBlock)block
{
    auto *cppRuntime = [self cppRuntime];
    if (cppRuntime == nullptr) {
        return;
    }
    auto drawLock = cppRuntime->getDrawLooper()->getDrawLock();
    block();
}

@end
