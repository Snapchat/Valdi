//
//  SCValdiViewNodeLoader.m
//  Valdi
//
//  Created by Simon Corsin on 12/11/17.
//  Copyright © 2017 Snap Inc. All rights reserved.
//

#import "valdi/ios/Utils/ContextUtils.h"
#import "valdi/ios/SCValdiContext+CPP.h"
#import "valdi/ios/SCValdiContext.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi/ios/CPPBindings/SCValdiMainThreadDispatcher.h"
#import "valdi_core/SCValdiView.h"
#import "valdi_core/SCValdiComponentContainerView.h"
#import "valdi/ios/SCValdiRuntime+Private.h"
#import "valdi/ios/CPPBindings/SCValdiRuntimeListener.h"
#import "valdi/ios/CPPBindings/SCValdiViewManager.h"
#import "valdi/ios/SCValdiViewModel.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"
#import "valdi_core/SCValdiViewOwner.h"
#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi/ios/CPPBindings/UIViewHolder.h"
#import "valdi/ios/SCValdiAutoDestroyingContext.h"

#import "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#import "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#import "valdi/runtime/Resources/AssetsManager.hpp"
#import "valdi_core/SCValdiRef.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCNValdiCoreModuleFactory.h"
#import "valdi_core/SCNValdiCoreModuleFactory+Private.h"
#import "valdi_core/SCNValdiCoreAsset+Private.h"
#import "valdi/runtime/Runtime.hpp"
#import <yoga/UIView+Yoga.h>
#import "SCValdiJSRuntimeImpl.h"
#import "valdi_core/SCNValdiCoreJSRuntime+Private.h"

#import "valdi/runtime/Attributes/AttributesBindingContextImpl.hpp"

#import "valdi/ios/NativeModules/Drawing/SCValdiDrawingModuleFactory.h"
#import "valdi/ios/NativeModules/SCValdiApplicationModule.h"
#import "valdi/ios/NativeModules/SCValdiDeviceModule.h"

#import "valdi/ios/Text/SCValdiFontManager.h"

#import "valdi_core/SCValdiFunctionWithBlock.h"
#import "valdi/ios/Utils/SCValdiViewFactoryImpl.h"
#import "valdi_core/cpp/Utils/Marshaller.hpp"

#import "valdi/ios/SCValdiAttributesBinder.h"

@interface SCValdiFontDataProviderImpl: NSObject <SCValdiFontDataProvider>

@end

@implementation SCValdiFontDataProviderImpl {
    Valdi::Weak<Valdi::Runtime> _runtime;
}

- (instancetype)initWithRuntime:(const Valdi::Ref<Valdi::Runtime> &)runtime
{
    self = [super init];

    if (self) {
        _runtime = runtime;
    }

    return self;
}

- (NSData *)fontDataForModuleName:(NSString *)moduleName fontPath:(NSString *)fontPath
{
    auto runtime = _runtime.lock();
    if (runtime == nullptr) {
        return nil;
    }

    auto bundle = runtime->getResourceManager().getBundle(ValdiIOS::StringFromNSString(moduleName));
    auto entry = bundle->getEntry(ValdiIOS::StringFromNSString([NSString stringWithFormat:@"res/%@.ttf", fontPath]));
    if (!entry) {
        return nil;
    }

    return ValdiIOS::NSDataFromBuffer(entry.value());
}

@end

@interface SCValdiRuntime () <SCValdiJSRuntimeProvider> {
    Valdi::SharedRuntime _runtime;
    Valdi::Ref<Valdi::ViewManagerContext> _viewManagerContext;
    __weak id<SCValdiRuntimeManagerProtocol> _runtimeManager;
    SCValdiJSRuntimeImpl *_jsRuntime;
    SCValdiApplicationModule *_applicationModule;
    BOOL _didFlushPendingMainThreadLoadOperations;
    SCValdiFontManager *_fontManager;
    SCValdiFontDataProviderImpl *_fontDataProvider;
}

@end

@implementation SCValdiRuntime

- (instancetype)initWithCppInstance:(const Valdi::SharedRuntime &)cppInstance
                 viewManagerContext:(const Valdi::Ref<Valdi::ViewManagerContext> &)viewManagerContext
                  runtimeManager:(id<SCValdiRuntimeManagerProtocol>)runtimeManager
                        fontManager:(SCValdiFontManager *)fontManager
{
    self = [super init];

    if (self) {
        auto runtimeListener = Valdi::makeShared<ValdiIOS::RuntimeListener>();
        runtimeListener->setRuntime(self);
        _runtime = cppInstance;
        _runtime->setListener(runtimeListener);
        _viewManagerContext = viewManagerContext;
        _runtimeManager = runtimeManager;
        _fontManager = fontManager;
        _fontDataProvider = [[SCValdiFontDataProviderImpl alloc] initWithRuntime:cppInstance];
        [_fontManager addFontDataProvider:_fontDataProvider];

        _deviceModule = [[SCValdiDeviceModule alloc] initWithJSQueueDispatcher:self];
        _applicationModule = [SCValdiApplicationModule new];

        [self registerNativeModuleFactory:[[SCValdiDrawingModuleFactory alloc] initWithFontManager:fontManager]];
        [self registerNativeModuleFactory:_deviceModule];
        [self registerNativeModuleFactory:_applicationModule];

        _jsRuntime = [[SCValdiJSRuntimeImpl alloc] initWithJSRuntimeProvider:self];

        _viewManagerContext->setAccessibilityEnabled(_runtime->getResourceManager().enableAccessibility());
    }

    return self;
}

- (void)dealloc
{
    [_fontManager removeFontDataProvider:_fontDataProvider];
}

- (void)applicationWillTerminate
{
    // Unlock the device module, in case it was about to be loaded but we never
    // got to finish its initialization. If this happened during termination, the
    // loadModule operation inside the device module would end up in a deadlock.
    [_deviceModule ensureDeviceModuleIsReadyForContextCreation];
}

- (void)emitInitMetrics
{
    _runtime->emitInitMetrics();
}

- (UIView<SCValdiRootViewProtocol> *)loadViewWithComponentPath:(NSString *)componentPath owner:(id)owner error:(NSError *__autoreleasing *)error
{
    return [self loadViewWithComponentPath:componentPath owner:owner viewModel:nil componentContext:nil error:error];
}

- (UIView<SCValdiRootViewProtocol> *)loadViewWithComponentPath:(NSString *)componentPath
                             owner:(id)owner
                         viewModel:(id)viewModel
                  componentContext:(id)componentContext
                             error:(NSError **)error
{
    return [[SCValdiComponentContainerView alloc] initWithComponentPath:componentPath
                                                     owner:owner
                                                 viewModel:viewModel
                                          componentContext:componentContext
                                                runtime:self
                                               ];
}

- (id<SCValdiContextProtocol>)createContextWithViewClass:(Class)viewClass
                                                   viewModel:(id)viewModel
                                            componentContext:(id)componentContext
{
    return [self createContextWithComponentPath:[viewClass componentPath] viewModel:viewModel componentContext:componentContext];
}

- (void)flushPendingMainThreadLoadOperations
{
    // If there was no root Valdi view alive when the device's userInterfaceStyle changed framework will not know about it.
    // That's why we're telling SCValdiDeviceModule to poll the system for the main UIScreen's userInterfaceStyle whenever inflating
    // a new root Valdi view.
    [_deviceModule ensureDeviceModuleIsReadyForContextCreation];
    [_applicationModule ensureApplicationModuleIsReadyForContextCreation];
}

- (void)flushPendingMainThreadLoadOperationsIfNeeded
{
    if ([NSThread isMainThread] && !_didFlushPendingMainThreadLoadOperations) {
        _didFlushPendingMainThreadLoadOperations = YES;
        [self flushPendingMainThreadLoadOperations];
    }
}

- (SCValdiContext *)doCreateContextWithComponentPath:(NSString *)componentPath
                                                      viewModel:(id)viewModel
                                               componentContext:(id)componentContext
{
    [self flushPendingMainThreadLoadOperations];

    auto cppViewModel = ValdiIOS::toLazyValueConvertible(viewModel);
    auto cppComponentContext = ValdiIOS::toLazyValueConvertible(componentContext);

    auto context = _runtime->createContext(_viewManagerContext, ValdiIOS::InternedStringFromNSString(componentPath), cppViewModel, cppComponentContext);

    SCValdiContext *valdiContext = ValdiIOS::getValdiContext(context);
    if (viewModel) {
        [valdiContext setViewModelNoUpdate:viewModel];
    }
    if (!_disableLegacyMeasureBehaviorByDefault) {
        valdiContext.useLegacyMeasureBehavior = YES;
    }

    context->onCreate();

    return valdiContext;
}

- (SCValdiContext *)doCreateContextWithComponentPath:(NSString *)componentPath
                                                  cppMarshaller:(void*)cppMarshaller
{
    [self flushPendingMainThreadLoadOperations];

    Valdi::Marshaller* marshaller = static_cast<Valdi::Marshaller*>(cppMarshaller);
    auto cppViewModel = ValdiIOS::toLazyValueConvertible(marshaller->get(0));
    auto cppComponentContext = ValdiIOS::toLazyValueConvertible(marshaller->get(1));

    auto context = _runtime->createContext(_viewManagerContext, ValdiIOS::InternedStringFromNSString(componentPath), cppViewModel, cppComponentContext);

    SCValdiContext *valdiContext = ValdiIOS::getValdiContext(context);
    if (!_disableLegacyMeasureBehaviorByDefault) {
        valdiContext.useLegacyMeasureBehavior = YES;
    }

    context->onCreate();

    return valdiContext;
}

- (id<SCValdiContextProtocol>)createContextWithComponentPath:(NSString *)componentPath
                                                      viewModel:(id)viewModel
                                               componentContext:(id)componentContext
{
    SCValdiContext *valdiContext = [self doCreateContextWithComponentPath:componentPath viewModel:viewModel componentContext:componentContext];
    return [[SCValdiAutoDestroyingContext alloc] initWithContext:valdiContext];
}

- (void)inflateView:(UIView<SCValdiRootViewProtocol> *)view
              owner:(id)owner
          viewModel:(id)viewModel
   componentContext:(id)componentContext
{
    NSAssert([NSThread isMainThread], @"Attempting to inflate a Valdi view on a non-main thread");

    SCValdiContext *valdiContext = [self doCreateContextWithComponentPath:view.componentPath viewModel:viewModel componentContext:componentContext];

    valdiContext.rootValdiViewShouldDestroyContext = YES;
    valdiContext.owner = owner;
    valdiContext.rootValdiView = view;
}

- (void)inflateView:(UIView<SCValdiRootViewProtocol> *)view
              owner:(id)owner
      cppMarshaller:(void*)cppMarshaller
{
    NSAssert([NSThread isMainThread], @"Attempting to inflate a Valdi view on a non-main thread");

    SCValdiContext *valdiContext = [self doCreateContextWithComponentPath:view.componentPath cppMarshaller:cppMarshaller];

    valdiContext.rootValdiViewShouldDestroyContext = YES;
    valdiContext.owner = owner;
    valdiContext.rootValdiView = view;
}

- (const Valdi::SharedRuntime &)cppInstance
{
    return _runtime;
}

- (id<SCValdiJSRuntime>)jsRuntime
{
    [self flushPendingMainThreadLoadOperationsIfNeeded];
    return _jsRuntime;
}

- (SCNValdiCoreJSRuntime *)getJsRuntime
{
    return djinni_generated_client::valdi_core::JSRuntime::fromCpp(Valdi::strongRef(self->_runtime->getJavaScriptRuntime()));
}

- (void)getJSRuntimeWithBlock:(void (^)(id<SCValdiJSRuntime>))block
{
    [self flushPendingMainThreadLoadOperationsIfNeeded];
    [self dispatchOnJSQueueWithBlock:^{
        block(self->_jsRuntime);
    } sync:NO];
}

- (void)executeMainThreadBatch:(dispatch_block_t)function
{
    auto batch = _runtime->getMainThreadManager().scopedBatch();
    [self dispatchOnJSQueueWithBlock:function sync:YES];
}

- (void)loadModule:(NSString *)moduleName completion:(void (^)(NSError *))completion
{
    auto wrappedCompletion = ValdiIOS::ValdiObjectFromNSObject(completion);
    auto moduleNameCpp = ValdiIOS::StringFromNSString(moduleName);

    _runtime->getResourceManager().loadModuleAsync(moduleNameCpp, Valdi::ResourceManagerLoadModuleType::Sources, [wrappedCompletion](const auto &result) {
        void (^completion)(NSError *) = ValdiIOS::NSObjectFromValdiObject(wrappedCompletion, NO);

        if (completion) {
            if (result) {
                completion(nil);
            } else {
                completion(ValdiIOS::NSErrorFromError(result.error()));
            }
        }
    });
}

- (NSString *)dumpLogMetadata
{
    auto logMetadata = _runtime->dumpLogs(true, false, false).serializeMetadata();
    return ValdiIOS::NSStringFromString(logMetadata);
}

- (NSString *)dumpLogs
{
    auto logMetadata = _runtime->dumpVerboseLogs();
    return ValdiIOS::NSStringFromSTDStringView(logMetadata);
}

- (NSArray<SCValdiContext *> *)getAllContexts
{
    auto contexts = _runtime->getContextManager().getAllContexts();
    NSMutableArray *outContexts = [NSMutableArray arrayWithCapacity:contexts.size()];

    for (const auto &context: contexts) {
        SCValdiContext *iosContext = ValdiIOS::getValdiContext(context);
        if (iosContext) {
            [outContexts addObject:iosContext];
        }
    }

    return [outContexts copy];
}

- (id<SCValdiContextProtocol>)currentContext
{
    return [SCValdiContext currentContext];
}

- (id<SCValdiRuntimeManagerProtocol>)manager {
    return _runtimeManager;
}

- (void)setIsIntegrationTestEnvironment:(BOOL)isIntegrationTestEnvironment
{
    [_applicationModule setIsIntegrationTestEnvironment:isIntegrationTestEnvironment];
}

- (void)setAllowDarkMode:(BOOL)allowDarkMode
    useScreenUserInterfaceStyleForDarkMode:(BOOL)useScreenUserInterfaceStyleForDarkMode
{
    [_deviceModule setAllowDarkMode:allowDarkMode
        useScreenUserInterfaceStyleForDarkMode:useScreenUserInterfaceStyleForDarkMode];
}

#pragma mark - Asset factories

static SCNValdiCoreAsset *getAssetWithKey(Valdi::Runtime &runtime, const Valdi::AssetKey &assetKey)
{
    const auto& assetsManager = runtime.getResourceManager().getAssetsManager();

    auto asset = assetsManager->getAsset(assetKey);

    return djinni_generated_client::valdi_core::Asset::fromCpp(asset.toShared());
}

- (SCNValdiCoreAsset *)assetWithModuleName:(NSString *)moduleName path:(NSString *)path
{
    auto moduleNameCpp = ValdiIOS::StringFromNSString(moduleName);
    auto pathCpp = ValdiIOS::StringFromNSString(path);
    auto bundle = _runtime->getResourceManager().getBundle(moduleNameCpp);
    return getAssetWithKey(*_runtime, Valdi::AssetKey(bundle, pathCpp));
}

- (SCNValdiCoreAsset *)assetWithURL:(NSString *)url
{
    auto urlCpp = ValdiIOS::StringFromNSString(url);
    return getAssetWithKey(*_runtime, Valdi::AssetKey(urlCpp));
}

#pragma mark - SCValdiJSQueueDispatcher

- (void)dispatchOnJSQueueWithBlock:(dispatch_block_t)block sync:(BOOL)sync
{
    if (sync) {
        _runtime->getJavaScriptRuntime()->dispatchSynchronouslyOnJsThread([&](auto &/*jsEntry*/) {
            block();
        });
    } else {
        auto wrappedValue = ValdiIOS::ValueFromNSObject([block copy]);

        _runtime->getJavaScriptRuntime()->dispatchOnJsThreadAsync(nullptr, [=](auto &/*jsEntry*/) {
            dispatch_block_t block = ValdiIOS::NSObjectFromValue(wrappedValue);
            block();
        });
    }
}

#pragma mark - Module factories

- (void)registerNativeModuleFactory:(id<SCNValdiCoreModuleFactory>)moduleFactory
{
    _runtime->registerNativeModuleFactory(
        djinni_generated_client::valdi_core::ModuleFactory::toCpp(moduleFactory));
}

- (void)setPerformHapticFeedbackFunctionBlock:(void (^)(NSString *))block
{
    if (!block) {
        self.deviceModule.performHapticFeedbackFunction = nil;
    } else {
        self.deviceModule.performHapticFeedbackFunction =
        [SCValdiFunctionWithBlock functionWithBlock:^BOOL(SCValdiMarshaller *marshaller) {
            NSString *typeString = @"action_sheet";

            if (SCValdiMarshallerIsString(marshaller, 0)) {
                typeString = SCValdiMarshallerGetString(marshaller, 0);
            }
            block(typeString);

            return NO;
        }];
    }
}

#pragma mark - View Factory

- (id<SCValdiViewFactory>)makeViewFactoryWithBlock:(SCValdiViewFactoryBlock)viewFactory
                                     attributesBinder:(SCValdiBindAttributesCallback)attributesBinder
                                             forClass:(__unsafe_unretained Class)viewClass
{
    auto viewClassName = ValdiIOS::StringFromNSString(NSStringFromClass(viewClass));

    auto &attributesManager = _viewManagerContext->getAttributesManager();

    auto attributes = attributesManager.getAttributesForClass(viewClassName);

    if (attributesBinder) {
        Valdi::AttributesBindingContextImpl bindingContext(attributesManager.getAttributeIds(), attributesManager.getColorPalette(), _runtime->getLogger());

        SCValdiAttributesBinder *wrapper =
            [[SCValdiAttributesBinder alloc] initWithNativeAttributesBindingContext:(SCValdiAttributesBinderNative *)&bindingContext fontManager:_fontManager];

        attributesBinder(wrapper);

        attributes = attributes->merge(
            viewClassName, bindingContext.getHandlers(), bindingContext.getDefaultDelegate(), bindingContext.getMeasureDelegate(), bindingContext.scrollAttributesBound(), true
        );
    }

    auto viewFactoryCpp = ValdiIOS::createLocalViewFactory(viewFactory, viewClassName, _viewManagerContext->getViewManager(), attributes);
    return [[SCValdiViewFactoryImpl alloc] initWithValue:Valdi::Value(viewFactoryCpp)];
}

- (NSDictionary<NSString*, NSString*>*)getAllModuleHashes
{
    NSMutableDictionary<NSString*, NSString*> *moduleHashes = [NSMutableDictionary dictionary];
    auto bundles = _runtime->getResourceManager().getAllInitializedBundles();
    for (const auto &[bundleName, bundle] : bundles) {
        auto hash = bundle->getHash();
        if (hash) {
            NSString *bundleNameNSString = ValdiIOS::NSStringFromString(bundleName);
            NSString *hashNSString = ValdiIOS::NSStringFromString(hash.value());
            moduleHashes[bundleNameNSString] = hashNSString;
        }
    }
    return [moduleHashes copy];
}

@end
