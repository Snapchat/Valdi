//
//  SCValdiScrollView.m
//  Valdi
//
//  Created by Simon Corsin on 5/12/18.
//

#import "valdi_core/SCValdiScrollView.h"
#import "valdi_core/SCValdiScrollViewInner.h"

#import "valdi_core/SCValdiNoAnimationDelegate.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiScrollViewDelegate.h"
#import "valdi_core/SCValdiIScrollPerfLoggerBridge.h"
#import "valdi_core/SCValdiAttributesBinderBase.h"
#import "valdi_core/UIView+ValdiObjects.h"
#import "valdi_core/UIView+ValdiBase.h"

typedef NS_ENUM(NSInteger, SCValdiScrollViewKeyboardDismissMode) {
    // Default behavior, keyboard dismisses as soon as scrolling starts
    SCValdiScrollViewKeyboardDismissModeImmediate,
    // Keyboard dismisses when touches go beyound the max Y boundary of the scroll view
    SCValdiScrollViewKeyboardDismissModeTouchExitBelow,
    // Keyboard dismisses when touches go beyound the min Y boundary of the scroll view
    SCValdiScrollViewKeyboardDismissModeTouchExitAbove,
};


static NSString* const kSCValdiScrollViewContentOffsetKey = @"contentOffset";

static CGFloat const kSCValdiKeyboardTranslationPadding = 10.0;

@interface SCValdiScrollView () {
    SCValdiScrollViewDelegate *_valdiScrollViewDelegate;
    CGFloat _keyboardOffsetY;

    BOOL _horizontalScroll;
    BOOL _translatesForKeyboard;
    BOOL _wantsVerticalScrollIndicator;
    BOOL _wantsHorizontalScrollIndicator;
    
    BOOL _dismissOnDrag;
    SCValdiScrollViewKeyboardDismissMode _dismissMode;
    
    SCValdiScrollViewInner *_scrollView;

    CGSize _rawContentSize;

    CGFloat _fadingEdgeLength;
    CAGradientLayer *_fadingEdgeGradient;
    NSMutableArray<NSNumber *> *_fadingEdgeLocations;
}
@end

@implementation SCValdiScrollView

#pragma mark - UIView methods

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    if (self) {
        _scrollView = [[SCValdiScrollViewInner alloc] initWithFrame:CGRectZero];
        [self addSubview:_scrollView];
        _horizontalScroll = NO;
        _rawContentSize = CGSizeZero;
        _dismissOnDrag = NO;
        _dismissMode = SCValdiScrollViewKeyboardDismissModeImmediate;
        if (@available(iOS 11, *)) {
            _scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
        }
        self.clipsToBounds = YES;

        [_scrollView addObserver:self
               forKeyPath:kSCValdiScrollViewContentOffsetKey
                  options:NSKeyValueObservingOptionNew
                  context:@selector(contentOffset)];
        

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_keyboardWillShow:)
                                                     name:UIKeyboardWillShowNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_keyboardWillHide:)
                                                     name:UIKeyboardWillHideNotification
                                                   object:nil];
    }

    return self;
}

- (void)dealloc
{
    [_scrollView removeObserver:self forKeyPath:kSCValdiScrollViewContentOffsetKey];
    _scrollView.delegate = nil;
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    [self _layoutScrollView];
    [self _updateFadingEdge];

    [self _layoutContentSize];
}

- (void)_layoutScrollView
{
    if (_scrollView.valdiViewNode != nil) {
        BOOL isHorizontal = _scrollView.valdiViewNode.isLayoutDirectionHorizontal;
        _horizontalScroll = isHorizontal;
        _scrollView.horizontalScroll = isHorizontal;
    }

    CGRect scrollViewFrame = self.bounds;
    scrollViewFrame.origin.y += _keyboardOffsetY;

    if (CGAffineTransformIsIdentity(_scrollView.transform)) {
        // We should technically be able to use the position/bounds logic
        // and not even set the frame. I'm keeping this for now to minimize
        // risks of regressions.
        _scrollView.frame = scrollViewFrame;
    } else {
        CGFloat scrollViewCenterX = CGRectGetMidX(scrollViewFrame);
        CGFloat scrollViewCenterY = CGRectGetMidY(scrollViewFrame);

        CGRect newBounds = _scrollView.bounds;
        newBounds.size.width = scrollViewFrame.size.width;
        newBounds.size.height = scrollViewFrame.size.height;

        _scrollView.center = CGPointMake(scrollViewCenterX, scrollViewCenterY);
        _scrollView.bounds = newBounds;
        
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context
{
    if (context == @selector(contentOffset)) {
        [self _updateFadingEdge];
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

#pragma mark - ContentOffset+ContentSize

- (void)scrollSpecsDidChangeWithContentOffset:(CGPoint)contentOffset contentSize:(CGSize)contentSize animated:(BOOL)animated
{
    if (!CGSizeEqualToSize(_rawContentSize, contentSize)) {
        [self setContentSize:contentSize];
    }
    if (!CGPointEqualToPoint(_scrollView.contentOffset, contentOffset)) {
        [self setContentOffset:contentOffset animated:animated];
    }

    if (_scrollView.valdiViewNode != nil) {
        BOOL isHorizontal = _scrollView.valdiViewNode.isLayoutDirectionHorizontal;

        if (_horizontalScroll != isHorizontal) {
            [self setNeedsLayout];
        }
    }
}

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated
{
    [_scrollView setContentOffset:contentOffset animated:animated];
}

- (void)setContentSize:(CGSize)contentSize
{
    _rawContentSize = contentSize;
    [self _layoutContentSize];
}

- (void)_layoutContentSize
{
    CGFloat width = _rawContentSize.width;
    CGFloat height = _rawContentSize.height;
    if (_horizontalScroll) {
        height = _scrollView.bounds.size.height;
    } else {
        width = _scrollView.bounds.size.width;
    }
    CGSize visibleContentSize = CGSizeMake(width, height);
    if (!CGSizeEqualToSize(_scrollView.contentSize, visibleContentSize)) {
        _scrollView.contentSize = visibleContentSize;
    }
}

#pragma mark - UIView+Valdi

- (void)setClipsToBounds:(BOOL)clipsToBounds
{
    [super setClipsToBounds:clipsToBounds];
    _scrollView.clipsToBounds = clipsToBounds;
}

- (BOOL)clipsToBoundsByDefault
{
    return YES;
}

- (void)setValdiContext:(id<SCValdiContextProtocol>)valdiContext
{
    [super setValdiContext:valdiContext];

    _scrollView.valdiContext = valdiContext;
}

- (void)setValdiViewNode:(id<SCValdiViewNodeProtocol>)valdiViewNode
{
    [super setValdiViewNode:valdiViewNode];

    _scrollView.valdiViewNode = valdiViewNode;
}

#pragma mark - SCValdiContentViewProviding

- (UIView *)contentViewForInsertingValdiChildren
{
    return _scrollView;
}

#pragma mark - SCValdiViewComponent methods

- (void)didMoveToValdiContext:(id<SCValdiContextProtocol>)valdiContext
                        viewNode:(id<SCValdiViewNodeProtocol>)viewNode
{
    BOOL isHorizontal = viewNode.isLayoutDirectionHorizontal;
    _horizontalScroll = isHorizontal;
    _scrollView.horizontalScroll = isHorizontal;

    [self _layoutContentSize];

    // Ensure we create and set the scroll view delegate
    [self _scrollViewDelegate];
}

- (BOOL)willEnqueueIntoValdiPool
{
    // Make sure we don't send callbacks
    _scrollView.delegate = nil;

    [self setContentOffset:CGPointZero animated:NO];
    [self setContentSize:CGSizeZero];

    return YES;
}

#pragma mark - Public methods

- (UIScrollView *)innerScrollView
{
    return _scrollView;
}

#pragma mark - Attribute binding helper methods

- (BOOL)valdi_setBounces:(BOOL)attributeValue
{
    _scrollView.bounces = attributeValue;
    return YES;
}

- (BOOL)valdi_setBouncesFromDragAtStart:(BOOL)attributeValue
{
    _scrollView.bouncesFromDragAtStart = attributeValue;
    return YES;
}

- (BOOL)valdi_setBouncesFromDragAtEnd:(BOOL)attributeValue
{
    _scrollView.bouncesFromDragAtEnd = attributeValue;
    return YES;
}

- (BOOL)valdi_setBouncesVerticalWithSmallContent:(BOOL)attributeValue
{
    _scrollView.alwaysBounceVertical = attributeValue;
    return YES;
}

- (BOOL)valdi_setBouncesHorizontalWithSmallContent:(BOOL)attributeValue
{
    _scrollView.alwaysBounceHorizontal = attributeValue;
    return YES;
}


- (void)_updateKeyboardMode
{
    // Proactively remove the panGestureRecognizer handler, it's only needed when
    // dismiss is on _and_ the mode is DismissModeTouchExitBelow
    [_scrollView.panGestureRecognizer removeTarget:self action:@selector(handleScrollPan:)];

    // turn off dismiss mode completely
    if (!_dismissOnDrag) {
        _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeNone;
        return;
    }
    
    // Dismisses as soon as scrolling happens, use built in feature
    if (_dismissMode == SCValdiScrollViewKeyboardDismissModeImmediate) {
        _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
        return;
    }
    
    // Only dismisses when the touches exit the bounds of the scroll area, set up the
    // pan gesture target
    [_scrollView.panGestureRecognizer addTarget:self action:@selector(handleScrollPan:)];
    _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeNone;
}

- (BOOL)valdi_setDismissKeyboardOnDrag:(BOOL)attributeValue
{
    _dismissOnDrag = attributeValue;
    [self _updateKeyboardMode];

    return YES;
}

- (NSString *)valdi_setDismissKeyboardOnDragMode:(NSString *)attributeValue
{
    if ([attributeValue isEqualToString:@"touch-exit-below"]) {
        _dismissMode = SCValdiScrollViewKeyboardDismissModeTouchExitBelow;
    } else if ([attributeValue isEqualToString:@"touch-exit-above"]) {
        _dismissMode = SCValdiScrollViewKeyboardDismissModeTouchExitAbove;
    } else {
        _dismissMode = SCValdiScrollViewKeyboardDismissModeImmediate;
    }
    [self _updateKeyboardMode];
    return attributeValue;
}

- (BOOL)valdi_setTranslatesForKeyboard:(BOOL)attributeValue
{
    _translatesForKeyboard = attributeValue;

    if (_keyboardOffsetY != 0) {
        _keyboardOffsetY = 0;
        [self _layoutScrollView];
    }

    return YES;
}

- (BOOL)valdi_setPagingEnabled:(BOOL)attributeValue
{
    _scrollView.pagingEnabled = attributeValue;
    return YES;
}

- (BOOL)valdi_setShowsHorizontalScrollIndicator:(BOOL)attributeValue
{
    _scrollView.showsHorizontalScrollIndicator = attributeValue;
    return YES;
}

- (BOOL)valdi_setShowsVerticalScrollIndicator:(BOOL)attributeValue
{
    _scrollView.showsVerticalScrollIndicator = attributeValue;
    return YES;
}

- (BOOL)valdi_setCancelsTouchesOnScroll:(BOOL)attributeValue
{
    [self _scrollViewDelegate].cancelsTouchesOnScroll = attributeValue;
    return YES;
}

- (void)valdi_setScrollEnabled:(BOOL)attributeValue
{
    _scrollView.panGestureRecognizerEnabled = attributeValue;
}

- (void)valdi_setScrollPerfLoggerBridge:(id)attributeValue
{
    if (!attributeValue) {
        _valdiScrollViewDelegate.scrollPerfLoggerBridge = nil;
        return;
    }
    id<SCValdiIScrollPerfLoggerBridge> bridge = ProtocolAs(attributeValue, SCValdiIScrollPerfLoggerBridge);
    if (bridge) {
        _valdiScrollViewDelegate.scrollPerfLoggerBridge = bridge;
    } else {
        SCValdiErrorThrow(@"scrollPerfLoggerBridge needs to conform to SCValdiIScrollPerfLoggerBridge");
    }
}

- (BOOL)valdi_setDecelerationRate:(NSString *)decelerationRate
{
    if (!decelerationRate || [decelerationRate isEqualToString:@"normal"]) {
        _scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;
        return YES;
    } else if ([decelerationRate isEqualToString:@"fast"]) {
        _scrollView.decelerationRate = UIScrollViewDecelerationRateFast;
        return YES;
    } else {
        return NO;
    }
}

# pragma mark Fading Edge Length methods

- (void)valdi_setFadingEdgeLength:(CGFloat)fadingEdgeLength
{
    _fadingEdgeLength = fadingEdgeLength;

    if (_fadingEdgeLength <= 0) {
        _scrollView.layer.mask = nil;
        _fadingEdgeGradient = nil;
        _fadingEdgeLocations = nil;
        return;
    }

    if (!_fadingEdgeLocations) {
        _fadingEdgeLocations = [NSMutableArray arrayWithObjects:@(0), @(0), @(1), @(1), nil];
    }

    if (!_fadingEdgeGradient) {
        _fadingEdgeGradient = [[CAGradientLayer alloc] init];
        _fadingEdgeGradient.delegate = [SCValdiNoAnimationDelegate sharedInstance];
        _fadingEdgeGradient.colors = @[
            (id)UIColor.clearColor.CGColor,
            (id)UIColor.whiteColor.CGColor,
            (id)UIColor.whiteColor.CGColor,
            (id)UIColor.clearColor.CGColor
        ];
        _fadingEdgeGradient.locations = _fadingEdgeLocations;
        _scrollView.layer.mask = _fadingEdgeGradient;
    }

    [self _updateFadingEdgeDirection];
}

- (void)_updateFadingEdgeDirection
{
    [self _updateFadingEdgeDirectionAndInvalidateLayout:YES];
}

- (void)_updateFadingEdgeDirectionAndInvalidateLayout:(BOOL)invalidateLayout
{
    if (_fadingEdgeGradient == nil) {
        return;
    }

    if (_horizontalScroll) {
        _fadingEdgeGradient.startPoint = CGPointMake(0.0, 0.5);
        _fadingEdgeGradient.endPoint = CGPointMake(1.0, 0.5);
    } else {
        _fadingEdgeGradient.startPoint = CGPointMake(0.5, 0.0);
        _fadingEdgeGradient.endPoint = CGPointMake(0.5, 1.0);
    }

    if (invalidateLayout) {
        [self setNeedsLayout];
    }
}

- (void)_updateFadingEdge
{
    if (_fadingEdgeLocations) {
        CGSize boundsSize = self.bounds.size;
        CGSize contentSize = _scrollView.contentSize;
        CGPoint offset = _scrollView.contentOffset;
        
        if (_horizontalScroll) {
            CGFloat maxOffset = MIN(_fadingEdgeLength, contentSize.width - boundsSize.width);
            CGFloat startFadeStrength = [self _fadeStrengthForOffset:offset.x maxOffset:maxOffset];
            CGFloat endFadeStrength = [self _fadeStrengthForOffset:contentSize.width - boundsSize.width - offset.x maxOffset:maxOffset];
            
            CGFloat edgeFadeRatio = _fadingEdgeLength / boundsSize.width;
            [_fadingEdgeLocations replaceObjectAtIndex:1 withObject:@(edgeFadeRatio * startFadeStrength)];
            [_fadingEdgeLocations replaceObjectAtIndex:2 withObject:@(1 - edgeFadeRatio * endFadeStrength)];
        } else {
            CGFloat maxOffset = MIN(_fadingEdgeLength, contentSize.height - boundsSize.height);
            CGFloat startFadeStrength = [self _fadeStrengthForOffset:offset.y maxOffset:maxOffset];
            CGFloat endFadeStrength = [self _fadeStrengthForOffset:contentSize.height - boundsSize.height - offset.y maxOffset:maxOffset];
            
            CGFloat edgeFadeRatio = _fadingEdgeLength / boundsSize.height;
            [_fadingEdgeLocations replaceObjectAtIndex:1 withObject:@(edgeFadeRatio * startFadeStrength)];
            [_fadingEdgeLocations replaceObjectAtIndex:2 withObject:@(1 - edgeFadeRatio * endFadeStrength)];
        }
        
        [self _updateFadingEdgeDirectionAndInvalidateLayout:NO];
        _fadingEdgeGradient.locations = _fadingEdgeLocations;
        // Move the mask to the content offset location.
        CGRect fadingEdgeRect = self.bounds;
        fadingEdgeRect.origin = _scrollView.contentOffset;
        _fadingEdgeGradient.frame = fadingEdgeRect;
    }
}

- (CGFloat)_fadeStrengthForOffset:(CGFloat)offset maxOffset:(CGFloat)maxOffset {
    if (maxOffset <= 0) {
        return 0;
    }
    
    return [self _easeInOut: MAX(0, MIN(1, offset / maxOffset))];
}

- (CGFloat)_easeInOut:(CGFloat)t
{
    // Take a number between 0 and 1 and return a number between 0 and 1
    return (t < 0.5) ? (2.0 * t * t) : (-1.0 + (4.0 - 2.0 * t) * t);
}

#pragma mark - Static methods

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    [attributesBinder bindScrollAttributes];

    [attributesBinder bindAttribute:@"bouncesVerticalWithSmallContent"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBouncesVerticalWithSmallContent:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBouncesVerticalWithSmallContent:NO];
        }];

    [attributesBinder bindAttribute:@"bouncesHorizontalWithSmallContent"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBouncesHorizontalWithSmallContent:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBouncesHorizontalWithSmallContent:NO];
        }];

    [attributesBinder bindAttribute:@"bounces"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBounces:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBounces:YES];
        }];

    [attributesBinder bindAttribute:@"dismissKeyboardOnDrag"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setDismissKeyboardOnDrag:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setDismissKeyboardOnDrag:NO];
        }];
    
    [attributesBinder bindAttribute:@"dismissKeyboardOnDragMode"
           invalidateLayoutOnChange:NO
           withStringBlock:^BOOL(__kindof UIView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
              return [view valdi_setDismissKeyboardOnDragMode:attributeValue];
           }
           resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
               [view valdi_setDismissKeyboardOnDragMode:@"immediate"];
           }];

    [attributesBinder bindAttribute:@"translatesForKeyboard" // TODO(664) deprecate
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setTranslatesForKeyboard:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setTranslatesForKeyboard:NO];
        }];

    [attributesBinder bindAttribute:@"pagingEnabled"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setPagingEnabled:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setPagingEnabled:NO];
        }];

    [attributesBinder bindAttribute:@"showsHorizontalScrollIndicator"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setShowsHorizontalScrollIndicator:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setShowsHorizontalScrollIndicator:YES];
        }];

    [attributesBinder bindAttribute:@"showsVerticalScrollIndicator"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setShowsVerticalScrollIndicator:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setShowsVerticalScrollIndicator:YES];
        }];

    [attributesBinder bindAttribute:@"cancelsTouchesOnScroll"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setCancelsTouchesOnScroll:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setCancelsTouchesOnScroll:YES];
        }];

    [attributesBinder bindAttribute:@"bouncesFromDragAtStart"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBouncesFromDragAtStart:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBouncesFromDragAtStart:YES];
        }];

    [attributesBinder bindAttribute:@"bouncesFromDragAtEnd"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBouncesFromDragAtEnd:attributeValue];
        }
        resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBouncesFromDragAtEnd:YES];
        }];

    [attributesBinder bindAttribute:@"scrollEnabled"
           invalidateLayoutOnChange:NO
                      withBoolBlock:^BOOL(SCValdiScrollView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
                          [view valdi_setScrollEnabled:attributeValue];
                          return YES;
                      }
                         resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
                             [view valdi_setScrollEnabled:YES];
                         }];
    [attributesBinder bindAttribute:@"scrollPerfLoggerBridge"
           invalidateLayoutOnChange:NO
                      withUntypedBlock:^BOOL(SCValdiScrollView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
                          [view valdi_setScrollPerfLoggerBridge:attributeValue];
                          return YES;
                      }
                         resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
                             [view valdi_setScrollPerfLoggerBridge:nil];
                         }];

    [attributesBinder bindAttribute:@"fadingEdgeLength"
           invalidateLayoutOnChange:NO
                      withDoubleBlock:^BOOL(SCValdiScrollView *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
                          [view valdi_setFadingEdgeLength:attributeValue];
                          return YES;
                      }
                         resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
                            [view valdi_setFadingEdgeLength:0];
                         }];
    [attributesBinder bindAttribute:@"decelerationRate"
           invalidateLayoutOnChange:NO
                      withStringBlock:^BOOL(SCValdiScrollView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
                          return [view valdi_setDecelerationRate:attributeValue];
                      }
                         resetBlock:^(SCValdiScrollView *view, id<SCValdiAnimatorProtocol> animator) {
                             [view valdi_setDecelerationRate:nil];
                         }];
}

#pragma mark - Private property methods

- (SCValdiScrollViewDelegate *)_scrollViewDelegate
{
    if (_valdiScrollViewDelegate == nil) {
        _valdiScrollViewDelegate = [[SCValdiScrollViewDelegate alloc] init];
    }
    if (_scrollView.delegate != _valdiScrollViewDelegate) {
        _scrollView.delegate = _valdiScrollViewDelegate;
    }
    return _valdiScrollViewDelegate;
}

#pragma mark - Internal methods

static UIView *_Nullable _SCFirstResponderInViewTree(UIView *view)
{
    for (UIView *subview in view.subviews) {
        if (subview.isFirstResponder) {
            return subview;
        }

        UIView *result = _SCFirstResponderInViewTree(subview);
        if (result) {
            return result;
        }
    }
    return nil;
}

- (void)_keyboardWillShow:(NSNotification *)notification
{
    if (!_translatesForKeyboard) {
        return;
    }

    // Find the input view, which should be the first responder in the scrollview's view hierarchy
    UIView *firstResponder = _SCFirstResponderInViewTree(_scrollView);
    if (!firstResponder) {
        return;
    }

    CGRect keyboardFrame = ObjectAs(notification.userInfo[UIKeyboardFrameEndUserInfoKey], NSValue).CGRectValue;
    NSTimeInterval duration =
        ObjectAs(notification.userInfo[UIKeyboardAnimationDurationUserInfoKey], NSNumber).doubleValue;
    UIViewAnimationCurve animationCurve =
        ObjectAs(notification.userInfo[UIKeyboardAnimationCurveUserInfoKey], NSNumber).unsignedIntegerValue;

    // Figure out how much of the input view will overlap with the keyboard
    CGRect firstResponderFrame = [firstResponder convertRect:firstResponder.bounds toView:nil];
    CGFloat overlap =
        MAX(0, CGRectGetMaxY(firstResponderFrame) - keyboardFrame.origin.y + kSCValdiKeyboardTranslationPadding);
    if (overlap == 0) {
        return;
    }
    _keyboardOffsetY = -overlap;

    [UIView animateWithDuration:duration
                          delay:0
                        options:(animationCurve << 16 | UIViewAnimationOptionBeginFromCurrentState)
                     animations:^{
                         [self _layoutScrollView];
                     }
                     completion:nil];
}

- (void)_keyboardWillHide:(NSNotification *)notification
{
    if (!_translatesForKeyboard) {
        return;
    }
    _keyboardOffsetY = 0;

    NSTimeInterval duration =
        ObjectAs(notification.userInfo[UIKeyboardAnimationDurationUserInfoKey], NSNumber).doubleValue;
    UIViewAnimationCurve animationCurve =
        ObjectAs(notification.userInfo[UIKeyboardAnimationCurveUserInfoKey], NSNumber).unsignedIntegerValue;
    [UIView animateWithDuration:duration
                          delay:0
                        options:(animationCurve << 16 | UIViewAnimationOptionBeginFromCurrentState)
                     animations:^{
                         [self _layoutScrollView];
                     }
                     completion:nil];
}

// Used to detect when the scroll drag gesture exits the bounds of the scroll view
- (void)handleScrollPan:(UIPanGestureRecognizer *)panGesture {

    if (!_dismissOnDrag) {
        return;
    }

    if (_dismissMode == SCValdiScrollViewKeyboardDismissModeImmediate) {
        return;
    }
    
    if (panGesture.state != UIGestureRecognizerStateChanged) {
        return;
    }
    
    for (NSUInteger i = 0; i < panGesture.numberOfTouches; i++) {
        CGPoint p = [panGesture locationOfTouch:i inView:_scrollView];
        switch(_dismissMode) {
            case SCValdiScrollViewKeyboardDismissModeTouchExitBelow:
                if (p.y > CGRectGetMaxY(_scrollView.bounds)) {
                    [UIApplication.sharedApplication sendAction:@selector(resignFirstResponder) to:nil from:nil forEvent:nil];
                    return;
                }
                break;
            case SCValdiScrollViewKeyboardDismissModeTouchExitAbove:
                if (p.y < CGRectGetMinY(_scrollView.bounds)) {
                    [UIApplication.sharedApplication sendAction:@selector(resignFirstResponder) to:nil from:nil forEvent:nil];
                    return;
                }
                break;
        }
    }
}

@end
