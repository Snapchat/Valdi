//
//  SCValdiTextGradientHelper.m
//  Valdi
//

#import "valdi/ios/Text/SCValdiTextGradientHelper.h"

#import "valdi/ios/Utils/GradientUtils.h"
#import "valdi_core/SCValdiAnimatorBase.h"

@implementation SCValdiTextGradientHelper {
    CAGradientLayer *_gradientLayer;
    UIColor *_gradientColor;
    BOOL _needsColorUpdate;
}

- (CAGradientLayer *)gradientLayer
{
    return _gradientLayer;
}

- (UIColor *)gradientColor
{
    return _gradientColor;
}

- (BOOL)setGradientAttributes:(NSArray *)attributeValue
{
    NSArray *colors = attributeValue.firstObject;
    if (colors.count < 2) {
        _gradientLayer = nil;
        _gradientColor = nil;
        _needsColorUpdate = NO;
        return NO;
    }

    _gradientLayer = setUpGradientLayerForRawAttributes(attributeValue, nil);
    _gradientColor = nil;
    _needsColorUpdate = YES;
    return YES;
}

- (BOOL)hasGradient
{
    return _gradientLayer != nil;
}

- (BOOL)needsColorUpdate
{
    return _needsColorUpdate;
}

- (void)layoutInView:(UIView *)view animator:(id<SCValdiAnimatorProtocol>)animator
{
    if (!_gradientLayer) {
        return;
    }

    if (animator) {
        CGSize size = view.frame.size;
        CGPoint center = CGPointMake(size.width / 2, size.height / 2);
        [animator addAnimationOnLayer:_gradientLayer forKeyPath:@"position" value:[NSValue valueWithCGPoint:center]];
        [animator addAnimationOnLayer:_gradientLayer forKeyPath:@"bounds" value:[NSValue valueWithCGRect:CGRectMake(0, 0, size.width, size.height)]];
    } else {
        _gradientLayer.frame = view.layer.bounds;
    }
}

- (BOOL)layoutIfNeededInView:(UIView *)view animator:(id<SCValdiAnimatorProtocol>)animator
{
    if (!_gradientLayer || CGRectEqualToRect(_gradientLayer.frame, view.layer.bounds)) {
        return NO;
    }

    [self layoutInView:view animator:animator];
    _needsColorUpdate = YES;
    return YES;
}

- (BOOL)updateColorIfNeeded
{
    if (!_needsColorUpdate) {
        return NO;
    } else if (!_gradientLayer) {
        _gradientColor = nil;
        _needsColorUpdate = NO;
        return YES;
    } else if (CGSizeEqualToSize(_gradientLayer.bounds.size, CGSizeZero)) {
        return NO;
    }

    [_gradientLayer layoutIfNeeded];
    UIGraphicsBeginImageContextWithOptions(_gradientLayer.bounds.size, NO, 0.0);
    [_gradientLayer renderInContext:UIGraphicsGetCurrentContext()];
    UIImage *gradientImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    if (!gradientImage) {
        return NO;
    }

    _gradientColor = [UIColor colorWithPatternImage:gradientImage];
    _needsColorUpdate = NO;
    return YES;
}

@end
