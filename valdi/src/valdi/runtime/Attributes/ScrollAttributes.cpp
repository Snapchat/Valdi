//
//  ScrollAttributes.cpp
//  valdi
//
//  Created by Simon Corsin on 8/6/21.
//

#include "valdi/runtime/Attributes/ScrollAttributes.hpp"
#include "valdi/runtime/Attributes/AttributeHandlerDelegateWithCallable.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"

#include "valdi/runtime/Attributes/AttributesBinderHelper.hpp"
#include "valdi/runtime/Attributes/CompositeAttribute.hpp"
#include "valdi/runtime/Attributes/NoOpDefaultAttributeHandler.hpp"

#include <cmath>

namespace Valdi {

ScrollAttributes::ScrollAttributes(AttributeIds& attributeIds) : _attributeIds(attributeIds) {}

void ScrollAttributes::bind(AttributeHandlerById& attributes) {
    AttributesBinderHelper binder(_attributeIds, attributes);

    binder.bindViewNodeBoolean("horizontal", &ViewNode::setHorizontalScroll);
    binder.bindViewNodeInt("circularRatio", &ViewNode::setScrollCircularRatio);

    binder.bindViewNodeCallback("onScroll", &ViewNode::setOnScrollCallback);
    binder.bindViewNodeCallback("onScrollEnd", &ViewNode::setOnScrollEndCallback);
    binder.bindViewNodeCallback("onDragStart", &ViewNode::setOnDragStartCallback);
    binder.bindViewNodeCallback("onDragEnding", &ViewNode::setOnDragEndingCallback);
    binder.bindViewNodeCallback("onDragEnd", &ViewNode::setOnDragEndCallback);

    binder.bindViewNodeFloat("viewportExtensionTop", &ViewNode::setViewportExtensionTop);
    binder.bindViewNodeFloat("viewportExtensionBottom", &ViewNode::setViewportExtensionBottom);
    binder.bindViewNodeFloat("viewportExtensionLeft", &ViewNode::setViewportExtensionLeft);
    binder.bindViewNodeFloat("viewportExtensionRight", &ViewNode::setViewportExtensionRight);

    binder.bindViewNodeCallback("onContentSizeChange", &ViewNode::setOnContentSizeChangeCallback);

    std::vector<CompositeAttributePart> parts;
    parts.emplace_back(_attributeIds.getIdForName("contentOffsetX"), true);
    parts.emplace_back(_attributeIds.getIdForName("contentOffsetY"), true);
    parts.emplace_back(_attributeIds.getIdForName("contentOffsetAnimated"), true);

    binder.bindComposite(
        "contentOffset",
        std::move(parts),
        [](ViewTransactionScope& viewTransactionScope, ViewNode& viewNode, const Value& value) -> Result<Void> {
            const auto* compositeArray = value.getArray();
            if (compositeArray == nullptr || compositeArray->size() != 3) {
                return Error("Expected 3 elements in composite attribute");
            }

            auto directionAgnosticContentOffsetX = (*compositeArray)[0].toFloat();
            auto directionAgnosticContentOffsetY = (*compositeArray)[1].toFloat();
            auto animated = (*compositeArray)[2].toBool();

            if (std::isnan(directionAgnosticContentOffsetX) || std::isnan(directionAgnosticContentOffsetY)) {
                return Error("Can't set a scroll content offset with NaN value");
            }

            viewNode.setScrollContentOffset(viewTransactionScope,
                                            Point(directionAgnosticContentOffsetX, directionAgnosticContentOffsetY),
                                            animated);

            return Void();
        },
        [](ViewTransactionScope& viewTransactionScope, ViewNode& viewNode) {
            viewNode.setScrollContentOffset(viewTransactionScope, Point(), false);
        });

    binder.bindViewNodeFloat("staticContentWidth", &ViewNode::setScrollStaticContentWidth);
    binder.bindViewNodeFloat("staticContentHeight", &ViewNode::setScrollStaticContentHeight);

    binder.bindViewNodeBoolean("maintainScrollAnchor", &ViewNode::setMaintainScrollAnchor);
}

} // namespace Valdi
