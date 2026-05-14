//
//  SCValdiActionWithBlock.h
//  Valdi
//
//  Created by Simon Corsin on 5/7/18.
//

#import "valdi_core/SCMacros.h"

#import "valdi_core/SCValdiAction.h"
#import "valdi_core/SCValdiFunctionCompat.h"
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef id _Nullable (^SCValdiActionBlock)(NSArray<id>* parameters);

/**
 An SCValdiAction that responds using a block.
 */
@interface SCValdiActionWithBlock : SCValdiFunctionCompat <SCValdiAction>

VALDI_NO_INIT

- (instancetype)initWithBlock:(SCValdiActionBlock)block;

+ (instancetype)actionWithBlock:(SCValdiActionBlock)block;

@end

NS_ASSUME_NONNULL_END
