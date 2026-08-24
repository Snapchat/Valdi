#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiTextView.h"

@interface SCValdiTextView (InvalidationTesting)
- (BOOL)_updateAttributedTextIfNeeded;
- (void)valdi_setValue:(id)textValue;
- (void)valdi_setFontAttributes:(id)fontAttributes;
- (BOOL)valdi_setCharacterLimit:(NSNumber *)characterLimit;
- (BOOL)valdi_setEnabled:(BOOL)enabled;
@end

@interface SCValdiRebuildCountingTextView : SCValdiTextView
@property (nonatomic) NSUInteger rebuildCallCount;
@end

@implementation SCValdiRebuildCountingTextView

- (BOOL)_updateAttributedTextIfNeeded
{
    // Count rebuilds actually performed, not calls: layout passes invoke this
    // unconditionally and no-op when the dirty flag is clear.
    if ([[self valueForKey:@"needAttributedTextUpdate"] boolValue]) {
        self.rebuildCallCount += 1;
    }
    return [super _updateAttributedTextIfNeeded];
}

@end

@interface SCValdiTextViewInvalidationTests : XCTestCase
@end

@implementation SCValdiTextViewInvalidationTests

- (SCValdiRebuildCountingTextView *)settledView
{
    SCValdiRebuildCountingTextView *view =
        [[SCValdiRebuildCountingTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    [view valdi_setValue:@"hello"];
    [view setNeedsLayout];
    [view layoutIfNeeded];
    view.rebuildCallCount = 0;
    return view;
}

- (UITextView *)editableTextViewIn:(SCValdiTextView *)view
{
    for (UIView *subview in view.subviews) {
        if ([subview isKindOfClass:UITextView.class]) {
            UITextView *textView = (UITextView *)subview;
            if (textView.editable && textView.userInteractionEnabled) {
                return textView;
            }
        }
    }
    return nil;
}

- (void)testAttributeSettersCoalesceIntoSingleLayoutRebuild
{
    SCValdiRebuildCountingTextView *view = [self settledView];

    [view valdi_setFontAttributes:nil];
    [view valdi_setCharacterLimit:@100];
    [view valdi_setEnabled:NO];
    XCTAssertEqual(view.rebuildCallCount, 0,
                   @"attribute setters must defer the rebuild instead of running it inline");

    [view layoutIfNeeded];
    XCTAssertEqual(view.rebuildCallCount, 1,
                   @"three deferred invalidations must coalesce into one rebuild at layout");
}

- (void)testDeferredCharacterLimitAppliesOnNextLayout
{
    SCValdiRebuildCountingTextView *view = [self settledView];

    [view valdi_setCharacterLimit:@3];
    [view layoutIfNeeded];

    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertEqualObjects(textView.text, @"hel",
                          @"a deferred character limit must still apply on the next layout pass");
}

- (void)testIdenticalPlainStringRebindSkipsRebuild
{
    SCValdiRebuildCountingTextView *view = [self settledView];

    [view valdi_setValue:@"hello"];
    XCTAssertEqual(view.rebuildCallCount, 0, @"rebinding an identical plain string must be a no-op");

    [view valdi_setValue:@"world"];
    XCTAssertEqual(view.rebuildCallCount, 1, @"a changed value must rebuild synchronously");
    XCTAssertEqualObjects([self editableTextViewIn:view].text, @"world");
}

- (void)testIdenticalValueRebindStillResyncsDivergedTextView
{
    SCValdiRebuildCountingTextView *view = [self settledView];
    UITextView *textView = [self editableTextViewIn:view];

    // JS rejects an edit by re-setting the previous value while the UITextView already
    // shows the typed characters; the rebind must not be skipped in that state.
    textView.text = @"hellox";
    [view valdi_setValue:@"hello"];

    XCTAssertEqual(view.rebuildCallCount, 1);
    XCTAssertEqualObjects(textView.text, @"hello",
                          @"re-setting the previous value must clobber rejected characters");
}

@end
