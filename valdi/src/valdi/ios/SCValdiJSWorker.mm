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

- (NSInteger)pushModuleAthPath:(NSString *)modulePath inMarshaller:(SCValdiMarshallerRef)marshaller
{
    NSInteger objectIndex = [_jsRuntime pushModuleToMarshaller:_nativeObjectsManager path:modulePath marshallerHandle:(int64_t)marshaller];
    SCValdiMarshallerCheck(marshaller);
    return objectIndex;
}

- (void)preloadModuleAtPath:(NSString *)path maxDepth:(NSUInteger)maxDepth
{
    [_jsRuntime preloadModule:path maxDepth:(int32_t)maxDepth];
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
    auto wrappedValue = ValdiIOS::ValueFromNSObject([block copy]);
    ([self cppRuntime])->dispatchOnJsThreadAsync(nullptr, [=](auto &/*jsEntry*/) {
            dispatch_block_t block = ValdiIOS::NSObjectFromValue(wrappedValue);
            block();
        });
}

- (id<SCValdiJSRuntime>)createScopedJSRuntimeWithScopeName:(NSString *)scopeName
{
    SCNValdiCoreJSRuntimeNativeObjectsManager *nativeObjectsManager = [_jsRuntime createNativeObjectsManagerWithScopeName:scopeName];
    return [[SCValdiJSWorker alloc] initWithWorkerRuntime:_jsRuntime nativeObjectsManager:nativeObjectsManager];
}

- (id<SCValdiJSRuntime>)createScopedJSRuntime
{
    return [self createScopedJSRuntimeWithScopeName:@""];
}

- (void)dispose
{
    NSAssert(_nativeObjectsManager, @"Cannot dispose a scoped JSRuntime that was not created with createScopedJSRuntime");

    if (_nativeObjectsManager) {
        [_jsRuntime destroyNativeObjectsManager:_nativeObjectsManager];
    }
}

- (void)dispatchInJsThreadSyncWithBlock:(dispatch_block_t)block
{
    ([self cppRuntime])->dispatchSynchronouslyOnJsThread([&](auto &/*jsEntry*/) {
            block();
        });
}

@end
