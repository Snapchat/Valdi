#import <XCTest/XCTest.h>

#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi/ios/Views/SCValdiTextView.h"
#import "valdi_core/SCValdiFontManagerProtocol.h"
#import "valdi_core/SCValdiViewLayoutAttributes.h"

// Regression coverage for the live UITextView consuming backgroundEffectPadding as
// lineFragmentPadding (both horizontal sides) plus a vertical textContainerInset, so measurement
// must reserve the same space. Without the reserve, a text view sized to its measured width loses
// 2x padding of usable width and wraps one character per line (a bubble-wrap caption
// rendered vertically in a caption editor).
@interface SCValdiTextView (MeasurementTesting)
+ (CGSize)measureSizeWithMaxSize:(CGSize)maxSize
                   fontAttributes:(SCValdiFontAttributes *)fontAttributes
                      fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                             text:(id)text
                      placeholder:(NSString *)placeholder
          backgroundEffectPadding:(CGFloat)backgroundEffectPadding
                  traitCollection:(UITraitCollection *)traitCollection;
+ (CGSize)valdi_onMeasureWithAttributes:(id<SCValdiViewLayoutAttributes>)attributes
                                maxSize:(CGSize)maxSize
                            fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                        traitCollection:(UITraitCollection *)traitCollection;
@end

@interface SCValdiTestLayoutAttributes : NSObject <SCValdiViewLayoutAttributes>
@property (nonatomic, strong) NSDictionary<NSString *, id> *values;
@end

@implementation SCValdiTestLayoutAttributes
- (id)valueForAttributeName:(NSString *)attributeName
{
    return self.values[attributeName];
}
- (BOOL)boolValueForAttributeName:(NSString *)attributeName
{
    return [self.values[attributeName] boolValue];
}
- (NSString *)stringValueForAttributeName:(NSString *)attributeName
{
    return self.values[attributeName];
}
- (CGFloat)doubleValueForAttributeName:(NSString *)attributeName
{
    return [self.values[attributeName] doubleValue];
}
@end

@interface SCValdiTextViewMeasurementTests : XCTestCase
@end

@implementation SCValdiTextViewMeasurementTests

static UITraitCollection *SCTestTraitCollection(void)
{
    return [UITraitCollection traitCollectionWithDisplayScale:2.0];
}

static SCValdiFontAttributes *SCTestFontAttributes(void)
{
    NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary new];
    attributes[NSFontAttributeName] = [UIFont systemFontOfSize:28];
    return [[SCValdiFontAttributes alloc] initWithAttributes:attributes
                                                       font:nil
                                                      color:[UIColor blackColor]
                                               textAligment:NSTextAlignmentCenter
                                              numberOfLines:0
                                              lineBreakMode:NSLineBreakByWordWrapping
                                       needAttributedString:NO];
}

static CGSize SCTestMeasure(NSString *text, CGSize maxSize, CGFloat backgroundEffectPadding)
{
    return [SCValdiTextView measureSizeWithMaxSize:maxSize
                                    fontAttributes:SCTestFontAttributes()
                                       fontManager:nil
                                              text:text
                                       placeholder:nil
                           backgroundEffectPadding:backgroundEffectPadding
                                   traitCollection:SCTestTraitCollection()];
}

- (void)testBackgroundEffectPaddingIsReservedInMeasuredSize
{
    const CGFloat padding = 10.0;
    const CGSize maxSize = CGSizeMake(400.0, CGFLOAT_MAX);

    CGSize withoutPadding = SCTestMeasure(@"test", maxSize, 0.0);
    CGSize withPadding = SCTestMeasure(@"test", maxSize, padding);

    XCTAssertEqualWithAccuracy(withPadding.width,
                               withoutPadding.width + padding * 2.0,
                               0.5,
                               @"Measured width must reserve lineFragmentPadding on both sides");
    XCTAssertEqualWithAccuracy(withPadding.height,
                               withoutPadding.height + padding,
                               0.5,
                               @"Measured height must reserve the vertical textContainerInset");
}

- (void)testUsableWidthAtMeasuredSizeFitsTextOnOneLine
{
    const CGFloat padding = 10.0;
    const CGSize maxSize = CGSizeMake(400.0, CGFLOAT_MAX);

    CGSize rawTextSize = SCTestMeasure(@"test", maxSize, 0.0);
    CGSize measured = SCTestMeasure(@"test", maxSize, padding);

    // The live text container gets (measured width - 2x padding) of usable width; if that is
    // narrower than the text itself, a hugging text view re-wraps and renders vertically.
    XCTAssertGreaterThanOrEqual(measured.width - padding * 2.0 + 0.5,
                                rawTextSize.width,
                                @"Usable width at the measured size must still fit the text");
}

- (void)testConstrainedMeasureReservesPaddingFromAvailableSpace
{
    const CGFloat padding = 10.0;
    const CGSize maxSize = CGSizeMake(120.0, 200.0);
    NSString *text = @"a longer caption that has to wrap across lines";

    CGSize padded = SCTestMeasure(text, maxSize, padding);
    CGSize reduced = SCTestMeasure(text, CGSizeMake(maxSize.width - padding * 2.0, maxSize.height - padding), 0.0);

    XCTAssertEqualWithAccuracy(padded.width, reduced.width + padding * 2.0, 0.5);
    XCTAssertEqualWithAccuracy(padded.height, reduced.height + padding, 0.5);
}

- (void)testZeroPaddingMatchesRawTextLayoutMeasurement
{
    const CGSize maxSize = CGSizeMake(400.0, CGFLOAT_MAX);

    CGSize viaTextView = SCTestMeasure(@"test", maxSize, 0.0);
    CGSize viaTextLayout = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                      fontAttributes:SCTestFontAttributes()
                                                         fontManager:nil
                                                                text:@"test"
                                                     traitCollection:SCTestTraitCollection()];

    XCTAssertEqualWithAccuracy(viaTextView.width, viaTextLayout.width, 0.5);
    XCTAssertEqualWithAccuracy(viaTextView.height, viaTextLayout.height, 0.5);
}

// The core regression contract — an empty/nil value must measure one line, not zero — is
// pinned in SCValdiTextMeasureRenderParityTests. This guards the secondary path: when a
// placeholder is also bound, the input must measure the placeholder's size.
- (void)testEmptyTextWithPlaceholderMeasuresPlaceholderSize
{
    const CGSize maxSize = CGSizeMake(400.0, CGFLOAT_MAX);
    SCValdiFontAttributes *fontAttributes = SCTestFontAttributes();
    UITraitCollection *traits = SCTestTraitCollection();

    CGSize placeholderOnly = [SCValdiTextView measureSizeWithMaxSize:maxSize
                                                      fontAttributes:fontAttributes
                                                         fontManager:nil
                                                                text:@"Send a chat"
                                                         placeholder:nil
                                             backgroundEffectPadding:0.0
                                                     traitCollection:traits];
    XCTAssertGreaterThan(placeholderOnly.width, 0.0);
    XCTAssertGreaterThan(placeholderOnly.height, 0.0);

    for (id text in @[ @"", [NSNull null] ]) {
        id textOrNil = (text == [NSNull null]) ? nil : text;
        CGSize measured = [SCValdiTextView measureSizeWithMaxSize:maxSize
                                                   fontAttributes:fontAttributes
                                                      fontManager:nil
                                                             text:textOrNil
                                                      placeholder:@"Send a chat"
                                          backgroundEffectPadding:0.0
                                                  traitCollection:traits];
        XCTAssertEqualWithAccuracy(measured.width, placeholderOnly.width, 0.01);
        XCTAssertEqualWithAccuracy(measured.height, placeholderOnly.height, 0.01);
    }
}

// Same guard through the production measure entry point the layout engine calls
// (valdi_onMeasureWithAttributes), where value/placeholder arrive as layout attributes.
- (void)testOnMeasureWithEmptyValueAndPlaceholderIsNotZero
{
    SCValdiTestLayoutAttributes *attributes = [SCValdiTestLayoutAttributes new];
    attributes.values = @{ @"value" : @"", @"placeholder" : @"Send a chat" };

    CGSize measured = [SCValdiTextView valdi_onMeasureWithAttributes:attributes
                                                             maxSize:CGSizeMake(400.0, CGFLOAT_MAX)
                                                         fontManager:nil
                                                     traitCollection:SCTestTraitCollection()];

    XCTAssertGreaterThan(measured.width, 0.0, @"placeholder must size the empty input");
    XCTAssertGreaterThan(measured.height, 0.0, @"placeholder must size the empty input");
}

// The pure regression shape: an empty value with no placeholder bound must still measure one
// line tall through the layout engine's entry point. Everything sized from this measure (or
// from Drawing.measureText) collapses when it returns zero, whatever the view displays.
- (void)testOnMeasureWithEmptyValueAndNoPlaceholderMeasuresOneLineTall
{
    SCValdiTestLayoutAttributes *attributes = [SCValdiTestLayoutAttributes new];
    CGSize maxSize = CGSizeMake(400.0, CGFLOAT_MAX);
    UITraitCollection *traits = SCTestTraitCollection();

    attributes.values = @{ @"value" : @"" };
    CGSize empty = [SCValdiTextView valdi_onMeasureWithAttributes:attributes
                                                          maxSize:maxSize
                                                      fontManager:nil
                                                  traitCollection:traits];

    attributes.values = @{ @"value" : @"x" };
    CGSize oneLine = [SCValdiTextView valdi_onMeasureWithAttributes:attributes
                                                            maxSize:maxSize
                                                        fontManager:nil
                                                    traitCollection:traits];

    XCTAssertGreaterThan(oneLine.height, 0.0);
    XCTAssertEqual(empty.height, oneLine.height,
                   @"an empty value must measure one line tall, not collapse to zero");
}

@end
