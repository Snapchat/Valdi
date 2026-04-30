//
//  NativeBridge.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/9/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/android/AndroidViewFactory.hpp"
#include "valdi/android/RuntimeManagerWrapper.hpp"
#include "valdi/android/RuntimeWrapper.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

#include "valdi/runtime/Attributes/AttributeOwner.hpp"
#include "valdi/runtime/Attributes/AttributesApplier.hpp"
#include "valdi/runtime/Attributes/AttributesBindingContextImpl.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNodePath.hpp"
#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi_core/jni/IndirectJavaGlobalRef.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaClass.hpp"
#include "valdi_core/jni/JavaRunnable.hpp"

#include "utils/platform/BuildOptions.hpp"
#include "utils/time/StopWatch.hpp"
#include "valdi_core/NativeAsset.hpp"
#include "valdi_core/NativeHTTPRequestManager.hpp"
#include "valdi_core/NativeJSRuntime.hpp"
#include "valdi_core/NativeModuleFactoriesProvider.hpp"
#include "valdi_core/NativeModuleFactory.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/TimePoint.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/ValdiBuildFlags.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"

#include "valdi/android/AccessibilityBridge.hpp"
#include "valdi/android/AndroidAssetLoader.hpp"
#include "valdi/android/AndroidBitmap.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/AttributesBindingContextWrapper.hpp"
#include "valdi/android/DeferredViewOperations.hpp"
#include "valdi/android/NativeBridge.hpp"

#if SNAP_DRAWING_ENABLED
#include "snap_drawing/cpp/Utils/LottieAnimatedImage.hpp"
#include "valdi/android/snap_drawing/AndroidSnapDrawingRuntime.hpp"
#endif

#include <android/native_window_jni.h>

inline ValdiAndroid::RuntimeWrapper* getRuntimeWrapper(jlong handle) {
    return reinterpret_cast<ValdiAndroid::RuntimeWrapper*>(handle);
}

inline Valdi::Runtime* getRuntime(jlong handle) {
    auto* runtimeWrapper = getRuntimeWrapper(handle);
    if (runtimeWrapper != nullptr) {
        return &runtimeWrapper->getRuntime();
    }

    return nullptr;
}

static Valdi::SharedContext getContext(jlong contextHandle) {
    return Valdi::unsafeBridge<Valdi::Context>(reinterpret_cast<void*>(contextHandle));
}

static Valdi::SharedViewNodeTree getViewNodeTree(jlong contextHandle) {
    auto context = getContext(contextHandle);
    if (context == nullptr) {
        return nullptr;
    }

    auto* runtime = context->getRuntime();
    if (runtime == nullptr) {
        return nullptr;
    }

    return runtime->getOrCreateViewNodeTreeForContextId(context->getContextId());
}

Valdi::Ref<Valdi::ViewNode> getViewNode(jlong viewNodeHandle) {
    return ValdiAndroid::bridge<Valdi::ViewNode>(viewNodeHandle);
}

inline ValdiAndroid::RuntimeManagerWrapper* getRuntimeManagerWrapper(jlong handle) {
    return reinterpret_cast<ValdiAndroid::RuntimeManagerWrapper*>(handle);
}

Valdi::Value javaToValue(const char* traceTag,
                         ValdiAndroid::JavaEnv javaEnv,
                         const ValdiAndroid::JavaObject& javaObject) {
    VALDI_TRACE(traceTag);
    return ValdiAndroid::toValue(javaEnv, javaObject.newLocalRef());
}

::djinni::LocalRef<jobject> sGetRuntimeAttachedObject(Valdi::Runtime* runtime) {
    auto* runtimeListener = dynamic_cast<ValdiAndroid::RuntimeListener*>(runtime->getListener().get());
    if (runtimeListener == nullptr) {
        return ::djinni::LocalRef<jobject>();
    }

    return runtimeListener->getAttachedObject().get();
}

//  NOLINTNEXTLINE
jint ValdiAndroid::NativeBridge::getBuildOptions(fbjni::alias_ref<fbjni::JClass> /* clazz */) {
    int32_t buildOptions = 0;
    if constexpr (snap::kIsGoldBuild || snap::kIsDevBuild) {
        buildOptions |= 1 << 0;
    }

    if constexpr (snap::kLoggingCompiledIn) {
        buildOptions |= 1 << 1;
    }

    if constexpr (Valdi::kTracingEnabled) {
        buildOptions |= 1 << 2;
    }

    if constexpr (sizeof(void*) == 8) {
        buildOptions |= 1 << 3;
    }

#ifdef SNAP_DRAWING_ENABLED
    buildOptions |= 1 << 4;

    if constexpr (snap::drawing::kLottieEnabled) {
        buildOptions |= 1 << 5;
    }
#endif

    return static_cast<jint>(buildOptions);
}

jlong ValdiAndroid::NativeBridge::createRuntimeManager( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,        // NOLINT
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
    jint anrTimeoutMs) {
    auto javaEnv = ValdiAndroid::JavaEnv();

    auto* runtimeManagerWrapper = new ValdiAndroid::RuntimeManagerWrapper(javaEnv,
                                                                          mainThreadDispatcher,
                                                                          snapDrawingFrameScheduler,
                                                                          viewManager,
                                                                          logger,
                                                                          contextManager,
                                                                          localResourceResolver,
                                                                          assetManager,
                                                                          keychain,
                                                                          cacheRootDir,
                                                                          applicationId,
                                                                          pointScale,
                                                                          longPressTimeoutMs,
                                                                          doubleTapTimeoutMs,
                                                                          dragTouchSlopPixels,
                                                                          touchTolerancePixels,
                                                                          scrollFriction,
                                                                          debugTouchEvents,
                                                                          keepDebuggerServiceOnPause,
                                                                          javaScriptEngineType,
                                                                          static_cast<uint64_t>(maxCacheSizeInBytes),
                                                                          jsThreadQoS,
                                                                          anrTimeoutMs);

    return reinterpret_cast<std::uintptr_t>(runtimeManagerWrapper);
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    deleteRuntime
 * Signature: (J)V
 */
void ValdiAndroid::NativeBridge::deleteRuntimeManager( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong handle) {
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(handle);
    delete runtimeManagerWrapper;
}

void ValdiAndroid::NativeBridge::registerViewClassReplacement( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,               // NOLINT
    jlong runtimeManagerHandle,
    jstring sourceClass,
    jstring replacementClass) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper != nullptr) {
        auto javaEnv = ValdiAndroid::JavaEnv();
        wrapper->getAndroidViewManagerContext()->registerViewClassReplacement(
            ValdiAndroid::toInternedString(javaEnv, sourceClass),
            ValdiAndroid::toInternedString(javaEnv, replacementClass));
    }
}

void ValdiAndroid::NativeBridge::prepareRenderBackend( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong runtimeManagerHandle,
    jboolean useSnapDrawing) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    if (static_cast<bool>(useSnapDrawing)) {
        wrapper->prepareSnapDrawingRenderBackend();
    } else {
        wrapper->prepareAndroidRenderBackend();
    }
}

void ValdiAndroid::NativeBridge::emitRuntimeManagerInitMetrics( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                // NOLINT
    jlong runtimeManagerHandle) {
    auto wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    wrapper->getRuntimeManager().emitInitMetrics();
}

void ValdiAndroid::NativeBridge::emitUserSessionReadyMetrics( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,              // NOLINT
    jlong runtimeManagerHandle) {
    auto wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    wrapper->getRuntimeManager().emitUserSessionReadyMetrics();
}

void ValdiAndroid::NativeBridge::registerAssetLoader( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /*clazz*/,        // NOLINT
    jlong runtimeManagerHandle,
    jobject assetLoader,
    jobject supportedSchemes,
    jint supportedOutputTypes) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper != nullptr) {
        wrapper->registerAssetLoader(assetLoader, supportedSchemes, supportedOutputTypes);
    }
}

void ValdiAndroid::NativeBridge::unregisterAssetLoader( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /*clazz*/,          // NOLINT
    jlong runtimeManagerHandle,
    jobject assetLoader) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper != nullptr) {
        wrapper->unregisterAssetLoader(assetLoader);
    }
}

jboolean ValdiAndroid::NativeBridge::notifyAssetLoaderCompleted( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /*clazz*/,                   // NOLINT
    jlong callbackHandle,
    jobject asset,
    jstring error,
    jboolean isImage) {
    auto callback = Valdi::unsafeBridge<Valdi::AssetLoaderCompletion>(reinterpret_cast<void*>(callbackHandle));
    if (callback == nullptr) {
        return static_cast<jboolean>(false);
    }

    if (error != nullptr) {
        callback->onLoadComplete(Valdi::Error(toInternedString(JavaEnv(), error)));
    } else if (asset != nullptr) {
        auto loadedAsset = AndroidAssetLoader::createLoadedAsset(JavaEnv(), asset, isImage);
        callback->onLoadComplete(loadedAsset);
    } else {
        callback->onLoadComplete(Valdi::Error("Asset is null"));
    }

    return static_cast<jboolean>(true);
}

void ValdiAndroid::NativeBridge::setRuntimeManagerRequestManager( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                  // NOLINT
    jlong runtimeManagerHandle,
    jobject requestManager) {
    auto* env = fbjni::Environment::current();
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (runtimeManagerWrapper != nullptr) {
        if (requestManager != nullptr) {
            runtimeManagerWrapper->setRequestManager(
                djinni_generated_client::valdi_core::NativeHTTPRequestManager::toCpp(env, requestManager));
        } else {
            runtimeManagerWrapper->setRequestManager(nullptr);
        }
    }
}

void ValdiAndroid::NativeBridge::setRuntimeManagerJsThreadQoS( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,               // NOLINT
    jlong runtimeManagerHandle,
    jint jsThreadQoS) {
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (runtimeManagerWrapper != nullptr) {
        runtimeManagerWrapper->getRuntimeManager().setJsThreadQoS(static_cast<Valdi::ThreadQoSClass>(jsThreadQoS));
    }
}

jlong ValdiAndroid::NativeBridge::createRuntime( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeManagerHandle,
    jobject customResourceResolver) {
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    auto runtime = runtimeManagerWrapper->createRuntime(customResourceResolver);

    auto* runtimeWrapper = new ValdiAndroid::RuntimeWrapper(runtime, runtimeManagerWrapper);

    return reinterpret_cast<std::uintptr_t>(runtimeWrapper);
}

void ValdiAndroid::NativeBridge::onRuntimeReady(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                jlong runtimeManagerHandle,
                                                jlong runtimeHandle) {
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    runtimeManagerWrapper->getRuntimeManager().attachHotReloader(runtimeWrapper->getSharedRuntime());
    runtimeWrapper->getRuntime().emitInitMetrics();
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    deleteRuntime
 * Signature: (J)V
 */
void ValdiAndroid::NativeBridge::deleteRuntime(  // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong handle) {
    auto* runtimeWrapper = getRuntimeWrapper(handle);
    delete runtimeWrapper;
}

void ValdiAndroid::NativeBridge::loadModule(     // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jstring moduleName,
    fbjni::alias_ref<JValdiFunction> completion) { // NOLINT
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return;
    }
    auto* env = fbjni::Environment::current();
    auto javaEnv = ValdiAndroid::JavaEnv();
    auto cppModuleName = ValdiAndroid::toInternedString(javaEnv, moduleName);
    auto completionConverted = ValdiAndroid::toValue(javaEnv, ValdiAndroid::JavaEnv::newLocalRef(completion.get()));

    if (completionConverted.getFunction() == nullptr) {
        ValdiAndroid::throwJavaValdiException(env, "completion should be a function");
        return;
    }

    runtime->getResourceManager().loadModuleAsync(
        cppModuleName, Valdi::ResourceManagerLoadModuleType::Sources, [=](const auto& result) {
            Valdi::Value parameter;
            if (!result) {
                parameter = Valdi::Value(result.error());
            }

            (*completionConverted.getFunction())(&parameter, 1);
        });
}

jobject ValdiAndroid::NativeBridge::createContext( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,   // NOLINT
    jlong handle,
    jstring componentPath,
    jobject viewModel,
    jobject componentContext,
    jboolean useSnapDrawing) {
    auto* wrapper = getRuntimeWrapper(handle);
    if (wrapper == nullptr) {
        return nullptr;
    }

    auto* runtime = &wrapper->getRuntime();

    auto javaEnv = ValdiAndroid::JavaEnv();

    auto cppComponentPath = ValdiAndroid::toInternedString(javaEnv, componentPath);
    auto viewModelRetainedJava = ValdiAndroid::javaObjectToLazyValueConvertible(javaEnv, viewModel, "ViewModel");
    auto componentContextRetainedJava =
        ValdiAndroid::javaObjectToLazyValueConvertible(javaEnv, componentContext, "ComponentContext");

    Valdi::Ref<Valdi::ViewManagerContext> viewManagerContext;
    if (useSnapDrawing != 0) {
        viewManagerContext = wrapper->getRuntimeManagerWrapper()->getOrCreateSnapDrawingViewManagerContext();
    } else {
        viewManagerContext = wrapper->getRuntimeManagerWrapper()->getAndroidViewManagerContext();
    }

    auto context = runtime->createContext(
        viewManagerContext, cppComponentPath, viewModelRetainedJava, componentContextRetainedJava);

    auto javaContext = ValdiAndroid::fromValdiContextUserData(context->getUserData());
    return javaContext.releaseObject();
}

void ValdiAndroid::NativeBridge::contextOnCreate( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,  // NOLINT
    jlong contextHandle) {
    auto context = getContext(contextHandle);
    if (context != nullptr) {
        context->onCreate();
    }
}

void ValdiAndroid::NativeBridge::updateResource( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jbyteArray resource,
    jstring bundleName,
    jstring filePathWithinBundle) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();
    auto bytes = ValdiAndroid::toByteArray(javaEnv, resource);
    auto cppBundleName = ValdiAndroid::toInternedString(javaEnv, bundleName);
    auto cppFilePathWithinBundle = ValdiAndroid::toInternedString(javaEnv, filePathWithinBundle);

    runtime->updateResource(bytes, cppBundleName, cppFilePathWithinBundle);
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    destroyContext
 * Signature: (JJ)V
 */
void ValdiAndroid::NativeBridge::destroyContext( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong handle,
    jlong contextHandle) {
    auto* runtime = getRuntime(handle);
    auto context = getContext(contextHandle);

    if (runtime != nullptr && context != nullptr) {
        runtime->destroyContext(context);
    }
}

void ValdiAndroid::NativeBridge::setParentContext( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,   // NOLINT
    jlong contextHandle,
    jlong parentContextHandle) {
    auto viewNodeTree = getViewNodeTree(contextHandle);
    auto parentViewNodeTree = getViewNodeTree(parentContextHandle);

    if (viewNodeTree != nullptr) {
        viewNodeTree->setParentTree(parentViewNodeTree);
    }
}

jobject ValdiAndroid::NativeBridge::getViewNodeBackingView( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,            // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return nullptr;
    }

    auto view = viewNode->getViewAndDisablePooling();

    if (view == nullptr) {
        return nullptr;
    }

    return ValdiAndroid::javaObjectFromValdiObject(view, true).releaseObject();
}

void doSetRootViewToContext(jlong contextHandle, const Valdi::Ref<Valdi::View>& rootView) {
    auto viewNodeTree = getViewNodeTree(contextHandle);

    if (viewNodeTree == nullptr) {
        return;
    }

    viewNodeTree->setRootView(rootView);
}

void ValdiAndroid::NativeBridge::setRootView(    // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jlong contextHandle,
    jobject rootView) {
    auto* wrapper = getRuntimeWrapper(runtimeHandle);

    Valdi::Ref<Valdi::View> rootValdiView;
    if (rootView != nullptr) {
        rootValdiView = ValdiAndroid::toValdiView(ValdiAndroid::ViewType(ValdiAndroid::JavaEnv(), rootView),
                                                  wrapper->getRuntimeManagerWrapper()->getAndroidViewManager());
    }

    doSetRootViewToContext(contextHandle, rootValdiView);
}

void ValdiAndroid::NativeBridge::setRetainsLayoutSpecsOnInvalidateLayout( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                          // NOLINT
    jlong contextHandle,
    jboolean retainsLayoutSpecsOnInvalidateLayout) {
    auto viewNodeTree = getViewNodeTree(contextHandle);

    if (viewNodeTree == nullptr) {
        return;
    }

    viewNodeTree->setRetainsLayoutSpecsOnInvalidateLayout(static_cast<bool>(retainsLayoutSpecsOnInvalidateLayout));
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    setViewModel
 * Signature: (JJLjava/lang/Object;)V
 */
void ValdiAndroid::NativeBridge::setViewModel( // NOLINT

    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong contextHandle,
    jobject viewModel) {
    auto context = getContext(contextHandle);
    if (context == nullptr) {
        return;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();
    context->setViewModel(ValdiAndroid::javaObjectToLazyValueConvertible(javaEnv, viewModel, "ViewModel"));
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    callJSFunction
 * Signature: (JJLjava/lang/String;[Ljava/lang/Object;)V
 */
void ValdiAndroid::NativeBridge::callJSFunction( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jlong contextHandle,
    jstring jsString,
    fbjni::alias_ref<fbjni::JArrayClass<jobject>> jsParameters) { // NOLINT
    auto* runtime = getRuntime(runtimeHandle);
    auto context = getContext(contextHandle);

    if (context == nullptr || runtime == nullptr) {
        return;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();

    auto additionalParameters = ValdiAndroid::toValue(javaEnv, ValdiAndroid::JavaEnv::newLocalRef(jsParameters.get()));

    runtime->getJavaScriptRuntime()->callComponentFunction(
        context->getContextId(), ValdiAndroid::toInternedString(javaEnv, jsString), additionalParameters.getArrayRef());
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    getViewInContextForId
 * Signature: (JLjava/lang/String;)Ljava/lang/Object;
 */
jobject ValdiAndroid::NativeBridge::getViewInContextForId( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,           // NOLINT
    jlong contextHandle,
    jstring viewId) {
    auto viewNodeTree = getViewNodeTree(contextHandle);

    if (viewNodeTree == nullptr) {
        return nullptr;
    }
    auto* env = fbjni::Environment::current();
    auto javaEnv = ValdiAndroid::JavaEnv();

    auto viewIdCpp = ValdiAndroid::toInternedString(javaEnv, viewId);
    auto nodePathResult = Valdi::ViewNodePath::parse(viewIdCpp.toStringView());
    if (!nodePathResult) {
        ValdiAndroid::throwJavaValdiException(env, nodePathResult.error());
        return nullptr;
    }

    auto view = viewNodeTree->getViewForNodePath(nodePathResult.value());

    return ValdiAndroid::fromValdiView(view).releaseObject();
}

jlong ValdiAndroid::NativeBridge::getRetainedViewNodeInContext( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                // NOLINT
    jlong contextHandle,
    jstring viewId,
    jint viewNodeId) {
    static constexpr jint kRootViewNodeIdSentinel = -1;
    auto viewNodeTree = getViewNodeTree(contextHandle);

    if (viewNodeTree == nullptr) {
        return 0;
    }

    Valdi::Ref<Valdi::ViewNode> viewNode;

    if (viewId != nullptr) {
        auto* env = fbjni::Environment::current();
        auto javaEnv = ValdiAndroid::JavaEnv();

        auto viewIdCpp = ValdiAndroid::toInternedString(javaEnv, viewId);
        auto nodePathResult = Valdi::ViewNodePath::parse(viewIdCpp.toStringView());
        if (!nodePathResult) {
            ValdiAndroid::throwJavaValdiException(env, nodePathResult.error());
            return 0;
        }

        viewNode = viewNodeTree->getViewNodeForNodePath(nodePathResult.value());
    } else {
        if (viewNodeId == kRootViewNodeIdSentinel) {
            viewNode = viewNodeTree->getRootViewNode();
        } else {
            viewNode = viewNodeTree->getViewNode(static_cast<Valdi::RawViewNodeId>(viewNodeId));
        }
    }

    return ValdiAndroid::bridgeRetain(viewNode.get());
}

jobject ValdiAndroid::NativeBridge::getRetainedViewNodeChildren( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                 // NOLINT
    jlong viewNodeHandle,
    jint mode) {
    static constexpr jint kModeChildrenShallowAll = 1;
    static constexpr jint kModeChildrenShallowVisible = 2;

    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return nullptr;
    }

    std::vector<jlong> childrenHandles;

    bool modeShallowAll = mode == kModeChildrenShallowAll;
    bool modeShallowVisible = mode == kModeChildrenShallowVisible;
    if (modeShallowAll || modeShallowVisible) {
        auto size = viewNode->getChildCount();
        childrenHandles.reserve(size);
        for (size_t i = 0; i < size; i++) {
            auto childShallow = viewNode->getChildAt(i);
            if (modeShallowAll || childShallow->isVisibleInViewport()) {
                childrenHandles.emplace_back(ValdiAndroid::bridgeRetain(childShallow));
            }
        }
    }

    return ValdiAndroid::toJavaLongArray(ValdiAndroid::JavaEnv(), childrenHandles.data(), childrenHandles.size())
        .releaseObject();
}

jlong ValdiAndroid::NativeBridge::getRetainedViewNodeHitTestAccessibility( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                           // NOLINT
    jlong runtimeHandle,
    jlong viewNodeHandle,
    jint x,
    jint y) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr || runtimeWrapper == nullptr) {
        return 0;
    }

    Valdi::Point directionDependentVisual =
        Valdi::Point(Valdi::pixelsToPoints(static_cast<int32_t>(x), runtimeWrapper->getPointScale()),
                     Valdi::pixelsToPoints(static_cast<int32_t>(y), runtimeWrapper->getPointScale()));

    auto hitViewNode = viewNode->hitTestAccessibilityChildren(directionDependentVisual);
    if (hitViewNode != nullptr) {
        return ValdiAndroid::bridgeRetain(hitViewNode);
    }
    return 0;
}

jobject ValdiAndroid::NativeBridge::getViewNodeAccessibilityHierarchyRepresentation( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                                     // NOLINT
    jlong runtimeHandle,
    jlong viewNodeHandle) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr || runtimeWrapper == nullptr) {
        return nullptr;
    }

    std::vector<Valdi::Value> values;
    AccessibilityBridge::walkViewNode(viewNode.get(), values, runtimeWrapper->getPointScale());

    auto javaEnv = ValdiAndroid::JavaEnv();
    auto javaArray = ValdiAndroid::toJavaObject(javaEnv, values);
    return javaArray.releaseObject();
}

jint ValdiAndroid::NativeBridge::performViewNodeAction( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,        // NOLINT
    jlong viewNodeHandle,
    jint action,
    jint param1,
    jint param2) {
    static constexpr jint kActionEnsureFrameIsVisibleWithinParentScrolls = 1;
    static constexpr jint kActionScrollByOnePage = 2;

    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return 0;
    }

    jint handled = 0;

    auto* viewNodeTree = viewNode->getViewNodeTree();

    if (viewNodeTree == nullptr) {
        // TODO: is this a valid state?
        return 0;
    }

    viewNodeTree->withLock([&]() {
        auto& viewTransactionScope = viewNodeTree->getCurrentViewTransactionScope();
        if (action == kActionEnsureFrameIsVisibleWithinParentScrolls) {
            auto animated = static_cast<bool>(param1);
            viewNode->ensureFrameIsVisibleWithinParentScrolls(viewTransactionScope, animated);
            handled = 0;
            return;
        }

        if (action == kActionScrollByOnePage) {
            auto animated = static_cast<bool>(param1);
            auto scrollDirection = static_cast<Valdi::ScrollDirection>(param2);
            bool didScroll =
                viewNode->scrollByOnePage(viewTransactionScope, scrollDirection, animated, nullptr, nullptr);
            handled = didScroll ? 1 : 0;
            return;
        }
    });

    return handled;
}

void ValdiAndroid::NativeBridge::waitUntilAllUpdatesCompleted( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,               // NOLINT
    jlong contextHandle,
    jobject callback,
    jboolean shouldFlushRenderRequests) {
    auto context = getContext(contextHandle);
    if (context == nullptr) {
        return;
    }

    if (callback == nullptr) {
        context->waitUntilAllUpdatesCompletedSync(static_cast<bool>(shouldFlushRenderRequests));
    } else {
        auto value = ValdiAndroid::valueFromJavaValdiFunction(ValdiAndroid::JavaEnv(), callback);
        context->waitUntilAllUpdatesCompleted(value.getFunctionRef());
    }
}

void ValdiAndroid::NativeBridge::onNextLayout(   // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong contextHandle,
    jobject callback) {
    auto viewNodeTree = getViewNodeTree(contextHandle);

    if (viewNodeTree == nullptr) {
        return;
    }

    auto value = ValdiAndroid::valueFromJavaValdiFunction(ValdiAndroid::JavaEnv(), callback);
    auto lock = viewNodeTree->lock();
    viewNodeTree->onNextLayout(value.getFunctionRef());
}

void ValdiAndroid::NativeBridge::scheduleExclusiveUpdate( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,          // NOLINT
    jlong contextHandle,
    jobject runnable) {
    auto viewNodeTree = getViewNodeTree(contextHandle);

    if (viewNodeTree == nullptr) {
        return;
    }

    auto runnableRef = Valdi::makeShared<ValdiAndroid::JavaRunnable>(ValdiAndroid::JavaEnv(), runnable);

#if VALDI_DEBUG_TREE_UPDATES
    viewNodeTree->scheduleExclusiveUpdate(
        [runnableRef = std::move(runnableRef)]() { (*runnableRef)(); }, Valdi::DispatchFunction(), "native_bridge");
#else
    viewNodeTree->scheduleExclusiveUpdate([runnableRef = std::move(runnableRef)]() { (*runnableRef)(); },
                                          Valdi::DispatchFunction());
#endif
}

void ValdiAndroid::NativeBridge::setViewInflationEnabled( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,          // NOLINT
    jlong contextHandle,
    jboolean enabled) {
    auto tree = getViewNodeTree(contextHandle);
    if (tree == nullptr) {
        return;
    }
    tree->setViewInflationEnabled(enabled != 0);
}

void ValdiAndroid::NativeBridge::enqueueWorkerTask( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,    // NOLINT
    jlong runtimeManagerHandle,
    jobject runnable) {
    auto runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);

    if (runtimeManagerWrapper == nullptr) {
        return;
    }

    auto runnableRef = Valdi::makeShared<ValdiAndroid::JavaRunnable>(ValdiAndroid::JavaEnv(), runnable);
    runtimeManagerWrapper->getRuntimeManager().getWorkerQueue()->async(
        [runnableRef = std::move(runnableRef)]() { (*runnableRef)(); });
}

/*
 * Class:     snapchat_valdi_nativebridge_ValdiNativeBridge
 * Method:    performMainThreadCallback
 * Signature: (J)V
 */
void ValdiAndroid::NativeBridge::performCallback( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,  // NOLINT
    jlong callbackHandle) {
    auto* callback = reinterpret_cast<Valdi::DispatchFunction*>(callbackHandle);
    std::unique_ptr<Valdi::DispatchFunction> ptr(callback);

    (*callback)();
}

void ValdiAndroid::NativeBridge::discardCallback( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,  // NOLINT
    jlong callbackHandle) {
    auto* callback = reinterpret_cast<Valdi::DispatchFunction*>(callbackHandle);
    delete callback;
}

/*
 * Class:     com_snapchat_client_valdi_NativeBridge
 * Method:    deleteNativeHandle
 * Signature: (J)V
 */
void ValdiAndroid::NativeBridge::deleteNativeHandle( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,     // NOLINT
    jlong nativeHandle) {
    auto* value = reinterpret_cast<Valdi::Value*>(nativeHandle);
    delete value;
}

void ValdiAndroid::NativeBridge::releaseNativeRef(fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
                                                  jlong handle) {
    Valdi::unsafeBridgeRelease(reinterpret_cast<void*>(handle));
}

jint ValdiAndroid::NativeBridge::getNodeId(      // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    return static_cast<jint>(viewNode != nullptr ? viewNode->getRawId() : 0);
}

jstring ValdiAndroid::NativeBridge::getViewClassName( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,      // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return nullptr;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();
    return reinterpret_cast<jstring>(ValdiAndroid::toJavaObject(javaEnv, viewNode->getViewClassName()).releaseObject());
}

/*
 * Class:     com_snapchat_client_valdi_NativeBridge
 * Method:    registerNativeModuleFactory
 * Signature: (J)V
 */
void ValdiAndroid::NativeBridge::registerNativeModuleFactory( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,              // NOLINT
    jlong runtimeHandle,
    jobject moduleFactory) {
    auto* env = fbjni::Environment::current();
    auto javaEnv = ValdiAndroid::JavaEnv();
    auto* runtime = getRuntime(runtimeHandle);
    runtime->registerNativeModuleFactory(
        djinni_generated_client::valdi_core::NativeModuleFactory::toCpp(env, moduleFactory));
}

void ValdiAndroid::NativeBridge::registerTypeConverter( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,        // NOLINT
    jlong runtimeManagerHandle,
    jstring className,
    jstring functionPath) {
    auto javaEnv = ValdiAndroid::JavaEnv();
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);

    auto classNameCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), className);
    auto functionPathCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), functionPath);

    runtimeManagerWrapper->getRuntimeManager().registerTypeConverter(classNameCpp, functionPathCpp);
}

void ValdiAndroid::NativeBridge::registerModuleFactoriesProvider( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                  // NOLINT
    jlong runtimeManagerHandle,
    jobject moduleFactoriesProvider) {
    auto* env = fbjni::Environment::current();
    auto javaEnv = ValdiAndroid::JavaEnv();
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    runtimeManagerWrapper->getRuntimeManager().registerModuleFactoriesProvider(
        djinni_generated_client::valdi_core::NativeModuleFactoriesProvider::toCpp(env, moduleFactoriesProvider));
}

jboolean ValdiAndroid::NativeBridge::isViewNodeLayoutDirectionHorizontal( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                          // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return static_cast<jboolean>(false);
    }

    return static_cast<jboolean>(viewNode->isHorizontal());
}

jlong ValdiAndroid::NativeBridge::getViewNodePoint( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,    // NOLINT
    jlong runtimeHandle,
    jlong viewNodeHandle,
    jint x,
    jint y,
    jint mode,
    jboolean fromBoundsOrigin) {
    static constexpr jint kModeDirectionAgnosticRelative = 1;
    static constexpr jint kModeDirectionAgnosticAbsolute = 2;
    static constexpr jint kModeVisualRelative = 3;
    static constexpr jint kModeVisualAbsolute = 4;
    static constexpr jint kModeViewportRelative = 5;
    static constexpr jint kModeViewportAbsolute = 6;

    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr || runtimeWrapper == nullptr) {
        return 0;
    }

    auto pointScale = runtimeWrapper->getPointScale();

    Valdi::Point input = Valdi::Point(Valdi::pixelsToPoints(static_cast<int32_t>(x), pointScale),
                                      Valdi::pixelsToPoints(static_cast<int32_t>(y), pointScale));

    if (static_cast<bool>(fromBoundsOrigin)) {
        input = input.withOffset(viewNode->getBoundsOriginPoint());
    }

    Valdi::Point output;

    if (mode == kModeDirectionAgnosticRelative) {
        output = viewNode->convertPointToRelativeDirectionAgnostic(input);
    } else if (mode == kModeDirectionAgnosticAbsolute) {
        output = viewNode->convertPointToAbsoluteDirectionAgnostic(input);
    } else if (mode == kModeVisualRelative) {
        output = viewNode->convertSelfVisualToParentVisual(input);
    } else if (mode == kModeVisualAbsolute) {
        output = viewNode->convertSelfVisualToRootVisual(input);
    } else if (mode == kModeViewportRelative) {
        output = viewNode->getCalculatedViewport().location();
    } else if (mode == kModeViewportAbsolute) {
        auto viewportLocation = viewNode->getCalculatedViewport().location();
        output = viewNode->convertSelfVisualToRootVisual(viewportLocation);
    } else {
        ValdiAndroid::JavaEnv::getUnsafeEnv()->ThrowNew(
            ValdiAndroid::JavaEnv::getCache().getValdiExceptionClass().getClass(), "Invalid coordinate mode");
    }

    return Valdi::pointsToPackedPixels(output.x, output.y, pointScale);
}

jlong ValdiAndroid::NativeBridge::getViewNodeSize( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,   // NOLINT,
    jlong runtimeHandle,
    jlong viewNodeHandle,
    jint mode) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr || runtimeWrapper == nullptr) {
        return 0;
    }

    static constexpr jint kModeFrame = 1;
    static constexpr jint kModeViewport = 2;

    Valdi::Size output;

    if (mode == kModeFrame) {
        output = viewNode->getCalculatedFrame().size();
    } else if (mode == kModeViewport) {
        output = viewNode->getCalculatedViewport().size();
    } else {
        ValdiAndroid::JavaEnv::getUnsafeEnv()->ThrowNew(
            ValdiAndroid::JavaEnv::getCache().getValdiExceptionClass().getClass(), "Invalid coordinate mode");
    }

    auto pointScale = runtimeWrapper->getPointScale();
    return Valdi::pointsToPackedPixels(output.width, output.height, pointScale);
}

jboolean ValdiAndroid::NativeBridge::canViewNodeScroll( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,        // NOLINT
    jlong runtimeHandle,
    jlong viewNodeHandle,
    jint x,
    jint y,
    jint direction) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr || runtimeWrapper == nullptr) {
        return static_cast<jboolean>(false);
    }

    Valdi::Point directionDependentVisual =
        Valdi::Point(Valdi::pixelsToPoints(static_cast<int32_t>(x), runtimeWrapper->getPointScale()),
                     Valdi::pixelsToPoints(static_cast<int32_t>(y), runtimeWrapper->getPointScale()));

    return static_cast<jboolean>(
        viewNode->canScroll(directionDependentVisual, static_cast<Valdi::ScrollDirection>(direction)));
}

jboolean ValdiAndroid::NativeBridge::isViewNodeScrollingOrAnimating( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                     // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return 0;
    }

    return static_cast<jboolean>(viewNode->isScrollingOrAnimatingScroll());
}

jboolean ValdiAndroid::NativeBridge::invalidateLayout( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return static_cast<jboolean>(false);
    }

    if (viewNode->getViewNodeTree() == nullptr) {
        return static_cast<jboolean>(false);
    }

    bool invalidated = false;

    viewNode->getViewNodeTree()->withLock([&]() { invalidated = viewNode->invalidateMeasuredSize(); });

    return static_cast<jboolean>(invalidated);
}

jlong ValdiAndroid::NativeBridge::measureLayout( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jlong contextHandle,
    jint maxWidth,
    jint widthMode,
    jint maxHeight,
    jint heightMode,
    jboolean isRTL) {
    auto viewNodeTree = getViewNodeTree(contextHandle);
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);

    if (viewNodeTree == nullptr || runtimeWrapper == nullptr) {
        return 0;
    }

    auto pointScale = runtimeWrapper->getPointScale();

    auto resolvedWidthMode = static_cast<Valdi::MeasureMode>(widthMode);
    auto resolvedHeightMode = static_cast<Valdi::MeasureMode>(heightMode);
    auto maxWidthFloat = static_cast<float>(maxWidth) / pointScale;
    auto maxHeightFloat = static_cast<float>(maxHeight) / pointScale;

    auto layoutDirection = static_cast<bool>(isRTL) ? Valdi::LayoutDirectionRTL : Valdi::LayoutDirectionLTR;

    auto lock = viewNodeTree->lock();
    auto measuredSize = viewNodeTree->measureLayout(
        maxWidthFloat, resolvedWidthMode, maxHeightFloat, resolvedHeightMode, layoutDirection);

    return Valdi::pointsToPackedPixels(measuredSize.width, measuredSize.height, pointScale);
}

void ValdiAndroid::NativeBridge::setLayoutSpecs( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jlong contextHandle,
    jint width,
    jint height,
    jboolean isRTL) {
    auto viewNodeTree = getViewNodeTree(contextHandle);
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);

    if (viewNodeTree == nullptr || runtimeWrapper == nullptr) {
        return;
    }

    auto widthFloat = static_cast<float>(width) / runtimeWrapper->getPointScale();
    auto heightFloat = static_cast<float>(height) / runtimeWrapper->getPointScale();
    auto layoutDirection = static_cast<bool>(isRTL) ? Valdi::LayoutDirectionRTL : Valdi::LayoutDirectionLTR;

    viewNodeTree->setLayoutSpecs(Valdi::Size(widthFloat, heightFloat), layoutDirection);
}

void ValdiAndroid::NativeBridge::setVisibleViewport( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,     // NOLINT
    jlong runtimeHandle,
    jlong contextHandle,
    jint x,
    jint y,
    jint width,
    jint height,
    jboolean shouldUnset) {
    auto viewNodeTree = getViewNodeTree(contextHandle);
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);

    if (viewNodeTree == nullptr || runtimeWrapper == nullptr) {
        return;
    }

    if (static_cast<bool>(shouldUnset)) {
        viewNodeTree->setViewport(std::nullopt);
    } else {
        auto pointScale = runtimeWrapper->getPointScale();
        auto xFloat = static_cast<float>(x) / pointScale;
        auto yFloat = static_cast<float>(y) / pointScale;
        auto widthFloat = static_cast<float>(width) / pointScale;
        auto heightFloat = static_cast<float>(height) / pointScale;

        viewNodeTree->setViewport({Valdi::Frame(xFloat, yFloat, widthFloat, heightFloat)});
    }
}

void ValdiAndroid::NativeBridge::callOnJsThread( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jboolean sync,
    jobject runnable) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);

    if (runtimeWrapper == nullptr) {
        return;
    }

    auto runnableRef = Valdi::makeShared<ValdiAndroid::JavaRunnable>(ValdiAndroid::JavaEnv(), runnable);

    runtimeWrapper->getRuntime().getJavaScriptRuntime()->dispatchOnJsThread(
        nullptr,
        (sync != 0u) ? Valdi::JavaScriptTaskScheduleTypeAlwaysSync : Valdi::JavaScriptTaskScheduleTypeDefault,
        0,
        [runnableRef = std::move(runnableRef)](auto& /*jsEntry*/) { (*runnableRef)(); });
}

void ValdiAndroid::NativeBridge::enqueueLoadOperation( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong runtimeManagerHandle,
    jobject runnable) {
    auto* runtimeManager = getRuntimeManagerWrapper(runtimeManagerHandle);

    if (runtimeManager == nullptr) {
        return;
    }

    auto runnableRef = Valdi::makeShared<ValdiAndroid::JavaRunnable>(ValdiAndroid::JavaEnv(), runnable);

    runtimeManager->getRuntimeManager().enqueueLoadOperation(
        [runnableRef = std::move(runnableRef)]() { (*runnableRef)(); });
}

void ValdiAndroid::NativeBridge::flushLoadOperations( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,      // NOLINT
    jlong runtimeManagerHandle) {
    auto* runtimeManager = getRuntimeManagerWrapper(runtimeManagerHandle);

    if (runtimeManager == nullptr) {
        return;
    }

    runtimeManager->getRuntimeManager().flushLoadOperations();
}

void ValdiAndroid::NativeBridge::callSyncWithJsThread( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong runtimeHandle,
    jobject runnable) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);

    if (runtimeWrapper == nullptr) {
        return;
    }

    auto runnableRef = Valdi::makeShared<ValdiAndroid::JavaRunnable>(ValdiAndroid::JavaEnv(), runnable);

    if (runtimeWrapper->getRuntime().getMainThreadManager().currentThreadIsMainThread()) {
        auto batch = runtimeWrapper->getRuntime().getMainThreadManager().scopedBatch();
        (*runnableRef)();
    } else {
        auto runtime = runtimeWrapper->getSharedRuntime();
        runtimeWrapper->getRuntime().getMainThreadManager().dispatch(
            nullptr, [runnableRef = std::move(runnableRef), runtime = std::move(runtime)]() {
                auto batch = runtime->getMainThreadManager().scopedBatch();
                (*runnableRef)();
            });
    }
}

enum ScrollEventType {
    ScrollEventTypeOnScroll = 1,
    ScrollEventTypeOnScrollEnd = 2,
    ScrollEventTypeOnDragStart = 3,
    ScrollEventTypeOnDragEnding = 4,
};

static jlong convertPointToPackedPixels(const Valdi::Point& point, float pointScale) {
    return Valdi::pointsToPackedPixels(point.x, point.y, pointScale);
}

jlong ValdiAndroid::NativeBridge::notifyScroll(  // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    jlong viewNodeHandle,
    jint eventType,
    jint pixelDirectionDependentContentOffsetX,
    jint pixelDirectionDependentContentOffsetY,
    jint pixelDirectionDependentUnclampedContentOffsetX,
    jint pixelDirectionDependentUnclampedContentOffsetY,
    jfloat pixelDirectionDependentInvertedVelocityX,
    jfloat pixelDirectionDependentInvertedVelocityY) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    jlong out = std::numeric_limits<jlong>::min();

    if (runtimeWrapper == nullptr || viewNode == nullptr) {
        return out;
    }

    auto* viewNodeTree = viewNode->getViewNodeTree();

    if (viewNodeTree == nullptr) {
        // TODO: is this a valid state?
        return 0;
    }

    viewNodeTree->withLock([&]() {
        auto pointScale = runtimeWrapper->getPointScale();

        auto pointDirectionDependentContentOffset =
            Valdi::Point::fromPixels(static_cast<int32_t>(pixelDirectionDependentContentOffsetX),
                                     static_cast<int32_t>(pixelDirectionDependentContentOffsetY),
                                     pointScale);

        auto pointDirectionDependentUnclampedContentOffset =
            Valdi::Point::fromPixels(static_cast<int32_t>(pixelDirectionDependentUnclampedContentOffsetX),
                                     static_cast<int32_t>(pixelDirectionDependentUnclampedContentOffsetY),
                                     pointScale);

        auto pointDirectionDependentVelocityX =
            -Valdi::pixelsToPoints(static_cast<float>(pixelDirectionDependentInvertedVelocityX), pointScale);
        auto pointDirectionDependentVelocityY =
            -Valdi::pixelsToPoints(static_cast<float>(pixelDirectionDependentInvertedVelocityY), pointScale);
        auto pointDirectionDependentVelocity =
            Valdi::Point(pointDirectionDependentVelocityX, pointDirectionDependentVelocityY);

        switch (static_cast<ScrollEventType>(eventType)) {
            case ScrollEventTypeOnScroll: {
                auto adjustedDirectionDependentContentOffset =
                    viewNode->onScroll(pointDirectionDependentContentOffset,
                                       pointDirectionDependentUnclampedContentOffset,
                                       pointDirectionDependentVelocity);

                if (adjustedDirectionDependentContentOffset) {
                    out = convertPointToPackedPixels(adjustedDirectionDependentContentOffset.value(), pointScale);
                }
            } break;

            case ScrollEventTypeOnScrollEnd:
                viewNode->onScrollEnd(pointDirectionDependentContentOffset,
                                      pointDirectionDependentUnclampedContentOffset);
                break;
            case ScrollEventTypeOnDragStart:
                viewNode->onDragStart(pointDirectionDependentContentOffset,
                                      pointDirectionDependentUnclampedContentOffset,
                                      pointDirectionDependentVelocity);
                break;
            case ScrollEventTypeOnDragEnding: {
                auto adjustedPointDirectionDependentContentOffset =
                    viewNode->onDragEnding(pointDirectionDependentContentOffset,
                                           pointDirectionDependentUnclampedContentOffset,
                                           pointDirectionDependentVelocity);
                if (adjustedPointDirectionDependentContentOffset) {
                    out = convertPointToPackedPixels(adjustedPointDirectionDependentContentOffset.value(), pointScale);
                }
            } break;
            default:
                ValdiAndroid::JavaEnv::getUnsafeEnv()->ThrowNew(
                    ValdiAndroid::JavaEnv::getCache().getValdiExceptionClass().getClass(), "Invalid scroll event type");
        }
    });

    return out;
}

void ValdiAndroid::NativeBridge::unloadAllJsModules( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,     // NOLINT
    jlong runtimeHandle) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);

    if (runtimeWrapper == nullptr) {
        return;
    }

    runtimeWrapper->getRuntime().getJavaScriptRuntime()->unloadAllModules();
}

void ValdiAndroid::NativeBridge::clearViewPools( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeManagerHandle) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);

    if (wrapper == nullptr) {
        return;
    }

    wrapper->getRuntimeManager().clearViewPools();
}

void ValdiAndroid::NativeBridge::applicationSetConfiguration( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,              // NOLINT
    jlong runtimeManagerHandle,
    jfloat dynamicTypeScale) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    wrapper->setDynamicTypeScale(dynamicTypeScale);
}

void ValdiAndroid::NativeBridge::setPointScale(  // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeManagerHandle,
    jfloat pointScale) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    wrapper->setPointScale(pointScale);
}

void ValdiAndroid::NativeBridge::applicationDidResume( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong runtimeManagerHandle) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    wrapper->applicationDidResume();
}

void ValdiAndroid::NativeBridge::applicationWillPause( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong runtimeManagerHandle) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }
    wrapper->applicationWillPause();
}

void ValdiAndroid::NativeBridge::applicationIsInLowMemory( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,           // NOLINT
    jlong runtimeManagerHandle) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }
    wrapper->applicationIsInLowMemory();
}

jstring ValdiAndroid::NativeBridge::getViewNodeDebugDescription( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                 // NOLINT
    jlong viewNodeHandle) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<jstring>(
        ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), viewNode->toXML()).releaseObject());
}

void ValdiAndroid::NativeBridge::valueChangedForAttribute( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,           // NOLINT
    jlong runtimeHandle,
    jlong viewNodeHandle,
    jlong attributeNamePtr,
    jobject value) {
    auto* runtime = getRuntime(runtimeHandle);
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr || runtime == nullptr) {
        return;
    }
    auto javaEnv = ValdiAndroid::JavaEnv();

    auto attributeName = ValdiAndroid::unwrapInternedString(attributeNamePtr);
    auto attributeValue = ValdiAndroid::toValue(javaEnv, ValdiAndroid::JavaEnv::newLocalRef(value));

    runtime->updateAttributeState(*viewNode, attributeName, attributeValue);
}

void ValdiAndroid::NativeBridge::setValueForAttribute( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,       // NOLINT
    jlong viewNodeHandle,
    jstring attribute,
    jobject value,
    jboolean keepAsOverride) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();

    auto attributeName = ValdiAndroid::toInternedString(javaEnv, attribute);
    auto attributeValue = ValdiAndroid::toValue(javaEnv, ValdiAndroid::JavaEnv::newLocalRef(value));

    auto attributeId = viewNode->getAttributeIds().getIdForName(attributeName);

    auto* viewNodeTree = viewNode->getViewNodeTree();

    Valdi::AttributeOwner* attributeOwner;
    if (static_cast<bool>(keepAsOverride)) {
        attributeOwner = Valdi::AttributeOwner::getNativeOverridenAttributeOwner();
    } else {
        attributeOwner = viewNodeTree;
    }

    if (viewNodeTree == nullptr) {
        // TODO: is this a valid state?
        // should we still call setAttribute, just without flush (since, presumably, this ViewNode wouldn't have a View
        // to flush to?)
    } else {
        viewNodeTree->withLock([&]() {
            auto& viewTransactionScope = viewNodeTree->getCurrentViewTransactionScope();
            viewNode->setAttribute(viewTransactionScope, attributeId, attributeOwner, attributeValue, nullptr);
            viewNode->getAttributesApplier().flush(viewTransactionScope);

            // TODO: does onAttributeChange have to be under the getViewNodeTree lock?
            if (!static_cast<bool>(keepAsOverride)) {
                viewNodeTree->getContext()->onAttributeChange(
                    viewNode->getRawId(), attributeName, attributeValue, false);
            }
        });
    }
}

void ValdiAndroid::NativeBridge::notifyApplyAttributeFailed(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                                            jlong viewNodeHandle,
                                                            jint attributeId,
                                                            jstring errorMessage) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return;
    }

    auto errorMessageCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), errorMessage);

    viewNode->notifyAttributeFailed(static_cast<Valdi::AttributeId>(attributeId), Valdi::Error(errorMessageCpp));
}

jobject ValdiAndroid::NativeBridge::getValueForAttribute( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,          // NOLINT
    jlong viewNodeHandle,
    jstring attribute) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return nullptr;
    }

    auto attributeName = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), attribute);
    auto attributeId = viewNode->getAttributeIds().getIdForName(attributeName);
    auto value = viewNode->getAttributesApplier().getResolvedAttributeValue(attributeId);
    if (value.isNullOrUndefined()) {
        return nullptr;
    }

    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), value).releaseObject();
}

void ValdiAndroid::NativeBridge::reapplyAttribute( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,   // NOLINT
    jlong viewNodeHandle,
    jstring attribute) {
    auto viewNode = getViewNode(viewNodeHandle);
    if (viewNode == nullptr) {
        return;
    }

    auto attributeName = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), attribute);
    auto attributeId = viewNode->getAttributeIds().getIdForName(attributeName);

    auto* viewNodeTree = viewNode->getViewNodeTree();

    if (viewNodeTree == nullptr) {
        // TODO: is this a valid state?
        // should we still call reapplyAttribute, just without flush (since, presumably, this ViewNode wouldn't have a
        // View to flush to?)
    } else {
        viewNodeTree->withLock([&]() {
            auto& viewTransactionScope = viewNodeTree->getCurrentViewTransactionScope();
            viewNode->getAttributesApplier().reapplyAttribute(viewTransactionScope, attributeId);
            viewNode->getAttributesApplier().flush(viewTransactionScope);
        });
    }
}

void ValdiAndroid::NativeBridge::setRuntimeAttachedObject( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,           // NOLINT
    jlong runtimeHandle,
    jobject object) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return;
    }

    auto* runtimeListener = dynamic_cast<ValdiAndroid::RuntimeListener*>(runtime->getListener().get());
    if (object == nullptr) {
        runtimeListener->setAttachedObject(ValdiAndroid::GlobalRefJavaObjectBase());
    } else {
        auto javaEnv = ValdiAndroid::JavaEnv();

        runtimeListener->setAttachedObject(ValdiAndroid::GlobalRefJavaObjectBase(javaEnv, object, "Runtime"));
    }
}

jobject ValdiAndroid::NativeBridge::getRuntimeAttachedObject( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,              // NOLINT
    jlong runtimeHandle) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return nullptr;
    }
    return sGetRuntimeAttachedObject(runtime).release();
}

jobject ValdiAndroid::NativeBridge::getRuntimeAttachedObjectFromContext( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                         // NOLINT
    jlong contextHandle) {
    auto context = getContext(contextHandle);
    if (context == nullptr || context->getRuntime() == nullptr) {
        return nullptr;
    }

    return sGetRuntimeAttachedObject(context->getRuntime()).release();
}

jobject ValdiAndroid::NativeBridge::getCurrentContext(
    fbjni::alias_ref<fbjni::JClass> /* clazz */) { // NOLINT(performance-unnecessary-value-param)
    auto currentContext = Valdi::Context::current();
    if (currentContext == nullptr) {
        return nullptr;
    }

    auto javaContext = ValdiAndroid::fromValdiContextUserData(currentContext->getUserData());
    return javaContext.releaseObject();
}

jobject ValdiAndroid::NativeBridge::getAllRuntimeAttachedObjects( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                  // NOLINT
    jlong runtimeManagerHandle) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return nullptr;
    }
    auto javaEnv = ValdiAndroid::JavaEnv();

    auto runtimes = wrapper->getRuntimeManager().getAllRuntimes();
    auto result = Valdi::ValueArray::make(runtimes.size());
    size_t i = 0;
    for (const auto& runtime : wrapper->getRuntimeManager().getAllRuntimes()) {
        auto attachedObject = sGetRuntimeAttachedObject(runtime.get());
        if (attachedObject == nullptr) {
            continue;
        }
        result->emplace(i++, ValdiAndroid::toValue(javaEnv, std::move(attachedObject)));
    }

    auto javaArray = ValdiAndroid::toJavaObject(javaEnv, result);
    return javaArray.releaseObject();
}

void ValdiAndroid::NativeBridge::preloadViews(   // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeManagerHandle,
    jstring className,
    jint count) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }
    auto cppClassName = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), className);

    wrapper->getAndroidViewManagerContext()->preloadViews(cppClassName, static_cast<size_t>(count));
}

jobject ValdiAndroid::NativeBridge::captureJavaScriptStackTraces( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,                  // NOLINT
    jlong runtimeHandle,
    jlong timeoutMs) {
    Valdi::Ref<Valdi::ValueArray> retval;

    auto runtime = getRuntime(runtimeHandle);
    if (runtime != nullptr) {
        // TODO(rjaber): Move to better duration
        const std::vector<Valdi::JavaScriptCapturedStacktrace> threadStacks =
            runtime->getJavaScriptRuntime()->captureStackTraces(std::chrono::steady_clock::duration(timeoutMs * 1000));
        retval = Valdi::ValueArray::make(threadStacks.size());
        for (size_t i = 0; i < threadStacks.size(); i++) { // NOLINT
            auto currentStack = threadStacks[i];
            auto threadStackPacked = Valdi::ValueArray::make(2);
            threadStackPacked->emplace(0, Valdi::Value(currentStack.getStackTrace()));
            threadStackPacked->emplace(1, Valdi::Value(static_cast<int32_t>(currentStack.getStatus())));
            retval->emplace(i, Valdi::Value(threadStackPacked));
        }
    }

    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), retval).releaseObject();
}

/**
 * Java->C++ entry point for getting all hashes of loaded modules.
 *
 * Parameters:
 * - clazz: The Java class reference (unused).
 * - runtimeHandle: The java handle to the runtime.
 *
 * Returns:
 * - A jobject representing a Java array of bundle names and their corresponding hash.
 *
 * Notes:
 * - Iterates over all loaded bundle names and retrieves their hash.
 * - The result is converted to a jobject. As a double nested array [[bundleName, hash]].
 */
jobject ValdiAndroid::NativeBridge::getAllModuleHashes( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,        // NOLINT
    jlong runtimeHandle) {
    Valdi::Ref<Valdi::ValueArray> retval;

    auto runtime = getRuntime(runtimeHandle);
    if (runtime != nullptr) {
        auto bundles = runtime->getResourceManager().getAllInitializedBundles();
        Valdi::ValueArrayBuilder builder;
        for (const auto& [bundleName, bundle] : bundles) {
            auto hash = bundle->getHash();
            if (hash) {
                auto bundleNameAndHash = Valdi::ValueArray::make(2);
                bundleNameAndHash->emplace(0, Valdi::Value(bundleName));
                bundleNameAndHash->emplace(1, Valdi::Value(hash.moveValue()));
                builder.append(Valdi::Value(bundleNameAndHash));
            }
        }
        retval = builder.build();
    }
    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), retval).releaseObject();
}

jobject ValdiAndroid::NativeBridge::dumpMemoryStatistics( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,          // NOLINT
    jlong runtimeManagerHandle) {
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    auto retval = Valdi::ValueArray::make(2);
    if (runtimeManagerWrapper != nullptr) {
        Valdi::JavaScriptContextMemoryStatistics stats =
            runtimeManagerWrapper->getRuntimeManager().dumpMemoryStatistics();

        retval->emplace(0, Valdi::Value(static_cast<int64_t>(stats.memoryUsageBytes)));
        retval->emplace(1, Valdi::Value(static_cast<int64_t>(stats.objectsCount)));
    } else {
        retval->emplace(0, Valdi::Value(static_cast<int64_t>(-1)));
        retval->emplace(1, Valdi::Value(static_cast<int64_t>(-1)));
    }

    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), retval).releaseObject();
}

void ValdiAndroid::NativeBridge::forceBindAttributes( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,      // NOLINT
    jlong runtimeManagerHandle,
    jstring className) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }
    auto cppClassName = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), className);

    wrapper->getRuntimeManager().forceBindAttributes(cppClassName);
}

static Valdi::Ref<ValdiAndroid::AttributesBindingContextWrapper> getBindingContextWrapper(jlong bindingContextHandle) {
    return Valdi::unsafeBridge<ValdiAndroid::AttributesBindingContextWrapper>(
        reinterpret_cast<void*>(bindingContextHandle));
}

jint ValdiAndroid::NativeBridge::bindAttribute(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                               jlong bindingContextHandle,
                                               jint type,
                                               jstring name,
                                               jboolean invalidateLayoutOnChange,
                                               jobject delegate,
                                               jobject compositeParts) {
    auto wrapper = getBindingContextWrapper(bindingContextHandle);
    return wrapper->bindAttributes(type, name, invalidateLayoutOnChange, delegate, compositeParts);
}

void ValdiAndroid::NativeBridge::bindScrollAttributes(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                                      jlong bindingContextHandle) {
    auto wrapper = getBindingContextWrapper(bindingContextHandle);
    wrapper->bindScrollAttributes();
}

void ValdiAndroid::NativeBridge::bindAssetAttributes(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                                     jlong bindingContextHandle,
                                                     jint outputType) {
    auto wrapper = getBindingContextWrapper(bindingContextHandle);
    wrapper->bindAssetAttributes(static_cast<snap::valdi_core::AssetOutputType>(outputType));
}

void ValdiAndroid::NativeBridge::setMeasureDelegate(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                                    jlong bindingContextHandle,
                                                    jobject measureDelegate) {
    auto wrapper = getBindingContextWrapper(bindingContextHandle);
    wrapper->setMeasureDelegate(measureDelegate);
}

void ValdiAndroid::NativeBridge::setPlaceholderViewMeasureDelegate(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                                                   jlong bindingContextHandle,
                                                                   jobject legacyMeasureDelegate) {
    auto wrapper = getBindingContextWrapper(bindingContextHandle);
    wrapper->setPlaceholderViewMeasureDelegate(legacyMeasureDelegate);
}

void ValdiAndroid::NativeBridge::registerAttributePreprocessor(fbjni::alias_ref<fbjni::JClass> clazz, // NOLINT
                                                               jlong bindingContextHandle,
                                                               jstring attributeName,
                                                               jboolean enableCache,
                                                               jobject preprocessor) {
    auto wrapper = getBindingContextWrapper(bindingContextHandle);
    wrapper->registerAttributePreprocessor(attributeName, enableCache, preprocessor);
}

void ValdiAndroid::NativeBridge::setUserSession( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeManagerHandle,
    jstring userId) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return;
    }

    auto cppUserId = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), userId);

    wrapper->getRuntimeManager().setUserSession(cppUserId);
}

jobject ValdiAndroid::NativeBridge::getJSRuntime( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,  // NOLINT
    jlong runtimeHandle) {
    auto* env = fbjni::Environment::current();
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return nullptr;
    }

    return djinni_generated_client::valdi_core::NativeJSRuntime::fromCpp(
               env, Valdi::strongRef(runtime->getJavaScriptRuntime()))
        .release();
}

void ValdiAndroid::NativeBridge::performGcNow(   // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return;
    }

    runtime->getJavaScriptRuntime()->performGc();
}

void ValdiAndroid::NativeBridge::setKeepViewAliveOnDestroy( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,            // NOLINT
    jlong contextHandle,
    jboolean keepViewAliveOnDestroy) {
    auto viewNodeTree = getViewNodeTree(contextHandle);
    if (viewNodeTree != nullptr) {
        viewNodeTree->setKeepViewAliveOnDestroy(static_cast<bool>(keepViewAliveOnDestroy));
    }
}

jlong ValdiAndroid::NativeBridge::createViewFactory( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,     // NOLINT
    jlong runtimeManagerHandle,
    jstring viewClassName,
    jobject viewFactory,
    jboolean hasBindAttributes) {
    auto* runtimeManagerWrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (runtimeManagerWrapper == nullptr) {
        return 0;
    }

    const auto& viewManagerContext = runtimeManagerWrapper->getAndroidViewManagerContext();
    auto& androidViewManager = runtimeManagerWrapper->getAndroidViewManager();

    auto javaEnv = ValdiAndroid::JavaEnv();

    auto cppViewClassName = ValdiAndroid::toInternedString(javaEnv, viewClassName);

    auto viewFactoryObject = ValdiAndroid::GlobalRefJavaObjectBase(javaEnv, viewFactory, "ViewFactory");

    auto attributes = viewManagerContext->getAttributesManager().getAttributesForClass(cppViewClassName);

    if (hasBindAttributes == JNI_TRUE) {
        Valdi::AttributesBindingContextImpl bindingContext(viewManagerContext->getAttributesManager().getAttributeIds(),
                                                           viewManagerContext->getAttributesManager().getColorPalette(),
                                                           runtimeManagerWrapper->getRuntimeManager().getLogger());

        auto wrapper = Valdi::makeShared<AttributesBindingContextWrapper>(androidViewManager, bindingContext);
        auto ptr = reinterpret_cast<int64_t>(Valdi::unsafeBridgeCast(wrapper.get()));

        ValdiAndroid::JavaEnv::getCache().getViewFactoryBindAttributesMethod().call(viewFactoryObject.toObject(), ptr);

        attributes = attributes->merge(cppViewClassName,
                                       bindingContext.getHandlers(),
                                       bindingContext.getDefaultDelegate(),
                                       bindingContext.getMeasureDelegate(),
                                       bindingContext.scrollAttributesBound(),
                                       true);
    }

    auto viewFactoryCpp = Valdi::makeShared<AndroidViewFactory>(
        std::move(viewFactoryObject), cppViewClassName, androidViewManager, attributes, true);

    auto* value = new Valdi::Value(viewFactoryCpp);
    return reinterpret_cast<jlong>(value);
}

void ValdiAndroid::NativeBridge::registerLocalViewFactory( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,           // NOLINT
    jlong contextHandle,
    jlong viewFactoryHandle) {
    auto viewNodeTree = getViewNodeTree(contextHandle);
    if (viewNodeTree == nullptr) {
        return;
    }

    auto* value = reinterpret_cast<Valdi::Value*>(viewFactoryHandle);
    if (value == nullptr) {
        return;
    }

    auto viewFactory = Valdi::castOrNull<Valdi::ViewFactory>(value->getValdiObject());

    viewNodeTree->registerLocalViewFactory(viewFactory->getViewClassName(), viewFactory);
}

jstring ValdiAndroid::NativeBridge::dumpLogMetadata( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,     // NOLINT
    jlong runtimeHandle,
    jboolean isCrashing) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return nullptr;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();
    auto logMetadata = runtime->dumpLogs(true, false, static_cast<bool>(isCrashing)).serializeMetadata();

    return reinterpret_cast<jstring>(ValdiAndroid::toJavaObject(javaEnv, logMetadata).releaseObject());
}

jstring ValdiAndroid::NativeBridge::dumpLogs(    // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return nullptr;
    }

    auto javaEnv = ValdiAndroid::JavaEnv();
    auto logs = runtime->dumpLogs(false, true, false);

    auto stats = IndirectJavaGlobalRef::dumpStats();

    Valdi::ValueArrayBuilder builder;
    builder.reserve(stats.activeReferencesByTag.size());

    for (const auto& it : stats.activeReferencesByTag) {
        auto tag = it.first == nullptr ? std::string_view("<null>") : std::string_view(it.first);
        builder.append(Valdi::Value(STRING_FORMAT("{} = {}", tag, it.second)));
    }

    logs.appendVerbose(
        STRING_LITERAL("JNI Global Refs"),
        Valdi::Value()
            .setMapValue("tableSize", Valdi::Value(static_cast<int32_t>(stats.tableSize)))
            .setMapValue("activeReferencesCount", Valdi::Value(static_cast<int32_t>(stats.activeReferencesCount)))
            .setMapValue("references", Valdi::Value(builder.build())));

    auto verboseLogs = logs.verboseLogsToString();

    return reinterpret_cast<jstring>(ValdiAndroid::toJavaObject(javaEnv, verboseLogs).releaseObject());
}

jobject ValdiAndroid::NativeBridge::getAllContexts( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */,    // NOLINT
    jlong runtimeHandle) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return nullptr;
    }

    auto contexts = runtime->getContextManager().getAllContexts();
    Valdi::ValueArrayBuilder builder;
    builder.reserve(contexts.size());
    for (const auto& context : contexts) {
        auto userData = context->getUserData();
        if (userData != nullptr) {
            builder.append(Valdi::Value(userData));
        }
    }

    return ValdiAndroid::toJavaObject(ValdiAndroid::JavaEnv(), builder.build()).releaseObject();
}

jobject ValdiAndroid::NativeBridge::getAsset(  // NOLINT
    fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
    jlong runtimeHandle,
    jstring moduleName,
    jstring pathOrUrl) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    if (runtimeWrapper == nullptr) {
        return nullptr;
    }

    const auto& assetsManager = runtimeWrapper->getRuntime().getResourceManager().getAssetsManager();

    auto pathCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), pathOrUrl);

    Valdi::Ref<Valdi::Asset> asset;
    if (moduleName != nullptr) {
        auto moduleNameCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), moduleName);
        auto bundle = runtimeWrapper->getRuntime().getResourceManager().getBundle(moduleNameCpp);
        asset = assetsManager->getAsset(Valdi::AssetKey(bundle, pathCpp));
    } else {
        asset = assetsManager->getAsset(Valdi::AssetKey(pathCpp));
    }

    return djinni_generated_client::valdi_core::NativeAsset::fromCpp(ValdiAndroid::JavaEnv::getUnsafeEnv(),
                                                                     asset.toShared())
        .release();
}

jobject ValdiAndroid::NativeBridge::getModuleEntry( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /*clazz*/,      // NOLINT
    jlong runtimeHandle,
    jstring moduleName,
    jstring path,
    jboolean saveToDisk) {
    auto* runtimeWrapper = getRuntimeWrapper(runtimeHandle);
    if (runtimeWrapper == nullptr) {
        return nullptr;
    }

    auto moduleNameCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), moduleName);
    auto pathCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), path);
    auto bundle = runtimeWrapper->getRuntime().getResourceManager().getBundle(moduleNameCpp);
    auto entry = bundle->getEntry(pathCpp);
    if (!entry) {
        return nullptr;
    }

    if (saveToDisk != 0) {
        Valdi::Path path(".valdi_extracted_module");
        path.append(moduleNameCpp.toStringView()).append(pathCpp.toStringView());
        const auto& diskCache = runtimeWrapper->getRuntime().getDiskCache();
        auto result = diskCache->store(path, entry.value());
        if (!result) {
            ValdiAndroid::throwJavaValdiException(fbjni::Environment::current(), result.error());
            return nullptr;
        }

        auto filePath = diskCache->getAbsoluteURL(path).substring(std::string_view("file://").length());
        return newJavaStringUTF8(filePath.toStringView()).release();
    } else {
        return newJavaByteArray(entry.value().data(), entry.value().size()).release();
    }
}

// NOLINTNEXTLINE
jdouble ValdiAndroid::NativeBridge::getCurrentEventTime(fbjni::alias_ref<fbjni::JClass> /*clazz*/) {
    return static_cast<jdouble>(Valdi::TimePoint::now().getTime());
}

void ValdiAndroid::NativeBridge::startProfiling( // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle) {
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return;
    }
    runtime->getJavaScriptRuntime()->startProfiling();
}

void ValdiAndroid::NativeBridge::stopProfiling(  // NOLINT
    fbjni::alias_ref<fbjni::JClass> /* clazz */, // NOLINT
    jlong runtimeHandle,
    fbjni::alias_ref<JValdiFunction> completion) { // NOLINT
    auto* runtime = getRuntime(runtimeHandle);
    if (runtime == nullptr) {
        return;
    }
    auto* env = fbjni::Environment::current();
    auto javaEnv = ValdiAndroid::JavaEnv();
    auto completionConverted = ValdiAndroid::toValue(javaEnv, ValdiAndroid::JavaEnv::newLocalRef(completion.get()));

    if (completionConverted.getFunction() == nullptr) {
        ValdiAndroid::throwJavaValdiException(env, "completion should be a function");
        return;
    }

    runtime->getJavaScriptRuntime()->stopProfiling([=](const auto& result) {
        Valdi::Value content;
        Valdi::Value error;
        if (result) {
            Valdi::ValueArrayBuilder builder;
            for (auto const& item : result.value()) {
                builder.append(Valdi::Value(item));
            }
            content = Valdi::Value(builder.build());
        } else {
            content = Valdi::Value(Valdi::ValueArray::make(0));
            error = Valdi::Value(result.error().toStringBox());
        }
        (*completionConverted.getFunction())({content, error});
    });
}

jobject ValdiAndroid::NativeBridge::wrapAndroidBitmap(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                      jobject bitmap) {
    auto androidBitmap = Valdi::makeShared<AndroidBitmap>(JavaObject(JavaEnv(), bitmap));

    return newJavaObjectWrappingValue(JavaEnv(), Valdi::Value(androidBitmap)).releaseObject();
}

#ifdef SNAP_DRAWING_ENABLED

static Valdi::Ref<ValdiAndroid::SnapDrawingLayerRootHost> getSnapDrawingRoot(jlong snapDrawingRootHandle) {
    return Valdi::unsafeBridge<ValdiAndroid::SnapDrawingLayerRootHost>(reinterpret_cast<void*>(snapDrawingRootHandle));
}

static Valdi::Ref<ValdiAndroid::AndroidSnapDrawingRuntime> getSnapDrawingRuntime(jlong snapDrawingRuntimeHandle) {
    return Valdi::unsafeBridge<ValdiAndroid::AndroidSnapDrawingRuntime>(
        reinterpret_cast<void*>(snapDrawingRuntimeHandle));
}

void ValdiAndroid::NativeBridge::setSnapDrawingRootView(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                        jlong contextHandle,
                                                        jlong snapDrawingRootHandle,
                                                        jlong previousSnapDrawingRootHandle,
                                                        jobject hostView) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);
    auto previousHostCpp = getSnapDrawingRoot(previousSnapDrawingRootHandle);
    if (previousHostCpp != nullptr) {
        previousHostCpp->setContentLayerWithHostView(nullptr);
    }

    if (hostCpp != nullptr) {
        doSetRootViewToContext(contextHandle, hostCpp->setContentLayerWithHostView(hostView));
    } else {
        doSetRootViewToContext(contextHandle, nullptr);
    }
}

jlong ValdiAndroid::NativeBridge::getSnapDrawingRuntimeHandle(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                              jlong runtimeManagerHandle) {
    auto* wrapper = getRuntimeManagerWrapper(runtimeManagerHandle);
    if (wrapper == nullptr) {
        return 0;
    }

    return static_cast<jlong>(
        reinterpret_cast<std::uintptr_t>(Valdi::unsafeBridgeRetain(wrapper->getOrCreateSnapDrawingRuntime().get())));
}

jlong ValdiAndroid::NativeBridge::createSnapDrawingRoot(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                        jlong snapDrawingRuntimeHandle,
                                                        jboolean disallowSynchronousDraw) {
    auto snapDrawingRuntime = getSnapDrawingRuntime(snapDrawingRuntimeHandle);
    if (snapDrawingRuntime == nullptr) {
        return 0;
    }

    auto host = snapDrawingRuntime->createHost(disallowSynchronousDraw != 0);

    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(Valdi::unsafeBridgeRetain(host.get())));
}

jboolean ValdiAndroid::NativeBridge::dispatchSnapDrawingTouchEvent(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
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
                                                                   jobject motionEvent) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);

    auto source = Valdi::makeShared<GlobalRefJavaObject>(JavaEnv(), motionEvent, "MotionEvent", false);

    auto handled = hostCpp->dispatchTouchEvent(static_cast<snap::drawing::TouchEventType>(type),
                                               static_cast<int64_t>(eventTimeNanos),
                                               x,
                                               y,
                                               dx,
                                               dy,
                                               pointerCount,
                                               actionIndex,
                                               pointerLocations,
                                               source);

    return static_cast<jboolean>(handled ? 1 : 0);
}

void ValdiAndroid::NativeBridge::snapDrawingLayout(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                   jlong snapDrawingRootHandle,
                                                   jint width,
                                                   jint height) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);
    hostCpp->setSize(width, height);
}

void ValdiAndroid::NativeBridge::snapDrawingDrawInBitmap(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                         jlong snapDrawingRootHandle,
                                                         jint surfacePresenterId,
                                                         jobject bitmapHandler,
                                                         jboolean isOwned) {
    auto androidBitmap = Valdi::makeShared<AndroidBitmapHandler>(JavaEnv(), bitmapHandler, isOwned != 0);

    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);
    hostCpp->drawInBitmap(static_cast<snap::drawing::SurfacePresenterId>(surfacePresenterId), androidBitmap);
}

void ValdiAndroid::NativeBridge::snapDrawingSetSurface(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                       jlong snapDrawingRootHandle,
                                                       jint surfacePresenterId,
                                                       jobject surface) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);

    auto cppSurfacePresenterId = static_cast<snap::drawing::SurfacePresenterId>(surfacePresenterId);

    if (surface == nullptr) {
        hostCpp->setSurface(cppSurfacePresenterId, nullptr);
    } else {
        auto* nativeWindow = ANativeWindow_fromSurface(JavaEnv::getUnsafeEnv(), surface);
        hostCpp->setSurface(cppSurfacePresenterId, nativeWindow);

        if (nativeWindow != nullptr) {
            ANativeWindow_release(nativeWindow);
        }
    }
}

void ValdiAndroid::NativeBridge::snapDrawingSetSurfaceNeedsRedraw(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                                  jlong snapDrawingRootHandle,
                                                                  jint surfacePresenterId) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);
    auto cppSurfacePresenterId = static_cast<snap::drawing::SurfacePresenterId>(surfacePresenterId);
    hostCpp->setSurfaceNeedsRedraw(cppSurfacePresenterId);
}

void ValdiAndroid::NativeBridge::snapDrawingOnSurfaceSizeChanged(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                                 jlong snapDrawingRootHandle,
                                                                 jint surfacePresenterId) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);
    auto cppSurfacePresenterId = static_cast<snap::drawing::SurfacePresenterId>(surfacePresenterId);
    hostCpp->onSurfaceSizeChanged(cppSurfacePresenterId);
}

static void onRegisterTypefaceError(const Valdi::StringBox& familyName, const Valdi::Error& error) {
    ValdiAndroid::throwJavaValdiException(ValdiAndroid::JavaEnv::getUnsafeEnv(),
                                          error.rethrow(STRING_FORMAT("Failed to register font '{}'", familyName)));
}

void ValdiAndroid::NativeBridge::snapDrawingRegisterTypeface(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                             jlong snapDrawingRuntimeHandle,
                                                             jstring familyName,
                                                             jstring fontWeight,
                                                             jstring fontStyle,
                                                             jboolean isFallback,
                                                             jbyteArray typefaceBytes,
                                                             jint typefaceFd) {
    auto snapDrawingRuntime = getSnapDrawingRuntime(snapDrawingRuntimeHandle);
    if (snapDrawingRuntime == nullptr) {
        return;
    }

    auto familyNameCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), familyName);
    auto fontWeightCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), fontWeight);
    auto fontSlantCpp = ValdiAndroid::toInternedString(ValdiAndroid::JavaEnv(), fontStyle);

    auto fontWeightResult = snap::drawing::FontStyle::parseWeight(fontWeightCpp.toStringView());
    if (!fontWeightResult) {
        onRegisterTypefaceError(familyNameCpp, fontWeightResult.error());
        return;
    }
    auto fontSlantResult = snap::drawing::FontStyle::parseSlant(fontSlantCpp.toStringView());
    if (!fontSlantResult) {
        onRegisterTypefaceError(familyNameCpp, fontSlantResult.error());
        return;
    }
    auto fontStyleCpp =
        snap::drawing::FontStyle(snap::drawing::FontWidthNormal, fontWeightResult.value(), fontSlantResult.value());

    Valdi::BytesView fontData;

    if (typefaceBytes != nullptr) {
        fontData = ValdiAndroid::toByteArray(ValdiAndroid::JavaEnv(), typefaceBytes);
    } else if (static_cast<int>(typefaceFd) != 0) {
        auto fontDataResult = Valdi::DiskUtils::loadFromFd(static_cast<int>(typefaceFd));
        if (!fontDataResult) {
            onRegisterTypefaceError(familyNameCpp, fontDataResult.error());
            return;
        }
        fontData = fontDataResult.moveValue();
    } else {
        onRegisterTypefaceError(familyNameCpp,
                                Valdi::Error("Font should be provided through bytes or file descriptor"));
        return;
    }

    snapDrawingRuntime->getViewManager()->registerTypeface(
        familyNameCpp, fontStyleCpp, static_cast<bool>(isFallback), fontData);
}

void ValdiAndroid::NativeBridge::snapDrawingSetSurfacePresenterManager(
    fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
    jlong snapDrawingRootHandle,
    jobject surfacePresenterManager) {
    auto hostCpp = getSnapDrawingRoot(snapDrawingRootHandle);
    hostCpp->setSurfacePresenterManager(surfacePresenterManager);
}

void ValdiAndroid::NativeBridge::snapDrawingCallFrameCallback(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                              jlong frameCallback,
                                                              jlong frameTimeNanos) {
    snap::drawing::AndroidFrameScheduler::performCallback(static_cast<int64_t>(frameCallback),
                                                          static_cast<int64_t>(frameTimeNanos));
}

void ValdiAndroid::NativeBridge::snapDrawingDrawLayerInBitmap(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                              jlong snapDrawingLayerHandle,
                                                              jobject bitmap) {
    auto* env = fbjni::Environment::current();
    auto layer = valueFromJavaCppHandle(snapDrawingLayerHandle).getTypedRef<Valdi::View>();
    if (layer == nullptr) {
        ValdiAndroid::throwJavaValdiException(env, "Could not unwrap SnapDrawingLayer");
        return;
    }
    auto androidBitmap = Valdi::makeShared<AndroidBitmap>(JavaObject(JavaEnv(), bitmap));

    auto result = SnapDrawingLayerRootHost::drawLayerInBitmap(layer, androidBitmap);
    if (!result) {
        ValdiAndroid::throwJavaValdiException(env, result.error().toString().c_str());
    }
}

jlong ValdiAndroid::NativeBridge::snapDrawingGetMaxRenderTargetSize(fbjni::alias_ref<fbjni::JClass> /*clazz*/, // NOLINT
                                                                    jlong snapDrawingRuntimeHandle) {
    auto snapDrawingRuntime = getSnapDrawingRuntime(snapDrawingRuntimeHandle);
    if (snapDrawingRuntime == nullptr) {
        return -1;
    }
    return snapDrawingRuntime->getMaxRenderTargetSize();
}

#endif

void ValdiAndroid::NativeBridge::registerNatives() {
    javaClassStatic()->registerNatives({
        makeNativeMethod("getBuildOptions", ValdiAndroid::NativeBridge::getBuildOptions),
        makeNativeMethod("getAllRuntimeAttachedObjects", ValdiAndroid::NativeBridge::getAllRuntimeAttachedObjects),
        makeNativeMethod("prepareRenderBackend", ValdiAndroid::NativeBridge::prepareRenderBackend),
        makeNativeMethod("emitRuntimeManagerInitMetrics", ValdiAndroid::NativeBridge::emitRuntimeManagerInitMetrics),
        makeNativeMethod("emitUserSessionReadyMetrics", ValdiAndroid::NativeBridge::emitUserSessionReadyMetrics),
        makeNativeMethod("getJSRuntime", ValdiAndroid::NativeBridge::getJSRuntime),
        makeNativeMethod("getViewNodeBackingView", ValdiAndroid::NativeBridge::getViewNodeBackingView),
        makeNativeMethod("getValueForAttribute", ValdiAndroid::NativeBridge::getValueForAttribute),
        makeNativeMethod("getViewInContextForId", ValdiAndroid::NativeBridge::getViewInContextForId),
        makeNativeMethod("getRetainedViewNodeInContext", ValdiAndroid::NativeBridge::getRetainedViewNodeInContext),
        makeNativeMethod("getRetainedViewNodeChildren", ValdiAndroid::NativeBridge::getRetainedViewNodeChildren),
        makeNativeMethod("getRetainedViewNodeHitTestAccessibility",
                         ValdiAndroid::NativeBridge::getRetainedViewNodeHitTestAccessibility),
        makeNativeMethod("getViewNodeAccessibilityHierarchyRepresentation",
                         ValdiAndroid::NativeBridge::getViewNodeAccessibilityHierarchyRepresentation),
        makeNativeMethod("performViewNodeAction", ValdiAndroid::NativeBridge::performViewNodeAction),
        makeNativeMethod("waitUntilAllUpdatesCompleted", ValdiAndroid::NativeBridge::waitUntilAllUpdatesCompleted),
        makeNativeMethod("onNextLayout", ValdiAndroid::NativeBridge::onNextLayout),
        makeNativeMethod("scheduleExclusiveUpdate", ValdiAndroid::NativeBridge::scheduleExclusiveUpdate),
        makeNativeMethod("setViewInflationEnabled", ValdiAndroid::NativeBridge::setViewInflationEnabled),
        makeNativeMethod("getRuntimeAttachedObject", ValdiAndroid::NativeBridge::getRuntimeAttachedObject),
        makeNativeMethod("getRuntimeAttachedObjectFromContext",
                         ValdiAndroid::NativeBridge::getRuntimeAttachedObjectFromContext),
        makeNativeMethod("getCurrentContext", ValdiAndroid::NativeBridge::getCurrentContext),
        makeNativeMethod("getNodeId", ValdiAndroid::NativeBridge::getNodeId),
        makeNativeMethod("getViewClassName", ValdiAndroid::NativeBridge::getViewClassName),
        makeNativeMethod("getViewNodeDebugDescription", ValdiAndroid::NativeBridge::getViewNodeDebugDescription),
        makeNativeMethod("invalidateLayout", ValdiAndroid::NativeBridge::invalidateLayout),
        makeNativeMethod("isViewNodeLayoutDirectionHorizontal",
                         ValdiAndroid::NativeBridge::isViewNodeLayoutDirectionHorizontal),
        makeNativeMethod("createRuntime", ValdiAndroid::NativeBridge::createRuntime),
        makeNativeMethod("onRuntimeReady", ValdiAndroid::NativeBridge::onRuntimeReady),
        makeNativeMethod("createRuntimeManager", ValdiAndroid::NativeBridge::createRuntimeManager),
        makeNativeMethod("getViewNodePoint", ValdiAndroid::NativeBridge::getViewNodePoint),
        makeNativeMethod("getViewNodeSize", ValdiAndroid::NativeBridge::getViewNodeSize),
        makeNativeMethod("canViewNodeScroll", ValdiAndroid::NativeBridge::canViewNodeScroll),
        makeNativeMethod("isViewNodeScrollingOrAnimating", ValdiAndroid::NativeBridge::isViewNodeScrollingOrAnimating),
        makeNativeMethod("applicationSetConfiguration", ValdiAndroid::NativeBridge::applicationSetConfiguration),
        makeNativeMethod("setPointScale", ValdiAndroid::NativeBridge::setPointScale),
        makeNativeMethod("applicationDidResume", ValdiAndroid::NativeBridge::applicationDidResume),
        makeNativeMethod("applicationIsInLowMemory", ValdiAndroid::NativeBridge::applicationIsInLowMemory),
        makeNativeMethod("applicationWillPause", ValdiAndroid::NativeBridge::applicationWillPause),
        makeNativeMethod("setRootView", ValdiAndroid::NativeBridge::setRootView),
        makeNativeMethod("measureLayout", ValdiAndroid::NativeBridge::measureLayout),
        makeNativeMethod("setLayoutSpecs", ValdiAndroid::NativeBridge::setLayoutSpecs),
        makeNativeMethod("setVisibleViewport", ValdiAndroid::NativeBridge::setVisibleViewport),
        makeNativeMethod("callJSFunction", ValdiAndroid::NativeBridge::callJSFunction),
        makeNativeMethod("callOnJsThread", ValdiAndroid::NativeBridge::callOnJsThread),
        makeNativeMethod("callSyncWithJsThread", ValdiAndroid::NativeBridge::callSyncWithJsThread),
        makeNativeMethod("enqueueLoadOperation", ValdiAndroid::NativeBridge::enqueueLoadOperation),
        makeNativeMethod("flushLoadOperations", ValdiAndroid::NativeBridge::flushLoadOperations),
        makeNativeMethod("clearViewPools", ValdiAndroid::NativeBridge::clearViewPools),
        makeNativeMethod("createContext", ValdiAndroid::NativeBridge::createContext),
        makeNativeMethod("contextOnCreate", ValdiAndroid::NativeBridge::contextOnCreate),
        makeNativeMethod("deleteNativeHandle", ValdiAndroid::NativeBridge::deleteNativeHandle),
        makeNativeMethod("releaseNativeRef", ValdiAndroid::NativeBridge::releaseNativeRef),
        makeNativeMethod("deleteRuntime", ValdiAndroid::NativeBridge::deleteRuntime),
        makeNativeMethod("deleteRuntimeManager", ValdiAndroid::NativeBridge::deleteRuntimeManager),
        makeNativeMethod("destroyContext", ValdiAndroid::NativeBridge::destroyContext),
        makeNativeMethod("forceBindAttributes", ValdiAndroid::NativeBridge::forceBindAttributes),
        makeNativeMethod("bindAttribute", ValdiAndroid::NativeBridge::bindAttribute),
        makeNativeMethod("bindScrollAttributes", ValdiAndroid::NativeBridge::bindScrollAttributes),
        makeNativeMethod("bindAssetAttributes", ValdiAndroid::NativeBridge::bindAssetAttributes),
        makeNativeMethod("setPlaceholderViewMeasureDelegate",
                         ValdiAndroid::NativeBridge::setPlaceholderViewMeasureDelegate),
        makeNativeMethod("setMeasureDelegate", ValdiAndroid::NativeBridge::setMeasureDelegate),
        makeNativeMethod("registerAttributePreprocessor", ValdiAndroid::NativeBridge::registerAttributePreprocessor),
        makeNativeMethod("setUserSession", ValdiAndroid::NativeBridge::setUserSession),
        makeNativeMethod("loadModule", ValdiAndroid::NativeBridge::loadModule),
        makeNativeMethod("enqueueWorkerTask", ValdiAndroid::NativeBridge::enqueueWorkerTask),
        makeNativeMethod("performCallback", ValdiAndroid::NativeBridge::performCallback),
        makeNativeMethod("discardCallback", ValdiAndroid::NativeBridge::discardCallback),
        makeNativeMethod("performGcNow", ValdiAndroid::NativeBridge::performGcNow),
        makeNativeMethod("preloadViews", ValdiAndroid::NativeBridge::preloadViews),
        makeNativeMethod("reapplyAttribute", ValdiAndroid::NativeBridge::reapplyAttribute),
        makeNativeMethod("registerNativeModuleFactory", ValdiAndroid::NativeBridge::registerNativeModuleFactory),
        makeNativeMethod("registerTypeConverter", ValdiAndroid::NativeBridge::registerTypeConverter),
        makeNativeMethod("registerModuleFactoriesProvider",
                         ValdiAndroid::NativeBridge::registerModuleFactoriesProvider),
        makeNativeMethod("registerViewClassReplacement", ValdiAndroid::NativeBridge::registerViewClassReplacement),
        makeNativeMethod("registerAssetLoader", ValdiAndroid::NativeBridge::registerAssetLoader),
        makeNativeMethod("unregisterAssetLoader", ValdiAndroid::NativeBridge::unregisterAssetLoader),
        makeNativeMethod("notifyAssetLoaderCompleted", ValdiAndroid::NativeBridge::notifyAssetLoaderCompleted),
        makeNativeMethod("createViewFactory", ValdiAndroid::NativeBridge::createViewFactory),
        makeNativeMethod("registerLocalViewFactory", ValdiAndroid::NativeBridge::registerLocalViewFactory),
        makeNativeMethod("setParentContext", ValdiAndroid::NativeBridge::setParentContext),
        makeNativeMethod("setKeepViewAliveOnDestroy", ValdiAndroid::NativeBridge::setKeepViewAliveOnDestroy),
        makeNativeMethod("notifyScroll", ValdiAndroid::NativeBridge::notifyScroll),
        makeNativeMethod("setValueForAttribute", ValdiAndroid::NativeBridge::setValueForAttribute),
        makeNativeMethod("notifyApplyAttributeFailed", ValdiAndroid::NativeBridge::notifyApplyAttributeFailed),
        makeNativeMethod("setRuntimeAttachedObject", ValdiAndroid::NativeBridge::setRuntimeAttachedObject),
        makeNativeMethod("setRuntimeManagerRequestManager",
                         ValdiAndroid::NativeBridge::setRuntimeManagerRequestManager),
        makeNativeMethod("setRetainsLayoutSpecsOnInvalidateLayout",
                         ValdiAndroid::NativeBridge::setRetainsLayoutSpecsOnInvalidateLayout),
        makeNativeMethod("setViewModel", ValdiAndroid::NativeBridge::setViewModel),
        makeNativeMethod("unloadAllJsModules", ValdiAndroid::NativeBridge::unloadAllJsModules),
        makeNativeMethod("updateResource", ValdiAndroid::NativeBridge::updateResource),
        makeNativeMethod("valueChangedForAttribute", ValdiAndroid::NativeBridge::valueChangedForAttribute),
        makeNativeMethod("dumpMemoryStatistics", ValdiAndroid::NativeBridge::dumpMemoryStatistics),
        makeNativeMethod("dumpLogMetadata", ValdiAndroid::NativeBridge::dumpLogMetadata),
        makeNativeMethod("dumpLogs", ValdiAndroid::NativeBridge::dumpLogs),
        makeNativeMethod("getAllContexts", ValdiAndroid::NativeBridge::getAllContexts),
        makeNativeMethod("getAsset", ValdiAndroid::NativeBridge::getAsset),
        makeNativeMethod("getModuleEntry", ValdiAndroid::NativeBridge::getModuleEntry),
        makeNativeMethod("getCurrentEventTime", ValdiAndroid::NativeBridge::getCurrentEventTime),
        makeNativeMethod("captureJavaScriptStackTraces", ValdiAndroid::NativeBridge::captureJavaScriptStackTraces),
        makeNativeMethod("getAllModuleHashes", ValdiAndroid::NativeBridge::getAllModuleHashes),
        makeNativeMethod("startProfiling", ValdiAndroid::NativeBridge::startProfiling),
        makeNativeMethod("stopProfiling", ValdiAndroid::NativeBridge::stopProfiling),
        makeNativeMethod("wrapAndroidBitmap", ValdiAndroid::NativeBridge::wrapAndroidBitmap),
#ifdef SNAP_DRAWING_ENABLED
        makeNativeMethod("getSnapDrawingRuntimeHandle", ValdiAndroid::NativeBridge::getSnapDrawingRuntimeHandle),
        makeNativeMethod("setSnapDrawingRootView", ValdiAndroid::NativeBridge::setSnapDrawingRootView),
        makeNativeMethod("createSnapDrawingRoot", ValdiAndroid::NativeBridge::createSnapDrawingRoot),
        makeNativeMethod("dispatchSnapDrawingTouchEvent", ValdiAndroid::NativeBridge::dispatchSnapDrawingTouchEvent),
        makeNativeMethod("snapDrawingLayout", ValdiAndroid::NativeBridge::snapDrawingLayout),
        makeNativeMethod("snapDrawingRegisterTypeface", ValdiAndroid::NativeBridge::snapDrawingRegisterTypeface),
        makeNativeMethod("snapDrawingDrawInBitmap", ValdiAndroid::NativeBridge::snapDrawingDrawInBitmap),
        makeNativeMethod("snapDrawingSetSurface", ValdiAndroid::NativeBridge::snapDrawingSetSurface),
        makeNativeMethod("snapDrawingSetSurfaceNeedsRedraw",
                         ValdiAndroid::NativeBridge::snapDrawingSetSurfaceNeedsRedraw),
        makeNativeMethod("snapDrawingOnSurfaceSizeChanged",
                         ValdiAndroid::NativeBridge::snapDrawingOnSurfaceSizeChanged),
        makeNativeMethod("snapDrawingSetSurfacePresenterManager",
                         ValdiAndroid::NativeBridge::snapDrawingSetSurfacePresenterManager),
        makeNativeMethod("snapDrawingCallFrameCallback", ValdiAndroid::NativeBridge::snapDrawingCallFrameCallback),
        makeNativeMethod("snapDrawingDrawLayerInBitmap", ValdiAndroid::NativeBridge::snapDrawingDrawLayerInBitmap),
        makeNativeMethod("snapDrawingGetMaxRenderTargetSize",
                         ValdiAndroid::NativeBridge::snapDrawingGetMaxRenderTargetSize),
#endif
    });
}
