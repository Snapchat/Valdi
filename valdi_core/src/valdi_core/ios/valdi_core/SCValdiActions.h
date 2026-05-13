//
//  SCValdiActions.h
//  Valdi
//
//  Created by Simon Corsin on 4/27/18.
//

#import "valdi_core/SCMacros.h"

#import "valdi_core/SCValdiAction.h"

#import <Foundation/Foundation.h>

@class SCValdiActionHandlerHolder;

NS_ASSUME_NONNULL_BEGIN

/**
 Contains all actions given their name.
 */
@interface SCValdiActions : NSObject

@property (readonly, nonatomic) SCValdiActionHandlerHolder* actionHandlerHolder;
@property (readonly, nonatomic) NSDictionary<NSString*, id<SCValdiAction>>* actionByName;

VALDI_NO_INIT

- (instancetype)initWithActionByName:(NSDictionary<NSString*, id<SCValdiAction>>*)actionByName
                 actionHandlerHolder:(SCValdiActionHandlerHolder*)actionHandlerHolder;

- (nullable id<SCValdiAction>)actionForName:(NSString*)name;

@end

NS_ASSUME_NONNULL_END
