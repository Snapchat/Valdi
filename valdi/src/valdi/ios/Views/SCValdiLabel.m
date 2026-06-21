//
//  SCValdiLabel.m
//  Valdi
//
//  Created by Simon Corsin on 5/18/20.
//

#import "valdi/ios/Views/SCValdiLabel.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi/ios/Categories/UIView+Valdi.h"

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiFont.h"
#import "valdi/ios/Text/SCValdiAttributedText.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi/ios/Text/SCValdiTextGradientHelper.h"
#import "valdi/ios/Text/SCValdiOnTapAttribute.h"
#import "valdi/ios/Text/SCValdiOnLayoutAttribute.h"
#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiRectUtils.h"
#import "valdi_core/SCValdiResult.h"

static NSString *const kTextGradientLayoutKey = @"text_gradient";

@interface SCValdiLabel() <SCValdiAttributedTextOnTapGestureRecognizerFunctionProvider>

@end

@implementation SCValdiLabel {
    SCValdiTextLayout *_textLayout;
    SCValdiFontAttributes *_fontAttributes;
    id<SCValdiFontManagerProtocol> _fontManager;
    id _textValue;
    BOOL _needAttributedTextUpdate;
    SCValdiTextMode _labelMode;
    BOOL _hasOnTapGestureRecognizer;
    SCValdiTextGradientHelper *_textGradientHelper;
    BOOL _updateOnLayout;
    SCValdiCustomUnderlineStyle *_customUnderlineStyle;
    NSAttributedString *_customUnderlineSourceAttributedString;
    NSArray<NSValue *> *_customUnderlineCharacterRanges;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    if (self) {
        self.shadowOffset = CGSizeMake(0, 0);
        self.userInteractionEnabled = YES;
        self.adjustsFontForContentSizeCategory = NO;
        _labelMode = SCValdiTextModeText;
    }

    return self;
}

- (void)valdi_applySlowClipping:(BOOL)slowClipping animator:(id<SCValdiAnimatorProtocol> )animator
{
    self.clipsToBounds = slowClipping;
}

- (void)layoutSubviews
{
    [self _updateTextGradientColorIfNeeded];
    [self _updateAttributedTextIfNeeded];
    [super layoutSubviews];
    _textLayout.size = self.bounds.size;

    // TODO(3065): Also update on view size changed
    if (_updateOnLayout) {
        // Everything has been layed out, Attributed text callbacks go here
        NSAttributedString *attributedString = (_textLayout) ? _textLayout.attributedString : self.attributedText;
        if (!attributedString) {
            return;
        }

        [attributedString enumerateAttribute:kSCValdiAttributedStringKeyOnLayout
                                                 inRange:NSMakeRange(0, attributedString.length)
                                                 options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                              usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
                        if (value) {
                            SCValdiOnLayoutAttribute *attribute = value;
                            CGRect newBounds = [_textLayout boundingRectForRange:range];
                            SCValdiMarshallerScoped(marshaller, {
                                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.x));
                                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.y));
                                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.width));
                                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.height));
                                [attribute.callback performWithMarshaller:marshaller];
                            });
                        }
                    }];

        _updateOnLayout = NO;
    }
}

- (CGPoint)convertPoint:(CGPoint)point fromView:(UIView *)view
{
    return [self valdi_convertPoint:point fromView:view];
}

- (CGPoint)convertPoint:(CGPoint)point toView:(UIView *)view
{
    return [self valdi_convertPoint:point toView:view];
}

- (CGSize)sizeThatFits:(CGSize)size
{
    [self _updateAttributedTextIfNeeded];
    return [super sizeThatFits:size];
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.valdiHitTest != nil) {
        return [self valdi_hitTest:point withEvent:event withCustomHitTest:self.valdiHitTest];
    }
    return [super hitTest:point withEvent:event];
}

- (void)drawTextInRect:(CGRect)rect
{
    if (_textLayout) {
        [_textLayout drawInRect:rect];
        [self _drawCustomUnderlinesInRect:rect];
    } else {
        [super drawTextInRect:rect];
    }
}

+ (CGSize)measureSizeWithMaxSize:(CGSize)maxSize
                  fontAttributes:(SCValdiFontAttributes *)fontAttributes
                     fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                            text:(id)text
                 traitCollection:(UITraitCollection *)traitCollection
{
    return [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                      fontAttributes:fontAttributes
                                         fontManager:fontManager
                                                text:text
                                     traitCollection:traitCollection];
}

- (BOOL)_needAttributedString
{
    if (_fontAttributes.needAttributedString) {
        return YES;
    }

    if (_textValue && (![_textValue isKindOfClass:[NSString class]])) {
        return YES;
    }

    return NO;
}

- (id<SCValdiFunction>)onTapFunctionAtLocation:(CGPoint)location
{
    NSAttributedString *attributedString = _textLayout.attributedString;
    if (!attributedString) {
        return nil;
    }

    NSUInteger index = [_textLayout characterIndexAtPoint:location];
    if (index == NSNotFound || index >= attributedString.length) {
        return nil;
    }

    SCValdiOnTapAttribute *onTapAttribute = [attributedString
                                                attribute:kSCValdiAttributedStringKeyOnTap
                                                atIndex:index
                                                effectiveRange:nil];
    return onTapAttribute.callback;
}

- (SCValdiAttributedTextOnTapGestureRecognizer *)_getAttributedTextOnTapGestureRecognizer
{
    if (!_hasOnTapGestureRecognizer) {
        return nil;
    }

    for (UIGestureRecognizer *gestureRecognizer in self.gestureRecognizers) {
        SCValdiAttributedTextOnTapGestureRecognizer *attributedOnTapGestureRecognizer = ObjectAs(gestureRecognizer, SCValdiAttributedTextOnTapGestureRecognizer);
        if (attributedOnTapGestureRecognizer) {
            return attributedOnTapGestureRecognizer;
        }
    }

    return nil;
}

- (void)_removeAttributedTextOnTapGestureRecognizer
{
    SCValdiAttributedTextOnTapGestureRecognizer *attributedOnTapGestureRecognizer = [self _getAttributedTextOnTapGestureRecognizer];
    if (attributedOnTapGestureRecognizer) {
        _hasOnTapGestureRecognizer = NO;
        [self removeGestureRecognizer:attributedOnTapGestureRecognizer];
    }
}

- (void)_addAttributedTextOnTapGestureRecognizer
{
    SCValdiAttributedTextOnTapGestureRecognizer *attributedOnTapGestureRecognizer = [self _getAttributedTextOnTapGestureRecognizer];
    if (!attributedOnTapGestureRecognizer) {
        attributedOnTapGestureRecognizer = [[SCValdiAttributedTextOnTapGestureRecognizer alloc] init];
        _hasOnTapGestureRecognizer = YES;
        [self addGestureRecognizer:attributedOnTapGestureRecognizer];
        attributedOnTapGestureRecognizer.functionProvider = self;
    }
}

- (NSAttributedString *)_displayAttributedStringForAttributedString:(NSAttributedString *)attributedString
{
    _customUnderlineSourceAttributedString = nil;
    _customUnderlineCharacterRanges = nil;
    if (!_customUnderlineStyle || attributedString.length == 0) {
        return attributedString;
    }

    NSArray<NSValue *> *customUnderlineCharacterRanges = nil;
    NSAttributedString *displayAttributedString =
        SCValdiAttributedStringByRemovingNativeUnderlines(attributedString, &customUnderlineCharacterRanges);
    if (customUnderlineCharacterRanges.count == 0) {
        return attributedString;
    }

    _customUnderlineSourceAttributedString = [attributedString copy];
    _customUnderlineCharacterRanges = customUnderlineCharacterRanges;
    return displayAttributedString;
}

- (UIColor *)_customUnderlineColorForRange:(NSRange)range
{
    return SCValdiCustomUnderlineColorForRange(_customUnderlineSourceAttributedString, range, self.textColor);
}

- (void)_drawCustomUnderlinesInRect:(CGRect)rect
{
    if (!_customUnderlineStyle
        || !_customUnderlineSourceAttributedString
        || _customUnderlineCharacterRanges.count == 0
        || !_textLayout) {
        return;
    }

    CGContextRef context = UIGraphicsGetCurrentContext();
    if (!context) {
        return;
    }

    CGContextSaveGState(context);
    CGFloat underlineLineWidth = _customUnderlineStyle.height;
    CGContextSetLineWidth(context, underlineLineWidth);
    SCValdiCustomUnderlineApplyDashPattern(context, _customUnderlineStyle);

    CGFloat underlineOffset = _customUnderlineStyle.offset;
    for (NSValue *rangeValue in _customUnderlineCharacterRanges) {
        NSRange range = rangeValue.rangeValue;
        [[self _customUnderlineColorForRange:range] setStroke];
        SCValdiCustomUnderlineDrawRects(context, [_textLayout underlineRectsForRange:range
                                                                       inDrawingRect:rect
                                                                           lineWidth:underlineLineWidth
                                                                     underlineOffset:underlineOffset]);
    }

    CGContextRestoreGState(context);
}

- (void)_updateAttributedTextIfNeeded
{
    if (_needAttributedTextUpdate) {
        _needAttributedTextUpdate = NO;

        BOOL isRightToLeft = self.valdiViewNode.isRightToLeft;
        UITraitCollection *traitCollection = self.valdiContext.traitCollection;

        SCValdiFontAttributes *fontAttributes = [self fontAttributes];

        if ([self _needAttributedString]) {
            NSAttributedString *attributedString = [NSAttributedString attributedStringWithValdiText:_textValue
                                                                                             attributes:[fontAttributes resolveAttributesWithIsRightToLeft:isRightToLeft traitCollection:traitCollection]
                                                                                          isRightToLeft:isRightToLeft
                                                                                            fontManager:_fontManager
                                                                                        traitCollection:traitCollection];
            NSAttributedString *displayAttributedString = [self _displayAttributedStringForAttributedString:attributedString];

            __block BOOL hasOnTapAttribute = NO;
            [attributedString enumerateAttribute:kSCValdiAttributedStringKeyOnTap
                                         inRange:NSMakeRange(0, attributedString.length)
                                         options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                      usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
                if (value) {
                    *stop = YES;
                    hasOnTapAttribute = YES;
                }
            }];

            __block BOOL hasOnLayoutAttribute = NO;
            [attributedString enumerateAttribute:kSCValdiAttributedStringKeyOnLayout
                                         inRange:NSMakeRange(0, attributedString.length)
                                         options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                      usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
                if (value) {
                    *stop = YES;
                    hasOnLayoutAttribute = YES;
                }
            }];

            BOOL hasCustomUnderlineAttribute = _customUnderlineSourceAttributedString != nil;
            if (hasOnTapAttribute || hasOnLayoutAttribute || hasCustomUnderlineAttribute) {
                [self updateLabelMode:SCValdiTextModeValdiTextLayout];
                _textLayout.attributedString = displayAttributedString;
                [self setNeedsDisplay];
            } else {
                [self updateLabelMode:SCValdiTextModeAttributedText];

                self.attributedText = displayAttributedString;
            }

            if (hasOnLayoutAttribute) {
                _updateOnLayout = YES;
            }

            if (hasOnTapAttribute) {
                [self _addAttributedTextOnTapGestureRecognizer];
            } else {
                [self _removeAttributedTextOnTapGestureRecognizer];
            }

        } else {
            // Can set without attributed text

            _customUnderlineSourceAttributedString = nil;
            _customUnderlineCharacterRanges = nil;
            [self updateLabelMode:SCValdiTextModeText];
            [self _removeAttributedTextOnTapGestureRecognizer];

            UIFont *font = [fontAttributes.font resolveFontFromTraitCollection:traitCollection];
            if (self.font != font) {
                self.font = font;
            }

            if (self.textColor != fontAttributes.color) {
                self.textColor = fontAttributes.color;
            }

            NSTextAlignment resolvedTextAlignment = [fontAttributes resolveTextAlignmentWithIsRightToLeft:isRightToLeft];
            if (self.textAlignment != resolvedTextAlignment) {
                self.textAlignment = resolvedTextAlignment;
            }

            self.text = _textValue;
        }

        // Overrides color for attributed and non-attributed text if text gradient is specified
        UIColor *textGradientColor = _textGradientHelper.gradientColor;
        if (textGradientColor && self.textColor != textGradientColor) {
            self.textColor = textGradientColor;
        }

        if (self.numberOfLines != fontAttributes.numberOfLines) {
            self.numberOfLines = fontAttributes.numberOfLines;
        }

        // When rendering, we always set the label's lineBreakMode to lineBreakByTruncatingTail, even when the attributed text
        // has a different line break mode. This ensures the label still tries to fill out all available space
        // with text (if numberOfLines allows) and if the text doesn't fit - it renders a nice ellipsis at the end of the last visible line.
        if (fontAttributes.lineBreakMode == NSLineBreakByClipping) {
            self.lineBreakMode = NSLineBreakByClipping;
        } else {
            self.lineBreakMode = NSLineBreakByTruncatingTail;
        }
    }
}

- (BOOL)requiresLayoutWhenAnimatingBounds
{
    return NO;
}

- (SCValdiFontAttributes *)fontAttributes
{
    if (_fontAttributes) {
        return _fontAttributes;
    }
    return [NSAttributedString defaultFontAttributes];
}

- (void)valdi_setFontManager:(id<SCValdiFontManagerProtocol>)fontManager
{
    _fontManager = fontManager;
}

- (void)valdi_setFontAttributes:(SCValdiFontAttributes *)fontAttributes
{
    _fontAttributes = fontAttributes;
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
}

- (void)valdi_setText:(id)textValue
{
    _textValue = textValue;
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
}

- (void)valdi_setCustomUnderlineStyle:(SCValdiCustomUnderlineStyle *)customUnderlineStyle
{
    _customUnderlineStyle = customUnderlineStyle;
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
    [self setNeedsDisplay];
}

- (SCValdiTextGradientHelper *)_createTextGradientHelperIfNeeded
{
    if (!_textGradientHelper) {
        _textGradientHelper = [SCValdiTextGradientHelper new];
    }
    return _textGradientHelper;
}

- (BOOL)valdi_setTextGradient:(NSArray *)attributeValue
                        animator:(id<SCValdiAnimatorProtocol>)animator
{
    NSArray *colors = attributeValue.firstObject;
    if (colors.count < 2) {
        if (_textGradientHelper) {
            [_textGradientHelper setGradientAttributes:nil];
        }
        [self.valdiViewNode setDidFinishLayoutBlock:nil forKey:kTextGradientLayoutKey];
        _needAttributedTextUpdate = YES;
        [self setNeedsLayout];
        [self setNeedsDisplay];
        return YES;
    }

    [[self _createTextGradientHelperIfNeeded] setGradientAttributes:attributeValue];
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
    [self setNeedsDisplay];
    [self _updateTextGradientLayerWithAnimator:animator];

    [self.valdiViewNode setDidFinishLayoutBlock:^(SCValdiLabel *view, id<SCValdiAnimatorProtocol> animator) {
        [view _updateTextGradientLayerWithAnimator:animator];
    } forKey:kTextGradientLayoutKey];

    return YES;
}

- (void)valdi_layoutTextGradientLayerWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    [_textGradientHelper layoutInView:self animator:animator];
}

- (void)_updateTextGradientColorIfNeeded
{
    if ([_textGradientHelper updateColorIfNeeded]) {
        _needAttributedTextUpdate = YES;
    }
}

- (void)_updateTextGradientLayerWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    if (![_textGradientHelper layoutIfNeededInView:self animator:animator]) {
        return;
    }
    _needAttributedTextUpdate = YES;
    [self setNeedsDisplay];
}

+ (CGSize)valdi_onMeasureWithAttributes:(id<SCValdiViewLayoutAttributes>)attributes
                                   maxSize:(CGSize)maxSize
                               fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                           traitCollection:(UITraitCollection *)traitCollection
{
    SCValdiFontAttributes *fontAttributes = ObjectAs([attributes valueForAttributeName:@"fontSpecs"], SCValdiFontAttributes);
    id text = [attributes valueForAttributeName:@"value"];

    return [SCValdiLabel measureSizeWithMaxSize:maxSize
                                    fontAttributes:fontAttributes
                                        fontManager:fontManager
                                              text:text
                                   traitCollection:traitCollection];
}

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    id<SCValdiFontManagerProtocol> fontManager = [attributesBinder fontManager];

    [attributesBinder bindCompositeAttribute:@"fontSpecs" parts:[NSAttributedString valdiFontAttributes] withUntypedBlock:^BOOL(__kindof SCValdiLabel *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setFontManager:fontManager];
        [view valdi_setFontAttributes:ObjectAs(attributeValue, SCValdiFontAttributes)];
        return YES;
    } resetBlock:^(SCValdiLabel *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setFontAttributes:nil];
    }];

    [attributesBinder bindAttribute:@"value" invalidateLayoutOnChange:YES withTextBlock:^BOOL(SCValdiLabel *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setFontManager:fontManager];
        [view valdi_setText:attributeValue];
        return YES;
    } resetBlock:^(SCValdiLabel *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setText:nil];
    }];

    [attributesBinder bindAttribute:@"customUnderlineStyle"
        invalidateLayoutOnChange:NO
        withUntypedBlock:^BOOL(SCValdiLabel *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setCustomUnderlineStyle:ObjectAs(attributeValue, SCValdiCustomUnderlineStyle)];
            return YES;
        }
        resetBlock:^(SCValdiLabel *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setCustomUnderlineStyle:nil];
        }];

    [attributesBinder registerPreprocessorForAttribute:@"customUnderlineStyle" enableCache:YES withBlock:^id(id value) {
        NSString *styleString = ObjectAs(value, NSString);
        if (!styleString) {
            return SCValdiResultFailure(@"customUnderlineStyle must be a string");
        }

        NSError *error = nil;
        SCValdiCustomUnderlineStyle *style = [SCValdiCustomUnderlineStyle styleWithString:styleString error:&error];
        if (!style) {
            return SCValdiResultFailure(error.localizedDescription ?: @"Invalid customUnderlineStyle");
        }

        return SCValdiResultSuccessWithData(style);
    }];

    [attributesBinder registerPreprocessorForAttribute:@"font" enableCache:YES withBlock:^id(id value) {
        return [SCValdiFont fontFromValdiAttribute:ObjectAs(value, NSString) fontManager:fontManager];
    }];

    [attributesBinder registerPreprocessorForAttribute:@"fontSpecs" enableCache:YES withBlock:^id(id value) {
        return [NSAttributedString fontAttributesWithCompositeValue:value];
    }];

    [attributesBinder bindAttribute:@"adjustsFontSizeToFitWidth"
        invalidateLayoutOnChange:YES
        withBoolBlock:^BOOL(UILabel *label, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            label.adjustsFontSizeToFitWidth = attributeValue;
            return YES;
        }
        resetBlock:^(UILabel *label, id<SCValdiAnimatorProtocol> animator) {
            label.adjustsFontSizeToFitWidth = NO;
        }];

    [attributesBinder bindAttribute:@"minimumScaleFactor"
        invalidateLayoutOnChange:YES
        withDoubleBlock:^BOOL(UILabel *label, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
            label.minimumScaleFactor = attributeValue;
            return YES;
        }
        resetBlock:^(UILabel *label, id<SCValdiAnimatorProtocol> animator) {
            label.minimumScaleFactor = 0;
        }];

    [attributesBinder bindAttribute:@"textGradient"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(SCValdiLabel *view, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setTextGradient:attributeValue animator:animator];
        }
        resetBlock:^(SCValdiLabel *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setTextGradient:nil animator:animator];
        }];

    [attributesBinder bindCompositeAttribute:@"shadowAttributes"
                                       parts:[self _valdiShadowComponents]
                            withUntypedBlock:^BOOL(SCValdiLabel *label, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
        NSArray *attributeValueArray = ObjectAs(attributeValue, NSArray);
        if (attributeValueArray.count != 2) {
            return NO;
        }

        NSArray<id> *textShadow = ObjectAs(attributeValueArray[0], NSArray);
        NSArray<id> *boxShadow = ObjectAs(attributeValueArray[1], NSArray);

        if (textShadow != nil && boxShadow != nil) {
            SCLogValdiError(@"Combining boxShadow and textShadow on the same label is not currently supported.");
            return NO;
        }

        if (boxShadow != nil) {
            return [label valdi_setBoxShadow:boxShadow animator:animator];
        } else {
            label.layer.shadowPath = nil;
        }

        return SCValdiSetTextHolderTextShadow(label, textShadow);
    }
        resetBlock:^(SCValdiLabel *label, id<SCValdiAnimatorProtocol> animator) {
            SCValdiResetTextHolderTextShadow(label);
    }];

    [attributesBinder setMeasureDelegate:^CGSize(id<SCValdiViewLayoutAttributes> attributes, CGSize maxSize, UITraitCollection *traitCollection) {
        return [SCValdiLabel valdi_onMeasureWithAttributes:attributes maxSize:maxSize fontManager:fontManager traitCollection:traitCollection];
    }];
}

+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)_valdiShadowComponents
{
    return @[
             [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"textShadow"
                                                                     type:SCNValdiCoreAttributeTypeUntyped
                                                                 optional:YES
                                                 invalidateLayoutOnChange:NO],
             [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"boxShadow"
                                                                     type:SCNValdiCoreAttributeTypeUntyped
                                                                 optional:YES
                                                 invalidateLayoutOnChange:NO],
             ];
}

- (void)updateLabelMode:(SCValdiTextMode)labelMode
{
    if (_labelMode == labelMode) {
        return;
    }

    // Cleanup
    switch (_labelMode) {
        case SCValdiTextModeText:
            self.text = nil;
            break;
        case SCValdiTextModeAttributedText:
            [self _clearAttributedText];
            break;
        case SCValdiTextModeValdiTextLayout:
            _textLayout = nil;
            [self setNeedsDisplay];
            break;
    }

    // Setup
    _labelMode = labelMode;

    switch (labelMode) {
        case SCValdiTextModeText:
            break;
        case SCValdiTextModeAttributedText:
            break;
        case SCValdiTextModeValdiTextLayout:
            _textLayout = [SCValdiTextLayout new];
            _textLayout.size = self.bounds.size;
            [self setNeedsDisplay];
            break;
    }
}

- (void)_clearAttributedText
{
    // There's a UIKit bug, where the label caches a set of "default attributes"
    // based on the `attributedText` that was previously set on it. Setting `attributedText`
    // to nil does not seem to clear the paragraphStyle from these cached attributes.
    // Calling the private API -[UILabel _setDefaultAttributes:nil] seem to help,
    // but in the interest of avoiding using a private system API we looked for an
    // alternative approach.
    //
    // Setting an attributed string value with an explicit default paragraph style,
    // then setting a nil `text` before setting a nil `attributedText` seems to do it.
    static dispatch_once_t onceToken;
    static NSAttributedString *cleanupAttributedString;
    dispatch_once(&onceToken, ^{
        NSDictionary *cleanupAttributes = @{
            NSParagraphStyleAttributeName: NSParagraphStyle.defaultParagraphStyle
        };
        // This doesn't seem to work correctly with an empty string value
        cleanupAttributedString = [[NSAttributedString alloc] initWithString:@"-"
                                                                  attributes:cleanupAttributes];
    });
    self.attributedText = cleanupAttributedString;
    self.text = nil;

    self.attributedText = nil;
}

- (BOOL)willEnqueueIntoValdiPool
{
    if (_labelMode != SCValdiTextModeText) {
        [self updateLabelMode:SCValdiTextModeText];
        _needAttributedTextUpdate = YES;
    }

    return self.class == [SCValdiLabel class];
}

- (NSString *)accessibilityLabel
{
    if (_labelMode == SCValdiTextModeValdiTextLayout) {
        return [_textLayout.attributedString string];
    }

    return [super accessibilityLabel];
}

- (UIAccessibilityTraits)accessibilityTraits
{
    UIAccessibilityTraits traits = [super accessibilityTraits];

    /* UILabel adds button when the text is empty, which we don't want */
    traits &= ~UIAccessibilityTraitButton;
    traits |= UIAccessibilityTraitStaticText;

    return traits;
}

@end
