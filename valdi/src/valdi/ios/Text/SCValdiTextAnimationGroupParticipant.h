//
//  SCValdiTextAnimationGroupParticipant.h
//  Valdi
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class SCValdiTextAnimationCoordinator;

/**
 * A native text view that can be driven by an ancestor text animation group.
 *
 * Membership and animation context are intentionally separate. The native text
 * views that own animated text rendering register with a nearby group because
 * they observe their own hierarchy changes. The group then applies only the
 * scheduling context participants need: the shared coordinator and the
 * participant's base part index in DFS order.
 */
@protocol SCValdiTextAnimationGroupParticipant <NSObject>

/**
 * The number of animated attributed-text parts currently rendered by this
 * participant. The group sums this value across earlier DFS participants to
 * compute a participant's base part index.
 */
- (NSUInteger)valdi_textAnimationPartCount;

/**
 * Applies or clears the group-owned scheduling context for this participant.
 *
 * Passing nil restores local, participant-owned animation scheduling. Passing a
 * coordinator suppresses the participant's local frame driver and makes its
 * effective part delay start at `basePartIndex`.
 */
- (void)valdi_applyTextAnimationCoordinator:(nullable SCValdiTextAnimationCoordinator *)coordinator
                              basePartIndex:(NSUInteger)basePartIndex;

/**
 * Clears the participant's membership back-reference when the group forcibly
 * detaches it, such as during unregister or view-pool recycling.
 */
- (void)valdi_clearTextAnimationGroupRegistration;

/**
 * First phase of a grouped animation frame.
 *
 * Participants report existing active animation start times to the coordinator
 * here, before any participant creates new animation ranges for the frame.
 */
- (void)valdi_prepareGroupedTextAnimationFrame;

/**
 * Second phase of a grouped animation frame.
 *
 * Participants invalidate/redraw their current animated text state and return
 * YES while they still have active animation ranges.
 */
- (BOOL)valdi_invalidateGroupedTextAnimationFrame;

@end

NS_ASSUME_NONNULL_END
