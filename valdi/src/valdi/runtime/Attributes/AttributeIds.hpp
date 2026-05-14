//
//  AttributeIds.hpp
//  valdi
//
//  Created by Simon Corsin on 5/12/20.
//

#pragma once

#include "utils/base/NonCopyable.hpp"
#include "valdi/runtime/Attributes/AttributeId.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <mutex>
#include <string_view>
#include <vector>

namespace Valdi {

enum DefaultAttribute : AttributeId {
    DefaultAttributeId = 1,
    DefaultAttributeElementTag,
    DefaultAttributeCSSDocument,
    DefaultAttributeCSSClass,
    DefaultAttributeStyle,
    DefaultAttributeTranslationX,
    DefaultAttributeTranslationY,
    DefaultAttributeContentOffsetX,
    DefaultAttributeContentOffsetY,
    DefaultAttributeLazyLayout,
    DefaultAttributeValue,
    DefaultAttributePlaceholder,
    DefaultAttributeSrc,
    DefaultAttributeOpacity,
    DefaultAttributeEnabled,
    DefaultAttributeAccessibilityId,
    DefaultAttributeScaleX,
    DefaultAttributeScaleY,
};

class AttributeIds : public snap::NonCopyable {
public:
    AttributeIds();
    ~AttributeIds();

    AttributeId getIdForName(std::string_view name);
    AttributeId getIdForName(const StringBox& name);
    StringBox getNameForId(AttributeId id) const;

    std::vector<AttributeId> getIdsForNames(const std::vector<StringBox>& names);

private:
    mutable std::mutex _mutex;
    std::vector<StringBox> _attributes;
    FlatMap<StringBox, AttributeId> _idForName;

    void registerDefaultAttribute(DefaultAttribute defaultAttribute, std::string_view name);
};

} // namespace Valdi
