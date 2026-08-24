#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/Views/SCValdiTextView.h"

// _animatedTextView (per-glyph animation overlay) and _placeholder are built lazily on first use.
// These are the attribute-binding entry points and lazy-construction seams that drive them; none are
// public, so redeclare what the tests exercise directly.
@interface SCValdiTextView (SCValdiTextViewLazySubviewTests)
- (void)valdi_setValue:(id)textValue;
- (BOOL)valdi_setPlaceholder:(nullable NSString *)placeholder;
- (BOOL)valdi_setTextShadow:(NSArray *)textShadow;
- (UITextView *)_ensureAnimatedTextView;
- (void)_updateAnimatedTextOverlayWithAttributedString:(NSAttributedString *)attributedString
                                             isEnabled:(BOOL)isEnabled;
@end

static NSUInteger const kLayoutPassCount = 5;
static CGFloat const kValidShadowRadius = 3.0;
static CGFloat const kValidShadowOpacity = 0.5;

@interface SCValdiTextViewLazySubviewTests : XCTestCase
@end

@implementation SCValdiTextViewLazySubviewTests

#pragma mark - Helpers

// The three text holders differ by interaction/edit flags: the base view is the only editable and
// interactive one, the placeholder is editable but inert, and the animation overlay is neither.
- (UITextView *)baseTextViewIn:(SCValdiTextView *)view
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

- (UITextView *)placeholderIn:(SCValdiTextView *)view
{
    for (UIView *subview in view.subviews) {
        if ([subview isKindOfClass:UITextView.class]) {
            UITextView *textView = (UITextView *)subview;
            if (textView.editable && !textView.userInteractionEnabled) {
                return textView;
            }
        }
    }
    return nil;
}

- (UITextView *)animationOverlayIn:(SCValdiTextView *)view
{
    for (UIView *subview in view.subviews) {
        if ([subview isKindOfClass:UITextView.class]) {
            UITextView *textView = (UITextView *)subview;
            if (!textView.editable) {
                return textView;
            }
        }
    }
    return nil;
}

- (void)runLayoutPassesOn:(SCValdiTextView *)view
{
    for (NSUInteger i = 0; i < kLayoutPassCount; i++) {
        [view setNeedsLayout];
        [view layoutIfNeeded];
    }
}

- (NSArray *)validTextShadow
{
    // [color, radius, opacity, offsetX, offsetY] — SCTextShadowParameterCount entries.
    return @[ @(0xFF0000FF), @(kValidShadowRadius), @(kValidShadowOpacity), @1.0, @2.0 ];
}

#pragma mark - Z-order

// Eager construction added the subviews in a fixed order (base view, overlay, placeholder), so the
// placeholder always drew above the overlay. With lazy construction the insertion has to anchor to
// whichever sibling exists, or the order flips with attribute application order.
- (void)testPlaceholderIsAboveAnimationOverlayWhenOverlayIsBuiltFirst
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    UITextView *overlay = [view _ensureAnimatedTextView];
    XCTAssertTrue([view valdi_setPlaceholder:@"hint"]);
    UITextView *placeholder = [self placeholderIn:view];

    XCTAssertNotNil(overlay);
    XCTAssertNotNil(placeholder);
    XCTAssertGreaterThan([view.subviews indexOfObject:placeholder],
                         [view.subviews indexOfObject:overlay],
                         @"placeholder must sit above the animation overlay");
}

- (void)testPlaceholderIsAboveAnimationOverlayWhenPlaceholderIsBuiltFirst
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    XCTAssertTrue([view valdi_setPlaceholder:@"hint"]);
    UITextView *placeholder = [self placeholderIn:view];
    UITextView *overlay = [view _ensureAnimatedTextView];

    XCTAssertNotNil(overlay);
    XCTAssertNotNil(placeholder);
    XCTAssertGreaterThan([view.subviews indexOfObject:placeholder],
                         [view.subviews indexOfObject:overlay],
                         @"placeholder must sit above the animation overlay regardless of which "
                         @"subview materializes first");
}

- (void)testBothLazySubviewsAreAboveTheBaseTextView
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    UITextView *base = [self baseTextViewIn:view];
    XCTAssertTrue([view valdi_setPlaceholder:@"hint"]);
    UITextView *overlay = [view _ensureAnimatedTextView];
    UITextView *placeholder = [self placeholderIn:view];

    NSUInteger baseIndex = [view.subviews indexOfObject:base];
    XCTAssertLessThan(baseIndex, [view.subviews indexOfObject:overlay]);
    XCTAssertLessThan(baseIndex, [view.subviews indexOfObject:placeholder]);
}

#pragma mark - Text shadow

// A malformed shadow is rejected outright: no text holder's layer is touched. Stashing it anyway made
// a later-built placeholder replay the rejected value and end up with no shadow at all, while the base
// view kept rendering the last valid one.
- (void)testRejectedTextShadowDoesNotClobberTheStashedValue
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    UITextView *base = [self baseTextViewIn:view];

    XCTAssertTrue([view valdi_setTextShadow:[self validTextShadow]]);
    // Hoisted: an array literal's comma would split the assertion macro's arguments.
    NSArray *malformedShadow = @[ @(0xFF0000FF), @1.0 ];
    XCTAssertFalse([view valdi_setTextShadow:malformedShadow],
                   @"a shadow with the wrong parameter count must fail the attribute");

    XCTAssertEqualWithAccuracy(base.layer.shadowRadius, kValidShadowRadius, 0.001,
                               @"the rejected shadow must leave the base view untouched");

    XCTAssertTrue([view valdi_setPlaceholder:@"hint"]);
    UITextView *placeholder = [self placeholderIn:view];
    XCTAssertNotNil(placeholder);
    XCTAssertEqualWithAccuracy(placeholder.layer.shadowRadius, kValidShadowRadius, 0.001);
    XCTAssertEqualWithAccuracy(placeholder.layer.shadowOpacity, kValidShadowOpacity, 0.001,
                               @"a placeholder built later must replay the shadow the base view renders");
}

- (void)testValidTextShadowIsReplayedOnLazySubviews
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    XCTAssertTrue([view valdi_setTextShadow:[self validTextShadow]]);

    XCTAssertTrue([view valdi_setPlaceholder:@"hint"]);
    UITextView *placeholder = [self placeholderIn:view];
    UITextView *overlay = [view _ensureAnimatedTextView];

    XCTAssertEqualWithAccuracy(placeholder.layer.shadowRadius, kValidShadowRadius, 0.001);
    XCTAssertEqualWithAccuracy(overlay.layer.shadowRadius, kValidShadowRadius, 0.001);
}

#pragma mark - Scroll offset mirroring

// The overlay draws the same glyphs as the base view, so it has to share its scroll offset. It is
// created empty, and UIScrollView clamps contentOffset against contentSize, so an offset written
// before the overlay has content is dropped — leaving animated glyphs misaligned with already
// scrolled text until the next user scroll.
- (void)testAnimationOverlayMirrorsScrollOffsetWhenBuiltOverScrolledText
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 60)];
    NSString *longText = [@"" stringByPaddingToLength:600 withString:@"scrolled caption text " startingAtIndex:0];
    [view valdi_setValue:longText];
    [self runLayoutPassesOn:view];

    UITextView *base = [self baseTextViewIn:view];
    XCTAssertGreaterThan(base.contentSize.height, 120.0, @"the text must overflow the 60pt bounds to scroll");
    base.contentOffset = CGPointMake(0, 40);

    NSAttributedString *attributedString =
        [[NSAttributedString alloc] initWithString:longText
                                       attributes:@{NSFontAttributeName : base.font ?: [UIFont systemFontOfSize:14]}];
    [view _updateAnimatedTextOverlayWithAttributedString:attributedString isEnabled:YES];
    [self runLayoutPassesOn:view];

    UITextView *overlay = [self animationOverlayIn:view];
    XCTAssertNotNil(overlay);
    XCTAssertEqualWithAccuracy(overlay.contentOffset.y, base.contentOffset.y, 0.5,
                               @"the overlay must adopt the base view's scroll offset when it is built");
}

// Enabling an animation runs from attribute setters as well as -layoutSubviews, and it does not have
// to resize the view, so the mirror cannot depend on a layout pass happening to follow. This drives
// the enable on an overlay that already exists (no subview insertion to dirty the layout) and only
// calls -layoutIfNeeded, which is a no-op unless the enable path itself established or scheduled it.
- (void)testAnimationOverlayMirrorsScrollOffsetWithoutAnExplicitLayoutPass
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 60)];
    NSString *longText = [@"" stringByPaddingToLength:600 withString:@"scrolled caption text " startingAtIndex:0];
    [view valdi_setValue:longText];
    [view _ensureAnimatedTextView];
    [self runLayoutPassesOn:view];

    UITextView *base = [self baseTextViewIn:view];
    base.contentOffset = CGPointMake(0, 40);

    NSAttributedString *attributedString =
        [[NSAttributedString alloc] initWithString:longText
                                       attributes:@{NSFontAttributeName : base.font ?: [UIFont systemFontOfSize:14]}];
    [view _updateAnimatedTextOverlayWithAttributedString:attributedString isEnabled:YES];
    [view layoutIfNeeded];

    UITextView *overlay = [self animationOverlayIn:view];
    XCTAssertEqualWithAccuracy(overlay.contentOffset.y, base.contentOffset.y, 0.5,
                               @"enabling the overlay must mirror the offset without the caller "
                               @"having to trigger a layout pass");
}

- (void)testAnimationOverlayFollowsSubsequentScrolls
{
    SCValdiTextView *view = [[SCValdiTextView alloc] initWithFrame:CGRectMake(0, 0, 300, 60)];
    NSString *longText = [@"" stringByPaddingToLength:600 withString:@"scrolled caption text " startingAtIndex:0];
    [view valdi_setValue:longText];
    [self runLayoutPassesOn:view];

    UITextView *base = [self baseTextViewIn:view];
    NSAttributedString *attributedString =
        [[NSAttributedString alloc] initWithString:longText
                                       attributes:@{NSFontAttributeName : base.font ?: [UIFont systemFontOfSize:14]}];
    [view _updateAnimatedTextOverlayWithAttributedString:attributedString isEnabled:YES];
    [self runLayoutPassesOn:view];

    base.contentOffset = CGPointMake(0, 55);

    UITextView *overlay = [self animationOverlayIn:view];
    XCTAssertEqualWithAccuracy(overlay.contentOffset.y, 55.0, 0.5,
                               @"-scrollViewDidScroll: must keep mirroring the base view's offset");
}

@end
