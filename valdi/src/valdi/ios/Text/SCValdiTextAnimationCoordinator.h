//
//  SCValdiTextAnimationCoordinator.h
//  Valdi
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Coordinates attributed text animation timeline state across multiple rendered
 * text views inside a text animation group.
 *
 * The coordinator deliberately does not know about labels, text views, or view
 * hierarchy membership. It only owns per-frame timeline bookkeeping: existing
 * scheduled start times observed during a frame, and the shared start time for
 * newly-created animations in the same timeline key.
 */
@interface SCValdiTextAnimationCoordinator : NSObject

/**
 * Clears per-frame timeline bookkeeping.
 *
 * The owning text animation group calls this once at the start of each frame
 * before asking participants to report their active animations.
 */
- (void)resetFrameState;

/**
 * Records an already-active animation's scheduled start time for a timeline.
 *
 * Participants call this during the prepare pass. If multiple participants
 * report the same timeline key, the coordinator keeps the furthest scheduled
 * start time so newly-created animations can continue after existing work.
 */
- (void)recordExistingAnimationScheduledStartTime:(CFTimeInterval)scheduledStartTime
                                   forTimelineKey:(NSString *)timelineKey;

/**
 * Returns the shared start time for a new animation in a timeline.
 *
 * All new animations with the same timeline key in the same frame receive the
 * same base start time. If that timeline already has active animations, the new
 * base start time is offset after the furthest existing scheduled start time by
 * `timeOffset`.
 */
- (CFTimeInterval)startTimeForNewAnimationWithTimelineKey:(NSString *)timelineKey
                                               timeOffset:(CFTimeInterval)timeOffset
                                              currentTime:(CFTimeInterval)currentTime;

@end

NS_ASSUME_NONNULL_END
