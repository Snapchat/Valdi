//
//  SCValdiBridgeFunction.h
//  valdi_core-ios
//
//  Created by Simon Corsin on 1/31/23.
//

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiJSRuntime.h"
#import "valdi_core/SCValdiMarshallableObject.h"
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiBridgeFunction : SCValdiMarshallableObject

@property (readonly, nonatomic) id callBlock;

VALDI_NO_INIT

/**
 Return the module path used by the function.
 Will be overriden by generated classes from the @GenerateNativeFunction
 code annotation.
 */
+ (NSString*)modulePath;

/**
 * Returns whether async strict mode is enabled for this function.
 * Will be overridden by generated classes.
 */
+ (BOOL)asyncStrictMode;

/**
 * Resolve and instantiate the function from the given JSRuntime instance
 */
+ (nonnull instancetype)functionWithJSRuntime:(nonnull id<SCValdiJSRuntime>)jsRuntime;

@end

extern id SCValdiMakeBridgeFunctionFromJSRuntime(Class objectClass, id<SCValdiJSRuntime> jsRuntime, NSString* path);

NS_ASSUME_NONNULL_END
