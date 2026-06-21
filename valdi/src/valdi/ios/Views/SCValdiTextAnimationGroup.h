//
//  SCValdiTextAnimationGroup.h
//  Valdi
//

#import "valdi_core/SCValdiView.h"
#import "valdi/ios/Text/SCValdiTextAnimationGroupParticipant.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiTextAnimationCoordinator;

@interface SCValdiTextAnimationGroup : SCValdiView

@property (nonatomic, strong, readonly) SCValdiTextAnimationCoordinator *textAnimationCoordinator;

- (void)registerTextAnimationParticipant:(UIView<SCValdiTextAnimationGroupParticipant> *)participant;
- (void)unregisterTextAnimationParticipant:(UIView<SCValdiTextAnimationGroupParticipant> *)participant;
- (void)startTextAnimationFrameLoopIfNeeded;

@end

NS_ASSUME_NONNULL_END
