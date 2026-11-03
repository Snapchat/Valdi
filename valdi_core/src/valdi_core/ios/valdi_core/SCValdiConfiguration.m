//
//  SCValdiConfiguration.m
//  valdi-ios
//
//  Created by Simon Corsin on 5/8/20.
//

#import "SCValdiConfiguration.h"

#import <stdatomic.h>

static atomic_bool sDeferCompactPickerStyleResetEnabled = false;
static atomic_bool sScrollPinchWhileDraggingEnabled = false;

@implementation SCValdiConfiguration

+ (void)setDeferCompactPickerStyleResetEnabled:(BOOL)enabled
{
    atomic_store(&sDeferCompactPickerStyleResetEnabled, enabled);
}

+ (BOOL)deferCompactPickerStyleResetEnabled
{
    return atomic_load(&sDeferCompactPickerStyleResetEnabled);
}

+ (void)setScrollPinchWhileDraggingEnabled:(BOOL)enabled
{
    atomic_store(&sScrollPinchWhileDraggingEnabled, enabled);
}

+ (BOOL)scrollPinchWhileDraggingEnabled
{
    return atomic_load(&sScrollPinchWhileDraggingEnabled);
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        // Killswitch default: gesture prewarm is on unless a tweak flips it off.
        _enableGesturePrewarm = YES;
        self.enableDebuggerService = YES;
    }
    return self;
}

@end
