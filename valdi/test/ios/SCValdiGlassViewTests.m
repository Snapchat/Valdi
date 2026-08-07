//
//  SCValdiGlassViewTests.m
//  ios_tests
//
//  Unit tests for SCValdiGlassView, the iOS native backing for `<glass>`.
//

#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiGlassView.h"
// Declares +bindAttributes:, which SCValdiGlassView overrides to register its attributes.
#import "valdi/ios/Categories/UIView+Valdi.h"
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

// Runs +bindAttributes: against a mock binder and returns the string apply/reset blocks
// registered for `attributeName`, so an attribute's behavior can be exercised without a
// live Valdi runtime.
static void _SCValdiTestCaptureStringAttribute(NSString *attributeName,
                                               SCValdiAttributeBindMethodString __strong *applyBlock,
                                               SCValdiAttributeBindMethodReset __strong *resetBlock)
{
    id binder = OCMProtocolMock(@protocol(SCValdiAttributesBinderProtocol));
    OCMStub([binder bindAttribute:attributeName
                 invalidateLayoutOnChange:NO
                          withStringBlock:OCMOCK_ANY
                               resetBlock:OCMOCK_ANY])
        .andDo(^(NSInvocation *invocation) {
            __unsafe_unretained SCValdiAttributeBindMethodString apply = nil;
            __unsafe_unretained SCValdiAttributeBindMethodReset reset = nil;
            [invocation getArgument:&apply atIndex:4];
            [invocation getArgument:&reset atIndex:5];
            *applyBlock = apply;
            *resetBlock = reset;
        });

    [SCValdiGlassView bindAttributes:binder];
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

// `glassAppearance` is opt-in: with the attribute unset the material must follow the app's
// appearance (unspecified override), so glass consumers that do not set it are unaffected.
// The AI Remix input bar is the only caller that pins an appearance.
- (void)testDoesNotPinAppearanceByDefault
{
    XCTAssertEqual(self.glassView.overrideUserInterfaceStyle, UIUserInterfaceStyleUnspecified);
}

// `glassAppearance` pins the material to one variant; an unrecognized value must fall back to
// following the app rather than silently picking one. Reset restores that fallback because these
// views are pooled (see willEnqueueIntoValdiPool) and would otherwise leak a pinned appearance.
- (void)testGlassAppearanceMapsToOverrideUserInterfaceStyle
{
    SCValdiAttributeBindMethodString applyAppearance = nil;
    SCValdiAttributeBindMethodReset resetAppearance = nil;
    _SCValdiTestCaptureStringAttribute(@"glassAppearance", &applyAppearance, &resetAppearance);
    XCTAssertNotNil(applyAppearance);
    XCTAssertNotNil(resetAppearance);

    XCTAssertTrue(applyAppearance(self.glassView, @"dark", nil));
    XCTAssertEqual(self.glassView.overrideUserInterfaceStyle, UIUserInterfaceStyleDark);

    XCTAssertTrue(applyAppearance(self.glassView, @"light", nil));
    XCTAssertEqual(self.glassView.overrideUserInterfaceStyle, UIUserInterfaceStyleLight);

    XCTAssertTrue(applyAppearance(self.glassView, @"sepia", nil));
    XCTAssertEqual(self.glassView.overrideUserInterfaceStyle, UIUserInterfaceStyleUnspecified);

    XCTAssertTrue(applyAppearance(self.glassView, @"dark", nil));
    resetAppearance(self.glassView, nil);
    XCTAssertEqual(self.glassView.overrideUserInterfaceStyle, UIUserInterfaceStyleUnspecified);
}

@end
