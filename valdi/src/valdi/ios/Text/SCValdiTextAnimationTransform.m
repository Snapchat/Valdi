//
//  SCValdiTextAnimationTransform.m
//  valdi-ios
//
//  Created by OpenAI on 6/2/26.
//

#import "valdi/ios/Text/SCValdiTextAnimationTransform.h"

@implementation SCValdiTextAnimationTransform

- (instancetype)initWithKey:(nullable NSString *)key
                  partIndex:(NSUInteger)partIndex
               translationY:(CGFloat)translationY
                      scale:(CGFloat)scale
                    opacity:(CGFloat)opacity
                   duration:(double)duration
     timeOffsetBetweenParts:(double)timeOffsetBetweenParts
                 groupIndex:(NSUInteger)groupIndex
           partIndexInGroup:(NSUInteger)partIndexInGroup
{
    self = [super init];
    if (self) {
        _key = [key copy];
        _partIndex = partIndex;
        _translationY = translationY;
        _scale = scale;
        _opacity = opacity;
        _duration = duration;
        _timeOffsetBetweenParts = timeOffsetBetweenParts;
        _groupIndex = groupIndex;
        _partIndexInGroup = partIndexInGroup;
    }
    return self;
}

@end
