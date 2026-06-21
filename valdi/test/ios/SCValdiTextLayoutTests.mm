#import <XCTest/XCTest.h>

#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiTextAnimationTransform.h"
#import "valdi/ios/Text/SCValdiTextAnimationCoordinator.h"
#import "valdi/ios/Views/SCValdiTextViewEffectsLayoutManager.h"

@interface SCValdiTextLayoutTests: XCTestCase

@end

@implementation SCValdiTextLayoutTests

- (void)testCanUseSuppliedLayoutManager
{
    NSLayoutManager *layoutManager = [NSLayoutManager new];
    SCValdiTextLayout *layout = [[SCValdiTextLayout alloc] initWithLayoutManager:layoutManager];

    XCTAssertEqual(layout.layoutManager, layoutManager);
}

- (void)testAnimationLayoutManagerReportsActiveAnimation
{
    SCValdiTextViewEffectsLayoutManager *layoutManager = [SCValdiTextViewEffectsLayoutManager new];
    SCValdiTextLayout *layout = [[SCValdiTextLayout alloc] initWithLayoutManager:layoutManager];
    SCValdiTextAnimationTransform *animationTransform =
        [[SCValdiTextAnimationTransform alloc] initWithKey:@"title"
                                                 partIndex:0
                                              translationY:12.0
                                                     scale:0.5
                                                   opacity:0.0
                                                  duration:1.0
                                    timeOffsetBetweenParts:0.0
                                                groupIndex:0
                                          partIndexInGroup:0];
    NSAttributedString *attributedString =
        [[NSAttributedString alloc] initWithString:@"Hello"
                                        attributes:@{
                                            NSFontAttributeName: [UIFont systemFontOfSize:20],
                                            kSCValdiAttributedStringKeyAnimationTransform: animationTransform,
                                        }];

    layout.attributedString = attributedString;
    layout.size = CGSizeMake(200, 100);

    XCTAssertTrue([attributedString hasValdiAnimationTransform]);
    XCTAssertTrue([layoutManager invalidateAnimatedTextProgress]);
    XCTAssertTrue(layoutManager.hasActiveAnimationRanges);
}

- (void)testTextAnimationCoordinatorReusesStartTimeForNewAnimationsInSameTimeline
{
    SCValdiTextAnimationCoordinator *coordinator = [SCValdiTextAnimationCoordinator new];

    CFTimeInterval firstStartTime = [coordinator startTimeForNewAnimationWithTimelineKey:@"intro"
                                                                              timeOffset:0.08
                                                                             currentTime:10.0];
    CFTimeInterval secondStartTime = [coordinator startTimeForNewAnimationWithTimelineKey:@"intro"
                                                                               timeOffset:0.08
                                                                              currentTime:10.4];

    XCTAssertEqualWithAccuracy(firstStartTime, 10.0, 0.001);
    XCTAssertEqualWithAccuracy(secondStartTime, 10.0, 0.001);
}

- (void)testTextAnimationCoordinatorOffsetsAfterExistingAnimations
{
    SCValdiTextAnimationCoordinator *coordinator = [SCValdiTextAnimationCoordinator new];
    [coordinator recordExistingAnimationScheduledStartTime:12.0 forTimelineKey:@"intro"];

    CFTimeInterval startTime = [coordinator startTimeForNewAnimationWithTimelineKey:@"intro"
                                                                         timeOffset:0.08
                                                                        currentTime:10.0];

    XCTAssertEqualWithAccuracy(startTime, 12.08, 0.001);
}

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
