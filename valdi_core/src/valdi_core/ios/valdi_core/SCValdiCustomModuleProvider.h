//
//  SCValdiCustomModuleProvider.h
//  valdi_core
//
//  Created by Simon Corsin on 9/27/21.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Protocol representing a custom provider for .valdimodule files.
 * When provided to the Runtime, getCustomModuleData will be called
 * to give an opportunity to the provider to return the module data that
 * should be used. This can be used to either provide more up to date
 * module data, or to provide module data for modules that are not bundled
 * in the app bundle.
 */
@protocol SCValdiCustomModuleProvider <NSObject>

/**
 Returns the module data (.valdimodule) file for the given module path.
 If the module cannot be loaded because of an error, this function should return
 nil and the error pointer should be set to the underlying error.
 If the module should be loaded from the built-in modules, this function should
 return nil and the error should be set to nil.
 */
- (NSData*)customModuleDataForPath:(NSString*)modulePath error:(NSError**)error;

@optional

/**
 Returns an identity of the data customModuleDataForPath:error: would return for the given module path, without
 loading it, or nil if this provider does not serve the path. The identity must change whenever the data could
 change. Without this method the runtime has to load every module to identify it.
 */
- (nullable NSString*)customModuleIdentityForPath:(NSString*)modulePath;

@end

NS_ASSUME_NONNULL_END
