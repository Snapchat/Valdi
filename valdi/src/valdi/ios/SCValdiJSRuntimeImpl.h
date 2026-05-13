//
//  SCValdiCachingJSRuntime.h
//  valdi-ios
//
//  Created by Simon Corsin on 4/11/19.
//

#import <Foundation/Foundation.h>

#import "valdi_core/SCNValdiCoreJSRuntime.h"
#import "valdi_core/SCValdiJSQueueDispatcher.h"
#import "valdi_core/SCValdiJSRuntime.h"

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiJSRuntimeProvider <SCValdiJSQueueDispatcher>

- (SCNValdiCoreJSRuntime*)getJsRuntime;

@end

@interface SCValdiJSRuntimeImpl : NSObject <SCValdiJSRuntime>

- (instancetype)initWithJSRuntimeProvider:(id<SCValdiJSRuntimeProvider>)jsRuntimeProvider;

@end

NS_ASSUME_NONNULL_END
