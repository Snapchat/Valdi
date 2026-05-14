//
//  SCValdiJSRuntime.h
//  valdi-ios
//
//  Created by Simon Corsin on 4/11/19.
//

#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiMarshaller.h"
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
An opaque instance of the underlying JS runtime that can be used with @GenerateNativeFunc
generated code to call functions defined in your Valdi modules.
*/
@protocol SCValdiJSRuntime <NSObject>

/**
 Used by the @GenerateNativeFunc generated code to get and call your TypeScript functions.
 */
- (NSInteger)pushModuleAthPath:(NSString*)modulePath inMarshaller:(SCValdiMarshallerRef)marshaller;

/**
    Preload the given module given as an absolute path (e.g. 'valdi_core/src/Renderer').
    When maxDepth is more than 1, the preload will apply recursively to modules that the given
    modulePath imports, up until the given depth.
    */
- (void)preloadModuleAtPath:(NSString*)path maxDepth:(NSUInteger)maxDepth;

/**
 * Batch-preload multiple modules in a single C++/JS-thread dispatch.
 */
- (void)preloadModulesAtPaths:(NSArray<NSString*>*)paths maxDepth:(NSUInteger)maxDepth;

/**
 * Pre-warm the JS-side value marshaller for the given object's type.
 * On first encounter, the JS runtime compiles a marshalling plan for each class schema;
 * calling this method triggers that compilation so that subsequent setupRootComponent or
 * callModuleFunction calls avoid the first-time cost.
 * The object must conform to SCValdiMarshallable (i.e. be a generated Valdi model class).
 * Safe to call from any thread; work is dispatched to the JS thread.
 */
- (void)warmUpValueMarshallerForObject:(id)object;

- (void)addHotReloadObserver:(id<SCValdiFunction>)hotReloadObserver forModulePath:(NSString*)modulePath;

/**
 * Observe hot reload for the module at the given path.
 * Calls the given callback when the module is hot reloaded.
 */
- (void)addHotReloadObserverWithBlock:(dispatch_block_t)block forModulePath:(NSString*)modulePath;

/**
 * Create a new scoped JSRuntime.
 *
 * The scoped JSRuntime will have its own native objects manager, which will cause all native
 * references emitted during interactions with the scoped JSRuntime to be disposed when
 * the scoped JSRuntime itself is disposed. You can use createScopedJSRuntime when you have a
 * bounded task and want to eagerly dispose native references when the task is done, rather than
 * wait until the JS engine garbage collects them.
 *
 * On iOS, ARC deallocates objects deterministically so the risk of cross-language retain cycles
 * is lower than on Android. However, scoped runtimes can still be useful when you want
 * deterministic cleanup of native references at a well-defined point rather than relying on
 * the JS garbage collector.
 *
 * @param scopeName A descriptive name identifying where this scoped runtime is created from.
 *                  This name appears in error messages to help debug issues with disposed
 *                  references. Callers should provide a meaningful name (e.g., class name
 *                  or feature name) to make error messages actionable.
 */
- (id<SCValdiJSRuntime>)createScopedJSRuntimeWithScopeName:(NSString*)scopeName;

/**
 * Destroy the JSRuntime. This is only legal to call on a JSRuntime instance returned from createScopedJSRuntime.
 * If not called, scoped JSRuntimes will be disposed when their reference count reaches 0.
 */
- (void)dispose;

- (void)dispatchInJsThread:(dispatch_block_t)block;

- (void)dispatchInJsThreadSyncWithBlock:(dispatch_block_t)block;

@end

NS_ASSUME_NONNULL_END
