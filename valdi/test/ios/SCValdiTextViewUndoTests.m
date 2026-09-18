#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Views/SCValdiTextView.h"

@interface SCValdiTextView (UndoTesting)
- (void)valdi_setValue:(id)textValue;
- (void)valdi_setFontAttributes:(SCValdiFontAttributes *)fontAttributes;
- (BOOL)valdi_setCharacterLimit:(NSNumber *)characterLimit;
@end

@interface SCValdiTextViewUndoTests : XCTestCase
@property (nonatomic, strong) UIWindow *window;
@property (nonatomic, strong) SCValdiTextView *view;
@property (nonatomic, strong) UITextView *textView;
@end

@implementation SCValdiTextViewUndoTests

- (void)setUp
{
    [super setUp];
    self.window = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    self.window.rootViewController = [UIViewController new];
    self.view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    [self.window.rootViewController.view addSubview:self.view];
    [self.window makeKeyAndVisible];
    [self.view valdi_setValue:@"hello"];
    [self.view layoutIfNeeded];
    for (UIView *subview in self.view.subviews) {
        if ([subview isKindOfClass:UITextView.class]) {
            UITextView *textView = (UITextView *)subview;
            if (textView.editable && textView.userInteractionEnabled) {
                self.textView = textView;
                break;
            }
        }
    }
    XCTAssertNotNil(self.textView);
    self.textView.autocorrectionType = UITextAutocorrectionTypeNo;
    self.textView.spellCheckingType = UITextSpellCheckingTypeNo;
    XCTAssertTrue([self.textView becomeFirstResponder]);
    XCTAssertNotNil(self.textView.undoManager);
    [self.textView.undoManager removeAllActions];
    self.textView.undoManager.groupsByEvent = NO;
}

- (void)tearDown
{
    [self closeUndoGroups];
    [self.textView.undoManager removeAllActions];
    [self.textView resignFirstResponder];
    self.window.hidden = YES;
    self.textView = nil;
    self.view = nil;
    self.window = nil;
    [super tearDown];
}

- (void)closeUndoGroups
{
    NSUndoManager *undoManager = self.textView.undoManager;
    while (undoManager.groupingLevel > 0) {
        [undoManager endUndoGrouping];
    }
}

- (void)insertTextInOpenUndoGroup
{
    self.textView.selectedRange = NSMakeRange(self.textView.text.length, 0);
    NSUndoManager *undoManager = self.textView.undoManager;
    XCTAssertEqual(undoManager.groupingLevel, 0);
    [undoManager beginUndoGrouping];
    [self.textView insertText:@" world"];
    XCTAssertGreaterThan(undoManager.groupingLevel, 0);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
    XCTAssertTrue(undoManager.canUndo, @"UIKit insertion must create a real undo entry");
    [self.view valdi_setValue:self.textView.text];
    XCTAssertTrue(undoManager.canUndo, @"echoing the native edit must preserve its open undo group");
}

- (void)assertUndoHistoryClearedWithText:(NSString *)text
{
    [self closeUndoGroups];
    XCTAssertFalse(self.textView.undoManager.canUndo);
    XCTAssertFalse(self.textView.undoManager.canRedo);
    XCTAssertNoThrow([self.textView.undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, text);
}

- (NSAttributedString *)styledText:(NSString *)text
{
    return [[NSAttributedString alloc] initWithString:text
                                         attributes:@{NSForegroundColorAttributeName : UIColor.redColor}];
}

- (void)testShorterPlainReplacementClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    self.textView.text = @"hi";
    [self assertUndoHistoryClearedWithText:@"hi"];
}

- (void)testEqualLengthPlainReplacementClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    self.textView.text = @"other words";
    [self assertUndoHistoryClearedWithText:@"other words"];
}

- (void)testShorterAttributedReplacementClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    self.textView.attributedText = [self styledText:@"hi"];
    [self assertUndoHistoryClearedWithText:@"hi"];
}

- (void)testEqualLengthAttributedReplacementClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    self.textView.attributedText = [self styledText:@"other words"];
    [self assertUndoHistoryClearedWithText:@"other words"];
}

- (void)testNilPlainReplacementClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    self.textView.text = nil;
    [self assertUndoHistoryClearedWithText:@""];
}

- (void)testNilAttributedReplacementClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    self.textView.attributedText = nil;
    [self assertUndoHistoryClearedWithText:@""];
}

- (void)testUnchangedPlainTextPreservesTypingUndoAndRedo
{
    [self insertTextInOpenUndoGroup];
    NSRange selection = self.textView.selectedRange;
    self.textView.text = [self.textView.text copy];
    XCTAssertTrue(NSEqualRanges(self.textView.selectedRange, selection));
    [self closeUndoGroups];
    XCTAssertTrue(self.textView.undoManager.canUndo);
    XCTAssertNoThrow([self.textView.undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"hello");
    XCTAssertTrue(self.textView.undoManager.canRedo);
    XCTAssertNoThrow([self.textView.undoManager redo]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
}

- (void)testStyleOnlyReplacementPreservesTypingUndoAndRedo
{
    [self insertTextInOpenUndoGroup];
    self.textView.attributedText = [self styledText:self.textView.text];
    [self closeUndoGroups];
    XCTAssertTrue(self.textView.undoManager.canUndo);
    XCTAssertNoThrow([self.textView.undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"hello");
    XCTAssertTrue(self.textView.undoManager.canRedo);
    XCTAssertNoThrow([self.textView.undoManager redo]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
    self.textView.selectedRange = NSMakeRange(self.textView.text.length, 0);
    [self.textView.undoManager beginUndoGrouping];
    [self.textView insertText:@"!"];
    XCTAssertEqualObjects(self.textView.text, @"hello world!");
    [self closeUndoGroups];
    XCTAssertTrue(self.textView.undoManager.canUndo);
    XCTAssertNoThrow([self.textView.undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
}

- (void)testPlainReconciliationClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    [self.view valdi_setValue:@"hi"];
    [self assertUndoHistoryClearedWithText:@"hi"];
}

- (void)enableAttributedTextWithColor:(UIColor *)color
{
    SCValdiFontAttributes *attributes =
        [[SCValdiFontAttributes alloc] initWithAttributes:@{NSForegroundColorAttributeName : color}
                                                    font:nil
                                                   color:color
                                            textAligment:NSTextAlignmentLeft
                                           numberOfLines:0
                                           lineBreakMode:NSLineBreakByWordWrapping
                                    needAttributedString:YES];
    [self.view valdi_setFontAttributes:attributes];
    [self.view layoutIfNeeded];
}

- (void)testAttributedReconciliationClearsTypingUndo
{
    [self enableAttributedTextWithColor:UIColor.redColor];
    [self insertTextInOpenUndoGroup];
    XCTAssertTrue(self.textView.undoManager.canUndo);
    [self.view valdi_setValue:@"hi"];
    [self assertUndoHistoryClearedWithText:@"hi"];
}

- (void)testStyleReconciliationPreservesSelectionAndTypingUndo
{
    [self enableAttributedTextWithColor:UIColor.redColor];
    [self insertTextInOpenUndoGroup];
    NSRange selection = NSMakeRange(2, 0);
    self.textView.selectedRange = selection;
    [self enableAttributedTextWithColor:UIColor.blueColor];
    XCTAssertTrue(NSEqualRanges(self.textView.selectedRange, selection));
    [self closeUndoGroups];
    XCTAssertTrue(self.textView.undoManager.canUndo);
    XCTAssertNoThrow([self.textView.undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"hello");
    XCTAssertTrue(self.textView.undoManager.canRedo);
    XCTAssertNoThrow([self.textView.undoManager redo]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
}

- (void)testDeferredCharacterLimitClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    XCTAssertTrue([self.view valdi_setCharacterLimit:@3]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
    XCTAssertTrue(self.textView.undoManager.canUndo);
    [self.view layoutIfNeeded];
    [self assertUndoHistoryClearedWithText:@"hel"];
}

- (void)testDeferredAttributedCharacterLimitClearsTypingUndo
{
    [self enableAttributedTextWithColor:UIColor.redColor];
    [self insertTextInOpenUndoGroup];
    XCTAssertTrue([self.view valdi_setCharacterLimit:@3]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
    XCTAssertTrue(self.textView.undoManager.canUndo);
    [self.view layoutIfNeeded];
    [self assertUndoHistoryClearedWithText:@"hel"];
}

- (void)testSwitchingToAttributedModeClearsTypingUndo
{
    [self insertTextInOpenUndoGroup];
    [self enableAttributedTextWithColor:UIColor.redColor];
    [self assertUndoHistoryClearedWithText:@"hello world"];
}

- (void)replaceText:(id)text inTextView:(UITextView *)textView attributed:(BOOL)attributed
{
    id previousText = attributed ? (id)textView.attributedText : textView.text;
    [textView.undoManager registerUndoWithTarget:textView handler:^(UITextView *target) {
        XCTAssertTrue(target.undoManager.isUndoing || target.undoManager.isRedoing);
        [self replaceText:previousText inTextView:target attributed:attributed];
    }];
    if (attributed) {
        textView.attributedText = text;
    } else {
        textView.text = text;
    }
}

- (void)assertReplayPreservedWithAttributedText:(BOOL)attributed
{
    NSUndoManager *undoManager = self.textView.undoManager;
    id replacement = attributed ? (id)[self styledText:@"replacement"] : @"replacement";
    [undoManager beginUndoGrouping];
    [undoManager registerUndoWithTarget:self.textView handler:^(UITextView *target) {
        [self replaceText:replacement inTextView:target attributed:attributed];
    }];
    [undoManager endUndoGrouping];
    XCTAssertTrue(undoManager.canUndo);
    XCTAssertNoThrow([undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"replacement");
    XCTAssertTrue(undoManager.canRedo);
    XCTAssertNoThrow([undoManager redo]);
    XCTAssertEqualObjects(self.textView.text, @"hello");
    XCTAssertTrue(undoManager.canUndo);
    XCTAssertNoThrow([undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"replacement");
}

- (void)testPlainSetterDuringUndoAndRedoPreservesInverseActions
{
    [self assertReplayPreservedWithAttributedText:NO];
}

- (void)testAttributedSetterDuringUndoAndRedoPreservesInverseActions
{
    [self assertReplayPreservedWithAttributedText:YES];
}

- (void)assertOtherTextViewHistoryPreservedAfterAttributedReplacement:(BOOL)attributed
{
    [self insertTextInOpenUndoGroup];
    [self closeUndoGroups];
    SCValdiTextView *otherView = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 120, 300, 100)];
    [self.window.rootViewController.view addSubview:otherView];
    [otherView valdi_setValue:@"second"];
    [otherView layoutIfNeeded];
    UITextView *otherTextView = nil;
    for (UIView *subview in otherView.subviews) {
        if ([subview isKindOfClass:UITextView.class]) {
            UITextView *candidate = (UITextView *)subview;
            if (candidate.editable && candidate.userInteractionEnabled) {
                otherTextView = candidate;
                break;
            }
        }
    }
    XCTAssertNotNil(otherTextView);
    XCTAssertTrue([otherTextView becomeFirstResponder]);
    NSUndoManager *firstManager = self.textView.undoManager;
    NSUndoManager *otherManager = otherTextView.undoManager;
    NSUndoManager *windowManager = self.window.undoManager;
    NSLog(@"Undo manager identities: first=%@ %p second=%@ %p window=%@ %p firstEqualsSecond=%d firstEqualsWindow=%d secondEqualsWindow=%d",
          NSStringFromClass(firstManager.class), (__bridge void *)firstManager,
          NSStringFromClass(otherManager.class), (__bridge void *)otherManager,
          NSStringFromClass(windowManager.class), (__bridge void *)windowManager,
          firstManager == otherManager, firstManager == windowManager, otherManager == windowManager);
    XCTAssertNotNil(firstManager);
    XCTAssertNotNil(otherManager);
    XCTAssertNotEqual(firstManager, otherManager);
    XCTAssertNotEqual(firstManager, windowManager);
    XCTAssertNotEqual(otherManager, windowManager);
    NSObject *unrelatedTarget = [NSObject new];
    __block BOOL unrelatedActionExecuted = NO;
    @try {
        otherManager.groupsByEvent = NO;
        otherTextView.selectedRange = NSMakeRange(otherTextView.text.length, 0);
        [otherManager beginUndoGrouping];
        [otherTextView insertText:@" edit"];
        [otherManager endUndoGrouping];
        XCTAssertEqualObjects(otherTextView.text, @"second edit");
        XCTAssertTrue(otherManager.canUndo);
        XCTAssertTrue(firstManager.canUndo);
        if (windowManager != nil) {
            windowManager.groupsByEvent = NO;
            [windowManager beginUndoGrouping];
            [windowManager registerUndoWithTarget:unrelatedTarget handler:^(NSObject *target) {
                unrelatedActionExecuted = YES;
            }];
            [windowManager endUndoGrouping];
            XCTAssertTrue(windowManager.canUndo);
        }
        if (attributed) {
            self.textView.attributedText = [self styledText:@"replacement"];
        } else {
            self.textView.text = @"replacement";
        }
        XCTAssertFalse(firstManager.canUndo);
        XCTAssertTrue(otherManager.canUndo);
        XCTAssertNoThrow([otherManager undo]);
        XCTAssertEqualObjects(otherTextView.text, @"second");
        XCTAssertTrue(otherManager.canRedo);
        XCTAssertNoThrow([otherManager redo]);
        XCTAssertEqualObjects(otherTextView.text, @"second edit");
        XCTAssertEqualObjects(self.textView.text, @"replacement");
        if (windowManager != nil) {
            XCTAssertTrue(windowManager.canUndo);
            XCTAssertNoThrow([windowManager undo]);
            XCTAssertTrue(unrelatedActionExecuted);
        }
    } @finally {
        while (otherManager.groupingLevel > 0) {
            [otherManager endUndoGrouping];
        }
        [otherManager removeAllActions];
        [windowManager removeAllActionsWithTarget:unrelatedTarget];
        [otherTextView resignFirstResponder];
        [otherView removeFromSuperview];
    }
}

- (void)testPlainReplacementPreservesOtherTextViewAndWindowUndo
{
    [self assertOtherTextViewHistoryPreservedAfterAttributedReplacement:NO];
}

- (void)testAttributedReplacementPreservesOtherTextViewAndWindowUndo
{
    [self assertOtherTextViewHistoryPreservedAfterAttributedReplacement:YES];
}

- (void)testRemovingTextViewTargetLeavesNativeTypingUndo
{
    [self insertTextInOpenUndoGroup];
    NSUndoManager *undoManager = self.textView.undoManager;
    [undoManager removeAllActionsWithTarget:self.textView];
    [self closeUndoGroups];
    NSLog(@"Targeted undo removal: target=%@ manager=%@ canUndo=%d",
          NSStringFromClass(self.textView.class), NSStringFromClass(undoManager.class), undoManager.canUndo);
    XCTAssertTrue(undoManager.canUndo);
    XCTAssertNoThrow([undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"hello");
    XCTAssertTrue(undoManager.canRedo);
    XCTAssertNoThrow([undoManager redo]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
}

- (void)testRemovingTextViewAndStorageTargetsLeavesEmptyUndoGroup
{
    [self insertTextInOpenUndoGroup];
    NSUndoManager *undoManager = self.textView.undoManager;
    [undoManager removeAllActionsWithTarget:self.textView];
    [undoManager removeAllActionsWithTarget:self.textView.textStorage];
    [self closeUndoGroups];
    NSLog(@"Targeted undo removal: target=%@ manager=%@ canUndo=%d",
          NSStringFromClass(self.textView.textStorage.class), NSStringFromClass(undoManager.class), undoManager.canUndo);
    XCTAssertTrue(undoManager.canUndo);
    XCTAssertNoThrow([undoManager undo]);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
    XCTAssertFalse(undoManager.canRedo);
    XCTAssertEqualObjects(self.textView.text, @"hello world");
}

- (void)testStockTextKitTextViewRetainsStaleTypingUndoAfterReplacement
{
    NSTextStorage *storage = [NSTextStorage new];
    NSLayoutManager *layoutManager = [NSLayoutManager new];
    [storage addLayoutManager:layoutManager];
    NSTextContainer *container = [NSTextContainer new];
    [layoutManager addTextContainer:container];
    UITextView *stockView = [[UITextView alloc] initWithFrame:CGRectMake(0, 120, 300, 100)
                                             textContainer:container];
    [self.window.rootViewController.view addSubview:stockView];
    stockView.text = @"hello";
    XCTAssertTrue([stockView becomeFirstResponder]);
    NSUndoManager *undoManager = stockView.undoManager;
    XCTAssertNotNil(undoManager);
    @try {
        undoManager.groupsByEvent = NO;
        stockView.selectedRange = NSMakeRange(stockView.text.length, 0);
        [undoManager beginUndoGrouping];
        [stockView insertText:@" world"];
        XCTAssertEqualObjects(stockView.text, @"hello world");
        XCTAssertTrue(undoManager.canUndo);
        stockView.attributedText = [self styledText:@"hi"];
        while (undoManager.groupingLevel > 0) {
            [undoManager endUndoGrouping];
        }
        NSLog(@"Stock replacement control: view=%@ manager=%@ text=%@ canUndo=%d",
              NSStringFromClass(stockView.class), NSStringFromClass(undoManager.class),
              stockView.text, undoManager.canUndo);
        XCTAssertEqualObjects(stockView.text, @"hi");
        XCTAssertTrue(undoManager.canUndo);
    } @finally {
        while (undoManager.groupingLevel > 0) {
            [undoManager endUndoGrouping];
        }
        [undoManager removeAllActions];
        [stockView resignFirstResponder];
        [stockView removeFromSuperview];
    }
}

@end
