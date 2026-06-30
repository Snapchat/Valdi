//
//  SCValdiTextAnimationCoordinator.m
//  Valdi
//

#import "valdi/ios/Text/SCValdiTextAnimationCoordinator.h"

@interface SCValdiTextAnimationCoordinatorTimelineState : NSObject
@property (nonatomic, assign) BOOL hasExistingAnimationStartTime;
@property (nonatomic, assign) CFTimeInterval existingAnimationStartTime;
@property (nonatomic, assign) BOOL hasNewAnimationStartTime;
@property (nonatomic, assign) CFTimeInterval newAnimationStartTime;
@end

@implementation SCValdiTextAnimationCoordinatorTimelineState
@end

@interface SCValdiTextAnimationCoordinator ()
@property (nonatomic, strong) NSMutableDictionary<NSString *, SCValdiTextAnimationCoordinatorTimelineState *> *timelineStates;
@end

@implementation SCValdiTextAnimationCoordinator

- (instancetype)init
{
    self = [super init];
    if (self) {
        _timelineStates = [NSMutableDictionary new];
    }
    return self;
}

- (SCValdiTextAnimationCoordinatorTimelineState *)_timelineStateForKey:(NSString *)timelineKey
{
    SCValdiTextAnimationCoordinatorTimelineState *timelineState = self.timelineStates[timelineKey];
    if (!timelineState) {
        timelineState = [SCValdiTextAnimationCoordinatorTimelineState new];
        self.timelineStates[timelineKey] = timelineState;
    }
    return timelineState;
}

- (void)resetFrameState
{
    [self.timelineStates removeAllObjects];
}

- (void)recordExistingAnimationScheduledStartTime:(CFTimeInterval)scheduledStartTime
                                   forTimelineKey:(NSString *)timelineKey
{
    SCValdiTextAnimationCoordinatorTimelineState *timelineState = [self _timelineStateForKey:timelineKey];
    if (!timelineState.hasExistingAnimationStartTime ||
        scheduledStartTime > timelineState.existingAnimationStartTime) {
        timelineState.hasExistingAnimationStartTime = YES;
        timelineState.existingAnimationStartTime = scheduledStartTime;
    }
}

- (CFTimeInterval)startTimeForNewAnimationWithTimelineKey:(NSString *)timelineKey
                                               timeOffset:(CFTimeInterval)timeOffset
                                              currentTime:(CFTimeInterval)currentTime
{
    SCValdiTextAnimationCoordinatorTimelineState *timelineState = [self _timelineStateForKey:timelineKey];
    if (!timelineState.hasNewAnimationStartTime) {
        timelineState.hasNewAnimationStartTime = YES;
        timelineState.newAnimationStartTime = timelineState.hasExistingAnimationStartTime ?
            MAX(currentTime, timelineState.existingAnimationStartTime + timeOffset) :
            currentTime;
    }
    return timelineState.newAnimationStartTime;
}

@end
