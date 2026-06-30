//
//  NSAttributedString+Valdi.m
//  Valdi
//
//  Created by Nathaniel Parrott on 8/10/18.
//

#import "valdi/ios/Text/NSAttributedString+Valdi.h"

#import "valdi/ios/Text/SCValdiAttributedText.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiFont.h"

#import "valdi_core/SCNValdiCoreCompositeAttributePart.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/SCValdiUndefinedValue.h"
#import "valdi_core/SCValdiLogger.h"

static NSString *const kSCValdiFontShorthandAttribute = @"font";

static NSString *const kSCValdiColorAttribute = @"color";
static NSString *const kSCValdiTextAlignAttribute = @"textAlign";
static NSString *const kSCValdiLineHeightAttribute = @"lineHeight";
static NSString *const kSCValdiLineHeightAbsoluteAttribute = @"lineHeightAbsolute";
static NSString *const kSCValdiTextDecorationAttribute = @"textDecoration";
static NSString *const kSCValdiLetterSpacingAttribute = @"letterSpacing";
static NSString *const kSCValdiNumberOfLinesAttribute = @"numberOfLines";
static NSString *const kSCValdiTextOverflowAttribute = @"textOverflow";

/// Returns a boxed NSTextAlignment value, or nil
static NSNumber *_SCValdiParseTextAlignment(NSString *value)
{
    static NSDictionary<NSString *, NSNumber *> *textAlignmentMap;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        textAlignmentMap = @{
            @"left" : @(NSTextAlignmentLeft),
            @"center" : @(NSTextAlignmentCenter),
            @"right" : @(NSTextAlignmentRight),
            @"justified" : @(NSTextAlignmentJustified),
        };
    });
    return textAlignmentMap[value];
}

static void _SCValdiAppendTextDecoration(NSMutableDictionary<NSAttributedStringKey, id> *attributes,
                                         SCValdiTextDecoration textDecoration)
{
    switch (textDecoration) {
        case SCValdiTextDecorationUnset:
            break;
        case SCValdiTextDecorationNone:
            [attributes removeObjectForKey:NSUnderlineStyleAttributeName];
            [attributes removeObjectForKey:NSStrikethroughStyleAttributeName];
            break;
        case SCValdiTextDecorationUnderline:
            [attributes removeObjectForKey:NSStrikethroughStyleAttributeName];
            attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);
            break;
        case SCValdiTextDecorationDashedUnderline:
            [attributes removeObjectForKey:NSStrikethroughStyleAttributeName];
            attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle | NSUnderlinePatternDash);
            break;
        case SCValdiTextDecorationDottedUnderline:
            [attributes removeObjectForKey:NSStrikethroughStyleAttributeName];
            attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle | NSUnderlinePatternDot);
            break;
        case SCValdiTextDecorationStrikethrough:
            [attributes removeObjectForKey:NSUnderlineStyleAttributeName];
            attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);
            break;
    }
}

@implementation NSAttributedString (Valdi)

static SCValdiTextDecoration SCValdiTextDecorationFromString(NSString *str) {
    if (!str) {
        return SCValdiTextDecorationUnset;
    }
    if ([str isEqualToString:@"none"]) {
        return SCValdiTextDecorationNone;
    } else if ([str isEqualToString:@"underline"]) {
        return SCValdiTextDecorationUnderline;
    } else if ([str isEqualToString:@"dashed-underline"]) {
        return SCValdiTextDecorationDashedUnderline;
    } else if ([str isEqualToString:@"dotted-underline"]) {
        return SCValdiTextDecorationDottedUnderline;
    } else if ([str isEqualToString:@"strikethrough"]) {
        return SCValdiTextDecorationStrikethrough;
    } else {
        SCLogValdiError(@"Invalid TextDecoration '%@'", str);
        return SCValdiTextDecorationNone;
    }
}

+ (SCValdiFontAttributes *)defaultFontAttributes
{
    static dispatch_once_t onceToken;
    static SCValdiFontAttributes *defaultAttributes;
    dispatch_once(&onceToken, ^{
        defaultAttributes = [self fontAttributesWithCompositeValue:nil];
    });
    return defaultAttributes;
}

+ (NSParagraphStyle *)defaultParagraphStyle
{
    static dispatch_once_t onceToken;
    static NSParagraphStyle *style;
    dispatch_once(&onceToken, ^{
        /**
         UILabel fill some private properties inside the NSParagraphStyle.
         We extract the paragraphStyle from its synthesized attributedText
         so that we have the same drawing behavior between NSAttributedString
         and setting text straight on a UILabel.
         */
        UILabel *label = [UILabel new];
        label.text = @"_";
        id defaultParagraphStyle = [label.attributedText attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil];
        if ([defaultParagraphStyle isKindOfClass:[NSParagraphStyle class]]) {
            style = defaultParagraphStyle;
        } else {
            style = [NSParagraphStyle defaultParagraphStyle];
        }
    });

    return style;
}

+ (NSAttributedString *)valdi_attributedStringWithAttachment:(NSTextAttachment *)attachment
                                                  attributes:(NSMutableDictionary<NSAttributedStringKey, id> *)attributes
{
    if (@available(iOS 18.0, macOS 15.0, tvOS 18.0, watchOS 11.0, visionOS 2.0, *)) {
        return [NSAttributedString attributedStringWithAttachment:attachment attributes:attributes];
    }

    id previousAttachment = attributes[NSAttachmentAttributeName];
    attributes[NSAttachmentAttributeName] = attachment;
    unichar attachmentCharacter = NSAttachmentCharacter;
    NSString *attachmentString = [NSString stringWithCharacters:&attachmentCharacter length:1];
    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:attachmentString
                                                                           attributes:attributes];
    if (previousAttachment != nil) {
        attributes[NSAttachmentAttributeName] = previousAttachment;
    } else {
        [attributes removeObjectForKey:NSAttachmentAttributeName];
    }
    return attributedString;
}

+ (SCValdiFontAttributes *)fontAttributesWithFont:(SCValdiFont *)font
                                               color:(NSNumber *)color
                                          textAlign:(NSString *)textAlign
                                         lineHeight:(NSNumber *)lineHeight
                                lineHeightAbsolute:(NSNumber *)lineHeightAbsolute
                                      textDecoration:(NSString *)textDecoration
                                       letterSpacing:(NSNumber *)letterSpacing
                                       numberOfLines:(NSNumber *)numberOfLines
                                        textOverflow:(NSString *)textOverflow
{
    UIColor *resolvedColor;
    if (color) {
        resolvedColor = UIColorFromValdiAttributeValue(color.unsignedIntegerValue);
    } else {
        resolvedColor = [UIColor blackColor];
    }

    NSTextAlignment resolvedTextAlign = NSTextAlignmentNatural;
    if (textAlign) {
        resolvedTextAlign = (NSTextAlignment)_SCValdiParseTextAlignment(textAlign).integerValue;
    }

    NSInteger resolvedNumberOfLines = 1;
    if (numberOfLines) {
        resolvedNumberOfLines = numberOfLines.integerValue;
    }

    // Need attributed string to handle those attributes

    NSMutableDictionary<NSAttributedStringKey, id> *attributes = [NSMutableDictionary dictionary];
    attributes[NSForegroundColorAttributeName] = resolvedColor;

    if (letterSpacing) {
        attributes[NSKernAttributeName] = letterSpacing;
    }

    NSMutableParagraphStyle *paragraphStyle = [[self defaultParagraphStyle] mutableCopy];
    paragraphStyle.alignment = resolvedTextAlign;

    NSLineBreakMode resolvedLineBreakMode;
    if (resolvedNumberOfLines != 1) {
        resolvedLineBreakMode = NSLineBreakByWordWrapping;
    } else {
        resolvedLineBreakMode = NSLineBreakByTruncatingTail;
    }

    if ([textOverflow isEqualToString:@"clip"]) {
        resolvedLineBreakMode = NSLineBreakByClipping;
    }

    paragraphStyle.lineBreakMode = resolvedLineBreakMode;

    if (lineHeightAbsolute) {
        attributes[SCValdiLineHeightAbsoluteAttributeName] = lineHeightAbsolute;
    } else if (lineHeight) {
        attributes[SCValdiLineHeightAttributeName] = lineHeight;
    }

    attributes[NSParagraphStyleAttributeName] = [paragraphStyle copy];

    _SCValdiAppendTextDecoration(attributes, SCValdiTextDecorationFromString(textDecoration));

    BOOL needAttributedString = lineHeight || lineHeightAbsolute || letterSpacing || textDecoration;

    return [[SCValdiFontAttributes alloc] initWithAttributes:[attributes copy]
                                                           font:font
                                                          color:resolvedColor
                                                   textAligment:resolvedTextAlign
                                                  numberOfLines:resolvedNumberOfLines
                                                  lineBreakMode:resolvedLineBreakMode
                                           needAttributedString:needAttributedString];
}

+ (SCValdiFontAttributes *)fontAttributesWithCompositeValue:(NSArray<id> *)compositeValue
{
    NSArray<SCNValdiCoreCompositeAttributePart *> *parts = self.valdiFontAttributes;

    if (compositeValue) {
        if (![compositeValue isKindOfClass:[NSArray class]] || compositeValue.count != parts.count) {
            SCLogValdiError(@"Expected %d parts in font attribute value", (int)parts.count);
            return nil;
        }
    }

    NSNumber *color = ObjectAs(compositeValue[0], NSNumber);
    NSString *textAlign = ObjectAs(compositeValue[1], NSString);
    NSNumber *lineHeight = ObjectAs(compositeValue[2], NSNumber);
    NSNumber *lineHeightAbsolute = ObjectAs(compositeValue[3], NSNumber);
    NSString *textDecoration = ObjectAs(compositeValue[4], NSString);
    SCValdiFont *font = ObjectAs(compositeValue[5], SCValdiFont);
    NSNumber *letterSpacing = ObjectAs(compositeValue[6], NSNumber);
    NSNumber *numberOfLines = ObjectAs(compositeValue[7], NSNumber);
    NSString *textOverflow = ObjectAs(compositeValue[8], NSString);

    return [self fontAttributesWithFont:font
                                  color:color
                             textAlign:textAlign
                             lineHeight:lineHeight
                    lineHeightAbsolute:lineHeightAbsolute
                         textDecoration:textDecoration
                          letterSpacing:letterSpacing
                          numberOfLines:numberOfLines
                           textOverflow:textOverflow];
}

+ (SCValdiFontAttributes *)fontAttributesWithCompositeValueGrowable:(NSArray<id> *)compositeValue
{
    NSArray<SCNValdiCoreCompositeAttributePart *> *parts = self.valdiFontAttributesGrowable;

    if (compositeValue) {
        if (![compositeValue isKindOfClass:[NSArray class]] || compositeValue.count != parts.count) {
            SCLogValdiError(@"Expected %d parts in font attribute value", (int)parts.count);
            return nil;
        }
    }

    NSNumber *color = ObjectAs(compositeValue[0], NSNumber);
    NSString *textAlign = ObjectAs(compositeValue[1], NSString);
    NSNumber *lineHeight = ObjectAs(compositeValue[2], NSNumber);
    NSNumber *lineHeightAbsolute = ObjectAs(compositeValue[3], NSNumber);
    NSString *textDecoration = ObjectAs(compositeValue[4], NSString);
    SCValdiFont *font = ObjectAs(compositeValue[5], SCValdiFont);
    NSNumber *letterSpacing = ObjectAs(compositeValue[6], NSNumber);
    NSNumber *numberOfLines = ObjectAs(compositeValue[7], NSNumber);

    // Force textOverflow to allow for word wrapping + growing lines.
    return [self fontAttributesWithFont:font
                                  color:color
                             textAlign:textAlign
                             lineHeight:lineHeight
                    lineHeightAbsolute:lineHeightAbsolute
                         textDecoration:textDecoration
                          letterSpacing:letterSpacing
                          numberOfLines:numberOfLines ?: @0
                           textOverflow:nil];
}

+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)valdiFontAttributes
{
    static NSArray<SCNValdiCoreCompositeAttributePart *> *attributedTextSubAttrs;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        NSMutableArray<SCNValdiCoreCompositeAttributePart *> *mutableAttributedTextSubAttrs = [NSMutableArray arrayWithArray:[self valdiFontAttributesGrowable]];
        [mutableAttributedTextSubAttrs addObject:[[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiNumberOfLinesAttribute
                                                                                                         type:SCNValdiCoreAttributeTypeInt
                                                                                                     optional:YES
                                                                                         invalidateLayoutOnChange:YES]];
        [mutableAttributedTextSubAttrs addObject:[[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiTextOverflowAttribute
                                                                                                         type:SCNValdiCoreAttributeTypeString
                                                                                                     optional:YES
                                                                                         invalidateLayoutOnChange:YES]];
        attributedTextSubAttrs = [NSArray arrayWithArray:mutableAttributedTextSubAttrs];
    });

    return attributedTextSubAttrs;
}


+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)valdiFontAttributesGrowable
{
    static NSArray<SCNValdiCoreCompositeAttributePart *> *attributedTextSubAttrs;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        attributedTextSubAttrs = @[
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiColorAttribute
                                                                 type:SCNValdiCoreAttributeTypeColor
                                                             optional:YES
                                             invalidateLayoutOnChange:NO],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiTextAlignAttribute
                                                                 type:SCNValdiCoreAttributeTypeString
                                                             optional:YES
                                             invalidateLayoutOnChange:NO],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiLineHeightAttribute
                                                                 type:SCNValdiCoreAttributeTypeDouble
                                                             optional:YES
                                             invalidateLayoutOnChange:YES],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiLineHeightAbsoluteAttribute
                                                                 type:SCNValdiCoreAttributeTypeDouble
                                                             optional:YES
                                             invalidateLayoutOnChange:YES],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiTextDecorationAttribute
                                                                 type:SCNValdiCoreAttributeTypeString
                                                             optional:YES
                                             invalidateLayoutOnChange:NO],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiFontShorthandAttribute
                                                                 type:SCNValdiCoreAttributeTypeString
                                                             optional:YES
                                             invalidateLayoutOnChange:YES],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiLetterSpacingAttribute
                                                                 type:SCNValdiCoreAttributeTypeDouble
                                                             optional:YES
                                             invalidateLayoutOnChange:YES],
            [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kSCValdiNumberOfLinesAttribute
                                                                 type:SCNValdiCoreAttributeTypeInt
                                                             optional:YES
                                             invalidateLayoutOnChange:YES]
        ];
    });
    return attributedTextSubAttrs;
}



+ (NSAttributedString*)trimAttributedString:(NSAttributedString*)attributedString
                            characterLimit:(NSInteger)characterLimit
{
    if (attributedString.length <= characterLimit) {
        return attributedString;
    }
    NSAttributedString *trimmedString = [attributedString attributedSubstringFromRange:NSMakeRange(0, characterLimit)];
    return trimmedString;
}

@end
