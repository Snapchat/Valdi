//
//  YGEdgesAttributeHandlerDelegate.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#include "valdi/runtime/Attributes/Yoga/YGEdgesAttributeHandlerDelegate.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"

#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"

#include <yoga/node/Node.h>

namespace Valdi {

Result<Void> YGEdgesBaseAttributeHandlerDelegate::onApply(YGNodeRef node, const Value& value) {
    if (value.getArray() == nullptr || value.getArray()->size() != 5) {
        return Error("Should have 5 components in composite attribute");
    }

    const auto& parts = *value.getArray();

    auto topEdge = facebook::yoga::StyleLength::undefined();
    auto endEdge = facebook::yoga::StyleLength::undefined();
    auto bottomEdge = facebook::yoga::StyleLength::undefined();
    auto startEdge = facebook::yoga::StyleLength::undefined();

    // Our behavior here is a bit different than how web CSS work.
    // We fill the values using the shorthand value if it is set,
    // then we override the edges if a more specific edge was set.
    // e.g.: margin and margin-top is set, margin will be set for all
    // attributes, and the top will be overriden.
    // Having the same behavior as CSS would require more bookkeeping
    // to handle reset.

    if (!parts[0].isNull()) {
        auto shorthandResult = parseFlexBoxShorthand(parts[0]);
        if (!shorthandResult) {
            return shorthandResult.moveError();
        }

        const auto& values = shorthandResult.value();

        if (values.size() > 4) {
            return Error("Can only have 4 components in flexbox shorthand");
        }

        if (!values.empty()) {
            topEdge = values[0];
        }
        if (values.size() >= 2) {
            endEdge = values[1];
        } else {
            endEdge = topEdge;
        }
        if (values.size() >= 3) {
            bottomEdge = values[2];
        } else {
            bottomEdge = topEdge;
        }
        if (values.size() == 4) {
            startEdge = values[3];
        } else {
            startEdge = endEdge;
        }
    }

    if (!parts[1].isNull()) {
        auto result = valueToYGStyleLength(parts[1]);
        if (!result) {
            return result.moveError();
        }
        topEdge = result.value();
    }
    if (!parts[2].isNull()) {
        auto result = valueToYGStyleLength(parts[2]);
        if (!result) {
            return result.moveError();
        }
        endEdge = result.value();
    }
    if (!parts[3].isNull()) {
        auto result = valueToYGStyleLength(parts[3]);
        if (!result) {
            return result.moveError();
        }
        bottomEdge = result.value();
    }
    if (!parts[4].isNull()) {
        auto result = valueToYGStyleLength(parts[4]);
        if (!result) {
            return result.moveError();
        }
        startEdge = result.value();
    }

    setEdges(facebook::yoga::resolveRef(node)->style(), topEdge, endEdge, bottomEdge, startEdge);

    return Void();
}

void YGEdgesBaseAttributeHandlerDelegate::onReset(YGNodeRef node, YGNodeRef /*defaultYogaNode*/) {
    setEdges(facebook::yoga::resolveRef(node)->style(),
             facebook::yoga::StyleLength::undefined(),
             facebook::yoga::StyleLength::undefined(),
             facebook::yoga::StyleLength::undefined(),
             facebook::yoga::StyleLength::undefined());
}

Result<std::vector<facebook::yoga::StyleLength>> YGEdgesBaseAttributeHandlerDelegate::parseFlexBoxShorthand(
    const Value& value) {
    std::vector<facebook::yoga::StyleLength> values;

    if (value.isNumber()) {
        // Case where we pass a number directly
        values.emplace_back(valueToYGStyleLength(value).value());

        return std::move(values);
    } else if (value.isString()) {
        // Case where pass a string, e.g. shorthand
        values.reserve(4);

        auto strBox = value.toStringBox();

        AttributeParser parser(strBox.toStringView());
        while (!parser.isAtEnd()) {
            auto value = parseYGStyleLength(parser);
            if (!value) {
                return parser.getError();
            }

            values.emplace_back(value.value());
        }
    } else {
        return ValueConverter::invalidTypeFailure(value, ValueType::StaticString);
    }

    return std::move(values);
}

} // namespace Valdi
