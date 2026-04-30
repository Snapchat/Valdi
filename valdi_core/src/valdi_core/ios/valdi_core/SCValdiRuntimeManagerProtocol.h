#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "valdi_core/SCValdiConfiguration.h"
#import "valdi_core/SCValdiCustomModuleProvider.h"
#import "valdi_core/SCValdiRuntimeProtocol.h"

@protocol SCValdiRuntimeManagerProtocol;
@class SCValdiCapturedJSStacktrace;
@protocol SCSnapDrawingRuntime;
@protocol SCNValdiCoreModuleFactoriesProvider;

NS_ASSUME_NONNULL_BEGIN

typedef struct {
    int64_t memoryUsageBytes;
    int64_t objectsCount;
} SCValdiMemoryStatistics;

typedef void (^SCValdiRuntimeCreatedCallback)(id<SCValdiRuntimeProtocol>);

/**
 The public API of the runtime manager.
 */
@protocol SCValdiRuntimeManagerProtocol <SCValdiMainRuntimeProvider>

/**
 Whether emitted Objective-C references are being tracked. Will be true if the SCValdiConfiguration
 was updated with enableReferenceTracking set.
 */
@property (readonly, nonatomic) BOOL referenceTrackingEnabled;

/**
 The block will be provided with a SCValdiConfiguration instance that you can mutate
 and the runtime manager will apply this configuration after the block finishes executing.
 */
- (void)updateConfiguration:(void (^)(SCValdiConfiguration* configuration))updateBlock;

/**
 Call this to unload all JS modules from memory and trigger garbage collection.
 */
- (void)unloadAllJsModules;

/**
 Creates a new isolated ValdiRuntime with a custom module provider.
 Valdi modules from this runtime will be loaded with the module provider
 instead of using the built-in modules.
 */
- (nullable id<SCValdiRuntimeProtocol>)createRuntimeWithCustomModuleProvider:
    (id<SCValdiCustomModuleProvider>)customModuleProvider;

/**
 * Returns the SnapDrawing Runtime
 */
- (nullable id<SCSnapDrawingRuntime>)snapDrawingRuntime;

/**
 * Registers a callback that will be called when the main runtime is created.
 */
- (void)registerMainRuntimeCreatedCallback:(SCValdiRuntimeCreatedCallback)callback;

/**
 * Registesr a ModuleFactoriesProvider, which will be invoked to provide additional modules when the runtime
 * is being initialized.
 */
- (void)registerModuleFactoriesProvider:(id<SCNValdiCoreModuleFactoriesProvider>)moduleFactoriesProvider;

/**
 * Register a type converter for the given native class and a path to the converter function at the TypeScript level.
 */
- (void)registerTypeConverterForClass:(Class)cls converterFunctionPath:(NSString*)converterFunctionPath;

/**
 * Register a type converter for the given native class and a path to the converter function at the TypeScript level.
 */
- (void)registerTypeConverterForClassName:(NSString*)clsName converterFunctionPath:(NSString*)converterFunctionPath;

/**
 Captures the stacktraces of all JS threads.
 */
- (nullable NSArray<SCValdiCapturedJSStacktrace*>*)captureStackTracesWithTimeoutMs:(NSUInteger)timeoutMs;

- (void)getWorkerOnExecutor:(NSString*)executor block:(void (^)(id<SCValdiJSRuntime>))block;

/**
 Aggregates memory statistics across all JS runtimes managed by this runtime manager.
 */
- (SCValdiMemoryStatistics)dumpMemoryStatistics;

@end

NS_ASSUME_NONNULL_END
