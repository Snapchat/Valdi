//
//  YGValueAttributeHandlerDelegate.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#pragma once

#include "valdi/runtime/Attributes/Yoga/YogaGetterSetterAttributeHandlerDelegate.hpp"

namespace Valdi {

class YGStyleLengthAttributeHandlerDelegate
    : public YogaGetterSetterAttributeHandlerDelegate<facebook::yoga::StyleLength> {
public:
    explicit YGStyleLengthAttributeHandlerDelegate(YGNodeValueGetterSetter<facebook::yoga::StyleLength> getterSetter);

protected:
    Result<Void> onApply(YGNodeRef node, const Value& value) override;
};

class YGStyleSizeLengthAttributeHandlerDelegate
    : public YogaGetterSetterAttributeHandlerDelegate<facebook::yoga::StyleSizeLength> {
public:
    explicit YGStyleSizeLengthAttributeHandlerDelegate(
        YGNodeValueGetterSetter<facebook::yoga::StyleSizeLength> getterSetter);

protected:
    Result<Void> onApply(YGNodeRef node, const Value& value) override;
};

} // namespace Valdi
