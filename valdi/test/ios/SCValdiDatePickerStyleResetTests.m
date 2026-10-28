//
//  SCValdiDatePickerStyleResetTests.m
//  ios_tests
//
//  Unit tests for the `preferredStyle` reset path shared by SCValdiDatePicker and
//  SCValdiTimePicker, and the +[SCValdiConfiguration deferCompactPickerStyleResetEnabled]
//  switch that defers the compact-to-wheels rebuild to the next measure or layout pass.
//

#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiDatePicker.h"
#import "valdi/ios/Views/SCValdiDatePickerUtils.h"
#import "valdi/ios/Views/SCValdiTimePicker.h"
// Declares +bindAttributes:, which both pickers override to register `preferredStyle`.
#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi_core/SCValdiAttributesBinderBase.h"
#import "valdi_core/SCValdiConfiguration.h"

static const NSInteger kWheels = 1;
static const NSInteger kCompact = 2;

// Records the int apply/reset blocks bound for `preferredStyle` and swallows every other
// binder call, so the real attribute code can run without a Valdi runtime or OCMock
// (which is not available when this target is built from the monorepo).
@interface SCValdiPreferredStyleRecordingBinder : NSObject <SCValdiAttributesBinderProtocol>
@property (nonatomic, copy) SCValdiAttributeBindMethodInt applyBlock;
@property (nonatomic, copy) SCValdiAttributeBindMethodReset resetBlock;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wprotocol"
@implementation SCValdiPreferredStyleRecordingBinder

- (void)bindAttribute:(NSString *)attributeName
    invalidateLayoutOnChange:(BOOL)invalidateLayoutOnChange
                withIntBlock:(SCValdiAttributeBindMethodInt)intBlock
                  resetBlock:(SCValdiAttributeBindMethodReset)resetBlock
{
    if ([attributeName isEqualToString:@"preferredStyle"]) {
        self.applyBlock = intBlock;
        self.resetBlock = resetBlock;
    }
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
    return [super methodSignatureForSelector:selector]
        ?: [NSMethodSignature signatureWithObjCTypes:"v@:"];
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
}

@end
#pragma clang diagnostic pop

static void _SCValdiTestCapturePreferredStyle(Class pickerClass,
                                              SCValdiAttributeBindMethodInt __strong *applyBlock,
                                              SCValdiAttributeBindMethodReset __strong *resetBlock)
{
    SCValdiPreferredStyleRecordingBinder *binder = [SCValdiPreferredStyleRecordingBinder new];
    [pickerClass bindAttributes:binder];
    *applyBlock = binder.applyBlock;
    *resetBlock = binder.resetBlock;
}

@interface SCValdiDatePickerStyleResetTests : XCTestCase
@end

@implementation SCValdiDatePickerStyleResetTests

- (void)tearDown
{
    [SCValdiConfiguration setDeferCompactPickerStyleResetEnabled:NO];
    [super tearDown];
}

- (void)testDeferSwitchDefaultsToOff
{
    XCTAssertFalse([SCValdiConfiguration deferCompactPickerStyleResetEnabled]);
}

#pragma mark - SCValdiDatePickerResetStyle / SCValdiDatePickerApplyDeferredStyle

- (void)testResetRestoresWheelsImmediatelyWhenSwitchIsOff
{
    UIDatePicker *picker = [[UIDatePicker alloc] initWithFrame:CGRectZero];
    SCValdiSetDatePickerStyle(picker, kCompact);
    BOOL needsWheels = NO;

    SCValdiDatePickerResetStyle(picker, &needsWheels);

    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleWheels);
    XCTAssertFalse(needsWheels);
}

- (void)testResetDefersWheelsForCompactPickerWhenSwitchIsOn
{
    [SCValdiConfiguration setDeferCompactPickerStyleResetEnabled:YES];
    UIDatePicker *picker = [[UIDatePicker alloc] initWithFrame:CGRectZero];
    SCValdiSetDatePickerStyle(picker, kCompact);
    BOOL needsWheels = NO;

    SCValdiDatePickerResetStyle(picker, &needsWheels);
    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleCompact);
    XCTAssertTrue(needsWheels);

    SCValdiDatePickerApplyDeferredStyle(picker, &needsWheels);
    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleWheels);
    XCTAssertFalse(needsWheels);
}

// The switch only touches compact pickers; a wheels picker keeps the immediate reset.
- (void)testResetDoesNotDeferForWheelsPickerWhenSwitchIsOn
{
    [SCValdiConfiguration setDeferCompactPickerStyleResetEnabled:YES];
    UIDatePicker *picker = [[UIDatePicker alloc] initWithFrame:CGRectZero];
    SCValdiSetDatePickerStyle(picker, kWheels);
    BOOL needsWheels = NO;

    SCValdiDatePickerResetStyle(picker, &needsWheels);

    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleWheels);
    XCTAssertFalse(needsWheels);
}

- (void)testApplyDeferredStyleIsNoOpWithoutPendingReset
{
    UIDatePicker *picker = [[UIDatePicker alloc] initWithFrame:CGRectZero];
    SCValdiSetDatePickerStyle(picker, kCompact);
    BOOL needsWheels = NO;

    SCValdiDatePickerApplyDeferredStyle(picker, &needsWheels);

    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleCompact);
}

#pragma mark - Bound `preferredStyle` attribute on the picker views

- (void)_assertPickerClass:(Class)pickerClass appliesDeferredResetOnMeasure:(BOOL)useSizeThatFits
{
    SCValdiAttributeBindMethodInt apply = nil;
    SCValdiAttributeBindMethodReset reset = nil;
    _SCValdiTestCapturePreferredStyle(pickerClass, &apply, &reset);
    XCTAssertNotNil(apply);
    XCTAssertNotNil(reset);

    [SCValdiConfiguration setDeferCompactPickerStyleResetEnabled:YES];
    UIDatePicker *picker = [[pickerClass alloc] initWithFrame:CGRectMake(0, 0, 320, 44)];
    // Pickers init as wheels; the attribute switches them to compact.
    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleWheels);
    apply(picker, kCompact, nil);
    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleCompact);

    reset(picker, nil);
    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleCompact, @"reset must not rebuild synchronously");

    if (useSizeThatFits) {
        [picker sizeThatFits:CGSizeMake(320, CGFLOAT_MAX)];
    } else {
        [picker layoutIfNeeded];
    }
    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleWheels);
}

- (void)testDatePickerAppliesDeferredResetOnMeasure
{
    [self _assertPickerClass:[SCValdiDatePicker class] appliesDeferredResetOnMeasure:YES];
}

- (void)testDatePickerAppliesDeferredResetOnLayout
{
    [self _assertPickerClass:[SCValdiDatePicker class] appliesDeferredResetOnMeasure:NO];
}

- (void)testTimePickerAppliesDeferredResetOnMeasure
{
    [self _assertPickerClass:[SCValdiTimePicker class] appliesDeferredResetOnMeasure:YES];
}

- (void)testTimePickerAppliesDeferredResetOnLayout
{
    [self _assertPickerClass:[SCValdiTimePicker class] appliesDeferredResetOnMeasure:NO];
}

// A pooled picker that is reset and then re-applied as compact before its next layout must
// stay compact: the apply clears the pending wheels reset.
- (void)testApplyAfterDeferredResetCancelsPendingWheels
{
    SCValdiAttributeBindMethodInt apply = nil;
    SCValdiAttributeBindMethodReset reset = nil;
    _SCValdiTestCapturePreferredStyle([SCValdiTimePicker class], &apply, &reset);

    [SCValdiConfiguration setDeferCompactPickerStyleResetEnabled:YES];
    UIDatePicker *picker = [[SCValdiTimePicker alloc] initWithFrame:CGRectMake(0, 0, 320, 44)];
    apply(picker, kCompact, nil);
    reset(picker, nil);
    apply(picker, kCompact, nil);

    [picker layoutIfNeeded];
    [picker sizeThatFits:CGSizeMake(320, CGFLOAT_MAX)];

    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleCompact);
}

- (void)testBoundResetRestoresWheelsImmediatelyWhenSwitchIsOff
{
    SCValdiAttributeBindMethodInt apply = nil;
    SCValdiAttributeBindMethodReset reset = nil;
    _SCValdiTestCapturePreferredStyle([SCValdiDatePicker class], &apply, &reset);

    UIDatePicker *picker = [[SCValdiDatePicker alloc] initWithFrame:CGRectMake(0, 0, 320, 44)];
    apply(picker, kCompact, nil);
    reset(picker, nil);

    XCTAssertEqual(picker.preferredDatePickerStyle, UIDatePickerStyleWheels);
}

@end
