#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Views/SCValdiLabel.h"
#import "valdi/ios/Views/SCValdiTextLayoutView.h"

@interface SCValdiLabel (SelectionTests)

- (void)valdi_setText:(id)textValue;
- (void)valdi_setFontAttributes:(SCValdiFontAttributes *)fontAttributes;
- (BOOL)valdi_setSelection:(NSArray *)selection;
- (void)updateLabelMode:(SCValdiTextMode)labelMode usesEffectsLayoutManager:(BOOL)usesEffectsLayoutManager;

@end

@interface SCValdiLabelSelectionTests : XCTestCase
@end

@implementation SCValdiLabelSelectionTests

- (SCValdiTextLayoutView *)textLayoutViewForLabel:(SCValdiLabel *)label
{
    [label sizeThatFits:label.bounds.size];
    [label layoutIfNeeded];
    for (UIView *subview in label.subviews) {
        if ([subview isKindOfClass:SCValdiTextLayoutView.class]) {
            return (SCValdiTextLayoutView *)subview;
        }
    }
    return nil;
}

- (void)testSelectableIsLazy
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    NSUInteger initialInteractionCount = label.interactions.count;
    Ivar textLayoutViewIvar = class_getInstanceVariable(SCValdiLabel.class, "_textLayoutView");
    XCTAssertNotEqual(textLayoutViewIvar, NULL);
    XCTAssertNil(object_getIvar(label, textLayoutViewIvar));

    [label valdi_setText:@"Hello selectable label"];
    XCTAssertEqual(label.interactions.count, initialInteractionCount);
    XCTAssertNil(object_getIvar(label, textLayoutViewIvar));

    [label valdi_setSelectable:YES];
    XCTAssertNil(object_getIvar(label, textLayoutViewIvar));

    SCValdiTextLayoutView *textLayoutView = [self textLayoutViewForLabel:label];
    XCTAssertNotNil(textLayoutView);
    XCTAssertNotNil(object_getIvar(label, textLayoutViewIvar));
    NSUInteger textLayoutInitialInteractionCount = textLayoutView.interactions.count;
    XCTAssertEqual(textLayoutInitialInteractionCount, initialInteractionCount);
    XCTAssertTrue([textLayoutView pointInside:CGPointMake(10, 10) withEvent:nil]);
    XCTAssertGreaterThan(textLayoutView.interactions.count, textLayoutInitialInteractionCount);

    [label valdi_setSelectable:NO];
    [label sizeThatFits:label.bounds.size];
    XCTAssertEqual(label.interactions.count, initialInteractionCount);
    XCTAssertNil(object_getIvar(label, textLayoutViewIvar));
}

- (void)testTextLayoutModeSwitchToEffectsPreservesTextLayoutView
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    Ivar textLayoutViewIvar = class_getInstanceVariable(SCValdiLabel.class, "_textLayoutView");

    [label updateLabelMode:SCValdiTextModeValdiTextLayout usesEffectsLayoutManager:NO];
    SCValdiTextLayoutView *initialTextLayoutView = object_getIvar(label, textLayoutViewIvar);
    XCTAssertNotNil(initialTextLayoutView);
    XCTAssertFalse(initialTextLayoutView.usesEffectsLayoutManager);

    [label updateLabelMode:SCValdiTextModeValdiTextLayout usesEffectsLayoutManager:YES];
    SCValdiTextLayoutView *updatedTextLayoutView = object_getIvar(label, textLayoutViewIvar);
    XCTAssertEqual(updatedTextLayoutView, initialTextLayoutView);
    XCTAssertTrue(updatedTextLayoutView.usesEffectsLayoutManager);
    XCTAssertEqual(updatedTextLayoutView.superview, label);
}

- (void)testSelectionStateIsAppliedWhenTextLayoutViewIsCreated
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    Ivar textLayoutViewIvar = class_getInstanceVariable(SCValdiLabel.class, "_textLayoutView");

    [label valdi_setText:@"Hello selectable label"];
    BOOL didSetSelection = [label valdi_setSelection:@[@0, @5]];
    XCTAssertTrue(didSetSelection);
    XCTAssertNil(object_getIvar(label, textLayoutViewIvar));

    [label valdi_setSelectable:YES];
    XCTAssertNil(object_getIvar(label, textLayoutViewIvar));

    id<UITextInput> textInput = (id<UITextInput>)[self textLayoutViewForLabel:label];
    XCTAssertNotNil(textInput);
    XCTAssertEqualObjects([textInput textInRange:textInput.selectedTextRange], @"Hello");
}

- (void)testSelectableLabelProvidesTextInputSelection
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    [label valdi_setText:@"Hello selectable label"];
    [label valdi_setSelectable:YES];

    SCValdiTextLayoutView *textLayoutView = [self textLayoutViewForLabel:label];
    id<UITextInput> textInput = (id<UITextInput>)textLayoutView;
    XCTAssertNotNil(textInput);
    UITextPosition *start = [textInput positionFromPosition:textInput.beginningOfDocument offset:6];
    UITextPosition *end = [textInput positionFromPosition:start offset:10];
    textInput.selectedTextRange = [textInput textRangeFromPosition:start toPosition:end];

    XCTAssertEqualObjects([textInput textInRange:textInput.selectedTextRange], @"selectable");
    XCTAssertFalse(CGRectIsEmpty([textInput firstRectForRange:textInput.selectedTextRange]));
    XCTAssertGreaterThan([textInput selectionRectsForRange:textInput.selectedTextRange].count, 0U);

    XCTAssertTrue([textLayoutView canPerformAction:@selector(copy:) withSender:nil]);
    [textLayoutView copy:nil];
}

- (void)testSelectableLabelProvidesFirstWordSelectionGeometry
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 120, 180)];
    [label valdi_setText:@"Hello selectable label wraps across multiple lines for caret testing"];
    [label valdi_setSelectable:YES];

    id<UITextInput> textInput = (id<UITextInput>)[self textLayoutViewForLabel:label];
    XCTAssertNotNil(textInput);
    UITextPosition *start = textInput.beginningOfDocument;
    UITextPosition *end = [textInput positionFromPosition:start offset:5];
    textInput.selectedTextRange = [textInput textRangeFromPosition:start toPosition:end];

    XCTAssertEqualObjects([textInput textInRange:textInput.selectedTextRange], @"Hello");

    CGRect firstWordRect = [textInput firstRectForRange:textInput.selectedTextRange];
    XCTAssertFalse(CGRectIsEmpty(firstWordRect));
    XCTAssertGreaterThan([textInput selectionRectsForRange:textInput.selectedTextRange].count, 0U);

    CGRect startCaretRect = [textInput caretRectForPosition:start];
    XCTAssertFalse(CGRectIsEmpty(startCaretRect));
    XCTAssertLessThanOrEqual(CGRectGetHeight(startCaretRect), CGRectGetHeight(firstWordRect) * 1.5);
}

- (void)testSelectableLabelTokenizerEnclosesFirstWordAtBeginningOfDocument
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    [label valdi_setText:@"Compare SCValdiLabel selection"];
    [label valdi_setSelectable:YES];

    id<UITextInput> textInput = (id<UITextInput>)[self textLayoutViewForLabel:label];
    XCTAssertNotNil(textInput);
    id<UITextInputTokenizer> tokenizer = textInput.tokenizer;
    UITextRange *firstWordRange = [textInput.tokenizer rangeEnclosingPosition:textInput.beginningOfDocument
                                                              withGranularity:UITextGranularityWord
                                                                  inDirection:(UITextDirection)UITextStorageDirectionForward];

    XCTAssertNotNil(firstWordRange);
    XCTAssertEqualObjects([textInput textInRange:firstWordRange], @"Compare");
    XCTAssertTrue([tokenizer isPosition:textInput.beginningOfDocument
                         withinTextUnit:UITextGranularityWord
                            inDirection:(UITextDirection)UITextStorageDirectionForward]);

    UITextPosition *firstWordEnd = [tokenizer positionFromPosition:textInput.beginningOfDocument
                                                        toBoundary:UITextGranularityWord
                                                       inDirection:(UITextDirection)UITextStorageDirectionForward];
    XCTAssertEqual([textInput offsetFromPosition:textInput.beginningOfDocument toPosition:firstWordEnd], 7);
    XCTAssertEqual([textInput characterOffsetOfPosition:firstWordEnd withinRange:firstWordRange], 7);
    XCTAssertEqualObjects([textInput textInRange:[textInput textRangeFromPosition:textInput.beginningOfDocument toPosition:firstWordEnd]], @"Compare");
}

- (void)testCollapsedSelectionExpandsOnlyWhenThereIsNoExistingSelection
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    [label valdi_setText:@"Compare SCValdiLabel selection"];
    [label valdi_setSelectable:YES];

    id<UITextInput> textInput = (id<UITextInput>)[self textLayoutViewForLabel:label];
    XCTAssertNotNil(textInput);

    UITextPosition *documentStart = textInput.beginningOfDocument;
    textInput.selectedTextRange = [textInput textRangeFromPosition:documentStart toPosition:documentStart];
    XCTAssertEqualObjects([textInput textInRange:textInput.selectedTextRange], @"Compare");

    UITextPosition *afterCompare = [textInput positionFromPosition:documentStart offset:7];
    UITextPosition *afterSpace = [textInput positionFromPosition:documentStart offset:8];
    textInput.selectedTextRange = [textInput textRangeFromPosition:afterCompare toPosition:afterSpace];
    XCTAssertEqualObjects([textInput textInRange:textInput.selectedTextRange], @" ");

    textInput.selectedTextRange = [textInput textRangeFromPosition:afterSpace toPosition:afterSpace];
    XCTAssertEqual([textInput offsetFromPosition:textInput.selectedTextRange.start toPosition:textInput.selectedTextRange.end], 0);
}

- (void)testMultiLineSelectionRectsAreReportedPerLine
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 120, 180)];
    [label valdi_setFontAttributes:[NSAttributedString fontAttributesWithFont:nil
                                                                        color:nil
                                                                   textAlign:nil
                                                                  lineHeight:nil
                                                        lineHeightAbsolute:nil
                                                              textDecoration:nil
                                                               letterSpacing:nil
                                                                numberOfLines:@0
                                                                 textOverflow:nil]];
    [label valdi_setText:@"Compare SCValdiLabel and SCValdiTextView selection. Long press and drag to select part of this paragraph."];
    [label valdi_setSelectable:YES];

    id<UITextInput> textInput = (id<UITextInput>)[self textLayoutViewForLabel:label];
    XCTAssertNotNil(textInput);

    UITextPosition *start = textInput.beginningOfDocument;
    UITextPosition *end = [textInput positionFromPosition:start offset:74];
    UITextRange *range = [textInput textRangeFromPosition:start toPosition:end];
    NSArray<UITextSelectionRect *> *selectionRects = [textInput selectionRectsForRange:range];

    XCTAssertGreaterThanOrEqual(selectionRects.count, 3U);
    XCTAssertEqualWithAccuracy(CGRectGetMinX(selectionRects.firstObject.rect), 0.0, 1.0);
    XCTAssertEqualWithAccuracy(CGRectGetWidth(selectionRects.firstObject.rect), CGRectGetWidth(label.bounds), 1.0);
    XCTAssertEqualWithAccuracy(CGRectGetMinX(selectionRects[1].rect), 0.0, 1.0);
    XCTAssertEqualWithAccuracy(CGRectGetWidth(selectionRects[1].rect), CGRectGetWidth(label.bounds), 1.0);
    XCTAssertLessThan(CGRectGetWidth(selectionRects.lastObject.rect), CGRectGetWidth(label.bounds));
    CGRect caretRect = [textInput caretRectForPosition:start];
    for (UITextSelectionRect *selectionRect in selectionRects) {
        XCTAssertLessThanOrEqual(CGRectGetHeight(selectionRect.rect), CGRectGetHeight(caretRect) * 1.25);
    }
}

- (void)testSelectableLabelAcceptsSelectionHandleHitTestingOutsideBounds
{
    SCValdiLabel *label = [[SCValdiLabel alloc] initWithFrame:CGRectMake(0, 0, 240, 80)];
    [label valdi_setText:@"Compare SCValdiLabel selection"];
    [label valdi_setSelectable:YES];

    SCValdiTextLayoutView *textLayoutView = [self textLayoutViewForLabel:label];
    id<UITextInput> textInput = (id<UITextInput>)textLayoutView;
    XCTAssertNotNil(textInput);

    UITextPosition *start = textInput.beginningOfDocument;
    UITextPosition *end = [textInput positionFromPosition:start offset:7];
    textInput.selectedTextRange = [textInput textRangeFromPosition:start toPosition:end];

    CGPoint outsideTextLayoutPoint = CGPointMake(-4, -4);
    XCTAssertFalse([label pointInside:outsideTextLayoutPoint withEvent:nil]);
    XCTAssertTrue([textLayoutView pointInsideActiveSelectionHandleBounds:outsideTextLayoutPoint]);
    XCTAssertFalse([textLayoutView pointInsideActiveSelectionHandleBounds:CGPointMake(-80, -80)]);
}

@end
