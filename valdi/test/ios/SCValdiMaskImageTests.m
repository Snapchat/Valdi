//
//  SCValdiMaskImageTests.m
//  ios_tests
//
//  Regression guard for the `maskImage` attribute on UIView+Valdi.
//
//  The PR #107 text-rendering import deleted the entire iOS image-mask implementation from
//  UIView+Valdi.m — the `maskImage` binding, valdi_applyImageMask:animator:,
//  valdi_resetImageMaskWithAnimator: and SCValdiMaskLayer's maskImageGradientLayer. The Android
//  and C++ sides still declare and process the attribute, so it stayed a silent no-op on iOS:
//  no build error, no crash, masks simply stopped rendering. It shipped unnoticed because
//  nothing covered this path.
//

#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

// Declares +bindAttributes:, which registers `maskImage`.
#import "valdi/ios/Categories/UIView+Valdi.h"

// A two-stop gradient in the raw attribute shape GradientUtils expects:
// @[colors, locations, orientation, radial], colors being packed ARGB values. Two colors is the
// minimum that installs a gradient — valdi_applyImageMask: treats fewer as "clear the mask".
static NSArray *_SCValdiTestTwoStopGradient(void)
{
    return @[ @[ @(0xFF000000), @(0x00000000) ] ];
}

// Runs +[UIView bindAttributes:] against a mock binder and returns the array apply/reset blocks
// registered for `attributeName`, so the binding can be exercised without a live Valdi runtime.
// Mirrors the capture helper in SCValdiGlassViewTests.
static void _SCValdiTestCaptureArrayAttribute(NSString *attributeName,
                                              SCValdiAttributeBindMethodArray __strong *applyBlock,
                                              SCValdiAttributeBindMethodReset __strong *resetBlock)
{
    id binder = OCMProtocolMock(@protocol(SCValdiAttributesBinderProtocol));
    OCMStub([binder bindAttribute:attributeName
                 invalidateLayoutOnChange:NO
                           withArrayBlock:OCMOCK_ANY
                               resetBlock:OCMOCK_ANY])
        .andDo(^(NSInvocation *invocation) {
            __unsafe_unretained SCValdiAttributeBindMethodArray apply = nil;
            __unsafe_unretained SCValdiAttributeBindMethodReset reset = nil;
            [invocation getArgument:&apply atIndex:4];
            [invocation getArgument:&reset atIndex:5];
            *applyBlock = apply;
            *resetBlock = reset;
        });

    [UIView bindAttributes:binder];
}

@interface SCValdiMaskImageTests : XCTestCase
@property (nonatomic, strong) UIView *view;
@property (nonatomic, strong) SCValdiAttributeBindMethodArray applyMaskImage;
@property (nonatomic, strong) SCValdiAttributeBindMethodReset resetMaskImage;
@end

@implementation SCValdiMaskImageTests

- (void)setUp
{
    [super setUp];

    self.view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];

    SCValdiAttributeBindMethodArray apply = nil;
    SCValdiAttributeBindMethodReset reset = nil;
    _SCValdiTestCaptureArrayAttribute(@"maskImage", &apply, &reset);
    self.applyMaskImage = apply;
    self.resetMaskImage = reset;
}

- (void)tearDown
{
    self.view = nil;
    self.applyMaskImage = nil;
    self.resetMaskImage = nil;

    [super tearDown];
}

// Returns the gradient layer SCValdiMaskLayer holds for the image mask. SCValdiMaskLayer is
// private to UIView+Valdi.m, so reach the property by key rather than importing the class.
- (CAGradientLayer *)maskImageGradientLayer
{
    CALayer *maskLayer = self.view.layer.mask;
    if (maskLayer == nil) {
        return nil;
    }
    return [maskLayer valueForKey:@"maskImageGradientLayer"];
}

// Guards every test that invokes the captured blocks. When `maskImage` is not registered the
// blocks are nil, and calling a nil block segfaults — which would abort the whole test binary and
// hide every other suite's result rather than reporting one clean failure.
- (BOOL)requireMaskImageBlocks
{
    if (self.applyMaskImage == nil || self.resetMaskImage == nil) {
        XCTFail(@"`maskImage` is not registered by +[UIView bindAttributes:], so its apply/reset "
                @"blocks could not be captured. The attribute is silently unresolvable on iOS and "
                @"masks never render.");
        return NO;
    }
    return YES;
}

- (void)testMaskImageAttributeIsRegistered
{
    XCTAssertNotNil(self.applyMaskImage,
                    @"+[UIView bindAttributes:] must register an apply block for `maskImage`. "
                    @"Without it the attribute is silently unresolvable on iOS and masks never render.");
    XCTAssertNotNil(self.resetMaskImage,
                    @"+[UIView bindAttributes:] must register a reset block for `maskImage`.");
}

- (void)testApplyingGradientInstallsMaskLayer
{
    if (![self requireMaskImageBlocks]) {
        return;
    }

    XCTAssertNil(self.view.layer.mask, @"a fresh view should carry no mask layer");

    XCTAssertTrue(self.applyMaskImage(self.view, _SCValdiTestTwoStopGradient(), nil));

    XCTAssertNotNil(self.view.layer.mask, @"applying `maskImage` must install a mask layer");
    XCTAssertNotNil([self maskImageGradientLayer],
                    @"applying `maskImage` must install a gradient layer on the mask");
}

- (void)testResettingClearsImageMask
{
    if (![self requireMaskImageBlocks]) {
        return;
    }

    self.applyMaskImage(self.view, _SCValdiTestTwoStopGradient(), nil);
    XCTAssertNotNil([self maskImageGradientLayer], @"precondition: mask gradient is installed");

    self.resetMaskImage(self.view, nil);

    XCTAssertNil([self maskImageGradientLayer], @"resetting `maskImage` must clear the gradient layer");
}

// Fewer than two colors cannot describe a gradient, so production treats it as a clear rather
// than installing a degenerate one-stop layer.
- (void)testFewerThanTwoColorsClearsTheGradient
{
    if (![self requireMaskImageBlocks]) {
        return;
    }

    self.applyMaskImage(self.view, _SCValdiTestTwoStopGradient(), nil);
    XCTAssertNotNil([self maskImageGradientLayer], @"precondition: mask gradient is installed");

    XCTAssertTrue(self.applyMaskImage(self.view, @[ @[ @(0xFF000000) ] ], nil));

    XCTAssertNil([self maskImageGradientLayer],
                 @"a single color must clear the gradient rather than install a one-stop layer");
}

@end
