#import <XCTest/XCTest.h>

#import "valdi/ios/Utils/GradientUtils.h"
#import "valdi/ios/Views/SCValdiShapeView.h"
#import "valdi_core/SCValdiAnimatorBase.h"

@interface SCValdiShapeView (Tests)

- (CAShapeLayer *)shapeLayer;
- (void)valdi_setPath:(id)pathData animator:(id<SCValdiAnimatorProtocol>)animator;
- (void)valdi_setStrokeStart:(CGFloat)strokeStart animator:(id<SCValdiAnimatorProtocol>)animator;
- (void)valdi_setStrokeEnd:(CGFloat)strokeEnd animator:(id<SCValdiAnimatorProtocol>)animator;
- (BOOL)valdi_setFillGradient:(NSArray *)attributeValue animator:(id<SCValdiAnimatorProtocol>)animator;
- (void)_resetFillGradientWithAnimator:(id<SCValdiAnimatorProtocol>)animator;
- (void)_setFillColor:(UIColor *)color animator:(id<SCValdiAnimatorProtocol>)animator;
- (void)_setStrokeColor:(UIColor *)color animator:(id<SCValdiAnimatorProtocol>)animator;
- (void)_setLineWidth:(CGFloat)lineWidth animator:(id<SCValdiAnimatorProtocol>)animator;

@end

@interface SCValdiShapeViewTests : XCTestCase
@end

@implementation SCValdiShapeViewTests

- (NSData *)rectangularPathData
{
    const double values[] = {
        100, 100, 1,
        1, 10, 10,
        2, 90, 10,
        2, 90, 90,
        2, 10, 90,
        7,
    };
    return [NSData dataWithBytes:values length:sizeof(values)];
}

- (NSArray *)linearGradient
{
    return @[@[@0xFF0000FF, @0x00FF00FF], @[@0, @1], @(SCValdiLinearGradientOrientationLeftRight), @NO];
}

- (CAGradientLayer *)gradientLayerForView:(SCValdiShapeView *)view
{
    for (CALayer *layer in view.layer.sublayers) {
        if ([layer isKindOfClass:CAGradientLayer.class]) {
            return (CAGradientLayer *)layer;
        }
    }
    return nil;
}

- (CAShapeLayer *)strokeLayerForView:(SCValdiShapeView *)view
{
    for (CALayer *layer in view.layer.sublayers) {
        if ([layer isKindOfClass:CAShapeLayer.class]) {
            return (CAShapeLayer *)layer;
        }
    }
    return nil;
}

- (void)testLinearFillGradientUsesShapeMaskAndPreservesStroke
{
    SCValdiShapeView *view = [[SCValdiShapeView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    [view valdi_setPath:[self rectangularPathData] animator:nil];
    [view _setFillColor:UIColor.blueColor animator:nil];
    [view _setStrokeColor:UIColor.blackColor animator:nil];
    [view _setLineWidth:4 animator:nil];

    XCTAssertTrue([view valdi_setFillGradient:[self linearGradient] animator:nil]);

    CAGradientLayer *gradientLayer = [self gradientLayerForView:view];
    CAShapeLayer *maskLayer = (CAShapeLayer *)gradientLayer.mask;
    CAShapeLayer *strokeLayer = [self strokeLayerForView:view];

    XCTAssertNotNil(gradientLayer);
    XCTAssertEqualObjects(gradientLayer.type, kCAGradientLayerAxial);
    XCTAssertEqualObjects(gradientLayer.locations, (@[@0, @1]));
    XCTAssertTrue(CGPointEqualToPoint(gradientLayer.startPoint, CGPointMake(0, 0.5)));
    XCTAssertTrue(CGPointEqualToPoint(gradientLayer.endPoint, CGPointMake(1, 0.5)));
    XCTAssertTrue(CGRectEqualToRect(gradientLayer.frame, view.bounds));
    XCTAssertTrue(CGPathEqualToPath(maskLayer.path, view.shapeLayer.path));
    XCTAssertEqual(view.shapeLayer.fillColor, NULL);
    XCTAssertEqual(view.shapeLayer.strokeColor, NULL);
    XCTAssertTrue(CGColorEqualToColor(strokeLayer.strokeColor, UIColor.blackColor.CGColor));
    XCTAssertEqual(strokeLayer.lineWidth, 4);
    XCTAssertGreaterThan([view.layer.sublayers indexOfObject:strokeLayer],
                         [view.layer.sublayers indexOfObject:gradientLayer]);
}

- (void)testRadialFillGradientUsesRadialGradientLayer
{
    SCValdiShapeView *view = [[SCValdiShapeView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    NSArray *gradient = @[@[@0xFF0000FF, @0x0000FFFF], @[], @0, @YES];

    XCTAssertTrue([view valdi_setFillGradient:gradient animator:nil]);

    CAGradientLayer *gradientLayer = [self gradientLayerForView:view];
    XCTAssertEqualObjects(gradientLayer.type, kCAGradientLayerRadial);
    XCTAssertTrue(CGPointEqualToPoint(gradientLayer.startPoint, CGPointMake(0.5, 0.5)));
    XCTAssertTrue(CGPointEqualToPoint(gradientLayer.endPoint, CGPointMake(1, 1)));
}

- (void)testFillGradientTakesPrecedenceAndRestoresUpdatedFillColor
{
    SCValdiShapeView *view = [[SCValdiShapeView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    [view _setFillColor:UIColor.blueColor animator:nil];
    [view _setStrokeColor:UIColor.redColor animator:nil];
    [view valdi_setFillGradient:[self linearGradient] animator:nil];

    [view _setFillColor:UIColor.yellowColor animator:nil];
    [view _setStrokeColor:UIColor.purpleColor animator:nil];

    XCTAssertEqual(view.shapeLayer.fillColor, NULL);
    XCTAssertEqual(view.shapeLayer.strokeColor, NULL);
    XCTAssertNotNil([self gradientLayerForView:view]);

    [view _resetFillGradientWithAnimator:nil];

    XCTAssertNil([self gradientLayerForView:view]);
    XCTAssertNil([self strokeLayerForView:view]);
    XCTAssertTrue(CGColorEqualToColor(view.shapeLayer.fillColor, UIColor.yellowColor.CGColor));
    XCTAssertTrue(CGColorEqualToColor(view.shapeLayer.strokeColor, UIColor.purpleColor.CGColor));
}

- (void)testSingleColorFillGradientFallsBackToSolidFill
{
    SCValdiShapeView *view = [[SCValdiShapeView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    [view valdi_setFillGradient:[self linearGradient] animator:nil];

    NSArray *singleColorGradient = @[@[@0x0000FFFF], @[], @0, @NO];
    XCTAssertTrue([view valdi_setFillGradient:singleColorGradient animator:nil]);

    XCTAssertNil([self gradientLayerForView:view]);
    XCTAssertTrue(CGColorEqualToColor(view.shapeLayer.fillColor, UIColor.blueColor.CGColor));
}

- (void)testFillGradientUpdatesWithPathStrokeAndBounds
{
    SCValdiShapeView *view = [[SCValdiShapeView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    [view valdi_setFillGradient:[self linearGradient] animator:nil];
    [view valdi_setPath:[self rectangularPathData] animator:nil];
    [view _setStrokeColor:UIColor.purpleColor animator:nil];
    [view _setLineWidth:6 animator:nil];
    [view valdi_setStrokeStart:0.25 animator:nil];
    [view valdi_setStrokeEnd:0.75 animator:nil];

    CAGradientLayer *gradientLayer = [self gradientLayerForView:view];
    CAShapeLayer *maskLayer = (CAShapeLayer *)gradientLayer.mask;
    CAShapeLayer *strokeLayer = [self strokeLayerForView:view];

    XCTAssertTrue(CGPathEqualToPath(maskLayer.path, view.shapeLayer.path));
    XCTAssertTrue(CGPathEqualToPath(strokeLayer.path, view.shapeLayer.path));
    XCTAssertTrue(CGColorEqualToColor(strokeLayer.strokeColor, UIColor.purpleColor.CGColor));
    XCTAssertEqual(strokeLayer.lineWidth, 6);
    XCTAssertEqualWithAccuracy(strokeLayer.strokeStart, 0.25, 0.001);
    XCTAssertEqualWithAccuracy(strokeLayer.strokeEnd, 0.75, 0.001);

    view.frame = CGRectMake(0, 0, 160, 120);
    [view setNeedsLayout];
    [view layoutIfNeeded];

    XCTAssertTrue(CGRectEqualToRect(gradientLayer.frame, view.bounds));
    XCTAssertTrue(CGRectEqualToRect(maskLayer.frame, view.bounds));
    XCTAssertTrue(CGRectEqualToRect(strokeLayer.frame, view.bounds));
}

@end
