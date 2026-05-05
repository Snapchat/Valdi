//
//  ManagedContextNativeModuleFactory.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 6/17/24.
//

#include "valdi/snap_drawing/Modules/ManagedContextNativeModuleFactory.hpp"
#include "snap_drawing/cpp/Drawing/Composition/Compositor.hpp"
#include "snap_drawing/cpp/Drawing/Composition/CompositorPlaneList.hpp"
#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/Raster/RasterContext.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ILayerRoot.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ContextAutoDestroy.hpp"
#include "valdi/runtime/Context/IViewNodesAssetTracker.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithMethod.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

namespace snap::drawing {

class ManagedContextLayerRoot : public ILayerRoot {
public:
    explicit ManagedContextLayerRoot(bool useNewExternalSurfaceRasterMethod)
        : _useNewExternalSurfaceRasterMethod(useNewExternalSurfaceRasterMethod) {}
    ~ManagedContextLayerRoot() override = default;

    EventId enqueueEvent(EventCallback&& eventCallback, Duration after) final {
        return EventId();
    }

    bool cancelEvent(EventId eventId) final {
        return false;
    }

    bool shouldRasterizeExternalSurface() const final {
        return !_useNewExternalSurfaceRasterMethod;
    }

    void onInitialize() final {}
    void setChildNeedsDisplay() final {}
    void requestLayout(ILayer* layer) final {}
    void requestFocus(ILayer* layer) final {}

    LayerId allocateLayerId() final {
        return ++_layerIdSequence;
    }

private:
    bool _useNewExternalSurfaceRasterMethod;
    uint64_t _layerIdSequence = 0;
};

class SnapDrawingValdiContext : public Valdi::ContextAutoDestroy {
public:
    SnapDrawingValdiContext(Valdi::Runtime* runtime,
                            const Ref<Valdi::ViewNodeTree>& viewNodeTree,
                            Valdi::ContextId contextId,
                            Valdi::ContextManager& contextManager,
                            bool useNewExternalSurfaceRasterMethod,
                            bool enableDeltaRasterization)
        : Valdi::ContextAutoDestroy(contextId, contextManager),
          _runtime(Valdi::weakRef(runtime)),
          _viewNodeTree(viewNodeTree),
          _layerRoot(Valdi::makeShared<ManagedContextLayerRoot>(useNewExternalSurfaceRasterMethod)),
          _rasterContext(Valdi::makeShared<RasterContext>(runtime->getLogger(),
                                                          useNewExternalSurfaceRasterMethod ?
                                                              ExternalSurfaceRasterizationMethod::ACCURATE :
                                                              ExternalSurfaceRasterizationMethod::FAST,
                                                          enableDeltaRasterization)),
          _useNewExternalSurfaceRasterMethod(useNewExternalSurfaceRasterMethod) {
        auto rootLayer = valdiViewToLayer(_viewNodeTree->getRootView());
        if (rootLayer != nullptr) {
            rootLayer->onParentChanged(_layerRoot);
        }
    }

    ~SnapDrawingValdiContext() override {
        destroy();
    }

    void destroy() {
        _destroyed.store(true);
        auto runtime = _runtime.lock();
        if (runtime != nullptr) {
            Valdi::ContextAutoDestroy::destroy();

            runtime->destroyViewNodeTree(*_viewNodeTree);
        }
    }

    bool isDestroyed() const {
        return _destroyed.load();
    }

    const Ref<Valdi::ViewNodeTree>& getViewNodeTree() const {
        return _viewNodeTree;
    }

    bool isUsingNewExternalSurfaceRasterMethod() const {
        return _useNewExternalSurfaceRasterMethod;
    }

    const Ref<RasterContext>& getRasterContext() const {
        return _rasterContext;
    }

    Ref<Valdi::Runtime> getRuntime() const {
        return _runtime.lock();
    }

private:
    std::atomic<bool> _destroyed{false};
    Valdi::Weak<Valdi::Runtime> _runtime;
    Ref<Valdi::ViewNodeTree> _viewNodeTree;
    Ref<ManagedContextLayerRoot> _layerRoot;
    Ref<RasterContext> _rasterContext;
    bool _useNewExternalSurfaceRasterMethod;
};

class SnapDrawingFrame : public Valdi::ValdiObject {
public:
    explicit SnapDrawingFrame(const Ref<RasterContext>& rasterContext, const Ref<DisplayList>& displayList)
        : _rasterContext(rasterContext), _displayList(displayList) {}
    ~SnapDrawingFrame() override = default;

    void dispose() {
        _displayList = nullptr;
        _rasterContext = nullptr;
    }

    VALDI_CLASS_HEADER_IMPL(SnapDrawingFrame)

    Valdi::Result<RasterContext::RasterResult> raster(const Ref<Valdi::IBitmap>& bitmap,
                                                      bool shouldClearBitmapBeforeDrawing) {
        return _rasterContext->raster(_displayList, bitmap, shouldClearBitmapBeforeDrawing);
    }

    Valdi::Result<RasterContext::RasterResult> rasterDelta(const Ref<Valdi::IBitmap>& bitmap) {
        return _rasterContext->rasterDelta(_displayList, bitmap);
    }

    bool isDisposed() const {
        return _displayList == nullptr;
    }

private:
    Ref<RasterContext> _rasterContext;
    Ref<DisplayList> _displayList;
};

class AssetTrackerBridge : public Valdi::IViewNodesAssetTracker {
public:
    static constexpr int32_t kEventTypeBeganRequesting = 1;
    static constexpr int32_t kEventTypeEndRequesting = 2;
    static constexpr int32_t kEventTypeLoadedAssetChanged = 3;

    explicit AssetTrackerBridge(Ref<Valdi::ValueFunction> callback) : _callback(std::move(callback)) {}

    ~AssetTrackerBridge() override = default;

    void onBeganRequestingLoadedAsset(Valdi::RawViewNodeId viewNodeId, const Ref<Valdi::Asset>& asset) override {
        _callback->call(Valdi::ValueFunctionFlagsNone,
                        {Valdi::Value(kEventTypeBeganRequesting), Valdi::Value(viewNodeId)});
    }

    void onEndRequestingLoadedAsset(Valdi::RawViewNodeId viewNodeId, const Ref<Valdi::Asset>& asset) override {
        _callback->call(Valdi::ValueFunctionFlagsNone,
                        {Valdi::Value(kEventTypeEndRequesting), Valdi::Value(viewNodeId)});
    }

    void onLoadedAssetChanged(Valdi::RawViewNodeId viewNodeId,
                              const Ref<Valdi::Asset>& asset,
                              const std::optional<Valdi::StringBox>& error) override {
        _callback->call(Valdi::ValueFunctionFlagsNone,
                        {Valdi::Value(kEventTypeLoadedAssetChanged),
                         Valdi::Value(viewNodeId),
                         error ? Valdi::Value(error.value()) : Valdi::Value::undefined()});
    }

private:
    Ref<Valdi::ValueFunction> _callback;
};

ManagedContextNativeModuleFactory::ManagedContextNativeModuleFactory(
    Valdi::Function<Ref<Runtime>()> runtimeProvider,
    Valdi::Function<Ref<Valdi::ViewManagerContext>()> viewManagerContextProvider)
    : _runtimeProvider(std::move(runtimeProvider)),
      _viewManagerContextProvider(std::move(viewManagerContextProvider)) {}

ManagedContextNativeModuleFactory::~ManagedContextNativeModuleFactory() = default;

Valdi::StringBox ManagedContextNativeModuleFactory::getModulePath() {
    return STRING_LITERAL("drawing/src/ManagedContextNative");
}

Valdi::Value ManagedContextNativeModuleFactory::loadModule() {
    Valdi::Value out;

    Valdi::ValueFunctionMethodBinder<ManagedContextNativeModuleFactory> binder(this, out);
    binder.bind("createValdiContextWithSnapDrawing",
                &ManagedContextNativeModuleFactory::createValdiContextWithSnapDrawing);
    binder.bind("destroyValdiContextWithSnapDrawing",
                &ManagedContextNativeModuleFactory::destroyValdiContextWithSnapDrawing);
    binder.bind("layoutAsync", &ManagedContextNativeModuleFactory::layoutAsync);
    binder.bind("measureAsync", &ManagedContextNativeModuleFactory::measureAsync);
    binder.bind("drawFrame", &ManagedContextNativeModuleFactory::drawFrame);
    binder.bind("drawFrameSync", &ManagedContextNativeModuleFactory::drawFrameSync);
    binder.bind("disposeFrame", &ManagedContextNativeModuleFactory::disposeFrame);
    binder.bind("rasterFrame", &ManagedContextNativeModuleFactory::rasterFrame);
    return out;
}

static Ref<SnapDrawingValdiContext> getSnapDrawingValdiContextFromCallContext(
    const Valdi::ValueFunctionCallContext& callContext) {
    return callContext.getParameter(0).checkedToValdiObject<SnapDrawingValdiContext>(callContext.getExceptionTracker());
}

Valdi::Value ManagedContextNativeModuleFactory::createValdiContextWithSnapDrawing(
    const Valdi::ValueFunctionCallContext& callContext) {
    auto context = Valdi::Context::current();
    if (context == nullptr || context->getRuntime() == nullptr) {
        callContext.getExceptionTracker().onError("Need a Context with an attached Runtime");
        return Valdi::Value();
    }

    auto useNewExternalSurfaceRasterMethod = callContext.getParameterAsBool(0);
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    auto enableDeltaRasterization = callContext.getParameterAsBool(1);
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    Ref<Valdi::IViewNodesAssetTracker> assetTracker;
    if (callContext.getParametersSize() > 2) {
        auto callback = callContext.getParameterAsFunction(2);
        if (callback == nullptr) {
            return Valdi::Value();
        }
        assetTracker = Valdi::makeShared<AssetTrackerBridge>(std::move(callback));
    }

    auto* runtime = context->getRuntime();

    auto newContext =
        runtime->getContextManager().createContext(nullptr, _viewManagerContextProvider(), /* deferRender */ false);
    auto viewNodeTree = runtime->createViewNodeTree(newContext, Valdi::ViewNodeTreeThreadAffinity::ANY);
    viewNodeTree->setRootViewWithDefaultViewClass();
    viewNodeTree->setRetainsLayoutSpecsOnInvalidateLayout(true);
    viewNodeTree->setAssetTracker(assetTracker);

    return Valdi::Value()
        .setMapValue("contextId", Valdi::Value(static_cast<int32_t>(newContext->getContextId())))
        .setMapValue("native",
                     Valdi::Value(Valdi::makeShared<SnapDrawingValdiContext>(runtime,
                                                                             viewNodeTree,
                                                                             newContext->getContextId(),
                                                                             runtime->getContextManager(),
                                                                             useNewExternalSurfaceRasterMethod,
                                                                             enableDeltaRasterization)));
}

Valdi::Value ManagedContextNativeModuleFactory::destroyValdiContextWithSnapDrawing(
    const Valdi::ValueFunctionCallContext& callContext) {
    auto snapDrawingValdiContext = getSnapDrawingValdiContextFromCallContext(callContext);
    if (snapDrawingValdiContext != nullptr) {
        snapDrawingValdiContext->destroy();
    }
    return Valdi::Value();
}

Valdi::Value ManagedContextNativeModuleFactory::layoutAsync(const Valdi::ValueFunctionCallContext& callContext) {
    auto snapDrawingValdiContext = getSnapDrawingValdiContextFromCallContext(callContext);
    if (snapDrawingValdiContext == nullptr) {
        return Valdi::Value();
    }

    auto width = static_cast<float>(callContext.getParameterAsDouble(1));
    auto height = static_cast<float>(callContext.getParameterAsDouble(2));
    auto rtl = callContext.getParameterAsBool(3);
    auto callback = callContext.getParameterAsFunction(4);
    if (callback == nullptr) {
        callContext.getExceptionTracker().onError("layoutAsync requires a callback parameter");
        return Valdi::Value();
    }

    auto runtime = snapDrawingValdiContext->getRuntime();
    if (runtime == nullptr) {
        callContext.getExceptionTracker().onError("Runtime is no longer available");
        return Valdi::Value();
    }

    auto context = Valdi::Context::currentRef();
    auto layoutDirection = rtl ? Valdi::LayoutDirectionRTL : Valdi::LayoutDirectionLTR;

    runtime->getMainThreadManager().dispatch(
        context, [snapDrawingValdiContext, callback, width, height, layoutDirection] {
            if (snapDrawingValdiContext->isDestroyed()) {
                callback->call(Valdi::ValueFunctionFlagsNone, {});
                return;
            }
            auto viewNodeTree = snapDrawingValdiContext->getViewNodeTree();
            if (viewNodeTree != nullptr) {
                viewNodeTree->setLayoutSpecs(Valdi::Size(width, height), layoutDirection);
            }
            callback->call(Valdi::ValueFunctionFlagsNone, {});
        });

    return Valdi::Value();
}

Valdi::Value ManagedContextNativeModuleFactory::measureAsync(const Valdi::ValueFunctionCallContext& callContext) {
    auto snapDrawingValdiContext = getSnapDrawingValdiContextFromCallContext(callContext);
    if (snapDrawingValdiContext == nullptr) {
        return Valdi::Value();
    }

    auto maxWidth = static_cast<float>(callContext.getParameterAsDouble(1));
    auto widthMode = static_cast<Valdi::MeasureMode>(callContext.getParameterAsInt(2));
    auto maxHeight = static_cast<float>(callContext.getParameterAsDouble(3));
    auto heightMode = static_cast<Valdi::MeasureMode>(callContext.getParameterAsInt(4));
    auto rtl = callContext.getParameterAsBool(5);
    auto callback = callContext.getParameterAsFunction(6);
    if (callback == nullptr) {
        callContext.getExceptionTracker().onError("measureAsync requires a callback parameter");
        return Valdi::Value();
    }

    auto runtime = snapDrawingValdiContext->getRuntime();
    if (runtime == nullptr) {
        callContext.getExceptionTracker().onError("Runtime is no longer available");
        return Valdi::Value();
    }

    auto context = Valdi::Context::currentRef();
    auto layoutDirection = rtl ? Valdi::LayoutDirectionRTL : Valdi::LayoutDirectionLTR;

    runtime->getMainThreadManager().dispatch(
        context, [snapDrawingValdiContext, callback, maxWidth, widthMode, maxHeight, heightMode, layoutDirection] {
            if (snapDrawingValdiContext->isDestroyed()) {
                callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(0.0), Valdi::Value(0.0)});
                return;
            }
            auto viewNodeTree = snapDrawingValdiContext->getViewNodeTree();
            if (viewNodeTree == nullptr) {
                callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(0.0), Valdi::Value(0.0)});
                return;
            }
            auto size = viewNodeTree->measureLayout(maxWidth, widthMode, maxHeight, heightMode, layoutDirection);
            callback->call(
                Valdi::ValueFunctionFlagsNone,
                {Valdi::Value(static_cast<double>(size.width)), Valdi::Value(static_cast<double>(size.height))});
        });

    return Valdi::Value();
}

Valdi::Value ManagedContextNativeModuleFactory::drawFrame(const Valdi::ValueFunctionCallContext& callContext) {
    auto snapDrawingValdiContext = getSnapDrawingValdiContextFromCallContext(callContext);
    if (snapDrawingValdiContext == nullptr) {
        return Valdi::Value();
    }

    auto callback = callContext.getParameterAsFunction(1);
    if (callback == nullptr) {
        callContext.getExceptionTracker().onError("drawFrame requires a callback parameter");
        return Valdi::Value();
    }

    auto runtime = snapDrawingValdiContext->getRuntime();
    if (runtime == nullptr) {
        callContext.getExceptionTracker().onError("Runtime is no longer available");
        return Valdi::Value();
    }

    auto context = Valdi::Context::currentRef();

    runtime->getMainThreadManager().dispatch(context, [snapDrawingValdiContext, callback] {
        if (snapDrawingValdiContext->isDestroyed()) {
            callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(), Valdi::Value(0.0)});
            return;
        }
        auto start = std::chrono::steady_clock::now();

        auto viewNodeTreeLock = snapDrawingValdiContext->getViewNodeTree()->lock();
        auto rootViewNode = snapDrawingValdiContext->getViewNodeTree()->getRootViewNode();
        if (rootViewNode == nullptr) {
            callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(), Valdi::Value(0.0)});
            return;
        }

        auto frame = rootViewNode->getCalculatedFrame();
        if (frame.isEmpty()) {
            callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(), Valdi::Value(0.0)});
            return;
        }

        auto rootLayer = valdiViewToLayer(rootViewNode->getView());
        if (rootLayer == nullptr) {
            callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(), Valdi::Value(0.0)});
            return;
        }

        rootLayer->setFrame(Rect::makeLTRB(frame.getLeft(), frame.getTop(), frame.getRight(), frame.getBottom()));
        rootLayer->layoutIfNeeded();

        auto displayList = Valdi::makeShared<DisplayList>(Size(frame.width, frame.height), TimePoint(0.0));
        DrawMetrics metrics;
        rootLayer->draw(*displayList, metrics);

        auto elapsed = std::chrono::steady_clock::now() - start;
        double mainThreadMs = std::chrono::duration<double, std::milli>(elapsed).count();

        auto snapDrawingFrame =
            Valdi::makeShared<SnapDrawingFrame>(snapDrawingValdiContext->getRasterContext(), displayList);
        callback->call(Valdi::ValueFunctionFlagsNone, {Valdi::Value(snapDrawingFrame), Valdi::Value(mainThreadMs)});
    });

    return Valdi::Value();
}

static Valdi::Value onDrawError(const Valdi::ValueFunctionCallContext& callContext) {
    callContext.getExceptionTracker().onError(
        "SnapDrawing Context has no root view or frame size. Please make sure to render at least one view with a "
        "positive size and call layout() before drawing");
    return Valdi::Value();
}

Valdi::Value ManagedContextNativeModuleFactory::drawFrameSync(const Valdi::ValueFunctionCallContext& callContext) {
    auto snapDrawingValdiContext = getSnapDrawingValdiContextFromCallContext(callContext);
    if (snapDrawingValdiContext == nullptr) {
        return Valdi::Value();
    }

    auto viewNodeTreeLock = snapDrawingValdiContext->getViewNodeTree()->lock();
    auto rootViewNode = snapDrawingValdiContext->getViewNodeTree()->getRootViewNode();
    if (rootViewNode == nullptr) {
        return onDrawError(callContext);
    }

    auto frame = rootViewNode->getCalculatedFrame();
    if (frame.isEmpty()) {
        return onDrawError(callContext);
    }

    auto rootLayer = valdiViewToLayer(rootViewNode->getView());
    if (rootLayer == nullptr) {
        return onDrawError(callContext);
    }

    rootLayer->setFrame(Rect::makeLTRB(frame.getLeft(), frame.getTop(), frame.getRight(), frame.getBottom()));
    rootLayer->layoutIfNeeded();

    auto displayList = Valdi::makeShared<DisplayList>(Size(frame.width, frame.height), TimePoint(0.0));
    DrawMetrics metrics;
    rootLayer->draw(*displayList, metrics);

    return Valdi::Value(Valdi::makeShared<SnapDrawingFrame>(snapDrawingValdiContext->getRasterContext(), displayList));
}

static Ref<SnapDrawingFrame> getSnapDrawingFrameFromCallContext(const Valdi::ValueFunctionCallContext& callContext) {
    return callContext.getParameter(0).checkedToValdiObject<SnapDrawingFrame>(callContext.getExceptionTracker());
}

Valdi::Value ManagedContextNativeModuleFactory::disposeFrame(const Valdi::ValueFunctionCallContext& callContext) {
    auto frame = getSnapDrawingFrameFromCallContext(callContext);
    if (frame != nullptr) {
        frame->dispose();
    }

    return Valdi::Value();
}

Valdi::Value ManagedContextNativeModuleFactory::rasterFrame(const Valdi::ValueFunctionCallContext& callContext) {
    auto frame = getSnapDrawingFrameFromCallContext(callContext);
    if (frame == nullptr) {
        return Valdi::Value();
    }

    if (frame->isDisposed()) {
        callContext.getExceptionTracker().onError("Frame was disposed");
        return Valdi::Value();
    }

    auto bitmap = callContext.getParameter(1).checkedToValdiObject<Valdi::IBitmap>(callContext.getExceptionTracker());
    if (bitmap == nullptr) {
        return Valdi::Value();
    }

    auto shouldClearBitmapBeforeDrawing = callContext.getParameterAsBool(2);
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    auto deltaRasterization = callContext.getParameterAsBool(3);
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    Valdi::Result<RasterContext::RasterResult> result;

    if (deltaRasterization) {
        result = frame->rasterDelta(bitmap);
    } else {
        result = frame->raster(bitmap, shouldClearBitmapBeforeDrawing);
    }

    if (!result) {
        callContext.getExceptionTracker().onError(result.moveError());
        return Valdi::Value();
    }

    // Convert damage rects to JavaScript array
    Valdi::ValueArrayBuilder damageRectsArray;
    for (const auto& rect : result.value().damageRects) {
        Valdi::Value rectObject;
        rectObject.setMapValue("x", Valdi::Value(rect.x()));
        rectObject.setMapValue("y", Valdi::Value(rect.y()));
        rectObject.setMapValue("width", Valdi::Value(rect.width()));
        rectObject.setMapValue("height", Valdi::Value(rect.height()));
        damageRectsArray.append(std::move(rectObject));
    }

    return Valdi::Value(damageRectsArray.build());
}

} // namespace snap::drawing
