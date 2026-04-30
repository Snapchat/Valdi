//
//  SCValdiSurfacePresenterView.m
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 2/14/22.
//

#import "SCValdiSurfacePresenterView.h"
#import <QuartzCore/CAShapeLayer.h>

@interface SCValdiSurfacePresenterView()

@property (strong, nonatomic) NSView *embeddedView;

@end


@interface SCValdiMetalSurfaceView: SCValdiSurfacePresenterView
@end

@implementation SCValdiMetalSurfaceView

- (void)layout
{
    CGRect bounds = self.bounds;
    CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
    CGFloat contentsScale = metalLayer.contentsScale;
    CGSize drawableSize = CGSizeMake(bounds.size.width * contentsScale, bounds.size.height * contentsScale);
    if (!CGSizeEqualToSize(metalLayer.drawableSize, drawableSize)) {
        [self.delegate surfacePresenterView:self willResizeDrawableWithBlock:^{
            metalLayer.drawableSize = drawableSize;
        }];
    }
}

@end

@interface SCValdiEmbeddedSurfaceView: SCValdiSurfacePresenterView
@end

@implementation SCValdiEmbeddedSurfaceView

- (void)layout
{
    NSView *embeddedView = self.embeddedView;
    embeddedView.layer.transform = self.viewTransform;
    embeddedView.frame = self.viewFrame;
}

- (NSSize)fittingSize
{
    return [self.embeddedView fittingSize];
}

@end

@implementation SCValdiSurfacePresenterView {
    CAShapeLayer *_clipLayer;
}

- (instancetype)init
{
    self = [super init];

    if (self) {
        self.translatesAutoresizingMaskIntoConstraints = NO;
    }

    return self;
}

- (BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window
{
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSView *)hitTest:(NSPoint)point
{
    NSView *view = [super hitTest:point];
    if (view == self) {
        return nil;
    }

    return view;
}

- (void)mouseDown:(NSEvent *)event
{
}

- (void)mouseUp:(NSEvent *)event
{
}

- (void)mouseDragged:(NSEvent *)event
{
}

- (void)setEmbeddedViewFrame:(CGRect)frame transform:(CATransform3D)transform opacity:(CGFloat)opacity clipPath:(CGPathRef)clipPath clipHasChanged:(BOOL)clipHasChanged
{
    _viewFrame = frame;
    _viewTransform = transform;

    if (!_embeddedView) {
        return;
    }

    _embeddedView.layer.transform = transform;
    _embeddedView.frame = frame;
    _embeddedView.alphaValue = opacity;

    if (clipHasChanged) {
        if (clipPath) {
            if (!_clipLayer) {
                _clipLayer = [CAShapeLayer layer];
            }

            _clipLayer.path = clipPath;

            if (!self.layer.mask) {
                self.layer.mask = _clipLayer;
            }
        } else {
            _clipLayer.path = nil;
            self.layer.mask = nil;
        }
    }
}

+ (instancetype)presenterViewWithMetalLayer:(CAMetalLayer *)metalLayer
{
    SCValdiSurfacePresenterView *surfaceView = [SCValdiMetalSurfaceView new];
    surfaceView.layer = metalLayer;
    metalLayer.delegate = surfaceView;

    return surfaceView;
}

+ (instancetype)presenterViewWithEmbeddedView:(NSView *)embeddedView
{
    SCValdiSurfacePresenterView *surfaceView = [SCValdiEmbeddedSurfaceView new];
    surfaceView.wantsLayer = YES;
    surfaceView.embeddedView = embeddedView;

    embeddedView.translatesAutoresizingMaskIntoConstraints = YES;

    [surfaceView addSubview:embeddedView];

    return surfaceView;
}

@end
