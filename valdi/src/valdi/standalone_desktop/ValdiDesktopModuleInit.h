//
//  ValdiDesktopModuleInit.h
//  C API for desktop valdi modules to self-register (SnapDrawing layer classes, component context).
//  Implemented in NativeModules.m; linked when the desktop app links valdi_macos_desktop_lib.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque runtime handle (SCValdiRuntime*).
typedef void* ValdiDesktopRuntimeHandle;

/// Called once per linked module from ValdiRunDesktopModuleInits. Register SnapDrawing layer classes and/or set component context provider.
typedef void (*ValdiDesktopModuleInitFn)(ValdiDesktopRuntimeHandle runtime);

/// Register a desktop module init. Call from C++ static init so the module is wired when the app runs.
void ValdiRegisterDesktopModuleInit(ValdiDesktopModuleInitFn fn);

/// Run all registered inits. Called by Valdi AppDelegate after SCValdiNativeModulesRegister.
void ValdiRunDesktopModuleInits(ValdiDesktopRuntimeHandle runtime);

/// Component context provider: returns context dict merged into the root component. Called from Obj-C block.
typedef void* (*ValdiDesktopContextProviderFn)(ValdiDesktopRuntimeHandle runtime);

/// Set the desktop component context provider from C. Replaces any block provider.
void ValdiSetDesktopComponentContextProviderFn(ValdiDesktopContextProviderFn fn);

/// Return the SnapDrawing view manager pointer for the given runtime. Use to register layer classes.
void* ValdiRuntimeGetSnapDrawingViewManager(ValdiDesktopRuntimeHandle runtime);

/// Build a one-key context dict: { contextKey: [runtime makeViewFactoryForSnapDrawingLayerClass:layerClassName] }.
void* ValdiRuntimeMakeViewFactoryContext(ValdiDesktopRuntimeHandle runtime, const char* layerClassName, const char* contextKey);

/// Register a component context entry (key -> value). Multiple modules can register; entries are merged into the root component context.
/// Call from module init; value is the context sub-dict (e.g. from ValdiRuntimeMakeViewFactoryContext) and is retained.
void ValdiDesktopRegisterContextEntry(const char* key, void* value);

/// Generic request: invoke a handler registered for requestType (e.g. "filePicker"). resultCallback(context, result) is called when done; result is opaque (e.g. const char* pathOrNull for file picker).
typedef void (*ValdiDesktopRequestResultCallback)(void* context, void* result);
typedef void (*ValdiDesktopRequestHandler)(void* context, ValdiDesktopRequestResultCallback resultCallback);
void ValdiDesktopRegisterRequestHandler(const char* requestType, ValdiDesktopRequestHandler handler);
void ValdiDesktopInvokeRequest(const char* requestType, void* context, ValdiDesktopRequestResultCallback resultCallback);

#ifdef __cplusplus
}
#endif
