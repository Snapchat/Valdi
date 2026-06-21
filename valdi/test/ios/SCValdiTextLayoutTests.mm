#import <XCTest/XCTest.h>

#import "valdi/ios/Text/SCValdiTextLayout.h"

@interface SCValdiTextLayoutTests: XCTestCase

@end

@implementation SCValdiTextLayoutTests

- (void)testUnderlineRectsUseFontDescentAndNotStrokeWidth
{
    UIFont *font = [UIFont systemFontOfSize:20];
    NSAttributedString *attributedString =
        [[NSAttributedString alloc] initWithString:@"Hello"
                                        attributes:@{ NSFontAttributeName: font }];
    SCValdiTextLayout *layout = [SCValdiTextLayout new];
    layout.attributedString = attributedString;

    CGRect drawingRect = CGRectMake(0, 0, 200, 100);
    layout.size = drawingRect.size;

    NSRange range = NSMakeRange(0, attributedString.length);
    CGRect textBounds = [layout boundingRectForRange:range];
    CGFloat drawOriginY = drawingRect.origin.y + (drawingRect.size.height - layout.usedRect.size.height) / 2.0;

    CGFloat underlineOffset = 0.5;
    NSArray<NSValue *> *thinUnderlineRectValues = [layout underlineRectsForRange:range
                                                                   inDrawingRect:drawingRect
                                                                       lineWidth:1.0
                                                                 underlineOffset:underlineOffset];
    NSArray<NSValue *> *thickUnderlineRectValues = [layout underlineRectsForRange:range
                                                                    inDrawingRect:drawingRect
                                                                        lineWidth:8.0
                                                                  underlineOffset:underlineOffset];
    XCTAssertEqual(1, thinUnderlineRectValues.count);
    XCTAssertEqual(1, thickUnderlineRectValues.count);
    CGRect thinUnderlineRect = thinUnderlineRectValues.firstObject.CGRectValue;
    CGRect thickUnderlineRect = thickUnderlineRectValues.firstObject.CGRectValue;

    CGFloat oldBottomAnchoredY = drawOriginY + CGRectGetMaxY(textBounds) - 0.5 + underlineOffset;

    XCTAssertEqualWithAccuracy(CGRectGetMidY(thinUnderlineRect), CGRectGetMidY(thickUnderlineRect), 0.001);
    XCTAssertEqualWithAccuracy(CGRectGetHeight(thickUnderlineRect), 8.0, 0.001);
    XCTAssertLessThan(CGRectGetMidY(thinUnderlineRect), oldBottomAnchoredY);
}

- (void)testUnderlineRectsSplitMixedFontRanges
{
    UIFont *smallFont = [UIFont systemFontOfSize:12];
    UIFont *largeFont = [UIFont systemFontOfSize:40];
    NSMutableAttributedString *attributedString =
        [[NSMutableAttributedString alloc] initWithString:@"small BIG"
                                               attributes:@{ NSFontAttributeName: smallFont }];
    [attributedString addAttribute:NSFontAttributeName
                             value:largeFont
                             range:NSMakeRange(6, 3)];

    SCValdiTextLayout *layout = [SCValdiTextLayout new];
    layout.attributedString = attributedString;

    CGRect drawingRect = CGRectMake(0, 0, 500, 100);
    layout.size = drawingRect.size;

    NSArray<NSValue *> *underlineRectValues = [layout underlineRectsForRange:NSMakeRange(0, attributedString.length)
                                                               inDrawingRect:drawingRect
                                                                   lineWidth:2.0
                                                             underlineOffset:0.0];
    XCTAssertEqual(2, underlineRectValues.count);

    CGRect smallUnderlineRect = underlineRectValues[0].CGRectValue;
    CGRect largeUnderlineRect = underlineRectValues[1].CGRectValue;
    CGFloat expectedMidYDelta = ((-largeFont.descender) - (-smallFont.descender)) / 2.0;

    XCTAssertLessThan(CGRectGetMidY(smallUnderlineRect), CGRectGetMidY(largeUnderlineRect));
    XCTAssertEqualWithAccuracy(CGRectGetMidY(largeUnderlineRect) - CGRectGetMidY(smallUnderlineRect),
                               expectedMidYDelta,
                               0.001);
}

@end
