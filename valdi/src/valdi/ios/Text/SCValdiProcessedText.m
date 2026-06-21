//
//  SCValdiProcessedText.m
//  valdi-ios
//

#import "valdi/ios/Text/SCValdiProcessedText.h"

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiAttributedText.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/SCValdiFont.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiImageAttachmentInfo.h"
#import "valdi/ios/Text/SCValdiInlineViewAttachmentInfo.h"
#import "valdi/ios/Text/SCValdiTextAnimationTransform.h"

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiWrappedValue.h"
#import "valdi_core/UIColor+Valdi.h"

/**
 * File-private range/value pair used by SCValdiProcessedText side tables.
 */
@interface SCValdiProcessedTextRangeValue : NSObject
@property (nonatomic, assign) NSRange range;
@property (nonatomic, strong) id value;
@end

@implementation SCValdiProcessedTextRangeValue
@end

/**
 * File-private draw-on-top outline record for text effects that are rendered
 * outside normal NSAttributedString drawing.
 */
@interface SCValdiProcessedTextOuterOutline : NSObject
@property (nonatomic, assign) NSRange range;
@property (nonatomic, strong) UIColor *color;
@property (nonatomic, assign) CGFloat width;
@end

@implementation SCValdiProcessedTextOuterOutline
@end

/**
 * Maps source ranges from the parsed Valdi attributed text to destination ranges
 * after processed-text transformations such as character clamping.
 */
@interface SCValdiProcessedTextRangeMapping : NSObject
@property (nonatomic, assign) NSRange sourceRange;
@property (nonatomic, assign) NSRange destinationRange;
@end

@implementation SCValdiProcessedTextRangeMapping
@end

@implementation SCValdiProcessedTextConfiguration
@end

static const NSUInteger SCValdiProcessedTextMaxInlineAttachmentChildCount = 64 * 1024;

@interface SCValdiProcessedText ()

+ (SCValdiProcessedText *)processedTextWithString:(NSMutableAttributedString *)string
                                      onTapItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onTapItems
                                   onLayoutItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onLayoutItems
                            inlineAttachmentItems:(NSArray<SCValdiProcessedTextRangeValue *> *)inlineAttachmentItems
                                   animationItems:(NSArray<SCValdiProcessedTextRangeValue *> *)animationItems
                                    outlineItems:(NSArray<SCValdiProcessedTextOuterOutline *> *)outlineItems
                                   configuration:(SCValdiProcessedTextConfiguration *)configuration;

- (instancetype)initWithString:(NSMutableAttributedString *)string
                    onTapItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onTapItems
                 onLayoutItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onLayoutItems
          inlineAttachmentItems:(NSArray<SCValdiProcessedTextRangeValue *> *)inlineAttachmentItems
                 animationItems:(NSArray<SCValdiProcessedTextRangeValue *> *)animationItems
                   outlineItems:(NSArray<SCValdiProcessedTextOuterOutline *> *)outlineItems
    customUnderlineSourceString:(NSAttributedString *)customUnderlineSourceString
 customUnderlineCharacterRanges:(NSArray<NSValue *> *)customUnderlineCharacterRanges NS_DESIGNATED_INITIALIZER;

@end

static void SCValdiAppendTextDecoration(NSMutableDictionary<NSAttributedStringKey, id> *attributes,
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

static CGFloat SCValdiProcessedTextAttachmentBaselineOffset(UIFont *font,
                                                            CGFloat attachmentHeight,
                                                            SCValdiInlineViewVerticalAlignment verticalAlignment)
{
    switch (verticalAlignment) {
        case SCValdiInlineViewVerticalAlignmentTop:
            return font.ascender - attachmentHeight;
        case SCValdiInlineViewVerticalAlignmentBottom:
            return font.descender;
        case SCValdiInlineViewVerticalAlignmentBaseline:
            return 0;
        case SCValdiInlineViewVerticalAlignmentCenter:
            return (font.ascender + font.descender - attachmentHeight) / 2.0;
    }
    return (font.ascender + font.descender - attachmentHeight) / 2.0;
}

static CGFloat SCValdiProcessedTextAttachmentYOffset(CGFloat containerHeight,
                                                     CGFloat attachmentHeight,
                                                     SCValdiInlineViewVerticalAlignment verticalAlignment)
{
    switch (verticalAlignment) {
        case SCValdiInlineViewVerticalAlignmentTop:
            return 0;
        case SCValdiInlineViewVerticalAlignmentBottom:
            return containerHeight - attachmentHeight;
        case SCValdiInlineViewVerticalAlignmentBaseline:
            return containerHeight - attachmentHeight;
        case SCValdiInlineViewVerticalAlignmentCenter:
            return (containerHeight - attachmentHeight) / 2.0;
    }
    return (containerHeight - attachmentHeight) / 2.0;
}

static SCValdiProcessedTextRangeValue *SCValdiProcessedTextMakeRangeValue(NSRange range, id value)
{
    SCValdiProcessedTextRangeValue *item = [SCValdiProcessedTextRangeValue new];
    item.range = range;
    item.value = value;
    return item;
}

static BOOL SCValdiProcessedTextRangeContainsIndex(NSRange range, NSUInteger index)
{
    return range.location != NSNotFound && index >= range.location && index < NSMaxRange(range);
}

static void SCValdiProcessedTextAppendMapping(NSMutableArray<SCValdiProcessedTextRangeMapping *> *mappings,
                                              NSRange sourceRange,
                                              NSUInteger destinationLocation)
{
    if (sourceRange.length == 0) {
        return;
    }

    SCValdiProcessedTextRangeMapping *previousMapping = mappings.lastObject;
    if (previousMapping != nil &&
        NSMaxRange(previousMapping.sourceRange) == sourceRange.location &&
        NSMaxRange(previousMapping.destinationRange) == destinationLocation) {
        previousMapping.sourceRange = NSMakeRange(previousMapping.sourceRange.location,
                                                  previousMapping.sourceRange.length + sourceRange.length);
        previousMapping.destinationRange = NSMakeRange(previousMapping.destinationRange.location,
                                                       previousMapping.destinationRange.length + sourceRange.length);
        return;
    }

    SCValdiProcessedTextRangeMapping *mapping = [SCValdiProcessedTextRangeMapping new];
    mapping.sourceRange = sourceRange;
    mapping.destinationRange = NSMakeRange(destinationLocation, sourceRange.length);
    [mappings addObject:mapping];
}

static BOOL SCValdiProcessedTextMapRange(NSRange range,
                                         NSArray<SCValdiProcessedTextRangeMapping *> *mappings,
                                         NSRange *mappedRange)
{
    NSUInteger mappedLocation = NSNotFound;
    NSUInteger mappedEnd = 0;
    for (SCValdiProcessedTextRangeMapping *mapping in mappings) {
        NSRange intersection = NSIntersectionRange(range, mapping.sourceRange);
        if (intersection.length == 0) {
            continue;
        }

        NSUInteger destinationLocation = mapping.destinationRange.location + intersection.location - mapping.sourceRange.location;
        if (mappedLocation == NSNotFound) {
            mappedLocation = destinationLocation;
        }
        mappedEnd = destinationLocation + intersection.length;
    }

    if (mappedLocation == NSNotFound) {
        return NO;
    }

    *mappedRange = NSMakeRange(mappedLocation, mappedEnd - mappedLocation);
    return YES;
}

static void SCValdiProcessedTextDeleteCharactersOutsideMappings(
    NSMutableAttributedString *attributedString,
    NSArray<SCValdiProcessedTextRangeMapping *> *mappings)
{
    NSUInteger deleteEnd = attributedString.length;
    for (NSInteger i = (NSInteger)mappings.count - 1; i >= 0; i--) {
        SCValdiProcessedTextRangeMapping *mapping = mappings[(NSUInteger)i];
        NSUInteger keepEnd = NSMaxRange(mapping.sourceRange);
        if (deleteEnd > keepEnd) {
            [attributedString deleteCharactersInRange:NSMakeRange(keepEnd, deleteEnd - keepEnd)];
        }
        deleteEnd = mapping.sourceRange.location;
    }
    if (deleteEnd > 0) {
        [attributedString deleteCharactersInRange:NSMakeRange(0, deleteEnd)];
    }
}

static void SCValdiProcessedTextApplyMappingsToRangeValues(
    NSMutableArray<SCValdiProcessedTextRangeValue *> *items,
    NSArray<SCValdiProcessedTextRangeMapping *> *mappings)
{
    for (NSInteger i = (NSInteger)items.count - 1; i >= 0; i--) {
        SCValdiProcessedTextRangeValue *item = items[(NSUInteger)i];
        NSRange mappedRange;
        if (!SCValdiProcessedTextMapRange(item.range, mappings, &mappedRange)) {
            [items removeObjectAtIndex:(NSUInteger)i];
            continue;
        }
        item.range = mappedRange;
    }
}

static NSArray<NSValue *> *SCValdiProcessedTextRangesByApplyingMappings(
    NSArray<NSValue *> *ranges,
    NSArray<SCValdiProcessedTextRangeMapping *> *mappings)
{
    NSMutableArray<NSValue *> *mappedRanges = nil;
    for (NSValue *rangeValue in ranges) {
        NSRange mappedRange;
        if (!SCValdiProcessedTextMapRange(rangeValue.rangeValue, mappings, &mappedRange)) {
            continue;
        }
        if (mappedRanges == nil) {
            mappedRanges = [NSMutableArray new];
        }
        [mappedRanges addObject:[NSValue valueWithRange:mappedRange]];
    }
    return mappedRanges;
}

static void SCValdiProcessedTextApplyMappingsToOutlines(
    NSMutableArray<SCValdiProcessedTextOuterOutline *> *items,
    NSArray<SCValdiProcessedTextRangeMapping *> *mappings)
{
    for (NSInteger i = (NSInteger)items.count - 1; i >= 0; i--) {
        SCValdiProcessedTextOuterOutline *item = items[(NSUInteger)i];
        NSRange mappedRange;
        if (!SCValdiProcessedTextMapRange(item.range, mappings, &mappedRange)) {
            [items removeObjectAtIndex:(NSUInteger)i];
            continue;
        }
        item.range = mappedRange;
    }
}

static NSArray<NSValue *> *SCValdiProcessedTextReplaceNativeUnderlinesWithColorAttribute(
    NSMutableAttributedString *attributedString,
    NSAttributedStringKey colorAttributeName,
    UIColor *fallbackColor)
{
    __block NSMutableArray<NSValue *> *ranges = nil;
    __block NSMutableArray<UIColor *> *colors = nil;
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
            colors = [NSMutableArray new];
        }
        [ranges addObject:[NSValue valueWithRange:range]];
        [colors addObject:SCValdiCustomUnderlineColorForRange(attributedString, range, fallbackColor)];
    }];

    NSUInteger rangesCount = ranges.count;
    for (NSUInteger i = 0; i < rangesCount; i++) {
        NSRange range = ranges[i].rangeValue;
        [attributedString removeAttribute:NSUnderlineStyleAttributeName range:range];
        [attributedString removeAttribute:NSUnderlineColorAttributeName range:range];
        [attributedString addAttribute:colorAttributeName value:colors[i] range:range];
    }
    return ranges;
}

static void SCValdiProcessedTextApplyConfiguration(
    NSMutableAttributedString *attributedString,
    SCValdiProcessedTextConfiguration *configuration,
    NSAttributedString **customUnderlineSourceString,
    NSArray<NSValue *> **customUnderlineCharacterRanges)
{
    *customUnderlineSourceString = nil;
    *customUnderlineCharacterRanges = nil;

    if (attributedString.length == 0 || configuration == nil) {
        return;
    }

    UIColor *foregroundColorOverride = configuration.foregroundColorOverride;
    if (foregroundColorOverride != nil) {
        [attributedString addAttribute:NSForegroundColorAttributeName
                                 value:foregroundColorOverride
                                 range:NSMakeRange(0, attributedString.length)];
    }

    if (configuration.customUnderlineStyle == nil ||
        configuration.customUnderlineMode == SCValdiProcessedTextCustomUnderlineModeNone) {
        return;
    }

    switch (configuration.customUnderlineMode) {
        case SCValdiProcessedTextCustomUnderlineModeNone:
            break;
        case SCValdiProcessedTextCustomUnderlineModeRemoveNativeUnderline: {
            NSArray<NSValue *> *ranges = SCValdiCustomUnderlineRemoveNativeUnderlines(attributedString, NO);
            if (ranges.count > 0) {
                *customUnderlineSourceString = attributedString;
                *customUnderlineCharacterRanges = ranges;
            }
            break;
        }
        case SCValdiProcessedTextCustomUnderlineModeReplaceNativeUnderlineWithColorAttribute: {
            NSAttributedStringKey colorAttributeName = configuration.customUnderlineColorAttributeName;
            if (colorAttributeName == nil) {
                break;
            }
            UIColor *fallbackColor = configuration.customUnderlineFallbackColor ?: [UIColor blackColor];
            NSArray<NSValue *> *ranges =
                SCValdiProcessedTextReplaceNativeUnderlinesWithColorAttribute(attributedString,
                                                                              colorAttributeName,
                                                                              fallbackColor);
            *customUnderlineCharacterRanges = ranges;
            break;
        }
    }
}

@implementation SCValdiProcessedText {
    NSMutableArray<SCValdiProcessedTextRangeValue *> *_onTapItems;
    NSMutableArray<SCValdiProcessedTextRangeValue *> *_onLayoutItems;
    NSMutableArray<SCValdiProcessedTextRangeValue *> *_inlineAttachmentItems;
    NSMutableArray<SCValdiProcessedTextRangeValue *> *_animationItems;
    NSMutableArray<SCValdiProcessedTextOuterOutline *> *_outlineItems;
    NSMutableArray<id> *_inlineAttachmentItemsOrderedByChildIndex;
}

+ (SCValdiProcessedText *)processedTextWithString:(NSMutableAttributedString *)string
                                      onTapItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onTapItems
                                   onLayoutItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onLayoutItems
                            inlineAttachmentItems:(NSArray<SCValdiProcessedTextRangeValue *> *)inlineAttachmentItems
                                   animationItems:(NSArray<SCValdiProcessedTextRangeValue *> *)animationItems
                                    outlineItems:(NSArray<SCValdiProcessedTextOuterOutline *> *)outlineItems
                                   configuration:(SCValdiProcessedTextConfiguration *)configuration
{
    NSAttributedString *customUnderlineSourceString = nil;
    NSArray<NSValue *> *customUnderlineCharacterRanges = nil;
    SCValdiProcessedTextApplyConfiguration(string,
                                           configuration,
                                           &customUnderlineSourceString,
                                           &customUnderlineCharacterRanges);
    return [[SCValdiProcessedText alloc] initWithString:string
                                             onTapItems:onTapItems
                                          onLayoutItems:onLayoutItems
                                   inlineAttachmentItems:inlineAttachmentItems
                                          animationItems:animationItems
                                           outlineItems:outlineItems
                            customUnderlineSourceString:customUnderlineSourceString
                         customUnderlineCharacterRanges:customUnderlineCharacterRanges];
}

+ (SCValdiProcessedText *)processedTextWithAttributeValue:(id)attributeValue
                                               attributes:(NSDictionary<NSAttributedStringKey, id> *)attributes
                                            isRightToLeft:(BOOL)isRightToLeft
                                              fontManager:(nullable id<SCValdiFontManagerProtocol>)fontManager
                                          traitCollection:(UITraitCollection *)traitCollection
                                            configuration:(SCValdiProcessedTextConfiguration *)configuration
{
    if (attributeValue == nil) {
        return [self processedTextWithString:[NSMutableAttributedString new]
                                  onTapItems:nil
                               onLayoutItems:nil
                        inlineAttachmentItems:nil
                               animationItems:nil
                                outlineItems:nil
                               configuration:configuration];
    }

    if ([attributeValue isKindOfClass:[NSAttributedString class]]) {
        return [self processedTextWithString:[attributeValue mutableCopy]
                                  onTapItems:nil
                               onLayoutItems:nil
                        inlineAttachmentItems:nil
                               animationItems:nil
                                outlineItems:nil
                               configuration:configuration];
    }

    if ([attributeValue isKindOfClass:[NSString class]]) {
        NSMutableAttributedString *string =
            [[NSMutableAttributedString alloc] initWithString:attributeValue attributes:attributes];
        return [self processedTextWithString:string
                                  onTapItems:nil
                               onLayoutItems:nil
                        inlineAttachmentItems:nil
                               animationItems:nil
                                outlineItems:nil
                               configuration:configuration];
    }

    SCValdiAttributedText *valdiAttributedText = nil;
    if ([attributeValue isKindOfClass:[SCValdiWrappedValue class]]) {
        valdiAttributedText = [[SCValdiAttributedText alloc] initWithWrappedValue:attributeValue];
    } else if ([attributeValue isKindOfClass:[SCValdiAttributedText class]]) {
        valdiAttributedText = attributeValue;
    }
    if (valdiAttributedText == nil) {
        return [self processedTextWithString:[NSMutableAttributedString new]
                                  onTapItems:nil
                               onLayoutItems:nil
                        inlineAttachmentItems:nil
                               animationItems:nil
                                outlineItems:nil
                               configuration:configuration];
    }

    NSMutableAttributedString *resultString = [NSMutableAttributedString new];
    NSMutableArray<SCValdiProcessedTextRangeValue *> *onTapItems = nil;
    NSMutableArray<SCValdiProcessedTextRangeValue *> *onLayoutItems = nil;
    NSMutableArray<SCValdiProcessedTextRangeValue *> *inlineAttachmentItems = nil;
    NSMutableArray<SCValdiProcessedTextRangeValue *> *animationItems = nil;
    NSMutableArray<SCValdiProcessedTextOuterOutline *> *outlineItems = nil;

    NSUInteger count = valdiAttributedText.partsCount;
    for (NSUInteger i = 0; i < count; i++) {
        NSMutableDictionary<NSAttributedStringKey, id> *nsAttrs = [attributes mutableCopy] ?: [NSMutableDictionary new];

        NSString *content = [valdiAttributedText contentAtIndex:i];
        NSString *fontString = [valdiAttributedText fontAtIndex:i];
        UIColor *color = [valdiAttributedText colorAtIndex:i];
        UIColor *backgroundColor = [valdiAttributedText backgroundColorAtIndex:i];
        SCValdiTextDecoration textDecoration = [valdiAttributedText textDecorationAtIndex:i];
        UIColor *outlineColor = [valdiAttributedText outlineColorAtIndex:i];
        NSNumber *outlineWidth = [valdiAttributedText outlineWidthAtIndex:i];
        UIColor *outerOutlineColor = [valdiAttributedText outerOutlineColorAtIndex:i];
        NSNumber *outerOutlineWidth = [valdiAttributedText outerOutlineWidthAtIndex:i];
        SCValdiImageAttachmentInfo *imageAttachment = [valdiAttributedText imageAttachmentAtIndex:i];
        SCValdiInlineViewAttachmentInfo *inlineViewAttachment = [valdiAttributedText inlineViewAttachmentAtIndex:i];
        SCValdiTextAnimationTransform *animationTransform = [valdiAttributedText animationTransformAtIndex:i];

        if (color) {
            nsAttrs[NSForegroundColorAttributeName] = color;
        }
        if (backgroundColor) {
            nsAttrs[NSBackgroundColorAttributeName] = backgroundColor;
        }
        if (outlineWidth && outlineColor) {
            nsAttrs[NSStrokeColorAttributeName] = outlineColor;
            nsAttrs[NSStrokeWidthAttributeName] = @(-outlineWidth.floatValue);
        }
        if (fontString) {
            SCValdiFont *font = [SCValdiFont fontFromValdiAttribute:fontString fontManager:fontManager];
            nsAttrs[NSFontAttributeName] = [font resolveFontFromTraitCollection:traitCollection];
        }

        [SCValdiFontAttributes applyLineHeightInAttributes:nsAttrs font:ObjectAs(nsAttrs[NSFontAttributeName], UIFont)];
        SCValdiAppendTextDecoration(nsAttrs, textDecoration);

        NSUInteger partStart = resultString.length;
        NSAttributedString *partString = nil;
        if (imageAttachment) {
            NSTextAttachment *attachment = [[NSTextAttachment alloc] init];
            if (imageAttachment.imageData.length > 0) {
                attachment.image = [UIImage imageWithData:imageAttachment.imageData scale:UIScreen.mainScreen.scale];
            } else {
                attachment.image = [[UIImage alloc] init];
            }
            UIFont *font = nsAttrs[NSFontAttributeName];
            CGFloat baselineOffset = 0;
            if (font) {
                baselineOffset = SCValdiProcessedTextAttachmentBaselineOffset(
                    font,
                    imageAttachment.height,
                    SCValdiInlineViewVerticalAlignmentCenter);
            }
            attachment.bounds = CGRectMake(0, baselineOffset, imageAttachment.width, imageAttachment.height);
            partString = [NSAttributedString attributedStringWithAttachment:attachment];
            [resultString appendAttributedString:partString];
            [resultString appendAttributedString:[[NSAttributedString alloc] initWithString:@"\u2009" attributes:nsAttrs]];
        } else if (inlineViewAttachment) {
            NSTextAttachment *attachment = [[NSTextAttachment alloc] init];
            attachment.image = [[UIImage alloc] init];
            attachment.bounds = CGRectMake(0, 0, inlineViewAttachment.size.width, inlineViewAttachment.size.height);
            partString = [NSAttributedString valdi_attributedStringWithAttachment:attachment attributes:nsAttrs];
            [resultString appendAttributedString:partString];
            [resultString appendAttributedString:[[NSAttributedString alloc] initWithString:@"\u2009" attributes:nsAttrs]];
        } else {
            partString = [[NSAttributedString alloc] initWithString:content attributes:nsAttrs];
            [resultString appendAttributedString:partString];
        }

        NSRange partRange = NSMakeRange(partStart, partString.length);
        if (partRange.length == 0) {
            continue;
        }

        id<SCValdiFunction> onTapCallback = [valdiAttributedText onTapAtIndex:i];
        if (onTapCallback) {
            if (onTapItems == nil) {
                onTapItems = [NSMutableArray new];
            }
            [onTapItems addObject:SCValdiProcessedTextMakeRangeValue(partRange, onTapCallback)];
        }

        id<SCValdiFunction> onLayoutCallback = [valdiAttributedText onLayoutAtIndex:i];
        if (onLayoutCallback) {
            if (onLayoutItems == nil) {
                onLayoutItems = [NSMutableArray new];
            }
            [onLayoutItems addObject:SCValdiProcessedTextMakeRangeValue(partRange, onLayoutCallback)];
        }

        if (inlineViewAttachment) {
            if (inlineAttachmentItems == nil) {
                inlineAttachmentItems = [NSMutableArray new];
            }
            [inlineAttachmentItems addObject:SCValdiProcessedTextMakeRangeValue(partRange, inlineViewAttachment)];
        }

        if (animationTransform) {
            if (animationItems == nil) {
                animationItems = [NSMutableArray new];
            }
            [animationItems addObject:SCValdiProcessedTextMakeRangeValue(partRange, animationTransform)];
        }

        if (outerOutlineWidth && outerOutlineColor) {
            if (outlineItems == nil) {
                outlineItems = [NSMutableArray new];
            }
            SCValdiProcessedTextOuterOutline *outline = [SCValdiProcessedTextOuterOutline new];
            outline.range = partRange;
            outline.color = outerOutlineColor;
            outline.width = outerOutlineWidth.floatValue;
            [outlineItems addObject:outline];
        }
    }

    return [self processedTextWithString:resultString
                              onTapItems:onTapItems
                           onLayoutItems:onLayoutItems
                    inlineAttachmentItems:inlineAttachmentItems
                           animationItems:animationItems
                            outlineItems:outlineItems
                           configuration:configuration];
}

- (instancetype)initWithString:(NSMutableAttributedString *)string
                    onTapItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onTapItems
                 onLayoutItems:(NSArray<SCValdiProcessedTextRangeValue *> *)onLayoutItems
          inlineAttachmentItems:(NSArray<SCValdiProcessedTextRangeValue *> *)inlineAttachmentItems
                 animationItems:(NSArray<SCValdiProcessedTextRangeValue *> *)animationItems
                   outlineItems:(NSArray<SCValdiProcessedTextOuterOutline *> *)outlineItems
     customUnderlineSourceString:(NSAttributedString *)customUnderlineSourceString
  customUnderlineCharacterRanges:(NSArray<NSValue *> *)customUnderlineCharacterRanges
{
    self = [super init];
    if (self) {
        _attributedString = string;
        _onTapItems = [onTapItems mutableCopy];
        _onLayoutItems = [onLayoutItems mutableCopy];
        _inlineAttachmentItems = [inlineAttachmentItems mutableCopy];
        _animationItems = [animationItems mutableCopy];
        _outlineItems = [outlineItems mutableCopy];
        _customUnderlineSourceString = customUnderlineSourceString;
        _customUnderlineCharacterRanges = [customUnderlineCharacterRanges copy];
        [self _updateInlineAttachmentItemsOrderedByChildIndex];
    }
    return self;
}

- (void)_updateInlineAttachmentItemsOrderedByChildIndex
{
    NSMutableArray<id> *inlineAttachmentItemsOrderedByChildIndex = nil;
    for (SCValdiProcessedTextRangeValue *item in _inlineAttachmentItems) {
        SCValdiInlineViewAttachmentInfo *attachmentInfo = ObjectAs(item.value, SCValdiInlineViewAttachmentInfo);
        if (attachmentInfo == nil) {
            continue;
        }
        NSInteger childIndex = attachmentInfo.childIndex;
        if (childIndex < 0) {
            continue;
        }
        NSUInteger childIndexValue = (NSUInteger)childIndex;
        if (childIndexValue >= SCValdiProcessedTextMaxInlineAttachmentChildCount) {
            [NSException raise:NSInvalidArgumentException
                        format:@"Inline view attachment child index %@ exceeds the maximum supported child count %@",
                               @(childIndex),
                               @(SCValdiProcessedTextMaxInlineAttachmentChildCount)];
        }
        if (inlineAttachmentItemsOrderedByChildIndex == nil) {
            inlineAttachmentItemsOrderedByChildIndex = [NSMutableArray new];
        }
        while (inlineAttachmentItemsOrderedByChildIndex.count <= childIndexValue) {
            [inlineAttachmentItemsOrderedByChildIndex addObject:NSNull.null];
        }
        inlineAttachmentItemsOrderedByChildIndex[childIndexValue] = item;
    }
    _inlineAttachmentItemsOrderedByChildIndex = inlineAttachmentItemsOrderedByChildIndex;
}

- (BOOL)hasAnimationTransform
{
    return _animationItems.count > 0;
}

- (BOOL)hasInlineViewAttachment
{
    return _inlineAttachmentItems.count > 0;
}

- (BOOL)hasInlineViewAttachmentForIndex:(NSUInteger)childIndex
{
    return [self inlineViewAttachmentForViewIndex:childIndex] != nil;
}

- (BOOL)hasOnTap
{
    return _onTapItems.count > 0;
}

- (BOOL)hasOnLayout
{
    return _onLayoutItems.count > 0;
}

- (BOOL)hasOuterOutline
{
    return _outlineItems.count > 0;
}

- (BOOL)hasCustomUnderline
{
    return _customUnderlineCharacterRanges.count > 0;
}

- (NSUInteger)animationTransformsCount
{
    return _animationItems.count;
}

- (id<SCValdiFunction>)onTapAtIndex:(NSUInteger)index effectiveRange:(NSRangePointer)range
{
    for (SCValdiProcessedTextRangeValue *item in _onTapItems) {
        if (SCValdiProcessedTextRangeContainsIndex(item.range, index)) {
            if (range != NULL) {
                *range = item.range;
            }
            return item.value;
        }
    }
    return nil;
}

- (id<SCValdiFunction>)onLayoutAtIndex:(NSUInteger)index effectiveRange:(NSRangePointer)range
{
    for (SCValdiProcessedTextRangeValue *item in _onLayoutItems) {
        if (SCValdiProcessedTextRangeContainsIndex(item.range, index)) {
            if (range != NULL) {
                *range = item.range;
            }
            return item.value;
        }
    }
    return nil;
}

- (SCValdiInlineViewAttachmentInfo *)inlineViewAttachmentAtIndex:(NSUInteger)index effectiveRange:(NSRangePointer)range
{
    for (SCValdiProcessedTextRangeValue *item in _inlineAttachmentItems) {
        if (SCValdiProcessedTextRangeContainsIndex(item.range, index)) {
            if (range != NULL) {
                *range = item.range;
            }
            return item.value;
        }
    }
    return nil;
}

- (SCValdiInlineViewAttachmentInfo *)inlineViewAttachmentForViewIndex:(NSUInteger)childIndex
{
    if (childIndex >= _inlineAttachmentItemsOrderedByChildIndex.count) {
        return nil;
    }

    SCValdiProcessedTextRangeValue *item =
        ObjectAs(_inlineAttachmentItemsOrderedByChildIndex[childIndex], SCValdiProcessedTextRangeValue);
    return ObjectAs(item.value, SCValdiInlineViewAttachmentInfo);
}

- (CGRect)rectForInlineViewAttachment:(SCValdiInlineViewAttachmentInfo *)inlineViewAttachment
                         layoutManager:(NSLayoutManager *)layoutManager
                         textContainer:(NSTextContainer *)textContainer
{
    if (inlineViewAttachment == nil || layoutManager == nil || textContainer == nil) {
        return CGRectNull;
    }

    NSInteger childIndex = inlineViewAttachment.childIndex;
    if (childIndex < 0 || (NSUInteger)childIndex >= _inlineAttachmentItemsOrderedByChildIndex.count) {
        return CGRectNull;
    }

    id itemObject = _inlineAttachmentItemsOrderedByChildIndex[(NSUInteger)childIndex];
    SCValdiProcessedTextRangeValue *item = ObjectAs(itemObject, SCValdiProcessedTextRangeValue);
    if (item == nil) {
        return CGRectNull;
    }

    NSRange range = item.range;
    if (range.length == 0) {
        return CGRectNull;
    }

    [layoutManager ensureLayoutForTextContainer:textContainer];
    NSRange glyphRange = [layoutManager glyphRangeForCharacterRange:range actualCharacterRange:nil];
    if (glyphRange.location == NSNotFound || glyphRange.length == 0) {
        return CGRectNull;
    }

    CGSize attachmentSize = inlineViewAttachment.size;
    CGRect attachmentRect = [layoutManager boundingRectForGlyphRange:glyphRange inTextContainer:textContainer];
    if (CGRectIsEmpty(attachmentRect)) {
        attachmentRect.size = attachmentSize;
        return attachmentRect;
    }

    if (attachmentSize.width > 0 && attachmentRect.size.width != attachmentSize.width) {
        attachmentRect.size.width = attachmentSize.width;
    }
    if (attachmentSize.height > 0 && attachmentRect.size.height != attachmentSize.height) {
        CGRect lineRect = [layoutManager lineFragmentRectForGlyphAtIndex:glyphRange.location effectiveRange:nil];
        if (inlineViewAttachment.verticalAlignment == SCValdiInlineViewVerticalAlignmentBaseline) {
            CGPoint glyphLocation = [layoutManager locationForGlyphAtIndex:glyphRange.location];
            attachmentRect.origin.y = CGRectGetMinY(lineRect) + glyphLocation.y - attachmentSize.height;
        } else {
            attachmentRect.origin.y = lineRect.origin.y +
                SCValdiProcessedTextAttachmentYOffset(lineRect.size.height,
                                                      attachmentSize.height,
                                                      inlineViewAttachment.verticalAlignment);
        }
        attachmentRect.size.height = attachmentSize.height;
    }
    return attachmentRect;
}

- (void)clampToCharacterLimit:(NSInteger)characterLimit
               ignoreNewlines:(BOOL)ignoreNewlines
                    didChange:(BOOL *)didChange
{
    BOOL changed = NO;
    NSUInteger sourceLimit = _attributedString.length;
    if (characterLimit > 0 && sourceLimit > (NSUInteger)characterLimit) {
        sourceLimit = (NSUInteger)characterLimit;
        changed = YES;
    }

    NSMutableArray<SCValdiProcessedTextRangeMapping *> *mappings = [NSMutableArray new];
    __block BOOL removedNewlines = NO;
    __block NSUInteger destinationLocation = 0;
    if (ignoreNewlines) {
        NSString *stringValue = _attributedString.string;
        [stringValue enumerateSubstringsInRange:NSMakeRange(0, sourceLimit)
                                        options:NSStringEnumerationByComposedCharacterSequences
                                     usingBlock:^(NSString *substring,
                                                  NSRange substringRange,
                                                  NSRange enclosingRange,
                                                  BOOL *stop) {
            (void)enclosingRange;
            (void)stop;
            if ([substring rangeOfCharacterFromSet:[NSCharacterSet newlineCharacterSet]].location != NSNotFound) {
                removedNewlines = YES;
                return;
            }
            SCValdiProcessedTextAppendMapping(mappings, substringRange, destinationLocation);
            destinationLocation += substringRange.length;
        }];
    } else if (sourceLimit > 0) {
        SCValdiProcessedTextAppendMapping(mappings, NSMakeRange(0, sourceLimit), 0);
    }

    changed = changed || removedNewlines;
    if (!changed) {
        if (didChange != NULL) {
            *didChange = NO;
        }
        return;
    }

    if (didChange != NULL) {
        *didChange = YES;
    }

    NSAttributedString *previousAttributedString = _attributedString;
    NSAttributedString *previousCustomUnderlineSourceString = _customUnderlineSourceString;
    NSMutableAttributedString *attributedString = ObjectAs(_attributedString, NSMutableAttributedString);
    if (attributedString == nil) {
        attributedString = [_attributedString mutableCopy];
        _attributedString = attributedString;
    }
    if (previousCustomUnderlineSourceString == previousAttributedString) {
        _customUnderlineSourceString = attributedString;
    }
    SCValdiProcessedTextDeleteCharactersOutsideMappings(attributedString, mappings);
    if (previousCustomUnderlineSourceString != nil && previousCustomUnderlineSourceString != previousAttributedString) {
        NSMutableAttributedString *customUnderlineSourceString =
            ObjectAs(previousCustomUnderlineSourceString, NSMutableAttributedString);
        if (customUnderlineSourceString == nil) {
            customUnderlineSourceString = [previousCustomUnderlineSourceString mutableCopy];
            _customUnderlineSourceString = customUnderlineSourceString;
        }
        SCValdiProcessedTextDeleteCharactersOutsideMappings(customUnderlineSourceString, mappings);
    }
    SCValdiProcessedTextApplyMappingsToRangeValues(_onTapItems, mappings);
    SCValdiProcessedTextApplyMappingsToRangeValues(_onLayoutItems, mappings);
    SCValdiProcessedTextApplyMappingsToRangeValues(_inlineAttachmentItems, mappings);
    SCValdiProcessedTextApplyMappingsToRangeValues(_animationItems, mappings);
    SCValdiProcessedTextApplyMappingsToOutlines(_outlineItems, mappings);
    _customUnderlineCharacterRanges = SCValdiProcessedTextRangesByApplyingMappings(_customUnderlineCharacterRanges, mappings);
    [self _updateInlineAttachmentItemsOrderedByChildIndex];
}

- (void)enumerateOnLayoutCallbacksUsingBlock:(SCValdiProcessedTextCallbackRangeBlock)block
{
    BOOL stop = NO;
    for (SCValdiProcessedTextRangeValue *item in _onLayoutItems) {
        block(item.value, item.range, &stop);
        if (stop) {
            break;
        }
    }
}

- (void)enumerateInlineViewAttachmentsUsingBlock:(SCValdiProcessedTextInlineViewAttachmentBlock)block
{
    BOOL stop = NO;
    for (SCValdiProcessedTextRangeValue *item in _inlineAttachmentItems) {
        block(item.value, item.range, &stop);
        if (stop) {
            break;
        }
    }
}

- (void)enumerateAnimationTransformsUsingBlock:(SCValdiProcessedTextAnimationTransformBlock)block
{
    BOOL stop = NO;
    for (SCValdiProcessedTextRangeValue *item in _animationItems) {
        block(item.value, item.range, &stop);
        if (stop) {
            break;
        }
    }
}

- (void)enumerateOuterOutlinesUsingBlock:(SCValdiProcessedTextOuterOutlineBlock)block
{
    BOOL stop = NO;
    for (SCValdiProcessedTextOuterOutline *outline in _outlineItems) {
        block(outline.color, outline.width, outline.range, &stop);
        if (stop) {
            break;
        }
    }
}

- (BOOL)updateInlineAttachments
{
    if (!self.hasInlineViewAttachment || _attributedString.length == 0) {
        return NO;
    }

    __block BOOL changed = NO;
    [self enumerateInlineViewAttachmentsUsingBlock:^(SCValdiInlineViewAttachmentInfo *attachmentInfo,
                                                     NSRange range,
                                                     BOOL *stop) {
        (void)stop;
        NSTextAttachment *textAttachment =
            ObjectAs([self->_attributedString attribute:NSAttachmentAttributeName atIndex:range.location effectiveRange:nil],
                     NSTextAttachment);
        if (textAttachment == nil) {
            return;
        }

        CGSize attachmentSize = attachmentInfo.size;
        CGRect bounds = CGRectMake(0, 0, attachmentSize.width, attachmentSize.height);
        if (!CGRectEqualToRect(textAttachment.bounds, bounds)) {
            textAttachment.bounds = bounds;
            changed = YES;
        }
    }];

    return changed;
}

@end
