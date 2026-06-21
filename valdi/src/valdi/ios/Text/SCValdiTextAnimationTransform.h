//
//  SCValdiTextAnimationTransform.h
//  valdi-ios
//
//  Created by OpenAI on 6/2/26.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiTextAnimationTransform : NSObject

@property (nonatomic, copy, nullable, readonly) NSString *key;
@property (nonatomic, assign, readonly) NSUInteger partIndex;
@property (nonatomic, assign, readonly) CGFloat translationY;
@property (nonatomic, assign, readonly) CGFloat scale;
@property (nonatomic, assign, readonly) CGFloat opacity;
@property (nonatomic, assign, readonly) double duration;
@property (nonatomic, assign, readonly) double timeOffsetBetweenParts;
@property (nonatomic, assign, readonly) NSUInteger groupIndex;
@property (nonatomic, assign, readonly) NSUInteger partIndexInGroup;
@property (nonatomic, copy, nullable, readonly) NSString *partPattern;

- (instancetype)initWithKey:(nullable NSString *)key
                  partIndex:(NSUInteger)partIndex
               translationY:(CGFloat)translationY
                      scale:(CGFloat)scale
                    opacity:(CGFloat)opacity
                   duration:(double)duration
     timeOffsetBetweenParts:(double)timeOffsetBetweenParts
                 groupIndex:(NSUInteger)groupIndex
           partIndexInGroup:(NSUInteger)partIndexInGroup
                partPattern:(nullable NSString *)partPattern;

- (instancetype)copyWithPartIndex:(NSUInteger)partIndex
                 partIndexInGroup:(NSUInteger)partIndexInGroup;

@end

NS_ASSUME_NONNULL_END
