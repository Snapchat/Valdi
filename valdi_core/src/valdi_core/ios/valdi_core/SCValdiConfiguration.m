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
        self.enableDebuggerService = YES;
    }
    return self;
}

@end
