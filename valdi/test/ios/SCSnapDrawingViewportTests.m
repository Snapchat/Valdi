#import <QuartzCore/QuartzCore.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/SnapDrawing/SCSnapDrawingViewport.h"

static const CGRect kScreen = {{0, 0}, {393, 852}};
static const CGFloat kContentsScale = 3;

// A window, the sticker container that carries the gesture transform, and the SCSnapDrawingUIView layer.
@interface SCSnapDrawingTestTree : NSObject
@property (nonatomic, strong) CALayer *window;
@property (nonatomic, strong) CALayer *container;
@property (nonatomic, strong) CALayer *view;
@end

@implementation SCSnapDrawingTestTree

- (instancetype)init
{
    self = [super init];
    if (self) {
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        _window = [CALayer layer];
        _window.frame = kScreen;
        _container = [CALayer layer];
        _view = [CALayer layer];
        [_window addSublayer:_container];
        [_container addSublayer:_view];
        [CATransaction commit];
    }
    return self;
}

// Applies geometry without implicit animations, like UIKit does for view-backed layers.
- (void)setStickerSize:(CGSize)size center:(CGPoint)center scale:(CGFloat)scale
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _container.bounds = CGRectMake(0, 0, size.width, size.height);
    _container.position = center;
    _container.transform = CATransform3DMakeScale(scale, scale, 1);
    _view.bounds = _container.bounds;
    _view.position = CGPointMake(size.width / 2, size.height / 2);
    [CATransaction commit];
}

@end

// Hostless XCTest bundles have no render context, so real presentation layers are never created. `_presented`
// stands in for them: it holds the last committed geometry while `_model` holds the current transaction's.
@interface SCSnapDrawingViewportTests : XCTestCase
@end

@implementation SCSnapDrawingViewportTests {
    SCSnapDrawingTestTree *_model;
    SCSnapDrawingTestTree *_presented;
}

- (void)setUp
{
    [super setUp];
    _model = [SCSnapDrawingTestTree new];
    _presented = [SCSnapDrawingTestTree new];
}

- (void)setStickerSize:(CGSize)size center:(CGPoint)center scale:(CGFloat)scale
{
    [_model setStickerSize:size center:center scale:scale];
    [_presented setStickerSize:size center:center scale:scale];
}

- (CGRect)visibleRect
{
    CGRect windowRect =
        SCSnapDrawingWindowRectInLayers(_model.view, _model.window, _presented.view, _presented.window, kScreen);
    return SCSnapDrawingVisibleRect(_model.view.bounds, windowRect);
}

- (CGRect)modelVisibleRect
{
    return SCSnapDrawingVisibleRect(_model.view.bounds, [_model.view convertRect:kScreen fromLayer:_model.window]);
}

- (CGRect)presentationVisibleRect
{
    return SCSnapDrawingVisibleRect(_model.view.bounds,
                                    [_presented.view convertRect:kScreen fromLayer:_presented.window]);
}

- (void)assertRect:(CGRect)actual equals:(CGRect)expected
{
    XCTAssertEqualWithAccuracy(actual.origin.x, expected.origin.x, 0.5);
    XCTAssertEqualWithAccuracy(actual.origin.y, expected.origin.y, 0.5);
    XCTAssertEqualWithAccuracy(actual.size.width, expected.size.width, 0.5);
    XCTAssertEqualWithAccuracy(actual.size.height, expected.size.height, 0.5);
}

// The drawable a SCValdiSurfacePresenterView allocates for the region, against one screen's worth.
- (void)assertDrawableAtMostScreen:(CGRect)visible
{
    XCTAssertLessThanOrEqual(visible.size.width * kContentsScale, kScreen.size.width * kContentsScale + 0.5);
    XCTAssertLessThanOrEqual(visible.size.height * kContentsScale, kScreen.size.height * kContentsScale + 0.5);
}

// The pinch bake on lift: the sticker grows by `scale` while its ancestor scale drops back to 1 in the same
// transaction, so the presentation still shows the small, scaled sticker.
- (void)pinchBakeFromSize:(CGSize)size center:(CGPoint)center scale:(CGFloat)scale
{
    [self setStickerSize:size center:center scale:scale];
    [_model setStickerSize:CGSizeMake(size.width * scale, size.height * scale) center:center scale:1];
}

- (void)testPinchBakeUsesTheGeometryTheNextFrameShows
{
    [self pinchBakeFromSize:CGSizeMake(200, 200) center:CGPointMake(250, 426) scale:3];

    CGRect visible = [self visibleRect];
    CGRect onScreen = [self modelVisibleRect];
    XCTAssertFalse(CGRectContainsRect([self presentationVisibleRect], onScreen),
                   @"repro: the stale presentation region cuts the sticker");
    [self assertRect:visible equals:onScreen];
    [self assertDrawableAtMostScreen:visible];
}

- (void)testPinchBakeWithANonGeometryAnimationStillUsesTheModel
{
    CABasicAnimation *fade = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fade.duration = 1000;
    [_model.container addAnimation:fade forKey:@"opacity"];

    [self pinchBakeFromSize:CGSizeMake(200, 200) center:CGPointMake(250, 426) scale:3];
    [self assertRect:[self visibleRect] equals:[self modelVisibleRect]];
}

// An animated move sets the model to the destination up front; the clamp must follow what is on screen,
// not the destination and not a union of the two.
- (void)testAnimatedMoveFollowsThePresentation
{
    CGSize size = CGSizeMake(3000, 3000);
    CGPoint from = CGPointMake(196, 426);
    [self setStickerSize:size center:from scale:1];
    CGRect atStart = [self modelVisibleRect];

    [_model setStickerSize:size center:CGPointMake(-1500, -1500) scale:1];
    CABasicAnimation *move = [CABasicAnimation animationWithKeyPath:@"position"];
    move.duration = 1000;
    [_model.container addAnimation:move forKey:@"position"];

    CGRect visible = [self visibleRect];
    XCTAssertFalse(CGRectEqualToRect(atStart, [self modelVisibleRect]));
    [self assertRect:visible equals:atStart];
    [self assertDrawableAtMostScreen:visible];
}

- (void)testAbsurdZoomStaysScreenSizedDuringAndAfterThePinch
{
    CGSize size = CGSizeMake(300, 300);
    CGPoint center = CGPointMake(196.5, 426);
    CGFloat scale = 50;

    [self setStickerSize:size center:center scale:scale];
    [self assertRect:[self visibleRect]
              equals:CGRectMake(150 - kScreen.size.width / 2 / scale,
                                150 - kScreen.size.height / 2 / scale,
                                kScreen.size.width / scale,
                                kScreen.size.height / scale)];

    [self pinchBakeFromSize:size center:center scale:scale];
    CGRect onLift = [self visibleRect];
    [self assertRect:onLift equals:[self modelVisibleRect]];
    [self assertDrawableAtMostScreen:onLift];
    XCTAssertTrue(SCSnapDrawingShouldTrackViewport(YES, _model.view.bounds, onLift));

    // Committed: the presentation catches up and the region does not change.
    [_presented setStickerSize:_model.view.bounds.size center:center scale:1];
    [self assertRect:[self visibleRect] equals:onLift];
}

- (void)testStickerThatFitsAfterLiftIsNotClampedOrTracked
{
    [self pinchBakeFromSize:CGSizeMake(60, 60) center:CGPointMake(196, 426) scale:3];
    CGRect visible = [self visibleRect];
    XCTAssertTrue(CGRectEqualToRect(visible, _model.view.bounds));
    XCTAssertFalse(SCSnapDrawingShouldTrackViewport(YES, _model.view.bounds, visible));
}

- (void)testOversizedViewIsTrackedOnlyInAWindow
{
    [self setStickerSize:CGSizeMake(4000, 4000) center:CGPointMake(196, 426) scale:1];
    CGRect visible = [self visibleRect];
    [self assertDrawableAtMostScreen:visible];
    XCTAssertTrue(SCSnapDrawingShouldTrackViewport(YES, _model.view.bounds, visible));
    XCTAssertFalse(SCSnapDrawingShouldTrackViewport(NO, _model.view.bounds, visible));
}

- (void)testOffscreenViewHasEmptyRegion
{
    [self setStickerSize:CGSizeMake(100, 100) center:CGPointMake(2000, 2000) scale:1];
    XCTAssertTrue(CGRectEqualToRect([self visibleRect], CGRectZero));
}

- (void)testUnknownAnimationsCountAsGeometric
{
    XCTAssertFalse(SCSnapDrawingLayerChainHasGeometryAnimation(_model.view));

    CAAnimationGroup *group = [CAAnimationGroup animation];
    group.duration = 1000;
    [_model.container addAnimation:group forKey:@"group"];
    XCTAssertTrue(SCSnapDrawingLayerChainHasGeometryAnimation(_model.view));

    [_model.container removeAllAnimations];
    CABasicAnimation *scale = [CABasicAnimation animationWithKeyPath:@"transform.scale"];
    scale.duration = 1000;
    [_model.window addAnimation:scale forKey:@"scale"];
    XCTAssertTrue(SCSnapDrawingLayerChainHasGeometryAnimation(_model.view));
}

@end
