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
#import "valdi/ios/Text/SCValdiInlineTextChildLayout.h"
#import "valdi/ios/Text/SCValdiProcessedText.h"
#import "valdi/ios/Text/SCValdiTextAnimationTransform.h"
#import "valdi/ios/Text/SCValdiTextGradientHelper.h"
#import "valdi/ios/Text/SCValdiTextLayout.h"
#import "valdi/ios/Views/SCValdiTextAnimationGroup.h"

#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/SCValdiContentViewProviding.h"
#import "valdi_core/SCValdiTextInputTraitAttributes.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiResult.h"
#import "valdi_core/SCValdiConfigurableTextHolder.h"
#import "valdi_core/SCValdiConfigurableTextHolderTraitAttributes.h"
#import "valdi_core/SCValdiTextInputUnfocusReason.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"
#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"
#import "valdi/ios/Views/SCValdiLabelSelection.h"

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

static CGFloat SCValdiAnimatedTextVerticalOverflowPadding(SCValdiProcessedText *processedText,
                                                          NSAttributedString *attributedString)
{
    if (processedText == nil || attributedString.length == 0) {
        return 0.0;
    }

    __block CGFloat maxTranslation = 0.0;
    __block CGFloat maxScaleOverflow = 0.0;
    [processedText enumerateAnimationTransformsUsingBlock:^(SCValdiTextAnimationTransform *animationTransform,
                                                            NSRange range,
                                                            BOOL *stop) {
        (void)stop;
        if (range.length == 0 || range.location >= attributedString.length) {
            return;
        }
        UIFont *font = ObjectAs([attributedString attribute:NSFontAttributeName
                                                    atIndex:range.location
                                             effectiveRange:nil], UIFont);
        maxTranslation = MAX(maxTranslation, fabs(animationTransform.translationY));
        CGFloat scale = animationTransform.scale;
        CGFloat lineHeight = font != nil ? font.lineHeight : 0.0;
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
    return backgroundOnlyAttributedString;
}

// contentSize includes the current textContainerInset, whose top carries the previously applied
// gravity correction. Strip the current insets and re-add the base ones so the correction doesn't
// compound across passes (an empty centered text view would otherwise settle above center).
// contentSize can lag a just-written inset, so clamp the stripped height at zero to keep a stale
// read from going negative and overshooting the correction.
static CGFloat SCValdiTextViewContentHeightFromContentSize(UITextView *textView,
                                                           CGFloat baseTopInset,
                                                           CGFloat baseBottomInset)
{
    UIEdgeInsets currentInset = textView.textContainerInset;
    CGFloat strippedContentHeight = textView.contentSize.height - currentInset.top - currentInset.bottom;
    return MAX(0.0, strippedContentHeight) + baseTopInset + baseBottomInset;
}

// Number of line fragments TextKit laid out for `glyphRange`.
static NSUInteger SCValdiTextViewLaidOutLineCount(NSLayoutManager *layoutManager, NSRange glyphRange)
{
    NSUInteger lineCount = 0;
    NSUInteger glyphIndex = glyphRange.location;
    NSUInteger glyphEnd = NSMaxRange(glyphRange);
    while (glyphIndex < glyphEnd) {
        NSRange lineRange = NSMakeRange(0, 0);
        [layoutManager lineFragmentRectForGlyphAtIndex:glyphIndex effectiveRange:&lineRange];
        if (lineRange.length == 0) {
            break;
        }
        glyphIndex = NSMaxRange(lineRange);
        lineCount++;
    }
    return lineCount;
}

// YES when the container was too short for the rest of the text: TextKit drops a line fragment
// whole rather than clipping it. maximumNumberOfLines truncates by design and still wants centering.
static BOOL SCValdiTextViewLayoutIsHeightTruncated(UITextView *textView)
{
    if (textView.scrollEnabled) {
        // A scrolling text view gets an unbounded container, so its height never cuts the text off.
        return NO;
    }

    NSLayoutManager *layoutManager = textView.layoutManager;
    NSTextContainer *textContainer = textView.textContainer;
    NSRange laidOutGlyphs = [layoutManager glyphRangeForTextContainer:textContainer];
    NSRange laidOutCharacters = [layoutManager characterRangeForGlyphRange:laidOutGlyphs actualGlyphRange:NULL];
    if (NSMaxRange(laidOutCharacters) >= textView.textStorage.length) {
        return NO;
    }

    NSUInteger maximumNumberOfLines = textContainer.maximumNumberOfLines;
    if (maximumNumberOfLines == 0) {
        return YES;
    }
    return SCValdiTextViewLaidOutLineCount(layoutManager, laidOutGlyphs) < maximumNumberOfLines;
}

static CGFloat SCValdiTextViewContentHeightForGravity(UITextView *textView,
                                                      CGFloat baseTopInset,
                                                      CGFloat baseBottomInset)
{
    if (textView.attributedText.length == 0) {
        return SCValdiTextViewContentHeightFromContentSize(textView, baseTopInset, baseBottomInset);
    }

    [textView.layoutManager ensureLayoutForTextContainer:textView.textContainer];
    CGRect usedRect = [textView.layoutManager usedRectForTextContainer:textView.textContainer];
    if (CGRectIsEmpty(usedRect)) {
        return SCValdiTextViewContentHeightFromContentSize(textView, baseTopInset, baseBottomInset);
    }

    return CGRectGetMaxY(usedRect) + baseTopInset + baseBottomInset;
}

@interface SCValdiTextView() <SCValdiAttributedTextOnTapGestureRecognizerFunctionProvider, SCValdiContentViewProviding, SCValdiTextAnimationGroupParticipant>
@end

@interface SCValdiTextViewDisplayLinkProxy : NSProxy
- (instancetype)initWithTarget:(id)target;
@end

@implementation SCValdiTextViewDisplayLinkProxy {
    __weak id _target;
}

- (instancetype)initWithTarget:(id)target {
    _target = target;
    return self;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    [invocation invokeWithTarget:_target];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
    return [_target methodSignatureForSelector:sel];
}

@end

@implementation SCValdiTextView {
    /// YES if pressing the return key should dismiss the keyboard, o/w NO
    BOOL _closesWhenReturnKeyPressed;
    /// The maximum length of the text
    NSNumber *_characterLimit;
    /// YES if all text should be selected on begin editing
    BOOL _selectTextOnFocus;
    /// YES if read-only text should allow selection
    BOOL _selectable;
    /// YES if we discard any typed newline
    BOOL _ignoreNewlines;
    BOOL _enabled;
    BOOL _updating;
    BOOL _updatingContentInset;
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
    id<SCValdiFunction> _Nullable _onTextSelectionMenu;
    id<SCValdiFunction> _Nullable _onTextSelectionMenuAction;

    SCValdiTextViewPlaceholder *_placeholder;
    SCValdiTextViewInternal *_textView;
    SCValdiTextViewEffectsLayoutManager *_effectsLayoutManager;
    NSAttributedString *_attributedTextOnTapString;
    SCValdiProcessedText *_processedText;
    BOOL _hasOnTapGestureRecognizer;
    SCValdiCustomUnderlineStyle *_customUnderlineStyle;
    BOOL _hasCustomUnderlineAttribute;
    BOOL _hasTextOverflow;
    NSLineBreakMode _textOverflowLineBreakMode;
    SCValdiTextGradientHelper *_textGradientHelper;

    SCValdiTextInputUnfocusReason _lastUnfocusReason;
    /// YES if a focus request arrived before the view was attached to a window; applied in didMoveToWindow
    BOOL _pendingFocused;

    // State for the animated text overlay used when per-glyph transforms are present.
    SCValdiTextViewInternal *_animatedTextView;
    UIView *_valdiChildrenContainerView;
    SCValdiTextViewEffectsLayoutManager *_animatedTextEffectsLayoutManager;
    CGFloat _animatedTextVerticalOverflowPadding;
    BOOL _slowClipping;
    CADisplayLink *_animatedTextDisplayLink;
    __weak SCValdiTextAnimationGroup *_textAnimationGroup;
    NSUInteger _textAnimationPartCount;

    // Deferred-initialization state. _animatedTextView (the per-glyph animation overlay) and
    // _placeholder are built lazily on first use, so a plain text view or label constructs a single
    // UITextView/TextKit-1 stack instead of three. These ivars stash configuration that can arrive
    // before the lazily-created subview exists so -_ensureAnimatedTextView / -_ensurePlaceholder can
    // replay it at creation time. _textAnimationCoordinator mirrors the overlay layout manager's own
    // weak ownership (see SCValdiTextViewEffectsLayoutManager).
    UIColor *_pendingPlaceholderColor;
    NSArray *_textShadow;
    __weak SCValdiTextAnimationCoordinator *_textAnimationCoordinator;
    NSUInteger _textAnimationBasePartIndex;
}

+ (BOOL)valdi_managesChildFrames
{
    return YES;
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

        // _animatedTextView (per-glyph animation overlay) and _placeholder are created lazily; see
        // -_ensureAnimatedTextView / -_ensurePlaceholder. Most text views never animate or show a
        // placeholder, so this avoids building two extra UITextView/TextKit stacks up front.
        [self addSubview:_textView];

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
        _enabled = YES;
        _selectable = YES;

        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(_applicationDidBecomeActive)
                                                   name:UIApplicationDidBecomeActiveNotification
                                                 object:nil];
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(_windowDidBecomeKey:)
                                                   name:UIWindowDidBecomeKeyNotification
                                                 object:nil];
    }

    return self;
}

// Builds the per-glyph animation overlay on first enable. Everywhere else a nil _animatedTextView is
// harmless (ObjC messaging to nil is a no-op), so callers don't need to guard. Replays the state an
// eagerly-created overlay would already hold: view node, processed text, animation coordinator, and
// text shadow. Per-use geometry (insets, line-break mode, max lines, attributed text) is (re)applied
// by -_updateAnimatedTextOverlayWithAttributedString:isEnabled: immediately after this returns.
- (SCValdiTextViewInternal *)_ensureAnimatedTextView
{
    if (_animatedTextView != nil) {
        return _animatedTextView;
    }

    _animatedTextEffectsLayoutManager = [SCValdiTextViewEffectsLayoutManager new];
    _animatedTextEffectsLayoutManager.valdiViewNode = self.valdiViewNode;
    SCValdiTextViewInternal *animatedTextView =
        [[SCValdiTextViewInternal alloc] initWithFrame:self.bounds
                                         layoutManager:_animatedTextEffectsLayoutManager];
    animatedTextView.backgroundColor = [UIColor clearColor];
    animatedTextView.userInteractionEnabled = NO;
    animatedTextView.isAccessibilityElement = NO;
    animatedTextView.accessibilityElementsHidden = YES;
    animatedTextView.editable = NO;
    animatedTextView.textContainerInset = UIEdgeInsetsZero;
    animatedTextView.textContainer.lineFragmentPadding = 0;
    animatedTextView.showsHorizontalScrollIndicator = NO;
    animatedTextView.adjustsFontForContentSizeCategory = NO;
    animatedTextView.hidden = YES;
    _animatedTextView = animatedTextView;
    // Original z-order: the overlay sits above the base text view. The caller brings it further
    // forward while an animation is active.
    [self insertSubview:animatedTextView aboveSubview:_textView];

    // The overlay only paints transformed glyphs, so it never carries background effects or the
    // custom underline style (matching -_updateEffectsLayoutManager).
    _animatedTextEffectsLayoutManager.effects = nil;
    _animatedTextEffectsLayoutManager.customUnderlineStyle = nil;
    _animatedTextEffectsLayoutManager.processedText = _processedText;
    _animatedTextEffectsLayoutManager.textAnimationCoordinator = _textAnimationCoordinator;
    _animatedTextEffectsLayoutManager.textAnimationBasePartIndex = _textAnimationBasePartIndex;
    if (_textShadow != nil) {
        SCValdiSetTextHolderTextShadow(animatedTextView, _textShadow);
    }
    // Catch up on text-overflow state that arrived before the overlay existed (scrollEnabled and
    // lineBreakMode, via -_applyTextOverflowAttributes as -_ensurePlaceholder does). The scroll
    // offset can't be seeded here: the overlay has no content yet, so the geometry writes that follow
    // would make UIScrollView clamp it straight back to zero. -_syncAnimatedTextOverlayContentOffset
    // mirrors it once those writes are done.
    [self _applyTextOverflowAttributes];
    return animatedTextView;
}

// Builds the placeholder text view the first time a non-empty placeholder string is applied; a color
// that arrives first is stashed in _pendingPlaceholderColor and replayed here. Replays the
// font/overflow/line/inset/shadow configuration the eager placeholder would have accumulated so it
// renders identically.
- (SCValdiTextViewPlaceholder *)_ensurePlaceholder
{
    if (_placeholder != nil) {
        return _placeholder;
    }

    SCValdiTextViewPlaceholder *placeholder = [[SCValdiTextViewPlaceholder alloc] initWithFrame:self.bounds];
    placeholder.textColor = _pendingPlaceholderColor ?: [UIColor lightGrayColor];
    placeholder.userInteractionEnabled = NO;
    placeholder.backgroundColor = [UIColor clearColor];
    placeholder.textContainerInset = UIEdgeInsetsZero;
    placeholder.textContainer.lineFragmentPadding = 0;
    placeholder.showsHorizontalScrollIndicator = NO;
    placeholder.adjustsFontForContentSizeCategory = NO;
    // Hidden whenever there is real content, since the placeholder can be created after the value is
    // bound (attribute application order is not guaranteed).
    placeholder.hidden = _textView.attributedText.length > 0;
    _placeholder = placeholder;
    _pendingPlaceholderColor = nil;
    // Original z-order: base text view, animation overlay, placeholder on top. Anchoring to the
    // overlay when it already exists keeps that order regardless of which subview materializes first.
    [self insertSubview:placeholder aboveSubview:(_animatedTextView ?: _textView)];

    BOOL isRightToLeft = self.valdiViewNode.isRightToLeft;
    UITraitCollection *traitCollection = self.valdiContext.traitCollection;
    SCValdiSetTextHolderAttributes(placeholder, [self fontAttributes], traitCollection, isRightToLeft, nil);
    [self _applyNumberOfLinesAttributes];
    [self _applyTextOverflowAttributes];
    [self _updatePlaceholderInset];
    if (_textShadow != nil) {
        SCValdiSetTextHolderTextShadow(placeholder, _textShadow);
    }
    return placeholder;
}

- (void)dealloc
{
    [_textAnimationGroup unregisterTextAnimationParticipant:self];
    [_animatedTextDisplayLink invalidate];
    [_textView removeObserver:self forKeyPath:kSCValdiTextViewContentSizeKey];
    _textView.delegate = nil;
    _textView.textStorage.delegate = nil;
}

#pragma mark - UIView+Valdi

- (void)didMoveToValdiContext:(id<SCValdiContextProtocol>)valdiContext
                      viewNode:(id<SCValdiViewNodeProtocol>)viewNode
{
    (void)valdiContext;
    _effectsLayoutManager.valdiViewNode = viewNode;
    _animatedTextEffectsLayoutManager.valdiViewNode = viewNode;
}

- (BOOL)willEnqueueIntoValdiPool
{
    [_animatedTextEffectsLayoutManager saveAnimatedTextProgress];
    [_animatedTextEffectsLayoutManager clearAnimatedTextProgress];
    // A pooled view must not keep a display link running. clearAnimatedTextProgress does not stop it
    // on its own: the next fire calls invalidateAnimatedTextProgress, which recomputes the animation
    // ranges from the still-populated _processedText, so hasActiveAnimationRanges comes back YES and
    // the link never self-terminates — leaving every recycled animated text view ticking at display
    // rate while it sits in the pool.
    [self _stopAnimatedTextDisplayLink];
    [_textAnimationGroup unregisterTextAnimationParticipant:self];
    _textAnimationGroup = nil;
    _textAnimationPartCount = 0;
    [_textView unmarkText];
    [_textView resignFirstResponder];
    _pendingFocused = NO;
    _lastUnfocusReason = SCValdiTextInputUnfocusReasonUnknown;

    return self.class == [SCValdiTextView class];
}

- (void)didMoveToSuperview
{
    [super didMoveToSuperview];
    [self _updateTextAnimationGroupRegistration];
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    [self _updateTextAnimationGroupRegistration];
    [self _applyPendingFocusedIfNeeded];
}

- (void)_applicationDidBecomeActive
{
    // Every live instance observes this notification, including recycled views sitting in the
    // Valdi pool; skip the dispatch when there is nothing pending so foregrounding doesn't
    // enqueue no-op blocks per text input.
    if (!_pendingFocused) {
        return;
    }
    // UIKit still refuses first-responder transitions while this notification is being
    // delivered (the scene/window state is settling); attempt on the next runloop turn.
    __weak typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf _applyPendingFocusedIfNeeded];
    });
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    if (notification.object == self.window) {
        [self _applyPendingFocusedIfNeeded];
    }
}

- (void)_applyPendingFocusedIfNeeded
{
    if (!_pendingFocused || self.window == nil) {
        return;
    }
    // Focusing while another window is key (system alert, notification banner) doesn't bring up
    // the keyboard even when becomeFirstResponder reports success; keep the intent pending until
    // this view's window is key again.
    if (!self.window.isKeyWindow) {
        return;
    }
    if (_textView.isFirstResponder || [_textView becomeFirstResponder]) {
        _pendingFocused = NO;
    }
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
    [self _updateInlineTextAttachmentsIfNeeded];
    [super layoutSubviews];

    [self _updateFrame];
    [self _updateContentInset];
    [self _updatePlaceholderInset];
    [self _updateOnLayoutIfNeeded];
    [self _updateInlineTextChildFrames];
    [self _updateInlineTextChildAnimations];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    if (scrollView == _textView) {
        [self _syncAnimatedTextOverlayContentOffset];
    }
}

// The overlay renders the same text as _textView, so it has to scroll with it. UIScrollView re-clamps
// contentOffset against contentSize whenever the frame or the text container inset changes, which
// zeroes the mirror while the overlay is still empty (it is created with no attributedText and only
// receives content once an animation enables). Re-mirroring after those writes keeps an overlay that
// materializes over already-scrolled text aligned, instead of waiting for the next user scroll.
- (void)_syncAnimatedTextOverlayContentOffset
{
    if (_animatedTextView == nil) {
        return;
    }
    CGPoint contentOffset = _textView.contentOffset;
    if (!CGPointEqualToPoint(_animatedTextView.contentOffset, contentOffset)) {
        _animatedTextView.contentOffset = contentOffset;
    }
}

- (void)_updateFrame
{
    _valdiChildrenContainerView.frame = self.bounds;
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
                      backgroundEffectPadding:_effectsLayoutManager.backgroundPadding
                              traitCollection:self.valdiContext.traitCollection];
}

- (void)_updateTextViewInset:(UITextView *)textView
{
    UIEdgeInsets textContainerInset = textView.textContainerInset;
    CGFloat baseTopInset = textContainerInset.bottom;
    CGFloat baseBottomInset = textContainerInset.bottom;
    CGFloat boundsHeight = textView.bounds.size.height;

    if (textContainerInset.top != baseTopInset && SCValdiTextViewLayoutIsHeightTruncated(textView)) {
        textContainerInset.top = baseTopInset;
        textView.textContainerInset = textContainerInset;
    }

    CGFloat contentSizeHeight = SCValdiTextViewContentHeightForGravity(textView, baseTopInset, baseBottomInset);
    if (SCValdiTextViewLayoutIsHeightTruncated(textView)) {
        contentSizeHeight = MAX(contentSizeHeight, boundsHeight);
    }
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
    CGFloat onePixel = 1.0 / MAX(1.0, textView.traitCollection.displayScale);
    topCorrection = MIN(topCorrection, MAX(0.0, boundsHeight - contentSizeHeight - onePixel));

    textContainerInset.top = baseTopInset + topCorrection;
    textContainerInset.bottom = baseBottomInset;
    if (!UIEdgeInsetsEqualToEdgeInsets(textView.textContainerInset, textContainerInset)) {
        textView.textContainerInset = textContainerInset;
    }
    if (!UIEdgeInsetsEqualToEdgeInsets(textView.contentInset, UIEdgeInsetsZero)) {
        textView.contentInset = UIEdgeInsetsZero;
    }
}

- (void)_updateContentInset
{
    if (_updatingContentInset) {
        return;
    }
    _updatingContentInset = YES;
    [self _updateTextViewInset:(_textView)];
    [self _updateTextViewInset:(_animatedTextView)];
    [self _syncAnimatedTextOverlayContentOffset];
    _updatingContentInset = NO;
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

    if (_processedText == nil) {
        return;
    }
    UITextView *textView = _textView;

    [_processedText enumerateOnLayoutCallbacksUsingBlock:^(id<SCValdiFunction> callback, NSRange range, BOOL *stop) {
        (void)stop;
        UITextPosition *startPosition = [textView positionFromPosition:textView.beginningOfDocument offset:range.location];
        UITextPosition *endPosition = [textView positionFromPosition:startPosition offset:range.length];
        UITextRange *textRange = [textView textRangeFromPosition:startPosition toPosition:endPosition];
        CGRect newBounds = [textView firstRectForRange:textRange];
        SCValdiMarshallerScoped(marshaller, {
            SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.x + textView.contentInset.left));
            SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.origin.y + textView.contentInset.top));
            SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.width));
            SCValdiMarshallerPushDouble(marshaller, CGFloatNormalize(newBounds.size.height));
            [callback performWithMarshaller:marshaller];
        });
    }];

    _updateOnLayout = NO;
}

- (void)_updateInlineTextAttachmentsIfNeeded
{
    if (!_processedText.hasInlineViewAttachment) {
        return;
    }
    if ([_processedText updateInlineAttachments]) {
        _textView.attributedText = _processedText.attributedString;
        [self _updateAnimatedTextOverlayWithAttributedString:_processedText.attributedString
                                                   isEnabled:_processedText.hasAnimationTransform];
        NSRange range = NSMakeRange(0, _textView.attributedText.length);
        [_textView.layoutManager invalidateLayoutForCharacterRange:range actualCharacterRange:NULL];
        [_textView.layoutManager invalidateDisplayForCharacterRange:range];
        [_textView setNeedsDisplay];
        [_animatedTextView setNeedsDisplay];
    }
}

- (void)_updateInlineTextChildFrames
{
    UIView *childrenContainerView = _valdiChildrenContainerView;
    if (childrenContainerView == nil) {
        return;
    }
    NSAttributedString *attributedString = _textView.attributedText;
    if (attributedString.length == 0) {
        SCValdiApplyInlineTextChildFrames(_processedText, nil, nil, CGPointZero, childrenContainerView);
        return;
    }

    NSLayoutManager *layoutManager = _textView.layoutManager;
    NSTextContainer *textContainer = _textView.textContainer;
    [layoutManager ensureLayoutForTextContainer:textContainer];

    UIEdgeInsets textContainerInset = _textView.textContainerInset;
    UIEdgeInsets contentInset = _textView.contentInset;
    CGPoint contentOffset = _textView.contentOffset;
    CGPoint originOffset = CGPointMake(textContainerInset.left + contentInset.left - contentOffset.x,
                                       textContainerInset.top + contentInset.top - contentOffset.y);
    SCValdiApplyInlineTextChildFrames(_processedText,
                                      layoutManager,
                                      textContainer,
                                      originOffset,
                                      childrenContainerView);
}

- (void)_updateInlineTextChildAnimations
{
    UIView *childrenContainerView = _valdiChildrenContainerView;
    if (childrenContainerView == nil) {
        return;
    }
    SCValdiTextViewEffectsLayoutManager *animatedLayoutManager = _animatedTextEffectsLayoutManager;
    SCValdiApplyInlineTextChildAnimations(_processedText,
                                          childrenContainerView,
                                          ^SCValdiTextAnimationPresentation *(NSRange range) {
                                              return [animatedLayoutManager presentationForAnimationRange:range];
                                          });
}

#pragma mark - SCValdiContentViewProviding

- (UIView *)contentViewForInsertingValdiChildren
{
    if (_valdiChildrenContainerView == nil) {
        _valdiChildrenContainerView = [[UIView alloc] initWithFrame:self.bounds];
        _valdiChildrenContainerView.backgroundColor = [UIColor clearColor];
        _valdiChildrenContainerView.isAccessibilityElement = NO;
        [self addSubview:_valdiChildrenContainerView];
    }
    return _valdiChildrenContainerView;
}

- (void)_updateEffectsLayoutManager
{
    _effectsLayoutManager.effects = _backgroundEffects;
    _effectsLayoutManager.customUnderlineStyle = _customUnderlineStyle;
    _effectsLayoutManager.processedText = _processedText;
    _textView.textContainer.lineFragmentPadding = _effectsLayoutManager.backgroundPadding;
    CGFloat textContainerVerticalInset = _effectsLayoutManager.backgroundPadding / 2.0;
    _textView.textContainerInset = UIEdgeInsetsMake(textContainerVerticalInset, 0, textContainerVerticalInset, 0);

    // The base text view already draws background effects. The overlay should only paint transformed glyphs.
    _animatedTextEffectsLayoutManager.effects = nil;
    _animatedTextEffectsLayoutManager.customUnderlineStyle = nil;
    _animatedTextEffectsLayoutManager.processedText = _processedText;
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

- (SCValdiTextViewEffectsLayoutManager *)_animatedTextEffectsLayoutManager
{
    return (SCValdiTextViewEffectsLayoutManager *)_animatedTextView.textStorage.layoutManagers.firstObject;
}

- (void)_startAnimatedTextDisplayLinkIfNeeded
{
    if (_animatedTextDisplayLink != nil) {
        return;
    }

    _animatedTextDisplayLink = [CADisplayLink displayLinkWithTarget:[[SCValdiTextViewDisplayLinkProxy alloc] initWithTarget:self]
                                                              selector:@selector(_animatedTextDisplayLinkDidFire:)];
    [_animatedTextDisplayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)_stopAnimatedTextDisplayLink
{
    [_animatedTextDisplayLink invalidate];
    _animatedTextDisplayLink = nil;
}

- (void)_animatedTextDisplayLinkDidFire:(CADisplayLink *)displayLink
{
    SCValdiTextViewEffectsLayoutManager *layoutManager = [self _animatedTextEffectsLayoutManager];
    BOOL hasActiveAnimationRanges = [layoutManager invalidateAnimatedTextProgress];
    [_animatedTextView setNeedsDisplay];
    [self _updateInlineTextChildAnimations];

    if (!hasActiveAnimationRanges) {
        [self _stopAnimatedTextDisplayLink];
    }
}

- (void)_updateAnimatedTextOverlayWithAttributedString:(NSAttributedString *)attributedString
                                             isEnabled:(BOOL)isEnabled
{
    // When the overlay stays disabled (the common case) _animatedTextView remains nil and every
    // assignment below is a no-op; only materialize it when an animation actually needs it.
    if (isEnabled) {
        [self _ensureAnimatedTextView];
    }
    _animatedTextVerticalOverflowPadding =
        isEnabled && attributedString != nil ? SCValdiAnimatedTextVerticalOverflowPadding(_processedText, attributedString) : 0.0;
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
    if (isEnabled) {
        // The overlay just took on content, so mirror the offset here rather than waiting for a layout
        // pass: enabling an animation does not have to resize the view, and this runs from attribute
        // setters as well as -layoutSubviews. If UIScrollView clamped the write against a contentSize
        // it has not recomputed yet, schedule the pass that lets -_updateContentInset finish the job.
        [self _syncAnimatedTextOverlayContentOffset];
        if (!CGPointEqualToPoint(_animatedTextView.contentOffset, _textView.contentOffset)) {
            [self setNeedsLayout];
        }
        [[self _animatedTextEffectsLayoutManager] invalidateAnimatedTextProgress];
        [self _updateInlineTextChildAnimations];
        if (_textAnimationGroup != nil) {
            [_textAnimationGroup startTextAnimationFrameLoopIfNeeded];
        } else {
            [self _startAnimatedTextDisplayLinkIfNeeded];
        }
    } else {
        [self _stopAnimatedTextDisplayLink];
    }
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
    if (!_processedText.hasOnTap || !attributedString || attributedString.length == 0) {
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

    return [_processedText onTapAtIndex:characterIndex effectiveRange:NULL];
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

- (SCValdiProcessedTextConfiguration *)_processedTextConfigurationWithFontAttributes:(SCValdiFontAttributes *)fontAttributes
{
    UIColor *gradientColor = _textGradientHelper.gradientColor;
    if (gradientColor == nil && _customUnderlineStyle == nil) {
        return nil;
    }

    SCValdiProcessedTextConfiguration *configuration = [SCValdiProcessedTextConfiguration new];
    configuration.foregroundColorOverride = gradientColor;
    if (_customUnderlineStyle != nil) {
        configuration.customUnderlineStyle = _customUnderlineStyle;
        configuration.customUnderlineMode = SCValdiProcessedTextCustomUnderlineModeReplaceNativeUnderlineWithColorAttribute;
        configuration.customUnderlineColorAttributeName = kSCValdiTextViewCustomUnderlineColorAttribute;
        configuration.customUnderlineFallbackColor = gradientColor ?: fontAttributes.color ?: [UIColor blackColor];
    }
    return configuration;
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

            _processedText =
                [SCValdiProcessedText processedTextWithAttributeValue:_textValue
                                                           attributes:[fontAttributes resolveAttributesWithIsRightToLeft:isRightToLeft
                                                                                                          traitCollection:traitCollection]
                                                        isRightToLeft:isRightToLeft
                                                          fontManager:_fontManager
                                                      traitCollection:traitCollection
                                                        configuration:[self _processedTextConfigurationWithFontAttributes:fontAttributes]];

            BOOL didClamp = NO;
            [_processedText clampToCharacterLimit:[_characterLimit integerValue]
                                   ignoreNewlines:_ignoreNewlines
                                        didChange:&didClamp];
            if (didClamp) {
                changed = YES;
            }
            NSAttributedString *displayAttributedString = _processedText.attributedString;
            _hasCustomUnderlineAttribute = _processedText.hasCustomUnderline;

            BOOL hasOnLayout = _processedText.hasOnLayout;
            BOOL hasOnTap = _processedText.hasOnTap;
            BOOL needsEffectsLayoutManager =
                _processedText.hasAnimationTransform || _processedText.hasOuterOutline;
            if (_hasCustomUnderlineAttribute) {
                needsEffectsLayoutManager = YES;
            }

            _effectsLayoutManager.processedText = _processedText;
            _animatedTextEffectsLayoutManager.processedText = _processedText;
            _updateOnLayout = hasOnLayout;
            if (needsEffectsLayoutManager) {
                [self _updateEffectsLayoutManager];
            }

            BOOL useAnimatedTextOverlay = _processedText.hasAnimationTransform;
            _textAnimationPartCount = useAnimatedTextOverlay ? _processedText.animationTransformsCount : 0;
            [self _updateTextAnimationGroupRegistration];
            [self _updateTextAnimationGroupContext];
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

            if (hasOnTap) {
                _attributedTextOnTapString = displayAttributedString;
                [self _addAttributedTextOnTapGestureRecognizer];
            } else {
                _attributedTextOnTapString = nil;
                [self _removeAttributedTextOnTapGestureRecognizer];
            }

            _placeholder.hidden = _textView.attributedText.length > 0;
        } else {
            [self updateLabelMode:SCValdiTextModeText];;
            _processedText = nil;
            _effectsLayoutManager.processedText = nil;
            _animatedTextEffectsLayoutManager.processedText = nil;
            _textAnimationPartCount = 0;
            [self _updateTextAnimationGroupRegistration];
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

        // Skipped while the placeholder is still deferred; -_ensurePlaceholder applies the current
        // font attributes when it later builds the view.
        if (_placeholder != nil) {
            SCValdiSetTextHolderAttributes(_placeholder, fontAttributes, traitCollection, isRightToLeft, nil);
        }
        [self invalidateLayout];
    }
    _updating = NO;

    return changed;
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
    SCValdiTextAnimationGroup *group = [self _nearestTextAnimationGroup];
    if (group == _textAnimationGroup) {
        if (group != nil) {
            [self valdi_applyTextAnimationCoordinator:group.textAnimationCoordinator basePartIndex:0];
            [group setNeedsLayout];
        }
        return;
    }

    [_textAnimationGroup unregisterTextAnimationParticipant:self];
    _textAnimationGroup = group;
    if (group != nil) {
        [group registerTextAnimationParticipant:self];
    } else {
        [self valdi_applyTextAnimationCoordinator:nil basePartIndex:0];
    }
}

- (void)_updateTextAnimationGroupContext
{
    if (_textAnimationGroup != nil) {
        [self valdi_applyTextAnimationCoordinator:_textAnimationGroup.textAnimationCoordinator basePartIndex:0];
        [_textAnimationGroup setNeedsLayout];
    } else {
        [self valdi_applyTextAnimationCoordinator:nil basePartIndex:0];
    }
}

- (NSUInteger)valdi_textAnimationPartCount
{
    return _textAnimationPartCount;
}

- (void)valdi_applyTextAnimationCoordinator:(SCValdiTextAnimationCoordinator *)coordinator
                              basePartIndex:(NSUInteger)basePartIndex
{
    // Stash so a lazily-created overlay picks these up in -_ensureAnimatedTextView; the direct
    // assignments below are no-ops until the overlay exists.
    _textAnimationCoordinator = coordinator;
    _textAnimationBasePartIndex = basePartIndex;
    _animatedTextEffectsLayoutManager.textAnimationCoordinator = coordinator;
    _animatedTextEffectsLayoutManager.textAnimationBasePartIndex = basePartIndex;
    if (coordinator != nil) {
        [self _stopAnimatedTextDisplayLink];
    }
}

- (void)valdi_clearTextAnimationGroupRegistration
{
    _textAnimationGroup = nil;
    [self valdi_applyTextAnimationCoordinator:nil basePartIndex:0];
}

- (void)valdi_prepareGroupedTextAnimationFrame
{
    [_animatedTextEffectsLayoutManager prepareGroupedAnimatedTextProgress];
}

- (BOOL)valdi_invalidateGroupedTextAnimationFrame
{
    BOOL hasActiveAnimationRanges = [_animatedTextEffectsLayoutManager invalidateAnimatedTextProgress];
    [_animatedTextView setNeedsDisplay];
    [self _updateInlineTextChildAnimations];
    return hasActiveAnimationRanges;
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

// Attribute setters mark the text dirty and defer the rebuild to the next layout pass
// (layoutSubviews/sizeThatFits both flush it) so a render pass that applies several
// attributes pays for one SCValdiProcessedText build instead of one per setter. The
// full rebuild is expensive for animated/styled captions and runs on the main thread;
// it dominated PERF_STUCK_DETECTED hangs on the caption editor.
- (void)valdi_setFontAttributes:(SCValdiFontAttributes *)fontAttributes
{
    _fontAttributes = fontAttributes;
    _needAttributedTextUpdate = YES;
    [self _applyNumberOfLinesAttributes];
    [self setNeedsLayout];
}

- (void)valdi_setCustomUnderlineStyle:(SCValdiCustomUnderlineStyle *)customUnderlineStyle
{
    _customUnderlineStyle = customUnderlineStyle;
    _needAttributedTextUpdate = YES;
    if (_effectsLayoutManager) {
        [self _updateEffectsLayoutManager];
    }
    [self setNeedsLayout];
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
    // Rebinding an identical plain string is a no-op; skip the synchronous rebuild.
    // Restricted to the case where the displayed text already matches (and no marked
    // IME text is pending), because JS can re-set the previous value to reject an edit
    // and the rebuild is what clobbers the rejected characters back out of the view.
    if ([textValue isKindOfClass:[NSString class]] && _textView.markedTextRange == nil &&
        [ObjectAs(_textValue, NSString) isEqualToString:textValue] &&
        [_textView.text isEqualToString:textValue]) {
        return;
    }

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
    [self setNeedsLayout];
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

- (void)_updateTextViewInteractionMode
{
    _textView.editable = _enabled;
    _textView.selectable = _selectable;
    _textView.scrollEnabled = _enabled && !_hasTextOverflow;
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
    _enabled = enabled;
    [self _updateTextViewInteractionMode];
    _needAttributedTextUpdate = YES;
    [self setNeedsLayout];
    return YES;
}

- (BOOL)valdi_setSelectable:(BOOL)selectable
{
    _selectable = selectable;
    [self _updateTextViewInteractionMode];
    return YES;
}

- (BOOL)valdi_setFocused:(BOOL)focused
{
    // UIKit refuses becomeFirstResponder while the view is not attached to a window — which is
    // when the 'focused' attribute lands during an initial render pass — and the attribute system
    // never retries a failed application, permanently leaving the field unfocused (COMPOSER-6146).
    // Stash the request and apply it in didMoveToWindow instead.
    if (self.window == nil) {
        _pendingFocused = focused;
        return YES;
    }
    _pendingFocused = NO;
    // Re-entering UIKit's first responder machinery for a transition that already happened can
    // stall the main thread: it enqueues more work on UIKeyboardTaskQueue while the main thread may
    // already be blocked draining that same queue.
    if (focused == _textView.isFirstResponder) {
        return YES;
    }
    if (focused) {
        // UIKit can refuse in-window transitions (resign-active churn from notification banners),
        // and on a non-key window becomeFirstResponder can report success without ever showing
        // the keyboard. Keep the intent pending and apply it when the app becomes active or this
        // view's window becomes key, instead of failing permanently.
        if (!self.window.isKeyWindow || ![_textView becomeFirstResponder]) {
            _pendingFocused = YES;
        }
        return YES;
    }
    return [_textView resignFirstResponder];
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
    // Nothing to show and no placeholder view yet: stay deferred.
    if (_placeholder == nil && placeholder.length == 0) {
        return YES;
    }
    [self _ensurePlaceholder].text = placeholder;
    return YES;
}

- (BOOL)valdi_setPlaceholderColor:(nullable UIColor *)color
{
    // A color can arrive before the placeholder string; stash it until the view is built.
    if (_placeholder == nil) {
        _pendingPlaceholderColor = color;
        return YES;
    }
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

- (void)valdi_setOnTextSelectionMenu:(id<SCValdiFunction>)onTextSelectionMenu
{
    _onTextSelectionMenu = onTextSelectionMenu;
    [self _updateTextViewInteractionMode];
}

- (void)valdi_setOnTextSelectionMenuAction:(id<SCValdiFunction>)onTextSelectionMenuAction
{
    _onTextSelectionMenuAction = onTextSelectionMenuAction;
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
    // Malformed options leave every text holder's layer untouched, so keep any previously stashed
    // value: it is what _textView is still rendering, and what a subview built later must replay.
    if (!SCValdiSetTextHolderTextShadow(_textView, textShadow)) {
        return NO;
    }
    // Stash so a placeholder / animation overlay built later replays the same shadow.
    _textShadow = textShadow;
    if (_placeholder != nil) {
        SCValdiSetTextHolderTextShadow(_placeholder, textShadow);
    }
    if (_animatedTextView != nil) {
        SCValdiSetTextHolderTextShadow(_animatedTextView, textShadow);
    }
    return YES;
}

- (void) valdi_resetTextShadow
{
    _textShadow = nil;
    if (_placeholder != nil) {
        SCValdiResetTextHolderTextShadow(_placeholder);
    }
    if (_animatedTextView != nil) {
        SCValdiResetTextHolderTextShadow(_animatedTextView);
    }
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
          backgroundEffectPadding:(CGFloat)backgroundEffectPadding
                  traitCollection:(UITraitCollection *)traitCollection
{
    const CGFloat horizontalPadding = MAX(backgroundEffectPadding, 0.0) * 2.0;
    const CGFloat verticalPadding = MAX(backgroundEffectPadding, 0.0);
    CGSize availableSize = maxSize;
    if (backgroundEffectPadding > 0.0) {
        availableSize.width = MAX(availableSize.width - horizontalPadding, 0.0);
        availableSize.height = MAX(availableSize.height - verticalPadding, 0.0);
    }

    CGSize textSize = [SCValdiTextLayout measureSizeWithMaxSize:availableSize
                                                 fontAttributes:fontAttributes
                                                    fontManager:fontManager
                                                           text:text
                                                traitCollection:traitCollection];
    CGSize placeholderSize = [SCValdiTextLayout measureSizeWithMaxSize:availableSize
                                                         fontAttributes:fontAttributes
                                                            fontManager:fontManager
                                                                   text:placeholder
                                                        traitCollection:traitCollection];
    return CGSizeMake(MAX(textSize.width, placeholderSize.width) + horizontalPadding,
                      MAX(textSize.height, placeholderSize.height) + verticalPadding);
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
    CGFloat backgroundEffectPadding = [attributes doubleValueForAttributeName:@"backgroundEffectPadding"];

    return [SCValdiTextView measureSizeWithMaxSize:maxSize
                                    fontAttributes:fontAttributes
                                       fontManager:fontManager
                                              text:text
                                       placeholder:placeholder
                           backgroundEffectPadding:backgroundEffectPadding
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

    [attributesBinder bindAttribute:@"selectable"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiTextView *textView, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [textView valdi_setSelectable:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView, id<SCValdiAnimatorProtocol> animator) {
            [textView valdi_setSelectable:YES];
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

    [attributesBinder setMeasureDelegate:^CGSize(id<SCValdiViewLayoutAttributes> attributes,
                                                 CGSize maxSize,
                                                 UITraitCollection *traitCollection) {
        return [SCValdiTextView valdi_onMeasureWithAttributes:attributes
                                                      maxSize:maxSize
                                                  fontManager:fontManager
                                              traitCollection:traitCollection];
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

    [attributesBinder bindAttribute:@"onTextSelectionMenu"
        withFunctionBlock:^(SCValdiTextView *textView, id<SCValdiFunction> attributeValue) {
            [textView valdi_setOnTextSelectionMenu:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView) {
            [textView valdi_setOnTextSelectionMenu:nil];
        }];

    [attributesBinder bindAttribute:@"onTextSelectionMenuAction"
        withFunctionBlock:^(SCValdiTextView *textView, id<SCValdiFunction> attributeValue) {
            [textView valdi_setOnTextSelectionMenuAction:attributeValue];
        }
        resetBlock:^(SCValdiTextView *textView) {
            [textView valdi_setOnTextSelectionMenuAction:nil];
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
        invalidateLayoutOnChange:YES
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

    if ([self _needAttributedString]) {
        // A self-sized text view otherwise keeps its stale width until the JS round-trip lands, 
        // re-wrapping the just-typed text for a few frames
        [self invalidateLayout];
    }
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

- (NSArray<UIMenuElement *> *)_customEditMenuActionsForTextRange:(NSRange)range
{
    NSDictionary<NSString *, id> *event = SCValdiTextSelectionMenuEventForText(_textView.text, range);
    NSArray<NSDictionary<NSString *, NSString *> *> *menuActions =
        SCValdiTextSelectionMenuActionsForProvider(_onTextSelectionMenu, event);

    NSMutableArray<UIMenuElement *> *customActions = [NSMutableArray arrayWithCapacity:menuActions.count];
    for (NSDictionary<NSString *, NSString *> *menuAction in menuActions) {
        NSString *actionID = menuAction[SCValdiTextSelectionMenuActionIDKey];
        NSString *title = menuAction[SCValdiTextSelectionMenuActionTitleKey];
        __weak typeof(self) weakSelf = self;
        UIAction *action = [UIAction actionWithTitle:title image:nil identifier:nil handler:^(__kindof UIAction *uiAction) {
            [weakSelf _performTextSelectionMenuActionWithID:actionID range:range];
        }];
        [customActions addObject:action];
    }
    return customActions;
}

- (void)_performTextSelectionMenuActionWithID:(NSString *)actionID range:(NSRange)range
{
    NSDictionary<NSString *, id> *event = SCValdiTextSelectionMenuEventForText(_textView.text, range);
    SCValdiPerformTextSelectionMenuAction(_onTextSelectionMenuAction, actionID, event);
}

- (nullable UIMenu *)_editMenuForTextRange:(NSRange)range suggestedActions:(NSArray<UIMenuElement *> *)suggestedActions
{
    NSArray<UIMenuElement *> *customActions = [self _customEditMenuActionsForTextRange:range];
    if (customActions.count == 0) {
        return nil;
    }

    NSMutableArray<UIMenuElement *> *children = [NSMutableArray arrayWithCapacity:customActions.count + suggestedActions.count];
    [children addObjectsFromArray:customActions];
    [children addObjectsFromArray:suggestedActions];
    return [UIMenu menuWithTitle:@"" children:children];
}

#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 160000
- (nullable UIMenu *)textView:(UITextView *)textView
      editMenuForTextInRanges:(NSArray<NSValue *> *)ranges
             suggestedActions:(NSArray<UIMenuElement *> *)suggestedActions
{
    if (ranges.count == 0) {
        return nil;
    }

    NSRange selectedRange = ranges.firstObject.rangeValue;
    for (NSValue *rangeValue in ranges) {
        selectedRange = NSUnionRange(selectedRange, rangeValue.rangeValue);
    }
    return [self _editMenuForTextRange:selectedRange suggestedActions:suggestedActions];
}
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (nullable UIMenu *)textView:(UITextView *)textView
      editMenuForTextInRange:(NSRange)range
            suggestedActions:(NSArray<UIMenuElement *> *)suggestedActions
{
    return [self _editMenuForTextRange:range suggestedActions:suggestedActions];
}
#pragma clang diagnostic pop

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
