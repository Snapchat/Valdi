//
//  SCValdiRuntimeManager.m
//  Valdi
//
//  Created by Simon Corsin on 1/8/19.
//

#import "valdi/ios/SCValdiRuntimeManager.h"

#import "valdi/jsbridge/JavaScriptBridge.hpp"

#import "valdi/ios/CPPBindings/SCValdiLoggerBridge.h"
#import "valdi/ios/CPPBindings/SCValdiMainThreadDispatcher.h"
#import "valdi/ios/SCValdiRuntime+Private.h"
#import "valdi/ios/CPPBindings/SCValdiViewManager.h"
#import "valdi/ios/NativeModules/SCValdiDeviceModule.h"
#import "valdi/ios/Utils/SCValdiCapturedJSStacktrace.h"
#import "valdi_core/cpp/Utils/StringCache.hpp"
#import "valdi_core/SCValdiCacheUtils.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/cpp/Constants.hpp"

#import "valdi/ios/Text/SCValdiFontManager.h"
#import "valdi/ios/Text/NSAttributedString+Valdi.h"

#import "valdi_core/SCValdiView.h"
#import "valdi_core/SCValdiImageView.h"

#import "valdi/ios/SCValdiDefaultImageLoader.h"
#import "valdi/ios/SCValdiAssetLoader.h"

#import "valdi/ios/Resources/SCValdiResourceLoader.h"
#import "valdi/runtime/Resources/DiskCacheImpl.hpp"
#import "valdi_core/SCValdiValueUtils.h"
#import "valdi_core/SCNValdiCoreHTTPRequestManager+Private.h"
#import "valdi_core/SCNValdiCoreModuleFactoriesProvider+Private.h"
#import "valdi/SCNValdiKeychain+Private.h"
#import "valdi_core/SCNValdiCoreJSRuntime+Private.h"
#import "valdi/ios/SCValdiDefaultHTTPRequestManager.h"
#import "valdi/ios/SCValdiKeychainStore.h"
#import "valdi/ios/SCValdiUserDefaultsStore.h"
#import "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"
#import "valdi/runtime/RuntimeManager.hpp"
#import "valdi/runtime/Resources/AssetLoaderManager.hpp"

#import "valdi/ios/SnapDrawing/SCValdiSnapDrawingRuntime.h"
#import "valdi/snap_drawing/Modules/SnapDrawingModuleFactoriesProvider.hpp"
#import "valdi/snap_drawing/Runtime.hpp"

#import "valdi/ios/SCValdiJSWorker.h"

#include <chrono>
#import <memory>
#import <utils/debugging/Assert.hpp>

Valdi::StringBox resolveDocumentsDirectory() {
    NSString *documentsDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    if (!documentsDir) {
        documentsDir = NSTemporaryDirectory();
        SCLogValdiWarning(@"Could not resolve documents directory. Using temporary directory instead.");
    }


    NSURL *directoryURL = [[NSURL fileURLWithPath:documentsDir] URLByAppendingPathComponent:@"Valdi"];
    NSString *directoryPath = directoryURL.path;

    if (![NSFileManager.defaultManager fileExistsAtPath:directoryPath]) {
        [NSFileManager.defaultManager createDirectoryAtPath:directoryPath withIntermediateDirectories:YES attributes:nil error:nil];
    }
    [directoryURL setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:nil];

    return ValdiIOS::StringFromNSString(directoryURL.path);
}

static id<SCNValdiKeychain> SCValdiCreateKeychainStore() {
    #if TARGET_IPHONE_SIMULATOR
            return [SCValdiUserDefaultsStore new];
    #else
            return [SCValdiKeychainStore new];
    #endif
}

static void updateRuntimeManagersArray(void (^callback)(NSMutableArray<NSValue *> *runtimeManagers)) {
    static dispatch_once_t onceToken;
    static NSMutableArray<NSValue *> *kAllRuntimeManagers;
    dispatch_once(&onceToken, ^{
        kAllRuntimeManagers = [NSMutableArray array];
    });

    @synchronized (kAllRuntimeManagers) {
        callback(kAllRuntimeManagers);
    }
}

@interface SCValdiRuntimeManager () <SCValdiMainRuntimeProvider>
@end

@implementation SCValdiRuntimeManager {
    Valdi::Ref<Valdi::RuntimeManager> _cppInstance;
    SCValdiRuntime *_mainRuntime;
    SCValdiConfiguration *_configuration;

    Valdi::Shared<ValdiIOS::ViewManager> _viewManagerBridge;
    std::vector<std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider>> _pendingModuleFactoriesProviders;
    std::vector<Valdi::RegisteredTypeConverter> _pendingTypeConverters;
    Valdi::Ref<Valdi::DiskCacheImpl> _diskCache;
    Valdi::Ref<Valdi::ViewManagerContext> _viewManagerContext;

    SCValdiDefaultHTTPRequestManager *_defaultHTTPRequestManager;
    NSArray<id<SCValdiImageLoader>> *_previousImageLoaders;
    NSArray<id<SCValdiVideoLoader>> *_previousVideoLoaders;
    SCValdiSnapDrawingRuntime *_snapDrawingRuntime;
    SCValdiFontManager *_fontManager;

    BOOL _applicationIsTerminating;
    BOOL _forceTornDown;
    BOOL _referenceTrackingEnabled;
    NSValue *_weakBox;

    NSMutableArray<SCValdiRuntimeCreatedCallback> *_runtimeCreatedCallbacks;

    NSMapTable<NSString*, id> *_workerExecutorCache;
}

- (instancetype)init
{
    self = [super init];

    if (self) {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_applicationWillTerminate)
                                                     name:UIApplicationWillTerminateNotification
                                                   object:nil];

        _weakBox = [NSValue valueWithNonretainedObject:self];

        updateRuntimeManagersArray(^(NSMutableArray<NSValue *> *runtimeManagers) {
            [runtimeManagers addObject:self->_weakBox];
        });

        _runtimeCreatedCallbacks = [NSMutableArray array];

        _workerExecutorCache = [NSMapTable strongToWeakObjectsMapTable];
    }

    return self;
}

- (void)dealloc
{
    updateRuntimeManagersArray(^(NSMutableArray<NSValue *> *runtimeManagers) {
        [runtimeManagers removeObject:self->_weakBox];
    });

    Valdi::Ref<Valdi::RuntimeManager> runtimeManager;
    @synchronized (self) {
        _mainRuntime = nil;
        runtimeManager = std::move(_cppInstance);
    }
    if (runtimeManager != nullptr) {
        runtimeManager->fullTeardown();
    }
}

- (void)_initializeIfNeeded
{
    if (_cppInstance == nullptr) {

        if ([NSThread isMainThread]) {
            SCValdiRuntimeManagerInitMainThreadObjects();
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                SCValdiRuntimeManagerInitMainThreadObjects();
            });
        }

        _defaultHTTPRequestManager = [SCValdiDefaultHTTPRequestManager new];
        _fontManager = [SCValdiFontManager new];

        _viewManagerBridge = Valdi::makeShared<ValdiIOS::ViewManager>(_fontManager);

        auto logger = Valdi::makeShared<ValdiIOS::Logger>();
        auto mainThreadDispatcher = Valdi::makeShared<ValdiIOS::MainThreadDispatcher>();

        _diskCache = Valdi::makeShared<Valdi::DiskCacheImpl>(resolveDocumentsDirectory());

        id<SCNValdiKeychain> keychainStore = SCValdiCreateKeychainStore();

        _cppInstance = Valdi::makeShared<Valdi::RuntimeManager>(mainThreadDispatcher,
                                                                     [self _javaScriptBridge],
                                                                     _diskCache,
                                                                     djinni_generated_client::valdi::Keychain::toCpp(keychainStore),
                                                                     _viewManagerBridge,
                                                                     Valdi::PlatformTypeIOS,
                                                                     Valdi::ThreadQoSClassMax,
                                                                     logger,
                                                                     /* enableDebuggerService */ true,
                                                                     /* disableHotReloader */ false,
                                                                     /* isStandalone */ false);
        _cppInstance->postInit();
        NSString *bundleIdentifier = [NSBundle mainBundle].bundleIdentifier;
        _cppInstance->setApplicationId(ValdiIOS::StringFromNSString(bundleIdentifier));
        if ([NSThread isMainThread]) {
            _cppInstance->getMainThreadManager().markCurrentThreadIsMainThread();
        }

        [self _registerImageLoader:[SCValdiDefaultImageLoader new]];
        _cppInstance->registerBytesAssetLoader(_diskCache->scopedCache(Valdi::Path("downloaded_assets"), true));

        _viewManagerContext = _cppInstance->createViewManagerContext(*_viewManagerBridge, true);

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_didReceiveMemoryWarning)
                                                     name:UIApplicationDidReceiveMemoryWarningNotification
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_willEnterForeground)
                                                     name:UIApplicationWillEnterForegroundNotification
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_didEnterBackground)
                                                     name:UIApplicationDidEnterBackgroundNotification
                                                   object:nil];
        if ([UIApplication sharedApplication].applicationState != UIApplicationStateBackground) {
            _cppInstance->applicationDidResume();
        }

        [self registerViewClassReplacement:[UIImageView class] withViewClass:[SCValdiImageView class]];

        auto imageLoaderFactory = Valdi::makeShared<ValdiIOS::ImageAssetLoaderFactoryIOS>(_cppInstance->getWorkerQueue());
        _cppInstance->getAssetLoaderManager()->registerAssetLoaderFactory(imageLoaderFactory);

        [self _applyConfiguration];

        auto pendingModuleFactoriesProviders = std::move(_pendingModuleFactoriesProviders);
        auto pendingTypeConverters = std::move(_pendingTypeConverters);
        for (const auto &moduleFactoryProvider: pendingModuleFactoriesProviders) {
            _cppInstance->registerModuleFactoriesProvider(moduleFactoryProvider);
        }
#ifdef SNAP_DRAWING_ENABLED
        __weak SCValdiRuntimeManager *weakSelf = self;
        _cppInstance->registerModuleFactoriesProvider(Valdi::makeShared<snap::drawing::SnapDrawingModuleFactoriesProvider>([weakSelf]() -> Valdi::Ref<snap::drawing::Runtime> {
            id<SCSnapDrawingRuntime> snapDrawingRuntime = [weakSelf snapDrawingRuntime];
            return Valdi::unsafeBridge<snap::drawing::Runtime>([snapDrawingRuntime handle]);
        }).toShared());
    #endif

        for (const auto &typeConverter: pendingTypeConverters) {
            _cppInstance->registerTypeConverter(typeConverter.className, typeConverter.functionPath);
        }

        _cppInstance->emitInitMetrics();
    }
}

- (void)registerViewClassReplacement:(Class)viewClassToReplace withViewClass:(Class)replacementViewClass
{
    @synchronized (self) {
        [self _initializeIfNeeded];
        _viewManagerContext->registerViewClassReplacement(ValdiIOS::StringFromNSString(NSStringFromClass(viewClassToReplace)), ValdiIOS::StringFromNSString(NSStringFromClass(replacementViewClass)));
    }
}

- (void)setDebugMessageDisplayer:(id<SCValdiDebugMessageDisplayer>)debugMessageDisplayer
{
    [self updateConfiguration:^(SCValdiConfiguration *configuration) {
        configuration.debugMessageDisplayer = debugMessageDisplayer;
    }];
}

- (void)setCurrentUsername:(NSString *)currentUsername
{
    _currentUsername = [currentUsername copy];
}

// Remove once iOS uses the new method
- (void)setUserSessionWithUserId:(NSString *)userId userIv:(NSString *)userIv
{
    [self setUserSessionWithUserId:userId];
}

- (void)setUserSessionWithUserId:(NSString *)userId
{
    [self updateConfiguration:^(SCValdiConfiguration *configuration) {
        configuration.userId = userId;
    }];
}

- (SCValdiConfiguration *)_getOrCreateConfiguration
{
    if (_configuration) {
        return _configuration;
    }

    _configuration = [SCValdiConfiguration new];

    return _configuration;
}

- (void)_applyConfiguration
{
    if (_cppInstance == nullptr) {
        return;
    }

    SCValdiConfiguration *configuration = [self _getOrCreateConfiguration];

    _referenceTrackingEnabled = configuration.enableReferenceTracking;
    BOOL useScreenUserInterfaceStyleForDarkMode = !configuration.useViewControllerBasedUserInterfaceStyleForDarkMode;
    [_mainRuntime setAllowDarkMode:configuration.allowDarkMode
        useScreenUserInterfaceStyleForDarkMode:useScreenUserInterfaceStyleForDarkMode];
    [_mainRuntime setIsIntegrationTestEnvironment:configuration.isTestEnvironment];
    _mainRuntime.disableLegacyMeasureBehaviorByDefault = configuration.disableLegacyMeasureBehaviorByDefault;
    [_mainRuntime setPerformHapticFeedbackFunctionBlock:configuration.performHapticFeedbackBlock];

    _viewManagerBridge->setDebugMessageDisplayer(configuration.debugMessageDisplayer);
    _viewManagerBridge->setExceptionReporter(configuration.exceptionReporter);

    id<SCNValdiCoreHTTPRequestManager> requestManager = configuration.requestManager ?: _defaultHTTPRequestManager;
    _cppInstance->setRequestManager(djinni_generated_client::valdi_core::HTTPRequestManager::toCpp(requestManager));

    if (configuration.userId.length) {
        _cppInstance->setUserSession(ValdiIOS::StringFromNSString(configuration.userId));
    } else {
        _cppInstance->setUserSession(Valdi::StringBox());
    }

    if (configuration.disableGcStackUsageDetection) {
        Valdi::forceRetainJsObjects = true;
    }

    if (configuration.anrTimeoutMs > 0) {
        _cppInstance->getANRDetector()->start(std::chrono::milliseconds(configuration.anrTimeoutMs));
    } else {
        _cppInstance->getANRDetector()->stop();
    }

    [_fontManager setFontLoader:configuration.fontLoader];

    NSArray<id<SCValdiImageLoader>> *newImageLoaders = [configuration.imageLoaders copy];

    for (id<SCValdiImageLoader> imageLoader in _previousImageLoaders) {
        if (![newImageLoaders containsObject:imageLoader]) {
            [self _unregisterImageLoader:imageLoader];
        }
    }

    for (id<SCValdiImageLoader> imageLoader in newImageLoaders) {
        if (![_previousImageLoaders containsObject:imageLoader]) {
            [self _registerImageLoader:imageLoader];
        }
    }

    _previousImageLoaders = newImageLoaders;

    NSArray<id<SCValdiVideoLoader>> *newVideoLoaders = [configuration.videoLoaders copy];

    for (id<SCValdiVideoLoader> videoLoader in _previousVideoLoaders) {
        if (![newVideoLoaders containsObject:videoLoader]) {
            [self _unregisterVideoLoader:videoLoader];
        }
    }

    for (id<SCValdiVideoLoader> videoLoader in newVideoLoaders) {
        if (![_previousVideoLoaders containsObject:videoLoader]) {
            [self _registerVideoLoader:videoLoader];
        }
    }

    _previousVideoLoaders = newVideoLoaders;
}

- (void)_registerImageLoader:(id<SCValdiImageLoader>)imageLoader
{
    if ([imageLoader respondsToSelector:@selector(loadImageWithRequestPayload:parameters:completion:)]) {
        auto assetLoader = Valdi::makeShared<ValdiIOS::ImageAssetLoaderIOS>(imageLoader, _cppInstance->getWorkerQueue());
        _cppInstance->getAssetLoaderManager()->registerAssetLoader(assetLoader);
    }

    if ([imageLoader respondsToSelector:@selector(loadBytesWithRequestPayload:completion:)]) {
        auto assetLoader = Valdi::makeShared<ValdiIOS::BytesAssetLoaderIOS>(imageLoader);
        _cppInstance->getAssetLoaderManager()->registerAssetLoader(assetLoader);
    }
}

- (void)_registerVideoLoader:(id<SCValdiVideoLoader>)videoLoader
{
    auto assetLoader = Valdi::makeShared<ValdiIOS::VideoAssetLoaderIOS>(videoLoader, _cppInstance->getWorkerQueue());
    _cppInstance->getAssetLoaderManager()->registerAssetLoader(assetLoader);
}

- (void)_unregisterImageLoader:(id<SCValdiImageLoader>)imageLoader
{
    auto assetLoaders = _cppInstance->getAssetLoaderManager()->getAllAssetLoaders();

    for (const auto &assetLoaderCpp: assetLoaders) {
        auto assetLoader = Valdi::castOrNull<ValdiIOS::AssetLoaderIOS>(assetLoaderCpp);
        if (assetLoader != nullptr && assetLoader->getAssetLoader() == imageLoader) {
            _cppInstance->getAssetLoaderManager()->unregisterAssetLoader(assetLoaderCpp);
        }
    }
}

- (void)_unregisterVideoLoader:(id<SCValdiVideoLoader>)videoLoader
{
    auto assetLoaders = _cppInstance->getAssetLoaderManager()->getAllAssetLoaders();

    for (const auto &assetLoaderCpp: assetLoaders) {
        auto assetLoader = Valdi::castOrNull<ValdiIOS::VideoAssetLoaderIOS>(assetLoaderCpp);
        if (assetLoader != nullptr && assetLoader->getVideoLoader() == videoLoader) {
            _cppInstance->getAssetLoaderManager()->unregisterAssetLoader(assetLoaderCpp);
        }
    }
}

- (void)updateConfiguration:(void (^)(SCValdiConfiguration *))updateBlock
{
    @synchronized (self) {
        SCValdiConfiguration *configuration = [self _getOrCreateConfiguration];
        updateBlock(configuration);
        [self _applyConfiguration];
    }
}

- (BOOL)referenceTrackingEnabled
{
    return _referenceTrackingEnabled;
}

- (SCValdiRuntime *)mainRuntime
{
    @synchronized (self) {
        if (!_mainRuntime) {
            SCValdiConfiguration *configuration = [self _getOrCreateConfiguration];
            _mainRuntime = [self createRuntimeWithCustomModuleProvider:configuration.customModuleProvider];
            [self _applyConfiguration];

            for (SCValdiRuntimeCreatedCallback callback in _runtimeCreatedCallbacks) {
                callback(_mainRuntime);
            }
            [_runtimeCreatedCallbacks removeAllObjects];

            [_mainRuntime emitInitMetrics];
        }
    }

    return _mainRuntime;
}

- (id<SCValdiRuntimeProtocol>)provideMainRuntime
{
    return self.mainRuntime;
}

- (void)setRequestManager:(id<SCNValdiCoreHTTPRequestManager>)requestManager
{
    [self updateConfiguration:^(SCValdiConfiguration *configuration) {
        configuration.requestManager = requestManager;
    }];
}

- (void)unloadAllJsModules
{
    @synchronized (self) {
        if (_cppInstance == nullptr) {
            return;
        }

        for (auto &runtime : _cppInstance->getAllRuntimes()) {
            runtime->getJavaScriptRuntime()->unloadAllModules();
        }
    }
}

- (nullable SCValdiRuntime *)createRuntimeWithCustomModuleProvider:(id<SCValdiCustomModuleProvider>)customModuleProvider
{
    if (_applicationIsTerminating || _forceTornDown) {
        return nil;
    }
    [self _initializeIfNeeded];

    // Custom module provider takes precedence over the one in the configuration
    if (customModuleProvider == nil) {
        customModuleProvider = [self _getOrCreateConfiguration].customModuleProvider;
    }

    auto runtime =
    _cppInstance->createRuntime(Valdi::makeShared<ValdiIOS::ResourceLoader>(customModuleProvider), static_cast<double>(_viewManagerContext->getViewManager().getPointScale()));
    _cppInstance->emitXpatCreateRuntimeMetrics();

    SCValdiRuntime *objcRuntime = [[SCValdiRuntime alloc] initWithCppInstance:runtime viewManagerContext:_viewManagerContext runtimeManager:self fontManager:_fontManager];

    _cppInstance->emitIosRuntimeCreateMetrics();

    _cppInstance->attachHotReloader(runtime);

    return objcRuntime;
}

- (void)clearViewPools
{
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            _cppInstance->clearViewPools();
        }
    }
}

- (void)preloadViewsOfClass:(Class)viewClass count:(NSInteger)count
{
    @synchronized (self) {
        if (_viewManagerContext != nullptr) {
            NSString *viewClassName = NSStringFromClass(viewClass);
            auto viewClassNameCpp = ValdiIOS::InternedStringFromNSString(viewClassName);
            _viewManagerContext->preloadViews(viewClassNameCpp, static_cast<size_t>(count));
        }
    }
}

- (nullable id<SCSnapDrawingRuntime>)snapDrawingRuntime
{
#ifdef SNAP_DRAWING_ENABLED
    @synchronized (self) {
        [self _initializeIfNeeded];

        if (_snapDrawingRuntime == nil) {
            CGSize screenSize = [UIScreen mainScreen].nativeBounds.size;
            uint64_t maxCacheSizeInBytes = (uint64_t)(2 * screenSize.width * screenSize.height);

            _snapDrawingRuntime = [[SCValdiSnapDrawingRuntime alloc]
                                   initWithDiskCache:_diskCache.get()
                                   hostViewManager:_viewManagerBridge.get()
                                   logger:&_cppInstance->getLogger()
                                   workerQueue:_cppInstance->getWorkerQueue().get()
                                   assetLoaderManager:_cppInstance->getAssetLoaderManager().get()
                                   maxCacheSizeInBytes:maxCacheSizeInBytes];
        }

        return _snapDrawingRuntime;
    }
#else
    return nil;
#endif
}

- (void)_didReceiveMemoryWarning
{
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            _cppInstance->applicationIsLowInMemory();
        }
#ifdef SNAP_DRAWING_ENABLED
        [_snapDrawingRuntime onApplicationIsInLowMemory];
#endif
    }
}

- (void)_willEnterForeground
{
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            _cppInstance->applicationDidResume();
        }
#ifdef SNAP_DRAWING_ENABLED
        [_snapDrawingRuntime onApplicationEnteringForeground];
#endif
    }
}

- (void)_didEnterBackground
{
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            _cppInstance->applicationWillPause();
        }
#ifdef SNAP_DRAWING_ENABLED
        [_snapDrawingRuntime onApplicationEnteringBackground];
#endif
    }
}

- (void)_applicationWillTerminate
{
    @synchronized (self) {
        _applicationIsTerminating = YES;
        [_mainRuntime applicationWillTerminate];
        if (_cppInstance != nullptr) {
            _cppInstance->applicationWillTerminate();
        }
    }
}

- (void)registerMainRuntimeCreatedCallback:(SCValdiRuntimeCreatedCallback)callback {
    @synchronized (self) {
        if (_mainRuntime) {
            callback(_mainRuntime);
        } else {
            [_runtimeCreatedCallbacks addObject:[callback copy]];
        }
    }
}

- (void)registerModuleFactoriesProvider:(id<SCNValdiCoreModuleFactoriesProvider>)moduleFactoriesProvider
{
    auto moduleFactoriesProviderCpp = djinni_generated_client::valdi_core::ModuleFactoriesProvider::toCpp(moduleFactoriesProvider);
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            _cppInstance->registerModuleFactoriesProvider(moduleFactoriesProviderCpp);
        } else {
            _pendingModuleFactoriesProviders.emplace_back(std::move(moduleFactoriesProviderCpp));
        }
    }
}

- (void)registerTypeConverterForClass:(Class)cls converterFunctionPath:(NSString *)converterFunctionPath
{
    [self registerTypeConverterForClassName:NSStringFromClass(cls) converterFunctionPath:converterFunctionPath];
}

- (void)registerTypeConverterForClassName:(NSString*)clsName converterFunctionPath:(NSString *)converterFunctionPath
{
    Valdi::RegisteredTypeConverter typeConverter;
    typeConverter.className = ValdiIOS::InternedStringFromNSString(clsName);
    typeConverter.functionPath = ValdiIOS::InternedStringFromNSString(converterFunctionPath);

    @synchronized (self) {
        if (_cppInstance != nullptr) {
            _cppInstance->registerTypeConverter(typeConverter.className, typeConverter.functionPath);
        } else {
            _pendingTypeConverters.emplace_back(std::move(typeConverter));
        }
    }
}

- (Valdi::IJavaScriptBridge*)_javaScriptBridge
{
    SCValdiConfiguration *configuration = [self _getOrCreateConfiguration];
    // The cast is safe because the enum values are the same
    return Valdi::JavaScriptBridge::get(static_cast<snap::valdi_core::JavaScriptEngineType>(configuration.javaScriptEngineType));
}

static SCValdiJSThreadStatus toObjCThreadStatus(Valdi::JavaScriptCapturedStacktrace::Status status) {
    switch (status) {
        case Valdi::JavaScriptCapturedStacktrace::Status::NOT_RUNNING:
            return SCValdiJSThreadStatusNotRunning;
        case Valdi::JavaScriptCapturedStacktrace::Status::RUNNING:
            return SCValdiJSThreadStatusRunning;
        case Valdi::JavaScriptCapturedStacktrace::Status::TIMED_OUT:
            return SCValdiJSThreadStatusTimedOut;
    }
}

static SCValdiCapturedJSStacktrace *toObjCStacktrace(const Valdi::JavaScriptCapturedStacktrace &capturedStacktrace) {
    return [[SCValdiCapturedJSStacktrace alloc] initWithStackTrace:ValdiIOS::NSStringFromString(capturedStacktrace.getStackTrace())
                                                         threadStatus:toObjCThreadStatus(capturedStacktrace.getStatus())];
}

- (nullable NSArray<SCValdiCapturedJSStacktrace *> *)captureStackTracesWithTimeoutMs:(NSUInteger)timeoutMs
{
    NSMutableArray<SCValdiCapturedJSStacktrace *> *capturedJSStacktraces = [NSMutableArray array];
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            for (auto &runtime : _cppInstance->getAllRuntimes()) {
                auto capturedStacktraces = runtime->getJavaScriptRuntime()->captureStackTraces(std::chrono::milliseconds(timeoutMs));
                for (const auto &stackTrace: capturedStacktraces) {
                    [capturedJSStacktraces addObject:toObjCStacktrace(stackTrace)];
                }
            }
        }
    }

    return [capturedJSStacktraces copy];
}

- (SCValdiMemoryStatistics)dumpMemoryStatistics
{
    SCValdiMemoryStatistics result = {0, 0};
    @synchronized (self) {
        if (_cppInstance != nullptr) {
            auto stats = _cppInstance->dumpMemoryStatistics();
            result.memoryUsageBytes = static_cast<int64_t>(stats.memoryUsageBytes);
            result.objectsCount = static_cast<int64_t>(stats.objectsCount);
        }
    }
    return result;
}

- (void *)cppInstance
{
    @synchronized (self) {
        [self _initializeIfNeeded];
        return Valdi::unsafeBridgeCast(_cppInstance.get());
    }
}

- (void)getWorkerOnExecutor:(NSString*)executor block:(void (^)(id<SCValdiJSRuntime>))block
{
    id<SCValdiJSRuntime> existingWorker = nil;
    @synchronized(_workerExecutorCache) {
        existingWorker = [_workerExecutorCache objectForKey:executor];
    }
    if (existingWorker != nil) {
        [existingWorker dispatchInJsThread:^() { block(existingWorker); }];
    } else {
        auto runtimeInstance = self.mainRuntime.cppInstance;
        SC_ASSERT(runtimeInstance != nullptr);
        auto runtime = runtimeInstance->getJavaScriptRuntime();
        SC_ASSERT(runtime);
        auto workerRuntime = djinni_generated_client::valdi_core::JSRuntime::fromCpp(runtime->createWorker());
        SC_ASSERT(workerRuntime);
        auto newWorker = [[SCValdiJSWorker alloc] initWithWorkerRuntime:workerRuntime];
        SC_ASSERT(newWorker);
        @synchronized(_workerExecutorCache) {
            [_workerExecutorCache setObject:newWorker forKey:executor];
        }
        [newWorker dispatchInJsThread:^() { block(newWorker); }];
    }
}

+ (NSArray<SCValdiRuntimeManager *> *)allRuntimeManagers
{
    NSMutableArray<SCValdiRuntimeManager *> *resolvedRuntimeManagers = [NSMutableArray array];
    updateRuntimeManagersArray(^(NSMutableArray<NSValue *> *runtimeManagers) {
        for (NSValue *weakBox in runtimeManagers) {
            SCValdiRuntimeManager *instance = [weakBox nonretainedObjectValue];
            if (instance) {
                [resolvedRuntimeManagers addObject:instance];
            }
        }
    });

    return [resolvedRuntimeManagers copy];
}

#pragma mark - Static helpers

static void SCValdiRuntimeManagerInitMainThreadObjects(void) {
    // First call to +[NSAttributedString defaultParagraphStyle] needs to be on the main thread, since
    // it initializes a UILabel instance.
    [NSAttributedString defaultParagraphStyle];
}

@end
