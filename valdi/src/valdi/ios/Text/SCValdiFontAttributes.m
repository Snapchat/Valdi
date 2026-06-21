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
NSAttributedStringKey const SCValdiLineHeightMultipleAttributeName = @"valdi_lineHeightMultiple";
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
    NSNumber *lineHeightValue = ObjectAs(attributes[SCValdiLineHeightAttributeName], NSNumber);
    NSNumber *lineHeightMultiple = ObjectAs(attributes[SCValdiLineHeightMultipleAttributeName], NSNumber);
    if ((!lineHeightValue && !lineHeightMultiple) || !font) {
        return;
    }

    CGFloat lineHeight = lineHeightValue ? lineHeightValue.doubleValue : font.pointSize * lineHeightMultiple.doubleValue;
    if (lineHeight <= 0) {
        return;
    }

    NSParagraphStyle *paragraphStyle = ObjectAs(attributes[NSParagraphStyleAttributeName], NSParagraphStyle);
    NSMutableParagraphStyle *updatedParagraphStyle = paragraphStyle
        ? [paragraphStyle mutableCopy]
        : [[NSMutableParagraphStyle alloc] init];
    updatedParagraphStyle.lineHeightMultiple = 0;
    updatedParagraphStyle.minimumLineHeight = lineHeight;
    updatedParagraphStyle.maximumLineHeight = lineHeight;
    attributes[NSParagraphStyleAttributeName] = [updatedParagraphStyle copy];

    CGFloat baselineOffset;
    if (lineHeightValue || lineHeight >= font.lineHeight) {
        baselineOffset = (lineHeight - font.lineHeight) / 2.0;
    } else {
        baselineOffset = lineHeight - font.lineHeight;
    }
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
