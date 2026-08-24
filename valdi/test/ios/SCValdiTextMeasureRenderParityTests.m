//
//  SCValdiTextMeasureRenderParityTests.m
//  valdi-ios
//
//  Measurement must predict rendering: text measured with
//  SCValdiTextLayout measureSizeWithMaxSize: is used to size containers whose
//  content is then laid out by TextKit (SCValdiLabel draws through
//  SCValdiTextLayout, SCValdiTextView is a UITextView).
//  NSStringDrawing's boundingRectWithSize: excludes each line's font `leading`,
//  so for fonts with nonzero leading (Arial, Futura, HelveticaNeue)
//  it under-reports the rendered height by roughly leading x lines. A caption
//  sized from that measure clips the last line's descenders once the surplus
//  exceeds rounding slack.
//

#import <XCTest/XCTest.h>

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi_core/SCValdiRectUtils.h"

@interface SCValdiTextMeasureRenderParityTests : XCTestCase
@end

@implementation SCValdiTextMeasureRenderParityTests

- (void)tearDown
{
    [SCValdiTextLayout setFontLeadingInMeasureEnabled:YES];
    [super tearDown];
}

static SCValdiFontAttributes *ParityFontAttributes(UIFont *font)
{
    NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary new];
    attributes[NSFontAttributeName] = font;
    return [[SCValdiFontAttributes alloc] initWithAttributes:attributes
                                                        font:nil
                                                       color:[UIColor whiteColor]
                                                textAligment:NSTextAlignmentCenter
                                               numberOfLines:0
                                               lineBreakMode:NSLineBreakByWordWrapping
                                        needAttributedString:NO];
}

/// TextKit height of `text` laid out at `width`, mirroring how SCValdiTextView's
/// UITextView wraps its content (lineFragmentPadding 0, unlimited height).
static CGFloat ParityTextKitHeight(NSAttributedString *attributed, CGFloat width)
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

- (void)testMeasuredHeightFitsTextKitLayoutForFontsWithLeading
{
    NSString *text = @"The only way I could do that was if you";
    const CGFloat maxWidth = 386.0;
    UITraitCollection *traits = [UITraitCollection traitCollectionWithDisplayScale:3.0];

    // Zero-leading and nonzero-leading families, plus every fractional size a
    // pinch-to-scale gesture is known to produce.
    NSArray<NSString *> *fontNames = @[
        @"Helvetica-Bold", @"AvenirNext-Bold", @"ArialMT", @"Arial-BoldMT", @"Futura-Medium",
        @"HelveticaNeue", @"ChalkboardSE-Regular", @"Didot"
    ];
    NSMutableArray<NSNumber *> *sizes = [NSMutableArray new];
    [sizes addObject:@(20.98183485407094)];
    [sizes addObject:@(38.03290810399051)];
    [sizes addObject:@(40.61006718171105)];
    [sizes addObject:@(48.375803179339016)];
    for (CGFloat s = 15.0; s < 90.0; s += 1.7) {
        [sizes addObject:@(s)];
    }

    for (NSString *fontName in fontNames) {
        for (NSNumber *sizeNumber in sizes) {
            CGFloat fontSize = sizeNumber.doubleValue;
            UIFont *font = [UIFont fontWithName:fontName size:fontSize];
            if (!font) {
                continue;
            }
            SCValdiFontAttributes *fontAttributes = ParityFontAttributes(font);

            CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(maxWidth, CGFLOAT_MAX)
                                                          fontAttributes:fontAttributes
                                                             fontManager:nil
                                                                    text:text
                                                         traitCollection:traits];

            NSDictionary *resolved = [fontAttributes resolveAttributesWithIsRightToLeft:NO
                                                                        traitCollection:traits];
            NSAttributedString *attributed = [[NSAttributedString alloc] initWithString:text
                                                                              attributes:resolved];
            // The measured width becomes the hug width the text re-wraps at.
            CGFloat renderedHeight = ParityTextKitHeight(attributed, ceil(measured.width));

            XCTAssertGreaterThanOrEqual(
                measured.height + 0.01, renderedHeight,
                @"font %@ at %.4fpt: measured height %.2f is shorter than the TextKit-rendered "
                @"height %.2f — text drawn into a container sized from this measure clips",
                fontName, fontSize, measured.height, renderedHeight);
        }
    }
}

/// Blast-radius guard: for zero-leading fonts (Helvetica, AvenirNext, the SF system font —
/// the app's standard UI fonts) the option contributes no line height, so the size callers
/// receive must be unchanged. NSStringDrawing's raw rect does shift by a sub-pixel amount
/// even at zero leading, so what is pinned here is the normalized size this method returns,
/// swept finely enough to catch a shift that crossed a pixel boundary.
- (void)testZeroLeadingFontsMeasureUnchangedByFontLeadingOption
{
    NSString *text = @"The only way I could do that was if you";
    const CGSize maxSize = CGSizeMake(386.0, CGFLOAT_MAX);
    UITraitCollection *traits = [UITraitCollection traitCollectionWithDisplayScale:3.0];

    NSMutableArray<UIFont *> *fonts = [NSMutableArray new];
    for (CGFloat s = 12.0; s < 90.0; s += 0.25) {
        [fonts addObject:[UIFont fontWithName:@"Helvetica" size:s]];
        [fonts addObject:[UIFont fontWithName:@"Helvetica-Bold" size:s]];
        [fonts addObject:[UIFont fontWithName:@"AvenirNext-Bold" size:s]];
        [fonts addObject:[UIFont systemFontOfSize:s]];
    }

    for (UIFont *font in fonts) {
        XCTAssertEqual(font.leading, 0.0, @"%@ is expected to be a zero-leading font", font.fontName);
        SCValdiFontAttributes *fontAttributes = ParityFontAttributes(font);

        CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                      fontAttributes:fontAttributes
                                                         fontManager:nil
                                                                text:text
                                                     traitCollection:traits];

        // The measurement path exactly as it computed before NSStringDrawingUsesFontLeading
        // was added.
        NSDictionary *resolved = [fontAttributes resolveAttributesWithIsRightToLeft:NO
                                                                    traitCollection:traits];
        NSStringDrawingContext *context = [[NSStringDrawingContext alloc] init];
        [context setValue:@(fontAttributes.numberOfLines) forKey:@"maximumNumberOfLines"];
        CGRect legacyRect = [text boundingRectWithSize:maxSize
                                               options:NSStringDrawingUsesLineFragmentOrigin |
                                                       NSStringDrawingTruncatesLastVisibleLine
                                            attributes:resolved
                                               context:context];

        XCTAssertEqual(measured.width, CGFloatNormalizeCeil(legacyRect.size.width),
                       @"width changed for zero-leading font %@ at %.2fpt", font.fontName, font.pointSize);
        XCTAssertEqual(measured.height, CGFloatNormalizeCeil(legacyRect.size.height),
                       @"height changed for zero-leading font %@ at %.2fpt", font.fontName, font.pointSize);
    }
}

/// Same blast-radius guard for the attributed-text branch.
- (void)testZeroLeadingFontsMeasureUnchangedByFontLeadingOptionForAttributedText
{
    UITraitCollection *traits = [UITraitCollection traitCollectionWithDisplayScale:3.0];
    const CGSize maxSize = CGSizeMake(386.0, CGFLOAT_MAX);
    NSString *text = @"The only way I could do that was if you";

    for (CGFloat fontSize = 15.0; fontSize < 90.0; fontSize += 1.7) {
        for (NSString *fontName in @[ @"Helvetica", @"AvenirNext-Bold" ]) {
            UIFont *font = [UIFont fontWithName:fontName size:fontSize];
            XCTAssertEqual(font.leading, 0.0, @"%@ is expected to be a zero-leading font", fontName);
            NSAttributedString *attributed =
                [[NSAttributedString alloc] initWithString:text attributes:@{NSFontAttributeName : font}];

            CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                          fontAttributes:ParityFontAttributes(font)
                                                             fontManager:nil
                                                                    text:attributed
                                                         traitCollection:traits];

            NSStringDrawingContext *context = [[NSStringDrawingContext alloc] init];
            [context setValue:@(0) forKey:@"maximumNumberOfLines"];
            CGRect legacyRect = [attributed boundingRectWithSize:maxSize
                                                         options:NSStringDrawingUsesLineFragmentOrigin |
                                                                 NSStringDrawingTruncatesLastVisibleLine
                                                         context:context];

            XCTAssertEqual(measured.width, CGFloatNormalizeCeil(legacyRect.size.width),
                           @"attributed width changed for %@ at %.2fpt", fontName, fontSize);
            XCTAssertEqual(measured.height, CGFloatNormalizeCeil(legacyRect.size.height),
                           @"attributed height changed for %@ at %.2fpt", fontName, fontSize);
        }
    }
}

/// Attributed text takes a separate measurement branch (SCValdiProcessedText, then
/// -boundingRectWithSize:options:context:), and captions carry attributed text once they
/// contain mentions or styled ranges. Same fit-the-render requirement applies there.
- (void)testMeasuredHeightFitsTextKitLayoutForAttributedTextWithLeading
{
    UITraitCollection *traits = [UITraitCollection traitCollectionWithDisplayScale:3.0];
    const CGSize maxSize = CGSizeMake(386.0, CGFLOAT_MAX);
    NSString *text = @"The only way I could do that was if you";

    for (NSString *fontName in @[ @"ArialMT", @"Futura-Medium", @"HelveticaNeue", @"Didot" ]) {
        for (CGFloat fontSize = 18.0; fontSize < 80.0; fontSize += 7.3) {
            UIFont *font = [UIFont fontWithName:fontName size:fontSize];
            if (!font) {
                continue;
            }
            NSAttributedString *attributed =
                [[NSAttributedString alloc] initWithString:text attributes:@{NSFontAttributeName : font}];

            CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                          fontAttributes:ParityFontAttributes(font)
                                                             fontManager:nil
                                                                    text:attributed
                                                         traitCollection:traits];

            CGFloat renderedHeight = ParityTextKitHeight(attributed, ceil(measured.width));
            XCTAssertGreaterThanOrEqual(measured.height + 0.01, renderedHeight,
                                        @"attributed %@ at %.2fpt: measured height %.2f is shorter than the "
                                        @"TextKit-rendered height %.2f",
                                        fontName, fontSize, measured.height, renderedHeight);
        }
    }
}

/// A line fragment's height is driven by the tallest font on that line, so a mixed-font run
/// (plain caption text with a styled mention) must still measure at least as tall as it renders.
- (void)testMeasuredHeightFitsTextKitLayoutForMixedFontAttributedText
{
    UIFont *bodyFont = [UIFont fontWithName:@"Helvetica" size:44.0];
    UIFont *mentionFont = [UIFont fontWithName:@"Futura-Medium" size:52.0];
    if (!bodyFont || !mentionFont) {
        return;
    }

    NSMutableAttributedString *attributed =
        [[NSMutableAttributedString alloc] initWithString:@"look at this "
                                              attributes:@{NSFontAttributeName : bodyFont}];
    [attributed appendAttributedString:[[NSAttributedString alloc] initWithString:@"@spencer"
                                                                      attributes:@{NSFontAttributeName : mentionFont}]];

    CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(300, CGFLOAT_MAX)
                                                  fontAttributes:ParityFontAttributes(bodyFont)
                                                     fontManager:nil
                                                            text:attributed
                                                 traitCollection:[UITraitCollection traitCollectionWithDisplayScale:3.0]];

    CGFloat renderedHeight = ParityTextKitHeight(attributed, ceil(measured.width));
    XCTAssertGreaterThanOrEqual(measured.height + 0.01, renderedHeight,
                               @"mixed-font measured height %.2f is shorter than the TextKit-rendered height %.2f",
                               measured.height, renderedHeight);
}

- (void)testEmptyTextMeasuresZero
{
    SCValdiFontAttributes *fontAttributes = ParityFontAttributes([UIFont systemFontOfSize:20]);
    CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(300, CGFLOAT_MAX)
                                                  fontAttributes:fontAttributes
                                                     fontManager:nil
                                                            text:@""
                                                 traitCollection:[UITraitCollection traitCollectionWithDisplayScale:3.0]];
    XCTAssertEqual(measured.width, 0.0);
    XCTAssertEqual(measured.height, 0.0);
}

- (void)testEmptyAttributedTextMeasuresZero
{
    SCValdiFontAttributes *fontAttributes = ParityFontAttributes([UIFont systemFontOfSize:20]);
    NSAttributedString *empty = [[NSAttributedString alloc] initWithString:@""];
    CGSize measured = [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(300, CGFLOAT_MAX)
                                                  fontAttributes:fontAttributes
                                                     fontManager:nil
                                                            text:empty
                                                 traitCollection:[UITraitCollection traitCollectionWithDisplayScale:3.0]];
    XCTAssertEqual(measured.width, 0.0);
    XCTAssertEqual(measured.height, 0.0);
}

/// The leading-inclusive measure ships behind a process-wide killswitch
/// (SCValdiConfiguration.disableFontLeadingInTextMeasure). Killswitched, measurement must
/// reproduce the legacy leading-less NSStringDrawing result bit-for-bit; enabled (the default)
/// it must cover the TextKit-rendered height the legacy measure fell short of.
- (void)testFontLeadingKillswitchRestoresLegacyMeasurement
{
    XCTAssertTrue([SCValdiTextLayout fontLeadingInMeasureEnabled], @"the fix must be on by default");

    NSString *text = @"The only way I could do that was if you";
    const CGSize maxSize = CGSizeMake(386.0, CGFLOAT_MAX);
    UITraitCollection *traits = [UITraitCollection traitCollectionWithDisplayScale:3.0];
    UIFont *font = [UIFont fontWithName:@"ArialMT" size:48.375803179339016];
    XCTAssertGreaterThan(font.leading, 0.0, @"this guard needs a nonzero-leading font");
    SCValdiFontAttributes *fontAttributes = ParityFontAttributes(font);

    CGSize enabled = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                 fontAttributes:fontAttributes
                                                    fontManager:nil
                                                           text:text
                                                traitCollection:traits];

    [SCValdiTextLayout setFontLeadingInMeasureEnabled:NO];
    CGSize killswitched = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                      fontAttributes:fontAttributes
                                                         fontManager:nil
                                                                text:text
                                                     traitCollection:traits];
    [SCValdiTextLayout setFontLeadingInMeasureEnabled:YES];

    NSDictionary *resolved = [fontAttributes resolveAttributesWithIsRightToLeft:NO traitCollection:traits];
    NSStringDrawingContext *context = [[NSStringDrawingContext alloc] init];
    [context setValue:@(fontAttributes.numberOfLines) forKey:@"maximumNumberOfLines"];
    CGRect legacyRect = [text boundingRectWithSize:maxSize
                                           options:NSStringDrawingUsesLineFragmentOrigin |
                                                   NSStringDrawingTruncatesLastVisibleLine
                                        attributes:resolved
                                           context:context];

    XCTAssertEqual(killswitched.width, CGFloatNormalizeCeil(legacyRect.size.width));
    XCTAssertEqual(killswitched.height, CGFloatNormalizeCeil(legacyRect.size.height));
    XCTAssertGreaterThan(enabled.height, killswitched.height,
                         @"a nonzero-leading font must measure taller with the fix enabled");
}

- (void)testMaxNumberOfLinesLimitsMeasuredHeight
{
    UIFont *font = [UIFont systemFontOfSize:20];
    NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary new];
    attributes[NSFontAttributeName] = font;
    SCValdiFontAttributes *twoLines = [[SCValdiFontAttributes alloc] initWithAttributes:attributes
                                                                                    font:nil
                                                                                   color:[UIColor whiteColor]
                                                                            textAligment:NSTextAlignmentLeft
                                                                           numberOfLines:2
                                                                           lineBreakMode:NSLineBreakByWordWrapping
                                                                    needAttributedString:NO];
    NSString *longText = @"one two three four five six seven eight nine ten eleven twelve "
                         @"thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty";
    CGSize limited = [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(120, CGFLOAT_MAX)
                                                 fontAttributes:twoLines
                                                    fontManager:nil
                                                           text:longText
                                                traitCollection:[UITraitCollection traitCollectionWithDisplayScale:3.0]];
    CGSize unlimited = [SCValdiTextLayout measureSizeWithMaxSize:CGSizeMake(120, CGFLOAT_MAX)
                                                   fontAttributes:ParityFontAttributes(font)
                                                      fontManager:nil
                                                             text:longText
                                                  traitCollection:[UITraitCollection traitCollectionWithDisplayScale:3.0]];
    XCTAssertLessThan(limited.height, unlimited.height);
    XCTAssertLessThanOrEqual(limited.height, 2.0 * font.lineHeight + 2.0 * font.leading + 1.0);
}

@end
