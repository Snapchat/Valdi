//
//  SCValdiTextLayout.m
//  valdi-ios
//
//  Created by Simon Corsin on 12/21/22.
//


#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiRectUtils.h"

@implementation SCValdiTextLayout {
    NSTextStorage *_textStorage;
    NSTextContainer *_textContainer;
}

- (instancetype)init
{
    return [self initWithLayoutManager:[[NSLayoutManager alloc] init]];
}

- (instancetype)initWithLayoutManager:(NSLayoutManager *)layoutManager
{
    self = [super init];

    if (self) {
        _layoutManager = layoutManager;
        _textStorage = [[NSTextStorage alloc] init];
        [_textStorage addLayoutManager:_layoutManager];

        _textContainer = [[NSTextContainer alloc] init];
        _textContainer.lineFragmentPadding = 0;

        [_layoutManager addTextContainer:_textContainer];
    }

    return self;
}

- (void)setAttributedString:(NSAttributedString *)attributedString
{
    if (_attributedString != attributedString) {
        _attributedString = attributedString;

        if (!attributedString) {
            [_textStorage setAttributedString:[NSAttributedString new]];
        } else {
            [_textStorage setAttributedString:attributedString];
        }
    }
}

- (void)ensureLayout
{
    [_layoutManager ensureLayoutForTextContainer:_textContainer];
}

- (CGRect)_resolveDrawRectWithOrigin:(CGPoint)origin
{
    CGRect usedRect = self.usedRect;
    CGSize layoutSize = self.size;

    CGPoint drawOrigin = CGPointMake(origin.x, origin.y + (layoutSize.height - usedRect.size.height) / 2);

    return CGRectMake(drawOrigin.x, drawOrigin.y, usedRect.size.width, usedRect.size.height);
}

- (void)drawInRect:(CGRect)rect
{
    self.size = rect.size;

    [self ensureLayout];

    CGRect drawRect = [self _resolveDrawRectWithOrigin:rect.origin];

    NSRange glyphRange = [_layoutManager glyphRangeForTextContainer:_textContainer];
    [_layoutManager drawGlyphsForGlyphRange:glyphRange atPoint:drawRect.origin];
}

- (CGSize)size
{
    return _textContainer.size;
}

- (void)setSize:(CGSize)size
{
    if (!CGSizeEqualToSize(_textContainer.size, size)) {
        _textContainer.size = size;
    }
}

- (CGRect)usedRect
{
    [self ensureLayout];
    return [_layoutManager usedRectForTextContainer:_textContainer];
}

- (void)setMaxNumberOfLines:(NSUInteger)maxNumberOfLines
{
    _textContainer.maximumNumberOfLines = maxNumberOfLines;
}

- (NSUInteger)maxNumberOfLines
{
    return _textContainer.maximumNumberOfLines;
}

- (NSInteger)characterIndexAtPoint:(CGPoint)point
{
    CGRect drawRect = [self _resolveDrawRectWithOrigin:CGPointZero];
    if (!CGRectContainsPoint(drawRect, point)) {
        return NSNotFound;
    }

    CGPoint textContainerPoint = CGPointMake(point.x - drawRect.origin.x, point.y - drawRect.origin.y);
    NSUInteger location = [_layoutManager characterIndexForPoint:textContainerPoint
                                                 inTextContainer:_textContainer
                        fractionOfDistanceBetweenInsertionPoints:nil];

    return (NSInteger)location;
}

- (NSInteger)insertionIndexAtPoint:(CGPoint)point
{
    [self ensureLayout];

    NSUInteger textLength = _textStorage.length;
    if (textLength == 0) {
        return 0;
    }

    CGRect drawRect = [self _resolveDrawRectWithOrigin:CGPointZero];
    CGPoint textContainerPoint = CGPointMake(point.x - drawRect.origin.x, point.y - drawRect.origin.y);
    CGFloat fraction = 0.0;
    NSUInteger characterIndex = [_layoutManager characterIndexForPoint:textContainerPoint
                                                       inTextContainer:_textContainer
                              fractionOfDistanceBetweenInsertionPoints:&fraction];
    if (fraction > 0.5 && characterIndex < textLength) {
        characterIndex += 1;
    }

    return (NSInteger)MIN(characterIndex, textLength);
}

- (CGRect)boundingRectForRange:(NSRange)range
{
    // Convert characters to glyphs
    NSUInteger start = [_layoutManager glyphIndexForCharacterAtIndex: range.location];
    NSUInteger end = [_layoutManager glyphIndexForCharacterAtIndex:range.location + range.length];
    return [_layoutManager boundingRectForGlyphRange:NSMakeRange(start, end - start)
                                     inTextContainer:_textContainer];
}

- (NSRange)_glyphRangeForCharacterRange:(NSRange)range
{
    [self ensureLayout];

    NSUInteger textLength = _textStorage.length;
    if (textLength == 0 || range.length == 0 || range.location >= textLength) {
        return NSMakeRange(NSNotFound, 0);
    }

    NSUInteger startCharacterIndex = MIN(range.location, textLength - 1);
    NSUInteger endCharacterIndex = MIN(NSMaxRange(range) - 1, textLength - 1);
    NSUInteger startGlyphIndex = [_layoutManager glyphIndexForCharacterAtIndex:startCharacterIndex];
    NSUInteger endGlyphIndex = [_layoutManager glyphIndexForCharacterAtIndex:endCharacterIndex];
    if (endGlyphIndex < startGlyphIndex) {
        return NSMakeRange(NSNotFound, 0);
    }

    return NSMakeRange(startGlyphIndex, endGlyphIndex - startGlyphIndex + 1);
}

- (NSArray<NSValue *> *)selectionRectsForRange:(NSRange)range
                                 inDrawingRect:(CGRect)rect
{
    NSRange glyphRange = [self _glyphRangeForCharacterRange:range];
    if (glyphRange.location == NSNotFound || glyphRange.length == 0) {
        return @[];
    }

    CGRect drawRect = [self _resolveDrawRectWithOrigin:rect.origin];
    NSMutableArray<NSValue *> *rects = [NSMutableArray new];
    [_layoutManager enumerateLineFragmentsForGlyphRange:glyphRange
                                             usingBlock:^(CGRect lineRect,
                                                          CGRect usedRect,
                                                          NSTextContainer *textContainer,
                                                          NSRange lineGlyphRange,
                                                          BOOL *stop) {
        (void)lineRect;
        (void)usedRect;
        (void)stop;

        NSRange glyphRangeOnLine = NSIntersectionRange(glyphRange, lineGlyphRange);
        if (glyphRangeOnLine.length == 0) {
            return;
        }

        CGRect lineSelectionRect = [self->_layoutManager boundingRectForGlyphRange:glyphRangeOnLine
                                                                   inTextContainer:textContainer];
        if (CGRectIsEmpty(lineSelectionRect)) {
            return;
        }

        BOOL selectionStartsBeforeLine = glyphRange.location < lineGlyphRange.location;
        BOOL selectionEndsAfterLine = NSMaxRange(glyphRange) > NSMaxRange(lineGlyphRange);
        if (selectionStartsBeforeLine) {
            CGFloat selectionMaxX = CGRectGetMaxX(lineSelectionRect);
            lineSelectionRect.origin.x = lineRect.origin.x;
            lineSelectionRect.size.width = selectionMaxX - lineSelectionRect.origin.x;
        }
        if (selectionEndsAfterLine) {
            lineSelectionRect.size.width = CGRectGetMaxX(lineRect) - lineSelectionRect.origin.x;
        }

        lineSelectionRect.origin.x += drawRect.origin.x;
        lineSelectionRect.origin.y += drawRect.origin.y;
        [rects addObject:[NSValue valueWithCGRect:lineSelectionRect]];
    }];
    return rects;
}

- (CGRect)caretRectForCharacterIndex:(NSUInteger)characterIndex
                       inDrawingRect:(CGRect)rect
{
    [self ensureLayout];

    CGRect drawRect = [self _resolveDrawRectWithOrigin:rect.origin];
    NSUInteger textLength = _textStorage.length;
    CGRect usedRect = self.usedRect;

    if (textLength == 0) {
        return CGRectMake(drawRect.origin.x, drawRect.origin.y, 2.0, MAX(usedRect.size.height, 1.0));
    }

    NSUInteger clampedCharacterIndex = MIN(characterIndex, textLength);
    NSUInteger glyphCharacterIndex = clampedCharacterIndex == textLength ? textLength - 1 : clampedCharacterIndex;
    NSUInteger glyphIndex = [_layoutManager glyphIndexForCharacterAtIndex:glyphCharacterIndex];
    CGRect glyphRect = [_layoutManager boundingRectForGlyphRange:NSMakeRange(glyphIndex, 1)
                                                 inTextContainer:_textContainer];
    CGRect lineRect = [_layoutManager lineFragmentRectForGlyphAtIndex:glyphIndex effectiveRange:nil];
    CGFloat caretX = clampedCharacterIndex == textLength ? CGRectGetMaxX(glyphRect) : CGRectGetMinX(glyphRect);
    CGFloat caretHeight = MAX(lineRect.size.height, 1.0);

    return CGRectMake(drawRect.origin.x + caretX,
                      drawRect.origin.y + lineRect.origin.y,
                      2.0,
                      caretHeight);
}

- (NSArray<NSValue *> *)underlineRectsForRange:(NSRange)range
                                 inDrawingRect:(CGRect)rect
                                     lineWidth:(CGFloat)lineWidth
                               underlineOffset:(CGFloat)underlineOffset
{
    [self ensureLayout];
    CGRect drawRect = [self _resolveDrawRectWithOrigin:rect.origin];
    return SCValdiCustomUnderlineRectsForRange(_textStorage,
                                               _layoutManager,
                                               range,
                                               NSMakeRange(0, 0),
                                               NO,
                                               drawRect.origin,
                                               lineWidth,
                                               underlineOffset);
}

+ (CGRect)boundingRectWithAttributedString:(NSAttributedString *)attributedString
                                   maxSize:(CGSize)maxSize
                          maxNumberOfLines:(NSUInteger)maxNumberOfLines
{
    SCValdiTextLayout *textLayout = [SCValdiTextLayout new];
    [textLayout setAttributedString:attributedString];
    textLayout.size = maxSize;
    textLayout.maxNumberOfLines = maxNumberOfLines;

    return textLayout.usedRect;
}

+ (CGSize)measureSizeWithMaxSize:(CGSize)maxSize
                   fontAttributes:(SCValdiFontAttributes *)fontAttributes
                      fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                             text:(id)text
                  traitCollection:(UITraitCollection *)traitCollection
{
    if (!traitCollection) {
        SCLogValdiWarning(@"Trait collection is nil. This will cause incorrect text measurement for different font sizes");
    }

    if (!fontAttributes) {
        fontAttributes = [NSAttributedString defaultFontAttributes];
    }

    NSString *textValue = [text isKindOfClass:[NSString class]] ? text : nil;
    if (!textValue && [text isKindOfClass:[NSNull class]]) {
        textValue = @"";
    }

    BOOL isRightToLeft = NO; // Hard-coding this to NO, as it may have no measurement impact either way (and NO is faster)
    NSDictionary<NSAttributedStringKey, id> *attributes = [fontAttributes resolveAttributesWithIsRightToLeft:isRightToLeft
                                                                                              traitCollection:traitCollection];

    NSStringDrawingContext *context = [[NSStringDrawingContext alloc] init];
    [context setValue:@(fontAttributes.numberOfLines) forKey:@"maximumNumberOfLines"];

    NSStringDrawingOptions options = NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingTruncatesLastVisibleLine;

    CGRect boundingRect;
    if (textValue) {
        boundingRect = [textValue boundingRectWithSize:maxSize options:options attributes:attributes context:context];
    } else {
        NSAttributedString *attributedString = [NSAttributedString attributedStringWithValdiText:text
                                                                                      attributes:attributes
                                                                                   isRightToLeft:isRightToLeft
                                                                                     fontManager:fontManager
                                                                                 traitCollection:traitCollection];
        boundingRect = [attributedString boundingRectWithSize:maxSize options:options context:context];
    }

    CGSize outSize = boundingRect.size;
    outSize.width = CGFloatNormalizeCeil(outSize.width);
    outSize.height = CGFloatNormalizeCeil(outSize.height);
    return outSize;
}

@end
