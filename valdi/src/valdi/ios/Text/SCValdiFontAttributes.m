//
//  SCValdiFontAttributes.m
//  valdi-ios
//
//  Created by Simon Corsin on 5/18/20.
//

#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/SCValdiLogger.h"

NSAttributedStringKey const SCValdiLineHeightAttributeName = @"valdi_lineHeight";
NSAttributedStringKey const SCValdiLineHeightAbsoluteAttributeName = @"valdi_lineHeightAbsolute";
static CGFloat const SCValdiBaselineOffsetEpsilon = 0.001;

NSTextAlignment SCValdiFontAttributesResolveTextAlignment(NSTextAlignment textAlignment, BOOL isRightToLeft)
{
    if (isRightToLeft) {
        if (textAlignment == NSTextAlignmentRight) {
            return NSTextAlignmentLeft;
        } else if (textAlignment == NSTextAlignmentLeft) {
            return NSTextAlignmentRight;
        }
    }
    return textAlignment;
}

@implementation SCValdiFontAttributes {
    NSTextAlignment _textAlignment;
    NSDictionary<NSAttributedStringKey, id>* _attributesOriginal;
    NSDictionary<NSAttributedStringKey, id>* _attributesResolvedLeftToRight;
    NSDictionary<NSAttributedStringKey, id>* _attributesResolvedRightToLeft;
    UITraitCollection *_lastTraitCollectionLeftToRight;
    UITraitCollection *_lastTraitCollectionRightToLeft;
}

- (instancetype)initWithAttributes:(NSDictionary<NSAttributedStringKey, id> *)attributes
                              font:(SCValdiFont *)font
                             color:(UIColor *)color
                      textAligment:(NSTextAlignment)textAlignment
                     numberOfLines:(NSInteger)numberOfLines
                     lineBreakMode:(NSLineBreakMode)lineBreakMode
              needAttributedString:(BOOL)needAttributedString
{
    self = [self init];

    if (self) {
        _attributesOriginal = attributes;
        _attributesResolvedLeftToRight = nil; // lazily created
        _attributesResolvedRightToLeft = nil; // lazily created
        _lastTraitCollectionLeftToRight = nil;
        _lastTraitCollectionRightToLeft = nil;
        _font = font;
        _color = color;
        _textAlignment = textAlignment;
        _needAttributedString = needAttributedString;
        _lineBreakMode = lineBreakMode;
        _numberOfLines = numberOfLines;
    }

    return self;
}

- (NSTextAlignment)resolveTextAlignmentWithIsRightToLeft:(BOOL)isRightToLeft
{
    return SCValdiFontAttributesResolveTextAlignment(_textAlignment, isRightToLeft);
}

- (NSDictionary<NSAttributedStringKey, id> *)buildAttributesWithIsRightToLeft:(BOOL)isRightToLeft
                                                              traitCollection:(UITraitCollection *)traitCollection

{
    NSMutableDictionary<NSAttributedStringKey, id>* attributesResolved = [_attributesOriginal mutableCopy];
    NSParagraphStyle *paragraphStyle = attributesResolved[NSParagraphStyleAttributeName];
    if (isRightToLeft) {
        if (paragraphStyle != nil) {
            NSMutableParagraphStyle *paragraphStyleUpdated = [paragraphStyle mutableCopy];
            paragraphStyleUpdated.alignment = SCValdiFontAttributesResolveTextAlignment(paragraphStyleUpdated.alignment, isRightToLeft);
            attributesResolved[NSParagraphStyleAttributeName] = paragraphStyleUpdated;
        }
    }

    UIFont *font = ObjectAs(attributesResolved[NSFontAttributeName], UIFont);
    if (!font && _font) {
        font = [_font resolveFontFromTraitCollection:traitCollection];
        attributesResolved[NSFontAttributeName] = font;
    }
    [SCValdiFontAttributes applyLineHeightInAttributes:attributesResolved font:font];
    return [attributesResolved copy];
}

+ (void)applyLineHeightInAttributes:(NSMutableDictionary<NSAttributedStringKey, id> *)attributes
                                font:(UIFont *)font
{
    NSNumber *lineHeight = ObjectAs(attributes[SCValdiLineHeightAttributeName], NSNumber);
    NSNumber *lineHeightAbsolute = ObjectAs(attributes[SCValdiLineHeightAbsoluteAttributeName], NSNumber);
    if ((!lineHeight && !lineHeightAbsolute) || !font) {
        return;
    }

    NSParagraphStyle *paragraphStyle = ObjectAs(attributes[NSParagraphStyleAttributeName], NSParagraphStyle);
    NSMutableParagraphStyle *updatedParagraphStyle = paragraphStyle
        ? [paragraphStyle mutableCopy]
        : [[NSMutableParagraphStyle alloc] init];

    if (!lineHeightAbsolute) {
        // Relative lineHeight is a multiple of the font's natural line height, per the `lineHeight`
        // contract ("a value of 2 will double the height of each line") and the C++/Android layout.
        // NSParagraphStyle.lineHeightMultiple applies exactly that. It must NOT be resolved against
        // font.pointSize and clamped with minimum/maximumLineHeight: pointSize is roughly 0.8x the
        // natural line height, so any multiple below ~1.2 would clamp the line box under the glyphs
        // and clip the text (SEARCH-48847 — Search captions use 0.8-0.97 multiples).
        CGFloat multiple = lineHeight.doubleValue;
        if (multiple <= 0) {
            return;
        }
        updatedParagraphStyle.minimumLineHeight = 0;
        updatedParagraphStyle.maximumLineHeight = 0;
        updatedParagraphStyle.lineHeightMultiple = multiple;
        attributes[NSParagraphStyleAttributeName] = [updatedParagraphStyle copy];
        [attributes removeObjectForKey:NSBaselineOffsetAttributeName];
        return;
    }

    // Absolute lineHeight is an explicit point value: pin the line box to it and center the text
    // within, so a value smaller than the natural line height tightens symmetrically.
    CGFloat resolvedLineHeight = lineHeightAbsolute.doubleValue;
    if (resolvedLineHeight <= 0) {
        return;
    }
    updatedParagraphStyle.lineHeightMultiple = 0;
    updatedParagraphStyle.minimumLineHeight = resolvedLineHeight;
    updatedParagraphStyle.maximumLineHeight = resolvedLineHeight;
    attributes[NSParagraphStyleAttributeName] = [updatedParagraphStyle copy];

    CGFloat baselineOffset = (resolvedLineHeight - font.lineHeight) / 2.0;
    if (fabs(baselineOffset) > SCValdiBaselineOffsetEpsilon) {
        attributes[NSBaselineOffsetAttributeName] = @(baselineOffset);
    } else {
        [attributes removeObjectForKey:NSBaselineOffsetAttributeName];
    }
}


- (NSDictionary<NSAttributedStringKey, id> *)resolveAttributesWithIsRightToLeft:(BOOL)isRightToLeft
                                                                traitCollection:(UITraitCollection *)traitCollection
{
    @synchronized(self) {
        if (isRightToLeft) {
            if (_attributesResolvedRightToLeft == nil || traitCollection != _lastTraitCollectionRightToLeft) {
                _lastTraitCollectionRightToLeft = traitCollection;
                _attributesResolvedRightToLeft = [self buildAttributesWithIsRightToLeft:isRightToLeft
                                                                        traitCollection:traitCollection];
            }
            return _attributesResolvedRightToLeft;
        } else {
            if (_attributesResolvedLeftToRight == nil || traitCollection != _lastTraitCollectionLeftToRight) {
                _lastTraitCollectionLeftToRight = traitCollection;
                _attributesResolvedLeftToRight = [self buildAttributesWithIsRightToLeft:isRightToLeft
                                                                        traitCollection:traitCollection];
            }
            return _attributesResolvedLeftToRight;
        }
    }
}

- (NSString *)debugDescription
{
        return [NSString stringWithFormat:@"<%@ %p attributes:%@ font:%@ color:%@ textAlignment:%@ numberOfLines:%@, lineBreakMode:%@ needAttributedString:%@>",
                self.class,
                (void *)self,
                _attributesOriginal,
                self.font,
                self.color,
                @(_textAlignment),
                @(self.numberOfLines),
                @(self.lineBreakMode),
                @(self.needAttributedString)
                ];
}

@end
