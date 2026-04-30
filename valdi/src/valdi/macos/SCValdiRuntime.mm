//
//  SCValdiRuntime.m
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#import "SCValdiRuntime.h"

#import <Cocoa/Cocoa.h>

#import "valdi/snap_drawing/SnapDrawingViewManager.hpp"

#import "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#import "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#import "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#import "valdi/standalone_runtime/BridgeModules/ApplicationModule.hpp"
#import "valdi/standalone_runtime/BridgeModules/DeviceModule.hpp"
#import "valdi/jsbridge/JavaScriptBridge.hpp"

#import "valdi/runtime/RuntimeManager.hpp"
#import "valdi/runtime/Resources/DiskCacheImpl.hpp"
#import "valdi/runtime/Resources/AssetLoaderManager.hpp"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi_core/cpp/Threading/GCDDispatchQueue.hpp"
#import "valdi/runtime/Metrics/Metrics.hpp"
#import "valdi_core/cpp/Constants.hpp"
#import "valdi/runtime/Context/ViewManagerContext.hpp"

#import "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#import "valdi/snap_drawing/ImageLoading/ImageLoaderFactory.hpp"
#import "valdi/snap_drawing/Modules/SnapDrawingModuleFactoriesProvider.hpp"
#import "valdi/snap_drawing/Text/FontResolverWithRuntimeManager.hpp"

#import "SCValdiObjCUtils.h"
#import "SCValdiDefaultHTTPRequestManager.h"
#import "SCValdiMacOSViewManager.h"
#import "valdi_core/SCValdiWrappedValue+Private.h"
#import "valdi/runtime/Views/ViewFactory.hpp"
#import "valdi/snap_drawing/Runtime.hpp"
#import "SCValdiMacOSStringsBridgeModule.h"
#import "SCDirectoryUtils.h"
#import "valdi/macos/MacOSSnapDrawingRuntime.h"

class MainThreadDispatcher : public Valdi::IMainThreadDispatcher {
public:
    static void mainThreadEntry(void* context) {
        Valdi::DispatchFunction *function = reinterpret_cast<Valdi::DispatchFunction *>(context);
        (*function)();

        delete function;
    }

    void dispatch(Valdi::DispatchFunction* function, bool sync) override {
        if (sync) {
            dispatch_sync_f(dispatch_get_main_queue(), function, &MainThreadDispatcher::mainThreadEntry);
        } else {
            dispatch_async_f(dispatch_get_main_queue(), function, &MainThreadDispatcher::mainThreadEntry);
        }
    }
};

@implementation SCValdiRuntime {
    std::unique_ptr<ValdiMacOS::ViewManager> _macOSViewManager;
    Valdi::Ref<Valdi::ViewManagerContext> _viewManagerContext;
    Valdi::Ref<Valdi::RuntimeManager> _runtimeManager;
    Valdi::Ref<Valdi::DeviceModule> _deviceModule;
    Valdi::Ref<Valdi::ApplicationModule> _applicationModule;
    Valdi::SharedRuntime _runtime;
    Valdi::Ref<ValdiMacOS::MacOSSnapDrawingRuntime> _snapDrawingRuntime;
    Valdi::Ref<MainThreadDispatcher> _mainThreadDispatcher;
    NSMutableArray<dispatch_block_t> *_pendingReadyBlocks;
    BOOL _isReady;
}

- (instancetype)initWithUsingTemporaryCachesDirectory:(BOOL)usingTemporaryCachesDirectory
{
    return [self initWithUsingTemporaryCachesDirectory:usingTemporaryCachesDirectory useHermesEngine:NO];
}

- (instancetype)initWithUsingTemporaryCachesDirectory:(BOOL)usingTemporaryCachesDirectory
                                     useHermesEngine:(BOOL)useHermesEngine
{
    self = [super init];

    if (self) {
        Valdi::traceLoadModules = false;
        Valdi::traceInitialization = false;

        auto &logger = Valdi::ConsoleLogger::getLogger();

        if (Valdi::DispatchQueue::getMain() == nullptr) {
            Valdi::DispatchQueue::setMain(Valdi::makeShared<Valdi::GCDDispatchQueue>((__bridge void *)dispatch_get_main_queue()));
        }

        _mainThreadDispatcher = Valdi::makeShared<MainThreadDispatcher>();

        auto documentsDiskCache = Valdi::makeShared<Valdi::DiskCacheImpl>(ValdiMacOS::resolveDocumentsDirectory(usingTemporaryCachesDirectory));
        auto cachesDiskCache = Valdi::makeShared<Valdi::DiskCacheImpl>(ValdiMacOS::resolveCachesDirectory(usingTemporaryCachesDirectory));
        cachesDiskCache->setAllowedReadPath(STRING_LITERAL("/"));

        auto downloadPath = Valdi::Path("downloader");
        cachesDiskCache->remove(downloadPath);
        auto cachesImageCache = cachesDiskCache->scopedCache(downloadPath, true);

        auto requestManager = SCValdiCreateDefaultHTTPRequestManager();

        _macOSViewManager = std::make_unique<ValdiMacOS::ViewManager>();

        _runtimeManager = Valdi::makeShared<Valdi::RuntimeManager>(_mainThreadDispatcher,
                                                                               Valdi::JavaScriptBridge::get(useHermesEngine ? snap::valdi_core::JavaScriptEngineType::Hermes : snap::valdi_core::JavaScriptEngineType::QuickJS),
                                                                               documentsDiskCache,
                                                                               nullptr,
                                                                               nullptr,
                                                                               Valdi::PlatformTypeMacOS,
                                                                               Valdi::ThreadQoSClassMax,
                                                                               Valdi::strongSmallRef(&Valdi::ConsoleLogger::getLogger()),
                                                                               /* enableDebuggerService */ true,
                                                                               /* disableHotReloader */ false,
                                                                               /* isStandalone */ true);
        _runtimeManager->postInit();
        _runtimeManager->applicationDidResume();
        _runtimeManager->registerBytesAssetLoader(cachesImageCache);

        CGSize screenSize = [[NSScreen mainScreen] visibleFrame].size;
        auto maxCacheSizeInBytes = 2 * screenSize.width * screenSize.height;
        _snapDrawingRuntime = Valdi::makeShared<ValdiMacOS::MacOSSnapDrawingRuntime>(cachesDiskCache, *_macOSViewManager, logger, _runtimeManager->getWorkerQueue(), maxCacheSizeInBytes);
        _snapDrawingRuntime->registerAssetLoaders(*_runtimeManager->getAssetLoaderManager());
        _snapDrawingRuntime->getFontManager()->setListener(Valdi::makeShared<snap::drawing::FontResolverWithRuntimeManager>(_runtimeManager));

        [self setApplicationId:"ValdiDesktop"];
        _runtimeManager->setRequestManager(requestManager);

        // Rely on the hot reloader to get our resources for now
        _runtimeManager->setDisableRuntimeAutoInit(true);

        _runtimeManager->registerModuleFactoriesProvider(Valdi::makeShared<snap::drawing::SnapDrawingModuleFactoriesProvider>([snapDrawingRuntime = _snapDrawingRuntime]() -> Valdi::Ref<snap::drawing::Runtime> {
            return snapDrawingRuntime;
        }).toShared());

        _viewManagerContext = _runtimeManager->createViewManagerContext(*_snapDrawingRuntime->getViewManager(), false);

        auto resourceLoader = Valdi::makeShared<Valdi::StandaloneResourceLoader>();
        resourceLoader->addModuleSearchDirectory(StringFromNSString([[NSBundle mainBundle] resourcePath]));

        _runtime = _runtimeManager->createRuntime(resourceLoader, 1.0);

        _deviceModule = Valdi::makeShared<Valdi::DeviceModule>();
        _runtime->registerNativeModuleFactory(_deviceModule.toShared());

        _applicationModule = Valdi::makeShared<Valdi::ApplicationModule>();
        _runtime->registerNativeModuleFactory(_applicationModule.toShared());

        _runtime->registerNativeModuleFactory(Valdi::makeShared<ValdiMacOS::StringsModule>().toShared());

        _runtimeManager->attachHotReloader(_runtime);

        _pendingReadyBlocks = [NSMutableArray array];

        auto valdiCoreBundle = _runtime->getResourceManager().getBundle(STRING_LITERAL("valdi_core"));

        if (valdiCoreBundle->hasLoadedArchive()) {
            // We already have valdi_core, we can load the runtime now
            [self _initializeRuntime];
        } else {
            NSLog(@"Waiting for hot reloader resources before initializing runtime...");

            [self _initializeRuntimeIfReady];
        }

        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidResize:) name:NSWindowDidResizeNotification object:nil];
    }

    return self;
}

- (void)_initializeRuntimeIfReady
{
    if (_runtime->getHotReloadSequence() == 0) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self _initializeRuntimeIfReady];
        });
        return;
    }

    NSLog(@"Received hot reloader resources! Starting runtime...");
    [self _initializeRuntime];
}

- (void)_initializeRuntime
{
    _runtime->postInit();
    _runtime->getJavaScriptRuntime()->setDefaultViewManagerContext(_viewManagerContext);

    _isReady = YES;

    NSArray<dispatch_block_t> *completionBlocks = [_pendingReadyBlocks copy];
    [_pendingReadyBlocks removeAllObjects];

    for (dispatch_block_t block in completionBlocks) {
        block();
    }
}

- (void)_windowDidUpdate:(NSWindow *)window
{
    CGFloat backingScaleFactor = window.backingScaleFactor;
    _deviceModule->setDisplaySize(window.frame.size.width, window.frame.size.height, backingScaleFactor);

    [self setDisplayScale:backingScaleFactor];
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    NSWindow *window = notification.object;
    [self _windowDidUpdate:window];
}

- (void)_windowDidResize:(NSNotification *)notification
{
    NSWindow *window = notification.object;

    if (window.isKeyWindow) {
        [self _windowDidUpdate:window];
    }
}

- (void)setDisplayScale:(double)displayScale
{
    _snapDrawingRuntime->getResources()->setDisplayScale(static_cast<snap::drawing::Scalar>(displayScale));
}

- (id)makeViewFactoryForSnapDrawingLayerClass:(NSString *)className
{
    if (!className.length) {
        return nil;
    }
    auto snapViewManager = _snapDrawingRuntime->getViewManager();
    Valdi::StringBox name = StringFromNSString(className);
    auto factory = snapViewManager->createViewFactory(name, nullptr);
    if (!factory.get()) {
        return nil;
    }
    return [[SCValdiWrappedValue alloc] initWithValue:Valdi::Value(factory)];
}

- (void)waitUntilReadyWithCompletion:(dispatch_block_t)completion
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self waitUntilReadyWithCompletion:completion];
        });
        return;
    }

    if (_isReady) {
        completion();
        return;
    }

    [_pendingReadyBlocks addObject:completion];
}

- (Runtime *)nativeRuntime
{
    return (Runtime *)_runtime.get();
}

- (IMainThreadDispatcher *)mainThreadDispatcher
{
    return (IMainThreadDispatcher *)dynamic_cast<Valdi::IMainThreadDispatcher *>(_mainThreadDispatcher.get());
}

- (ViewManagerContext *)nativeViewManagerContext
{
    return (ViewManagerContext *)_viewManagerContext.get();
}

- (SnapDrawingRuntime *)snapDrawingRuntime
{
    return (SnapDrawingRuntime *)_snapDrawingRuntime.get();
}

- (void *)snapDrawingViewManager
{
    return _snapDrawingRuntime.get() != nullptr ? _snapDrawingRuntime->getViewManager().get() : nullptr;
}

- (void)setApplicationId:(const char *)applicationId
{
    _runtimeManager->setApplicationId(STRING_LITERAL(applicationId));
}

- (void)registerModuleFactoriesProvider:(ModuleFactoriesProviderSharedPtr*)moduleFactoriesProvider
{
    _runtimeManager->registerModuleFactoriesProvider(*((std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider> *) moduleFactoriesProvider));
}

+ (NSArray<NSString *> *)getLaunchArguments
{
    NSMutableArray *arguments = [NSMutableArray array];
    BOOL shouldIgnoreNext = NO;
    for (NSString *argument in [NSProcessInfo processInfo].arguments) {
        if (shouldIgnoreNext) {
            shouldIgnoreNext = NO;
            continue;
        }

        if ([argument hasPrefix:@"-NS"] || [argument hasPrefix:@"-Apple"]) {
            // Ignore Apple specific arguments
            shouldIgnoreNext = YES;
            continue;
        }
        [arguments addObject:argument];
    }

    return arguments;
}

@end
