//
//  SCValdiTextView.m
//  Valdi
//
//  Created by Andrew Lin on 11/6/18.
//

#import "valdi/ios/Views/SCValdiTextView.h"
#import "valdi/ios/Views/SCValdiTextViewEffectsLayoutManager.h"

#import "valdi/ios/Categories/UIView+Valdi.h"

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi/ios/Text/SCValdiFontAttributes.h"
#import "valdi/ios/Text/SCValdiFont.h"
#import "valdi/ios/Text/SCValdiAttributedTextHelper.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/SCValdiOnTapAttribute.h"
#import "valdi/ios/Text/SCValdiTextGradientHelper.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"

#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/SCValdiTextInputTraitAttributes.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiResult.h"
#import "valdi_core/SCValdiConfigurableTextHolder.h"
#import "valdi_core/SCValdiConfigurableTextHolderTraitAttributes.h"
#import "valdi_core/SCValdiTextInputUnfocusReason.h"
#import "valdi/ios/Text/SCValdiOnLayoutAttribute.h"
#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"

static NSString *const kTextGradientLayoutKey = @"text_gradient";

@interface SCValdiTextKit1TextView : UITextView<SCValdiConfigurableTextHolder, SCValdiTextHolder>
- (instancetype)initWithFrame:(CGRect)frame layoutManager:(NSLayoutManager *)layoutManager NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame textContainer:(NSTextContainer *)textContainer NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
@end

@implementation SCValdiTextKit1TextView {
    NSTextStorage *_valdiTextStorage;
    NSTextContainer *_valdiTextContainer;
    NSLayoutManager *_valdiLayoutManager;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    return [self initWithFrame:frame layoutManager:[NSLayoutManager new]];
}

- (instancetype)initWithFrame:(CGRect)frame layoutManager:(NSLayoutManager *)layoutManager
{
    NSTextStorage *textStorage = [[NSTextStorage alloc] init];
    [textStorage addLayoutManager:layoutManager];
    NSTextContainer *textContainer = [[NSTextContainer alloc] init];
    [layoutManager addTextContainer:textContainer];

    if (self = [super initWithFrame:frame textContainer:textContainer]) {
        _valdiTextStorage = textStorage;
        _valdiTextContainer = textContainer;
        _valdiLayoutManager = layoutManager;
    }
    return self;
}
@end

@interface SCValdiTextViewPlaceholder : SCValdiTextKit1TextView
@end

@implementation SCValdiTextViewPlaceholder
@end

@interface SCValdiTextViewInternal : SCValdiTextKit1TextView
@end

@implementation SCValdiTextViewInternal
@end

static NSString* const kSCValdiTextViewContentSizeKey = @"contentSize";
static CGFloat const SCValdiAnimatedTextOverlayPadding = 8.0;

typedef NS_ENUM(NSUInteger, SCValdiTextViewTextGravity) {
    SCValdiTextViewTextGravityTop,
    SCValdiTextViewTextGravityCenter,
    SCValdiTextViewTextGravityBottom,
};

static BOOL SCValdiAttributedStringHasAnimationTransform(NSAttributedString *attributedString)
{
    if (attributedString.length == 0) {
        return NO;
    }

    __block BOOL hasAnimationTransform = NO;
    [attributedString enumerateAttribute:kSCValdiAttributedStringKeyAnimationTransform
                                 inRange:NSMakeRange(0, attributedString.length)
                                 options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                              usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (value != nil && range.length > 0) {
            hasAnimationTransform = YES;
            *stop = YES;
        }
    }];
    return hasAnimationTransform;
}

static CGFloat SCValdiAnimatedTextVerticalOverflowPadding(NSAttributedString *attributedString)
{
    if (attributedString.length == 0) {
        return 0.0;
    }

    __block CGFloat maxTranslation = 0.0;
    __block CGFloat maxScaleOverflow = 0.0;
    [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length)
                                         options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                      usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attrs, NSRange range, BOOL *stop) {
        NSDictionary<NSString *, NSNumber *> *animationTransform = attrs[kSCValdiAttributedStringKeyAnimationTransform];
        if (![animationTransform isKindOfClass:[NSDictionary class]]) {
            return;
        }

        maxTranslation = MAX(maxTranslation, fabs(animationTransform[@"translationY"].doubleValue));
        CGFloat scale = animationTransform[@"scale"] != nil ? animationTransform[@"scale"].doubleValue : 1.0;
        UIFont *font = attrs[NSFontAttributeName];
        CGFloat lineHeight = [font isKindOfClass:[UIFont class]] ? font.lineHeight : 0.0;
        maxScaleOverflow = MAX(maxScaleOverflow, MAX(fabs(scale) - 1.0, 0.0) * lineHeight * 0.5);
    }];

    return ceil(maxTranslation + maxScaleOverflow + SCValdiAnimatedTextOverlayPadding);
}

static NSAttributedString *SCValdiBackgroundOnlyAttributedString(NSAttributedString *attributedString)
{
    NSMutableAttributedString *backgroundOnlyAttributedString = [attributedString mutableCopy];
    NSRange fullRange = NSMakeRange(0, backgroundOnlyAttributedString.length);
    [backgroundOnlyAttributedString addAttribute:NSForegroundColorAttributeName value:[UIColor clearColor] range:fullRange];
    [backgroundOnlyAttributedString addAttribute:NSStrokeColorAttributeName value:[UIColor clearColor] range:fullRange];
    [backgroundOnlyAttributedString addAttribute:kSCValdiOuterOutlineColorAttribute value:[UIColor clearColor] range:fullRange];
    [backgroundOnlyAttributedString removeAttribute:kSCValdiAttributedStringKeyAnimationTransform range:fullRange];
    return backgroundOnlyAttributedString;
}

@interface SCValdiTextView() <SCValdiAttributedTextOnTapGestureRecognizerFunctionProvider>

@end

@implementation SCValdiTextView {
    /// YES if pressing the return key should dismiss the keyboard, o/w NO
    BOOL _closesWhenReturnKeyPressed;
    /// The maximum length of the text
    NSNumber *_characterLimit;
    /// YES if all text should be selected on begin editing
    BOOL _selectTextOnFocus;
    /// YES if we discard any typed newline
    BOOL _ignoreNewlines;
    BOOL _updating;
    BOOL _updateOnLayout;
    /// The vertical gravity of the text
    SCValdiTextViewTextGravity _textGravity;
    id<SCValdiFontManagerProtocol> _fontManager;
    SCValdiTextViewBackgroundEffects *_backgroundEffects;

    id<SCValdiFunction> _Nullable _onWillChange;
    id<SCValdiFunction> _Nullable _onChange;
    id<SCValdiFunction> _Nullable _onEditBegin;
    id<SCValdiFunction> _Nullable _onEditEnd;
    id<SCValdiFunction> _Nullable _onReturn;
    id<SCValdiFunction> _Nullable _onWillDelete;
    id<SCValdiFunction> _Nullable _onSelectionChange;

    SCValdiTextViewPlaceholder *_placeholder;
    SCValdiTextViewInternal *_textView;
    SCValdiTextViewEffectsLayoutManager *_effectsLayoutManager;
    NSAttributedString *_attributedTextOnTapString;
    BOOL _hasOnTapGestureRecognizer;
    SCValdiCustomUnderlineStyle *_customUnderlineStyle;
    BOOL _hasCustomUnderlineAttribute;
    BOOL _hasTextOverflow;
    NSLineBreakMode _textOverflowLineBreakMode;
    SCValdiTextGradientHelper *_textGradientHelper;

    SCValdiTextInputUnfocusReason _lastUnfocusReason;

    // State for the animated text overlay used when per-glyph transforms are present.
    SCValdiTextViewInternal *_animatedTextView;
    SCValdiTextViewEffectsLayoutManager *_animatedTextEffectsLayoutManager;
    CGFloat _animatedTextVerticalOverflowPadding;
    BOOL _slowClipping;

}

- (void)valdi_applySlowClipping:(BOOL)slowClipping animator:(id<SCValdiAnimatorProtocol> )animator
{
    _slowClipping = slowClipping;
    _textView.clipsToBounds = slowClipping;
    _animatedTextView.clipsToBounds = slowClipping;
    _animatedTextView.layer.masksToBounds = slowClipping;
}

- (id)initWithFrame:(CGRect)frame
{
    if (self = [super initWithFrame:frame]) {
        // Valdi text views depend on TextKit 1 APIs for zero line padding, custom effects, and per-glyph drawing.
        // Build that text system up front so UIKit never has to switch a TextKit 2 UITextView into compatibility mode.
        _effectsLayoutManager = [SCValdiTextViewEffectsLayoutManager new];
        _textView = [[SCValdiTextViewInternal alloc] initWithFrame:frame layoutManager:_effectsLayoutManager];

        _textView.delegate = self;
        _textView.textStorage.delegate = self;
        _textView.backgroundColor = [UIColor clearColor];
        _textView.scrollEnabled = YES;
        _textView.textContainerInset = UIEdgeInsetsZero;
        _textView.textContainer.lineFragmentPadding = 0;
        _textView.showsHorizontalScrollIndicator = NO;
        _textView.adjustsFontForContentSizeCategory = NO;
        if (@available(iOS 17.0, *)) {
            _textView.inlinePredictionType = UITextInlinePredictionTypeNo;
        }

        _animatedTextEffectsLayoutManager = [SCValdiTextViewEffectsLayoutManager new];
        _animatedTextView = [[SCValdiTextViewInternal alloc] initWithFrame:frame
                                                             layoutManager:_animatedTextEffectsLayoutManager];
        _animatedTextView.backgroundColor = [UIColor clearColor];
        _animatedTextView.userInteractionEnabled = NO;
        _animatedTextView.isAccessibilityElement = NO;
        _animatedTextView.accessibilityElementsHidden = YES;
        _animatedTextView.editable = NO;
        _animatedTextView.scrollEnabled = YES;
        _animatedTextView.textContainerInset = UIEdgeInsetsZero;
        _animatedTextView.textContainer.lineFragmentPadding = 0;
        _animatedTextView.showsHorizontalScrollIndicator = NO;
        _animatedTextView.adjustsFontForContentSizeCategory = NO;
        _animatedTextView.hidden = YES;

        [self addSubview:_textView];
        [self addSubview:_animatedTextView];

        _placeholder = [[SCValdiTextViewPlaceholder alloc] initWithFrame:frame];
        _placeholder.textColor = [UIColor lightGrayColor];
        _placeholder.userInteractionEnabled = NO;
        _placeholder.backgroundColor = [UIColor clearColor];
        _placeholder.textContainerInset = UIEdgeInsetsZero;
        _placeholder.textContainer.lineFragmentPadding = 0;
        _placeholder.showsHorizontalScrollIndicator = NO;
        _placeholder.adjustsFontForContentSizeCategory = NO;

        [self addSubview:_placeholder];

        [_textView addObserver:self
                    forKeyPath:kSCValdiTextViewContentSizeKey
                       options:(NSKeyValueObservingOptionNew)
                    context:NULL];

        _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;
        _textGravity = SCValdiTextViewTextGravityCenter;

        _textMode = SCValdiTextModeText;
        _needAttributedTextUpdate = YES;
        _textOverflowLineBreakMode = NSLineBreakByWordWrapping;
        _textValue = nil;
    }

    return self;
}

- (void)dealloc
{
    [_textView removeObserver:self forKeyPath:kSCValdiTextViewContentSizeKey];
    _textView.delegate = nil;
    _textView.textStorage.delegate = nil;
}

#pragma mark - UIView+Valdi

- (BOOL)willEnqueueIntoValdiPool
{
    [_textView unmarkText];
    [_textView resignFirstResponder];
    _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;

    return self.class == [SCValdiTextView class];
}

-(void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    [self _updateContentInset];
}

- (void)layoutSubviews
{
    if ([_textGradientHelper layoutIfNeededInView:self animator:nil]) {
        _needAttributedTextUpdate = YES;
    }
    [self _updateTextGradientColorIfNeeded];
    [self _updateAttributedTextIfNeeded];
    [super layoutSubviews];

    [self _updateFrame];
    [self _updateContentInset];
    [self _updatePlaceholderInset];
    [self _updateOnLayoutIfNeeded];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    if (scrollView == _textView) {
        _animatedTextView.contentOffset = scrollView.contentOffset;
    }
}

- (void)_updateFrame
{
    // necessary to handle that on one line with higher heights, setting frame actually causes an adjustment in scroll slightly by a pixel (25.666 => 26 ex)
    if (!CGRectEqualToRect(_placeholder.frame, self.bounds)) {
        _placeholder.frame = self.bounds;
    }
    if (!CGRectEqualToRect(_textView.frame, self.bounds)) {
        _textView.frame = self.bounds;
        _updateOnLayout = YES;
    }

    CGRect animatedTextBounds = CGRectMake(self.bounds.origin.x,
                                           self.bounds.origin.y - _animatedTextVerticalOverflowPadding,
                                           self.bounds.size.width,
                                           self.bounds.size.height + (_animatedTextVerticalOverflowPadding * 2.0));
    if (!CGRectEqualToRect(_animatedTextView.frame, animatedTextBounds)) {
        _animatedTextView.frame = animatedTextBounds;
    }
}

- (CGSize)sizeThatFits:(CGSize)size
{
    [self _updateAttributedTextIfNeeded];
    return [self.class measureSizeWithMaxSize:size
                               fontAttributes:[self fontAttributes]
                                  fontManager:_fontManager
                                         text:_textValue
                                  placeholder:_placeholder.text
                              traitCollection:self.valdiContext.traitCollection];
}

- (void)_updateTextViewInset:(UITextView *)textView
{
    CGFloat boundsHeight = textView.bounds.size.height;
    CGFloat contentSizeHeight = textView.contentSize.height;
    CGFloat topCorrection;

    switch (_textGravity) {
        case SCValdiTextViewTextGravityTop:
            topCorrection = 0.0;
            break;
        case SCValdiTextViewTextGravityBottom:
            topCorrection = boundsHeight - contentSizeHeight;
            break;
        case SCValdiTextViewTextGravityCenter:
        default:
            topCorrection = (boundsHeight - contentSizeHeight) / 2.0;
            break;
    }

    topCorrection = (topCorrection < 0.0 ? 0.0 : topCorrection);

    [textView setContentInset:UIEdgeInsetsMake(topCorrection, 0, 0, 0)];
}

- (void)_updateContentInset
{
    [self _updateTextViewInset:(_textView)];
    [self _updateTextViewInset:(_animatedTextView)];
}

- (void)_updatePlaceholderInset
{
    [self _updateTextViewInset:(_placeholder)];
}

- (void)_updateOnLayoutIfNeeded
{
    if (!_updateOnLayout) {
        return;
    }

    NSAttributedString *attributedString = _textView.attributedText;
    if (!attributedString) {
        return;
    }
    UITextView *textView = _textView;

    [attributedString enumerateAttribute:kSCValdiAttributedStringKeyOnLayout
                                                inRange:NSMakeRange(0, attributedString.length)
                                                options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                            usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
                    if (!value) {
                        return;
                    }
                    SCValdiOnLayoutAttribute *attribute = value;
                    UITextPosition *startPosition = [textView positionFromPosition:textView.beginningOfDocument offset:range.location];
                    UITextPosition *endPosition = [textView positionFromPosition:startPosition offset:range.length];
                    UITextRange *textRange = [textView textRangeFromPosition:startPosition toPosition:endPosition];
                    CGRect newBounds = [textView firstRectForRange:textRange];
                    SCValdiMarshallerScoped(marshaller, {
                        SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.x + textView.contentInset.left));
                        SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.y + textView.contentInset.top));
                        SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.width));
                        SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.height));
                        [attribute.callback performWithMarshaller:marshaller];
                    });
                }];

    _updateOnLayout = NO;
}

- (void)_updateEffectsLayoutManager
{
    _effectsLayoutManager.effects = _backgroundEffects;
    _effectsLayoutManager.customUnderlineStyle = _customUnderlineStyle;
    _textView.textContainer.lineFragmentPadding = _effectsLayoutManager.backgroundPadding;
    CGFloat textContainerVerticalInset = _effectsLayoutManager.backgroundPadding / 2.0;
    _textView.textContainerInset = UIEdgeInsetsMake(textContainerVerticalInset, 0, textContainerVerticalInset, 0);

    // The base text view already draws background effects. The overlay should only paint transformed glyphs.
    _animatedTextEffectsLayoutManager.effects = nil;
    _animatedTextEffectsLayoutManager.customUnderlineStyle = nil;
    _animatedTextView.textContainer.lineFragmentPadding = _textView.textContainer.lineFragmentPadding;
    _animatedTextView.textContainer.maximumNumberOfLines = _textView.textContainer.maximumNumberOfLines;
    _animatedTextView.textContainer.lineBreakMode = _textView.textContainer.lineBreakMode;
    UIEdgeInsets animatedBaseInset = _textView.textContainerInset;
    _animatedTextView.textContainerInset = UIEdgeInsetsMake(animatedBaseInset.top + _animatedTextVerticalOverflowPadding,
                                                            animatedBaseInset.left,
                                                            animatedBaseInset.bottom + _animatedTextVerticalOverflowPadding,
                                                            animatedBaseInset.right);

    // Mark the textview to display again as the layout manager can get cached for only a color change
    [_textView setNeedsDisplay];
    [_animatedTextView setNeedsDisplay];
}

- (BOOL)_shouldUseAnimatedTextOverlayForAttributedString:(NSAttributedString *)attributedString
{
    return SCValdiAttributedStringHasAnimationTransform(attributedString);
}

- (void)_updateAnimatedTextOverlayWithAttributedString:(NSAttributedString *)attributedString
                                             isEnabled:(BOOL)isEnabled
{
    _animatedTextVerticalOverflowPadding =
        isEnabled && attributedString != nil ? SCValdiAnimatedTextVerticalOverflowPadding(attributedString) : 0.0;
    _animatedTextView.hidden = !isEnabled;
    _animatedTextView.backgroundColor = [UIColor clearColor];
    _animatedTextView.textContainer.lineFragmentPadding = _textView.textContainer.lineFragmentPadding;
    _animatedTextView.textContainer.maximumNumberOfLines = _textView.textContainer.maximumNumberOfLines;
    _animatedTextView.textContainer.lineBreakMode = _textView.textContainer.lineBreakMode;
    UIEdgeInsets baseInset = _textView.textContainerInset;
    _animatedTextView.textContainerInset = UIEdgeInsetsMake(baseInset.top + _animatedTextVerticalOverflowPadding,
                                                            baseInset.left,
                                                            baseInset.bottom + _animatedTextVerticalOverflowPadding,
                                                            baseInset.right);
    _animatedTextView.clipsToBounds = _slowClipping;
    _animatedTextView.layer.masksToBounds = _slowClipping;

    if (isEnabled) {
        [self bringSubviewToFront:_animatedTextView];
    }

    [self _updateFrame];
    _animatedTextView.attributedText = isEnabled ? attributedString : nil;
}

- (void)_applyTextOverflowAttributes
{
    NSLineBreakMode lineBreakMode = _textOverflowLineBreakMode;
    BOOL shouldScroll = !_hasTextOverflow;

    _textView.scrollEnabled = shouldScroll && _textView.editable;
    _textView.textContainer.lineBreakMode = lineBreakMode;

    _placeholder.scrollEnabled = shouldScroll;
    _placeholder.textContainer.lineBreakMode = lineBreakMode;

    _animatedTextView.scrollEnabled = shouldScroll;
    _animatedTextView.textContainer.lineBreakMode = lineBreakMode;
}

- (void)_applyNumberOfLinesAttributes
{
    NSInteger numberOfLines = [self fontAttributes].numberOfLines;
    _textView.textContainer.maximumNumberOfLines = numberOfLines;
    _placeholder.textContainer.maximumNumberOfLines = numberOfLines;
    _animatedTextView.textContainer.maximumNumberOfLines = numberOfLines;
}

- (id<SCValdiFunction>)onTapFunctionAtLocation:(CGPoint)location
{
    CGPoint textViewLocation = [self convertPoint:location toView:_textView];
    if (!CGRectContainsPoint(_textView.bounds, textViewLocation)) {
        return nil;
    }

    NSAttributedString *attributedString = _attributedTextOnTapString;
    if (!attributedString || attributedString.length == 0) {
        return nil;
    }

    UITextRange *textRange = [_textView characterRangeAtPoint:textViewLocation];
    UITextPosition *textPosition = textRange.start;
    if (!textPosition) {
        return nil;
    }

    NSInteger characterOffset = [_textView offsetFromPosition:_textView.beginningOfDocument
                                                   toPosition:textPosition];
    if (characterOffset < 0) {
        return nil;
    }

    NSUInteger characterIndex = (NSUInteger)characterOffset;
    if (characterIndex >= attributedString.length) {
        return nil;
    }

    NSRange onTapRange = NSMakeRange(0, 0);
    SCValdiOnTapAttribute *onTapAttribute = [attributedString attribute:kSCValdiAttributedStringKeyOnTap
                                                                atIndex:characterIndex
                                                  longestEffectiveRange:&onTapRange
                                                                inRange:NSMakeRange(0, attributedString.length)];
    if (!onTapAttribute.callback || onTapRange.length == 0) {
        return nil;
    }

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
        attributedOnTapGestureRecognizer.cannotBePreventedByOtherGestureRecognizers = YES;
        _hasOnTapGestureRecognizer = YES;
        [self addGestureRecognizer:attributedOnTapGestureRecognizer];
        attributedOnTapGestureRecognizer.functionProvider = self;
    }
}


#pragma mark - Action handling methods

INTERNED_STRING_CONST("focused", SCValdiTextViewFocusedKey);
INTERNED_STRING_CONST("value", SCValdiTextViewValueKey);
INTERNED_STRING_CONST("text", SCValdiTextViewTextKey);
INTERNED_STRING_CONST("selection", SCValdiTextViewSelectionKey);
INTERNED_STRING_CONST("selectionStart", SCValdiTextViewSelectionStartKey);
INTERNED_STRING_CONST("selectionEnd", SCValdiTextViewSelectionEndKey);
INTERNED_STRING_CONST("reason", SCValdiTextViewReasonKey);

static NSInteger SCValdiMarshallEditTextEvent(SCValdiMarshallerRef marshaller, UITextView *textView) {
    UITextPosition *origin = textView.beginningOfDocument;
    NSInteger objectIndex = SCValdiMarshallerPushMap(marshaller, 1);
    SCValdiMarshallerPushString(marshaller, textView.text ?: @"");
    SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextViewTextKey(), objectIndex);
    SCValdiMarshallerPushInt(marshaller, (int32_t)[textView offsetFromPosition:origin toPosition:textView.selectedTextRange.start]);
    SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextViewSelectionStartKey(), objectIndex);
    SCValdiMarshallerPushInt(marshaller, (int32_t)[textView offsetFromPosition:origin toPosition:textView.selectedTextRange.end]);
    SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextViewSelectionEndKey(), objectIndex);
    return objectIndex;
}

static void SCValdiCallEvent(id<SCValdiFunction> function, UITextView *textView)
{
    if (!function) {
        return;
    }
    SCValdiMarshallerScoped(marshaller, {
        SCValdiMarshallEditTextEvent(marshaller, textView);
        [function performWithMarshaller:marshaller];
    });
}

static void SCValdiCallEventWithReason(id<SCValdiFunction> function, UITextView *textView, NSInteger reasonId)
{
    if (!function) {
        return;
    }
    SCValdiMarshallerScoped(marshaller, {
        NSInteger objectIndex = SCValdiMarshallEditTextEvent(marshaller, textView);
        SCValdiMarshallerPushDouble(marshaller, reasonId);
        SCValdiMarshallerPutMapProperty(marshaller, SCValdiTextViewReasonKey(), objectIndex);
       [function performWithMarshaller:marshaller];
    });
}

#pragma mark - text value control

- (void)notifyTextValueDidChange
{
    [self.valdiContext didChangeValue:_textView.text ?: @""
                    forInternedValdiAttribute:SCValdiTextViewValueKey()
                              inViewNode:self.valdiViewNode];
    SCValdiCallEvent(_onChange, _textView);
}

#pragma mark - AttributedString management

- (BOOL)updateLabelMode:(SCValdiTextMode)labelMode
{
    return SCValdiUpdateLabelMode(self, _textView, labelMode);
}

- (BOOL)_needAttributedString
{
    return SCValdiNeedAttributedString(self, [self fontAttributes]);
}

- (NSAttributedString *)_displayAttributedStringForAttributedString:(NSAttributedString *)attributedString
                                                    fontAttributes:(SCValdiFontAttributes *)fontAttributes
{
    _hasCustomUnderlineAttribute = NO;
    if (!_customUnderlineStyle || attributedString.length == 0) {
        return attributedString;
    }

    UIColor *fallbackColor = _textGradientHelper.gradientColor ?: fontAttributes.color ?: [UIColor blackColor];
    return SCValdiAttributedStringByReplacingNativeUnderlinesWithColorAttribute(
        attributedString,
        kSCValdiTextViewCustomUnderlineColorAttribute,
        fallbackColor,
        &_hasCustomUnderlineAttribute);
}

- (NSDictionary<NSAttributedStringKey, id> *)_textAttributesForFontAttributes:(SCValdiFontAttributes *)fontAttributes
                                                               isRightToLeft:(BOOL)isRightToLeft
                                                             traitCollection:(UITraitCollection *)traitCollection
{
    NSDictionary<NSAttributedStringKey, id> *attributes = [fontAttributes resolveAttributesWithIsRightToLeft:isRightToLeft
                                                                                              traitCollection:traitCollection];
    UIColor *gradientColor = _textGradientHelper.gradientColor;
    if (!gradientColor) {
        return attributes;
    }

    NSMutableDictionary<NSAttributedStringKey, id> *gradientAttributes = [attributes mutableCopy];
    gradientAttributes[NSForegroundColorAttributeName] = gradientColor;
    return [gradientAttributes copy];
}

- (NSAttributedString *)_attributedStringByApplyingTextGradientColorIfNeeded:(NSAttributedString *)attributedString
{
    UIColor *gradientColor = _textGradientHelper.gradientColor;
    if (!gradientColor || attributedString.length == 0) {
        return attributedString;
    }

    NSMutableAttributedString *gradientAttributedString = [attributedString mutableCopy];
    [gradientAttributedString addAttribute:NSForegroundColorAttributeName
                                     value:gradientColor
                                     range:NSMakeRange(0, gradientAttributedString.length)];
    return gradientAttributedString;
}

- (BOOL)_updateAttributedTextIfNeeded
{
    BOOL changed = NO;
    _updating = YES;

    if (_needAttributedTextUpdate) {
        _needAttributedTextUpdate = NO;
        // Even if there is no change, we update the rendering, in case the textView.text was silently updated

        BOOL isRightToLeft = self.valdiViewNode.isRightToLeft;
        UITraitCollection *traitCollection = self.valdiContext.traitCollection;

        SCValdiFontAttributes *fontAttributes = [self fontAttributes];

        if ([self _needAttributedString]) {
            NSRange range = _textView.selectedRange;
            BOOL labelModeChanged = [self updateLabelMode:SCValdiTextModeAttributedText];

            NSAttributedString *attributedString = [NSAttributedString attributedStringWithValdiText:_textValue
                                                                                             attributes:[self _textAttributesForFontAttributes:fontAttributes
                                                                                                                                 isRightToLeft:isRightToLeft
                                                                                                                               traitCollection:traitCollection]
                                                                                          isRightToLeft:isRightToLeft
                                                                                            fontManager:_fontManager
                                                                                        traitCollection:traitCollection];
            attributedString = [self _attributedStringByApplyingTextGradientColorIfNeeded:attributedString];

            __block BOOL hasOnLayoutAttribute = NO;
            __block BOOL hasOnTapAttribute = NO;
            __block BOOL needsEffectsLayoutManager = NO;
            [attributedString enumerateAttribute:kSCValdiAttributedStringKeyOnLayout
                                         inRange:NSMakeRange(0, attributedString.length)
                                         options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                      usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
                if (value) {
                    *stop = YES;
                    hasOnLayoutAttribute = YES;
                }
            }];
            _updateOnLayout = hasOnLayoutAttribute;

            [attributedString enumerateAttribute:kSCValdiAttributedStringKeyOnTap
                                         inRange:NSMakeRange(0, attributedString.length)
                                         options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                      usingBlock:^(id  _Nullable value, NSRange range, BOOL * _Nonnull stop) {
                if (value) {
                    *stop = YES;
                    hasOnTapAttribute = YES;
                }
            }];

            [attributedString enumerateAttribute:kSCValdiOuterOutlineWidthAttribute 
                                        inRange:NSMakeRange(0, attributedString.length) 
                                        options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired 
                                        usingBlock:^(id value, NSRange range, BOOL *stop) {
                if (value) {
                    *stop = YES;
                    needsEffectsLayoutManager = YES;
                }
            }];

            [attributedString enumerateAttribute:kSCValdiAttributedStringKeyAnimationTransform
                                        inRange:NSMakeRange(0, attributedString.length)
                                        options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired
                                     usingBlock:^(id value, NSRange range, BOOL *stop) {
                if (value) {
                    *stop = YES;
                    needsEffectsLayoutManager = YES;
                }
            }];

            NSAttributedString *trimmedString = SCValdiClampAttributedStringValue(attributedString, [_characterLimit integerValue], _ignoreNewlines);
            if (![attributedString isEqualToAttributedString:trimmedString]) {
                changed = YES;
            }

            NSAttributedString *displayAttributedString = [self _displayAttributedStringForAttributedString:trimmedString
                                                                                             fontAttributes:fontAttributes];
            if (_hasCustomUnderlineAttribute) {
                needsEffectsLayoutManager = YES;
            }

            if (needsEffectsLayoutManager) {
                [self _updateEffectsLayoutManager];
            }

            BOOL useAnimatedTextOverlay = [self _shouldUseAnimatedTextOverlayForAttributedString:displayAttributedString];
            NSAttributedString *textViewAttributedString =
                useAnimatedTextOverlay ? SCValdiBackgroundOnlyAttributedString(displayAttributedString) : displayAttributedString;
            
            // Cursor position should be updated if it's not at the end of the string
            BOOL updateCursorPosition = range.location != _textView.attributedText.string.length && range.location < displayAttributedString.length;
            if (![_textView.attributedText isEqualToAttributedString:textViewAttributedString] || labelModeChanged) {
                _textView.attributedText = textViewAttributedString;
                if (updateCursorPosition) {
                    [self _applySelectionStart:range.location selectionEnd:range.location + range.length];
                }
            }
            [self _updateAnimatedTextOverlayWithAttributedString:displayAttributedString isEnabled:useAnimatedTextOverlay];

            if (hasOnTapAttribute) {
                _attributedTextOnTapString = displayAttributedString;
                [self _addAttributedTextOnTapGestureRecognizer];
            } else {
                _attributedTextOnTapString = nil;
                [self _removeAttributedTextOnTapGestureRecognizer];
            }

            _placeholder.hidden = _textView.attributedText.length > 0;
        } else {
            [self updateLabelMode:SCValdiTextModeText];;
            SCValdiSetTextHolderAttributes(_textView, fontAttributes, traitCollection, isRightToLeft, _textGradientHelper.gradientColor ?: fontAttributes.color);
            [self _updateAnimatedTextOverlayWithAttributedString:nil isEnabled:NO];
            _attributedTextOnTapString = nil;
            [self _removeAttributedTextOnTapGestureRecognizer];
            _hasCustomUnderlineAttribute = NO;

            NSString *value = SCValdiClampTextValue(_textValue, [_characterLimit integerValue], _ignoreNewlines);
            if (![_textView.text isEqualToString:value]) {
                changed = YES;
                _textView.text = value;
            }
            _placeholder.hidden = value.length > 0;
        }

        SCValdiSetTextHolderAttributes(_placeholder, fontAttributes, traitCollection, isRightToLeft, nil);
        [self invalidateLayout];
    }
    _updating = NO;

    return changed;
}

#pragma mark - Attributes

- (void)_setGravity:(SCValdiTextViewTextGravity)textGravity
{
    _textGravity = textGravity;
    [self _updatePlaceholderInset];
    [self _updateContentInset];
}

- (void)_setIgnoreNewlines:(BOOL)ignoreNewlines
{
    _ignoreNewlines = ignoreNewlines;
    [self _updateAttributedTextIfNeeded];
}

- (SCValdiFontAttributes *)fontAttributes
{
    if (_fontAttributes) {
        return _fontAttributes;
    }
    static dispatch_once_t onceToken;
    static SCValdiFontAttributes *fontAttributes;
    dispatch_once(&onceToken, ^{
        fontAttributes = [NSAttributedString fontAttributesWithCompositeValueGrowable:nil];
    });
    return fontAttributes;
}

- (void)valdi_setFontAttributes:(SCValdiFontAttributes *)fontAttributes
{
    _fontAttributes = fontAttributes;
    _needAttributedTextUpdate = YES;
    [self _applyNumberOfLinesAttributes];
    [self _updateAttributedTextIfNeeded];
}

- (void)valdi_setCustomUnderlineStyle:(SCValdiCustomUnderlineStyle *)customUnderlineStyle
{
    _customUnderlineStyle = customUnderlineStyle;
    _needAttributedTextUpdate = YES;
    if (_effectsLayoutManager) {
        [self _updateEffectsLayoutManager];
    }
    [self _updateAttributedTextIfNeeded];
    [_textView setNeedsDisplay];
}

- (BOOL)valdi_setTextOverflow:(NSString *)textOverflow
{
    if (textOverflow.length == 0) {
        _hasTextOverflow = NO;
        _textOverflowLineBreakMode = NSLineBreakByWordWrapping;
    } else if ([textOverflow isEqualToString:@"ellipsis"]) {
        _hasTextOverflow = YES;
        _textOverflowLineBreakMode = NSLineBreakByTruncatingTail;
    } else if ([textOverflow isEqualToString:@"clip"]) {
        _hasTextOverflow = YES;
        _textOverflowLineBreakMode = NSLineBreakByClipping;
    } else {
        SCLogValdiError(@"Invalid textOverflow value: %@", textOverflow);
        return NO;
    }

    [self _applyTextOverflowAttributes];
    [_textView setNeedsDisplay];
    [_animatedTextView setNeedsDisplay];
    return YES;
}

- (void)valdi_setValue:(id)textValue
{
    NSString *oldTextValue = _textView.text;
    _textValue = textValue;
    _needAttributedTextUpdate = YES;
    [self _updateAttributedTextIfNeeded];
    if (textValue != nil && ![oldTextValue isEqualToString:_textView.text]) {
        // Text changed programatically. Manually trigger delegate callback for selection change.
        // If the textValue is nil, it means we're resetting the binding and do not need to trigger events
        // Note: cannot perform an attributed text comparison as it compares `onLayout` closures which differ between instances
        [self textViewDidChangeSelection:_textView];
    }
}

- (BOOL)valdi_setCharacterLimit:(NSNumber *)characterLimit
{
    _characterLimit = characterLimit;
    _needAttributedTextUpdate = YES;
    [self _updateAttributedTextIfNeeded];
    return YES;
}

- (BOOL)valdi_setTextGravity:(NSString *)textGravity
{
    textGravity = [textGravity lowercaseString];
    if ([textGravity isEqualToString:@"top"]) {
        [self _setGravity:(SCValdiTextViewTextGravityTop)];
        return YES;
    }

    if ([textGravity isEqualToString:@"bottom"]) {
        [self _setGravity:(SCValdiTextViewTextGravityBottom)];
        return YES;
    }

    if (textGravity.length == 0 || [textGravity isEqualToString:@"center"]) {
        [self _setGravity:(SCValdiTextViewTextGravityCenter)];
        return YES;
    }

    return NO;
}

- (BOOL)valdi_setReturnType:(NSString*)returnType
{
    returnType = [returnType lowercaseString];
    if ([returnType isEqualToString:@"linereturn"] || returnType.length == 0) {
        [self _setIgnoreNewlines:NO];
        _textView.returnKeyType = UIReturnKeyDefault;
        return YES;
    } else {
        [self _setIgnoreNewlines:YES];
        return SCValdiTextInputSetReturnKeyText(_textView, returnType);
    }
}

- (BOOL)valdi_setAutocapitalization:(NSString *)autocapitalization
{
    return SCValdiTextInputSetAutocapitalization(_textView, autocapitalization);
}

- (BOOL)valdi_setAutocorrection:(NSString *)autocorrection
{
    return SCValdiTextInputSetAutocorrection(_textView, autocorrection);
}

- (BOOL)valdi_setKeyboardAppearance:(NSString *)keyboardAppearance
{
    return SCValdiTextInputSetKeyboardAppearance(_textView, keyboardAppearance);
}

- (BOOL)valdi_setTextDirection:(NSString *)textDirection
{
    return SCValdiTextInputSetTextDirection(_textView, textDirection);
}

- (BOOL)valdi_setEnabled:(BOOL)enabled
{
    _textView.editable = enabled;
    _textView.scrollEnabled = enabled && !_hasTextOverflow;
    _needAttributedTextUpdate = YES;
    [self _updateAttributedTextIfNeeded];
    return YES;
}

- (BOOL)valdi_setFocused:(BOOL)focused
{
    if (focused) {
        [_textView becomeFirstResponder];
    } else {
        [_textView resignFirstResponder];
    }
    return YES;
}

- (BOOL)valdi_setClosesWhenReturnKeyPressed:(BOOL)closesWhenReturnKeyPress
{
    _closesWhenReturnKeyPressed = closesWhenReturnKeyPress;
    return YES;
}

- (void)valdi_setFontManager:(id<SCValdiFontManagerProtocol>)fontManager
{
    _fontManager = fontManager;
}

- (BOOL)valdi_setPlaceholder:(nullable NSString *)placeholder
{
    _placeholder.text = placeholder;
    return YES;
}

- (BOOL)valdi_setPlaceholderColor:(nullable UIColor *)color
{
    _placeholder.textColor = color;
    return YES;
}

- (BOOL)valdi_setTintColor:(UIColor *)color
{
    _textView.tintColor = color;
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

- (void)_applySelectionStart:(NSInteger)selectionStart selectionEnd:(NSInteger)selectionEnd
{
    NSInteger offsetLimit = _textView.text.length;
    NSInteger offsetStart = MAX(0, MIN(offsetLimit, selectionStart));
    NSInteger offsetEnd = MAX(offsetStart, MIN(offsetLimit, selectionEnd));

    NSRange newRange = NSMakeRange(offsetStart, offsetEnd - offsetStart);
    if (!NSEqualRanges(_textView.selectedRange, newRange)) {
        _textView.selectedRange = newRange;
    }
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

    [self _applySelectionStart:[selection[0] unsignedIntValue] selectionEnd:[selection[1] unsignedIntValue]];

    return YES;
}

- (BOOL)valdi_setTextShadow:(NSArray *)textShadow
{
    SCValdiSetTextHolderTextShadow(_placeholder, textShadow);
    SCValdiSetTextHolderTextShadow(_textView, textShadow);
    return SCValdiSetTextHolderTextShadow(_animatedTextView, textShadow);
}

- (void) valdi_resetTextShadow
{
    SCValdiResetTextHolderTextShadow(_placeholder);
    SCValdiResetTextHolderTextShadow(_animatedTextView);
    SCValdiResetTextHolderTextShadow(_textView);
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
        [_textView setNeedsDisplay];
        return YES;
    }

    [[self _createTextGradientHelperIfNeeded] setGradientAttributes:attributeValue];
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
    [_textView setNeedsDisplay];
    [self _updateTextGradientLayerWithAnimator:animator];

    [self.valdiViewNode setDidFinishLayoutBlock:^(SCValdiTextView *view, id<SCValdiAnimatorProtocol> animator) {
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
    [self setNeedsLayout];
    [_textView setNeedsDisplay];
}

- (BOOL)valdi_setEnableInlinePredictions:(BOOL)enableInlinePredictions
{
    if (@available(iOS 17.0, *)) {
        _textView.inlinePredictionType = enableInlinePredictions ? UITextInlinePredictionTypeDefault : UITextInlinePredictionTypeNo;
    }

    return YES;
}

- (BOOL)valdi_setBackgroundEffectColor:(nullable UIColor *)color
{
    if (!_backgroundEffects) {
        _backgroundEffects = [SCValdiTextViewBackgroundEffects new];
    }
    _backgroundEffects.color = color;
    [self _updateEffectsLayoutManager];
    return YES;
}

- (BOOL)valdi_setBackgroundEffectBorderRadius:(double)borderRadius
{
    if (!_backgroundEffects) {
        _backgroundEffects = [SCValdiTextViewBackgroundEffects new];
    }
    _backgroundEffects.borderRadius = borderRadius;
    [self _updateEffectsLayoutManager];
    return YES;
}

- (BOOL)valdi_setBackgroundEffectPadding:(double)padding
{
    if (!_backgroundEffects) {
        _backgroundEffects = [SCValdiTextViewBackgroundEffects new];
    }
    _backgroundEffects.padding = padding;
    [self _updateEffectsLayoutManager];
    return YES;
}

#pragma mark - Static methods

+ (CGSize)measureSizeWithMaxSize:(CGSize)maxSize
                   fontAttributes:(SCValdiFontAttributes *)fontAttributes
                      fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                             text:(id)text
                      placeholder:(NSString *)placeholder
                  traitCollection:(UITraitCollection *)traitCollection
{
    CGSize textSize = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                 fontAttributes:fontAttributes
                                                    fontManager:fontManager
                                                           text:text
                                                traitCollection:traitCollection];
    CGSize placeholderSize = [SCValdiTextLayout measureSizeWithMaxSize:maxSize
                                                         fontAttributes:fontAttributes
                                                            fontManager:fontManager
                                                                   text:placeholder
                                                        traitCollection:traitCollection];
    return CGSizeMake(MAX(textSize.width, placeholderSize.width),
                      MAX(textSize.height, placeholderSize.height));
}

+ (CGSize)valdi_onMeasureWithAttributes:(id<SCValdiViewLayoutAttributes>)attributes
                                maxSize:(CGSize)maxSize
                            fontManager:(id<SCValdiFontManagerProtocol>)fontManager
                        traitCollection:(UITraitCollection *)traitCollection
{
    SCValdiFontAttributes *fontAttributes = ObjectAs([attributes valueForAttributeName:@"fontSpecs"], SCValdiFontAttributes);
    if (!fontAttributes) {
        fontAttributes = [NSAttributedString fontAttributesWithCompositeValueGrowable:nil];
    }
    id text = [attributes valueForAttributeName:@"value"];
    NSString *placeholder = ObjectAs([attributes valueForAttributeName:@"placeholder"], NSString);

    return [SCValdiTextView measureSizeWithMaxSize:maxSize
                                    fontAttributes:fontAttributes
                                       fontManager:fontManager
                                              text:text
                                       placeholder:placeholder
                                   traitCollection:traitCollection];
}

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    id<SCValdiFontManagerProtocol> fontManager = [attributesBinder fontManager];

     [attributesBinder bindCompositeAttribute:@"fontSpecs"
                                        parts:[NSAttributedString valdiFontAttributesGrowable]
                             withUntypedBlock:^BOOL(__kindof SCValdiTextView *textView, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
         [textView valdi_setFontManager:fontManager];
         [textView valdi_setFontAttributes:ObjectAs(attributeValue, SCValdiFontAttributes)];
         return YES;
     }
                                   resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
         [textView valdi_setFontAttributes:nil];
     }];

     [attributesBinder registerPreprocessorForAttribute:@"font" enableCache:YES withBlock:^id(id value) {
         return [SCValdiFont fontFromValdiAttribute:ObjectAs(value, NSString) fontManager:fontManager];
     }];

     [attributesBinder registerPreprocessorForAttribute:@"fontSpecs" enableCache:YES withBlock:^id(id value) {
         return [NSAttributedString fontAttributesWithCompositeValueGrowable:value];
     }];

    [attributesBinder bindAttribute:@"textGravity"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setTextGravity:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setTextGravity:nil];
        }];

    [attributesBinder bindAttribute:@"autocapitalization"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setAutocapitalization:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setAutocapitalization:nil];
        }];

    [attributesBinder bindAttribute:@"autocorrection"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setAutocorrection:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setAutocorrection:nil];
        }];

    [attributesBinder bindAttribute:@"textDirection"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setTextDirection:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setTextDirection:nil];
        }];

    [attributesBinder bindAttribute:@"keyboardAppearance"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setKeyboardAppearance:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setKeyboardAppearance:nil];
        }];

    [attributesBinder bindAttribute:@"enabled"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextView *textView, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setEnabled:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setEnabled:YES];
        }];

    [attributesBinder bindAttribute:@"focused"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextView *textView, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setFocused:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setFocused:NO];
        }];

    [attributesBinder bindAttribute:@"characterLimit"
        invalidateLayoutOnChange:YES
        withIntBlock:^BOOL(SCValdiTextView *textView, NSInteger attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setCharacterLimit:@(attributeValue)];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setCharacterLimit:nil];
        }];

    [attributesBinder bindAttribute:@"tintColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(SCValdiTextView *textView, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setTintColor:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setTintColor:nil];
        }];

    [attributesBinder bindAttribute:@"value"
        invalidateLayoutOnChange:YES
        withTextBlock:^BOOL(SCValdiTextView *textView, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setFontManager:fontManager];
            SCValdiAnimatorTransitionWrap(animator, textView, { [textView valdi_setValue:attributeValue]; });
            return YES;
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            SCValdiAnimatorTransitionWrap(animator, textView, { [textView valdi_setValue:nil]; });
        }];

    [attributesBinder bindAttribute:@"customUnderlineStyle"
        invalidateLayoutOnChange:NO
        withUntypedBlock:^BOOL(SCValdiTextView *textView, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setCustomUnderlineStyle:ObjectAs(attributeValue, SCValdiCustomUnderlineStyle)];
            return YES;
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setCustomUnderlineStyle:nil];
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

    [attributesBinder bindAttribute:@"textOverflow"
        invalidateLayoutOnChange:YES
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setTextOverflow:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setTextOverflow:nil];
        }];

    [attributesBinder bindAttribute:@"closesWhenReturnKeyPressed"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextView *textView, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setClosesWhenReturnKeyPressed:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setClosesWhenReturnKeyPressed:NO];
        }];

    [attributesBinder bindAttribute:@"selectTextOnFocus"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextView *textView, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setSelectTextOnFocus:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setSelectTextOnFocus:NO];
        }];

    [attributesBinder bindAttribute:@"returnType"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setReturnType:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setReturnType:nil];
        }];

    [attributesBinder bindAttribute:@"onWillChange"
        withFunctionBlock:^(SCValdiTextView *view, id<SCValdiFunction> attributeValue) {
            [view valdi_setOnWillChange:attributeValue];
        }
        resetBlock:^(SCValdiTextView *view) {
            [view valdi_setOnWillChange:nil];
        }];

    [attributesBinder bindAttribute:@"onChange"
        withFunctionBlock:^(SCValdiTextView *view, id<SCValdiFunction> attributeValue) {
            [view valdi_setOnChange:attributeValue];
        }
        resetBlock:^(SCValdiTextView *view) {
            [view valdi_setOnChange:nil];
        }];

    [attributesBinder bindAttribute:@"onEditBegin"
        withFunctionBlock:^(SCValdiTextView *view, id<SCValdiFunction> attributeValue) {
            [view valdi_setOnEditBegin:attributeValue];
        }
        resetBlock:^(SCValdiTextView *view) {
            [view valdi_setOnEditBegin:nil];
        }];

    [attributesBinder bindAttribute:@"onEditEnd"
        withFunctionBlock:^(SCValdiTextView *view, id<SCValdiFunction> attributeValue) {
            [view valdi_setOnEditEnd:attributeValue];
        }
        resetBlock:^(SCValdiTextView *view) {
            [view valdi_setOnEditEnd:nil];
        }];

    [attributesBinder bindAttribute:@"onReturn"
        withFunctionBlock:^(SCValdiTextView *view, id<SCValdiFunction> attributeValue) {
            [view valdi_setOnReturn:attributeValue];
        }
        resetBlock:^(SCValdiTextView *view) {
            [view valdi_setOnReturn:nil];
        }];

    [attributesBinder bindAttribute:@"onWillDelete"
        withFunctionBlock:^(SCValdiTextView *view, id<SCValdiFunction> attributeValue) {
            [view valdi_setOnWillDelete:attributeValue];
        }
        resetBlock:^(SCValdiTextView *view) {
            [view valdi_setOnWillDelete:nil];
        }];

    [attributesBinder bindAttribute:@"placeholderColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(SCValdiTextView *textView, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setPlaceholderColor:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setPlaceholderColor:nil];
        }];

    [attributesBinder bindAttribute:@"placeholder"
        invalidateLayoutOnChange:YES
        withStringBlock:^BOOL(SCValdiTextView *textView, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
           [textView valdi_setFontManager:fontManager];
            SCValdiAnimatorTransitionWrap(animator, textView, { [textView valdi_setPlaceholder:attributeValue]; });
            return YES;
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            SCValdiAnimatorTransitionWrap(animator, textView, { [textView valdi_setPlaceholder:nil]; });
        }];

    [attributesBinder setMeasureDelegate:^CGSize(id<SCValdiViewLayoutAttributes> attributes, CGSize maxSize, UITraitCollection *traitCollection) {
        return [SCValdiTextView valdi_onMeasureWithAttributes:attributes maxSize:maxSize fontManager:fontManager traitCollection:traitCollection];
    }];

    [attributesBinder bindAttribute:@"selection"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(SCValdiTextView *textView, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setSelection:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setSelection:@[]];
        }];

    [attributesBinder bindAttribute:@"onSelectionChange"
        withFunctionBlock:^(SCValdiTextView *textView, id<SCValdiFunction> attributeValue) {
            [textView valdi_setOnSelectionChange:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView) {
            [textView valdi_setOnSelectionChange:nil];
        }];

    [attributesBinder bindAttribute:@"textShadow"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(__kindof SCValdiTextView *textView, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setTextShadow:attributeValue];
        }
        resetBlock:^(__kindof SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_resetTextShadow];
        }];

    [attributesBinder bindAttribute:@"textGradient"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(SCValdiTextView *textView, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setTextGradient:attributeValue animator:animator];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setTextGradient:nil animator:animator];
        }];

    [attributesBinder bindAttribute:@"enableInlinePredictions"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextView *textView, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setEnableInlinePredictions:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setEnableInlinePredictions:NO];
        }];

    [attributesBinder bindAttribute:@"backgroundEffectColor"
        invalidateLayoutOnChange:YES
        withColorBlock:^BOOL(SCValdiTextView *textView, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setBackgroundEffectColor:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setBackgroundEffectColor:nil];
        }];

    [attributesBinder bindAttribute:@"backgroundEffectBorderRadius"
        invalidateLayoutOnChange:NO
        withDoubleBlock:^BOOL(SCValdiTextView *textView, double attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setBackgroundEffectBorderRadius:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setBackgroundEffectBorderRadius:0];
        }];

    [attributesBinder bindAttribute:@"backgroundEffectPadding"
        invalidateLayoutOnChange:NO
        withDoubleBlock:^BOOL(SCValdiTextView *textView, double attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setBackgroundEffectPadding:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setBackgroundEffectPadding:0];
        }];

}

#pragma mark - UITextViewDelegate implementation

- (BOOL)textView:(UITextView *)textView shouldChangeTextInRange:(NSRange)range replacementText:(NSString *)text
{
    // When the user just typed a singular line return
    if ([text isEqualToString:@"\n"]) {
        // Since there is no textviewShouldReturn, we schedule one such event if we see a linereturn
        if (_closesWhenReturnKeyPressed || _onReturn != nil) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self->_closesWhenReturnKeyPressed) {
                    self->_lastUnfocusReason = SCValdiTextInputUnfocusReasonReturnKeyPress;
                    [textView resignFirstResponder];
                }
                SCValdiCallEvent(self->_onReturn, self->_textView);
            });
        }
        if (_ignoreNewlines) {
            // If the only change is a newline, don't allow it
            return NO;
        }
    }

    if (text == nil) {
        return NO;
    }

    // Set the text to the clamped value if it violates formatting rules
    if ([self _needAttributedString]) {
        NSMutableAttributedString *mutableNewText = [textView.attributedText mutableCopy];
        [mutableNewText replaceCharactersInRange:range withAttributedString:[[NSAttributedString alloc] initWithString:text]];
        NSAttributedString *clampedText = SCValdiClampAttributedStringValue(mutableNewText, [_characterLimit integerValue], _ignoreNewlines);
        if(![mutableNewText.string isEqualToString:clampedText.string]) {
            textView.attributedText = clampedText;
            [self textViewDidChange:textView];
            return NO;
        }
    } else {
        NSString *newText = [textView.text stringByReplacingCharactersInRange:range withString:text];
        NSString *clampedText = SCValdiClampTextValueChanged(textView.text, text, range, [_characterLimit integerValue], _ignoreNewlines);
        if (![newText isEqualToString:clampedText]) {
            textView.text = clampedText;
            // Manually trigger the did change event to cause the event calls to fire
            [self textViewDidChange:textView];
            return NO;
        }
    }
    // Otherwise, good to go
    return YES;
}

- (void)textViewDidChange:(UITextView *)textView
{
    if (_updating) {
        return;
    }

    // Skip during IME composition to avoid interfering with marked text
    if (textView.markedTextRange != nil) {
        _placeholder.hidden = textView.text.length > 0;
        return;
    }

    if (_onWillChange != nil) {
        SCValdiMarshallerScoped(marshaller, {
            SCValdiMarshallEditTextEvent(marshaller, _textView);
            if ([_onWillChange performSyncWithMarshaller:marshaller propagatesError:NO] && SCValdiMarshallerIsMap(marshaller, -1)) {
                @try {
                    SCValdiMarshallerMustGetMapProperty(marshaller, SCValdiTextViewTextKey(), -1);
                    NSString* newText = SCValdiMarshallerGetString(marshaller, -1);
                    SCValdiMarshallerPop(marshaller);

                    SCValdiMarshallerMustGetMapProperty(marshaller, SCValdiTextViewSelectionStartKey(), -1);
                    NSInteger indexStart = SCValdiMarshallerGetInt(marshaller, -1);
                    SCValdiMarshallerPop(marshaller);

                    SCValdiMarshallerMustGetMapProperty(marshaller, SCValdiTextViewSelectionEndKey(), -1);
                    NSInteger indexEnd = SCValdiMarshallerGetInt(marshaller, -1);
                    SCValdiMarshallerPop(marshaller);

                    // First, update the text value (so the selection can have the proper clamped range)
                    // We update only non-attributed strings, as we expect the JS side to be generating AttributedText
                    if (![self _needAttributedString]) {
                        _textValue = newText;
                        _textView.text = newText;
                        _needAttributedTextUpdate = YES;
                        [self _updateAttributedTextIfNeeded];
                    }

                    // Then, update the selection range
                    NSInteger offsetLimit = _textView.text.length;
                    NSInteger offsetStart = MAX(0, MIN(offsetLimit, indexStart));
                    NSInteger offsetEnd = MAX(offsetStart, MIN(offsetLimit, indexEnd));
                    UITextPosition *positionOrigin = _textView.beginningOfDocument;
                    UITextPosition *positionStart = [_textView positionFromPosition:positionOrigin offset:offsetStart];
                    UITextPosition *positionEnd = [_textView positionFromPosition:positionOrigin offset:offsetEnd];
                    _textView.selectedTextRange = [_textView textRangeFromPosition:positionStart toPosition:positionEnd];

                } @catch (SCValdiError *exc) {
                    SCLogValdiError(@"Failed to unmarshall edit text event: %@", exc.reason);
                }
            }
        });
    }

    // we update only non-attributed strings, as we expect the JS side to be generating AttributedText
    if (![self _needAttributedString]) {
        _textValue = _textView.text;
        _needAttributedTextUpdate = YES;
        [self _updateAttributedTextIfNeeded];
    }

    [self notifyTextValueDidChange];
}

- (void)textViewDidBeginEditing:(UITextView *)textView
{
    if (textView && textView.window) {
        id<SCValdiViewNodeProtocol> viewNode = self.valdiViewNode;
        id<SCValdiContextProtocol> context = self.valdiContext;
        [context didChangeValue:@YES forInternedValdiAttribute:SCValdiTextViewFocusedKey() inViewNode:viewNode];

        // OnEditBegin event
        _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;
        SCValdiCallEvent(_onEditBegin, textView);

        // Post-focus auto-select
        if (_selectTextOnFocus) {
            // Without dispatch_async, `selectAll` only works every other call.
            // There are other parts in the app where we do this as well.
            dispatch_async(dispatch_get_main_queue(), ^{
                [textView selectAll:nil];
            });
        }
    }
}

- (void)textViewDidEndEditing:(UITextView *)textView
{
    if (textView && textView.window) {
        id<SCValdiViewNodeProtocol> viewNode = self.valdiViewNode;
        id<SCValdiContextProtocol> context = self.valdiContext;
        [context didChangeValue:@NO forInternedValdiAttribute:SCValdiTextViewFocusedKey() inViewNode:viewNode];

        // OnEditEnd event
        SCValdiCallEventWithReason(_onEditEnd, textView, _lastUnfocusReason);
        _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;

    }
}

- (void)textViewDidChangeSelection:(UITextView *)textView
{
    if (textView && textView.window && !_updating) {
        // Skip during IME composition to avoid interfering with marked text
        if (textView.markedTextRange != nil) {
            return;
        }

        id<SCValdiViewNodeProtocol> viewNode = self.valdiViewNode;
        id<SCValdiContextProtocol> context = self.valdiContext;

        NSInteger startPosition = textView.selectedRange.location;
        NSInteger endPosition = startPosition + textView.selectedRange.length;

        [context didChangeValue: @[@(startPosition), @(endPosition)] forInternedValdiAttribute:SCValdiTextViewSelectionKey() inViewNode:viewNode];

        SCValdiCallEvent(_onSelectionChange, textView);
    }
}


#pragma mark - NSTextStorageDelegate implementation

- (void)textStorage:(NSTextStorage *)textStorage
    didProcessEditing:(NSTextStorageEditActions)editedMask
                range:(NSRange)editedRange
       changeInLength:(NSInteger)delta
{
    if (_effectsLayoutManager == nil) {
        return;
    }

    // Invalidate all the glyphs. This resets the geometry as drawing the bubble wrap traverses each text container.
    // As previous text containers can change their layout, redrawing is critical to fix cached background drawings
    //   that make it look clipped
    // Example:
    //    _____________
    //   |    -----    |
    //   |    ¦ O ¦    |
    //   |   |  W  |   |
    //   |   -------   |
    [_textView setNeedsDisplay];
}


#pragma mark - UIAccessibilityElement

- (BOOL)isAccessibilityElement
{
    return YES;
}

- (NSString *)accessibilityLabel
{
    NSString *accessibilityLabel = [_textView accessibilityLabel];
    if ([accessibilityLabel length]) {
        return accessibilityLabel;
    }
    return [_placeholder accessibilityLabel];
}

- (NSString *)accessibilityHint
{
    NSString *accessibilityHint = [_textView accessibilityHint];
    if ([accessibilityHint length]) {
        return accessibilityHint;
    }
    return [_placeholder accessibilityHint];
}

- (NSString *)accessibilityValue
{
    NSString *accessibilityValue = [_textView accessibilityValue];
    if ([accessibilityValue length]) {
        return accessibilityValue;
    }
    return [_placeholder accessibilityValue];
}

- (UIAccessibilityTraits)accessibilityTraits
{
    return [_textView accessibilityTraits];
}

@end
