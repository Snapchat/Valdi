//
//  SCValdiRuntimeManager.h
//  Valdi
//
//  Created by Simon Corsin on 1/8/19.
//

#import "valdi_core/SCValdiConfiguration.h"
#import "valdi_core/SCValdiRuntimeManagerProtocol.h"

#import "valdi/ios/SCValdiRuntime.h"

#import <Foundation/Foundation.h>

@protocol SCValdiDebugMessageDisplayer;
@protocol SCNValdiCoreHTTPRequestManager;

@class SCValdiRuntimeManager;
@class SCValdiCapturedJSStacktrace;

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiRuntimeManager : NSObject <SCValdiRuntimeManagerProtocol>

/**
 The username of the current user. Used for hot reloading.
 */
@property (nonatomic, nullable, copy) NSString* currentUsername;

@property (nonatomic, nullable, readonly) SCValdiRuntime* mainRuntime;

/**
 Return the underlying C++ instance of the RuntimeManager.
 */
@property (nonatomic, readonly, nullable) void* cppInstance;

- (nullable SCValdiRuntime*)createRuntimeWithCustomModuleProvider:
    (nonnull id<SCValdiCustomModuleProvider>)moduleProvider;

- (void)clearViewPools;

/**
 Configure the current user session.
 */
- (void)setUserSessionWithUserId:(NSString*)userId;

- (void)setUserSessionWithUserId:(NSString*)userId userIv:(NSString*)userIv;

- (void)setDebugMessageDisplayer:(id<SCValdiDebugMessageDisplayer>)debugMessageDisplayer;

- (void)setRequestManager:(id<SCNValdiCoreHTTPRequestManager>)requestManager;

/**
 * Kick off a preload operation for the given view class name.
 * It will instantiate "count" instances of that view class and
 * put them into the view pool, which can make inflation faster
 * later on if those views are needed for rendering.
 * The preloader creates those views in the main thread but doesn't
 * spend more than 10ms per frame to create them.
 */
- (void)preloadViewsOfClass:(Class)viewClass count:(NSInteger)count;

- (void)registerViewClassReplacement:(Class)viewClassToReplace withViewClass:(Class)replacementViewClass;

/**
 Returns all of the active SCValdiRuntimeManager instances
 */
+ (NSArray<SCValdiRuntimeManager*>*)allRuntimeManagers;

@end

NS_ASSUME_NONNULL_END
