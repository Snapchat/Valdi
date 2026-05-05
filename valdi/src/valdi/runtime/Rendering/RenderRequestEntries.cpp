//
//  RenderRequestEntries.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 4/21/21.
//

#include "valdi/runtime/Rendering/RenderRequestEntries.hpp"

namespace Valdi::RenderRequestEntries {

EntryBase::EntryBase(RenderRequestEntryType type) : _type(type) {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void EntryBase::serialize(const StringBox& type, const AttributeIds& /*attributeIds*/, ValueMap& value) {
    static auto kType = STRING_LITERAL("type");
    value[kType] = Value(type);
}

ElementEntryBase::ElementEntryBase(RenderRequestEntryType type) : EntryBase(type), _elementId(0) {}

void ElementEntryBase::serialize(const StringBox& type, const AttributeIds& attributeIds, ValueMap& value) {
    static auto kElementId = STRING_LITERAL("elementId");
    value[kElementId] = Value(static_cast<int32_t>(_elementId));

    EntryBase::serialize(type, attributeIds, value);
}

CreateElement::CreateElement() : ElementEntryBase(CreateElement::kType) {}
CreateElement::~CreateElement() = default;

void CreateElement::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kCreateElement = STRING_LITERAL("CreateElement");
    static auto kViewClass = STRING_LITERAL("viewClass");

    value[kViewClass] = Value(_viewClassName);

    ElementEntryBase::serialize(kCreateElement, attributeIds, value);
}

DestroyElement::DestroyElement() : ElementEntryBase(DestroyElement::kType) {}

void DestroyElement::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kDestroyElement = STRING_LITERAL("DestroyElement");
    ElementEntryBase::serialize(kDestroyElement, attributeIds, value);
}

MoveElementToParent::MoveElementToParent() : ElementEntryBase(MoveElementToParent::kType) {}

void MoveElementToParent::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kMoveElementToParent = STRING_LITERAL("MoveElementToParent");
    static auto kParentElementId = STRING_LITERAL("parentElementId");
    static auto kParentIndex = STRING_LITERAL("parentIndex");

    value[kParentElementId] = Value(static_cast<int32_t>(_parentElementId));
    value[kParentIndex] = Value(static_cast<int32_t>(_parentIndex));

    ElementEntryBase::serialize(kMoveElementToParent, attributeIds, value);
}

SetRootElement::SetRootElement() : ElementEntryBase(SetRootElement::kType) {}

void SetRootElement::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kSetRootElement = STRING_LITERAL("SetRootElement");

    ElementEntryBase::serialize(kSetRootElement, attributeIds, value);
}

SetElementAttribute::SetElementAttribute() : ElementEntryBase(SetElementAttribute::kType), _attributeId(0) {}
SetElementAttribute::~SetElementAttribute() = default;

void SetElementAttribute::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kSetElementAttribute = STRING_LITERAL("SetElementAttribute");
    static auto kAttributeName = STRING_LITERAL("attributeName");
    static auto kAttributeValue = STRING_LITERAL("attributeValue");
    static auto kInjectedFromParent = STRING_LITERAL("injectedFromParent");

    value[kAttributeName] = Value(attributeIds.getNameForId(_attributeId));
    value[kAttributeValue] = _attributeValue;
    if (_injectedFromParent) {
        value[kInjectedFromParent] = Value(_injectedFromParent);
    }

    ElementEntryBase::serialize(kSetElementAttribute, attributeIds, value);
}

StartAnimations::StartAnimations() : EntryBase(StartAnimations::kType) {}
StartAnimations::~StartAnimations() = default;

void StartAnimations::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kStartAnimations = STRING_LITERAL("StartAnimations");
    static auto kOptions = STRING_LITERAL("options");

    value[kOptions] = Value(_options.toValueMap());

    EntryBase::serialize(kStartAnimations, attributeIds, value);
}

EndAnimations::EndAnimations() : EntryBase(EndAnimations::kType) {}

void EndAnimations::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kEndAnimations = STRING_LITERAL("EndAnimations");

    EntryBase::serialize(kEndAnimations, attributeIds, value);
}

CancelAnimation::CancelAnimation() : EntryBase(CancelAnimation::kType) {}

void CancelAnimation::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kCancelAnimation = STRING_LITERAL("CancelAnimation");
    static auto kToken = STRING_LITERAL("token");

    value[kToken] = Value(_token);

    EntryBase::serialize(kCancelAnimation, attributeIds, value);
}

OnLayoutComplete::OnLayoutComplete() : EntryBase(OnLayoutComplete::kType) {}
OnLayoutComplete::~OnLayoutComplete() = default;

void OnLayoutComplete::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kOnLayoutComplete = STRING_LITERAL("OnLayoutComplete");
    static auto kCallback = STRING_LITERAL("callback");

    value[kCallback] = Value(_callback);

    EntryBase::serialize(kOnLayoutComplete, attributeIds, value);
}

OnNextDraw::OnNextDraw() : EntryBase(OnNextDraw::kType) {}
OnNextDraw::~OnNextDraw() = default;

void OnNextDraw::serialize(const AttributeIds& attributeIds, ValueMap& value) {
    static auto kOnNextDraw = STRING_LITERAL("OnNextDraw");
    static auto kCallback = STRING_LITERAL("callback");

    value[kCallback] = Value(_callback);

    EntryBase::serialize(kOnNextDraw, attributeIds, value);
}

Value serializeEntry(EntryBase& entry, const AttributeIds& attributeIds) {
    auto map = makeShared<ValueMap>();
    visitEntry(entry, [&](auto& entry) { entry.serialize(attributeIds, *map); });
    return Value(map);
}

} // namespace Valdi::RenderRequestEntries
