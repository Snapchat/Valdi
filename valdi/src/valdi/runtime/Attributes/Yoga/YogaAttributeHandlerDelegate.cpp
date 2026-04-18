//
//  YogaAttributeHandlerDelegate.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#include "valdi/runtime/Attributes/Yoga/YogaAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/Yoga/YogaAttributes.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Views/Measure.hpp"

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
}

float YogaAttributeHandlerDelegate::roundToPixelGrid(double value) const {
    return Valdi::roundToPixelGrid(static_cast<float>(value), _yogaAttributes->_pointScale);
}

std::optional<facebook::yoga::StyleLength> YogaAttributeHandlerDelegate::parseYGStyleLength(AttributeParser& parser) {
    parser.tryParseWhitespaces();

    if (parser.tryParse("auto")) {
        return facebook::yoga::StyleLength::ofAuto();
    } else {
        auto d = parser.parseDimension();
        if (!d) {
            return std::nullopt;
        }

        if (d.value().unit == Dimension::Unit::Percent) {
            return facebook::yoga::StyleLength::percent(static_cast<float>(d.value().value));
        } else {
            return facebook::yoga::StyleLength::points(roundToPixelGrid(d.value().value));
        }
    }
}

Result<facebook::yoga::StyleLength> YogaAttributeHandlerDelegate::valueToYGStyleLength(const Value& value) {
    if (value.isNumber()) {
        return facebook::yoga::StyleLength::points(roundToPixelGrid(value.toDouble()));
    } else if (value.isString()) {
        auto strBox = value.toStringBox();
        AttributeParser parser(strBox.toStringView());

        auto styleLength = parseYGStyleLength(parser);
        if (!styleLength || !parser.ensureIsAtEnd()) {
            return parser.getError();
        }

        return styleLength.value();
    } else {
        return ValueConverter::invalidTypeFailure(value, ValueType::Double);
    }
}

std::optional<facebook::yoga::StyleSizeLength> YogaAttributeHandlerDelegate::parseYGStyleSizeLength(
    AttributeParser& parser) {
    parser.tryParseWhitespaces();

    if (parser.tryParse("auto")) {
        return facebook::yoga::StyleSizeLength::ofAuto();
    } else {
        auto d = parser.parseDimension();
        if (!d) {
            return std::nullopt;
        }

        if (d.value().unit == Dimension::Unit::Percent) {
            return facebook::yoga::StyleSizeLength::percent(static_cast<float>(d.value().value));
        } else {
            return facebook::yoga::StyleSizeLength::points(roundToPixelGrid(d.value().value));
        }
    }
}

Result<facebook::yoga::StyleSizeLength> YogaAttributeHandlerDelegate::valueToYGStyleSizeLength(const Value& value) {
    if (value.isNumber()) {
        return facebook::yoga::StyleSizeLength::points(roundToPixelGrid(value.toDouble()));
    } else if (value.isString()) {
        auto strBox = value.toStringBox();
        AttributeParser parser(strBox.toStringView());

        auto styleSizeLength = parseYGStyleSizeLength(parser);
        if (!styleSizeLength || !parser.ensureIsAtEnd()) {
            return parser.getError();
        }

        return styleSizeLength.value();
    } else {
        return ValueConverter::invalidTypeFailure(value, ValueType::Double);
    }
}

YGNodeRef YogaAttributeHandlerDelegate::getNodeRef(ViewNode& viewNode) const {
    return _isForChildrenNode ? viewNode.getYogaNodeForInsertingChildren() : viewNode.getYogaNode();
}

} // namespace Valdi
