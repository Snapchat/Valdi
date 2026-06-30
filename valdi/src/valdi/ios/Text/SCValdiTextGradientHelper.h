//
//  SCValdiTextGradientHelper.h
//  Valdi
//

#import <UIKit/UIKit.h>

@protocol SCValdiAnimatorProtocol;

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiTextGradientHelper : NSObject

@property (readonly, nonatomic, nullable) CAGradientLayer *gradientLayer;
@property (readonly, nonatomic, nullable) UIColor *gradientColor;

- (BOOL)setGradientAttributes:(nullable NSArray *)attributeValue;
- (BOOL)hasGradient;
- (BOOL)needsColorUpdate;
- (void)layoutInView:(UIView *)view animator:(nullable id<SCValdiAnimatorProtocol>)animator;
- (BOOL)layoutIfNeededInView:(UIView *)view animator:(nullable id<SCValdiAnimatorProtocol>)animator;
- (BOOL)updateColorIfNeeded;

@end

NS_ASSUME_NONNULL_END
