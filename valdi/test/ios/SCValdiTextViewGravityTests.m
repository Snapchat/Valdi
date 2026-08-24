#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Views/SCValdiTextView.h"

static NSUInteger const kLayoutPassCount = 5;

@interface SCValdiTextView (GravityTesting)
- (void)valdi_setFontAttributes:(SCValdiFontAttributes *)fontAttributes;
- (void)valdi_setValue:(id)textValue;
- (BOOL)valdi_setEnabled:(BOOL)enabled;
@end

@interface SCValdiTextViewGravityTests : XCTestCase
@end

@implementation SCValdiTextViewGravityTests

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

- (UITextView *)displayTextViewIn:(SCValdiTextView *)view
{
    for (UIView *subview in view.subviews) {
        if ([subview isKindOfClass:UITextView.class] && !subview.hidden) {
            return (UITextView *)subview;
        }
    }
    return nil;
}

- (SCValdiFontAttributes *)centeredAttributesWithFont:(UIFont *)font numberOfLines:(NSInteger)numberOfLines
{
    NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary new];
    attributes[NSFontAttributeName] = font;
    return [[SCValdiFontAttributes alloc] initWithAttributes:attributes
                                                       font:nil
                                                      color:[UIColor whiteColor]
                                               textAligment:NSTextAlignmentCenter
                                              numberOfLines:numberOfLines
                                              lineBreakMode:NSLineBreakByWordWrapping
                                       needAttributedString:NO];
}

- (SCValdiFontAttributes *)centeredAttributesWithFont:(UIFont *)font
{
    return [self centeredAttributesWithFont:font numberOfLines:0];
}

/// TextKit height `attributed` needs at `width` with no height limit - what the container has to
/// keep available for every line to lay out.
- (CGFloat)naturalHeightOf:(NSAttributedString *)attributed atWidth:(CGFloat)width
{
    NSTextStorage *storage = [[NSTextStorage alloc] initWithAttributedString:attributed];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [storage addLayoutManager:layoutManager];
    NSTextContainer *container = [[NSTextContainer alloc] initWithSize:CGSizeMake(width, CGFLOAT_MAX)];
    container.lineFragmentPadding = 0;
    [layoutManager addTextContainer:container];
    [layoutManager ensureLayoutForTextContainer:container];
    return CGRectGetMaxY([layoutManager usedRectForTextContainer:container]);
}

/// Number of glyphs TextKit actually laid out in `textView`'s container.
- (NSUInteger)laidOutGlyphCountIn:(UITextView *)textView
{
    NSLayoutManager *layoutManager = textView.layoutManager;
    [layoutManager ensureLayoutForTextContainer:textView.textContainer];
    return NSMaxRange([layoutManager glyphRangeForTextContainer:textView.textContainer]);
}

- (void)runLayoutPassesOn:(SCValdiTextView *)view
{
    for (NSUInteger i = 0; i < kLayoutPassCount; i++) {
        [view setNeedsLayout];
        [view layoutIfNeeded];
    }
}

// The center-gravity correction must not compound. The correction is written to
// textContainerInset.top, which feeds back into contentSize; recomputing from the polluted
// contentSize settled an empty text view at a third of the free space instead of centered.
- (void)testEmptyTextViewCentersAtSteadyState
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);

    [self runLayoutPassesOn:view];

    UIEdgeInsets inset = textView.textContainerInset;
    CGFloat rawContentHeight = textView.contentSize.height - inset.top - inset.bottom;
    XCTAssertGreaterThan(rawContentHeight, 0.0, @"an empty text view still has a caret line of content");
    XCTAssertLessThan(rawContentHeight, 100.0, @"one caret line must not fill the whole view");

    CGFloat expectedTop = (100.0 - rawContentHeight) / 2.0;
    XCTAssertEqualWithAccuracy(inset.top, expectedTop, 1.0);
}

- (void)testEmptyTextViewInsetIsStableAcrossLayoutPasses
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);

    [self runLayoutPassesOn:view];
    CGFloat settledTop = textView.textContainerInset.top;

    [self runLayoutPassesOn:view];
    XCTAssertEqualWithAccuracy(textView.textContainerInset.top, settledTop, 0.5);
}

- (void)testEmptyTextViewShorterThanContentClampsCorrectionToZero
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 4)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);

    [self runLayoutPassesOn:view];

    XCTAssertEqualWithAccuracy(textView.textContainerInset.top, 0.0, 0.5);
}

// A stale contentSize read (inset already written, contentSize not yet refreshed) must not push
// the correction past the bounds; the stripped content height clamps at zero.
- (void)testStaleOversizedInsetRecoversToCenter
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    UITextView *textView = [self editableTextViewIn:view];
    XCTAssertNotNil(textView);

    [self runLayoutPassesOn:view];
    textView.textContainerInset = UIEdgeInsetsMake(500.0, 0, 0, 0);
    [self runLayoutPassesOn:view];

    UIEdgeInsets inset = textView.textContainerInset;
    XCTAssertLessThanOrEqual(inset.top, 100.0);

    CGFloat rawContentHeight = textView.contentSize.height - inset.top - inset.bottom;
    CGFloat expectedTop = (100.0 - rawContentHeight) / 2.0;
    XCTAssertEqualWithAccuracy(inset.top, expectedTop, 1.0);
}

// A non-editable text view does not scroll, so its container is only as tall as its bounds minus
// textContainerInset - the inset the center-gravity correction is written to. Measuring the content
// through that shrunken container reports the truncated height, which grows the correction, which
// truncates further, and TextKit drops a line fragment whole rather than clipping it.
- (void)testPinchResizeKeepsEveryLineLaidOut
{
    NSString *text = @"The The Big Lie and it's lies have nothing going on with it so Iiii";
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 361, 40)];
    [view valdi_setFontAttributes:[self centeredAttributesWithFont:[UIFont fontWithName:@"Helvetica" size:15.4]]];
    [view valdi_setValue:text];
    [view valdi_setEnabled:NO];
    [self runLayoutPassesOn:view];

    UITextView *textView = [self displayTextViewIn:view];
    XCTAssertNotNil(textView);

    // Size the view to its content the way the caption does: measure, then fit the frame to it. A
    // pinch commits the scaled font a pass before the scaled frame, so the text is briefly one line
    // in the old, taller box - the read that used to lock the correction in.
    [view valdi_setFontAttributes:[self centeredAttributesWithFont:[UIFont fontWithName:@"Helvetica" size:9.24]]];
    [self runLayoutPassesOn:view];
    const CGFloat pinchedWidth = 222.0;
    CGFloat naturalHeight = [self naturalHeightOf:textView.attributedText atWidth:pinchedWidth];
    XCTAssertGreaterThan(naturalHeight, 0.0);
    view.frame = CGRectMake(0, 0, pinchedWidth, ceil(naturalHeight));
    [self runLayoutPassesOn:view];

    XCTAssertEqual([self laidOutGlyphCountIn:textView],
                   textView.layoutManager.numberOfGlyphs,
                   @"every glyph must stay laid out in a view sized to fit its text; the gravity "
                   @"correction must not eat the container height its own measurement depends on");

    // The text that fits is still centered: the truncation fallback must not fire here.
    CGRect usedRect = [textView.layoutManager usedRectForTextContainer:textView.textContainer];
    XCTAssertEqualWithAccuracy(textView.textContainerInset.top,
                               (CGRectGetHeight(textView.bounds) - CGRectGetHeight(usedRect)) / 2.0,
                               1.0);
}

// Once the container is too short for the rest of the text there is nothing left to center, so the
// correction must fall back to zero and hand the container its full height back.
- (void)testHeightTruncatedLayoutRecoversAllLines
{
    NSString *text = @"one two three four five six seven eight nine ten eleven twelve";
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 40)];
    [view valdi_setFontAttributes:[self centeredAttributesWithFont:[UIFont fontWithName:@"Helvetica" size:14]]];
    [view valdi_setValue:text];
    [view valdi_setEnabled:NO];
    [self runLayoutPassesOn:view];

    UITextView *textView = [self displayTextViewIn:view];
    XCTAssertNotNil(textView);
    NSUInteger settledGlyphCount = [self laidOutGlyphCountIn:textView];

    // Seed the pathological state directly: a correction wide enough to starve the container.
    textView.textContainerInset = UIEdgeInsetsMake(24.0, 0, 0, 0);
    [self runLayoutPassesOn:view];

    XCTAssertEqual([self laidOutGlyphCountIn:textView],
                   settledGlyphCount,
                   @"a stale oversized correction must not permanently drop laid-out lines");
}


// maximumNumberOfLines truncates by design, so the lines it does lay out still want centering: only
// a container too short for the text gives centering up.
- (void)testLineLimitedTruncationStillCenters
{
    NSString *text = @"one two three four five six seven eight nine ten eleven twelve";
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 200, 80)];
    [view valdi_setFontAttributes:[self centeredAttributesWithFont:[UIFont fontWithName:@"Helvetica" size:14]
                                                     numberOfLines:1]];
    [view valdi_setValue:text];
    [view valdi_setEnabled:NO];
    [self runLayoutPassesOn:view];

    UITextView *textView = [self displayTextViewIn:view];
    XCTAssertNotNil(textView);
    XCTAssertLessThan([self laidOutGlyphCountIn:textView],
                      textView.layoutManager.numberOfGlyphs,
                      @"a one-line limit must truncate this text");

    CGRect usedRect = [textView.layoutManager usedRectForTextContainer:textView.textContainer];
    XCTAssertEqualWithAccuracy(textView.textContainerInset.top, (80.0 - CGRectGetHeight(usedRect)) / 2.0, 1.5);
}

@end
