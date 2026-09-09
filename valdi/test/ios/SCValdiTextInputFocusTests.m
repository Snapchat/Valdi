#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiTextField.h"
#import "valdi/ios/Views/SCValdiTextView.h"

// valdi_setFocused: is the attribute-binding entry point; it is intentionally not part of
// the public headers, so redeclare it here for direct exercise.
@interface SCValdiTextField (SCValdiTextInputFocusTests)
- (BOOL)valdi_setFocused:(BOOL)focused;
- (BOOL)valdi_setEnabled:(BOOL)enabled;
@end

@interface SCValdiTextView (SCValdiTextInputFocusTests)
- (BOOL)valdi_setFocused:(BOOL)focused;
- (BOOL)valdi_setEnabled:(BOOL)enabled;
- (void)valdi_setValue:(id)value;
- (BOOL)valdi_setSelectTextOnFocus:(BOOL)selectTextOnFocus;
- (BOOL)valdi_setScrollToEndBeforeFocus:(BOOL)scrollToEndBeforeFocus;
@end

// The 'focused' attribute can be applied before the native view is attached to a UIWindow
// (e.g. during the initial render of a Valdi component that requests focus on create).
// UIKit refuses becomeFirstResponder for a windowless view, and the Valdi attribute system
// never retries a failed application, so without deferral the field stays unfocused forever
// (COMPOSER-6146). These tests pin the deferred-focus contract.
@interface SCValdiTextInputFocusTests : XCTestCase
@end

@implementation SCValdiTextInputFocusTests {
    UIWindow *_window;
}

- (void)setUp
{
    [super setUp];
    _window = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    [_window makeKeyAndVisible];
}

- (void)tearDown
{
    _window.hidden = YES;
    _window = nil;
    [super tearDown];
}

// The become-active retry is deferred by one runloop turn; drain it before asserting.
- (void)drainMainRunLoop
{
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
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

#pragma mark - SCValdiTextField

- (void)testTextFieldFocusBeforeWindowIsDeferredUntilAttach
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];

    XCTAssertTrue([field valdi_setFocused:YES], @"focus request before window attach must not fail the attribute");
    XCTAssertFalse(field.isFirstResponder);

    [_window addSubview:field];

    XCTAssertTrue(field.isFirstResponder, @"deferred focus must be applied when the view enters a window");
    [field removeFromSuperview];
}

- (void)testTextFieldFocusCancelledBeforeAttachIsNotApplied
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];

    XCTAssertTrue([field valdi_setFocused:YES]);
    XCTAssertTrue([field valdi_setFocused:NO]);

    [_window addSubview:field];

    XCTAssertFalse(field.isFirstResponder, @"a cancelled pending focus must not fire on window attach");
    [field removeFromSuperview];
}

- (void)testTextFieldFocusInWindowIsAppliedImmediately
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    [_window addSubview:field];

    XCTAssertTrue([field valdi_setFocused:YES]);
    XCTAssertTrue(field.isFirstResponder);

    XCTAssertTrue([field valdi_setFocused:NO]);
    XCTAssertFalse(field.isFirstResponder);
    [field removeFromSuperview];
}

- (void)testTextFieldDeferredFocusDoesNotReapplyOnWindowRoundTrip
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];

    XCTAssertTrue([field valdi_setFocused:YES]);
    [_window addSubview:field];
    XCTAssertTrue(field.isFirstResponder);

    XCTAssertTrue([field valdi_setFocused:NO]);
    [field removeFromSuperview];
    [_window addSubview:field];

    XCTAssertFalse(field.isFirstResponder, @"a consumed pending focus must not re-fire on a later attach");
    [field removeFromSuperview];
}

// UIKit can also refuse an in-window becomeFirstResponder (resign-active churn from notification
// banners, a non-key window — the condition in the COMPOSER-6146 report, where the editor stayed
// dead with the view attached). A disabled control reproduces that refusal deterministically.
- (void)testTextFieldRefusedInWindowFocusIsRetriedOnBecomeActive
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    [_window addSubview:field];
    [field valdi_setEnabled:NO];

    XCTAssertTrue([field valdi_setFocused:YES], @"an in-window refusal must not fail the attribute");
    XCTAssertFalse(field.isFirstResponder);

    [field valdi_setEnabled:YES];
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];
    [self drainMainRunLoop];

    XCTAssertTrue(field.isFirstResponder, @"pending focus must be retried when the app becomes active");
    [field removeFromSuperview];
}

// While another window is key (system alert, notification banner), applying the pending focus
// wouldn't bring up the keyboard — it must wait until the field's own window is key again.
- (void)testTextFieldPendingFocusWaitsForOwnWindowToBecomeKey
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    [_window addSubview:field];
    [field valdi_setEnabled:NO];
    XCTAssertTrue([field valdi_setFocused:YES]);
    [field valdi_setEnabled:YES];

    UIWindow *alertWindow = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    [alertWindow makeKeyAndVisible];

    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];
    [self drainMainRunLoop];
    XCTAssertFalse(field.isFirstResponder, @"pending focus must not be consumed while another window is key");

    [_window makeKeyAndVisible];

    XCTAssertTrue(field.isFirstResponder, @"pending focus must be applied when the field's window becomes key again");
    alertWindow.hidden = YES;
    [field removeFromSuperview];
}

// The initial application must also defer under a non-key window: becomeFirstResponder can
// report success there without showing the keyboard, which would consume the intent.
- (void)testTextFieldFocusRequestedWhileAnotherWindowIsKeyIsDeferred
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    [_window addSubview:field];

    UIWindow *alertWindow = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    [alertWindow makeKeyAndVisible];

    XCTAssertTrue([field valdi_setFocused:YES], @"focus while another window is key must not fail the attribute");
    XCTAssertFalse(field.isFirstResponder, @"focus must not be applied under a non-key window");

    [_window makeKeyAndVisible];

    XCTAssertTrue(field.isFirstResponder, @"deferred focus must be applied when the field's window becomes key");
    alertWindow.hidden = YES;
    [field removeFromSuperview];
}

- (void)testTextFieldRefusedFocusCancelledBeforeBecomeActiveIsNotApplied
{
    SCValdiTextField *field = [[SCValdiTextField alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    [_window addSubview:field];
    [field valdi_setEnabled:NO];

    XCTAssertTrue([field valdi_setFocused:YES]);
    XCTAssertTrue([field valdi_setFocused:NO]);

    [field valdi_setEnabled:YES];
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];
    [self drainMainRunLoop];

    XCTAssertFalse(field.isFirstResponder, @"a cancelled pending focus must not fire on become-active");
    [field removeFromSuperview];
}

#pragma mark - SCValdiTextView

- (void)testTextViewFocusBeforeWindowIsDeferredUntilAttach
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);

    XCTAssertTrue([view valdi_setFocused:YES], @"focus request before window attach must not fail the attribute");
    XCTAssertFalse(textView.isFirstResponder);

    [_window addSubview:view];

    XCTAssertTrue(textView.isFirstResponder, @"deferred focus must be applied when the view enters a window");
    [view removeFromSuperview];
}

- (void)testTextViewFocusCancelledBeforeAttachIsNotApplied
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);

    XCTAssertTrue([view valdi_setFocused:YES]);
    XCTAssertTrue([view valdi_setFocused:NO]);

    [_window addSubview:view];

    XCTAssertFalse(textView.isFirstResponder, @"a cancelled pending focus must not fire on window attach");
    [view removeFromSuperview];
}

- (void)testTextViewFocusInWindowIsAppliedImmediately
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);
    [_window addSubview:view];

    XCTAssertTrue([view valdi_setFocused:YES]);
    XCTAssertTrue(textView.isFirstResponder);

    XCTAssertTrue([view valdi_setFocused:NO]);
    XCTAssertFalse(textView.isFirstResponder);
    [view removeFromSuperview];
}

- (void)testTextViewMovesSelectionToEndBeforeFocusWhenEnabled
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);
    [view valdi_setValue:[@"Prompt " stringByPaddingToLength:1000 withString:@"long text " startingAtIndex:0]];
    [view valdi_setScrollToEndBeforeFocus:YES];
    [_window addSubview:view];
    [view layoutIfNeeded];
    XCTAssertGreaterThan(textView.contentSize.height, textView.bounds.size.height);

    textView.selectedRange = NSMakeRange(0, 0);
    textView.contentOffset = CGPointZero;
    XCTAssertTrue([view valdi_setFocused:YES]);
    XCTAssertTrue(textView.isFirstResponder);
    XCTAssertEqual(textView.selectedRange.location, textView.text.length);
    XCTAssertEqual(textView.selectedRange.length, 0);

    [view removeFromSuperview];
}

- (void)testTextViewDoesNotScrollToEndBeforeFocusByDefault
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);
    [view valdi_setValue:[@"Prompt " stringByPaddingToLength:1000 withString:@"long text " startingAtIndex:0]];
    [view valdi_setSelectTextOnFocus:YES];
    [_window addSubview:view];
    [view layoutIfNeeded];
    XCTAssertGreaterThan(textView.contentSize.height, textView.bounds.size.height);

    textView.selectedRange = NSMakeRange(0, 0);
    textView.contentOffset = CGPointZero;
    XCTAssertTrue([view valdi_setFocused:YES]);
    XCTAssertEqual(textView.selectedRange.location, 0);
    XCTAssertEqual(textView.selectedRange.length, 0);
    XCTAssertEqual(textView.contentOffset.y, 0.0);

    [view removeFromSuperview];
}

- (void)testTextViewFocusRequestedWhileAnotherWindowIsKeyIsDeferred
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);
    [_window addSubview:view];

    UIWindow *alertWindow = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    [alertWindow makeKeyAndVisible];

    XCTAssertTrue([view valdi_setFocused:YES], @"focus while another window is key must not fail the attribute");
    XCTAssertFalse(textView.isFirstResponder, @"focus must not be applied under a non-key window");

    [_window makeKeyAndVisible];

    XCTAssertTrue(textView.isFirstResponder, @"deferred focus must be applied when the view's window becomes key");
    alertWindow.hidden = YES;
    [view removeFromSuperview];
}

- (void)testPendingTextViewFocusDoesNotMoveSelectionAfterManualFocus
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);
    [view valdi_setValue:[@"Prompt " stringByPaddingToLength:1000 withString:@"long text " startingAtIndex:0]];
    [view valdi_setScrollToEndBeforeFocus:YES];
    [_window addSubview:view];
    [view layoutIfNeeded];

    UIWindow *alertWindow = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    [alertWindow makeKeyAndVisible];

    XCTAssertTrue([view valdi_setFocused:YES]);
    XCTAssertTrue([textView becomeFirstResponder]);
    XCTAssertTrue(textView.isFirstResponder);
    textView.selectedRange = NSMakeRange(0, 0);
    textView.contentOffset = CGPointZero;

    [_window makeKeyAndVisible];

    XCTAssertEqual(textView.selectedRange.location, 0);
    XCTAssertEqual(textView.selectedRange.length, 0);
    XCTAssertEqual(textView.contentOffset.y, 0.0);

    alertWindow.hidden = YES;
    [view removeFromSuperview];
}

// No SCValdiTextView analog of the refused-in-window tests: a disabled UITextView still accepts
// becomeFirstResponder (it stays focusable for selection UI), so there is no deterministic way to
// force the refusal from the public surface. The retry plumbing is identical to SCValdiTextField's
// and its pending-focus bookkeeping is covered by the windowless-deferral cases above.

@end
