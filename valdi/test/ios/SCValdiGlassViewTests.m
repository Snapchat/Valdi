//
//  SCValdiGlassViewTests.m
//  ios_tests
//
//  Unit tests for SCValdiGlassView, the iOS native backing for `<glass>`.
//

#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiGlassView.h"
// Declares requiresShapeLayerForBorderRadius / willEnqueueIntoValdiPool, which
// SCValdiGlassView overrides. SCValdiGlassView.h alone does not surface them.
#import "valdi_core/UIView+ValdiBase.h"

// Mirrors the production _SCValdiIsGlassEffectAvailable() guard in SCValdiGlassView.m:
// UIGlassEffect is constructed via the class factory +effectWithStyle: (not
// initWithStyle:), so probe the class method, and only on iOS 26+.
static BOOL _SCValdiTestGlassEffectAvailable(void)
{
    if (@available(iOS 26.0, *)) {
        Class glassEffectClass = NSClassFromString(@"UIGlassEffect");
        return glassEffectClass != nil && [glassEffectClass respondsToSelector:@selector(effectWithStyle:)];
    }
    return NO;
}

@interface SCValdiGlassViewTests : XCTestCase
@property (nonatomic, strong) SCValdiGlassView *glassView;
@end

@implementation SCValdiGlassViewTests

- (void)setUp
{
    [super setUp];
    self.glassView = [[SCValdiGlassView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
}

- (void)tearDown
{
    self.glassView = nil;
    [super tearDown];
}

// Children must be routed into the visual effect view's contentView; adding them
// directly to a UIVisualEffectView is unsupported.
- (void)testRoutesChildrenIntoContentView
{
    XCTAssertEqual([self.glassView contentViewForInsertingValdiChildren], self.glassView.contentView);
}

// On iOS 26 corners are applied natively via cornerConfiguration, so no shape-layer
// mask is needed; on the pre-26 blur fallback the shape-layer mask clips the backdrop.
// The property must therefore mirror !glassAvailable.
- (void)testRequiresShapeLayerForBorderRadius
{
    BOOL glassAvailable = _SCValdiTestGlassEffectAvailable();
    XCTAssertEqual([self.glassView requiresShapeLayerForBorderRadius], !glassAvailable);
}

- (void)testOptsIntoValdiViewPool
{
    XCTAssertTrue([self.glassView willEnqueueIntoValdiPool]);
}

// The view must always have an effect: a real UIGlassEffect on iOS 26+ (when the
// runtime guard passes), otherwise a UIBlurEffect fallback so the surface still
// reads as a translucent panel.
- (void)testHasAnEffectAfterInitialization
{
    XCTAssertNotNil(self.glassView.effect);

    if (_SCValdiTestGlassEffectAvailable()) {
        XCTAssertTrue([self.glassView.effect isKindOfClass:NSClassFromString(@"UIGlassEffect")]);
    } else {
        XCTAssertTrue([self.glassView.effect isKindOfClass:[UIBlurEffect class]]);
    }
}

@end
