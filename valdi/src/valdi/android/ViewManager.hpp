//
//  ViewManager.hpp
//  ValdiAndroidNative
//
//  Created by Simon Corsin on 6/13/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <atomic>

#include "valdi_core/jni/GlobalRefJavaObject.hpp"

#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include "valdi/RuntimeMessageHandler.hpp"

namespace Valdi {
class MainThreadManager;
}

namespace ValdiAndroid {

struct SerializedViewOperations;
class DeferredViewOperationsPool;
class DeferredViewOperations;

struct ResolvedJavaClass {
    JavaClass javaClass;
    bool isFallback;

    ResolvedJavaClass(JavaClass&& javaClass, bool isFallback)
        : javaClass(std::move(javaClass)), isFallback(isFallback) {}
};

class ViewManager : public GlobalRefJavaObject, public Valdi::IViewManager, public snap::valdi::RuntimeMessageHandler {
public:
    ViewManager(JavaEnv env, jobject object, float pointScale, Valdi::ILogger& logger);
    ~ViewManager() override;

    Valdi::PlatformType getPlatformType() const override;
    Valdi::RenderingBackendType getRenderingBackendType() const override;

    Valdi::StringBox getDefaultViewClass() override;

    Valdi::Ref<Valdi::ViewFactory> createViewFactory(
        const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes>& boundAttributes) override;

    void callAction(Valdi::ViewNodeTree* viewNodeTree,
                    const Valdi::StringBox& actionName,
                    const Valdi::Ref<Valdi::ValueArray>& parameters) override;

    Valdi::NativeAnimator createAnimator(snap::valdi_core::AnimationType type,
                                         const std::vector<double>& controlPoints,
                                         double duration,
                                         bool beginFromCurrentState,
                                         bool crossfade,
                                         double stiffness,
                                         double damping) override;

    std::vector<Valdi::StringBox> getClassHierarchy(const Valdi::StringBox& className) override;

    void bindAttributes(const Valdi::StringBox& className, Valdi::AttributesBindingContext& binder) override;

    float getPointScale() const override;
    void setPointScale(float pointScale);

    void onUncaughtJsError(const int32_t errorCode,
                           const Valdi::StringBox& moduleName,
                           const std::string& errorMessage,
                           const std::string& stackTrace) override;

    void onJsCrash(const Valdi::StringBox& moduleName,
                   const std::string& errorMessage,
                   const std::string& stackTrace,
                   bool isANR) override;

    void onDebugMessage(int32_t level, const std::string& message) override;

    bool supportsClassNameNatively(const Valdi::StringBox& className) override;

    Valdi::Ref<DeferredViewOperations> makeViewOperations();
    void flushViewOperations(Valdi::Ref<DeferredViewOperations> viewOperations, bool sync);

    Valdi::Value createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode>& viewNode,
                                       bool wrapInPlatformReference) override;

    Valdi::Ref<Valdi::IViewTransaction> createViewTransaction(
        const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager, bool shouldDefer) override;

    Valdi::Ref<Valdi::IBitmapFactory> getViewRasterBitmapFactory() const override;

    int64_t measure(const JavaObject& measureDelegate,
                    int64_t attributesHandle,
                    int32_t width,
                    int32_t widthMode,
                    int32_t height,
                    int32_t heightMode,
                    bool isRightToLeft);
    ViewType getMeasurerPlaceholderView(const JavaObject& attributesBinder);

private:
    Valdi::Ref<DeferredViewOperationsPool> _viewOperationsPool;
    std::atomic<float> _pointScale;
    [[maybe_unused]] Valdi::ILogger& _logger;

    JavaMethod<JavaObject, jclass> _createViewFactoryMethod;
    JavaMethod<VoidType, ValdiContext, std::string, ObjectArray> _callActionMethod;
    JavaMethod<JavaObject, ValdiContext, int64_t, bool> _createViewNodeWrapperMethod;
    JavaMethod<VoidType, jclass, int64_t> _bindAttributesMethod;
    JavaMethod<JavaObject, int32_t, ObjectArray, double, bool, bool, double, double> _createAnimatorMethod;
    JavaMethod<VoidType, int32_t, std::string> _presentDebugMessageMethod;
    JavaMethod<VoidType, int32_t, std::string, std::string, std::string> _onNonFatal;
    JavaMethod<VoidType, std::string, std::string, std::string, bool> _onJsCrash;
    JavaMethod<VoidType, JavaObject, ObjectArray, bool> _performViewOperationsMethod;
    JavaMethod<int64_t, JavaObject, int64_t, int32_t, int32_t, int32_t, int32_t, bool> _measureMethod;
    JavaMethod<ViewType, JavaObject> _getMeasurerPlaceholderViewMethod;

    Valdi::FlatMap<Valdi::StringBox, std::unique_ptr<ResolvedJavaClass>> _classByName;
    Valdi::Mutex _mutex;
    bool _scheduledFlush = false;

    Valdi::Size doMeasure(
        const ViewType& view, float width, Valdi::MeasureMode widthMode, float height, Valdi::MeasureMode heightMode);
    ResolvedJavaClass& getClassForName(const Valdi::StringBox& className);
    ResolvedJavaClass& getClassForName(const Valdi::StringBox& className, bool fallbackIfNeeded);
    ResolvedJavaClass& lockFreeGetClassForName(const Valdi::StringBox& className, bool fallbackIfNeeded);

    void doFlushViewOperations(const std::optional<SerializedViewOperations>& operations, bool sync);

    inline int32_t convertPoint(float point) const;
};

} // namespace ValdiAndroid
