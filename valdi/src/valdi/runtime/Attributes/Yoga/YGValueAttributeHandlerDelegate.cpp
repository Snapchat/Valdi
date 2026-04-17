//
//  YGValueAttributeHandlerDelegate.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#include "valdi/runtime/Attributes/Yoga/YGValueAttributeHandlerDelegate.hpp"

namespace Valdi {

YGStyleLengthAttributeHandlerDelegate::YGStyleLengthAttributeHandlerDelegate(
    YGNodeValueGetterSetter<facebook::yoga::StyleLength> getterSetter)
    : YogaGetterSetterAttributeHandlerDelegate<facebook::yoga::StyleLength>(getterSetter) {}

Result<Void> YGStyleLengthAttributeHandlerDelegate::onApply(YGNodeRef node, const Value& value) {
    auto styleLength = valueToYGStyleLength(value);
    if (!styleLength) {
        return styleLength.moveError();
    }

    return setValue(node, styleLength.value());
}

YGStyleSizeLengthAttributeHandlerDelegate::YGStyleSizeLengthAttributeHandlerDelegate(
    YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength> getterSetter)
    : YogaGetterSetterAttributeHandlerDelegate<facebook::yoga::StyleSizeLength>(getterSetter) {}

Result<Void> YGStyleSizeLengthAttributeHandlerDelegate::onApply(YGNodeRef node, const Value& value) {
    auto styleSizeLength = valueToYGStyleSizeLength(value);
    if (!styleSizeLength) {
        return styleSizeLength.moveError();
    }

    return setValue(node, styleSizeLength.value());
}

} // namespace Valdi
