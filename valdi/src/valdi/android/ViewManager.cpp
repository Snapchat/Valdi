//
//  ViewManager.cpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/13/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/android/ViewManager.hpp"
#include "valdi/android/AndroidBitmap.hpp"
#include "valdi_core/NativeAnimator.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

#include "valdi_core/cpp/Interfaces/IBitmap.hpp"

#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"

#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/DeferredViewTransaction.hpp"

#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "utils/debugging/Assert.hpp"
#include <boost/algorithm/string/replace.hpp>
#include <djinni/jni/djinni_support.hpp>
#include <fmt/format.h>

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include "valdi_core/jni/JNIMethodUtils.hpp"
#include "valdi_core/jni/JavaClass.hpp"

#include "valdi/android/AndroidBitmapFactory.hpp"
#include "valdi/android/AndroidViewFactory.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/AndroidViewTransaction.hpp"
#include "valdi/android/AttributesBindingContextWrapper.hpp"
#include "valdi/android/DeferredViewOperations.hpp"

#include <android/bitmap.h>

namespace ValdiAndroid {

ViewManager::ViewManager(JavaEnv env, jobject object, float pointScale, Valdi::ILogger& logger)
    : GlobalRefJavaObject(env, object, "ViewManager"),
      _viewOperationsPool(Valdi::makeShared<DeferredViewOperationsPool>()),
      _pointScale(pointScale),
      _logger(logger) {
    auto obj = toObject();
    auto cls = obj.getClass();
    cls.getMethod("createViewFactory", _createViewFactoryMethod);
    cls.getMethod("callAction", _callActionMethod);
    cls.getMethod("bindAttributes", _bindAttributesMethod);
    cls.getMethod("createAnimator", _createAnimatorMethod);
    cls.getMethod("presentDebugMessage", _presentDebugMessageMethod);
    cls.getMethod("onNonFatal", _onNonFatal);
    cls.getMethod("onJsCrash", _onJsCrash);
    cls.getMethod("performViewOperations", _performViewOperationsMethod);
    cls.getMethod("measure", _measureMethod);
    cls.getMethod("getMeasurerPlaceholderView", _getMeasurerPlaceholderViewMethod);
    cls.getMethod("createViewNodeWrapper", _createViewNodeWrapperMethod);
}

ViewManager::~ViewManager() = default;

Valdi::PlatformType ViewManager::getPlatformType() const {
    return Valdi::PlatformTypeAndroid;
}

Valdi::RenderingBackendType ViewManager::getRenderingBackendType() const {
    return Valdi::RenderingBackendTypeNative;
}

Valdi::StringBox ViewManager::getDefaultViewClass() {
    static auto kDefaultViewClass = STRING_LITERAL("com.snap.valdi.views.ValdiView");
    return kDefaultViewClass;
}

Valdi::Ref<Valdi::ViewFactory> ViewManager::createViewFactory(
    const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes>& boundAttributes) {
    auto* jClass = getClassForName(className).javaClass.getClass();
    auto viewFactory = _createViewFactoryMethod.call(toObject(), jClass);
    auto viewFactoryObject =
        ValdiAndroid::GlobalRefJavaObjectBase(getEnv(), viewFactory.getUnsafeObject(), "ViewFactory");

    return Valdi::makeShared<ValdiAndroid::AndroidViewFactory>(
        std::move(viewFactoryObject), className, *this, boundAttributes, false);
}

void ViewManager::callAction(Valdi::ViewNodeTree* viewNodeTree,
                             const Valdi::StringBox& actionName,
                             const Valdi::Ref<Valdi::ValueArray>& parameters) {
    auto jParameters = toJavaObject(getEnv(), parameters);
    _callActionMethod.call(toObject(),
                           fromValdiContextUserData(viewNodeTree->getContext()->getUserData()),
                           actionName.slowToString(),
                           jParameters);
}

ResolvedJavaClass& ViewManager::getClassForName(const Valdi::StringBox& className) {
    return getClassForName(className, true);
}

ResolvedJavaClass& ViewManager::getClassForName(const Valdi::StringBox& className, bool fallbackIfNeeded) {
    std::lock_guard<Valdi::Mutex> guard(_mutex);

    return lockFreeGetClassForName(className, fallbackIfNeeded);
}

ResolvedJavaClass& ViewManager::lockFreeGetClassForName(const Valdi::StringBox& className, bool fallbackIfNeeded) {
    const auto& it = _classByName.find(className);
    if (it != _classByName.end()) {
        return *it->second;
    }

    auto convertedClassName = boost::replace_all_copy(className.slowToString(), ".", "/");

    if (fallbackIfNeeded) {
        auto result = JavaClass::resolve(getEnv(), convertedClassName.c_str());
        if (!result) {
            VALDI_ERROR(_logger,
                        "Failed to resolve view class '{}', failing back to ValdiView instead: {}",
                        className,
                        result.error());

            auto* jclass =
                lockFreeGetClassForName(STRING_LITERAL("com.snap.valdi.views.ValdiView"), false).javaClass.getClass();
            return *_classByName
                        .try_emplace(className, std::make_unique<ResolvedJavaClass>(JavaClass(getEnv(), jclass), true))
                        .first->second;
        }

        return *_classByName.try_emplace(className, std::make_unique<ResolvedJavaClass>(result.moveValue(), false))
                    .first->second;
    } else {
        auto javaClass = JavaClass::resolveOrAbort(getEnv(), convertedClassName.c_str());

        return *_classByName.try_emplace(className, std::make_unique<ResolvedJavaClass>(std::move(javaClass), false))
                    .first->second;
    }
}

std::vector<Valdi::StringBox> ViewManager::getClassHierarchy(const Valdi::StringBox& className) {
    std::vector<Valdi::StringBox> out;
    out.emplace_back(className);

#ifdef VALDI_DEBUG_JNI_CALLS
    __android_log_print(ANDROID_LOG_INFO, "[valdi_jni]", "Resolving classHierarchy for %s", className.getCStr());
#endif

    auto* jclass = getClassForName(className).javaClass.getClass();

    while (true) {
        jclass = JavaEnv::accessEnvRet([&](JNIEnv& env) -> auto { return env.GetSuperclass(jclass); });
        if (jclass == nullptr) {
            break;
        }

        auto name = JavaEnv::getCache().getClassGetNameMethod().call(jclass);
        auto cppName = ValdiAndroid::toInternedString(getEnv(), name);

#ifdef VALDI_DEBUG_JNI_CALLS
        __android_log_print(
            ANDROID_LOG_INFO, "[valdi_jni]", "Parent class of %s -> %s", className.getCStr(), cppName.getCStr());
#endif

        out.emplace_back(std::move(cppName));
    }

    return out;
}

void ViewManager::bindAttributes(const Valdi::StringBox& className, Valdi::AttributesBindingContext& binder) {
    static auto kObjectClass = STRING_LITERAL("java.lang.Object");
    if (className == kObjectClass) {
        return;
    }

    auto wrapper = Valdi::makeShared<AttributesBindingContextWrapper>(*this, binder);
    auto ptr = reinterpret_cast<int64_t>(Valdi::unsafeBridgeCast(wrapper.get()));

    auto* jclass = getClassForName(className).javaClass.getClass();

    _bindAttributesMethod.call(toObject(), jclass, ptr);
}

Valdi::NativeAnimator ViewManager::createAnimator(snap::valdi_core::AnimationType type,
                                                  const std::vector<double>& controlPoints,
                                                  double duration,
                                                  bool beginFromCurrentState,
                                                  bool crossfade,
                                                  double stiffness,
                                                  double damping) {
    auto valueArray = Valdi::ValueArray::make(controlPoints.size());
    for (size_t i = 0; i < controlPoints.size(); i++) {
        valueArray->emplace(i, Valdi::Value(controlPoints[i]));
    }

    auto objectJArray = ValdiAndroid::toJavaObject(getEnv(), valueArray);
    auto object = _createAnimatorMethod.call(toObject(),
                                             static_cast<int32_t>(type),
                                             objectJArray,
                                             duration,
                                             beginFromCurrentState,
                                             crossfade,
                                             stiffness,
                                             damping);
    if (object == nullptr) {
        return nullptr;
    }

    return djinni_generated_client::valdi_core::NativeAnimator::toCpp(JavaEnv::getUnsafeEnv(),
                                                                      object.getUnsafeObject());
}

float ViewManager::getPointScale() const {
    return _pointScale.load(std::memory_order_relaxed);
}

void ViewManager::setPointScale(float pointScale) {
    _pointScale.store(pointScale, std::memory_order_relaxed);
}

void ViewManager::onDebugMessage(int32_t level, const std::string& message) {
    _presentDebugMessageMethod.call(toObject(), level, message);
}

void ViewManager::onUncaughtJsError(const int32_t errorCode,
                                    const Valdi::StringBox& moduleName,
                                    const std::string& errorMessage,
                                    const std::string& stackTrace) {
    _onNonFatal.call(toObject(), errorCode, moduleName.slowToString(), errorMessage, stackTrace);
}

void ViewManager::onJsCrash(const Valdi::StringBox& moduleName,
                            const std::string& errorMessage,
                            const std::string& stackTrace,
                            bool isANR) {
    _onJsCrash.call(toObject(), moduleName.slowToString(), errorMessage, stackTrace, isANR);
}

Valdi::Value ViewManager::createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode>& viewNode,
                                                bool wrapInPlatformReference) {
    auto javaValdiContext = viewNode->getViewNodeTree()->getContext()->getUserData();
    if (javaValdiContext == nullptr) {
        // This can happen if the context was destroyed. We return undefined in this case.
        return Valdi::Value::undefined();
    }

    auto viewNodeWrapper =
        _createViewNodeWrapperMethod.call(toObject(),
                                          fromValdiContextUserData(javaValdiContext),
                                          reinterpret_cast<int64_t>(Valdi::unsafeBridgeRetain(viewNode.get())),
                                          wrapInPlatformReference);

    return Valdi::Value(valdiObjectFromJavaObject(viewNodeWrapper, "ViewNode"));
}

int64_t ViewManager::measure(const JavaObject& measureDelegate,
                             int64_t attributesHandle,
                             int32_t width,
                             int32_t widthMode,
                             int32_t height,
                             int32_t heightMode,
                             bool isRightToLeft) {
    return _measureMethod.call(
        toObject(), measureDelegate, attributesHandle, width, widthMode, height, heightMode, isRightToLeft);
}

ViewType ViewManager::getMeasurerPlaceholderView(const JavaObject& attributesBinder) {
    return _getMeasurerPlaceholderViewMethod.call(toObject(), attributesBinder);
}

bool ViewManager::supportsClassNameNatively(const Valdi::StringBox& className) {
    return !getClassForName(className).isFallback;
}

Valdi::Ref<DeferredViewOperations> ViewManager::makeViewOperations() {
    return _viewOperationsPool->make();
}

void ViewManager::flushViewOperations(Valdi::Ref<DeferredViewOperations> viewOperations, bool sync) {
    if (viewOperations == nullptr) {
        doFlushViewOperations(std::nullopt, sync);
    } else {
        auto operations = viewOperations->dequeueOperations();
        doFlushViewOperations(operations, sync);
        _viewOperationsPool->release(std::move(viewOperations));
    }
}

void ViewManager::doFlushViewOperations(const std::optional<SerializedViewOperations>& operations, bool sync) {
    VALDI_TRACE("Valdi.flushOperations");

    if (operations) {
        const auto& buffer = operations.value().buffer;
        const auto& attachedValues = operations.value().attachedValues;
        auto byteBuffer = ValdiAndroid::toByteBuffer(
            getEnv(), const_cast<void*>(reinterpret_cast<const void*>(buffer->data())), buffer->size());
        auto objectJArray = ValdiAndroid::toJavaObject(getEnv(), attachedValues);

        _performViewOperationsMethod.call(toObject(), byteBuffer, objectJArray, sync);
    } else {
        _performViewOperationsMethod.call(toObject(), JavaObject(getEnv()), ObjectArray(getEnv()), sync);
    }
}

Valdi::Ref<Valdi::IViewTransaction> ViewManager::createViewTransaction(
    const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager, bool shouldDefer) {
    if (!shouldDefer || mainThreadManager == nullptr || mainThreadManager->currentThreadIsMainThread()) {
        return Valdi::makeShared<AndroidViewTransaction>(*this);
    } else {
        return Valdi::makeShared<Valdi::DeferredViewTransaction>(*this, *mainThreadManager);
    }
}

Valdi::Ref<Valdi::IBitmapFactory> ViewManager::getViewRasterBitmapFactory() const {
    return AndroidBitmapFactory::getSharedInstance();
}

} // namespace ValdiAndroid
