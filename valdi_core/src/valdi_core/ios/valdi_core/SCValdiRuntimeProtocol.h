#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "valdi_core/SCNValdiCoreModuleFactory.h"
#import "valdi_core/SCValdiContextProtocol.h"
#import "valdi_core/SCValdiJSRuntime.h"
#import "valdi_core/SCValdiRootViewProtocol.h"

@protocol SCValdiRuntimeManagerProtocol;
@protocol SCValdiContextProtocol;
@protocol SCValdiViewFactory;
@class SCNValdiCoreAsset;

NS_ASSUME_NONNULL_BEGIN

/**
 * The Valdi runtime is the main workhorse of the framework
 *  An instance of the runtime is all that’s needed to initialize a UIView that is configured and driven by a given
 * .valdimodule
 */
@protocol SCValdiRuntimeProtocol <NSObject>

/**
 * Register a factory instance for a global module that can be accessed from the TypeScript.
 * Use this to expose functionality to TypeScript that should be available globally.
 */
- (void)registerNativeModuleFactory:(id<SCNValdiCoreModuleFactory>)moduleFactory;

/** Call to asynchronously get an opaque instance of the underlying JS runtime that can then be used with
 * @GenerateNativeFunc generated code to call functions defined in your Valdi modules.
 */
- (void)getJSRuntimeWithBlock:(void (^)(id<SCValdiJSRuntime> _Nullable))block;

/**
 Returns the JS Runtime instance that can then be used with
 * @GenerateNativeFunc generated code to call functions defined in your Valdi modules.
 */
- (id<SCValdiJSRuntime> _Nullable)jsRuntime;

/**
 Inflate a valdi view directly. It will add all the subviews and apply the styling that are specified in the
 document.
 */
- (void)inflateView:(UIView<SCValdiRootViewProtocol>*)view
               owner:(id _Nullable)owner
           viewModel:(id _Nullable)viewModel
    componentContext:(id _Nullable)componentContext;

/**
 Inflate a valdi view directly. It will add all the subviews and apply the styling that are specified in the
 document.
 */
- (void)inflateView:(UIView<SCValdiRootViewProtocol>*)view owner:(id _Nullable)owner cppMarshaller:(void*)cppMarshaller;

/**
 Create a Valdi Context with the given componentPath, initial view model and component context.
 The backing component will asynchronously render.
 */
- (id<SCValdiContextProtocol> _Nullable)createContextWithComponentPath:(NSString*)componentPath
                                                             viewModel:(id _Nullable)viewModel
                                                      componentContext:(id _Nullable)componentContext;

/**
 Create a Valdi Context with the given view class, initial view model and component context.
 The view class MUST be inheriting SCValdiRootView
 The backing component will asynchronously render.
 */
- (id<SCValdiContextProtocol> _Nullable)createContextWithViewClass:(Class)viewClass
                                                         viewModel:(id _Nullable)viewModel
                                                  componentContext:(id _Nullable)componentContext;

/**
 Load the view given the Valdi document name. The document should be
 in the app bundle under <bundleName>/<viewName>.valdi to load the document.
 */
- (UIView<SCValdiRootViewProtocol>* _Nullable)loadViewWithComponentPath:(NSString*)componentPath
                                                                  owner:(id _Nullable)owner
                                                                  error:(NSError* _Nullable* _Nullable)error;

/**
 Load the view given the Valdi document name. The document should be
 in the app bundle under <bundleName>/<viewName>.valdi to load the document.
 */
- (UIView<SCValdiRootViewProtocol>* _Nullable)loadViewWithComponentPath:(NSString*)componentPath
                                                                  owner:(id _Nullable)owner
                                                              viewModel:(id _Nullable)viewModel
                                                       componentContext:(id _Nullable)componentContext
                                                                  error:(NSError* _Nullable* _Nullable)error;

/**
 * Execute the provided block on the runtime's main thread manager's main thread batch.
 */
- (void)executeMainThreadBatch:(dispatch_block_t)function;

/**
 * Dump the recorded log metadata into a string.
 */
- (NSString*)dumpLogMetadata;

/**
 * Dump the recorded logs into a string.
 */
- (NSString*)dumpLogs;

/**
 * Get the current ValdiContext instance.
 * Will be only available as part of call of JS to Objective-C
 */
- (id<SCValdiContextProtocol> _Nullable)currentContext;

/**
  Creates a SCValdiViewFactory instance from the given factory block and attributes binder.
  The returned instance can be passed around in the view model or component context.
  The created views from this factory will be enqueued into a local view pool instead of the global view pool.
  If provided, the attributesBinder callback will be used to bind any additional attributes.
  The view class is used to resolve the attributes of the view.
 */
- (id<SCValdiViewFactory>)makeViewFactoryWithBlock:(SCValdiViewFactoryBlock)viewFactory
                                  attributesBinder:(SCValdiBindAttributesCallback _Nullable)attributesBinder
                                          forClass:(Class)viewClass;

/**
 * Get the runtime manager that is responsible for this runtime.
 */
- (id<SCValdiRuntimeManagerProtocol> _Nullable)manager;

/**
 * Returns a Valdi asset for the given module name and path.
 * The returned asset can be used to load its underlying content for use
 * in iOS code, or it can be sent back to TS code for further processing.
 */
- (SCNValdiCoreAsset* _Nullable)assetWithModuleName:(NSString*)moduleName path:(NSString*)path;

/**
 * Returns a Valdi asset for the given URL.
 * The returned asset can be used to load its underlying content for use
 * in iOS code, or it can be sent back to TS code for further processing.
 */
- (SCNValdiCoreAsset* _Nullable)assetWithURL:(NSString*)url;

/**
 Synchronously flush the pending load operations that were enqueued in the main thread.
 This can be called in the main thread after initializing the Runtime for the first time if
 a response from the JS thread needs to be retrieved synchronously. By default, the runtime
 will initially enqueue some load operations in the main thread to populate data from some
 bridge modules like the device and application modules, which requires accesses to UIKit objects.
 It will prevent these modules to be usable in JS until the load operations have completed.
 flushPendingMainThreadLoadOperations() can be called in the main thread to immediately unblock
 these modules.
 */
@optional
- (void)flushPendingMainThreadLoadOperations;

@end

@protocol SCValdiMainRuntimeProvider <NSObject>
- (id<SCValdiRuntimeProtocol> _Nullable)provideMainRuntime;
@end

NS_ASSUME_NONNULL_END
