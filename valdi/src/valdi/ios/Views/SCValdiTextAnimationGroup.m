//
//  SCValdiTextAnimationGroup.m
//  Valdi
//

#import "valdi/ios/Views/SCValdiTextAnimationGroup.h"
#import "valdi/ios/Text/SCValdiTextAnimationCoordinator.h"

#import <QuartzCore/QuartzCore.h>

@interface SCValdiTextAnimationGroup ()
@property (nonatomic, strong) NSHashTable<UIView<SCValdiTextAnimationGroupParticipant> *> *participants;
@property (nonatomic, strong) NSMutableArray<UIView<SCValdiTextAnimationGroupParticipant> *> *orderedParticipants;
@property (nonatomic, strong, readwrite) SCValdiTextAnimationCoordinator *textAnimationCoordinator;
@property (nonatomic, strong) CADisplayLink *displayLink;
@end

@implementation SCValdiTextAnimationGroup

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _participants = [NSHashTable weakObjectsHashTable];
        _orderedParticipants = [NSMutableArray new];
        _textAnimationCoordinator = [SCValdiTextAnimationCoordinator new];
    }
    return self;
}

- (void)dealloc
{
    [self _stopTextAnimationFrameLoop];
}

- (BOOL)willEnqueueIntoValdiPool
{
    [self _stopTextAnimationFrameLoop];
    for (UIView<SCValdiTextAnimationGroupParticipant> *participant in self.participants.allObjects) {
        [participant valdi_clearTextAnimationGroupRegistration];
    }
    [self.participants removeAllObjects];
    [self.orderedParticipants removeAllObjects];
    [self.textAnimationCoordinator resetFrameState];
    return self.class == [SCValdiTextAnimationGroup class];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    [self _rebuildOrderedParticipantsAndApplyBaseIndexes];
}

- (void)registerTextAnimationParticipant:(UIView<SCValdiTextAnimationGroupParticipant> *)participant
{
    if (!participant) {
        return;
    }

    [self.participants addObject:participant];
    [participant valdi_applyTextAnimationCoordinator:self.textAnimationCoordinator basePartIndex:0];
    [self setNeedsLayout];
}

- (void)unregisterTextAnimationParticipant:(UIView<SCValdiTextAnimationGroupParticipant> *)participant
{
    if (!participant) {
        return;
    }

    [self.participants removeObject:participant];
    [participant valdi_clearTextAnimationGroupRegistration];
    [self setNeedsLayout];
}

- (void)startTextAnimationFrameLoopIfNeeded
{
    [self setNeedsLayout];
    if (self.displayLink != nil) {
        return;
    }

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(_textAnimationDisplayLinkDidFire:)];
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)_stopTextAnimationFrameLoop
{
    [self.displayLink invalidate];
    self.displayLink = nil;
}

- (void)_textAnimationDisplayLinkDidFire:(CADisplayLink *)displayLink
{
    [self layoutIfNeeded];
    NSArray<UIView<SCValdiTextAnimationGroupParticipant> *> *orderedParticipants = self.orderedParticipants;
    [self.textAnimationCoordinator resetFrameState];

    for (UIView<SCValdiTextAnimationGroupParticipant> *participant in orderedParticipants) {
        [participant valdi_prepareGroupedTextAnimationFrame];
    }

    BOOL hasActiveAnimations = NO;
    for (UIView<SCValdiTextAnimationGroupParticipant> *participant in orderedParticipants) {
        hasActiveAnimations = [participant valdi_invalidateGroupedTextAnimationFrame] || hasActiveAnimations;
    }

    if (!hasActiveAnimations) {
        [self _stopTextAnimationFrameLoop];
    }
}

- (void)_rebuildOrderedParticipantsAndApplyBaseIndexes
{
    [self.orderedParticipants removeAllObjects];
    for (UIView *subview in self.subviews) {
        [self _collectParticipantsInView:subview output:self.orderedParticipants];
    }

    NSUInteger basePartIndex = 0;
    for (UIView<SCValdiTextAnimationGroupParticipant> *participant in self.orderedParticipants) {
        [participant valdi_applyTextAnimationCoordinator:self.textAnimationCoordinator basePartIndex:basePartIndex];
        basePartIndex += [participant valdi_textAnimationPartCount];
    }
}

- (void)_collectParticipantsInView:(UIView *)view
                             output:(NSMutableArray<UIView<SCValdiTextAnimationGroupParticipant> *> *)output
{
    if ([view isKindOfClass:[SCValdiTextAnimationGroup class]]) {
        return;
    }

    UIView<SCValdiTextAnimationGroupParticipant> *participant =
        (UIView<SCValdiTextAnimationGroupParticipant> *)view;
    if ([self.participants containsObject:participant]) {
        [output addObject:participant];
    }

    for (UIView *subview in view.subviews) {
        [self _collectParticipantsInView:subview output:output];
    }
}

@end
