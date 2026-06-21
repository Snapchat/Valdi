//
//  StandaloneViewManager.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#include "valdi/standalone_runtime/StandaloneViewManager.hpp"
#include "valdi/standalone_runtime/StandaloneNodeRef.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"
#include "valdi/standalone_runtime/StandaloneViewTransaction.hpp"

#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/DeferredViewTransaction.hpp"
#include "valdi/runtime/Views/View.hpp"
#include "valdi/runtime/Views/ViewAttributeHandlerDelegate.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi_core/CompositeAttributePart.hpp"

#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"

namespace Valdi {

// A view class which artifically takes 10ms to inflate
const auto kSlowViewName = STRING_LITERAL("SlowView");

class DummyViewFactory : public Valdi::ViewFactory {
private:
    bool _keepAttributesHistory = false;
    bool _allowPooling = false;

public:
    DummyViewFactory(Valdi::StringBox viewClassName,
                     Valdi::IViewManager& viewManager,
                     Valdi::Ref<Valdi::BoundAttributes> boundAttributes,
                     bool keepAttributesHistory,
                     bool allowPooling)
        : Valdi::ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)),
          _keepAttributesHistory(keepAttributesHistory),
          _allowPooling(allowPooling) {}

protected:
    Ref<View> doCreateView(Valdi::ViewNodeTree* /*viewNodeTree*/, Valdi::ViewNode* /*viewNode*/) override {
        const auto& className = getViewClassName();
        if (className == kSlowViewName) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return Valdi::makeShared<StandaloneView>(className, _keepAttributesHistory, _allowPooling);
    }
};

class DummyAttributeHandlerDelegate : public ViewAttributeHandlerDelegate {
protected:
    Valdi::Result<Valdi::Void> onViewApply(const Ref<View>& view,
                                           const Valdi::StringBox& name,
                                           const Valdi::Value& value,
                                           const Valdi::Ref<Valdi::Animator>& /*animator*/) override {
        StandaloneView::unwrap(view)->setAttribute(name, value);
        return Valdi::Void();
    }

    void onViewReset(const Ref<View>& view,
                     const Valdi::StringBox& name,
                     const Valdi::Ref<Valdi::Animator>& /*animator*/) override {
        StandaloneView::unwrap(view)->setAttribute(name, Value::undefined());
    }
};

StandaloneViewManager::StandaloneViewManager() = default;

Valdi::Ref<Valdi::ViewFactory> StandaloneViewManager::createViewFactory(
    const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes>& boundAttributes) {
    auto factory = Valdi::makeShared<DummyViewFactory>(
        className, *this, boundAttributes, _keepAttributesHistory, _allowViewPooling);
    if (_managedChildFrameClasses.find(className) != _managedChildFrameClasses.end()) {
        factory->setManagesChildFrames(true);
    }
    return factory;
}

void StandaloneViewManager::callAction(ViewNodeTree* /*viewNodeTree*/,
                                       const Valdi::StringBox& /*actionName*/,
                                       const Ref<ValueArray>& /*parameters*/) {}

PlatformType StandaloneViewManager::getPlatformType() const {
    return PlatformTypeIOS;
}

RenderingBackendType StandaloneViewManager::getRenderingBackendType() const {
    return RenderingBackendTypeNative;
}

StringBox StandaloneViewManager::getDefaultViewClass() {
    auto kDefaultViewClass = STRING_LITERAL("SCValdiView");
    return kDefaultViewClass;
}

NativeAnimator StandaloneViewManager::createAnimator(snap::valdi_core::AnimationType /*type*/,
                                                     const std::vector<double>& /*controlPoints*/,
                                                     double /*duration*/,
                                                     bool /*beginFromCurrentState*/,
                                                     bool /*crossfade*/,
                                                     double /*stiffness*/,
                                                     double /*damping*/) {
    return nullptr;
}

std::vector<StringBox> StandaloneViewManager::getClassHierarchy(const StringBox& className) {
    std::vector<Valdi::StringBox> hierarchy;
    hierarchy.push_back(className);
    if (className != "UIView") {
        hierarchy.push_back(STRING_LITERAL("UIView"));
    }

    return hierarchy;
}

void doRegisterAttribute(const std::string_view& attributeName, Valdi::AttributesBindingContext& binder) {
    auto attributeNameBox = Valdi::StringCache::getGlobal().makeStringFromLiteral(attributeName);
    binder.bindUntypedAttribute(attributeNameBox, false, Valdi::makeShared<DummyAttributeHandlerDelegate>());
}

void doRegisterTextAttribute(const std::string_view& attributeName, Valdi::AttributesBindingContext& binder) {
    auto attributeNameBox = Valdi::StringCache::getGlobal().makeStringFromLiteral(attributeName);
    binder.bindTextAttribute(attributeNameBox, false, Valdi::makeShared<DummyAttributeHandlerDelegate>());
}

void StandaloneViewManager::bindAttributes(const Valdi::StringBox& className, Valdi::AttributesBindingContext& binder) {
    binder.setDefaultDelegate(Valdi::makeShared<DummyAttributeHandlerDelegate>());

    if (!_registerCustomAttributes) {
        return;
    }

    if (className == "UIView") {
        doRegisterAttribute("id", binder);
        doRegisterAttribute("class", binder);
        doRegisterAttribute("color", binder);
        doRegisterAttribute("dummyHeight", binder);
        doRegisterAttribute("padding", binder);
        doRegisterAttribute("onTap", binder);
        doRegisterAttribute("justifyContent", binder);
        doRegisterAttribute("alignItems", binder);
        doRegisterAttribute("top", binder);
        doRegisterAttribute("opacity", binder);
        doRegisterAttribute("border", binder);
        doRegisterAttribute("boxShadow", binder);
        doRegisterAttribute("borderRadius", binder);
        doRegisterAttribute("background", binder);
        doRegisterAttribute("backgroundColor", binder);
    }

    if (className == "UIButton" || className == "SCValdiLabel" || className == "SCValdiTextView") {
        doRegisterTextAttribute("value", binder);
        doRegisterAttribute("font", binder);
        doRegisterAttribute("text", binder);
        doRegisterAttribute("numberOfLines", binder);
        doRegisterAttribute("fontSize", binder);
        doRegisterAttribute("fontFamily", binder);
        doRegisterAttribute("fontWeight", binder);
        doRegisterAttribute("lineHeight", binder);
        doRegisterAttribute("textAlign", binder);
        doRegisterAttribute("textDecoration", binder);
    }

    if (className == "SCValdiImageView") {
        binder.bindAssetAttributes(snap::valdi_core::AssetOutputType::Dummy);
    }

    if (className == "SCValdiScrollView") {
        binder.bindScrollAttributes();
    }

    if (className == "UIRectangleView") {
        std::vector<snap::valdi_core::CompositeAttributePart> parts;
        parts.emplace_back(STRING_LITERAL("left"), snap::valdi_core::AttributeType::Double, false, false);
        parts.emplace_back(STRING_LITERAL("right"), snap::valdi_core::AttributeType::Double, false, false);
        parts.emplace_back(STRING_LITERAL("top"), snap::valdi_core::AttributeType::Double, false, false);
        parts.emplace_back(STRING_LITERAL("bottom"), snap::valdi_core::AttributeType::Double, false, false);
        parts.emplace_back(STRING_LITERAL("rStyle"), snap::valdi_core::AttributeType::String, true, false);

        auto rectangle = STRING_LITERAL("rectangle");
        auto delegate = Valdi::makeShared<DummyAttributeHandlerDelegate>();
        binder.bindCompositeAttribute(rectangle, parts, delegate);
    }
}

Valdi::Value StandaloneViewManager::createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode>& viewNode,
                                                          bool wrapInPlatformReference) {
    return Valdi::Value(makeShared<StandaloneNodeRef>(viewNode, wrapInPlatformReference));
}

Ref<IViewTransaction> StandaloneViewManager::createViewTransaction(const Ref<MainThreadManager>& mainThreadManager,
                                                                   bool shouldDefer) {
    if (mainThreadManager == nullptr || mainThreadManager->currentThreadIsMainThread() ||
        (!shouldDefer && !_alwaysRenderInMainThread)) {
        return makeShared<StandaloneViewTransaction>();
    } else {
        return Valdi::makeShared<Valdi::DeferredViewTransaction>(*this, *mainThreadManager);
    }
}

Ref<IBitmapFactory> StandaloneViewManager::getViewRasterBitmapFactory() const {
    return nullptr;
}

void StandaloneViewManager::setKeepAttributesHistory(bool keepAttributesHistory) {
    _keepAttributesHistory = keepAttributesHistory;
}

void StandaloneViewManager::setRegisterCustomAttributes(bool registerCustomAttributes) {
    _registerCustomAttributes = registerCustomAttributes;
}

void StandaloneViewManager::setAllowViewPooling(bool allowViewPooling) {
    _allowViewPooling = allowViewPooling;
}

void StandaloneViewManager::setAlwaysRenderInMainThread(bool alwaysRenderInMainThread) {
    _alwaysRenderInMainThread = alwaysRenderInMainThread;
}

void StandaloneViewManager::setManagesChildFramesForClass(const StringBox& className, bool managesChildFrames) {
    if (managesChildFrames) {
        _managedChildFrameClasses.insert(className);
    } else {
        _managedChildFrameClasses.erase(className);
    }
}

} // namespace Valdi
