//
//  RuntimeManagerWrapper.hpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <atomic>

#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Lazy.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include <jni.h>
#include <memory>

#include "valdi/android/Logger.hpp"
#include "valdi/android/MainThreadDispatcher.hpp"
#include "valdi/android/ResourceLoader.hpp"
#include "valdi/android/RuntimeListener.hpp"
#include "valdi/android/ViewManager.hpp"
#if SNAP_DRAWING_ENABLED
#include "snap_drawing/cpp/Touches/GesturesConfiguration.hpp"
#include "valdi/android/snap_drawing/AndroidSnapDrawingRuntime.hpp"
#endif

namespace snap::valdi {
class BundleManager;
class Keychain;
struct HTTPResponse;
} // namespace snap::valdi

namespace Valdi {
class IDiskCache;
class ViewManagerContext;
class LibraryLoader;
} // namespace Valdi

namespace snap::drawing {
class SnapDrawingViewManager;
class ImageLoader;
class Renderer;
} // namespace snap::drawing

namespace ValdiAndroid {

class ViewManager;
class AndroidSnapDrawingRuntime;

class RuntimeManagerWrapper {
public:
    RuntimeManagerWrapper(JavaEnv env,
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
                          float pointScale,
                          jint longPressTimeoutMs,
                          jint doubleTapTimeoutMs,
                          jint dragTouchSlopPixels,
                          jint touchTolerancePixels,
                          jfloat scrollFriction,
                          jboolean debugTouchEvents,
                          jboolean keepDebuggerServiceOnPause,
                          jobject javaScriptEngineType,
                          uint64_t maxCacheSizeInBytes,
                          jint jsThreadQoS,
                          jint anrTimeoutMs);
    ~RuntimeManagerWrapper();

    const Valdi::Ref<Valdi::ViewManagerContext>& getAndroidViewManagerContext() const;
    const Valdi::Ref<Valdi::ViewManagerContext>& getOrCreateSnapDrawingViewManagerContext();

    Valdi::SharedRuntime createRuntime(jobject customResourceResolver);

    Logger& getLogger();

    float getPointScale() const;
    void setPointScale(float pointScale);

    void setRequestManager(const Valdi::Shared<snap::valdi_core::HTTPRequestManager>& requestManager);

    void registerAssetLoader(jobject assetLoader, jobject supportedSchemes, jint supportedOutputTypes);
    void unregisterAssetLoader(jobject assetLoader);

    void prepareAndroidRenderBackend();
    void prepareSnapDrawingRenderBackend();

    void applicationDidResume();
    void applicationWillPause();
    void applicationIsInLowMemory();

    const Valdi::Ref<AndroidSnapDrawingRuntime>& getOrCreateSnapDrawingRuntime();

    Valdi::RuntimeManager& getRuntimeManager() const;

    ViewManager& getAndroidViewManager() const;

    Valdi::Shared<Valdi::IResourceLoader> createResourceLoader(jobject customResourceResolver) const;

    void setDynamicTypeScale(jfloat dynamicTypeScale);

private:
    Valdi::Ref<MainThreadDispatcher> _mainThreadDispatcher;
    Valdi::Ref<Logger> _logger;
    Valdi::Ref<ViewManager> _viewManager;
    GlobalRefJavaObject _contextManager;
    Valdi::Ref<ResourceLoader> _resourceLoader;
    Valdi::Ref<Valdi::IDiskCache> _diskCache;
    Valdi::StringBox _applicationId;
    std::atomic<float> _pointScale;

    Valdi::Ref<Valdi::RuntimeManager> _runtimeManager;

    Valdi::Ref<Valdi::ViewManagerContext> _androidViewManagerContext;
    jfloat _dynamicTypeScale = 1.0f;

#if SNAP_DRAWING_ENABLED
    Valdi::Lazy<Valdi::Ref<AndroidSnapDrawingRuntime>> _snapDrawingRuntime;
    Valdi::Lazy<Valdi::Ref<Valdi::ViewManagerContext>> _snapDrawingViewManagerContext;

    void applyDynamicTypeScale(const Valdi::Ref<AndroidSnapDrawingRuntime>& snapDrawingRuntime) const;
#endif
};

} // namespace ValdiAndroid
