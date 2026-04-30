//
//  UIView+Valdi.m
//  Valdi
//
//  Created by Simon Corsin on 12/12/17.
//  Copyright © 2017 Snap Inc. All rights reserved.
//

#import "valdi/ios/Categories/UIView+Valdi.h"

#import "valdi_core/NSScanner+CGFloat.h"
#import "valdi/ios/SCValdiContext.h"
#import "valdi/ios/Gestures/SCValdiGestureRecognizers.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiNoAnimationDelegate.h"
#import "valdi_core/SCValdiViewComponent.h"
#import "valdi/ios/SCValdiRuntime.h"
#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi/ios/Utils/GradientUtils.h"
#import "valdi_core/UIColor+Valdi.h"
#import "valdi_core/SCValdiGeometricPath.h"

#import <objc/runtime.h>
#import <yoga/UIView+Yoga.h>

static NSString *const kBackgroundGradientLayoutKey = @"background_gradient";
static NSString *const kShadowPathLayoutKey = @"shadow_path";
static NSString *const kMaskLayerPath = @"mask_layer";

static CACornerMask kCornerMaskAll = kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;

static UIBezierPath *SCBezierPathWithRadii(CGFloat topLeftRadius, CGFloat topRightRadius, CGFloat bottomLeftRadius, CGFloat bottomRightRadius, CGSize size)
{
    CGFloat width = size.width;
    CGFloat height = size.height;
    CGFloat firstAngel = M_PI;
    CGFloat secondAngel = firstAngel + M_PI_2;
    CGFloat thirdAngel = secondAngel + M_PI_2;
    CGFloat forthAngel = thirdAngel + M_PI_2;
    UIBezierPath *path = [UIBezierPath bezierPath];
    [path moveToPoint:CGPointMake(0, topLeftRadius)];
    [path addArcWithCenter:CGPointMake(topLeftRadius, topLeftRadius)
                    radius:topLeftRadius
                startAngle:firstAngel
                  endAngle:secondAngel
                 clockwise:YES];
    [path addLineToPoint:CGPointMake(width - topRightRadius, 0)];
    [path addArcWithCenter:CGPointMake(width - topRightRadius, topRightRadius)
                    radius:topRightRadius
                startAngle:secondAngel
                  endAngle:thirdAngel
                 clockwise:YES];
    [path addLineToPoint:CGPointMake(width, height - bottomRightRadius)];
    [path addArcWithCenter:CGPointMake(width - bottomRightRadius, height - bottomRightRadius)
                    radius:bottomRightRadius
                startAngle:thirdAngel
                  endAngle:forthAngel
                 clockwise:YES];
    [path addLineToPoint:CGPointMake(bottomLeftRadius, height)];
    [path addArcWithCenter:CGPointMake(bottomLeftRadius, height - bottomLeftRadius)
                    radius:bottomLeftRadius
                startAngle:forthAngel
                  endAngle:firstAngel
                 clockwise:YES];
    [path addLineToPoint:CGPointMake(0, topLeftRadius)];

    [path closePath];
    return path;
}

@interface SCValdiMaskLayer: CAShapeLayer

@property (readonly, nonatomic) BOOL isEmpty;

@property (strong, nonatomic) id maskPath;
@property (assign, nonatomic) CGFloat maskOpacity;

@end

@implementation SCValdiMaskLayer {
    CAShapeLayer *_innerMaskLayer;
    CGFloat _topLeftCornerRadius;
    CGFloat _topRightCornerRadius;
    CGFloat _bottomRightCornerRadius;
    CGFloat _bottomLeftCornerRadius;
}

- (instancetype)init
{
    self = [super init];

    if (self) {
        _maskOpacity = 1.0f;
        self.fillRule = kCAFillRuleEvenOdd;
        self.delegate = [SCValdiNoAnimationDelegate sharedInstance];
    }

    return self;
}

- (void)layoutSublayers
{
    [super layoutSublayers];

    _innerMaskLayer.frame = self.bounds;

    [self _updatePath];
}

- (void)updateBounds:(CGRect)bounds
{
    self.bounds = bounds;
}

- (void)_updatePath
{
    CGRect bounds = self.bounds;
    CGMutablePathRef path = CGPathCreateMutable();

    if ([self hasCornerRadius]) {
        UIBezierPath *cornerPath = SCBezierPathWithRadii(_topLeftCornerRadius, _topRightCornerRadius, _bottomLeftCornerRadius, _bottomRightCornerRadius, bounds.size);
        CGPathAddPath(path, nil, cornerPath.CGPath);
    } else {
        CGPathAddRect(path, nil, CGRectMake(0, 0, bounds.size.width, bounds.size.height));
    }

    if (_maskPath) {
        _innerMaskLayer.frame = bounds;
        CGPathRef innerMaskPath = CGPathFromGeometricPathData(_maskPath, bounds.size);

        if (innerMaskPath) {
            CGPathAddPath(path, nil, innerMaskPath);
        }

        _innerMaskLayer.path = innerMaskPath;

        if (innerMaskPath) {
            CFRelease(innerMaskPath);
        }
    } else {
        _innerMaskLayer.mask = nil;
    }

    self.path = path;

    if (path) {
        CFRelease(path);
    }
}

- (BOOL)hasCornerRadius
{
    return  _topLeftCornerRadius != 0 || _topRightCornerRadius != 0 || _bottomRightCornerRadius != 0 || _bottomLeftCornerRadius != 0;
}

- (BOOL)isEmpty
{
    return ![self hasCornerRadius] && self.maskPath == nil && self.maskOpacity == 1.0f;
}

- (void)setTopLeftCornerRadius:(CGFloat)topLeftCornerRadius
          topRightCornerRadius:(CGFloat)topRightCornerRadius
       bottomRightCornerRadius:(CGFloat)bottomRightCornerRadius
        bottomLeftCornerRadius:(CGFloat)bottomLeftCornerRadius
{
    _topLeftCornerRadius = topLeftCornerRadius;
    _topRightCornerRadius = topRightCornerRadius;
    _bottomRightCornerRadius = bottomRightCornerRadius;
    _bottomLeftCornerRadius = bottomLeftCornerRadius;

    [self _updatePath];
}

- (void)setMaskPath:(id)maskPath
{
    _maskPath = maskPath;

    if (maskPath) {
        if (!_innerMaskLayer) {
            _innerMaskLayer = [CAShapeLayer new];
            _innerMaskLayer.delegate = [SCValdiNoAnimationDelegate sharedInstance];
            _innerMaskLayer.opacity = 1.0 - _maskOpacity;
            _innerMaskLayer.frame = self.bounds;
            [self addSublayer:_innerMaskLayer];
        }
        [self _updatePath];
    } else {
        [_innerMaskLayer removeFromSuperlayer];
        _innerMaskLayer = nil;
    }
}

- (void)setMaskOpacity:(CGFloat)maskOpacity
{
    _maskOpacity = maskOpacity;
    _innerMaskLayer.opacity = 1.0 - maskOpacity;
}

@end

@protocol SCValdiHitTestSlop<NSObject>

- (void)setHitTestSlop:(UIEdgeInsets)hitTestSlop;

@end

@interface UIView ()

/**
 Used to manage gradient backgrounds
 */
@property (strong, nonatomic) CAGradientLayer* backgroundGradientLayer;

@end

@implementation UIView (Valdi)

- (void)valdi_updateMaskLayerWithBlock:(void(^)(SCValdiMaskLayer *maskLayer))block
{
    SCValdiMaskLayer *maskLayer = ObjectAs(self.layer.mask, SCValdiMaskLayer);
    if (!maskLayer) {
        maskLayer = [SCValdiMaskLayer new];
        self.layer.mask = maskLayer;
        maskLayer.frame = self.layer.bounds;
    }

    block(maskLayer);

    id<SCValdiViewNodeProtocol> valdiViewNode = self.valdiViewNode;
    if (maskLayer.isEmpty) {
        self.layer.mask = nil;

        if ([valdiViewNode hasDidFinishLayoutBlockForKey:kMaskLayerPath]) {
            [valdiViewNode setDidFinishLayoutBlock:nil forKey:kMaskLayerPath];
        }
    } else {
        if (![valdiViewNode hasDidFinishLayoutBlockForKey:kMaskLayerPath]) {
            [valdiViewNode setDidFinishLayoutBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
                SCValdiMaskLayer *maskLayer = ObjectAs(view.layer.mask, SCValdiMaskLayer);
                maskLayer.frame = view.bounds;
            } forKey:kMaskLayerPath];
        }
    }
}

- (void)valdi_applyShadowPathWithAnimator:(id<SCValdiAnimatorProtocol> )animator
{
    CGPathRef shadowPath;
    CAShapeLayer *maskLayer = ObjectAs(self.layer.mask, CAShapeLayer);
    if (maskLayer) {
        shadowPath = maskLayer.path;
        SCLogValdiError(@"Combining boxShadow and borderRadius with multiple borders is currently not supported. "
                           @"The shadow might not be displayed at all.");
    } else {
        shadowPath =
            [[UIBezierPath bezierPathWithRoundedRect:self.layer.bounds cornerRadius:self.layer.cornerRadius] CGPath];
    }

    if (animator) {
        [animator addAnimationOnLayer:self.layer forKeyPath:@"shadowPath" value:(__bridge id)shadowPath];
    } else {
        self.layer.shadowPath = shadowPath;
    }
}

- (BOOL)valdi_setBoxShadow:(NSArray<id> *)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    if (!attributeValue || attributeValue.count < 3) {
        attributeValue = @[ @(NO), @0, @0, @0, @([UIColor clearColor].valdiAttributeValue) ];
        [self.valdiViewNode setDidFinishLayoutBlock:nil forKey:kShadowPathLayoutKey];
        self.layer.shadowPath = nil;
    }

    if ([[attributeValue objectAtIndex:0] boolValue]) {
        [self.valdiViewNode setDidFinishLayoutBlock:nil forKey:kShadowPathLayoutKey];
        self.layer.shadowPath = nil;
    } else {
        [self valdi_applyShadowPathWithAnimator:animator];
        [self.valdiViewNode setDidFinishLayoutBlock:^(UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_applyShadowPathWithAnimator:animator];
        }
                                                forKey:kShadowPathLayoutKey];
    }

    CGSize shadowOffset =
        CGSizeMake([[attributeValue objectAtIndex:1] doubleValue], [[attributeValue objectAtIndex:2] doubleValue]);
    if (animator) {
        [animator addAnimationOnLayer:self.layer forKeyPath:@"shadowOffset" value:@(shadowOffset)];
    } else {
        self.layer.shadowOffset = shadowOffset;
    }

    if (attributeValue.count > 3) {
        CGFloat shadowRadius = [[attributeValue objectAtIndex:3] doubleValue];
        if (animator) {
            [animator addAnimationOnLayer:self.layer forKeyPath:@"shadowRadius" value:@(shadowRadius)];
        } else {
            self.layer.shadowRadius = shadowRadius;
        }
    }
    if (attributeValue.count > 4) {
        UIColor *shadowColor =
            UIColorFromValdiAttributeValue((int64_t)[[attributeValue objectAtIndex:4] integerValue]);
        CGFloat red;
        CGFloat green;
        CGFloat blue;
        CGFloat alpha;
        [shadowColor getRed:&red green:&green blue:&blue alpha:&alpha];
        CGColorRef shadowCGColor = [UIColor colorWithRed:red green:green blue:blue alpha:1.0].CGColor;
        if (animator) {
            [animator addAnimationOnLayer:self.layer forKeyPath:@"shadowColor" value:(__bridge id)shadowCGColor];
            [animator addAnimationOnLayer:self.layer forKeyPath:@"shadowOpacity" value:@(alpha)];
        } else {
            self.layer.shadowColor = shadowCGColor;
            self.layer.shadowOpacity = alpha;
        }
    }

    return YES;
}

- (BOOL)valdi_setObjectFit:(NSString *)attributeValue
{
    if (!attributeValue) {
        self.contentMode = UIViewContentModeScaleToFill;
        return YES;
    }

    NSString *contentModeString = attributeValue;
    UIViewContentMode contentMode;
    if ([contentModeString isEqualToString:@"none"]) {
        contentMode = UIViewContentModeCenter;
    } else if ([contentModeString isEqualToString:@"fill"]) {
        contentMode = UIViewContentModeScaleToFill;
    } else if ([contentModeString isEqualToString:@"cover"]) {
        contentMode = UIViewContentModeScaleAspectFill;
    } else if ([contentModeString isEqualToString:@"contain"]) {
        contentMode = UIViewContentModeScaleAspectFit;
    } else {
        return NO;
    }

    self.contentMode = contentMode;

    return YES;
}

- (BOOL)valdi_setBorderColor:(UIColor *)borderColor animator:(id<SCValdiAnimatorProtocol> )animator
{
    borderColor = borderColor ?: [UIColor clearColor];
    if (animator) {
        [animator addAnimationOnLayer:self.layer forKeyPath:@"borderColor" value:(__bridge id)borderColor.CGColor];
    } else {
        self.layer.borderColor = borderColor.CGColor;
    }
    return YES;
}

- (BOOL)valdi_setBorder:(NSArray *)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    if (!attributeValue) {
        attributeValue = @[ @0, @([UIColor blackColor].valdiAttributeValue) ];
    }

    [self valdi_setBorderWidth:[[attributeValue objectAtIndex:0] doubleValue] animator:animator];

    if (attributeValue.count > 1) {
        UIColor *color = UIColorFromValdiAttributeValue((int64_t)[[attributeValue objectAtIndex:1] integerValue]);
        [self valdi_setBorderColor:color animator:animator];
    }

    return YES;
}

- (void)valdi_layoutBackgroundLayer:(CAGradientLayer *)backgroundLayer animator:(id<SCValdiAnimatorProtocol> )animator
{
    if (animator) {
        CGSize size = self.frame.size;
        CGPoint center = CGPointMake(size.width / 2, size.height / 2);
        [animator addAnimationOnLayer:backgroundLayer
                               forKeyPath:@"position"
                                    value:[NSValue valueWithCGPoint:center]];
        [animator addAnimationOnLayer:backgroundLayer
                               forKeyPath:@"bounds"
                                    value:[NSValue valueWithCGRect:CGRectMake(0, 0, size.width, size.height)]];
    } else {
        backgroundLayer.frame = self.layer.bounds;
    }
}

- (BOOL)valdi_applyMaskPath:(id)attributeValue animator:(id<SCValdiAnimatorProtocol>)animator
{
    [self valdi_updateMaskLayerWithBlock:^(SCValdiMaskLayer *maskLayer) {
        maskLayer.maskPath = attributeValue;
    }];

    return YES;
}

- (void)valdi_resetMaskPathWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    [self valdi_applyMaskPath:nil animator:animator];
}

- (BOOL)valdi_applyMaskOpacity:(CGFloat)maskOpacity animator:(id<SCValdiAnimatorProtocol>)animator
{
    [self valdi_updateMaskLayerWithBlock:^(SCValdiMaskLayer *maskLayer) {
        maskLayer.maskOpacity = maskOpacity;
    }];

    return YES;
}

- (void)valdi_resetMaskOpacity:(id<SCValdiAnimatorProtocol>)animator
{
    [self valdi_applyMaskOpacity:1 animator:animator];
}

- (BOOL)valdi_setBackground:(NSArray *)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    NSArray *colors = attributeValue.firstObject;

    CAGradientLayer *backgroundGradientLayer = self.backgroundGradientLayer;
    if (colors.count < 2 && backgroundGradientLayer) {
        [backgroundGradientLayer removeFromSuperlayer];
        self.backgroundGradientLayer = nil;
        backgroundGradientLayer = nil;

        [self.valdiViewNode setDidFinishLayoutBlock:nil forKey:kBackgroundGradientLayoutKey];
    }

    if (colors.count == 0) {
        [self valdi_setBackgroundColor:nil animator:animator];
    } else if (colors.count == 1) {
        [self valdi_setBackgroundColor:UIColorFromValdiAttributeValue(
                                              (int64_t)[[colors objectAtIndex:0] integerValue])
                                 animator:animator];
    } else {
        if (!backgroundGradientLayer) {
            backgroundGradientLayer = [[CAGradientLayer alloc] init];
            backgroundGradientLayer.delegate = [SCValdiNoAnimationDelegate sharedInstance];
            backgroundGradientLayer.zPosition = -1.0;
            self.backgroundGradientLayer = backgroundGradientLayer;
        }

        backgroundGradientLayer = setUpGradientLayerForRawAttributes(attributeValue, backgroundGradientLayer);

        backgroundGradientLayer.cornerRadius = self.layer.cornerRadius;
        if (@available(iOS 11.0, *)) {
            backgroundGradientLayer.maskedCorners = self.layer.maskedCorners;
        }

        [self valdi_layoutBackgroundLayer:backgroundGradientLayer animator:animator];

        [self.valdiViewNode setDidFinishLayoutBlock:^(UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_layoutBackgroundLayer:backgroundGradientLayer animator:animator];
        }
                                                forKey:kBackgroundGradientLayoutKey];

        [self.layer insertSublayer:backgroundGradientLayer atIndex:0];
    }
    return YES;
}

- (BOOL)valdi_setBackgroundColor:(UIColor *)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    // We want `nil` to default to "clear" rather than "black".
    attributeValue = attributeValue ?: [UIColor clearColor];
    if (animator) {
        [animator addAnimationOnLayer:self.layer
                           forKeyPath:@"backgroundColor"
                                value:(__bridge id)attributeValue.CGColor];
    }
    self.layer.backgroundColor = attributeValue.CGColor;
    return YES;
}

- (void)valdi_applySlowClipping:(BOOL)slowClipping animator:(id<SCValdiAnimatorProtocol> )animator
{
    self.clipsToBounds = slowClipping;
}

static CGFloat SCValdiComputeCornerRadius(SCValdiDoubleValue value, CGFloat sizeLimit)
{
    if (value.isPercent) {
        return MIN(sizeLimit / 2, value.value / 100 * sizeLimit);
    } else {
        if (sizeLimit > 0) {
            return MIN(sizeLimit / 2, value.value);
        } else {
            return value.value; // TODO(1146) Graceful degradation, but we shouln't hit this
        }
    }
}

static BOOL SCValdiUpdateHighestCornerRadius(CGFloat cornerRadius, CACornerMask corner, CGFloat *outCornerRadius,
                                                CACornerMask *outCornerMask)
{
    if (cornerRadius != 0) {
        if (*outCornerRadius != 0 && *outCornerRadius != cornerRadius) {
            return NO;
        }
        *outCornerRadius = cornerRadius;
        *outCornerMask = (*outCornerMask) | corner;
    }

    return YES;
}

static void SCValdiDetermineCornerRadiusMethod(CGFloat topLeftRadius, CGFloat topRightRadius,
                                                  CGFloat bottomLeftRadius, CGFloat bottomRightRadius,
                                                  BOOL *outNeedShapeLayer, CACornerMask *outCornerMask,
                                                  CGFloat *outCornerRadius)
{
    CACornerMask mask = 0;
    CGFloat cornerRadius = 0;

    if (!SCValdiUpdateHighestCornerRadius(topLeftRadius, kCALayerMinXMinYCorner, &cornerRadius, &mask) ||
        !SCValdiUpdateHighestCornerRadius(topRightRadius, kCALayerMaxXMinYCorner, &cornerRadius, &mask) ||
        !SCValdiUpdateHighestCornerRadius(bottomLeftRadius, kCALayerMinXMaxYCorner, &cornerRadius, &mask) ||
        !SCValdiUpdateHighestCornerRadius(bottomRightRadius, kCALayerMaxXMaxYCorner, &cornerRadius, &mask)) {
        *outNeedShapeLayer = YES;
        return;
    }

    if (mask == 0) {
        // Keep all corners by default
        mask = kCornerMaskAll;
    }

    *outNeedShapeLayer = NO;
    *outCornerRadius = cornerRadius;
    *outCornerMask = mask;
}

- (void)valdi_applyCornerMask:(CACornerMask)cornerMask cornerRadius:(CGFloat)cornerRadius animator:(id<SCValdiAnimatorProtocol>)animator
{
    CALayer *gradientLayer = self.backgroundGradientLayer;
    if (animator) {
        [animator addAnimationOnLayer:self.layer forKeyPath:@"cornerRadius" value:@(cornerRadius)];
        if (gradientLayer) {
            [animator addAnimationOnLayer:gradientLayer forKeyPath:@"cornerRadius" value:@(cornerRadius)];
        }
    } else {
        self.layer.cornerRadius = cornerRadius;
        gradientLayer.cornerRadius = cornerRadius;
    }

    if (@available(iOS 11.0, *)) {
        self.layer.maskedCorners = cornerMask;
        gradientLayer.maskedCorners = cornerMask;
    }
}

- (void)valdi_applyMaskLayerWithTopLeftRadius:(CGFloat)topLeftRadius
                                  topRightRadius:(CGFloat)topRightRadius
                                bottomLeftRadius:(CGFloat)bottomLeftRadius
                               bottomRightRadius:(CGFloat)bottomRightRadius
                                            bounds:(CGRect)bounds
                                        animator:(id<SCValdiAnimatorProtocol>)animator
{

    [self valdi_updateMaskLayerWithBlock:^(SCValdiMaskLayer *maskLayer) {
        if (animator) {
            if (!maskLayer.path) {
                // Making sure we have a from path to animate if we didn't have one.
                CACornerMask previousCornerMask = kCornerMaskAll;
                if (@available(iOS 11.0, *)) {
                    previousCornerMask = self.layer.maskedCorners;
                }
                CGFloat previousCornerRadius = self.layer.cornerRadius;
                CGFloat previousTopLeftRadius = previousCornerMask & kCALayerMinXMinYCorner ? previousCornerRadius : 0;
                CGFloat previousTopRightRadius = previousCornerMask & kCALayerMaxXMinYCorner ? previousCornerRadius : 0;
                CGFloat previousBottomLeftRadius = previousCornerMask & kCALayerMinXMaxYCorner ? previousCornerRadius : 0;
                CGFloat previousBottomRightRadius = previousCornerMask & kCALayerMaxXMaxYCorner ? previousCornerRadius : 0;

                [maskLayer setTopLeftCornerRadius:previousTopLeftRadius
                             topRightCornerRadius:previousTopRightRadius
                          bottomRightCornerRadius:previousBottomRightRadius
                           bottomLeftCornerRadius:previousBottomLeftRadius];
            }

            CGPathRef previousPath = maskLayer.path;
            CFRetain(previousPath);

            [maskLayer updateBounds:bounds];
            [maskLayer setTopLeftCornerRadius:topLeftRadius
                         topRightCornerRadius:topRightRadius
                      bottomRightCornerRadius:bottomRightRadius
                       bottomLeftCornerRadius:bottomLeftRadius];

            CGPathRef newPath = maskLayer.path;
            CFRetain(newPath);

            // Set the old path so that the animation catches it
            maskLayer.path = previousPath;
            [animator addAnimationOnLayer:maskLayer forKeyPath:@"path" value:(__bridge id)newPath];

            CFRelease(newPath);
            CFRelease(previousPath);
        } else {
            [maskLayer setTopLeftCornerRadius:topLeftRadius
                         topRightCornerRadius:topRightRadius
                      bottomRightCornerRadius:bottomRightRadius
                       bottomLeftCornerRadius:bottomLeftRadius];
        }
    }];

    [self valdi_applyCornerMask:kCornerMaskAll cornerRadius:0 animator:nil];
}

- (void)valdi_applyBorderRadius:(SCValdiCornerValues)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    CGSize size = self.bounds.size;
    CGFloat sizeLimit = MIN(size.width, size.height);

    CGFloat topLeftRadius = SCValdiComputeCornerRadius(attributeValue.topLeft, sizeLimit);
    CGFloat topRightRadius = SCValdiComputeCornerRadius(attributeValue.topRight, sizeLimit);
    CGFloat bottomLeftRadius = SCValdiComputeCornerRadius(attributeValue.bottomLeft, sizeLimit);
    CGFloat bottomRightRadius = SCValdiComputeCornerRadius(attributeValue.bottomRight, sizeLimit);

    BOOL needShapeLayer = NO;
    CGFloat cornerRadius = 0;
    CACornerMask cornerMask = 0;

    SCValdiDetermineCornerRadiusMethod(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius,
                                          &needShapeLayer, &cornerMask, &cornerRadius);

    if (!needShapeLayer) {
        if (@available(iOS 11.0, *)) {
            // On iOS 11 and above, we can use the masked corners property unless
            // we are animating between different maskedCorner values or if we already
            // had a mask layer.
            if (animator) {
                needShapeLayer = (cornerMask != self.layer.maskedCorners) || self.layer.mask;
            }
        } else {
            // on iOS 10, because we don't have the masked corners property,
            // we should always use a shape layer if all corner radii are NOT equal.
            needShapeLayer = !(topLeftRadius == topRightRadius && topRightRadius == bottomLeftRadius &&
                               bottomLeftRadius == bottomRightRadius);
        }
    }

    needShapeLayer = needShapeLayer || [self requiresShapeLayerForBorderRadius];
    if (!needShapeLayer) {
        if (self.layer.mask) {
            [self valdi_updateMaskLayerWithBlock:^(SCValdiMaskLayer *maskLayer) {
                [maskLayer updateBounds:self.bounds];
                [maskLayer setTopLeftCornerRadius:0 topRightCornerRadius:0 bottomRightCornerRadius:0 bottomLeftCornerRadius:0];
            }];
        }

        [self valdi_applyCornerMask:cornerMask cornerRadius:cornerRadius animator:animator];
    } else {
        [self valdi_applyMaskLayerWithTopLeftRadius:topLeftRadius
                                        topRightRadius:topRightRadius
                                      bottomLeftRadius:bottomLeftRadius
                                     bottomRightRadius:bottomRightRadius
                                                bounds:self.bounds
                                              animator:animator];
    }

    if (self.layer.shadowPath) {
        [self valdi_applyShadowPathWithAnimator:animator];
    }
}

- (BOOL)valdi_setBorderRadius:(SCValdiCornerValues)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    [self valdi_applyBorderRadius:attributeValue animator:animator];

    [self.valdiViewNode setDidFinishLayoutBlock:^(UIView *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_applyBorderRadius:attributeValue animator:animator];
    }
                                            forKey:@"uiview-borderradius"];

    return YES;
}

- (BOOL)valdi_setBorderWidth:(CGFloat)attributeValue animator:(id<SCValdiAnimatorProtocol> )animator
{
    CGFloat normalizedValue = CGFloatNormalizeFloor(attributeValue);
    if (animator) {
        [animator addAnimationOnLayer:self.layer forKeyPath:@"borderWidth" value:@(normalizedValue)];
    } else {
        self.layer.borderWidth = normalizedValue;
    }

    return YES;
}

- (UIGestureRecognizer *)valdi_gestureRecognizerForClass:(Class)cls
{
    for (UIGestureRecognizer *gestureRecognizer in self.gestureRecognizers) {
        if (gestureRecognizer.class == cls) {
            return gestureRecognizer;
        }
    }
    return nil;
}

- (UIGestureRecognizer<SCValdiGestureRecognizer> *)valdi_setupGestureRecognizerForClass:(Class)cls function:(id<SCValdiFunction>)function predicate:(id<SCValdiFunction>)predicate
{
    UIGestureRecognizer<SCValdiGestureRecognizer> *gestureRecognizer = (UIGestureRecognizer<SCValdiGestureRecognizer> *)[self valdi_gestureRecognizerForClass:cls];
    if (!gestureRecognizer) {
        gestureRecognizer = [[cls alloc] init];
        [self addGestureRecognizer:gestureRecognizer];
    }

    [gestureRecognizer setFunction:function];
    [gestureRecognizer setPredicate:predicate];

    return gestureRecognizer;
}

- (void)valdi_resetGestureRecognizerForClass:(Class)cls
{
    UIGestureRecognizer *gestureRecognizer = [self valdi_gestureRecognizerForClass:cls];
    if (gestureRecognizer) {
        [self removeGestureRecognizer:gestureRecognizer];
    }
}

- (BOOL)valdi_setOnTouchGestures:(id)attributeValue
{
    NSArray *attributeValueArray = ObjectAs(attributeValue, NSArray);

    if (attributeValueArray.count != 4) {
        return NO;
    }

    SCValdiTouchGestureRecognizer *touchGestureRecognizer =
        ObjectAs([self valdi_gestureRecognizerForClass:[SCValdiTouchGestureRecognizer class]],
                                SCValdiTouchGestureRecognizer);
    if (!touchGestureRecognizer) {
        touchGestureRecognizer = [SCValdiTouchGestureRecognizer new];
        [self addGestureRecognizer:touchGestureRecognizer];
    }

    id<SCValdiFunction> onTouchFunction = ProtocolAs(attributeValueArray[0], SCValdiFunction);
    id<SCValdiFunction> onTouchStartFunction = ProtocolAs(attributeValueArray[1], SCValdiFunction);
    id<SCValdiFunction> onTouchEndFunction = ProtocolAs(attributeValueArray[2], SCValdiFunction);
    NSNumber *onTouchDelayDuration = ObjectAs(attributeValueArray[3], NSNumber);

    [touchGestureRecognizer setFunction:onTouchFunction forGestureType:SCValdiTouchGestureTypeAll];
    [touchGestureRecognizer setFunction:onTouchStartFunction forGestureType:SCValdiTouchGestureTypeBegan];
    [touchGestureRecognizer setFunction:onTouchEndFunction forGestureType:SCValdiTouchGestureTypeEnded];

    touchGestureRecognizer.onTouchDelayDuration = onTouchDelayDuration ? [onTouchDelayDuration doubleValue] : 0;

    return YES;
}

- (void)valdi_resetOnTouchGestures
{
    SCValdiTouchGestureRecognizer *touchGestureRecognizer =
        ObjectAs([self valdi_gestureRecognizerForClass:[SCValdiTouchGestureRecognizer class]],
                                SCValdiTouchGestureRecognizer);
    if (!touchGestureRecognizer) {
        return;
    }

    [self removeGestureRecognizer:touchGestureRecognizer];
}

- (BOOL)valdi_setAlpha:(CGFloat)alpha animator:(id<SCValdiAnimatorProtocol> )animator
{
    if (animator) {
        [animator addAnimationOnLayer:self.layer forKeyPath:@"opacity" value:@(alpha)];
    } else {
        self.layer.opacity = (float)alpha;
    }
    return YES;
}

- (BOOL)valdi_setTranslationX:(CGFloat)translationX
                    translationY:(CGFloat)translationY
                          scaleX:(CGFloat)scaleX
                          scaleY:(CGFloat)scaleY
                        rotation:(CGFloat)rotation
                        animator:(id<SCValdiAnimatorProtocol> )animator
{
    CATransform3D scale = CATransform3DMakeScale(scaleX, scaleY, 1.0);
    CATransform3D rotate = CATransform3DMakeRotation(rotation, 0.0, 0.0, 1.0);
    CATransform3D translate = CATransform3DMakeTranslation(translationX, translationY, 0.0);
    CATransform3D transform = CATransform3DConcat(CATransform3DConcat(scale, rotate), translate);

    if (animator) {
        NSValue *value = [NSValue valueWithCATransform3D:transform];
        [animator addAnimationOnLayer:self.layer forKeyPath:@"transform" value:value];
    } else {
        self.layer.transform = transform;
    }
    return YES;
}

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    [attributesBinder bindAttribute:@"background"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(__kindof UIView *view, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBackground:attributeValue animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBackground:@[] animator:animator];
        }];

    [attributesBinder bindAttribute:@"backgroundColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(__kindof UIView *view, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBackgroundColor:attributeValue animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBackgroundColor:nil animator:animator];
        }];

    // No-op binding. this attribute is only used for android.
    [attributesBinder bindAttribute:@"filterTouchesWhenObscured"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(__kindof UIView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return YES;
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
        }];
    [attributesBinder bindAttribute:@"touchEnabled"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(__kindof UIView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            view.userInteractionEnabled = attributeValue;
            return YES;
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            view.userInteractionEnabled = YES;
        }];
    [attributesBinder bindAttribute:@"borderRadius"
        invalidateLayoutOnChange:NO
        withBordersBlock:^BOOL(__kindof UIView *view, SCValdiCornerValues attributeValue,
                               id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBorderRadius:attributeValue animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBorderRadius:(SCValdiCornerValues) {
                .topLeft = SCValdiDoubleMakeValue(0), .topRight = SCValdiDoubleMakeValue(0),
                .bottomRight = SCValdiDoubleMakeValue(0), .bottomLeft = SCValdiDoubleMakeValue(0)
            }
                                  animator:animator];
        }];
    [attributesBinder bindAttribute:@"opacity"
        invalidateLayoutOnChange:NO
        withPercentBlock:^BOOL(__kindof UIView *view, SCValdiDoubleValue attributeValue,
                               id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setAlpha:SCValdiDoubleValueToRatio(attributeValue) animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setAlpha:1 animator:animator];
        }];
    [attributesBinder bindAttribute:@"boxShadow"
        invalidateLayoutOnChange:NO
        withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBoxShadow:ObjectAs(attributeValue, NSArray) animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBoxShadow:nil animator:animator];
        }];
    [attributesBinder bindAttribute:@"objectFit"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(__kindof UIView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setObjectFit:attributeValue];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setObjectFit:nil];
        }];
    [attributesBinder bindAttribute:@"accessibilityId"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(__kindof UIView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            view.accessibilityIdentifier = attributeValue;
            return YES;
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            view.accessibilityIdentifier = nil;
        }];
    [attributesBinder bindAttribute:@"border"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(__kindof UIView *view, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBorder:attributeValue animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBorder:nil animator:animator];
        }];
    [attributesBinder bindAttribute:@"borderWidth"
        invalidateLayoutOnChange:NO
        withDoubleBlock:^BOOL(__kindof UIView *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBorderWidth:attributeValue animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBorderWidth:0 animator:animator];
        }];
    [attributesBinder bindAttribute:@"borderColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(__kindof UIView *view, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setBorderColor:attributeValue animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setBorderColor:nil animator:animator];
        }];
    [attributesBinder bindAttribute:@"slowClipping"
        invalidateLayoutOnChange:NO
        withBoolBlock:^BOOL(__kindof UIView *view, BOOL attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_applySlowClipping:attributeValue animator:animator];
            return YES;
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_applySlowClipping:view.clipsToBoundsByDefault animator:animator];
        }];

    [attributesBinder bindAttribute:@"maskPath" invalidateLayoutOnChange:NO withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
        return [view valdi_applyMaskPath:attributeValue animator:animator];;
    } resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_resetMaskPathWithAnimator:animator];
    }];

    [attributesBinder bindAttribute:@"maskOpacity" invalidateLayoutOnChange:NO withDoubleBlock:^BOOL(__kindof UIView *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
        return [view valdi_applyMaskOpacity:attributeValue animator:animator];
    } resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_resetMaskOpacity:animator];
    }];

    [attributesBinder bindAttribute:@"onTap"
        withFunctionAndPredicateBlock:^(__kindof UIView *view, id<SCValdiFunction> attributeValue, id<SCValdiFunction> predicate, id additionalValue) {
            [view valdi_setupGestureRecognizerForClass:[SCValdiTapGestureRecognizer class] function:attributeValue predicate:predicate];
        }
        resetBlock:^(__kindof UIView *view) {
            [view valdi_resetGestureRecognizerForClass:[SCValdiTapGestureRecognizer class]];
        }];
    [attributesBinder bindAttribute:@"onDoubleTap"
      withFunctionAndPredicateBlock:^(__kindof UIView *view, id<SCValdiFunction> attributeValue, id<SCValdiFunction> predicate, id additionalValue) {
          [view valdi_setupGestureRecognizerForClass:[SCValdiFastDoubleTapGestureRecognizer class] function:attributeValue predicate:predicate];
      }
                         resetBlock:^(__kindof UIView *view) {
                             [view valdi_resetGestureRecognizerForClass:[SCValdiFastDoubleTapGestureRecognizer class]];
                         }];
    [attributesBinder bindAttribute:@"onDrag"
        withFunctionAndPredicateBlock:^(__kindof UIView *view, id<SCValdiFunction> attributeValue, id<SCValdiFunction> predicate, id additionalValue) {
            [view valdi_setupGestureRecognizerForClass:[SCValdiDragGestureRecognizer class]
                                                   function:attributeValue predicate:predicate];
        }
        resetBlock:^(__kindof UIView *view) {
            [view valdi_resetGestureRecognizerForClass:[SCValdiDragGestureRecognizer class]];
        }];
    [attributesBinder bindAttribute:@"onPinch"
        withFunctionAndPredicateBlock:^(__kindof UIView *view, id<SCValdiFunction> attributeValue, id<SCValdiFunction> predicate, id additionalValue) {
            [view valdi_setupGestureRecognizerForClass:[SCValdiPinchGestureRecognizer class]
                                                   function:attributeValue predicate:predicate];
        }
        resetBlock:^(__kindof UIView *view) {
            [view valdi_resetGestureRecognizerForClass:[SCValdiPinchGestureRecognizer class]];
        }];
    [attributesBinder bindAttribute:@"onRotate"
        withFunctionAndPredicateBlock:^(__kindof UIView *view, id<SCValdiFunction> attributeValue, id<SCValdiFunction> predicate, id additionalValue) {
            [view valdi_setupGestureRecognizerForClass:[SCValdiRotationGestureRecognizer class]
                                                   function:attributeValue predicate:predicate];
        }
        resetBlock:^(__kindof UIView *view) {
            [view valdi_resetGestureRecognizerForClass:[SCValdiRotationGestureRecognizer class]];
        }];
    SCNValdiCoreCompositeAttributePart *onLongPressDuration = [[SCNValdiCoreCompositeAttributePart alloc]
                                                              initWithAttribute:@"longPressDuration"
                                                              type:SCNValdiCoreAttributeTypeDouble
                                                              optional:YES
                                                              invalidateLayoutOnChange:NO];
    [attributesBinder bindAttribute:@"onLongPress"
                additionalAttribute:onLongPressDuration
        withFunctionAndPredicateBlock:^(__kindof UIView *view, id<SCValdiFunction> attributeValue, id<SCValdiFunction> predicate, id additionalValue) {
            SCValdiLongPressGestureRecognizer *longPressGestureRecognizer = (SCValdiLongPressGestureRecognizer *)[view valdi_setupGestureRecognizerForClass:[SCValdiLongPressGestureRecognizer class]
                                                                                                                    function:attributeValue predicate:predicate];
            NSNumber *longPressDuration = ObjectAs(additionalValue, NSNumber);
            if (longPressDuration) {
                longPressGestureRecognizer.minimumPressDuration = [longPressDuration doubleValue];
            } else {
                longPressGestureRecognizer.minimumPressDuration = kSCValdiMinLongPressDuration;
            }
        }
        resetBlock:^(__kindof UIView *view) {
            [view valdi_resetGestureRecognizerForClass:[SCValdiLongPressGestureRecognizer class]];
        }];
    [attributesBinder bindCompositeAttribute:@"onTouchComposite"
                                       parts:[self _valdiOnTouchComponents]
                            withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
                                    return [view valdi_setOnTouchGestures:(attributeValue)];
                                 }
                                  resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
                                    [view valdi_resetOnTouchGestures];
                                 }];

    [attributesBinder bindCompositeAttribute:@"transform"
        parts:[self _valdiTransformComponents]
        withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            NSArray *attributeValueArray = ObjectAs(attributeValue, NSArray);
            if (attributeValueArray.count != 5) {
                return NO;
            }

            id<SCValdiViewNodeProtocol> viewNode = view.valdiViewNode;

            CGFloat translationX = [viewNode resolveDeltaX:ObjectAs(attributeValueArray[0], NSNumber).doubleValue directionAgnostic:NO];
            CGFloat translationY = ObjectAs(attributeValueArray[1], NSNumber).doubleValue;

            CGFloat scaleX = (ObjectAs(attributeValueArray[2], NSNumber) ?: @(1.0)).doubleValue;
            CGFloat scaleY = (ObjectAs(attributeValueArray[3], NSNumber) ?: @(1.0)).doubleValue;

            CGFloat rotation = [viewNode resolveDeltaX:ObjectAs(attributeValueArray[4], NSNumber).doubleValue directionAgnostic:NO];

            return [view valdi_setTranslationX:translationX
                                     translationY:translationY
                                           scaleX:scaleX
                                           scaleY:scaleY
                                         rotation:rotation
                                         animator:animator];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setTranslationX:0 translationY:0 scaleX:1.0 scaleY:1.0 rotation:0.0 animator:animator];
        }];
    [attributesBinder bindCompositeAttribute:@"touchAreaExtensionComposite"
        parts:[self _valdiTouchAreaExtensionComponents]
        withUntypedBlock:^BOOL(__kindof UIView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setTouchAreaExtensionValue:attributeValue];
        }
        resetBlock:^(__kindof UIView *view, id<SCValdiAnimatorProtocol> animator) {
            if ([view respondsToSelector:@selector(setHitTestSlop:)]) {
                [(id)view setHitTestSlop:UIEdgeInsetsZero];
            }
        }];
    [attributesBinder bindAttribute:@"hitTest"
        withFunctionBlock:^(UIView *view, id<SCValdiFunction> attributeValue) {
            view.valdiHitTest = attributeValue;
        }
        resetBlock:^(__kindof UIView *view) {
            view.valdiHitTest = nil;
        }];
}

+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)_valdiTransformComponents
{
    return @[
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"translationX"
                                                                type:SCNValdiCoreAttributeTypeDouble
                                                            optional:YES
                                            invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"translationY"
                                                                type:SCNValdiCoreAttributeTypeDouble
                                                            optional:YES
                                            invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"scaleX"
                                                                type:SCNValdiCoreAttributeTypeDouble
                                                            optional:YES
                                            invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"scaleY"
                                                                type:SCNValdiCoreAttributeTypeDouble
                                                            optional:YES
                                            invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"rotation"
                                                                type:SCNValdiCoreAttributeTypeDouble
                                                            optional:YES
                                            invalidateLayoutOnChange:NO],
    ];
}

+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)_valdiTouchAreaExtensionComponents
{
    NSMutableArray<SCNValdiCoreCompositeAttributePart *> *parts = [NSMutableArray arrayWithCapacity:5];
    [parts addObject:[[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:@"touchAreaExtension"
                                                                             type:SCNValdiCoreAttributeTypeDouble
                                                                         optional:YES
                                                         invalidateLayoutOnChange:NO]];
    for (NSString *direction in @[ @"Top", @"Right", @"Bottom", @"Left" ]) {
        [parts addObject:[[SCNValdiCoreCompositeAttributePart alloc]
                                    initWithAttribute:[@"touchAreaExtension" stringByAppendingString:direction]
                                                 type:SCNValdiCoreAttributeTypeDouble
                                             optional:YES
                             invalidateLayoutOnChange:NO]];
    }
    return parts;
}

+ (NSArray<SCNValdiCoreCompositeAttributePart *> *)_valdiOnTouchComponents
{
    return @[
        [[SCNValdiCoreCompositeAttributePart alloc]
                                    initWithAttribute:@"onTouch"
                                                 type:SCNValdiCoreAttributeTypeUntyped
                                             optional:YES
                                 invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc]
                                    initWithAttribute:@"onTouchStart"
                                                 type:SCNValdiCoreAttributeTypeUntyped
                                             optional:YES
                             invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc]
                                    initWithAttribute:@"onTouchEnd"
                                                 type:SCNValdiCoreAttributeTypeUntyped
                                             optional:YES
                             invalidateLayoutOnChange:NO],
        [[SCNValdiCoreCompositeAttributePart alloc]
                                    initWithAttribute:@"onTouchDelayDuration"
                                                 type:SCNValdiCoreAttributeTypeDouble
                                             optional:YES
                             invalidateLayoutOnChange:NO],
      ];
}

- (BOOL)valdi_setTouchAreaExtensionValue:(id)attributeValue
{
    NSArray<NSNumber *> *valueArray = ObjectAs(attributeValue, NSArray);
    if (valueArray.count != 5) {
        return NO;
    }

    UIEdgeInsets insets = UIEdgeInsetsZero;
    if (ObjectAs(valueArray[0], NSNumber)) {
        insets.top = insets.right = insets.bottom = insets.left =
            -1 * ObjectAs(valueArray[0], NSNumber).doubleValue;
    }
    if (ObjectAs(valueArray[1], NSNumber)) {
        insets.top = -1 * ObjectAs(valueArray[1], NSNumber).doubleValue;
    }
    if (ObjectAs(valueArray[2], NSNumber)) {
        insets.right = -1 * ObjectAs(valueArray[2], NSNumber).doubleValue;
    }
    if (ObjectAs(valueArray[3], NSNumber)) {
        insets.bottom = -1 * ObjectAs(valueArray[3], NSNumber).doubleValue;
    }
    if (ObjectAs(valueArray[4], NSNumber)) {
        insets.left = -1 * ObjectAs(valueArray[4], NSNumber).doubleValue;
    }

    if ([self respondsToSelector:@selector(setHitTestSlop:)]) {
        [(id)self setHitTestSlop:insets];
    }
    return YES;
}

- (void)setBackgroundGradientLayer:(CALayer *)backgroundGradientLayer
{
    objc_setAssociatedObject(self, @selector(backgroundGradientLayer), backgroundGradientLayer,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (CAGradientLayer *)backgroundGradientLayer
{
    return objc_getAssociatedObject(self, @selector(backgroundGradientLayer));
}

@end
