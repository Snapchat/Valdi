//
//  AttributeIds.cpp
//  valdi
//
//  Created by Simon Corsin on 5/12/20.
//

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "utils/debugging/Assert.hpp"

namespace Valdi {

AttributeIds::AttributeIds() {
    // Start at 1
    getIdForName("");

    /**
     WARNING: Registration order is important, this allows to implement
     default attribute identifiers using the DefaultAttribute enum constants.
     */

    registerDefaultAttribute(DefaultAttributeId, "id");
    registerDefaultAttribute(DefaultAttributeElementTag, "tag");
    registerDefaultAttribute(DefaultAttributeCSSDocument, "cssDocument");
    registerDefaultAttribute(DefaultAttributeCSSClass, "class");
    registerDefaultAttribute(DefaultAttributeStyle, "style");
    registerDefaultAttribute(DefaultAttributeTranslationX, "translationX");
    registerDefaultAttribute(DefaultAttributeTranslationY, "translationY");
    registerDefaultAttribute(DefaultAttributeContentOffsetX, "contentOffsetX");
    registerDefaultAttribute(DefaultAttributeContentOffsetY, "contentOffsetY");
    registerDefaultAttribute(DefaultAttributeLazyLayout, "lazyLayout");
    registerDefaultAttribute(DefaultAttributeValue, "value");
    registerDefaultAttribute(DefaultAttributePlaceholder, "placeholder");
    registerDefaultAttribute(DefaultAttributeSrc, "src");
    registerDefaultAttribute(DefaultAttributeOpacity, "opacity");
    registerDefaultAttribute(DefaultAttributeEnabled, "enabled");
    registerDefaultAttribute(DefaultAttributeAccessibilityId, "accessibilityId");
    registerDefaultAttribute(DefaultAttributeScaleX, "scaleX");
    registerDefaultAttribute(DefaultAttributeScaleY, "scaleY");
}

AttributeIds::~AttributeIds() = default;

AttributeId AttributeIds::getIdForName(std::string_view name) {
    return getIdForName(StringCache::getGlobal().makeString(name));
}

AttributeId AttributeIds::getIdForName(const StringBox& name) {
    std::lock_guard<std::mutex> guard(_mutex);
    const auto& it = _idForName.find(name);
    if (it != _idForName.end()) {
        return it->second;
    }

    auto id = _attributes.size();
    _attributes.emplace_back(name);
    _idForName[name] = id;
    return id;
}

StringBox AttributeIds::getNameForId(AttributeId id) const {
    std::lock_guard<std::mutex> guard(_mutex);
    if (id >= _attributes.size()) {
        return StringBox();
    }
    return _attributes[id];
}

std::vector<AttributeId> AttributeIds::getIdsForNames(const std::vector<StringBox>& attributeNames) {
    std::vector<AttributeId> attributeIds(attributeNames.size());
    for (const auto& attributeName : attributeNames) {
        attributeIds.emplace_back(getIdForName(attributeName));
    }
    return attributeIds;
}

void AttributeIds::registerDefaultAttribute(DefaultAttribute defaultAttribute, std::string_view name) {
    auto id = getIdForName(name);
    SC_ASSERT(defaultAttribute == id);
}

} // namespace Valdi
