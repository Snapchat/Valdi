//
//  SCValdiTextLayoutView.m
//  Valdi
//

#import "valdi/ios/Views/SCValdiTextLayoutView.h"

#import "valdi/ios/Text/SCValdiAttributedText.h"
#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"
#import "valdi/ios/Text/SCValdiProcessedText.h"
#import "valdi/ios/Text/SCValdiTextAnimationCoordinator.h"
#import "valdi/ios/Text/SCValdiTextAnimationGroupParticipant.h"
#import "valdi/ios/Text/SCValdiTextAnimationPresentation.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi/ios/Views/SCValdiLabelSelection.h"
#import "valdi/ios/Views/SCValdiTextAnimationGroup.h"
#import "valdi/ios/Views/SCValdiTextViewEffectsLayoutManager.h"

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiContextProtocol.h"
#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiRectUtils.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"

INTERNED_STRING_CONST("text", SCValdiLabelTextKey);
INTERNED_STRING_CONST("selection", SCValdiLabelSelectionKey);
INTERNED_STRING_CONST("selectionStart", SCValdiLabelSelectionStartKey);
INTERNED_STRING_CONST("selectionEnd", SCValdiLabelSelectionEndKey);

static const CGFloat SCValdiTextSelectionHandleHitTestOutset = 44.0;

@interface SCValdiTextLayoutView () <UITextInput, UITextInteractionDelegate, SCValdiTextAnimationGroupParticipant>
- (void)_becomeSelectionFirstResponder;
- (void)_installSelectionInteractionsIfNeeded;
- (void)_removeSelectionInteractionOverlay;
- (void)_updateTextAnimationGroupRegistration;
@end

@interface SCValdiTextLayoutSelectionInteractionView : UIView
@property (nonatomic, weak) SCValdiTextLayoutView *textLayoutView;
@end

@implementation SCValdiTextLayoutSelectionInteractionView

// Selection handles can extend outside the label's Valdi layout bounds. This
// transparent window overlay gives those handle touches a hittable owner without
// changing generic Valdi view hit testing.
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    CGPoint textLayoutPoint = [_textLayoutView convertPoint:point fromView:self];
    return [_textLayoutView pointInsideActiveSelectionHandleBounds:textLayoutPoint];
}

// Route only active selection-handle touches back to the real text input view so
// UIKit's UITextInteraction keeps receiving the drag sequence.
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self pointInside:point withEvent:event] ? _textLayoutView : nil;
}

@end

@implementation SCValdiTextLayoutView {
    SCValdiTextLayout *_textLayout;
    SCValdiProcessedText *_processedText;
    SCValdiTextViewEffectsLayoutManager *_textLayoutEffectsLayoutManager;
    CADisplayLink *_animatedTextDisplayLink;
    SCValdiCustomUnderlineStyle *_customUnderlineStyle;
    NSAttributedString *_customUnderlineSourceAttributedString;
    NSArray<NSValue *> *_customUnderlineCharacterRanges;
    SCValdiLabelSelectionState *_selectionState;
    BOOL _usesEffectsLayoutManager;
    __weak SCValdiTextAnimationGroup *_textAnimationGroup;
    __weak SCValdiTextAnimationCoordinator *_textAnimationCoordinator;
    __weak id<SCValdiViewNodeProtocol> _textAnimationViewNode;
    NSUInteger _textAnimationBasePartIndex;
    NSUInteger _textAnimationPartCount;
}

- (void)_finishInitializationWithUsesEffectsLayoutManager:(BOOL)usesEffectsLayoutManager
{
    self.backgroundColor = UIColor.clearColor;
    self.opaque = NO;
    self.userInteractionEnabled = YES;
    [self configureWithUsesEffectsLayoutManager:usesEffectsLayoutManager];
}

- (instancetype)initWithFrame:(CGRect)frame
{
    return [self initWithFrame:frame usesEffectsLayoutManager:NO];
}

- (instancetype)initWithFrame:(CGRect)frame usesEffectsLayoutManager:(BOOL)usesEffectsLayoutManager
{
    self = [super initWithFrame:frame];
    if (self) {
        [self _finishInitializationWithUsesEffectsLayoutManager:usesEffectsLayoutManager];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self _finishInitializationWithUsesEffectsLayoutManager:NO];
    }
    return self;
}

- (void)dealloc
{
    [_textAnimationGroup unregisterTextAnimationParticipant:self];
    [self stopAnimations];
    [self _removeSelectionInteractions];
}

- (SCValdiTextLayout *)textLayout
{
    [self _ensureTextLayout];
    return _textLayout;
}

- (BOOL)usesEffectsLayoutManager
{
    return _usesEffectsLayoutManager;
}

- (BOOL)selectable
{
    return _selectionState.selectable;
}

- (BOOL)pointInsideActiveSelectionHandleBounds:(CGPoint)point
{
    if (!self.selectable || _selectionState.selectedRange.length == 0) {
        return NO;
    }

    // UIKit selection handles can sit just outside the text view bounds. Keep
    // nearby touches routed to UITextInteraction while a selection is active.
    return CGRectContainsPoint(CGRectInset(self.bounds,
                                           -SCValdiTextSelectionHandleHitTestOutset,
                                           -SCValdiTextSelectionHandleHitTestOutset),
                               point);
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
    BOOL pointInside = [super pointInside:point withEvent:event];
    if (!pointInside) {
        pointInside = [self pointInsideActiveSelectionHandleBounds:point];
    }
    if (pointInside && self.selectable) {
        [self _installSelectionInteractionsIfNeeded];
    }
    return pointInside;
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    _textLayout.size = self.bounds.size;
    [self _updateSelectionInteractionOverlayFrameIfNeeded];
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    [self _updateTextAnimationGroupRegistration];
    if (self.window) {
        [self _updateSelectionInteractionOverlayForCurrentSelection];
    } else {
        [self _removeSelectionInteractionOverlay];
    }
}

- (void)didMoveToSuperview
{
    [super didMoveToSuperview];
    [self _updateTextAnimationGroupRegistration];
}

- (void)drawRect:(CGRect)rect
{
    [_textLayout drawInRect:self.bounds];
    [self _drawCustomUnderlinesInRect:self.bounds];
}

- (void)configureWithUsesEffectsLayoutManager:(BOOL)usesEffectsLayoutManager
{
    if (_textLayout && _usesEffectsLayoutManager == usesEffectsLayoutManager) {
        return;
    }

    SCValdiProcessedText *processedText = _processedText;
    NSUInteger maxNumberOfLines = _textLayout.maxNumberOfLines ?: _maxNumberOfLines;

    _usesEffectsLayoutManager = usesEffectsLayoutManager;
    if (usesEffectsLayoutManager) {
        _textLayoutEffectsLayoutManager = [SCValdiTextViewEffectsLayoutManager new];
        _textLayout = [[SCValdiTextLayout alloc] initWithLayoutManager:_textLayoutEffectsLayoutManager];
    } else {
        _textLayoutEffectsLayoutManager = nil;
        _textLayout = [SCValdiTextLayout new];
    }

    _textLayout.processedText = processedText;
    _textLayout.maxNumberOfLines = maxNumberOfLines;
    _textLayout.size = self.bounds.size;
    _textLayoutEffectsLayoutManager.valdiViewNode = _textAnimationViewNode;
    _textLayoutEffectsLayoutManager.processedText = processedText;
    _textLayoutEffectsLayoutManager.customUnderlineStyle = _customUnderlineStyle;
    _textLayoutEffectsLayoutManager.customUnderlineSourceAttributedString = _customUnderlineSourceAttributedString;
    _textLayoutEffectsLayoutManager.customUnderlineCharacterRanges = _customUnderlineCharacterRanges;
    _textLayoutEffectsLayoutManager.customUnderlineFallbackColor = _defaultTextColor ?: UIColor.blackColor;
    _textAnimationPartCount = usesEffectsLayoutManager && processedText != nil ? processedText.animationTransformsCount : 0;
    [self setTextAnimationCoordinator:_textAnimationCoordinator basePartIndex:_textAnimationBasePartIndex];
    [self _updateTextAnimationGroupRegistration];
    [self setNeedsDisplay];
}

- (void)setTextAnimationViewNode:(id<SCValdiViewNodeProtocol>)viewNode
{
    _textAnimationViewNode = viewNode;
    _textLayoutEffectsLayoutManager.valdiViewNode = viewNode;
}

- (void)setMaxNumberOfLines:(NSUInteger)maxNumberOfLines
{
    _maxNumberOfLines = maxNumberOfLines;
    _textLayout.maxNumberOfLines = maxNumberOfLines;
}

- (void)setProcessedText:(SCValdiProcessedText *)processedText
{
    _processedText = processedText;
    [self _ensureTextLayout].processedText = processedText;
    _textLayoutEffectsLayoutManager.processedText = processedText;
    _textAnimationPartCount = self.usesEffectsLayoutManager && processedText != nil ? processedText.animationTransformsCount : 0;
    [self _updateTextAnimationGroupRegistration];
    [self _clampSelectionToCurrentText];
    [self setNeedsDisplay];
}

- (void)updateInlineAttachmentsAndUpdate
{
    if (![_processedText updateInlineAttachments]) {
        return;
    }

    [_textLayout refreshProcessedTextStorage];
    [_textLayout invalidateLayout];
    [self setNeedsDisplay];
}

- (void)setCustomUnderlineStyle:(SCValdiCustomUnderlineStyle *)customUnderlineStyle
         sourceAttributedString:(NSAttributedString *)sourceAttributedString
                characterRanges:(NSArray<NSValue *> *)characterRanges
{
    _customUnderlineStyle = customUnderlineStyle;
    _customUnderlineSourceAttributedString = sourceAttributedString;
    _customUnderlineCharacterRanges = [characterRanges copy];
    _textLayoutEffectsLayoutManager.customUnderlineStyle = customUnderlineStyle;
    _textLayoutEffectsLayoutManager.customUnderlineSourceAttributedString = sourceAttributedString;
    _textLayoutEffectsLayoutManager.customUnderlineCharacterRanges = characterRanges;
    _textLayoutEffectsLayoutManager.customUnderlineFallbackColor = _defaultTextColor ?: UIColor.blackColor;
    [self setNeedsDisplay];
}

- (void)setDefaultTextColor:(UIColor *)defaultTextColor
{
    if (_defaultTextColor == defaultTextColor) {
        return;
    }

    _defaultTextColor = defaultTextColor;
    _textLayoutEffectsLayoutManager.customUnderlineFallbackColor = defaultTextColor ?: UIColor.blackColor;
}

- (void)performOnLayoutCallbacks
{
    if (_processedText == nil) {
        return;
    }

    [_processedText enumerateOnLayoutCallbacksUsingBlock:^(id<SCValdiFunction> callback, NSRange range, BOOL *stop) {
            CGRect newBounds = [self->_textLayout boundingRectForRange:range];
            SCValdiMarshallerScoped(marshaller, {
                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.x));
                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.y));
                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.width));
                SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.height));
                [callback performWithMarshaller:marshaller];
            });
    }];
}

- (id<SCValdiViewNodeProtocol>)_resolveValdiViewNode
{
    return [_delegate valdiViewNodeForTextLayoutView:self];
}

- (void)invalidateAnimatedTextProgress
{
    [_textLayoutEffectsLayoutManager invalidateAnimatedTextProgress];
    [_delegate textLayoutViewDidInvalidateAnimatedTextProgress:self];
    if (_textLayoutEffectsLayoutManager.textAnimationCoordinator == nil) {
        [self _startAnimatedTextDisplayLinkIfNeeded];
    } else {
        [_textAnimationGroup startTextAnimationFrameLoopIfNeeded];
    }
}

- (void)setTextAnimationCoordinator:(SCValdiTextAnimationCoordinator *)coordinator basePartIndex:(NSUInteger)basePartIndex
{
    _textAnimationCoordinator = coordinator;
    _textAnimationBasePartIndex = basePartIndex;
    _textLayoutEffectsLayoutManager.textAnimationCoordinator = coordinator;
    _textLayoutEffectsLayoutManager.textAnimationBasePartIndex = basePartIndex;
    if (coordinator != nil) {
        [self stopAnimations];
    }
}

- (void)prepareGroupedTextAnimationFrame
{
    [_textLayoutEffectsLayoutManager prepareGroupedAnimatedTextProgress];
}

- (BOOL)invalidateGroupedTextAnimationFrame
{
    BOOL hasActiveAnimationRanges = [_textLayoutEffectsLayoutManager invalidateAnimatedTextProgress];
    [self setNeedsDisplay];
    [_delegate textLayoutViewDidInvalidateAnimatedTextProgress:self];
    return hasActiveAnimationRanges;
}

- (CGFloat)animatedTextOpacityForRange:(NSRange)range
{
    SCValdiTextAnimationPresentation *presentation = [self animatedTextPresentationForRange:range];
    return presentation != nil ? presentation.opacity : 1.0;
}

- (SCValdiTextAnimationPresentation *)animatedTextPresentationForRange:(NSRange)range
{
    if (_textLayoutEffectsLayoutManager == nil) {
        return nil;
    }
    return [_textLayoutEffectsLayoutManager presentationForAnimationRange:range];
}

- (void)valdi_prepareGroupedTextAnimationFrame
{
    [self prepareGroupedTextAnimationFrame];
}

- (BOOL)valdi_invalidateGroupedTextAnimationFrame
{
    return [self invalidateGroupedTextAnimationFrame];
}

- (SCValdiTextAnimationGroup *)_nearestTextAnimationGroup
{
    UIView *ancestor = self.superview;
    while (ancestor != nil) {
        SCValdiTextAnimationGroup *group = ObjectAs(ancestor, SCValdiTextAnimationGroup);
        if (group != nil) {
            return group;
        }
        ancestor = ancestor.superview;
    }
    return nil;
}

- (void)_updateTextAnimationGroupRegistration
{
    SCValdiTextAnimationGroup *group = _textAnimationPartCount > 0 ? [self _nearestTextAnimationGroup] : nil;
    if (group == _textAnimationGroup) {
        if (group != nil) {
            [self setTextAnimationCoordinator:group.textAnimationCoordinator basePartIndex:0];
            [group setNeedsLayout];
        }
        return;
    }

    [_textAnimationGroup unregisterTextAnimationParticipant:self];
    _textAnimationGroup = group;
    if (group != nil) {
        [group registerTextAnimationParticipant:self];
    } else {
        [self setTextAnimationCoordinator:nil basePartIndex:0];
    }
}

- (NSUInteger)valdi_textAnimationPartCount
{
    return _textAnimationPartCount;
}

- (void)valdi_applyTextAnimationCoordinator:(SCValdiTextAnimationCoordinator *)coordinator
                              basePartIndex:(NSUInteger)basePartIndex
{
    [self setTextAnimationCoordinator:coordinator basePartIndex:basePartIndex];
}

- (void)valdi_clearTextAnimationGroupRegistration
{
    _textAnimationGroup = nil;
    [self setTextAnimationCoordinator:nil basePartIndex:0];
}

- (void)stopAnimations
{
    [_textLayoutEffectsLayoutManager saveAnimatedTextProgress];
    [_animatedTextDisplayLink invalidate];
    _animatedTextDisplayLink = nil;
}

- (void)prepareForRecycling
{
    [_textLayoutEffectsLayoutManager saveAnimatedTextProgress];
    [_textLayoutEffectsLayoutManager clearAnimatedTextProgress];
    [_animatedTextDisplayLink invalidate];
    _animatedTextDisplayLink = nil;
}

- (id<SCValdiFunction>)onTapFunctionAtLocation:(CGPoint)location
{
    NSAttributedString *attributedString = _processedText.attributedString;
    if (!attributedString) {
        return nil;
    }

    NSUInteger index = [_textLayout characterIndexAtPoint:location];
    if (index == NSNotFound || index >= attributedString.length) {
        return nil;
    }

    return [_processedText onTapAtIndex:index effectiveRange:NULL];
}

- (SCValdiTextLayout *)_ensureTextLayout
{
    if (!_textLayout) {
        [self configureWithUsesEffectsLayoutManager:_usesEffectsLayoutManager];
    }
    return _textLayout;
}

- (UIColor *)_customUnderlineColorForRange:(NSRange)range
{
    UIColor *defaultColor = _defaultTextColor ?: UIColor.blackColor;
    return SCValdiCustomUnderlineColorForRange(_customUnderlineSourceAttributedString, range, defaultColor);
}

- (void)_drawCustomUnderlinesInRect:(CGRect)rect
{
    if (!_customUnderlineStyle
        || _textLayoutEffectsLayoutManager != nil
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

- (void)_startAnimatedTextDisplayLinkIfNeeded
{
    if (_animatedTextDisplayLink != nil) {
        return;
    }

    _animatedTextDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(_animatedTextDisplayLinkDidFire:)];
    [_animatedTextDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)_animatedTextDisplayLinkDidFire:(CADisplayLink *)displayLink
{
    BOOL hasActiveAnimationRanges = [_textLayoutEffectsLayoutManager invalidateAnimatedTextProgress];
    [self setNeedsDisplay];
    [_delegate textLayoutViewDidInvalidateAnimatedTextProgress:self];

    if (!hasActiveAnimationRanges) {
        [self stopAnimations];
    }
}

#pragma mark - Selection helpers

- (NSString *)_currentTextString
{
    return _processedText.attributedString.string ?: @"";
}

- (NSUInteger)_currentTextLength
{
    return [self _currentTextString].length;
}

- (SCValdiLabelSelectionState *)_ensureSelectionState
{
    if (!_selectionState) {
        _selectionState = [SCValdiLabelSelectionState new];
    }
    return _selectionState;
}

- (void)_updateSelectionInteractionOverlayFrameIfNeeded
{
    if (!_selectionState.selectionInteractionOverlayView || !self.window) {
        return;
    }

    CGRect overlayFrameInSelf = CGRectInset(self.bounds,
                                            -SCValdiTextSelectionHandleHitTestOutset,
                                            -SCValdiTextSelectionHandleHitTestOutset);
    UIView *overlayView = _selectionState.selectionInteractionOverlayView;
    overlayView.frame = [self.window convertRect:overlayFrameInSelf fromView:self];
}

- (void)_removeSelectionInteractionOverlay
{
    [_selectionState.selectionInteractionOverlayView removeFromSuperview];
    _selectionState.selectionInteractionOverlayView = nil;
}

- (NSRange)_clampedSelectionRangeWithStart:(NSInteger)start end:(NSInteger)end
{
    NSInteger textLength = (NSInteger)[self _currentTextLength];
    NSInteger offsetStart = MAX(0, MIN(textLength, start));
    NSInteger offsetEnd = MAX(offsetStart, MIN(textLength, end));
    return NSMakeRange((NSUInteger)offsetStart, (NSUInteger)(offsetEnd - offsetStart));
}

- (NSInteger)_offsetFromTextPosition:(UITextPosition *)position
{
    SCValdiLabelTextPosition *labelPosition = ObjectAs(position, SCValdiLabelTextPosition);
    if (!labelPosition) {
        return 0;
    }
    return labelPosition.offset;
}

- (NSRange)_rangeFromTextRange:(UITextRange *)textRange
{
    if (!textRange) {
        return NSMakeRange(0, 0);
    }

    NSInteger startOffset = [self _offsetFromTextPosition:textRange.start];
    NSInteger endOffset = [self _offsetFromTextPosition:textRange.end];
    return [self _clampedSelectionRangeWithStart:startOffset end:endOffset];
}

- (void)_notifySelectionChange
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    id<SCValdiContextProtocol> context = [_delegate valdiContextForTextLayoutView:self];
    id<SCValdiViewNodeProtocol> viewNode = [self _resolveValdiViewNode];
    if (!selectionState.selectable || !self.window || !context || !viewNode) {
        return;
    }

    NSInteger selectionStart = (NSInteger)selectionState.selectedRange.location;
    NSInteger selectionEnd = (NSInteger)NSMaxRange(selectionState.selectedRange);

    [context didChangeValue:@[@(selectionStart), @(selectionEnd)]
    forInternedValdiAttribute:SCValdiLabelSelectionKey()
                   inViewNode:viewNode];

    if (selectionState.onSelectionChange) {
        SCValdiMarshallerScoped(marshaller, {
            NSInteger objectIndex = SCValdiMarshallerPushMap(marshaller, 1);
            SCValdiMarshallerPushString(marshaller, [self _currentTextString]);
            SCValdiMarshallerPutMapProperty(marshaller, SCValdiLabelTextKey(), objectIndex);
            SCValdiMarshallerPushInt(marshaller, (int32_t)selectionStart);
            SCValdiMarshallerPutMapProperty(marshaller, SCValdiLabelSelectionStartKey(), objectIndex);
            SCValdiMarshallerPushInt(marshaller, (int32_t)selectionEnd);
            SCValdiMarshallerPutMapProperty(marshaller, SCValdiLabelSelectionEndKey(), objectIndex);
            [selectionState.onSelectionChange performWithMarshaller:marshaller];
        });
    }
}

- (void)_becomeSelectionFirstResponder
{
    if (!self.selectable) {
        return;
    }

    if (!self.isFirstResponder) {
        [self becomeFirstResponder];
    }
}

- (void)_setSelectedRange:(NSRange)selectedRange notify:(BOOL)notify
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    if (!selectionState) {
        return;
    }

    NSRange clampedRange = [self _clampedSelectionRangeWithStart:(NSInteger)selectedRange.location
                                                            end:(NSInteger)NSMaxRange(selectedRange)];
    if (clampedRange.length > 0) {
        [self _becomeSelectionFirstResponder];
    }
    if (NSEqualRanges(selectionState.selectedRange, clampedRange)) {
        return;
    }

    [selectionState.inputDelegate selectionWillChange:self];
    selectionState.selectedRange = clampedRange;
    if (notify) {
        // UITextInteraction may synchronously query geometry from selectionDidChange.
        // Deliver the Valdi event after the range is committed without re-entering UIKit.
        __weak typeof(self) weakSelf = self;
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }
            SCValdiLabelSelectionState *currentSelectionState = strongSelf->_selectionState;
            if (currentSelectionState && NSEqualRanges(currentSelectionState.selectedRange, clampedRange)) {
                [strongSelf _notifySelectionChange];
            }
        });
    }
    [selectionState.inputDelegate selectionDidChange:self];
    [self _updateSelectionInteractionOverlayForCurrentSelection];
}

- (void)_clampSelectionToCurrentText
{
    if (_selectionState) {
        [self _setSelectedRange:_selectionState.selectedRange notify:NO];
    }
}

- (void)_removeSelectionInteractions
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    for (id<UIInteraction> interaction in [selectionState.selectionInstalledInteractions reverseObjectEnumerator]) {
        if ([self.interactions containsObject:interaction]) {
            [self removeInteraction:interaction];
        }
    }

    if (@available(iOS 13.0, *)) {
        UITextInteraction *selectionInteraction = selectionState.selectionInteraction;
        if (selectionInteraction && [self.interactions containsObject:selectionInteraction]) {
            [self removeInteraction:selectionInteraction];
        }
        selectionState.selectionInteraction = nil;
    }

    selectionState.selectionInstalledInteractions = nil;
    [self _removeSelectionInteractionOverlay];
}

- (void)_installSelectionInteractionsIfNeeded
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    if (!selectionState.selectable) {
        return;
    }

    if (@available(iOS 13.0, *)) {
        if (selectionState.selectionInteraction) {
            return;
        }

        NSArray<id<UIInteraction>> *existingInteractions = self.interactions;
        UITextInteraction *selectionInteraction = [UITextInteraction textInteractionForMode:UITextInteractionModeNonEditable];
        selectionInteraction.delegate = self;
        selectionInteraction.textInput = self;
        selectionState.selectionInteraction = selectionInteraction;
        [self addInteraction:selectionInteraction];

        NSMutableArray<id<UIInteraction>> *installedInteractions = [NSMutableArray new];
        for (id<UIInteraction> interaction in self.interactions) {
            if (![existingInteractions containsObject:interaction]) {
                [installedInteractions addObject:interaction];
            }
        }
        selectionState.selectionInstalledInteractions = [installedInteractions copy];
    }
}

- (void)_updateSelectionInteractionOverlayForCurrentSelection
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    if (!selectionState.selectable) {
        return;
    }

    if (selectionState.selectedRange.length == 0 || !self.window) {
        [self _removeSelectionInteractionOverlay];
        return;
    }

    SCValdiTextLayoutSelectionInteractionView *overlayView =
        (SCValdiTextLayoutSelectionInteractionView *)selectionState.selectionInteractionOverlayView;
    if (!overlayView) {
        overlayView = [SCValdiTextLayoutSelectionInteractionView new];
        overlayView.backgroundColor = UIColor.clearColor;
        overlayView.opaque = NO;
        overlayView.isAccessibilityElement = NO;
        overlayView.accessibilityElementsHidden = YES;
        overlayView.textLayoutView = self;
        selectionState.selectionInteractionOverlayView = overlayView;
    }

    if (overlayView.superview != self.window) {
        [self.window addSubview:overlayView];
    }
    [self _updateSelectionInteractionOverlayFrameIfNeeded];
}

- (void)setSelectable:(BOOL)selectable
{
    if (selectable) {
        if (self.selectable) {
            return;
        }

        SCValdiLabelSelectionState *selectionState = [self _ensureSelectionState];
        selectionState.selectable = YES;
        [self _setSelectedRange:selectionState.selectedRange notify:NO];
    } else {
        if (!_selectionState) {
            return;
        }

        if (self.isFirstResponder) {
            [self resignFirstResponder];
        }
        [self _removeSelectionInteractions];
        _selectionState = nil;
    }
}

- (BOOL)setSelection:(NSArray *)selection
{
    if (selection.count != 2) {
        SCLogValdiError(@"Setting text selection requires a start and end point");
        return NO;
    }
    if (![selection[0] isKindOfClass:[NSNumber class]] || ![selection[1] isKindOfClass:[NSNumber class]]) {
        SCLogValdiError(@"Setting text selection requires number start and end points");
        return NO;
    }

    if (_selectionState) {
        [self _setSelectedRange:[self _clampedSelectionRangeWithStart:[selection[0] integerValue]
                                                                 end:[selection[1] integerValue]]
                         notify:YES];
    }
    return YES;
}

- (void)setOnSelectionChange:(id<SCValdiFunction>)onSelectionChange
{
    if (!onSelectionChange && !_selectionState) {
        return;
    }
    [self _ensureSelectionState].onSelectionChange = onSelectionChange;
}

- (void)setOnTextSelectionMenu:(id<SCValdiFunction>)onTextSelectionMenu
{
    if (!onTextSelectionMenu && !_selectionState) {
        return;
    }
    [self _ensureSelectionState].onTextSelectionMenu = onTextSelectionMenu;
}

- (void)setOnTextSelectionMenuAction:(id<SCValdiFunction>)onTextSelectionMenuAction
{
    if (!onTextSelectionMenuAction && !_selectionState) {
        return;
    }
    [self _ensureSelectionState].onTextSelectionMenuAction = onTextSelectionMenuAction;
}

#pragma mark - UIResponder edit actions

- (BOOL)canBecomeFirstResponder
{
    return self.selectable || [super canBecomeFirstResponder];
}

- (BOOL)becomeFirstResponder
{
    BOOL becameFirstResponder = self.selectable && [super becomeFirstResponder];
    if (becameFirstResponder) {
        [self _updateSelectionInteractionOverlayForCurrentSelection];
    }
    return becameFirstResponder;
}

- (BOOL)resignFirstResponder
{
    if (_selectionState.selectedRange.length > 0) {
        [self _setSelectedRange:NSMakeRange(0, 0) notify:YES];
    }
    [self _removeSelectionInteractionOverlay];
    return [super resignFirstResponder];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
    if (!self.selectable) {
        return [super canPerformAction:action withSender:sender];
    }

    if (action == @selector(copy:)) {
        return _selectionState.selectedRange.length > 0;
    }

    if (action == @selector(selectAll:)) {
        return [self _currentTextLength] > 0 && _selectionState.selectedRange.length < [self _currentTextLength];
    }

    return NO;
}

- (void)copy:(id)sender
{
    if (!self.selectable || _selectionState.selectedRange.length == 0) {
        return;
    }

    NSString *selectedText = [[self _currentTextString] substringWithRange:_selectionState.selectedRange];
    if (selectedText) {
        UIPasteboard.generalPasteboard.string = selectedText;
    }
}

- (void)selectAll:(id)sender
{
    if (!self.selectable) {
        return;
    }

    [self _setSelectedRange:NSMakeRange(0, [self _currentTextLength]) notify:YES];
}

- (NSArray<UIMenuElement *> *)_customEditMenuActionsForTextRange:(NSRange)range API_AVAILABLE(ios(16.0))
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    NSDictionary<NSString *, id> *event = SCValdiTextSelectionMenuEventForText([self _currentTextString], range);
    NSArray<NSDictionary<NSString *, NSString *> *> *menuActions =
        SCValdiTextSelectionMenuActionsForProvider(selectionState.onTextSelectionMenu, event);

    NSMutableArray<UIMenuElement *> *customActions = [NSMutableArray arrayWithCapacity:menuActions.count];
    for (NSDictionary<NSString *, NSString *> *menuAction in menuActions) {
        NSString *actionID = menuAction[SCValdiTextSelectionMenuActionIDKey];
        NSString *title = menuAction[SCValdiTextSelectionMenuActionTitleKey];
        __weak typeof(self) weakSelf = self;
        UIAction *action = [UIAction actionWithTitle:title image:nil identifier:nil handler:^(__kindof UIAction *uiAction) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }
            NSDictionary<NSString *, id> *actionEvent =
                SCValdiTextSelectionMenuEventForText([strongSelf _currentTextString], range);
            SCValdiPerformTextSelectionMenuAction(strongSelf->_selectionState.onTextSelectionMenuAction, actionID, actionEvent);
        }];
        [customActions addObject:action];
    }
    return customActions;
}

- (nullable UIMenu *)editMenuForTextRange:(UITextRange *)textRange
                         suggestedActions:(NSArray<UIMenuElement *> *)suggestedActions API_AVAILABLE(ios(16.0))
{
    if (!self.selectable) {
        return nil;
    }

    NSArray<UIMenuElement *> *customActions = [self _customEditMenuActionsForTextRange:[self _rangeFromTextRange:textRange]];
    if (customActions.count == 0) {
        return nil;
    }

    NSMutableArray<UIMenuElement *> *children = [NSMutableArray arrayWithCapacity:customActions.count + suggestedActions.count];
    [children addObjectsFromArray:customActions];
    [children addObjectsFromArray:suggestedActions];
    return [UIMenu menuWithTitle:@"" children:children];
}

#pragma mark - UITextInteractionDelegate

- (BOOL)interactionShouldBegin:(UITextInteraction *)interaction atPoint:(CGPoint)point API_AVAILABLE(ios(13.0))
{
    if (!self.selectable || [self _currentTextLength] == 0) {
        return NO;
    }

    [self _becomeSelectionFirstResponder];
    return YES;
}

#pragma mark - UIKeyInput

- (BOOL)hasText
{
    return [self _currentTextLength] > 0;
}

- (void)insertText:(NSString *)text
{
    // Labels are non-editable. Selection is supported, mutation is intentionally ignored.
}

- (void)deleteBackward
{
    // Labels are non-editable. Selection is supported, mutation is intentionally ignored.
}

#pragma mark - UITextInput

- (NSString *)textInRange:(UITextRange *)range
{
    NSRange resolvedRange = [self _rangeFromTextRange:range];
    NSString *text = [self _currentTextString];
    if (NSMaxRange(resolvedRange) > text.length) {
        return @"";
    }

    return [text substringWithRange:resolvedRange];
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
    // Labels are non-editable. Selection is supported, mutation is intentionally ignored.
}

- (UITextRange *)selectedTextRange
{
    if (!self.selectable) {
        return nil;
    }

    return [SCValdiLabelTextRange rangeWithNSRange:_selectionState.selectedRange];
}

- (void)setSelectedTextRange:(UITextRange *)selectedTextRange
{
    if (!self.selectable) {
        return;
    }

    NSRange selectedRange = [self _rangeFromTextRange:selectedTextRange];
    if (selectedRange.length == 0 && _selectionState.selectedRange.length == 0 && [self _currentTextLength] > 0) {
        UITextPosition *position = [SCValdiLabelTextPosition positionWithOffset:(NSInteger)selectedRange.location];
        UITextRange *wordTextRange = [self.tokenizer rangeEnclosingPosition:position
                                                            withGranularity:UITextGranularityWord
                                                                inDirection:(UITextDirection)UITextStorageDirectionForward];
        NSRange wordRange = [self _rangeFromTextRange:wordTextRange];
        if (wordRange.location != NSNotFound && wordRange.length > 0) {
            selectedRange = wordRange;
        }
    }

    [self _setSelectedRange:selectedRange notify:YES];
}

- (UITextRange *)markedTextRange
{
    return nil;
}

- (NSDictionary<NSAttributedStringKey, id> *)markedTextStyle
{
    return _selectionState.markedTextStyle;
}

- (void)setMarkedTextStyle:(NSDictionary<NSAttributedStringKey, id> *)markedTextStyle
{
    if (_selectionState) {
        _selectionState.markedTextStyle = markedTextStyle;
    }
}

- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange
{
    // Labels are non-editable. Marked text is not supported.
}

- (void)unmarkText
{
}

- (UITextPosition *)beginningOfDocument
{
    return [SCValdiLabelTextPosition positionWithOffset:0];
}

- (UITextPosition *)endOfDocument
{
    return [SCValdiLabelTextPosition positionWithOffset:(NSInteger)[self _currentTextLength]];
}

- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition toPosition:(UITextPosition *)toPosition
{
    return [SCValdiLabelTextRange rangeWithStartOffset:[self _offsetFromTextPosition:fromPosition]
                                             endOffset:[self _offsetFromTextPosition:toPosition]];
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position offset:(NSInteger)offset
{
    NSInteger textLength = (NSInteger)[self _currentTextLength];
    NSInteger resolvedOffset = MAX(0, MIN(textLength, [self _offsetFromTextPosition:position] + offset));
    return [SCValdiLabelTextPosition positionWithOffset:resolvedOffset];
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position
                             inDirection:(UITextLayoutDirection)direction
                                  offset:(NSInteger)offset
{
    NSInteger signedOffset = offset;
    if (direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp) {
        signedOffset = -offset;
    }

    return [self positionFromPosition:position offset:signedOffset];
}

- (NSComparisonResult)comparePosition:(UITextPosition *)position toPosition:(UITextPosition *)other
{
    NSInteger lhs = [self _offsetFromTextPosition:position];
    NSInteger rhs = [self _offsetFromTextPosition:other];
    if (lhs < rhs) {
        return NSOrderedAscending;
    }
    if (lhs > rhs) {
        return NSOrderedDescending;
    }
    return NSOrderedSame;
}

- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition
{
    return [self _offsetFromTextPosition:toPosition] - [self _offsetFromTextPosition:from];
}

- (id<UITextInputDelegate>)inputDelegate
{
    return _selectionState.inputDelegate;
}

- (void)setInputDelegate:(id<UITextInputDelegate>)inputDelegate
{
    if (_selectionState) {
        _selectionState.inputDelegate = inputDelegate;
    }
}

- (id<UITextInputTokenizer>)tokenizer
{
    SCValdiLabelSelectionState *selectionState = _selectionState;
    if (!selectionState) {
        return [[UITextInputStringTokenizer alloc] initWithTextInput:self];
    }

    if (!selectionState.tokenizer) {
        selectionState.tokenizer = [[UITextInputStringTokenizer alloc] initWithTextInput:self];
    }

    return selectionState.tokenizer;
}

- (UITextPosition *)positionWithinRange:(UITextRange *)range farthestInDirection:(UITextLayoutDirection)direction
{
    return (direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp) ? range.start : range.end;
}

- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction
{
    NSInteger offset = [self _offsetFromTextPosition:position];
    NSInteger textLength = (NSInteger)[self _currentTextLength];
    if (direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp) {
        return [SCValdiLabelTextRange rangeWithStartOffset:MAX(0, offset - 1) endOffset:offset];
    }

    return [SCValdiLabelTextRange rangeWithStartOffset:offset endOffset:MIN(textLength, offset + 1)];
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
    return [_delegate textLayoutViewIsRightToLeft:self] ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
}

- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection forRange:(UITextRange *)range
{
}

- (CGRect)firstRectForRange:(UITextRange *)range
{
    NSRange resolvedRange = [self _rangeFromTextRange:range];
    if (!_textLayout) {
        return CGRectZero;
    }

    if (resolvedRange.length == 0) {
        return [_textLayout caretRectForCharacterIndex:resolvedRange.location inDrawingRect:self.bounds];
    }

    NSArray<NSValue *> *rects = [_textLayout selectionRectsForRange:resolvedRange inDrawingRect:self.bounds];
    return rects.firstObject.CGRectValue;
}

- (CGRect)caretRectForPosition:(UITextPosition *)position
{
    if (!_textLayout) {
        return CGRectZero;
    }

    NSInteger offset = [self _offsetFromTextPosition:position];
    return [_textLayout caretRectForCharacterIndex:(NSUInteger)offset
                                    inDrawingRect:self.bounds];
}

- (NSArray<UITextSelectionRect *> *)selectionRectsForRange:(UITextRange *)range
{
    if (!_textLayout) {
        return @[];
    }

    NSRange resolvedRange = [self _rangeFromTextRange:range];
    NSArray<NSValue *> *rectValues = [_textLayout selectionRectsForRange:resolvedRange
                                                           inDrawingRect:self.bounds];
    NSMutableArray<UITextSelectionRect *> *selectionRects = [NSMutableArray new];
    NSWritingDirection writingDirection = [_delegate textLayoutViewIsRightToLeft:self] ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    for (NSUInteger index = 0; index < rectValues.count; index++) {
        SCValdiLabelSelectionRect *selectionRect = [SCValdiLabelSelectionRect new];
        selectionRect.valdiRect = rectValues[index].CGRectValue;
        selectionRect.valdiWritingDirection = writingDirection;
        selectionRect.valdiContainsStart = index == 0;
        selectionRect.valdiContainsEnd = index == rectValues.count - 1;
        selectionRect.valdiIsVertical = NO;
        [selectionRects addObject:selectionRect];
    }

    return selectionRects;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point
{
    if (!_textLayout) {
        return [SCValdiLabelTextPosition positionWithOffset:0];
    }

    NSInteger offset = [_textLayout insertionIndexAtPoint:point];
    return [SCValdiLabelTextPosition positionWithOffset:offset];
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range
{
    NSInteger offset = [self _offsetFromTextPosition:[self closestPositionToPoint:point]];
    NSRange resolvedRange = [self _rangeFromTextRange:range];
    NSInteger start = (NSInteger)resolvedRange.location;
    NSInteger end = (NSInteger)NSMaxRange(resolvedRange);
    NSInteger clampedOffset = MAX(start, MIN(end, offset));
    return [SCValdiLabelTextPosition positionWithOffset:clampedOffset];
}

- (UITextRange *)characterRangeAtPoint:(CGPoint)point
{
    if (!_textLayout) {
        return nil;
    }

    NSInteger characterIndex = [_textLayout characterIndexAtPoint:point];
    if (characterIndex == NSNotFound || characterIndex >= (NSInteger)[self _currentTextLength]) {
        return nil;
    }

    return [SCValdiLabelTextRange rangeWithStartOffset:characterIndex endOffset:characterIndex + 1];
}

- (UIView *)textInputView
{
    return self;
}

- (UITextStorageDirection)selectionAffinity
{
    return _selectionState ? _selectionState.selectionAffinity : UITextStorageDirectionForward;
}

- (void)setSelectionAffinity:(UITextStorageDirection)selectionAffinity
{
    if (_selectionState) {
        _selectionState.selectionAffinity = selectionAffinity;
    }
}

- (NSDictionary<NSAttributedStringKey, id> *)textStylingAtPosition:(UITextPosition *)position
                                                       inDirection:(UITextStorageDirection)direction
{
    NSAttributedString *attributedString = _processedText.attributedString;
    NSInteger offset = [self _offsetFromTextPosition:position];
    if (offset < 0 || offset >= (NSInteger)attributedString.length) {
        return nil;
    }

    return [attributedString attributesAtIndex:(NSUInteger)offset effectiveRange:nil];
}

- (UITextPosition *)positionWithinRange:(UITextRange *)range atCharacterOffset:(NSInteger)offset
{
    NSRange resolvedRange = [self _rangeFromTextRange:range];
    NSInteger resolvedOffset = (NSInteger)resolvedRange.location + offset;
    NSInteger rangeEnd = (NSInteger)NSMaxRange(resolvedRange);
    NSInteger clampedOffset = MAX((NSInteger)resolvedRange.location, MIN(rangeEnd, resolvedOffset));
    return [SCValdiLabelTextPosition positionWithOffset:clampedOffset];
}

- (NSInteger)characterOffsetOfPosition:(UITextPosition *)position withinRange:(UITextRange *)range
{
    NSRange resolvedRange = [self _rangeFromTextRange:range];
    NSInteger offset = [self _offsetFromTextPosition:position] - (NSInteger)resolvedRange.location;
    return MAX(0, MIN((NSInteger)resolvedRange.length, offset));
}

@end
