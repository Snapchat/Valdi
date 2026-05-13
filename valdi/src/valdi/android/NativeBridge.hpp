#pragma once

#include <fbjni/fbjni.h>

namespace fbjni = facebook::jni;

namespace ValdiAndroid {

class JValdiFunction : public fbjni::JavaClass<JValdiFunction> {
public:
    static constexpr auto kJavaDescriptor = "Lcom/snap/valdi/callable/ValdiFunction;";
};

class NativeBridge : public fbjni::JavaClass<NativeBridge> {
public:
    static constexpr auto kJavaDescriptor = "Lcom/snapchat/client/valdi/NativeBridge;";

    static jint getBuildOptions(fbjni::alias_ref<fbjni::JClass> clazz);

    static jobject getAllRuntimeAttachedObjects(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);

    static jobject getJSRuntime(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static jstring getViewClassName(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);
    static jobject getValueForAttribute(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle, jstring attribute);
    static jobject getViewInContextForId(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle, jstring viewId);
    static jlong getRetainedViewNodeInContext(fbjni::alias_ref<fbjni::JClass> clazz,
                                              jlong contextHandle,
                                              jstring viewId,
                                              jint viewNodeId);
    static jobject getRetainedViewNodeChildren(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle, jint mode);
    static jlong getRetainedViewNodeHitTestAccessibility(
        fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle, jlong viewNodeHandle, jint x, jint y);
    static jobject getViewNodeAccessibilityHierarchyRepresentation(fbjni::alias_ref<fbjni::JClass> clazz,
                                                                   jlong runtimeHandle,
                                                                   jlong viewNodeHandle);
    static jint performViewNodeAction(
        fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle, jint action, jint param1, jint param2);
    static void waitUntilAllUpdatesCompleted(fbjni::alias_ref<fbjni::JClass> clazz,
                                             jlong contextHandle,
                                             jobject callback,
                                             jboolean shouldFlushRenderRequests);
    static void onNextLayout(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle, jobject callback);
    static void scheduleExclusiveUpdate(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle, jobject runnable);
    static void setViewInflationEnabled(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle, jboolean enabled);
    static jobject getRuntimeAttachedObject(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static jobject getRuntimeAttachedObjectFromContext(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle);
    static jobject getCurrentContext(fbjni::alias_ref<fbjni::JClass> clazz);
    static jint getNodeId(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);
    static jstring getViewNodeDebugDescription(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);
    static jobject getViewNodeBackingView(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);

    static jboolean invalidateLayout(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);
    static jboolean isViewNodeLayoutDirectionHorizontal(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);

    static jlong notifyScroll(fbjni::alias_ref<fbjni::JClass> clazz,
                              jlong runtimeHandle,
                              jlong viewNodeHandle,
                              jint eventType,
                              jint pixelDirectionDependentContentOffsetX,
                              jint pixelDirectionDependentContentOffsetY,
                              jint pixelDirectionDependentUnclampedContentOffsetX,
                              jint pixelDirectionDependentUnclampedContentOffsetY,
                              jfloat pixelDirectionDependentInvertedVelocityX,
                              jfloat pixelDirectionDependentInvertedVelocityY);

    static jlong createRuntime(fbjni::alias_ref<fbjni::JClass> clazz,
                               jlong runtimeManagerHandle,
                               jobject customResourceResolver);
    static void onRuntimeReady(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle, jlong runtimeHandle);
    static jlong createRuntimeManager(fbjni::alias_ref<fbjni::JClass> clazz,
                                      jobject mainThreadDispatcher,
                                      jobject snapDrawingFrameScheduler,
                                      jobject viewManager,
                                      jobject logger,
                                      jobject contextManager,
                                      jobject localResourceResolver,
                                      jobject assetManager,
                                      jobject keychain,
                                      jstring cacheRootDir,
                                      jstring applicationId,
                                      jfloat pointScale,
                                      jint longPressTimeoutMs,
                                      jint doubleTapTimeoutMs,
                                      jint dragTouchSlopPixels,
                                      jint touchTolerancePixels,
                                      jfloat scrollFriction,
                                      jboolean debugTouchEvents,
                                      jboolean keepDebuggerServiceOnPause,
                                      jlong maxCacheSizeInBytes,
                                      jobject javaScriptEngineType,
                                      jint jsThreadQoS,
                                      jint anrTimeoutMs);

    static jlong getViewNodePoint(fbjni::alias_ref<fbjni::JClass> clazz,
                                  jlong runtimeHandle,
                                  jlong viewNodeHandle,
                                  jint x,
                                  jint y,
                                  jint mode,
                                  jboolean fromBoundsOrigin);
    static jlong getViewNodeSize(fbjni::alias_ref<fbjni::JClass> clazz,
                                 jlong runtimeHandle,
                                 jlong viewNodeHandle,
                                 jint mode);
    static jboolean canViewNodeScroll(fbjni::alias_ref<fbjni::JClass> clazz,
                                      jlong runtimeHandle,
                                      jlong viewNodeHandle,
                                      jint x,
                                      jint y,
                                      jint direction);
    static jboolean isViewNodeScrollingOrAnimating(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle);
    static void applicationSetConfiguration(fbjni::alias_ref<fbjni::JClass> clazz,
                                            jlong runtimeManagerHandle,
                                            jfloat dynamicTypeScale);
    static void setPointScale(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle, jfloat pointScale);
    static void applicationDidResume(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);
    static void applicationIsInLowMemory(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);
    static void applicationWillPause(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);
    static void setRootView(fbjni::alias_ref<fbjni::JClass> clazz,
                            jlong runtimeHandle,
                            jlong contextHandle,
                            jobject rootView);
    static void setSnapDrawingRootView(fbjni::alias_ref<fbjni::JClass> clazz,
                                       jlong contextHandle,
                                       jlong snapDrawingRootHandle,
                                       jlong previousSnapDrawingRootHandle,
                                       jobject hostView);
    static jlong measureLayout(fbjni::alias_ref<fbjni::JClass> clazz,
                               jlong runtimeHandle,
                               jlong contextHandle,
                               jint maxWidth,
                               jint widthMode,
                               jint maxHeight,
                               jint heightMode,
                               jboolean isRTL);

    static void setLayoutSpecs(fbjni::alias_ref<fbjni::JClass> clazz,
                               jlong runtimeHandle,
                               jlong contextHandle,
                               jint width,
                               jint height,
                               jboolean isRTL);

    static void setVisibleViewport(fbjni::alias_ref<fbjni::JClass> clazz,
                                   jlong runtimeHandle,
                                   jlong contextHandle,
                                   jint x,
                                   jint y,
                                   jint width,
                                   jint height,
                                   jboolean shouldUnset);

    static void callJSFunction(fbjni::alias_ref<fbjni::JClass> clazz,
                               jlong runtimeHandle,
                               jlong contextHandle,
                               jstring jsString,
                               fbjni::alias_ref<fbjni::JArrayClass<jobject>> jsParameters);
    static void callOnJsThread(fbjni::alias_ref<fbjni::JClass> clazz,
                               jlong runtimeHandle,
                               jboolean sync,
                               jobject runnable);

    static void callSyncWithJsThread(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle, jobject runnable);

    static void enqueueLoadOperation(fbjni::alias_ref<fbjni::JClass> clazz,
                                     jlong runtimeManagerHandle,
                                     jobject runnable);
    static void flushLoadOperations(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);

    static void clearViewPools(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);

    static jobject createContext(fbjni::alias_ref<fbjni::JClass> clazz,
                                 jlong runtimeHandle,
                                 jstring componentPath,
                                 jobject viewModel,
                                 jobject componentContext,
                                 jboolean useSnapDrawing);
    static void contextOnCreate(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle);
    static void deleteNativeHandle(fbjni::alias_ref<fbjni::JClass> clazz, jlong nativeHandle);
    static void releaseNativeRef(fbjni::alias_ref<fbjni::JClass> clazz, jlong handle);
    static void discardCallback(fbjni::alias_ref<fbjni::JClass> clazz, jlong callbackHandle);
    static void deleteRuntime(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static void deleteRuntimeManager(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);
    static void destroyContext(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle, jlong contextHandle);
    static void setParentContext(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle, jlong parentContextHandle);

    static void forceBindAttributes(fbjni::alias_ref<fbjni::JClass> clazz,
                                    jlong runtimeManagerHandle,
                                    jstring className);
    static jint bindAttribute(fbjni::alias_ref<fbjni::JClass> clazz,
                              jlong bindingContextHandle,
                              jint type,
                              jstring name,
                              jboolean invalidateLayoutOnChange,
                              jobject delegate,
                              jobject compositeParts);
    static void bindScrollAttributes(fbjni::alias_ref<fbjni::JClass> clazz, jlong bindingContextHandle);
    static void bindAssetAttributes(fbjni::alias_ref<fbjni::JClass> clazz, jlong bindingContextHandle, jint outputType);
    static void setMeasureDelegate(fbjni::alias_ref<fbjni::JClass> clazz,
                                   jlong bindingContextHandle,
                                   jobject measureDelegate);
    static void setPlaceholderViewMeasureDelegate(fbjni::alias_ref<fbjni::JClass> clazz,
                                                  jlong bindingContextHandle,
                                                  jobject measureDelegate);

    static void registerAttributePreprocessor(fbjni::alias_ref<fbjni::JClass> clazz,
                                              jlong bindingContextHandle,
                                              jstring attributeName,
                                              jboolean enableCache,
                                              jobject preprocessor);

    static void setUserSession(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle, jstring userId);
    static void loadModule(fbjni::alias_ref<fbjni::JClass> clazz,
                           jlong runtimeHandle,
                           jstring moduleName,
                           fbjni::alias_ref<JValdiFunction> completion);
    static void enqueueWorkerTask(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle, jobject runnable);
    static void performCallback(fbjni::alias_ref<fbjni::JClass> clazz, jlong callbackHandle);
    static void performGcNow(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static void preloadViews(fbjni::alias_ref<fbjni::JClass> clazz,
                             jlong runtimeManagerHandle,
                             jstring className,
                             jint count);
    static void reapplyAttribute(fbjni::alias_ref<fbjni::JClass> clazz, jlong viewNodeHandle, jstring attribute);
    static void registerNativeModuleFactory(fbjni::alias_ref<fbjni::JClass> clazz,
                                            jlong runtimeHandle,
                                            jobject moduleFactory);
    static void registerTypeConverter(fbjni::alias_ref<fbjni::JClass> clazz,
                                      jlong runtimeManagerHandle,
                                      jstring className,
                                      jstring functionPath);
    static void registerModuleFactoriesProvider(fbjni::alias_ref<fbjni::JClass> clazz,
                                                jlong runtimeManagerHandle,
                                                jobject moduleFactoriesProvider);

    static void registerViewClassReplacement(fbjni::alias_ref<fbjni::JClass> clazz,
                                             jlong runtimeManagerHandle,
                                             jstring sourceClass,
                                             jstring replacementClass);

    static void prepareRenderBackend(fbjni::alias_ref<fbjni::JClass> clazz,
                                     jlong runtimeManagerHandle,
                                     jboolean useSnapDrawing);

    static void emitRuntimeManagerInitMetrics(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);

    static void emitUserSessionReadyMetrics(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);

    static void registerAssetLoader(fbjni::alias_ref<fbjni::JClass> clazz,
                                    jlong runtimeManagerHandle,
                                    jobject assetLoader,
                                    jobject supportedSchemes,
                                    jint supportedOutputTypes);
    static void unregisterAssetLoader(fbjni::alias_ref<fbjni::JClass> clazz,
                                      jlong runtimeManagerHandle,
                                      jobject assetLoader);
    static jboolean notifyAssetLoaderCompleted(
        fbjni::alias_ref<fbjni::JClass> clazz, jlong callbackHandle, jobject asset, jstring error, jboolean isImage);

    static void registerLocalViewFactory(fbjni::alias_ref<fbjni::JClass> clazz,
                                         jlong contextHandler,
                                         jlong viewFactoryHandle);
    static jlong createViewFactory(fbjni::alias_ref<fbjni::JClass> clazz,
                                   jlong runtimeManagerHandle,
                                   jstring viewClassName,
                                   jobject viewFactory,
                                   jboolean hasBindAttributes);

    static void setKeepViewAliveOnDestroy(fbjni::alias_ref<fbjni::JClass> clazz,
                                          jlong contextHandle,
                                          jboolean keepViewAliveOnDestroy);
    static void setValueForAttribute(fbjni::alias_ref<fbjni::JClass> clazz,
                                     jlong viewNodeHandle,
                                     jstring attribute,
                                     jobject value,
                                     jboolean keepAsOverride);

    static void notifyApplyAttributeFailed(fbjni::alias_ref<fbjni::JClass> clazz,
                                           jlong viewNodeHandle,
                                           jint attributeId,
                                           jstring errorMessage);

    static void setRuntimeAttachedObject(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle, jobject object);
    static void setRuntimeManagerRequestManager(fbjni::alias_ref<fbjni::JClass> clazz,
                                                jlong runtimeManagerHandle,
                                                jobject requestManager);
    static void setRuntimeManagerJsThreadQoS(fbjni::alias_ref<fbjni::JClass> clazz,
                                             jlong runtimeManagerHandle,
                                             jint jsThreadQoS);
    static void setRetainsLayoutSpecsOnInvalidateLayout(fbjni::alias_ref<fbjni::JClass> clazz,
                                                        jlong contextHandle,
                                                        jboolean retainsLayoutSpecsOnInvalidateLayout);
    static void setViewModel(fbjni::alias_ref<fbjni::JClass> clazz, jlong contextHandle, jobject viewModel);
    static void unloadAllJsModules(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static void updateResource(fbjni::alias_ref<fbjni::JClass> clazz,
                               jlong runtimeHandle,
                               jbyteArray resource,
                               jstring bundleName,
                               jstring filePathWithinBundle);
    static void valueChangedForAttribute(fbjni::alias_ref<fbjni::JClass> clazz,
                                         jlong runtimeHandle,
                                         jlong viewNodeHandle,
                                         jlong attributeNamePtr,
                                         jobject value);

    static jobject dumpMemoryStatistics(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);
    static jobject captureJavaScriptStackTraces(fbjni::alias_ref<fbjni::JClass> clazz,
                                                jlong runtimeHandle,
                                                jlong timeoutMs);
    static jobject getAllModuleHashes(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);

    static jstring dumpLogMetadata(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle, jboolean isCrashing);
    static jstring dumpLogs(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);

    static jobject getAllContexts(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static jobject getAsset(fbjni::alias_ref<fbjni::JClass> clazz,
                            jlong runtimeHandle,
                            jstring moduleName,
                            jstring pathOrUrl);
    static jobject getModuleEntry(fbjni::alias_ref<fbjni::JClass> clazz,
                                  jlong runtimeHandle,
                                  jstring moduleName,
                                  jstring path,
                                  jboolean saveToDisk);
    static jdouble getCurrentEventTime(fbjni::alias_ref<fbjni::JClass> clazz);

    static void startProfiling(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeHandle);
    static void stopProfiling(fbjni::alias_ref<fbjni::JClass> clazz,
                              jlong runtimeHandle,
                              fbjni::alias_ref<JValdiFunction> completion);

    static jobject wrapAndroidBitmap(fbjni::alias_ref<fbjni::JClass> clazz, jobject bitmap);

    static jlong getSnapDrawingRuntimeHandle(fbjni::alias_ref<fbjni::JClass> clazz, jlong runtimeManagerHandle);

    static jboolean dispatchSnapDrawingTouchEvent(fbjni::alias_ref<fbjni::JClass> clazz,
                                                  jlong snapDrawingRootHandle,
                                                  jint type,
                                                  jlong eventTimeNanos,
                                                  jint x,
                                                  jint y,
                                                  jint dx,
                                                  jint dy,
                                                  jint pointerCount,
                                                  jint actionIndex,
                                                  jintArray pointerLocations,
                                                  jobject motionEvent);
    static void snapDrawingLayout(fbjni::alias_ref<fbjni::JClass> clazz,
                                  jlong snapDrawingRootHandle,
                                  jint width,
                                  jint height);
    static jlong createSnapDrawingRoot(fbjni::alias_ref<fbjni::JClass> clazz,
                                       jlong snapDrawingRuntimeHandle,
                                       jboolean disallowSynchronousDraw);
    static void snapDrawingRegisterTypeface(fbjni::alias_ref<fbjni::JClass> clazz,
                                            jlong snapDrawingRuntimeHandle,
                                            jstring familyName,
                                            jstring fontWeight,
                                            jstring fontStyle,
                                            jboolean isFallback,
                                            jbyteArray typefaceBytes,
                                            jint typefaceFd);
    static void snapDrawingDrawInBitmap(fbjni::alias_ref<fbjni::JClass> clazz,
                                        jlong snapDrawingRootHandle,
                                        jint surfacePresenterId,
                                        jobject bitmapHandler,
                                        jboolean isOwned);
    static void snapDrawingSetSurface(fbjni::alias_ref<fbjni::JClass> clazz,
                                      jlong snapDrawingRootHandle,
                                      jint surfacePresenterId,
                                      jobject surface);
    static void snapDrawingSetSurfaceNeedsRedraw(fbjni::alias_ref<fbjni::JClass> clazz,
                                                 jlong snapDrawingRootHandle,
                                                 jint surfacePresenterId);
    static void snapDrawingOnSurfaceSizeChanged(fbjni::alias_ref<fbjni::JClass> clazz,
                                                jlong snapDrawingRootHandle,
                                                jint surfacePresenterId);
    static void snapDrawingSetSurfacePresenterManager(fbjni::alias_ref<fbjni::JClass> clazz,
                                                      jlong snapDrawingRootHandle,
                                                      jobject surfacePresenterManager);
    static void snapDrawingCallFrameCallback(fbjni::alias_ref<fbjni::JClass> clazz,
                                             jlong frameCallback,
                                             jlong frameTimeNanos);

    static void snapDrawingDrawLayerInBitmap(fbjni::alias_ref<fbjni::JClass> clazz,
                                             jlong snapDrawingLayerHandle,
                                             jobject bitmap);

    static jlong snapDrawingGetMaxRenderTargetSize(fbjni::alias_ref<fbjni::JClass>, jlong snapDrawingRuntimeHandle);

    static void registerNatives();
};

} // namespace ValdiAndroid
