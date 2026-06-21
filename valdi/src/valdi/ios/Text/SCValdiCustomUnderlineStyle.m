//
//  SCValdiCustomUnderlineStyle.m
//  Valdi
//

#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi_core/SCMacros.h"

#include <math.h>

static NSString *const SCValdiCustomUnderlineStyleErrorDomain = @"SCValdiCustomUnderlineStyleErrorDomain";

static void SCValdiCustomUnderlineStyleSetError(NSError **error, NSString *message)
{
    if (error) {
        *error = [NSError errorWithDomain:SCValdiCustomUnderlineStyleErrorDomain
                                     code:0
                                 userInfo:@{NSLocalizedDescriptionKey: message}];
    }
}

static BOOL SCValdiCustomUnderlineStyleScanNumber(NSScanner *scanner, double *value, NSError **error)
{
    if (![scanner scanDouble:value]) {
        SCValdiCustomUnderlineStyleSetError(error, @"Invalid customUnderlineStyle number");
        return NO;
    }

    if (!isfinite(*value)) {
        SCValdiCustomUnderlineStyleSetError(error, @"customUnderlineStyle values must be finite numbers");
        return NO;
    }

    return YES;
}

static BOOL SCValdiCustomUnderlineColorIsVisible(UIColor *color)
{
    return color != nil && CGColorGetAlpha(color.CGColor) > 0;
}

BOOL SCValdiCustomUnderlineShouldReplaceNativeUnderline(id value)
{
    NSNumber *underlineStyleValue = ObjectAs(value, NSNumber);
    if (!underlineStyleValue) {
        return NO;
    }

    NSInteger underlineStyle = underlineStyleValue.integerValue;
    return (underlineStyle & NSUnderlineStyleSingle) == NSUnderlineStyleSingle;
}

UIColor *SCValdiCustomUnderlineColorForRange(NSAttributedString *attributedString,
                                             NSRange range,
                                             UIColor *fallbackColor)
{
    if (attributedString.length == 0 || range.location >= attributedString.length) {
        return fallbackColor ?: [UIColor blackColor];
    }

    UIColor *underlineColor = ObjectAs([attributedString attribute:NSUnderlineColorAttributeName
                                                           atIndex:range.location
                                                    effectiveRange:nil], UIColor);
    if (SCValdiCustomUnderlineColorIsVisible(underlineColor)) {
        return underlineColor;
    }

    UIColor *foregroundColor = ObjectAs([attributedString attribute:NSForegroundColorAttributeName
                                                            atIndex:range.location
                                                     effectiveRange:nil], UIColor);
    if (SCValdiCustomUnderlineColorIsVisible(foregroundColor)) {
        return foregroundColor;
    }

    return fallbackColor ?: [UIColor blackColor];
}

NSArray<NSValue *> *SCValdiCustomUnderlineRemoveNativeUnderlines(NSMutableAttributedString *attributedString,
                                                                 BOOL removeUnderlineColor)
{
    __block NSMutableArray<NSValue *> *ranges = nil;
    [attributedString enumerateAttribute:NSUnderlineStyleAttributeName
                                 inRange:NSMakeRange(0, attributedString.length)
                                 options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                              usingBlock:^(id value, NSRange range, BOOL *stop) {
        (void)stop;
        if (!SCValdiCustomUnderlineShouldReplaceNativeUnderline(value)) {
            return;
        }
        if (ranges == nil) {
            ranges = [NSMutableArray new];
        }
        [ranges addObject:[NSValue valueWithRange:range]];
    }];

    for (NSValue *rangeValue in ranges) {
        NSRange range = rangeValue.rangeValue;
        [attributedString removeAttribute:NSUnderlineStyleAttributeName range:range];
        if (removeUnderlineColor) {
            [attributedString removeAttribute:NSUnderlineColorAttributeName range:range];
        }
    }
    return ranges;
}

void SCValdiCustomUnderlineApplyDashPattern(CGContextRef context, SCValdiCustomUnderlineStyle *style)
{
    if (!style.patterned) {
        CGContextSetLineDash(context, 0, nil, 0);
        return;
    }

    CGFloat lengths[] = {style.onWidth, style.offWidth};
    CGContextSetLineDash(context, 0, lengths, 2);
}

NSArray<NSValue *> *SCValdiCustomUnderlineRectsForRange(NSAttributedString *attributedString,
                                                        NSLayoutManager *layoutManager,
                                                        NSRange range,
                                                        NSRange visibleGlyphRange,
                                                        BOOL clipToVisibleGlyphRange,
                                                        CGPoint origin,
                                                        CGFloat lineWidth,
                                                        CGFloat underlineOffset)
{
    if (range.length == 0 || range.location == NSNotFound || range.location >= attributedString.length) {
        return @[];
    }

    NSRange clampedRange = NSIntersectionRange(range, NSMakeRange(0, attributedString.length));
    if (clampedRange.length == 0) {
        return @[];
    }

    NSMutableArray<NSValue *> *rects = [NSMutableArray array];
    [attributedString enumerateAttribute:NSFontAttributeName
                                 inRange:clampedRange
                                 options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                              usingBlock:^(id value, NSRange fontRange, BOOL *stop) {
        (void)stop;

        UIFont *font = [value isKindOfClass:[UIFont class]] ? value : nil;
        NSRange characterRun = NSIntersectionRange(clampedRange, fontRange);
        if (characterRun.length == 0) {
            return;
        }

        NSRange glyphRun = [layoutManager glyphRangeForCharacterRange:characterRun actualCharacterRange:nil];
        if (clipToVisibleGlyphRange) {
            glyphRun = NSIntersectionRange(glyphRun, visibleGlyphRange);
        }
        if (glyphRun.length == 0) {
            return;
        }

        [layoutManager enumerateLineFragmentsForGlyphRange:glyphRun
                                                usingBlock:^(CGRect lineRect,
                                                             CGRect usedRect,
                                                             NSTextContainer *textContainer,
                                                             NSRange lineGlyphRange,
                                                             BOOL *lineStop) {
            (void)usedRect;
            (void)lineStop;

            NSRange runOnLine = NSIntersectionRange(glyphRun, lineGlyphRange);
            if (runOnLine.length == 0) {
                return;
            }

            CGRect boundingRect = [layoutManager boundingRectForGlyphRange:runOnLine inTextContainer:textContainer];
            if (CGRectIsEmpty(boundingRect)) {
                return;
            }

            CGPoint glyphLocation = [layoutManager locationForGlyphAtIndex:runOnLine.location];
            CGFloat baselineY = CGRectGetMinY(lineRect) + glyphLocation.y;
            CGFloat descentDistance = font ? -font.descender : 0;
            CGFloat underlineCenterY = origin.y + baselineY + descentDistance / 2.0 + underlineOffset;
            CGRect underlineRect = CGRectMake(origin.x + CGRectGetMinX(boundingRect),
                                              underlineCenterY - lineWidth / 2.0,
                                              CGRectGetWidth(boundingRect),
                                              lineWidth);
            [rects addObject:[NSValue valueWithCGRect:underlineRect]];
        }];
    }];

    return rects;
}

void SCValdiCustomUnderlineDrawRects(CGContextRef context,
                                     NSArray<NSValue *> *underlineRects)
{
    for (NSValue *rectValue in underlineRects) {
        CGRect underlineRect = rectValue.CGRectValue;
        if (CGRectIsEmpty(underlineRect)) {
            continue;
        }

        CGFloat y = CGRectGetMidY(underlineRect);
        CGContextMoveToPoint(context, CGRectGetMinX(underlineRect), y);
        CGContextAddLineToPoint(context, CGRectGetMaxX(underlineRect), y);
        CGContextStrokePath(context);
    }
}

@implementation SCValdiCustomUnderlineStyle

- (instancetype)initWithHeight:(CGFloat)height
                       onWidth:(CGFloat)onWidth
                      offWidth:(CGFloat)offWidth
                        offset:(CGFloat)offset
{
    self = [super init];
    if (self) {
        _height = height;
        _onWidth = onWidth;
        _offWidth = offWidth;
        _offset = offset;
    }
    return self;
}

- (BOOL)isPatterned
{
    return _onWidth > 0 && _offWidth > 0;
}

+ (instancetype)styleWithString:(NSString *)styleString error:(NSError **)error
{
    if (error) {
        *error = nil;
    }

    NSScanner *scanner = [NSScanner scannerWithString:styleString];
    scanner.charactersToBeSkipped = [NSCharacterSet whitespaceAndNewlineCharacterSet];

    double height = 0;
    double onWidth = 0;
    double offWidth = 0;
    double offset = 0;
    if (!SCValdiCustomUnderlineStyleScanNumber(scanner, &height, error)
        || !SCValdiCustomUnderlineStyleScanNumber(scanner, &onWidth, error)
        || !SCValdiCustomUnderlineStyleScanNumber(scanner, &offWidth, error)
        || !SCValdiCustomUnderlineStyleScanNumber(scanner, &offset, error)) {
        return nil;
    }

    if (!scanner.isAtEnd) {
        SCValdiCustomUnderlineStyleSetError(
            error, @"customUnderlineStyle must contain exactly four numbers: height onWidth offWidth offset");
        return nil;
    }

    if (height <= 0) {
        SCValdiCustomUnderlineStyleSetError(error, @"customUnderlineStyle height must be positive");
        return nil;
    }

    BOOL solid = onWidth == 0 && offWidth == 0;
    BOOL patterned = onWidth > 0 && offWidth > 0;
    if (!solid && !patterned) {
        SCValdiCustomUnderlineStyleSetError(
            error, @"customUnderlineStyle onWidth and offWidth must both be positive, or both be 0");
        return nil;
    }

    return [[self alloc] initWithHeight:height onWidth:onWidth offWidth:offWidth offset:offset];
}

@end
