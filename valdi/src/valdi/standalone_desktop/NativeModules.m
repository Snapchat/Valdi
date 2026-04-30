//
//  NativeModules.m
//  valdi-desktop-apple
//

#import "valdi/standalone_desktop/NativeModules.h"
#import "valdi/standalone_desktop/ValdiDesktopModuleInit.h"
#import "valdi/macos/SCValdiRuntime.h"

__attribute__((weak)) void SCValdiNativeModulesRegister(SCValdiRuntime* runtime) {
    (void)runtime;
    // Weak default no-op hook for desktop app to register additional native modules.
    // Overridden by the host app (e.g. composer_snap_modules/SnapModules.mm) which
    // provides a strong definition that registers native module factories.
}

static SCValdiDesktopComponentContextProvider g_desktopComponentContextProvider;
static ValdiDesktopContextProviderFn g_desktopContextProviderFn;
static NSMutableDictionary *g_registeredContextEntries = nil;
static NSMutableArray *g_desktopModuleInits = nil;

void SCValdiSetDesktopComponentContextProvider(SCValdiDesktopComponentContextProvider provider) {
    g_desktopComponentContextProvider = provider;
    g_desktopContextProviderFn = NULL;
}

id SCValdiGetDesktopComponentContext(SCValdiRuntime *runtime) {
    if (g_desktopContextProviderFn) {
        return (__bridge id)g_desktopContextProviderFn((__bridge void *)runtime);
    }
    if (g_desktopComponentContextProvider) {
        return g_desktopComponentContextProvider(runtime);
    }
    if (g_registeredContextEntries.count > 0) {
        return [g_registeredContextEntries copy];
    }
    return nil;
}

#pragma mark - ValdiDesktopModuleInit (C API)

void ValdiRegisterDesktopModuleInit(ValdiDesktopModuleInitFn fn) {
    if (fn == NULL) {
        return;
    }
    if (g_desktopModuleInits == nil) {
        g_desktopModuleInits = [NSMutableArray array];
    }
    [g_desktopModuleInits addObject:[NSValue valueWithPointer:(void *)fn]];
}

void ValdiRunDesktopModuleInits(ValdiDesktopRuntimeHandle runtime) {
    for (NSValue *box in g_desktopModuleInits ?: @[]) {
        ValdiDesktopModuleInitFn fn = (ValdiDesktopModuleInitFn)[box pointerValue];
        fn(runtime);
    }
}

void ValdiSetDesktopComponentContextProviderFn(ValdiDesktopContextProviderFn fn) {
    g_desktopContextProviderFn = fn;
    g_desktopComponentContextProvider = nil;
}

void* ValdiRuntimeGetSnapDrawingViewManager(ValdiDesktopRuntimeHandle runtime) {
    SCValdiRuntime *rt = (__bridge SCValdiRuntime *)runtime;
    return [rt snapDrawingViewManager];
}

void* ValdiRuntimeMakeViewFactoryContext(ValdiDesktopRuntimeHandle runtime, const char* layerClassName, const char* contextKey) {
    SCValdiRuntime *rt = (__bridge SCValdiRuntime *)runtime;
    NSString *className = [NSString stringWithUTF8String:layerClassName];
    id factory = [rt makeViewFactoryForSnapDrawingLayerClass:className];
    if (factory == nil) {
        return nil;
    }
    NSString *key = [NSString stringWithUTF8String:contextKey];
    return (__bridge void *)@{ key: factory };
}

void ValdiDesktopRegisterContextEntry(const char* key, void* value) {
    if (key == NULL || value == NULL) {
        return;
    }
    if (g_registeredContextEntries == nil) {
        g_registeredContextEntries = [NSMutableDictionary dictionary];
    }
    g_registeredContextEntries[[NSString stringWithUTF8String:key]] = (__bridge id)value;
}

#pragma mark - Generic request handlers

static NSMutableDictionary<NSString *, NSValue *> *g_requestHandlers = nil;

void ValdiDesktopRegisterRequestHandler(const char* requestType, ValdiDesktopRequestHandler handler) {
    if (requestType == NULL || handler == NULL) {
        return;
    }
    if (g_requestHandlers == nil) {
        g_requestHandlers = [NSMutableDictionary dictionary];
    }
    NSString *key = [NSString stringWithUTF8String:requestType];
    g_requestHandlers[key] = [NSValue valueWithPointer:(void *)handler];
}

void ValdiDesktopInvokeRequest(const char* requestType, void* context, ValdiDesktopRequestResultCallback resultCallback) {
    if (requestType == NULL || resultCallback == NULL) {
        NSLog(@"[ValdiDesktop] InvokeRequest skipped: requestType=%s resultCallback=%p", requestType ? requestType : "(null)", (void*)resultCallback);
        return;
    }
    NSString *key = [NSString stringWithUTF8String:requestType];
    NSValue *box = g_requestHandlers ? g_requestHandlers[key] : nil;
    if (box != nil) {
        ValdiDesktopRequestHandler handler = (ValdiDesktopRequestHandler)[box pointerValue];
        handler(context, resultCallback);
    } else {
        NSLog(@"[ValdiDesktop] InvokeRequest: no handler registered for requestType=\"%s\" (did the module register?)", requestType);
    }
}
