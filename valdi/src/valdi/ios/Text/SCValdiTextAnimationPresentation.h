//
//  SCValdiTextAnimationPresentation.h
//  valdi-ios
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiTextAnimationPresentation : NSObject

@property (nonatomic, assign, readonly) CGFloat translationY;
@property (nonatomic, assign, readonly) CGFloat scale;
@property (nonatomic, assign, readonly) CGFloat opacity;
@property (nonatomic, assign, readonly) BOOL hasOpacityOverride;
@property (nonatomic, assign, readonly) BOOL hasTransformOverride;

- (instancetype)initWithTranslationY:(CGFloat)translationY
                                scale:(CGFloat)scale
                              opacity:(CGFloat)opacity NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
