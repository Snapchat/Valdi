//
//  SCValdiTextAnimationPresentation.m
//  valdi-ios
//

#import "valdi/ios/Text/SCValdiTextAnimationPresentation.h"

@implementation SCValdiTextAnimationPresentation

- (instancetype)initWithTranslationY:(CGFloat)translationY
                                scale:(CGFloat)scale
                              opacity:(CGFloat)opacity
{
    self = [super init];
    if (self) {
        _translationY = translationY;
        _scale = scale;
        _opacity = opacity;
    }
    return self;
}

- (BOOL)hasOpacityOverride
{
    return _opacity != 1.0;
}

- (BOOL)hasTransformOverride
{
    return _translationY != 0.0 || _scale != 1.0;
}

@end
