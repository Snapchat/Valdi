//
//  DefaultAttributes.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/10/18.
//

#include "valdi/runtime/Attributes/DefaultAttributes.hpp"
#include "valdi/runtime/Attributes/Yoga/YogaAttributes.hpp"

#include "valdi/runtime/Attributes/AttributesBinderHelper.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"

namespace Valdi {

DefaultAttributes::DefaultAttributes(YGConfig* yogaConfig, AttributeIds& attributeIds, float pointScale)
    : _attributeIds(attributeIds),
      _yogaAttributes(Valdi::makeShared<YogaAttributes>(yogaConfig, attributeIds, pointScale)),
      _accessibilityAttributes(Valdi::makeShared<AccessibilityAttributes>(attributeIds)),
      _scrollAnchorAttributes(Valdi::makeShared<ScrollAnchorAttributes>(attributeIds)) {}

void DefaultAttributes::bind(AttributeHandlerById& attributes) {
    _yogaAttributes->bind(attributes);
    _accessibilityAttributes->bind(attributes);
    _scrollAnchorAttributes->bind(attributes);

    AttributesBinderHelper binder(_attributeIds, attributes);

    binder.bindViewNodeCallback("onViewCreate", &ViewNode::setOnViewCreatedCallback);
    binder.bindViewNodeCallback("onViewDestroy", &ViewNode::setOnViewDestroyedCallback);
    binder.bindViewNodeCallback("onViewChange", &ViewNode::setOnViewChangedCallback);
    binder.bindViewNodeCallback("onLayoutComplete", &ViewNode::setOnLayoutCompletedCallback);
    binder.bindViewNodeCallback("onMeasure", &ViewNode::setOnMeasureCallback);

    binder.bindViewNodeBoolean("lazyLayout", &ViewNode::setPrefersLazyLayout);
    binder.bindViewNodeBoolean("animationsEnabled", &ViewNode::setAnimationsEnabled);
    binder.bindViewNodeBoolean("observeVisibility", &ViewNode::setShouldReceiveVisibilityUpdates);
    binder.bindViewNodeBoolean("extendViewportWithChildren", &ViewNode::setExtendViewportWithChildren);
    binder.bindViewNodeBoolean("ignoreParentViewport", &ViewNode::setIgnoreParentViewport);
    binder.bindViewNodeBoolean("canAlwaysScrollHorizontal", &ViewNode::setCanAlwaysScrollHorizontal);
    binder.bindViewNodeBoolean("canAlwaysScrollVertical", &ViewNode::setCanAlwaysScrollVertical);

    binder.bindViewNodeFloat("estimatedWidth", &ViewNode::setEstimatedWidth);
    binder.bindViewNodeFloat("estimatedHeight", &ViewNode::setEstimatedHeight);

    binder.bind(
        "limitToViewport",
        [](ViewTransactionScope& /*viewTransactionScope*/, ViewNode& viewNode, const Value& value) -> Result<Void> {
            if (value.toBool()) {
                viewNode.setLimitToViewport(LimitToViewportEnabled);
            } else {
                viewNode.setLimitToViewport(LimitToViewportDisabled);
            }
            return Void();
        },
        [](ViewTransactionScope& /*viewTransactionScope*/, auto& viewNode) {
            viewNode.setLimitToViewport(LimitToViewportUnset);
        });

    binder.bind(
        "lazy",
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode, const auto& value) -> Result<Void> {
            if (value.toBool()) {
                viewNode.setPrefersLazyLayout(viewTransactionScope, true);
                viewNode.setLimitToViewport(LimitToViewportEnabled);
            } else {
                viewNode.setPrefersLazyLayout(viewTransactionScope, false);
                viewNode.setLimitToViewport(LimitToViewportDisabled);
            }
            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode) {
            viewNode.setPrefersLazyLayout(viewTransactionScope, false);
            viewNode.setLimitToViewport(LimitToViewportUnset);
        });

    binder.bindViewNodeInt("zIndex", &ViewNode::setZIndex);

    binder.bind(
        "iosClass",
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode, const Value& value) -> Result<Void> {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, value.toStringBox(), PlatformTypeIOS);
            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode) {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, StringBox(), PlatformTypeIOS);
        });

    binder.bind(
        "androidClass",
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode, const Value& value) -> Result<Void> {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, value.toStringBox(), PlatformTypeAndroid);
            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode) {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, StringBox(), PlatformTypeAndroid);
        });

    binder.bind(
        "macosClass",
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode, const Value& value) -> Result<Void> {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, value.toStringBox(), PlatformTypeMacOS);
            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode) {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, StringBox(), PlatformTypeMacOS);
        });

    binder.bind(
        "webClass",
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode, const Value& value) -> Result<Void> {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, value.toStringBox(), PlatformTypeWeb);
            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode) {
            viewNode.setViewClassNameForPlatform(viewTransactionScope, StringBox(), PlatformTypeWeb);
        });

    binder.bind(
        "viewFactory",
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode, const Value& value) -> Result<Void> {
            auto viewFactory =
                viewNode.getViewNodeTree()->getCompatibleViewFactory(castOrNull<ViewFactory>(value.getValdiObject()));
            viewNode.setViewFactory(viewTransactionScope, viewFactory);
            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, auto& viewNode) {
            viewNode.setViewFactory(viewTransactionScope, nullptr);
        });
}

} // namespace Valdi
