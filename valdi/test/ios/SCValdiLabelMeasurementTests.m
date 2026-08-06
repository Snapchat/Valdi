#import <XCTest/XCTest.h>

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"

// Regression coverage for SEARCH-48847 / SEARCH-48849: after the PR #107 text-rendering reland,
// Search/Explore card captions render with their bottom halves clipped on iOS.
//
// Root cause is a units change in how a relative `lineHeight` (a multiple, as coreui text styles
// supply it) is applied. Before the reland, NSAttributedString+Valdi applied it as
// paragraphStyle.lineHeightMultiple, which UIKit multiplies against the font's *natural* line height,
// so a lineHeight of 1.0 produced exactly the natural line box. The reland moved the work into
// +[SCValdiFontAttributes applyLineHeightInAttributes:font:], which resolves the multiple as
// `font.pointSize * lineHeight` and hard-clamps the box with minimum/maximumLineHeight. Because a
// font's natural line height is larger than its point size (~1.2x), a lineHeight of 1.0 now clamps the
// line box *below* the height the glyphs occupy, so descenders and wrapped lines are drawn outside the
// measured node bounds and clipped.
//
// The invariant these tests pin: applying a lineHeight multiple of 1.0 must not shrink a label below
// the height it needs with no lineHeight at all. A 1.0 multiple means "natural line height"; anything
// smaller clips glyphs that still render at their natural size.
@interface SCValdiLabelMeasurementTests : XCTestCase
@end

@implementation SCValdiLabelMeasurementTests

static UITraitCollection *SCTestTraitCollection(void)
{
    return [UITraitCollection traitCollectionWithDisplayScale:2.0];
}

static SCValdiFontAttributes *SCTestFontAttributes(UIFont *font, NSNumber *lineHeightMultiple)
{
    NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary new];
    attributes[NSFontAttributeName] = font;
    if (lineHeightMultiple != nil) {
        attributes[SCValdiLineHeightAttributeName] = lineHeightMultiple;
    }
    return [[SCValdiFontAttributes alloc] initWithAttributes:attributes
                                                       font:nil
                                                      color:[UIColor blackColor]
                                               textAligment:NSTextAlignmentLeft
                                              numberOfLines:0
                                              lineBreakMode:NSLineBreakByWordWrapping
                                       needAttributedString:NO];
}

static CGSize SCTestMeasure(SCValdiFontAttributes *fontAttributes, NSString *text, CGFloat maxWidth)
{
    return [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(maxWidth, CGFLOAT_MAX)
                                     fontAttributes:fontAttributes
                                        fontManager:nil
                                               text:text
                                    traitCollection:SCTestTraitCollection()];
}

- (void)testLineHeightOneDoesNotClampSingleLineBelowNaturalHeight
{
    UIFont *font = [UIFont systemFontOfSize:13];
    NSString *text = @"Follow a Snap Star";
    CGFloat maxWidth = 240.0;

    CGSize natural = SCTestMeasure(SCTestFontAttributes(font, nil), text, maxWidth);
    CGSize withLineHeight = SCTestMeasure(SCTestFontAttributes(font, @(1.0)), text, maxWidth);

    XCTAssertGreaterThanOrEqual(withLineHeight.height + 0.5,
                                natural.height,
                                @"lineHeight 1.0 measured %.2f, shorter than the natural %.2f; the glyphs "
                                @"render at natural size and the caption is clipped.",
                                withLineHeight.height,
                                natural.height);
}

- (void)testLineHeightOneDoesNotClampWrappingCaptionBelowNaturalHeight
{
    UIFont *font = [UIFont systemFontOfSize:13];
    NSString *text = @"Discover trending lenses and shows near you today";
    CGFloat maxWidth = 96.0;

    CGSize natural = SCTestMeasure(SCTestFontAttributes(font, nil), text, maxWidth);
    CGSize withLineHeight = SCTestMeasure(SCTestFontAttributes(font, @(1.0)), text, maxWidth);

    XCTAssertGreaterThanOrEqual(withLineHeight.height + 0.5,
                                natural.height,
                                @"lineHeight 1.0 measured %.2f across a wrapping caption, shorter than the "
                                @"natural %.2f; wrapped lines are clipped.",
                                withLineHeight.height,
                                natural.height);
}

@end
