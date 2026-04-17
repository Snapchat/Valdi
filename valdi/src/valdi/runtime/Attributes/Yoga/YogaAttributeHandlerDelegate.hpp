//
//  YogaAttributeHandlerDelegate.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/3/21.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeHandlerDelegate.hpp"

#include <yoga/Yoga.h>
#include <yoga/numeric/FloatOptional.h>
#include <yoga/style/StyleLength.h>
#include <yoga/style/StyleSizeLength.h>

namespace Valdi {

class AttributeParser;
class YogaAttributes;
using YGFloatOptional = facebook::yoga::FloatOptional;

class YogaAttributeHandlerDelegate : public AttributeHandlerDelegate {
public:
    YogaAttributeHandlerDelegate();
    ~YogaAttributeHandlerDelegate() override;

    Result<Void> onApply(ViewTransactionScope& viewTransactionScope,
                         ViewNode& viewNode,
                         const Ref<View>& view,
                         const StringBox& name,
                         const Value& value,
                         const Ref<Animator>& animator) override;

    void onReset(ViewTransactionScope& viewTransactionScope,
                 ViewNode& viewNode,
                 const Ref<View>& view,
                 const StringBox& name,
                 const Ref<Animator>& /*animator*/) override;

protected:
    virtual Result<Void> onApply(YGNodeRef node, const Value& value) = 0;
    virtual void onReset(YGNodeRef node, YGNodeRef defaultYogaNode) = 0;

    std::optional<facebook::yoga::StyleLength> parseYGStyleLength(AttributeParser& parser);
    Result<facebook::yoga::StyleLength> valueToYGStyleLength(const Value& value);
    std::optional<facebook::yoga::StyleSizeLength> parseYGStyleSizeLength(AttributeParser& parser);
    Result<facebook::yoga::StyleSizeLength> valueToYGStyleSizeLength(const Value& value);

private:
    Ref<YogaAttributes> _yogaAttributes;
    bool _isForChildrenNode = false;

    friend YogaAttributes;

    YGNodeRef getNodeRef(ViewNode& viewNode) const;
    inline float roundToPixelGrid(double value) const;
};
} // namespace Valdi
