#import "valdi/ios/SCValdiJSWorker.h"
#import "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#import "valdi_core/SCNValdiCoreJSRuntime+Private.h"
#import "valdi_core/SCValdiFunctionWithBlock.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"

@implementation SCValdiJSWorker {
    SCNValdiCoreJSRuntime *_jsRuntime;
    SCNValdiCoreJSRuntimeNativeObjectsManager *_nativeObjectsManager;
}

- (instancetype)initWithWorkerRuntime:(SCNValdiCoreJSRuntime*)runtime
{
    self = [super init];

    if (self) {
        _jsRuntime = runtime;
    }

    return self;
}

- (instancetype)initWithWorkerRuntime:(SCNValdiCoreJSRuntime*)runtime
                 nativeObjectsManager:(SCNValdiCoreJSRuntimeNativeObjectsManager *)nativeObjectsManager
{
    self = [super init];

    if (self) {
        _jsRuntime = runtime;
        _nativeObjectsManager = nativeObjectsManager;
    }

    return self;
}

- (void)dealloc
{
    if (_nativeObjectsManager) {
        [_jsRuntime destroyNativeObjectsManager:_nativeObjectsManager];
    }
}

- (std::shared_ptr<Valdi::JavaScriptRuntime>)cppRuntime
{
    auto cppInterface = djinni_generated_client::valdi_core::JSRuntime::toCpp(_jsRuntime);
    auto cppRuntimeInstance = std::dynamic_pointer_cast<Valdi::JavaScriptRuntime>(cppInterface);
    SC_ASSERT(cppRuntimeInstance);
    return cppRuntimeInstance;
}

- (NSInteger)pushModuleAtPath:(NSString *)modulePath reportingErrorOnMarshaller:(SCValdiMarshallerRef)marshaller
{
    return [_jsRuntime pushModuleToMarshaller:_nativeObjectsManager path:modulePath marshallerHandle:(int64_t)marshaller];
}

- (NSInteger)pushModuleAthPath:(NSString *)modulePath inMarshaller:(SCValdiMarshallerRef)marshaller
{
    NSInteger objectIndex = [self pushModuleAtPath:modulePath reportingErrorOnMarshaller:marshaller];
    SCValdiMarshallerCheck(marshaller);
    return objectIndex;
}

- (void)preloadModuleAtPath:(NSString *)path maxDepth:(NSUInteger)maxDepth
{
    [_jsRuntime preloadModule:path maxDepth:(int32_t)maxDepth];
}

- (void)preloadModulesAtPaths:(NSArray<NSString *> *)paths maxDepth:(NSUInteger)maxDepth
{
    [_jsRuntime preloadModules:paths maxDepth:(int32_t)maxDepth];
}

- (void)warmUpValueMarshallerForObject:(id)object
{
    auto cpp = [self cppRuntime];
    if (!cpp) {
        return;
    }
    auto value = ValdiIOS::ValueFromNSObject(object);
    cpp->warmUpValueMarshaller(value);
}

- (void)addHotReloadObserver:(id<SCValdiFunction>)hotReloadObserver forModulePath:(NSString *)modulePath
{
    [_jsRuntime addModuleUnloadObserver:modulePath observer:hotReloadObserver];
}

- (void)addHotReloadObserverWithBlock:(dispatch_block_t)block forModulePath:(NSString *)modulePath
{
    [self addHotReloadObserver:[SCValdiFunctionWithBlock functionWithBlock:^BOOL(SCValdiMarshaller *marshaller) {
        block();
        return NO;
    }] forModulePath:modulePath];
}

- (void)dispatchInJsThread:(dispatch_block_t)block
{
    [self dispatchInJsThread:block attribution:@"platform.valdiJSWorker.dispatchInJsThread"];
}

- (void)dispatchInJsThreadSyncWithBlock:(dispatch_block_t)block
{
    [self dispatchInJsThreadSyncWithBlock:block
                              attribution:@"platform.valdiJSWorker.dispatchInJsThreadSync"];
}

- (void)dispatchInJsThread:(dispatch_block_t)block attribution:(NSString *)attribution
{
    auto wrappedValue = ValdiIOS::ValueFromNSObject([block copy]);
    auto cppRuntime = [self cppRuntime];
    auto attributionCpp = cppRuntime->anrDiagnosticsEnabled() ? ValdiIOS::InternedStringFromNSString(attribution)
                                                              : Valdi::StringBox();
    cppRuntime->dispatchOnJsThreadAsync(attributionCpp, [=](auto &/*jsEntry*/) {
            dispatch_block_t block = ValdiIOS::NSObjectFromValue(wrappedValue);
            block();
        });
}

- (id<SCValdiJSRuntime>)createScopedJSRuntimeWithScopeName:(NSString *)scopeName
{
    SCNValdiCoreJSRuntimeNativeObjectsManager *nativeObjectsManager = [_jsRuntime createNativeObjectsManager:scopeName];
    return [[SCValdiJSWorker alloc] initWithWorkerRuntime:_jsRuntime nativeObjectsManager:nativeObjectsManager];
}

- (void)dispose
{
    NSAssert(_nativeObjectsManager, @"Cannot dispose a scoped JSRuntime that was not created with createScopedJSRuntime");

    if (_nativeObjectsManager) {
        [_jsRuntime destroyNativeObjectsManager:_nativeObjectsManager];
    }
}

- (void)dispatchInJsThreadSyncWithBlock:(dispatch_block_t)block attribution:(NSString *)attribution
{
    auto cppRuntime = [self cppRuntime];
    auto attributionCpp = cppRuntime->anrDiagnosticsEnabled() ? ValdiIOS::InternedStringFromNSString(attribution)
                                                              : Valdi::StringBox();
    cppRuntime->dispatchSynchronouslyOnJsThread(attributionCpp, [&](auto &/*jsEntry*/) {
            block();
        });
}

@end
