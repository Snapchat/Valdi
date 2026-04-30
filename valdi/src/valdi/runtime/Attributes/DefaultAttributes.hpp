//
//  DefaultAttributes.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/10/18.
//

#pragma once

struct YGConfig;

#include "valdi/runtime/Attributes/AccessibilityAttributes.hpp"
#include "valdi/runtime/Attributes/AttributeHandler.hpp"
#include "valdi/runtime/Attributes/ScrollAnchorAttributes.hpp"

#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class ViewNode;
class YogaAttributes;
class AccessibilityAttributes;
class AttributeIds;

class DefaultAttributes {
public:
    DefaultAttributes(YGConfig* yogaConfig, AttributeIds& attributeIds, float pointScale);

    void bind(AttributeHandlerById& attributes);

private:
    AttributeIds& _attributeIds;
    Ref<YogaAttributes> _yogaAttributes;
    Ref<AccessibilityAttributes> _accessibilityAttributes;
    Ref<ScrollAnchorAttributes> _scrollAnchorAttributes;
};

} // namespace Valdi
