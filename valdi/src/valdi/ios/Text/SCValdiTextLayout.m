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
    NSLayoutManager *_layoutManager;
    NSTextStorage *_textStorage;
    NSTextContainer *_textContainer;
}

- (instancetype)init
{
    self = [super init];

    if (self) {
        _layoutManager = [[NSLayoutManager alloc] init];
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

    NSUInteger location = [_layoutManager characterIndexForPoint:point inTextContainer:_textContainer fractionOfDistanceBetweenInsertionPoints:nil];

    return (NSInteger)location;
}

- (CGRect)boundingRectForRange:(NSRange)range
{
    // Convert characters to glyphs
    NSUInteger start = [_layoutManager glyphIndexForCharacterAtIndex: range.location];
    NSUInteger end = [_layoutManager glyphIndexForCharacterAtIndex:range.location + range.length];
    return [_layoutManager boundingRectForGlyphRange:NSMakeRange(start, end - start)
                                     inTextContainer:_textContainer];
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
