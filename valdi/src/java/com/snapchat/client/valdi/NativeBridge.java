package com.snapchat.client.valdi;

import com.snap.valdi.callable.ValdiFunction;

public class NativeBridge {
    public static native int getBuildOptions();
    public static native long createRuntimeManager(Object mainThreadDispatcher,
                                                      Object snapDrawingFrameScheduler,
                                                      Object viewManager,
                                                      Object logger,
                                                      Object contextManager,
                                                      Object resourceResolver,
                                                      Object assetManager,
                                                      Object keychain,
                                                      String cacheRootDir,
                                                      String applicationId,
                                                      float pointScale,
                                                      int longPressTimeoutMs,
                                                      int doubleTapTimeoutMs,
                                                      int dragTouchSlopPixels,
                                                      int touchTolerancePixels,
                                                      float scrollFriction,
                                                      boolean debugTouchEvents,
                                                      boolean keepDebuggerServiceOnPause,
                                                      long maxCacheSizeInBytes,
                                                      Object javaScriptEngineType,
                                                      int javascriptThreadQoS,
                                                      int anrTimeoutMs);

    public static native void deleteRuntimeManager(long runtimeManagerHandle);

    public static native void setRuntimeManagerRequestManager(long runtimeManagerHandle, Object requestManager);
    public static native void setRuntimeManagerJsThreadQoS(long runtimeManagerHandle, int qosClass);

    public static native long createRuntime(long runtimeManagerHandle, Object customResourceResolver);
    public static native void onRuntimeReady(long runtimeManagerHandle, long runtimeHandle);

    public static native void deleteRuntime(long runtimeHandle);

    public static native void registerViewClassReplacement(long runtimeManagerHandle, String sourceClass, String newClass);

    public static native void prepareRenderBackend(long runtimeManagerHandle, boolean useSnapDrawing);

    public static native void emitRuntimeManagerInitMetrics(long runtimeManagerHandle);

    public static native void emitUserSessionReadyMetrics(long runtimeManagerHandle);

    public static native void registerAssetLoader(long runtimeManagerHandle,
                                                  Object assetLoader,
                                                  Object supportedSchemes,
                                                  int supportedOutputTypes);
    public static native void unregisterAssetLoader(long runtimeManagerHandle,
                                                    Object assetLoader);
    public static native boolean notifyAssetLoaderCompleted(long callbackHandle,
                                                         Object image,
                                                         String error,
                                                         boolean isImage);

    public static native Object createContext(long runtimeHandle, String componentPath, Object viewModel, Object componentContext, boolean useSnapDrawing);
    public static native void contextOnCreate(long contextHandle);

    public static native void destroyContext(long runtimeHandle, long contextHandle);

    public static native void setRootView(long runtimeHandle, long contextHandle, Object rootView);
    public static native void setSnapDrawingRootView(long contextHandle, long skiaRootHandle, long previousSkiaRootHandle, Object hostView);

    public static native void updateResource(long runtimeHandle, byte[] resource, String bundleName, String filePathWithinBundle);

    public static native void setRetainsLayoutSpecsOnInvalidateLayout(long contextHandle, boolean retainsLayoutSpecsOnInvalidateLayout);

    public static native void setViewModel(long contextHandle, Object viewModel);

    public static native void setParentContext(long contextHandle, long parentContextHandle);

    public static native int getNodeId(long viewNodeHandle);

    public static native String getViewClassName(long viewNodeHandle);

    public static native void callJSFunction(long runtimeHandle, long contextHandle, String functionName, Object[] parameters);

    public static native Object getViewInContextForId(long contextHandle, String viewId);

    public static native Object getViewNodeBackingView(long viewNodeHandle);

    public static native long getRetainedViewNodeInContext(long contextHandle, String viewId, int viewNodeId);

    public static native Object getRetainedViewNodeChildren(long viewNodeHandle, int mode);

    public static native long getRetainedViewNodeHitTestAccessibility(long runtimeHandle, long viewNodeHandle, int x, int y);

    public static native Object getViewNodeAccessibilityHierarchyRepresentation(long runtimeHandle, long viewNodeHandle);

    public static native int performViewNodeAction(long viewNodeHandle, int action, int param1, int param2);

    public static native void waitUntilAllUpdatesCompleted(long contextHandle, Object callback, boolean shouldFlushRenderRequests);

    public static native void onNextLayout(long contextHandle, Object callback);
    public static native void scheduleExclusiveUpdate(long contextHandle, Object runnable);

    public static native void setViewInflationEnabled(long contextHandle, boolean enabled);

    public static native void enqueueWorkerTask(long runtimeManagerHandle, Object runnable);

    public static native void performCallback(long nativePayloadHandle);
    public static native void discardCallback(long nativePayloadHandle);

    public static native void deleteNativeHandle(long actionHandle);
    public static native void releaseNativeRef(long nativeHandle);

    public static native void registerNativeModuleFactory(long runtimeHandle, Object moduleFactory);
    public static native void registerTypeConverter(long runtimeManagerHandle, String className, String functionPath);
    public static native void registerModuleFactoriesProvider(long runtimeManagerHandle, Object moduleFactoriesProvider);
    public static native long getViewNodePoint(long runtimeHandle, long viewNodeHandle, int x, int y, int mode, boolean fromBoundsOrigin);
    public static native long getViewNodeSize(long runtimeHandle, long viewNodeHandle, int mode);

    public static native boolean canViewNodeScroll(long runtimeHandle, long viewNodeHandler, int x, int y, int direction);

    public static native boolean isViewNodeScrollingOrAnimating(long viewNodeHandle);

    public static native boolean invalidateLayout(long viewNodeHandle);

    public static native void notifyApplyAttributeFailed(long viewNodeHandle, int attributeId, String errorMessage);

    public static native long notifyScroll(long runtimeHandle,
                                           long viewNodeHandle,
                                           int eventType,
                                           int contentOffsetX,
                                           int contentOffsetY,
                                           int unclampedContentOffsetX,
                                           int unclampedContentOffsetY,
                                           float pixelDirectionDependentInvertedVelocityX,
                                           float pixelDirectionDependentInvertedVelocityY);

    public static native void setLayoutSpecs(long runtimeHandle, long contextHandle, int width, int height, boolean isRTL);

    public static native long measureLayout(long runtimeHandle, long contextHandle, int maxWidth, int widthMode, int maxHeight, int heightMode, boolean isRTL);

    public static native void setVisibleViewport(long runtimeHandle, long contextHandle, int x, int y, int width, int height, boolean shouldUnset);

    // The given runnable must be an object implementing java.lang.Runnable
    public static native void callOnJsThread(long runtimeHandle, boolean sync, Object runnable);

    // The given runnable must be an object implementing java.lang.Runnable
    public static native void enqueueLoadOperation(long runtimeManagerHandle, Object runnable);

    public static native void flushLoadOperations(long runtimeManagerHandle);

    public static native void callSyncWithJsThread(long runtimeHandle, Object runnable);

    public static native void unloadAllJsModules(long runtimeHandle);

    public static native void clearViewPools(long runtimeManagerHandle);

    public static native void applicationSetConfiguration(long runtimeManagerHandle, float dynamicTypeScale);

    public static native void setPointScale(long runtimeManagerHandle, float pointScale);

    public static native void applicationDidResume(long runtimeManagerHandle);

    public static native void applicationWillPause(long runtimeManagerHandle);

    public static native void applicationIsInLowMemory(long runtimeManagerHandle);

    public static native String getViewNodeDebugDescription(long viewNodeHandle);

    public static native void valueChangedForAttribute(long runtimeHandle, long viewNodeHandle, long attributePtr, Object value);

    public static native void setValueForAttribute(long viewNodeHandle, String attribute, Object value, boolean keepAsOverride);

    public static native Object getValueForAttribute(long viewNodeHandle, String attribute);

    public static native void reapplyAttribute(long viewNodeHandle, String attribute);

    public static native boolean isViewNodeLayoutDirectionHorizontal(long viewNodeHandle);

    public static native Object dumpMemoryStatistics(long runtimeManagerHandle);
    public static native Object captureJavaScriptStackTraces(long runtimeHandle, long timeoutMs);
    public static native Object getAllModuleHashes(long runtimeHandle);

    public static native String dumpLogMetadata(long runtimeHandle, boolean isCrashLogs);

    public static native String dumpLogs(long runtimeHandle);

    public static native void setRuntimeAttachedObject(long runtimeHandle, Object object);

    public static native Object getRuntimeAttachedObject(long runtimeHandle);

    public static native Object getRuntimeAttachedObjectFromContext(long contextHandle);

    public static native Object getAllRuntimeAttachedObjects(long runtimeManagerHandle);

    public static native void preloadViews(long runtimeManagerHandle, String className, int count);

    public static native void forceBindAttributes(long runtimeManagerHandle, String className);

    public static native int bindAttribute(long bindingContextHandle,
                                           int type,
                                           String name,
                                           boolean invalidateLayoutOnChange,
                                           Object delegate,
                                           Object compositeParts);
    public static native void bindScrollAttributes(long bindingContextHandle);
    public static native void bindAssetAttributes(long bindingContextHandle, int outputType);
    public static native void setMeasureDelegate(long bindingContextHandle, Object measureDelegate);
    public static native void setPlaceholderViewMeasureDelegate(long bindingContextHandle, Object placeholderViewProvider);
    public static native void registerAttributePreprocessor(long bindingContextHandle,
                                                            String name,
                                                            boolean enableCache,
                                                            Object preprocessor);

    public static native void setUserSession(long runtimeManagerHandle, String userId);

    public static native Object getJSRuntime(long runtimeHandle);

    public static native void performGcNow(long runtimeHandle);

    public static native void loadModule(long runtimeHandle, String moduleName, ValdiFunction completion);

    public static native void setDisableViewReuse(long contextHandle, boolean disableViewReuse);

    public static native void setKeepViewAliveOnDestroy(long contextHandle, boolean keepViewAliveOnDestroy);

    public static native long createViewFactory(long runtimeManagerHandle, String viewClassName, Object viewFactory, boolean hasBindAttributes);

    public static native void registerLocalViewFactory(long contextHandler, long viewFactoryHandle);

    public static native Object getCurrentContext();

    public static native Object getAllContexts(long runtimeHandle);

    public static native Object getAsset(long runtimeHandle, String moduleName, String pathOrUrl);

    public static native Object getModuleEntry(long runtimeHandle, String moduleName, String path, boolean saveToDisk);

    public static native double getCurrentEventTime();

    public static native void startProfiling(long runtimeHandle);

    public static native void stopProfiling(long runtimeHandle, ValdiFunction completion);

    public static native Object wrapAndroidBitmap(Object androidBitmap);

    public static native long getSnapDrawingRuntimeHandle(long runtimeManagerHandle);
    public static native long createSnapDrawingRoot(long snapDrawingRuntimeHandle, boolean disallowSynchronousDraw);
    public static native boolean dispatchSnapDrawingTouchEvent(long skiaRootHandle, int type, long eventTimeNanos, int x, int y, int dx, int dy, int pointerCount, int actionIndex, int[] pointerLocations, Object motionEvent);
    public static native void snapDrawingLayout(long skiaRootHandle, int width, int height);
    public static native void snapDrawingRegisterTypeface(long snapDrawingRuntimeHandle,
                                                          String familyName,
                                                          String fontWeight,
                                                          String fontStyle,
                                                          boolean isFallback,
                                                          byte[] typefaceBytes,
                                                          int typefaceFd);
    public static native void snapDrawingDrawInBitmap(long skiaRootHandle, int surfacePresenterId, Object bitmapHandler, boolean isOwned);
    public static native void snapDrawingSetSurface(long skiaRootHandle, int surfacePresenterId, Object surface);
    public static native void snapDrawingOnSurfaceSizeChanged(long skiaRootHandle, int surfacePresenterId);
    public static native void snapDrawingSetSurfaceNeedsRedraw(long skiaRootHandle, int surfacePresenterId);
    public static native void snapDrawingSetSurfacePresenterManager(long skiaRootHandle, Object surfacePresenterManager);
    public static native void snapDrawingCallFrameCallback(long frameCallback, long frameTimeNanos);
    public static native void snapDrawingDrawLayerInBitmap(long snapDrawingLayerHandle, Object bitmap);
    public static native long snapDrawingGetMaxRenderTargetSize(long snapDrawingRuntimeHandle);

}
