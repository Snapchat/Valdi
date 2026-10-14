//
//  SCValdiConfiguration.m
//  valdi-ios
//
//  Created by Simon Corsin on 5/8/20.
//

#import "SCValdiConfiguration.h"

@implementation SCValdiConfiguration

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
