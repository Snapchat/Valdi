//
//  YogaAttributeHandlerDelegate.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#include "valdi/runtime/Attributes/Yoga/YogaAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/Yoga/YogaAttributes.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi_core/cpp/Views/Measure.hpp"

#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"

namespace Valdi {

YogaAttributeHandlerDelegate::YogaAttributeHandlerDelegate() = default;
YogaAttributeHandlerDelegate::~YogaAttributeHandlerDelegate() = default;

Result<Void> YogaAttributeHandlerDelegate::onApply(ViewTransactionScope& /*viewTransactionScope*/,
                                                   ViewNode& viewNode,
                                                   const Ref<View>& /*view*/,
                                                   const StringBox& /*name*/,
                                                   const Value& value,
                                                   const Ref<Animator>& /*animator*/) {
    auto* nodeRef = getNodeRef(viewNode);
    if (nodeRef == nullptr) {
        return Error("Unable to resolve Flexbox node");
    }

    auto result = onApply(nodeRef, value);
    if (!result) {
        return result;
    }

    viewNode.markLayoutDirty();
    if (viewNode.parentManagesChildFrames()) {
        viewNode.invalidateMeasuredSize();
    }

    return Void();
}

void YogaAttributeHandlerDelegate::onReset(ViewTransactionScope& /*viewTransactionScope*/,
                                           ViewNode& viewNode,
                                           const Ref<View>& /*view*/,
                                           const StringBox& /*name*/,
                                           const Ref<Animator>& /*animator*/) {
    auto* nodeRef = getNodeRef(viewNode);
    if (nodeRef == nullptr) {
        return;
    }

    onReset(nodeRef, _yogaAttributes->_defaultYogaNode);

    viewNode.markLayoutDirty();
    if (viewNode.parentManagesChildFrames()) {
        viewNode.invalidateMeasuredSize();
    }
}

float YogaAttributeHandlerDelegate::roundToPixelGrid(double value) const {
    return Valdi::roundToPixelGrid(static_cast<float>(value), _yogaAttributes->_pointScale);
}

std::optional<YGCompactValue> YogaAttributeHandlerDelegate::parseYGValue(AttributeParser& parser) {
    parser.tryParseWhitespaces();

    if (parser.tryParse("auto")) {
        return YGCompactValue::ofAuto();
    } else {
        auto d = parser.parseDimension();
        if (!d) {
            return std::nullopt;
        }

        if (d.value().unit == Dimension::Unit::Percent) {
            return YGCompactValue::of<YGUnitPercent>(static_cast<float>(d.value().value));
        } else {
            return YGCompactValue::of<YGUnitPoint>(roundToPixelGrid(d.value().value));
        }
    }
}

Result<YGCompactValue> YogaAttributeHandlerDelegate::valueToYGValue(const Value& value) {
    if (value.isNumber()) {
        return YGCompactValue::of<YGUnitPoint>(roundToPixelGrid(value.toDouble()));
    } else if (value.isString()) {
        auto strBox = value.toStringBox();
        AttributeParser parser(strBox.toStringView());

        auto ygValue = parseYGValue(parser);
        if (!ygValue || !parser.ensureIsAtEnd()) {
            return parser.getError();
        }

        return ygValue.value();
    } else {
        return ValueConverter::invalidTypeFailure(value, ValueType::Double);
    }
}

YGNodeRef YogaAttributeHandlerDelegate::getNodeRef(ViewNode& viewNode) const {
    return _isForChildrenNode ? viewNode.getYogaNodeForInsertingChildren() : viewNode.getYogaNode();
}

} // namespace Valdi
