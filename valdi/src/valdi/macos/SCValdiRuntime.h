//
//  SCValdiRuntime.h
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#import <Foundation/Foundation.h>

@class NSView;

typedef struct Runtime Runtime;
typedef struct IMainThreadDispatcher IMainThreadDispatcher;
typedef struct ModuleFactoriesProviderSharedPtr ModuleFactoriesProviderSharedPtr;
typedef struct ViewManagerContext ViewManagerContext;
typedef struct SnapDrawingRuntime SnapDrawingRuntime;

@interface SCValdiRuntime : NSObject

/**
 * `usingTemporaryCachesDirectory` controls whether our subdirectories are rooted within a temporary directory.
 * This should be `NO` for any release build where cache permanence is desired, but it comes at the cost of the user
 * seeing a privacy prompt for accessing their documents directory.
 */
- (instancetype)initWithUsingTemporaryCachesDirectory:(BOOL)usingTemporaryCachesDirectory
                                      useHermesEngine:(BOOL)useHermesEngine;
- (instancetype)initWithUsingTemporaryCachesDirectory:(BOOL)usingTemporaryCachesDirectory;
- (instancetype)init NS_UNAVAILABLE;

@property (readonly, nonatomic) Runtime* nativeRuntime;
@property (readonly, nonatomic) ViewManagerContext* nativeViewManagerContext;
@property (readonly, nonatomic) IMainThreadDispatcher* mainThreadDispatcher;
@property (readonly, nonatomic) SnapDrawingRuntime* snapDrawingRuntime;

- (void)waitUntilReadyWithCompletion:(dispatch_block_t)completion;
- (void)setApplicationId:(const char*)applicationId;
- (void)registerModuleFactoriesProvider:(ModuleFactoriesProviderSharedPtr*)moduleFactoriesProvider;
- (void)setDisplayScale:(double)displayScale;

/// Creates a ViewFactory for a SnapDrawing layer class by name (e.g. @"SCFilePickerView"). Use for desktop so the view
/// is a layer, not a bridged NSView.
- (id)makeViewFactoryForSnapDrawingLayerClass:(NSString*)className;

/// Returns the SnapDrawing view manager pointer (opaque). Use to register module layer classes (e.g. valdi_polyglot).
- (void*)snapDrawingViewManager;

+ (NSArray<NSString*>*)getLaunchArguments;

@end
