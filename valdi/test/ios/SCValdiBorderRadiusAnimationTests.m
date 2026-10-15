#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi_core/SCValdiAttributesBinderBase.h"

@interface UIView (BorderRadiusAnimationTesting)
- (BOOL)valdi_setBorderWidth:(CGFloat)attributeValue animator:(id<SCValdiAnimatorProtocol>)animator;
- (BOOL)valdi_setBorderColor:(UIColor *)borderColor animator:(id<SCValdiAnimatorProtocol>)animator;
- (BOOL)valdi_setBorderStyle:(NSString *)style animator:(id<SCValdiAnimatorProtocol>)animator;
- (BOOL)valdi_setBorderRadius:(SCValdiCornerValues)attributeValue animator:(id<SCValdiAnimatorProtocol>)animator;
@end

@interface SCValdiRecordingAnimator : NSObject <SCValdiAnimatorProtocol>
@property (nonatomic, readonly) NSMutableArray<NSString *> *keyPaths;
@property (nonatomic, readonly) NSMutableArray<CALayer *> *layers;
@end

@implementation SCValdiRecordingAnimator

- (instancetype)init
{
    self = [super init];
    if (self) {
        _keyPaths = [NSMutableArray array];
        _layers = [NSMutableArray array];
    }
    return self;
}

- (instancetype)initWithCurve:(UIViewAnimationCurve)curve
                controlPoints:(NSArray<NSNumber *> *)controlPoints
                     duration:(NSTimeInterval)duration
        beginFromCurrentState:(BOOL)beginFromCurrentState
                    crossfade:(BOOL)crossfade
                    stiffness:(CGFloat)stiffness
                      damping:(CGFloat)damping
{
    return [self init];
}

- (BOOL)crossfade
{
    return NO;
}

- (void)addAnimationOnLayer:(CALayer *)layer forKeyPath:(NSString *)keyPath value:(id)value
{
    [_keyPaths addObject:keyPath];
    [_layers addObject:layer];
    [layer setValue:value forKeyPath:keyPath];
}

- (void)addTransitionOnLayer:(CALayer *)layer
{
}

- (void)addCompletion:(dispatch_block_t)completion
{
    completion();
}

@end

static SCValdiCornerValues SCUniformCorners(CGFloat radius)
{
    SCValdiDoubleValue value = {.value = radius, .isPercent = NO};
    return (SCValdiCornerValues){.topLeft = value, .topRight = value, .bottomRight = value, .bottomLeft = value};
}

@interface SCValdiBorderRadiusAnimationTests : XCTestCase
@property (nonatomic, strong) UIView *view;
@property (nonatomic, strong) CAShapeLayer *borderLayer;
@end

@implementation SCValdiBorderRadiusAnimationTests

- (void)setUp
{
    [super setUp];
    self.view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 140, 64)];
    [self.view valdi_setBorderWidth:4 animator:nil];
    [self.view valdi_setBorderColor:[UIColor blackColor] animator:nil];
    [self.view valdi_setBorderStyle:@"dashed" animator:nil];
    for (CALayer *sublayer in self.view.layer.sublayers) {
        if ([sublayer isKindOfClass:CAShapeLayer.class]) {
            self.borderLayer = (CAShapeLayer *)sublayer;
        }
    }
    XCTAssertNotNil(self.borderLayer);
    XCTAssertNotNil(self.borderLayer.lineDashPattern);
}

- (void)testDashedBorderPathAnimatesWithBorderRadius
{
    CGPathRef squarePath = CGPathRetain(self.borderLayer.path);
    SCValdiRecordingAnimator *animator = [SCValdiRecordingAnimator new];

    [self.view valdi_setBorderRadius:SCUniformCorners(16) animator:animator];

    NSUInteger index = [animator.keyPaths indexOfObject:@"path"];
    XCTAssertNotEqual(index, NSNotFound);
    if (index == NSNotFound) {
        CGPathRelease(squarePath);
        return;
    }
    XCTAssertEqual(animator.layers[index], self.borderLayer);
    XCTAssertFalse(CGPathEqualToPath(squarePath, self.borderLayer.path));
    CGPathRelease(squarePath);
}

- (void)testDashedBorderPathUpdatesWithoutAnimator
{
    CGPathRef squarePath = CGPathRetain(self.borderLayer.path);

    [self.view valdi_setBorderRadius:SCUniformCorners(16) animator:nil];

    XCTAssertFalse(CGPathEqualToPath(squarePath, self.borderLayer.path));
    CGPathRelease(squarePath);
}

@end
