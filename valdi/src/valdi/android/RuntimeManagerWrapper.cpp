//
//  RuntimeManagerWrapper.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/6/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/android/RuntimeManagerWrapper.hpp"
#include "valdi/android/AndroidAssetLoader.hpp"
#include "valdi/android/AndroidDispatchQueue.hpp"
#include "valdi/android/DeferredViewOperations.hpp"

#include "valdi/android/AndroidKeychain.hpp"

#include "valdi_core/jni/JavaUtils.hpp"

#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"
#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi/runtime/Resources/AssetLoaderManager.hpp"
#include "valdi/runtime/Resources/DiskCacheImpl.hpp"
#include "valdi/runtime/Views/Measure.hpp"

#include "snap_drawing/cpp/Text/LoadableTypeface.hpp"
#include "valdi/snap_drawing/Modules/SnapDrawingModuleFactoriesProvider.hpp"
#include "valdi/snap_drawing/Text/FontResolverWithRuntimeManager.hpp"

#include "valdi/runtime/Utils/BytesUtils.hpp"
#include "valdi/runtime/Utils/HTTPRequestManagerUtils.hpp"
#include "valdi_core/NativeJavaScriptEngineType.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include "valdi/runtime/Context/ViewManagerContext.hpp"

#include "valdi/jsbridge/JavaScriptBridge.hpp"

#include "RuntimeManagerWrapper.hpp"
#include "valdi_core/HTTPRequest.hpp"
#include "valdi_core/HTTPRequestManager.hpp"
#include <utils/debugging/Assert.hpp>

namespace ValdiAndroid {

RuntimeManagerWrapper::RuntimeManagerWrapper(JavaEnv env,
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
                                             jint anrTimeoutMs)
    : _mainThreadDispatcher(Valdi::makeShared<MainThreadDispatcher>(env, mainThreadDispatcher)),
      _logger(Valdi::makeShared<Logger>(env, logger)),
      _contextManager(env, contextManager, "ContextManager"),
      _applicationId(ValdiAndroid::toInternedString(env, applicationId)),
      _pointScale(pointScale) {
    _resourceLoader = Valdi::makeShared<ResourceLoader>(env, localResourceResolver, assetManager, false);

    Valdi::DispatchQueue::setMain(Valdi::makeShared<AndroidDispatchQueue>(_mainThreadDispatcher));

    auto keychainCpp = Valdi::makeShared<AndroidKeychain>(keychain);
    auto cacheRootDirCpp = ValdiAndroid::toInternedString(env, cacheRootDir);
    auto diskCache = Valdi::makeShared<Valdi::DiskCacheImpl>(cacheRootDirCpp);
    diskCache->setAllowedReadPath(STRING_LITERAL("/"));
    _diskCache = std::move(diskCache);

    _viewManager = Valdi::makeShared<ViewManager>(env, viewManager, pointScale, *_logger);

    auto jsEngineType = djinni_generated_client::valdi_core::NativeJavaScriptEngineType::toCpp(
        ValdiAndroid::JavaEnv::getUnsafeEnv(), javaScriptEngineType);
    {
        if (jsEngineType == snap::valdi_core::JavaScriptEngineType::JSCore) {
            // Fallback to auto as we disabled JSCore support in debug builds
            jsEngineType = snap::valdi_core::JavaScriptEngineType::Auto;
            VALDI_WARN(*_logger, "JSCore is no longer supported on Android.");
        }

        VALDI_TRACE("Valdi.createNativeRuntimeManager");
        _runtimeManager = Valdi::makeShared<Valdi::RuntimeManager>(_mainThreadDispatcher,
                                                                   Valdi::JavaScriptBridge::get(jsEngineType),
                                                                   _diskCache,
                                                                   keychainCpp,
                                                                   _viewManager.toShared(),
                                                                   Valdi::PlatformTypeAndroid,
                                                                   static_cast<Valdi::ThreadQoSClass>(jsThreadQoS),
                                                                   _logger,
                                                                   /* enableDebuggerService */ true,
                                                                   /* disableHotReloader */ false,
                                                                   /* isStandalone */ false);
        _runtimeManager->postInit();
        _runtimeManager->setKeepDebuggerServiceOnPause(static_cast<bool>(keepDebuggerServiceOnPause));
        _runtimeManager->setApplicationId(_applicationId);
        _runtimeManager->registerBytesAssetLoader(_diskCache->scopedCache(Valdi::Path("downloader"), true));
        if (anrTimeoutMs > 0) {
            _runtimeManager->getANRDetector()->start(std::chrono::milliseconds(anrTimeoutMs));
        }
    }

    auto preloaderWorkQueue =
        Valdi::DispatchQueue::create(STRING_LITERAL("Valdi View Preloader"), Valdi::ThreadQoSClassLow);

    _androidViewManagerContext = _runtimeManager->createViewManagerContext(*_viewManager, true);
    _androidViewManagerContext->setPreloadingPaused(true);
    _androidViewManagerContext->setPreloadingWorkQueue(preloaderWorkQueue);

#if SNAP_DRAWING_ENABLED
    {
        auto gesturesConfiguration = snap::drawing::GesturesConfiguration(
            snap::drawing::Duration::fromMilliseconds(static_cast<double>(longPressTimeoutMs)),
            snap::drawing::Duration::fromMilliseconds(static_cast<double>(doubleTapTimeoutMs)),
            Valdi::pixelsToPoints(static_cast<int32_t>(dragTouchSlopPixels), pointScale),
            Valdi::pixelsToPoints(static_cast<int32_t>(touchTolerancePixels), pointScale),
            static_cast<snap::drawing::Scalar>(scrollFriction),
            static_cast<bool>(debugTouchEvents));
        _snapDrawingRuntime.setFactory([this,
                                        gesturesConfiguration,
                                        maxCacheSizeInBytes,
                                        snapDrawingFrameScheduler = GlobalRefJavaObjectBase(
                                            JavaEnv(), snapDrawingFrameScheduler, "SnapDrawingFrameScheduler")]() {
            VALDI_TRACE("Valdi.createSnapDrawingRuntime");
            auto snapDrawingRuntime =
                Valdi::makeShared<ValdiAndroid::AndroidSnapDrawingRuntime>(snapDrawingFrameScheduler,
                                                                           gesturesConfiguration,
                                                                           _diskCache,
                                                                           *_viewManager,
                                                                           *_logger,
                                                                           _runtimeManager->getWorkerQueue(),
                                                                           maxCacheSizeInBytes);

            snapDrawingRuntime->registerAssetLoaders(*_runtimeManager->getAssetLoaderManager());
            snapDrawingRuntime->getFontManager()->setListener(
                Valdi::makeShared<snap::drawing::FontResolverWithRuntimeManager>(_runtimeManager));
            applyDynamicTypeScale(snapDrawingRuntime);

            return snapDrawingRuntime;
        });

        _snapDrawingViewManagerContext.setFactory([this, preloaderWorkQueue]() {
            VALDI_TRACE("Valdi.createSnapDrawingViewManagerContext");
            auto snapDrawingViewManagerContext =
                _runtimeManager->createViewManagerContext(*_snapDrawingRuntime.getOrCreate()->getViewManager(), true);
            snapDrawingViewManagerContext->setPreloadingPaused(true);
            snapDrawingViewManagerContext->setPreloadingWorkQueue(preloaderWorkQueue);

            return snapDrawingViewManagerContext;
        });

        _runtimeManager->registerModuleFactoriesProvider(
            Valdi::makeShared<snap::drawing::SnapDrawingModuleFactoriesProvider>(
                [this]() -> Valdi::Ref<snap::drawing::Runtime> { return this->_snapDrawingRuntime.getOrCreate(); })
                .toShared());
    }
#endif
    _runtimeManager->getAssetLoaderManager()->registerAssetLoaderFactory(
        Valdi::makeShared<AndroidAssetLoaderFactory>(_resourceLoader));
}

RuntimeManagerWrapper::~RuntimeManagerWrapper() {
    Valdi::DispatchQueue::setMain(nullptr);
    _runtimeManager->fullTeardown();
}

const Valdi::Ref<Valdi::ViewManagerContext>& RuntimeManagerWrapper::getAndroidViewManagerContext() const {
    return _androidViewManagerContext;
}

ViewManager& RuntimeManagerWrapper::getAndroidViewManager() const {
    return *_viewManager;
}

const Valdi::Ref<Valdi::ViewManagerContext>& RuntimeManagerWrapper::getOrCreateSnapDrawingViewManagerContext() {
    return _snapDrawingViewManagerContext.getOrCreate();
}

Valdi::Shared<Valdi::IResourceLoader> RuntimeManagerWrapper::createResourceLoader(
    jobject customResourceResolver) const {
    if (customResourceResolver != nullptr) {
        return _resourceLoader->withCustomResourceResolver(customResourceResolver);
    } else {
        return _resourceLoader.toShared();
    }
}

Valdi::SharedRuntime RuntimeManagerWrapper::createRuntime(jobject customResourceResolver) {
    auto resourceLoader = createResourceLoader(customResourceResolver);

    auto javaEnv = ValdiAndroid::JavaEnv();
    auto javacontextManager = _contextManager.get();
    auto runtimeListener = Valdi::makeShared<RuntimeListener>(javaEnv, javacontextManager.get());

    auto runtime = _runtimeManager->createRuntime(resourceLoader, static_cast<double>(_viewManager->getPointScale()));
    runtime->setListener(runtimeListener);

    return runtime;
}

void RuntimeManagerWrapper::prepareAndroidRenderBackend() {
    _androidViewManagerContext->setPreloadingPaused(false);
}

void RuntimeManagerWrapper::prepareSnapDrawingRenderBackend() {
#if SNAP_DRAWING_ENABLED
    _snapDrawingViewManagerContext.getOrCreate()->setPreloadingPaused(false);
    _runtimeManager->enqueueLoadOperation([this]() { this->_snapDrawingRuntime.getOrCreate()->preload(); });
#endif
}

void RuntimeManagerWrapper::applicationWillPause() {
    _runtimeManager->applicationWillPause();

#if SNAP_DRAWING_ENABLED
    auto snapDrawingRuntime = _snapDrawingRuntime.getIfCreated();
    if (snapDrawingRuntime) {
        snapDrawingRuntime.value()->getDrawLooper()->onApplicationEnteringBackground();
    }
#endif
}

void RuntimeManagerWrapper::applicationDidResume() {
    _runtimeManager->applicationDidResume();

#if SNAP_DRAWING_ENABLED
    auto snapDrawingRuntime = _snapDrawingRuntime.getIfCreated();
    if (snapDrawingRuntime) {
        snapDrawingRuntime.value()->getDrawLooper()->onApplicationEnteringForeground();
    }
#endif
}

void RuntimeManagerWrapper::applicationIsInLowMemory() {
    _runtimeManager->applicationIsLowInMemory();

#if SNAP_DRAWING_ENABLED
    auto snapDrawingRuntime = _snapDrawingRuntime.getIfCreated();
    if (snapDrawingRuntime) {
        snapDrawingRuntime.value()->getDrawLooper()->onApplicationIsInLowMemory();
    }
#endif
}

#if SNAP_DRAWING_ENABLED
void RuntimeManagerWrapper::applyDynamicTypeScale(
    const Valdi::Ref<AndroidSnapDrawingRuntime>& snapDrawingRuntime) const {
    auto viewManager = snapDrawingRuntime->getViewManager();
    auto resources = viewManager->getResources();
    resources->setDynamicTypeScale(_dynamicTypeScale);
}
#endif

void RuntimeManagerWrapper::setDynamicTypeScale(jfloat dynamicTypeScale) {
    _dynamicTypeScale = dynamicTypeScale;
#if SNAP_DRAWING_ENABLED
    auto snapDrawingRuntime = _snapDrawingRuntime.getIfCreated();
    if (snapDrawingRuntime) {
        applyDynamicTypeScale(snapDrawingRuntime.value());
    }
#endif
}

void RuntimeManagerWrapper::registerAssetLoader(jobject assetLoader,
                                                jobject supportedSchemes,
                                                jint supportedOutputTypes) {
    auto assetLoaderManager = _runtimeManager->getAssetLoaderManager();

    for (int i = 0x000001; i < 0x001000; i = i << 4) {
        if ((supportedOutputTypes & i) != 0) {
            assetLoaderManager->registerAssetLoader(
                Valdi::makeShared<AndroidAssetLoader>(_resourceLoader, JavaEnv(), assetLoader, supportedSchemes, i));
        }
    }
}

void RuntimeManagerWrapper::unregisterAssetLoader(jobject assetLoader) {
    auto assetLoaders = _runtimeManager->getAssetLoaderManager()->getAllAssetLoaders();

    for (const auto& assetLoaderCpp : assetLoaders) {
        auto androidAssetLoader = Valdi::castOrNull<ValdiAndroid::AndroidAssetLoader>(assetLoaderCpp);
        if (androidAssetLoader != nullptr && JavaEnv::isSameObject(assetLoader, androidAssetLoader->get().get())) {
            _runtimeManager->getAssetLoaderManager()->unregisterAssetLoader(assetLoaderCpp);
        }
    }
}

void RuntimeManagerWrapper::setRequestManager(
    const Valdi::Shared<snap::valdi_core::HTTPRequestManager>& requestManager) {
    if (_runtimeManager != nullptr) {
        _runtimeManager->setRequestManager(requestManager);
    }
}

Logger& RuntimeManagerWrapper::getLogger() {
    return *_logger;
}

float RuntimeManagerWrapper::getPointScale() const {
    return _pointScale.load(std::memory_order_relaxed);
}

void RuntimeManagerWrapper::setPointScale(float pointScale) {
    _pointScale.store(pointScale, std::memory_order_relaxed);
    _viewManager->setPointScale(pointScale);
}

Valdi::RuntimeManager& RuntimeManagerWrapper::getRuntimeManager() const {
    return *_runtimeManager;
}

#if SNAP_DRAWING_ENABLED
const Valdi::Ref<AndroidSnapDrawingRuntime>& RuntimeManagerWrapper::getOrCreateSnapDrawingRuntime() {
    return _snapDrawingRuntime.getOrCreate();
}
#endif

} // namespace ValdiAndroid
