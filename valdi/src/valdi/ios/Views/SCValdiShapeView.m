//
//  SCValdiShapeView.m
//  SCValdiUI
//
//  Created by Nathaniel Parrott on 6/6/19.
//

#import "valdi/ios/Views/SCValdiShapeView.h"

#import "valdi_core/SCValdiLogger.h"
#import "valdi/ios/SCValdiAttributesBinder.h"
#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi/ios/Utils/GradientUtils.h"
#import "valdi_core/SCValdiGeometricPath.h"

static NSString *const kFillGradientLayoutKey = @"shape_fill_gradient";

@implementation SCValdiShapeView {
    UIColor *_fillColor;
    UIColor *_strokeColor;
    CAGradientLayer *_fillGradientLayer;
    CAShapeLayer *_fillGradientMaskLayer;
    CAShapeLayer *_fillGradientStrokeLayer;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    if (self) {
        [self shapeLayer].fillColor = nil;
        [self shapeLayer].strokeColor = nil;
        [self valdi_setStrokeCap:nil];
        [self valdi_setStrokeJoin:nil];
    }

    return self;
}

#pragma mark - Valdi

- (void)valdi_applyPath:(id)pathData animator:(id<SCValdiAnimatorProtocol>)animator
{
    CGPathRef pathRef = CGPathFromGeometricPathData(pathData, self.bounds.size);

    if (animator) {
        [animator addAnimationOnLayer:[self shapeLayer]
                           forKeyPath:@"path"
                                value:(__bridge id)pathRef];
        if (_fillGradientMaskLayer) {
            [animator addAnimationOnLayer:_fillGradientMaskLayer
                               forKeyPath:@"path"
                                    value:(__bridge id)pathRef];
            [animator addAnimationOnLayer:_fillGradientStrokeLayer
                               forKeyPath:@"path"
                                    value:(__bridge id)pathRef];
        }
    } else {
        [self shapeLayer].path = pathRef;
        _fillGradientMaskLayer.path = pathRef;
        _fillGradientStrokeLayer.path = pathRef;
    }
    
    if (pathRef) {
        CFRelease(pathRef);
    }
}

- (void)valdi_setPath:(id)pathData animator:(id<SCValdiAnimatorProtocol>)animator
{
    [self valdi_applyPath:pathData animator:animator];

    [self.valdiViewNode setDidFinishLayoutBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_applyPath:pathData animator:animator];
    } forKey:@"shape"];
}

- (void)valdi_setStrokeStart:(CGFloat)strokeStart animator:(id<SCValdiAnimatorProtocol>)animator
{
    if (animator) {
        [animator addAnimationOnLayer:[self shapeLayer]
                           forKeyPath:@"strokeStart"
                                value:@(strokeStart)];
        if (_fillGradientStrokeLayer) {
            [animator addAnimationOnLayer:_fillGradientStrokeLayer
                               forKeyPath:@"strokeStart"
                                    value:@(strokeStart)];
        }
    } else {
        [self shapeLayer].strokeStart = strokeStart;
        _fillGradientStrokeLayer.strokeStart = strokeStart;
    }
}

- (void)valdi_setStrokeEnd:(CGFloat)strokeEnd animator:(id<SCValdiAnimatorProtocol>)animator
{
    if (animator) {
        [animator addAnimationOnLayer:[self shapeLayer]
                           forKeyPath:@"strokeEnd"
                                value:@(strokeEnd)];
        if (_fillGradientStrokeLayer) {
            [animator addAnimationOnLayer:_fillGradientStrokeLayer
                               forKeyPath:@"strokeEnd"
                                    value:@(strokeEnd)];
        }
    } else {
        [self shapeLayer].strokeEnd = strokeEnd;
        _fillGradientStrokeLayer.strokeEnd = strokeEnd;
    }
}

- (void)_layoutFillGradientWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    if (!_fillGradientLayer) {
        return;
    }

    CGRect bounds = self.layer.bounds;
    if (animator) {
        CGPoint center = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
        NSValue *position = [NSValue valueWithCGPoint:center];
        NSValue *layerBounds = [NSValue valueWithCGRect:CGRectMake(0, 0, bounds.size.width, bounds.size.height)];

        for (CALayer *layer in @[_fillGradientLayer, _fillGradientMaskLayer, _fillGradientStrokeLayer]) {
            [animator addAnimationOnLayer:layer forKeyPath:@"position" value:position];
            [animator addAnimationOnLayer:layer forKeyPath:@"bounds" value:layerBounds];
        }
    } else {
        _fillGradientLayer.frame = bounds;
        _fillGradientMaskLayer.frame = _fillGradientLayer.bounds;
        _fillGradientStrokeLayer.frame = bounds;
    }
}

- (void)_resetFillGradientWithAnimator:(id<SCValdiAnimatorProtocol>)animator
{
    [_fillGradientLayer removeFromSuperlayer];
    [_fillGradientStrokeLayer removeFromSuperlayer];
    _fillGradientLayer = nil;
    _fillGradientMaskLayer = nil;
    _fillGradientStrokeLayer = nil;

    [self.valdiViewNode setDidFinishLayoutBlock:nil forKey:kFillGradientLayoutKey];
    [self _setFillColor:_fillColor animator:animator];
    [self _setStrokeColor:_strokeColor animator:animator];
}

- (BOOL)valdi_setFillGradient:(NSArray *)attributeValue animator:(id<SCValdiAnimatorProtocol>)animator
{
    NSArray *colors = attributeValue.firstObject;
    if (colors.count < 2) {
        if (colors.count == 1) {
            _fillColor = UIColorFromValdiAttributeValue((int64_t)[colors.firstObject integerValue]);
        }
        [self _resetFillGradientWithAnimator:animator];
        return YES;
    }

    CAShapeLayer *shapeLayer = [self shapeLayer];
    if (!_fillGradientLayer) {
        _fillGradientLayer = [CAGradientLayer layer];

        _fillGradientMaskLayer = [CAShapeLayer layer];
        _fillGradientMaskLayer.fillColor = UIColor.blackColor.CGColor;
        _fillGradientMaskLayer.path = shapeLayer.path;
        _fillGradientLayer.mask = _fillGradientMaskLayer;

        _fillGradientStrokeLayer = [CAShapeLayer layer];
        _fillGradientStrokeLayer.fillColor = nil;
        _fillGradientStrokeLayer.path = shapeLayer.path;
        _fillGradientStrokeLayer.strokeColor = shapeLayer.strokeColor;
        _fillGradientStrokeLayer.lineWidth = shapeLayer.lineWidth;
        _fillGradientStrokeLayer.lineCap = shapeLayer.lineCap;
        _fillGradientStrokeLayer.lineJoin = shapeLayer.lineJoin;
        _fillGradientStrokeLayer.strokeStart = shapeLayer.strokeStart;
        _fillGradientStrokeLayer.strokeEnd = shapeLayer.strokeEnd;

        shapeLayer.fillColor = nil;
        shapeLayer.strokeColor = nil;
        [shapeLayer insertSublayer:_fillGradientLayer atIndex:0];
        [shapeLayer insertSublayer:_fillGradientStrokeLayer above:_fillGradientLayer];
    }

    setUpGradientLayerForRawAttributes(attributeValue, _fillGradientLayer);
    [self _layoutFillGradientWithAnimator:animator];

    [self.valdiViewNode setDidFinishLayoutBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
        [view _layoutFillGradientWithAnimator:animator];
    } forKey:kFillGradientLayoutKey];

    return YES;
}

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    [attributesBinder bindAttribute:@"path"
        invalidateLayoutOnChange:NO
                   withUntypedBlock:^BOOL(SCValdiShapeView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setPath:attributeValue animator:animator];
        return YES;
    }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
        [view valdi_setPath:nil animator:animator];
        }];
    [attributesBinder bindAttribute:@"strokeCap"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiShapeView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setStrokeCap:attributeValue];
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setStrokeCap:nil];
        }];
    [attributesBinder bindAttribute:@"strokeJoin"
        invalidateLayoutOnChange:NO
        withStringBlock:^BOOL(SCValdiShapeView *view, NSString *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setStrokeJoin:attributeValue];
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setStrokeJoin:nil];
        }];
    [attributesBinder bindAttribute:@"fillColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(SCValdiShapeView *view, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view _setFillColor:attributeValue animator:animator];
            return YES;
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view _setFillColor:nil animator:animator];
        }];
    [attributesBinder bindAttribute:@"fillGradient"
        invalidateLayoutOnChange:NO
        withArrayBlock:^BOOL(SCValdiShapeView *view, NSArray *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            return [view valdi_setFillGradient:attributeValue animator:animator];
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view _resetFillGradientWithAnimator:animator];
        }];
    [attributesBinder bindAttribute:@"strokeColor"
        invalidateLayoutOnChange:NO
        withColorBlock:^BOOL(SCValdiShapeView *view, UIColor *attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view _setStrokeColor:attributeValue animator:animator];
            return YES;
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view _setStrokeColor:nil animator:animator];
        }];
    [attributesBinder bindAttribute:@"strokeWidth"
        invalidateLayoutOnChange:NO
        withDoubleBlock:^BOOL(SCValdiShapeView *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view _setLineWidth:attributeValue animator:animator];
            return YES;
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view _setLineWidth:0 animator:animator];
        }];
    [attributesBinder bindAttribute:@"strokeStart"
        invalidateLayoutOnChange:NO
        withDoubleBlock:^BOOL(SCValdiShapeView *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setStrokeStart:attributeValue animator:animator];
            return YES;
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setStrokeStart:0 animator:animator];
        }];
    [attributesBinder bindAttribute:@"strokeEnd"
        invalidateLayoutOnChange:NO
        withDoubleBlock:^BOOL(SCValdiShapeView *view, CGFloat attributeValue, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setStrokeEnd:attributeValue animator:animator];
            return YES;
        }
        resetBlock:^(SCValdiShapeView *view, id<SCValdiAnimatorProtocol> animator) {
            [view valdi_setStrokeEnd:1 animator:animator];
        }];
}

- (BOOL)willEnqueueIntoValdiPool
{
    return YES;
}

- (BOOL)requiresLayoutWhenAnimatingBounds
{
    return NO;
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self _layoutFillGradientWithAnimator:nil];
}

#pragma mark - Shape layer

- (CAShapeLayer *)shapeLayer
{
    return (CAShapeLayer *)self.layer;
}

+ (Class)layerClass
{
    return [CAShapeLayer class];
}

- (void)_setFillColor:(UIColor *)color animator:(id<SCValdiAnimatorProtocol> )animator
{
    _fillColor = color;
    if (_fillGradientLayer) {
        return;
    }

    if (animator) {
        [animator addAnimationOnLayer:self.shapeLayer forKeyPath:NSStringFromSelector(@selector(fillColor)) value:(__bridge id)color.CGColor];
    } else {
        self.shapeLayer.fillColor = color.CGColor;
    }
}

- (void)_setStrokeColor:(UIColor *)color animator:(id<SCValdiAnimatorProtocol> )animator
{
    _strokeColor = color;
    CAShapeLayer *strokeLayer = _fillGradientStrokeLayer ?: self.shapeLayer;

    if (animator) {
        [animator addAnimationOnLayer:strokeLayer
                           forKeyPath:NSStringFromSelector(@selector(strokeColor))
                                value:(__bridge id)color.CGColor];
    } else {
        strokeLayer.strokeColor = color.CGColor;
    }
}

- (void)_setLineWidth:(CGFloat)lineWidth animator:(id<SCValdiAnimatorProtocol> )animator
{
    if (animator) {
        [animator addAnimationOnLayer:self.shapeLayer forKeyPath:NSStringFromSelector(@selector(lineWidth)) value:@(lineWidth)];
        if (_fillGradientStrokeLayer) {
            [animator addAnimationOnLayer:_fillGradientStrokeLayer
                               forKeyPath:NSStringFromSelector(@selector(lineWidth))
                                    value:@(lineWidth)];
        }
    } else {
        self.shapeLayer.lineWidth = lineWidth;
        _fillGradientStrokeLayer.lineWidth = lineWidth;
    }
}

- (BOOL)valdi_setStrokeCap:(NSString *)strokeCap
{
    CAShapeLayerLineCap lineCap = kCALineCapButt;
    if (strokeCap) {
        if ([strokeCap isEqualToString:@"butt"]) {
            lineCap = kCALineCapButt;
        } else if ([strokeCap isEqualToString:@"round"]) {
            lineCap = kCALineCapRound;
        } else if ([strokeCap isEqualToString:@"square"]) {
            lineCap = kCALineCapSquare;
        } else {
            return NO;
        }
    }
    [self shapeLayer].lineCap = lineCap;
    _fillGradientStrokeLayer.lineCap = lineCap;
    return YES;
}

- (BOOL)valdi_setStrokeJoin:(NSString *)strokeJoin
{
    CAShapeLayerLineJoin lineJoin = kCALineJoinMiter;
    if (strokeJoin) {
        if ([strokeJoin isEqualToString:@"bevel"]) {
            lineJoin = kCALineJoinBevel;
        } else if ([strokeJoin isEqualToString:@"miter"]) {
            lineJoin = kCALineJoinMiter;
        } else if ([strokeJoin isEqualToString:@"round"]) {
            lineJoin = kCALineJoinRound;
        } else {
            return NO;
        }
    }
    [self shapeLayer].lineJoin = lineJoin;
    _fillGradientStrokeLayer.lineJoin = lineJoin;
    return YES;
}

@end
