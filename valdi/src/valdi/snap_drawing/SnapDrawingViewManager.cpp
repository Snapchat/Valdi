//
//  SnapDrawingViewManager.cpp
//  valdi-skia-app
//
//  Created by Simon Corsin on 6/28/20.
//

#include "valdi/snap_drawing/SnapDrawingViewManager.hpp"

#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/DeferredViewTransaction.hpp"
#include "valdi/runtime/Views/PlaceholderViewMeasureDelegate.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

#include "valdi/snap_drawing/Animations/ValdiAnimator.hpp"

#include "snap_drawing/cpp/Animations/ValueInterpolators.hpp"

#include "valdi/snap_drawing/Layers/BridgeLayer.hpp"
#include "valdi/snap_drawing/SnapDrawingViewTransaction.hpp"

#include "snap_drawing/cpp/Drawing/DrawingContext.hpp"

#include "snap_drawing/cpp/Layers/ButtonLayer.hpp"
#include "snap_drawing/cpp/Layers/ImageLayer.hpp"
#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"
#include "snap_drawing/cpp/Layers/SpinnerLayer.hpp"
#include "snap_drawing/cpp/Layers/TextLayer.hpp"

#include "valdi/snap_drawing/Layers/Classes/AnimatedImageLayerClass.hpp"
#include "valdi/snap_drawing/Layers/Classes/ImageLayerClass.hpp"
#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Layers/Classes/ScrollLayerClass.hpp"
#include "valdi/snap_drawing/Layers/Classes/SpinnerLayerClass.hpp"
#include "valdi/snap_drawing/Layers/Classes/TextLayerClass.hpp"
#include "valdi/snap_drawing/Layers/Classes/ValdiShapeLayerClass.hpp"

#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Utils/BitmapFactory.hpp"

#include "utils/debugging/Assert.hpp"
#include "utils/time/StopWatch.hpp"

namespace snap::drawing {

static Valdi::Ref<BridgeLayer> getBridgeLayer(const Valdi::Ref<Valdi::View>& view) {
    return Valdi::castOrNull<BridgeLayer>(valdiViewToLayer(view));
}

static Valdi::Ref<Valdi::View> getBridgeView(const Valdi::Ref<Valdi::View>& view) {
    auto layer = getBridgeLayer(view);
    return layer != nullptr ? layer->getView() : Valdi::Ref<Valdi::View>();
}

static Valdi::Ref<Valdi::View> makeBridgeLayer(const Valdi::Ref<Resources>& resources,
                                               const Valdi::Ref<Valdi::View>& sourceView,
                                               Valdi::IViewManager& viewManager) {
    auto snapDrawingView = snap::drawing::makeLayer<BridgeLayer>(resources);
    snapDrawingView->setBridgedView(Valdi::makeShared<BridgedView>(sourceView, viewManager));
    return layerToValdiView(snapDrawingView, true);
}

class BridgeAttributeHandlerDelegate : public Valdi::AttributeHandlerDelegate {
public:
    explicit BridgeAttributeHandlerDelegate(const Valdi::Ref<Valdi::AttributeHandlerDelegate>& sourceDelegate,
                                            Valdi::IViewManager& hostViewManager)
        : _sourceDelegate(sourceDelegate), _hostViewManager(hostViewManager) {}

    Valdi::Result<Valdi::Void> onApply(Valdi::ViewTransactionScope& viewTransactionScope,
                                       Valdi::ViewNode& viewNode,
                                       const Valdi::Ref<Valdi::View>& view,
                                       const Valdi::StringBox& name,
                                       const Valdi::Value& value,
                                       const Valdi::Ref<Valdi::Animator>& /*animator*/) override {
        auto layer = getBridgeLayer(view);
        if (layer == nullptr) {
            return Valdi::Void();
        }
        if (_sourceDelegate == nullptr) {
            return Valdi::Void();
        }
        auto result = _sourceDelegate->onApply(
            viewTransactionScope.withViewManager(_hostViewManager), viewNode, layer->getView(), name, value, nullptr);

        layer->setNeedsDisplay();

        return result;
    }

    void onReset(Valdi::ViewTransactionScope& viewTransactionScope,
                 Valdi::ViewNode& viewNode,
                 const Valdi::Ref<Valdi::View>& view,
                 const Valdi::StringBox& name,
                 const Valdi::Ref<Valdi::Animator>& /*animator*/) override {
        auto layer = getBridgeLayer(view);
        if (layer == nullptr) {
            return;
        }
        if (_sourceDelegate == nullptr) {
            return;
        }
        _sourceDelegate->onReset(
            viewTransactionScope.withViewManager(_hostViewManager), viewNode, layer->getView(), name, nullptr);

        layer->setNeedsDisplay();
    }

private:
    Valdi::Ref<Valdi::AttributeHandlerDelegate> _sourceDelegate;
    Valdi::IViewManager& _hostViewManager;
};

class BridgePlaceholderViewMeasureDelegate : public Valdi::PlaceholderViewMeasureDelegate {
public:
    BridgePlaceholderViewMeasureDelegate(const Ref<Resources>& resources,
                                         const Ref<Valdi::PlaceholderViewMeasureDelegate>& sourceMeasureDelegate,
                                         Valdi::IViewManager& hostViewManager)
        : _resources(resources), _sourceMeasureDelegate(sourceMeasureDelegate), _hostViewManager(hostViewManager) {}

    ~BridgePlaceholderViewMeasureDelegate() override = default;

    Valdi::Size measureView(const Valdi::Ref<Valdi::View>& view,
                            float width,
                            Valdi::MeasureMode widthMode,
                            float height,
                            Valdi::MeasureMode heightMode) override {
        auto bridgeView = getBridgeView(view);
        if (bridgeView == nullptr) {
            return Valdi::Size(0, 0);
        }
        return _sourceMeasureDelegate->measureView(bridgeView, width, widthMode, height, heightMode);
    }

    Valdi::Ref<Valdi::View> createPlaceholderView() override {
        auto sourceView = _sourceMeasureDelegate->createPlaceholderView();
        return makeBridgeLayer(_resources, sourceView, _hostViewManager);
    }

private:
    Ref<Resources> _resources;
    Ref<Valdi::PlaceholderViewMeasureDelegate> _sourceMeasureDelegate;
    Valdi::IViewManager& _hostViewManager;
};

class AttributesBindingContextProxy : public Valdi::AttributesBindingContext {
public:
    AttributesBindingContextProxy(const Ref<Resources>& resources,
                                  Valdi::AttributesBindingContext& sourceBinder,
                                  Valdi::IViewManager& hostViewManager)
        : _resources(resources), _sourceBinder(sourceBinder), _hostViewManager(hostViewManager) {}

    ~AttributesBindingContextProxy() override = default;

    void registerPreprocessor(const Valdi::StringBox& attribute,
                              bool enableCache,
                              Valdi::AttributePreprocessor&& preprocessor) override {
        _sourceBinder.registerPreprocessor(attribute, enableCache, std::move(preprocessor));
    }

    Valdi::AttributeId bindStringAttribute(const Valdi::StringBox& attribute,
                                           bool invalidateLayoutOnChange,
                                           const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindStringAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindDoubleAttribute(const Valdi::StringBox& attribute,
                                           bool invalidateLayoutOnChange,
                                           const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindDoubleAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindPercentAttribute(const Valdi::StringBox& attribute,
                                            bool invalidateLayoutOnChange,
                                            const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindPercentAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindBooleanAttribute(const Valdi::StringBox& attribute,
                                            bool invalidateLayoutOnChange,
                                            const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindBooleanAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindIntAttribute(const Valdi::StringBox& attribute,
                                        bool invalidateLayoutOnChange,
                                        const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindIntAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindColorAttribute(const Valdi::StringBox& attribute,
                                          bool invalidateLayoutOnChange,
                                          const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindColorAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindBorderAttribute(const Valdi::StringBox& attribute,
                                           bool invalidateLayoutOnChange,
                                           const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindBorderAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindUntypedAttribute(const Valdi::StringBox& attribute,
                                            bool invalidateLayoutOnChange,
                                            const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindUntypedAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindTextAttribute(const Valdi::StringBox& attribute,
                                         bool invalidateLayoutOnChange,
                                         const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindTextAttribute(
            attribute,
            invalidateLayoutOnChange,
            Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    Valdi::AttributeId bindCompositeAttribute(const Valdi::StringBox& attribute,
                                              const std::vector<snap::valdi_core::CompositeAttributePart>& parts,
                                              const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {
        return _sourceBinder.bindCompositeAttribute(
            attribute, parts, Valdi::makeShared<BridgeAttributeHandlerDelegate>(delegate, _hostViewManager));
    }

    void setDefaultDelegate(const Valdi::Ref<Valdi::AttributeHandlerDelegate>& delegate) override {}

    void setMeasureDelegate(const Valdi::Ref<Valdi::MeasureDelegate>& measureDelegate) override {
        auto measurePlaceholderViewDelegate = Valdi::castOrNull<Valdi::PlaceholderViewMeasureDelegate>(measureDelegate);
        if (measurePlaceholderViewDelegate != nullptr) {
            _sourceBinder.setMeasureDelegate(Valdi::makeShared<BridgePlaceholderViewMeasureDelegate>(
                _resources, measurePlaceholderViewDelegate, _hostViewManager));
        } else {
            _sourceBinder.setMeasureDelegate(measureDelegate);
        }
    }

    void bindScrollAttributes() override {
        _sourceBinder.bindScrollAttributes();
    }

    void bindAssetAttributes(snap::valdi_core::AssetOutputType assetOutputType) override {
        _sourceBinder.bindAssetAttributes(assetOutputType);
    }

private:
    Ref<Resources> _resources;
    Valdi::AttributesBindingContext& _sourceBinder;
    Valdi::IViewManager& _hostViewManager;
};

class SnapDrawingViewFactory : public Valdi::ViewFactory {
public:
    SnapDrawingViewFactory(Valdi::StringBox viewClassName,
                           Valdi::IViewManager& viewManager,
                           Valdi::Ref<Valdi::BoundAttributes> boundAttributes,
                           const Ref<ILayerClass>& viewClass)
        : Valdi::ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)),
          _viewClass(viewClass) {}

    ~SnapDrawingViewFactory() override = default;

protected:
    Valdi::Ref<Valdi::View> doCreateView(Valdi::ViewNodeTree* /*viewNodeTree*/,
                                         Valdi::ViewNode* /*viewNode*/) override {
        auto view = _viewClass->instantiate();

        return layerToValdiView(view, true);
    }

private:
    Ref<ILayerClass> _viewClass;
};

class BridgeLayerFactory : public Valdi::ViewFactory {
public:
    BridgeLayerFactory(const Ref<Resources>& resources,
                       const Ref<Valdi::ViewFactory>& factory,
                       Valdi::IViewManager& viewManager,
                       Valdi::IViewManager& hostViewManager,
                       Valdi::Ref<Valdi::BoundAttributes> boundAttributes)
        : Valdi::ViewFactory(factory->getViewClassName(), viewManager, std::move(boundAttributes)),
          _resources(resources),
          _viewFactory(factory),
          _hostViewManager(hostViewManager) {
        // These are always custom
        setIsUserSpecified(factory->isUserSpecified());
    }

    ~BridgeLayerFactory() override = default;

protected:
    Valdi::Ref<Valdi::View> doCreateView(Valdi::ViewNodeTree* viewNodeTree, Valdi::ViewNode* viewNode) override {
        auto view = _viewFactory->createView(viewNodeTree, viewNode, false);
        return makeBridgeLayer(_resources, view, _hostViewManager);
    }

private:
    Ref<Resources> _resources;
    Ref<Valdi::ViewFactory> _viewFactory;
    Valdi::IViewManager& _hostViewManager;
};

SnapDrawingViewManager::SnapDrawingViewManager(const Ref<Resources>& resources, Valdi::PlatformType platformType)
    : _logger(resources->getLogger()), _platformType(platformType), _resources(resources) {
    auto layerClass = Valdi::makeShared<LayerClass>(_resources);
    _baseLayerClassName = getClassName(*layerClass);
    auto textLayerClass = Valdi::makeShared<TextLayerClass>(_resources, layerClass);
    registerLayerClass(layerClass);
    registerLayerClass(textLayerClass);
    registerLayerClass(
        Valdi::makeShared<ScrollLayerClass>(_resources, platformType == Valdi::PlatformTypeAndroid, layerClass));
    registerLayerClass(Valdi::makeShared<SpinnerLayerClass>(_resources, layerClass));
    registerLayerClass(Valdi::makeShared<ValdiShapeLayerClass>(_resources, layerClass));

    auto imageViewClass = Valdi::makeShared<ImageLayerClass>(_resources, layerClass);
    registerLayerClass(imageViewClass);
    registerLayerClass(Valdi::makeShared<AnimatedImageLayerClass>(_resources, layerClass));
}

SnapDrawingViewManager::~SnapDrawingViewManager() = default;

Valdi::StringBox SnapDrawingViewManager::getClassName(const ILayerClass& layerClass) const {
    switch (_platformType) {
        case Valdi::PlatformTypeAndroid:
            return STRING_LITERAL(layerClass.getAndroidClassName());
        case Valdi::PlatformTypeIOS:
        case Valdi::PlatformTypeMacOS:
        case Valdi::PlatformTypeWeb:
            return STRING_LITERAL(layerClass.getIOSClassName());
    }
}

Valdi::PlatformType SnapDrawingViewManager::getPlatformType() const {
    return _platformType;
}

Valdi::RenderingBackendType SnapDrawingViewManager::getRenderingBackendType() const {
    return Valdi::RenderingBackendTypeSnapDrawing;
}

Valdi::StringBox SnapDrawingViewManager::getDefaultViewClass() {
    return _baseLayerClassName;
}

Valdi::Ref<Valdi::ViewFactory> SnapDrawingViewManager::createViewFactory(
    const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes>& boundAttributes) {
    if (shouldBridgeLayerClass(className)) {
        auto hostViewFactory = _hostViewManager->createViewFactory(className, nullptr);
        if (hostViewFactory.get() != nullptr) {
            return Valdi::makeShared<BridgeLayerFactory>(
                _resources, hostViewFactory, *this, *_hostViewManager, boundAttributes);
        }
        // Host doesn't actually provide this class (e.g. "Layout" on macOS); use layer fallback
    }
    {
        auto layerClass = getLayerClass(className);
        if (layerClass == nullptr) {
            VALDI_ERROR(_logger,
                        "Unable to resolve view factory for class {}, falling back to {}",
                        className,
                        getDefaultViewClass());
            layerClass = getBaseLayerClass();
            SC_ASSERT(layerClass != nullptr);
        }

        return Valdi::makeShared<SnapDrawingViewFactory>(className, *this, boundAttributes, layerClass);
    }
}

Valdi::Ref<Valdi::ViewFactory> SnapDrawingViewManager::bridgeViewFactory(
    const Valdi::Ref<Valdi::ViewFactory>& viewFactory, const Valdi::Ref<Valdi::BoundAttributes>& startingAttributes) {
    Valdi::AttributeHandlerById handlers;

    for (const auto& boundAttribute : viewFactory->getBoundAttributes()->getHandlers()) {
        if (boundAttribute.second.isCompositePart()) {
            handlers[boundAttribute.first] = boundAttribute.second;
        } else {
            handlers[boundAttribute.first] =
                boundAttribute.second.withDelegate(Valdi::makeShared<BridgeAttributeHandlerDelegate>(
                    boundAttribute.second.getDelegate(), *_hostViewManager));
        }
    }

    auto boundAttributes =
        startingAttributes->merge(viewFactory->getViewClassName(),
                                  handlers,
                                  viewFactory->getBoundAttributes()->getDefaultAttributeHandlerDelegate(),
                                  viewFactory->getBoundAttributes()->getMeasureDelegate(),
                                  viewFactory->getBoundAttributes()->isBackingClassScrollable(),
                                  /* allowOverride */ false);
    return Valdi::makeShared<BridgeLayerFactory>(
        _resources, viewFactory, *this, viewFactory->getViewManager(), boundAttributes);
}

void SnapDrawingViewManager::callAction(Valdi::ViewNodeTree* viewNodeTree,
                                        const Valdi::StringBox& actionName,
                                        const Valdi::Ref<Valdi::ValueArray>& parameters) {
    if (_hostViewManager != nullptr) {
        _hostViewManager->callAction(viewNodeTree, actionName, parameters);
        return;
    }
}

Valdi::NativeAnimator SnapDrawingViewManager::createAnimator(snap::valdi_core::AnimationType type,
                                                             const std::vector<double>& /*controlPoints*/,
                                                             double duration,
                                                             bool beginFromCurrentState,
                                                             bool /*crossfade*/,
                                                             double stiffness,
                                                             double damping) {
    InterpolationFunction interpolationFunction;
    switch (type) {
        case snap::valdi_core::AnimationType::Linear:
            interpolationFunction = InterpolationFunctions::linear();
            break;
        case snap::valdi_core::AnimationType::EaseIn:
            interpolationFunction = InterpolationFunctions::easeIn();
            break;
        case snap::valdi_core::AnimationType::EaseOut:
            interpolationFunction = InterpolationFunctions::easeOut();
            break;
        case snap::valdi_core::AnimationType::EaseInOut:
            interpolationFunction = InterpolationFunctions::easeInOut();
            break;
    }

    return Valdi::makeShared<ValdiAnimator>(
               Duration(duration), std::move(interpolationFunction), beginFromCurrentState, stiffness, damping)
        .toShared();
}

void SnapDrawingViewManager::generateClassHierarchy(const ILayerClass& layerClass,
                                                    std::vector<Valdi::StringBox>& out) const {
    out.emplace_back(getClassName(layerClass));

    const auto& parentClass = layerClass.getParent();
    if (parentClass != nullptr) {
        generateClassHierarchy(*parentClass, out);
    }
}

std::vector<Valdi::StringBox> SnapDrawingViewManager::getClassHierarchy(const Valdi::StringBox& className) {
    if (shouldBridgeLayerClass(className)) {
        auto hierarchy = _hostViewManager->getClassHierarchy(className);

        // We make up a class hierarchy where we put the default skia view class at the end, and any view classes
        // that the host view manager knows how to handle before.

        auto eraseIt = std::find(hierarchy.begin(), hierarchy.end(), getDefaultViewClass());
        if (eraseIt != hierarchy.end()) {
            hierarchy.erase(eraseIt, hierarchy.end());
        }

        eraseIt = std::find(hierarchy.begin(), hierarchy.end(), STRING_LITERAL("android.view.View"));
        if (eraseIt != hierarchy.end()) {
            hierarchy.erase(eraseIt, hierarchy.end());
        }
        // We put at the end our default view class, so that attributes which can natively handle
        // won't go through the host view manager
        hierarchy.emplace_back(getDefaultViewClass());

        return hierarchy;
    } else {
        auto layerClass = getLayerClass(className);
        std::vector<Valdi::StringBox> out;
        if (layerClass != nullptr) {
            generateClassHierarchy(*layerClass, out);
        }
        return out;
    }
}

void SnapDrawingViewManager::bindAttributes(const Valdi::StringBox& className,
                                            Valdi::AttributesBindingContext& binder) {
    if (shouldBridgeLayerClass(className)) {
        AttributesBindingContextProxy proxy(_resources, binder, *_hostViewManager);
        _hostViewManager->bindAttributes(className, proxy);
    } else {
        auto layerClass = getLayerClass(className);
        if (layerClass == nullptr) {
            // bind default attributes if we dont have a view factory
            layerClass = getBaseLayerClass();
        }

        if (layerClass != nullptr) {
            if (layerClass->canBeMeasured()) {
                binder.setMeasureDelegate(layerClass);
            }
            layerClass->bindAttributes(binder);
        }
    }
}

bool SnapDrawingViewManager::shouldBridgeLayerClass(const Valdi::StringBox& layerClassName) {
    if (getLayerClass(layerClassName) != nullptr) {
        return false;
    }
    if (_hostViewManager == nullptr) {
        return false;
    }
    return _hostViewManager->supportsClassNameNatively(layerClassName);
}

float SnapDrawingViewManager::getPointScale() const {
    if (_hostViewManager != nullptr) {
        return _hostViewManager->getPointScale();
    }

    return 1;
}

Valdi::Ref<ILayerClass> SnapDrawingViewManager::getLayerClass(const Valdi::StringBox& className) const {
    const auto& it = _layerClasses.find(className);
    if (it == _layerClasses.end()) {
        return nullptr;
    }
    return it->second;
}

void SnapDrawingViewManager::registerLayerClass(const Valdi::Ref<ILayerClass>& layerClass) {
    _layerClasses[getClassName(*layerClass)] = layerClass;
}

Valdi::Ref<ILayerClass> SnapDrawingViewManager::getBaseLayerClass() const {
    return getLayerClass(_baseLayerClassName);
}

void SnapDrawingViewManager::setHostViewManager(Valdi::IViewManager* hostViewManager) {
    _hostViewManager = hostViewManager;
}

Valdi::Value SnapDrawingViewManager::createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode>& viewNode,
                                                           bool wrapInPlatformReference) {
    if (_hostViewManager != nullptr) {
        return _hostViewManager->createViewNodeWrapper(viewNode, wrapInPlatformReference);
    }
    return Valdi::Value::undefined();
}

Valdi::Ref<Valdi::IViewTransaction> SnapDrawingViewManager::createViewTransaction(
    const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager, bool shouldDefer) {
    if (!shouldDefer || mainThreadManager == nullptr || mainThreadManager->currentThreadIsMainThread()) {
        return Valdi::makeShared<SnapDrawingViewTransaction>();
    } else {
        return Valdi::makeShared<Valdi::DeferredViewTransaction>(*this, *mainThreadManager);
    }
}

void SnapDrawingViewManager::registerTypeface(const Valdi::StringBox& fontFamily,
                                              FontStyle fontStyle,
                                              bool canUseAsFallback,
                                              const Valdi::BytesView& data) {
    _resources->getFontManager()->registerTypeface(fontFamily, fontStyle, canUseAsFallback, data);
}

const Ref<Resources>& SnapDrawingViewManager::getResources() const {
    return _resources;
}

Valdi::Ref<Valdi::IBitmapFactory> SnapDrawingViewManager::getViewRasterBitmapFactory() const {
    return snap::drawing::BitmapFactory::getInstance(Valdi::ColorType::ColorTypeRGBA8888);
}

} // namespace snap::drawing
