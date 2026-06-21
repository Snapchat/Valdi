//
//  SCValdiTextField.m
//  Valdi
//
//  Created by Joe Engelman on 8/30/18.
//

#import "valdi/ios/Views/SCValdiTextField.h"

#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/UIView+ValdiObjects.h"
#import "valdi_core/UIView+ValdiBase.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi/ios/SCValdiAttributesBinder.h"
#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi_core/SCNValdiCoreCompositeAttributePart.h"
#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiTextInputTraitAttributes.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiConfigurableTextHolderTraitAttributes.h"
#import "valdi_core/SCValdiTextInputUnfocusReason.h"

#import "valdi/ios/Text/SCValdiAttributedTextHelper.h"
#import "valdi/ios/Text/SCValdiFont.h"

@implementation SCValdiTextField {
    /// YES if pressing the return key should dismiss the keyboard, o/w NO
    BOOL _closesWhenReturnKeyPressed;
    /// The maximum length of the text
    NSNumber *_characterLimit;
    /// YES if all text should be selected on begin editing
    BOOL _selectTextOnFocus;

    id _textValue;
    SCValdiFontAttributes *_fontAttributes;
    id<SCValdiFontManagerProtocol> _fontManager;
    SCValdiTextMode _textMode;
    BOOL _needAttributedTextUpdate;

    id<SCValdiFunction> _Nullable _onWillChange;
    id<SCValdiFunction> _Nullable _onChange;
    id<SCValdiFunction> _Nullable _onEditBegin;
    id<SCValdiFunction> _Nullable _onEditEnd;
    id<SCValdiFunction> _Nullable _onReturn;
    id<SCValdiFunction> _Nullable _onWillDelete;
    id<SCValdiFunction> _Nullable _onSelectionChange;

    SCValdiTextInputUnfocusReason _lastUnfocusReason;
}

#pragma mark - UIView methods

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self addTarget:self action:@selector(onChange) forControlEvents:UIControlEventEditingChanged];

        self.adjustsFontForContentSizeCategory = NO;

        _closesWhenReturnKeyPressed = YES;

        self.returnKeyType = UIReturnKeyDone;

        self.delegate = self;

        _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;

        _textMode = SCValdiTextModeText;
        _needAttributedTextUpdate = YES;
        if (@available(iOS 17.0, *)) {
            self.inlinePredictionType = UITextInlinePredictionTypeNo;
        }
    }
    return self;
}

- (void)layoutSubviews
{
    [self _updateAttributedTextIfNeeded];
    [super layoutSubviews];
}

- (CGSize)sizeThatFits:(CGSize)size
{
    [self _updateAttributedTextIfNeeded];
    return [super sizeThatFits:size];
}

- (CGPoint)convertPoint:(CGPoint)point fromView:(UIView *)view
{
    return [self valdi_convertPoint:point fromView:view];
}

- (CGPoint)convertPoint:(CGPoint)point toView:(UIView *)view
{
    return [self valdi_convertPoint:point toView:view];
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.valdiHitTest != nil) {
        return [self valdi_hitTest:point withEvent:event withCustomHitTest:self.valdiHitTest];
    }
    return [super hitTest:point withEvent:event];
}

#pragma mark - UIView+Valdi

- (BOOL)willEnqueueIntoValdiPool
{
    // Because the text field is user editable, we can't recycle a text field.
    return NO;
}

- (void)setSelectedTextRange:(UITextRange *)selectedTextRange {
    [super setSelectedTextRange:selectedTextRange];

    SCValdiCallEvent(_onSelectionChange, self);
    [self notifySelectionChange];
}

#pragma mark - Action handling methods

INTERNED_STRING_CONST("focused", SCValdiTextFieldFocusedKey);
INTERNED_STRING_CONST("value", SCValdiTextFieldValueKey);
INTERNED_STRING_CONST("text", SCValdiTextFieldTextKey);
INTERNED_STRING_CONST("selection", SCValdiTextFieldSelectionKey);
INTERNED_STRING_CONST("selectionStart", SCValdiTextFieldSelectionStartKey);
INTERNED_STRING_CONST("selectionEnd", SCValdiTextFieldSelectionEndKey);
INTERNED_STRING_CONST("reason", SCValdiTextFieldReasonKey);

static NSInteger SCValdiMarshallEditTextEvent(SCValdiMarshallerRef marshaller, UITextField *textField) {
    UITextPosition *origin = textField.beginningOfDocument;
    NSInteger objectIndex = SCValdiMarshallerPushMap(marshaller, 1);
    SCValdiMarshallerPushString(marshaller, textField.text ?: @"");
    SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextFieldTextKey(), objectIndex);
    SCValdiMarshallerPushInt(marshaller, (int32_t)[textField offsetFromPosition:origin toPosition:textField.selectedTextRange.start]);
    SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextFieldSelectionStartKey(), objectIndex);
    SCValdiMarshallerPushInt(marshaller, (int32_t)[textField offsetFromPosition:origin toPosition:textField.selectedTextRange.end]);
    SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextFieldSelectionEndKey(), objectIndex);
    return objectIndex;
}

static void SCValdiCallEvent(id<SCValdiFunction> function, UITextField *textField)
{
    if (!function) {
        return;
    }
    SCValdiMarshallerScoped(marshaller, {
        SCValdiMarshallEditTextEvent(marshaller, textField);
        [function performWithMarshaller:marshaller];
    });
}

static void SCValdiCallEventWithReason(id<SCValdiFunction> function, UITextField *textField, NSInteger reasonId)
{
    if (!function) {
        return;
    }
    SCValdiMarshallerScoped(marshaller, {
        NSInteger objectIndex = SCValdiMarshallEditTextEvent(marshaller, textField);
        SCValdiMarshallerPushDouble(marshaller, reasonId);
        SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextFieldReasonKey(), objectIndex);
        [function performWithMarshaller:marshaller];
    });
}

#pragma mark - Internals

- (void)deleteBackward
{
    SCValdiCallEvent(_onWillDelete, self);
    [super deleteBackward];
}

- (void)onChange
{
    if (_onWillChange != nil) {
        SCValdiMarshallerScoped(marshaller, {
            SCValdiMarshallEditTextEvent(marshaller, self);
            if ([_onWillChange performSyncWithMarshaller:marshaller propagatesError:NO] && SCValdiMarshallerIsMap(marshaller, -1)) {
                @try {
                    SCValdiMarshallerMustGetMapProperty(marshaller, SCValdiTextFieldTextKey(), -1);
                    NSString* newText = SCValdiMarshallerGetString(marshaller, -1);
                    SCValdiMarshallerPop(marshaller);

                    SCValdiMarshallerMustGetMapProperty(marshaller, SCValdiTextFieldSelectionStartKey(), -1);
                    NSInteger indexStart = SCValdiMarshallerGetInt(marshaller, -1);
                    SCValdiMarshallerPop(marshaller);

                    SCValdiMarshallerMustGetMapProperty(marshaller, SCValdiTextFieldSelectionEndKey(), -1);
                    NSInteger indexEnd = SCValdiMarshallerGetInt(marshaller, -1);
                    SCValdiMarshallerPop(marshaller);

                    // we update only non-attributed strings, as we expect the JS side to be generating AttributedText
                    if (![self _needAttributedString]) {
                        _textValue = newText;
                        self.text = newText;
                        _needAttributedTextUpdate = YES;
                    }

                    // Then, update the selection range
                    NSInteger offsetLimit = self.text.length;
                    NSInteger offsetStart = MAX(0, MIN(offsetLimit, indexStart));
                    NSInteger offsetEnd = MAX(offsetStart, MIN(offsetLimit, indexEnd));
                    UITextPosition *positionOrigin = self.beginningOfDocument;
                    UITextPosition *positionStart = [self positionFromPosition:positionOrigin offset:offsetStart];
                    UITextPosition *positionEnd = [self positionFromPosition:positionOrigin offset:offsetEnd];
                    [self setSelectedTextRange:[self textRangeFromPosition:positionStart toPosition:positionEnd]];

                } @catch (SCValdiError *exc) {
                    SCLogValdiError(@"Failed to unmarshall edit text event: %@", exc.reason);
                }
            }
        });
    }

    // we update only non-attributed strings, as we expect the JS side to be generating AttributedText
    if (![self _needAttributedString]) {
        _textValue = self.text;
        _needAttributedTextUpdate = YES;
        [self _updateAttributedTextIfNeeded];
    }

    [self notifyTextValueDidChange];

    SCValdiCallEvent(_onChange, self);

    // Enables width of the view and its parents to update dynamically with text
    [self invalidateLayout];
}

#pragma mark - text value control

- (NSString *)computeTextValue:(NSString *)rawValue
{
    NSString *value = rawValue;
    // Clamp to character limit if needed
    NSInteger characterLimit = [_characterLimit integerValue];
    if (_characterLimit != nil && characterLimit >= 0 && value.length > (NSUInteger)characterLimit) {
        value = [value substringWithRange:NSMakeRange(0, characterLimit)];
    }
    return value;
}

- (NSAttributedString *)computeAttributedStringValue:(NSAttributedString *)rawValue
{
    NSAttributedString *value = rawValue;
    // Clamp to character limit if needed
    NSInteger characterLimit = [_characterLimit integerValue];
    if (_characterLimit != nil && characterLimit >= 0 && value.length > (NSUInteger)characterLimit) {
        value = [NSAttributedString trimAttributedString:value characterLimit:characterLimit];
    }
    return value;
}

- (void)notifyTextValueDidChange
{
    [self.valdiContext didChangeValue:self.text ?: @""
                    forInternedValdiAttribute:SCValdiTextFieldValueKey()
                              inViewNode:self.valdiViewNode];

}

- (void)notifySelectionChange
{
    UITextPosition *startPosition = self.selectedTextRange.start;
    UITextPosition *endPosition = self.selectedTextRange.end;
    NSInteger startOffset = [self offsetFromPosition:self.beginningOfDocument toPosition:startPosition];
    NSInteger endOffset = [self offsetFromPosition:self.beginningOfDocument toPosition:endPosition];

    [self.valdiContext didChangeValue: @[@(startOffset), @(endOffset)]
                    forInternedValdiAttribute:SCValdiTextFieldSelectionKey()
                              inViewNode:self.valdiViewNode];
}

#pragma mark - AttributedString management

- (void)updateLabelMode:(SCValdiTextMode)labelMode
{
    if (_textMode == labelMode) {
        return;
    }

    // Cleanup
    switch (_textMode) {
        case SCValdiTextModeText:
            self.text = nil;
            break;
        case SCValdiTextModeAttributedText:
            [self _clearAttributedText];
            break;
        case SCValdiTextModeValdiTextLayout:
            // noop - no support for touch events in SCValdiTextField
            break;
    }

    _textMode = labelMode;
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

- (void)_updateAttributedTextAndNotifyIfNeeded
{
    if ([self _updateAttributedTextIfNeeded]) {
        [self notifyTextValueDidChange];
    }
}

- (BOOL)_updateAttributedTextIfNeeded
{
    BOOL changed = NO;
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

            [self updateLabelMode:SCValdiTextModeAttributedText];

            NSAttributedString *trimmedString = [self computeAttributedStringValue:attributedString];
            if (![attributedString isEqualToAttributedString:trimmedString]) {
                changed = YES;
            }
            if (![self.attributedText isEqualToAttributedString:trimmedString]) {
                self.attributedText = trimmedString;
            }
        } else {
            // Can set without attributed text

            [self updateLabelMode:SCValdiTextModeText];;

            UIFont *font = [fontAttributes.font resolveFontFromTraitCollection:traitCollection];
            if (self.font != font) {
                self.font = font;
            }

            if (self.textColor != fontAttributes.color) {
                self.textColor = fontAttributes.color;
            }

            NSTextAlignment resolvedTextAlignment = [fontAttributes resolveTextAlignmentWithIsRightToLeft:isRightToLeft];

            // NSTextAlignmentNatural switches alignment based on keyboard language instead of app language,
            // introduce fix under COF to use NSTextAlignmentLeft and NSTextAlignmentRight
            // instead of Natural (the NSTextAlignment used for left)
            if (resolvedTextAlignment == NSTextAlignmentNatural) {
                if (traitCollection.layoutDirection == UITraitEnvironmentLayoutDirectionRightToLeft) {
                    resolvedTextAlignment = NSTextAlignmentRight;
                }
                else {
                    resolvedTextAlignment = NSTextAlignmentLeft;
                }
            }
            if (self.textAlignment != resolvedTextAlignment) {
                self.textAlignment = resolvedTextAlignment;
            }

            NSString *value = [self computeTextValue:_textValue];
            if (![self.text isEqualToString:value]) {
                changed = YES;
                self.text = value;
            }
        }
    }

    return changed;
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

#pragma mark - Attribute binding helper methods

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

- (void)valdi_setValue:(id)textValue
{
    _textValue = textValue;
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
}

- (BOOL)valdi_setCharacterLimit:(NSNumber *)characterLimit
{
    _characterLimit = characterLimit;
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
    return YES;
}

- (BOOL)valdi_setClosesWhenReturnKeyPressed:(BOOL)closesWhenReturnKeyPressed
{
    _closesWhenReturnKeyPressed = closesWhenReturnKeyPressed;
    return YES;
}

- (BOOL)valdi_setEnabled:(BOOL)enabled
{
    self.enabled = enabled;
    return YES;
}

- (BOOL)valdi_setFocused:(BOOL)focused
{
    if (focused) {
        [self becomeFirstResponder];
    } else {
        [self resignFirstResponder];
    }
    return YES;
}

- (BOOL)valdi_setTintColor:(UIColor *)color
{
    self.tintColor = color;
    return YES;
}

- (BOOL)valdi_setSelectTextOnFocus:(BOOL)selectTextOnFocus
{
    _selectTextOnFocus = selectTextOnFocus;
    return YES;
}

- (void)valdi_setOnWillChange:(id<SCValdiFunction>)onWillChange
{
    _onWillChange = onWillChange;
}

- (void)valdi_setOnChange:(id<SCValdiFunction>)onChange
{
    _onChange = onChange;
}

- (void)valdi_setOnEditBegin:(id<SCValdiFunction>)onEditBegin
{
    _onEditBegin = onEditBegin;
}

- (void)valdi_setOnEditEnd:(id<SCValdiFunction>)onEditEnd
{
    _onEditEnd = onEditEnd;
}

- (void)valdi_setOnReturn:(id<SCValdiFunction>)onReturn
{
    _onReturn = onReturn;
}

- (void)valdi_setOnWillDelete:(id<SCValdiFunction>)onWillDelete
{
    _onWillDelete = onWillDelete;
}

- (void)valdi_setOnSelectionChange:(id<SCValdiFunction>)onSelectionChange
{
    _onSelectionChange = onSelectionChange;
}

- (BOOL)valdi_setSelection:(NSArray *)selection
{
    if (selection.count != 2) {
        SCLogValdiError(@"Setting text selection requires a start and end point");
        return NO;
    }
    if (![selection[0] isKindOfClass:[NSNumber class]] || ![selection[1] isKindOfClass:[NSNumber class]]) {
        SCLogValdiError(@"Setting text selection requires number start and end points");
        return NO;
    }

    NSInteger offsetLimit = self.text.length;
    NSInteger offsetStart = MAX(0, MIN(offsetLimit, [selection[0] unsignedIntValue]));
    NSInteger offsetEnd = MAX(offsetStart, MIN(offsetLimit, [selection[1] unsignedIntValue]));
    UITextPosition *positionOrigin = self.beginningOfDocument;
    UITextPosition *positionStart = [self positionFromPosition:positionOrigin offset:offsetStart];
    UITextPosition *positionEnd = [self positionFromPosition:positionOrigin offset:offsetEnd];

    [self setSelectedTextRange:[self textRangeFromPosition:positionStart toPosition:positionEnd]];

    return YES;
}

- (BOOL)valdi_setPlaceholder:(nullable NSString *)placeholder color:(nullable UIColor *)color
{
    if (placeholder && color) {
        NSMutableDictionary* attr = @{NSForegroundColorAttributeName : color}.mutableCopy;
        if (self.font) {
            attr[NSFontAttributeName] = self.font;
        }

        // Ticket: 3940
        NSMutableParagraphStyle *paragraphStyle = [[NSMutableParagraphStyle alloc] init];
        NSTextAlignment resolvedTextAlignment;
        if (self.valdiContext.traitCollection.layoutDirection == UITraitEnvironmentLayoutDirectionRightToLeft) {
            resolvedTextAlignment = NSTextAlignmentRight;
        } else {
            resolvedTextAlignment = NSTextAlignmentLeft;
        }
        // TODO(simon): Should be used?
        (void)resolvedTextAlignment;
        attr[NSParagraphStyleAttributeName] = paragraphStyle;
        self.attributedPlaceholder = [[NSAttributedString alloc] initWithString:placeholder attributes:attr];
    } else {
        self.placeholder = placeholder;
    }
    return YES;
}

- (BOOL)valdi_setTextShadow:(NSArray *)textShadow
{
    return SCValdiSetTextHolderTextShadow(self, textShadow);
}

- (void) valdi_resetTextShadow
{
    SCValdiResetTextHolderTextShadow(self);
}

- (BOOL)valdi_setEnableInlinePredictions:(BOOL)enableInlinePredictions
{
    if (@available(iOS 17.0, *)) {
        self.inlinePredictionType = enableInlinePredictions ? UITextInlinePredictionTypeDefault : UITextInlinePredictionTypeNo;
    }

    return YES;
}

#pragma mark - Static methods

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    id<SCValdiFontManagerProtocol> fontManager = [attributesBinder fontManager];

    [attributesBinder bindCompositeAttribute:@"fontSpecs"
                                       parts:[NSAttributedString valdiFontAttributes]
                            withUntypedBlock:^BOOL(__kindof SCValdiTextField *textField, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
        [textField valdi_setFontManager:fontManager];
        [textField valdi_setFontAttributes:ObjectAs(attributeValue, SCValdiFontAttributes)];
        return YES;
    }
                                  resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
        [textField valdi_setFontAttributes:nil];
    }];

    [attributesBinder registerPreprocessorForAttribute:@"font" enableCache:YES withBlock:^id(id value) {
        return [SCValdiFont fontFromValdiAttribute:ObjectAs(value, NSString) fontManager:fontManager];
    }];

    [attributesBinder registerPreprocessorForAttribute:@"fontSpecs" enableCache:YES withBlock:^id(id value) {
        return [NSAttributedString fontAttributesWithCompositeValue:value];
    }];

    [attributesBinder bindAttribute:@"autocapitalization"
           invalidateLayoutOnChange:NO
                    withStringBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
                        return SCValdiTextInputSetAutocapitalization(textField, attributeValue);
                    }
                         resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
                             SCValdiTextInputSetAutocapitalization(textField, nil);
                         }];

    [attributesBinder bindAttribute:@"autocorrection"
           invalidateLayoutOnChange:NO
                    withStringBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
                        return SCValdiTextInputSetAutocorrection(textField, attributeValue);
                    }
                         resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
                             SCValdiTextInputSetAutocorrection(textField, nil);
                         }];

    [attributesBinder bindAttribute:@"keyboardAppearance"
           invalidateLayoutOnChange:NO
                    withStringBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
                        return SCValdiTextInputSetKeyboardAppearance(textField, attributeValue);
                    }
                         resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
                             SCValdiTextInputSetKeyboardAppearance(textField, nil);
                         }];

    [attributesBinder bindAttribute:@"contentType"
           invalidateLayoutOnChange:NO
                    withStringBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
                        return SCValdiTextInputSetContentType(textField, attributeValue);
                    }
                         resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
                             SCValdiTextInputSetContentType(textField, nil);
                         }];

    [attributesBinder bindAttribute:@"returnKeyText"
           invalidateLayoutOnChange:NO
                    withStringBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
                        return SCValdiTextInputSetReturnKeyText(textField, attributeValue);
                    }
                         resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
                             SCValdiTextInputSetReturnKeyText(textField, nil);
                         }];

    [attributesBinder bindAttribute:@"characterLimit"
        invalidateLayoutOnChange:YES
        withIntBlock:^BOOL(SCValdiTextField *textField, NSInteger attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setCharacterLimit:@(attributeValue)];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setCharacterLimit:nil];
        }];

    [attributesBinder bindAttribute:@"closesWhenReturnKeyPressed"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextField *textField, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setClosesWhenReturnKeyPressed:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setClosesWhenReturnKeyPressed:YES];
        }];

    [attributesBinder bindAttribute:@"enabled"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextField *textField, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setEnabled:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setEnabled:YES];
        }];

    [attributesBinder bindAttribute:@"focused"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextField *textField, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setFocused:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setFocused:NO];
        }];

    [attributesBinder bindAttribute:@"tintColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(SCValdiTextField *textField, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setTintColor:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setTintColor:nil];
        }];

    [attributesBinder bindAttribute:@"selectTextOnFocus"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextField *textField, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setSelectTextOnFocus:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setSelectTextOnFocus:NO];
        }];

    [attributesBinder bindAttribute:@"onWillChange"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnWillChange:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnWillChange:nil];
        }];

    [attributesBinder bindAttribute:@"onChange"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnChange:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnChange:nil];
        }];

    [attributesBinder bindAttribute:@"onEditBegin"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnEditBegin:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnEditBegin:nil];
        }];

    [attributesBinder bindAttribute:@"onEditEnd"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnEditEnd:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnEditEnd:nil];
        }];

    [attributesBinder bindAttribute:@"onReturn"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnReturn:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnReturn:nil];
        }];

    [attributesBinder bindAttribute:@"onWillDelete"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnWillDelete:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnWillDelete:nil];
        }];

    [attributesBinder bindCompositeAttribute:@"placeholderAttributes"
        parts:[SCValdiTextField _valdiPlaceholderComponents]
        withUntypedBlock:^BOOL(SCValdiTextField *textField, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            NSArray *attributeValueArray = ObjectAs(attributeValue, NSArray);
            if (attributeValueArray.count != 2) {
                return NO;
            }

            NSString *value = ObjectAs(attributeValueArray[0], NSString);
            NSNumber *colorInt = ObjectAs(attributeValueArray[1], NSNumber);
            UIColor *color = colorInt ? UIColorFromValdiAttributeValue(colorInt.integerValue) : nil;

            [textField valdi_setFontManager:fontManager];
            return [textField valdi_setPlaceholder:value color:color];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setPlaceholder:nil color:nil];
        }];

    [attributesBinder bindAttribute:@"selection"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(__kindof SCValdiTextField *textField, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setSelection:attributeValue];
        }
        resetBlock:^(__kindof SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setSelection:@[]];
        }];

    [attributesBinder bindAttribute:@"onSelectionChange"
        withFunctionBlock:^(SCValdiTextField *textField, id<SCValdiFunction> attributeValue) {
            [textField valdi_setOnSelectionChange:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField) {
            [textField valdi_setOnSelectionChange:nil];
        }];

    [attributesBinder bindAttribute:@"textShadow"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(__kindof SCValdiTextField *textField, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setTextShadow:attributeValue];
        }
        resetBlock:^(__kindof SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_resetTextShadow];
        }];

    [attributesBinder bindAttribute:@"value"
        invalidateLayoutOnChange:YES
                      withTextBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setFontManager:fontManager];
            SCValdiAnimatorTransitionWrap(animator, textField, { [textField valdi_setValue:attributeValue]; });
            return YES;
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            SCValdiAnimatorTransitionWrap(animator, textField, { [textField valdi_setValue:nil]; });
        }];

    [attributesBinder setPlaceholderViewMeasureDelegate:^UIView *{
        return [SCValdiTextField new];
    }];

    [attributesBinder bindAttribute:@"enableInlinePredictions"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextField *textField, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textField valdi_setEnableInlinePredictions:attributeValue];
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            [textField valdi_setEnableInlinePredictions:NO];
        }];

    [attributesBinder bindAttribute:@"textDirection"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextField *textField, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return SCValdiTextInputSetTextDirection(textField, attributeValue);
        }
        resetBlock:^(SCValdiTextField *textField, id<SCValdiAnimatorProtocol> animator) {
            SCValdiTextInputSetTextDirection(textField, nil);
        }];
}

#pragma mark - Private property methods

+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)_valdiPlaceholderComponents
{
    return @[
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"placeholder"
                                                                type:SCNValdiCoreAttributeTypeString
                                                            optional:YES
                                            invalidateLayoutOnChange:YES],
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"placeholderColor"
                                                                type:SCNValdiCoreAttributeTypeColor
                                                            optional:YES
                                            invalidateLayoutOnChange:NO],
    ];
}

#pragma mark - UITextFieldDelegate implementation

- (BOOL)textField:(UITextField *)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString *)string
{
    // Make sure we're not out of bounds (which can happen in certain copy-pasting scenarios)
    if (range.location + range.length > textField.text.length) {
        return NO;
    }
    // Set the text to the clamped value if it violates formatting rules
    NSString *newText = [textField.text stringByReplacingCharactersInRange:range withString:string];
    NSString *clampedText = SCValdiClampTextValueChanged(textField.text, string, range, [_characterLimit integerValue], false);
    if (![newText isEqualToString:clampedText]) {
        textField.text = clampedText;
        return NO;
    }
    // Otherwise, good to go
    return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    if (_closesWhenReturnKeyPressed) {
        _lastUnfocusReason = SCValdiTextInputUnfocusReasonReturnKeyPress;
        [textField resignFirstResponder];
    }
    SCValdiCallEvent(_onReturn, self);
    return YES;
}

- (void)textFieldDidBeginEditing:(UITextField *)textField
{
    id<SCValdiViewNodeProtocol> viewNode = self.valdiViewNode;
    id<SCValdiContextProtocol> context = self.valdiContext;
    [context didChangeValue:@YES forInternedValdiAttribute:SCValdiTextFieldFocusedKey() inViewNode:viewNode];

    // OnEditBegin event
    _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;
    SCValdiCallEvent(_onEditBegin, self);

    // Post-focus auto-select
    if (_selectTextOnFocus) {
        // Without dispatch_async, `selectAll` only works every other call.
        // There are other parts in the app where we do this as well.
        dispatch_async(dispatch_get_main_queue(), ^{
            [textField selectAll:nil];
        });
    }
}

- (void)textFieldDidEndEditing:(UITextField *)textField
{
    id<SCValdiViewNodeProtocol> viewNode = self.valdiViewNode;
    id<SCValdiContextProtocol> context = self.valdiContext;
    [context didChangeValue:@NO forInternedValdiAttribute:SCValdiTextFieldFocusedKey() inViewNode:viewNode];

    // OnEditEnd event
    SCValdiCallEventWithReason(_onEditEnd, self, _lastUnfocusReason);
    _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;
}

@end
