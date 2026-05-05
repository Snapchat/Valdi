//
//  RenderRequestEntries.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 4/21/21.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi/runtime/Rendering/AnimationOptions.hpp"

#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

namespace Valdi {

enum RenderRequestEntryType : uint8_t {
    CreateElement,
    DestroyElement,
    MoveElementToParent,
    SetRootElement,
    SetElementAttribute,
    StartAnimations,
    EndAnimations,
    CancelAnimation,
    OnLayoutComplete,
    OnNextDraw,
};

namespace RenderRequestEntries {

class EntryBase {
public:
    explicit EntryBase(RenderRequestEntryType type);
    ~EntryBase() = default;

    constexpr RenderRequestEntryType getType() const {
        return _type;
    }

    void serialize(const StringBox& type, const AttributeIds& attributeIds, ValueMap& value);

private:
    RenderRequestEntryType _type : 8;
};

class ElementEntryBase : public EntryBase {
public:
    explicit ElementEntryBase(RenderRequestEntryType type);
    ~ElementEntryBase() = default;

    constexpr RawViewNodeId getElementId() const {
        return _elementId;
    }

    constexpr void setElementId(RawViewNodeId elementId) {
        _elementId = elementId;
    }

    void serialize(const StringBox& type, const AttributeIds& attributeIds, ValueMap& value);

private:
    RawViewNodeId _elementId : 24;
};

class CreateElement : public ElementEntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::CreateElement;

    CreateElement();
    ~CreateElement();

    constexpr const StringBox& getViewClassName() const {
        return _viewClassName;
    }
    constexpr void setViewClassName(StringBox&& viewClassName) {
        _viewClassName = std::move(viewClassName);
    }
    constexpr void setViewClassName(const StringBox& viewClassName) {
        _viewClassName = viewClassName;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    StringBox _viewClassName;
};

class DestroyElement : public ElementEntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::DestroyElement;

    DestroyElement();
    ~DestroyElement() = default;

    void serialize(const AttributeIds& attributeIds, ValueMap& value);
};

class MoveElementToParent : public ElementEntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::MoveElementToParent;

    MoveElementToParent();
    ~MoveElementToParent() = default;

    constexpr RawViewNodeId getParentElementId() const {
        return _parentElementId;
    }
    constexpr void setParentElementId(RawViewNodeId parentElementId) {
        _parentElementId = parentElementId;
    }

    constexpr int getParentIndex() const {
        return _parentIndex;
    }
    constexpr void setParentIndex(int parentIndex) {
        _parentIndex = parentIndex;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    RawViewNodeId _parentElementId = 0;
    int _parentIndex = 0;
};

class SetRootElement : public ElementEntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::SetRootElement;

    SetRootElement();
    ~SetRootElement() = default;

    void serialize(const AttributeIds& attributeIds, ValueMap& value);
};

class SetElementAttribute : public ElementEntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::SetElementAttribute;

    SetElementAttribute();
    ~SetElementAttribute();

    constexpr AttributeId getAttributeId() const {
        return _attributeId;
    }
    constexpr void setAttributeId(AttributeId attributeId) {
        _attributeId = attributeId;
    }

    constexpr const Value& getAttributeValue() const {
        return _attributeValue;
    }
    constexpr void setAttributeValue(Value&& attributeValue) {
        _attributeValue = std::move(attributeValue);
    }
    constexpr void setAttributeValue(const Value& attributeValue) {
        _attributeValue = attributeValue;
    }

    constexpr bool isInjectedFromParent() const {
        return _injectedFromParent;
    }

    constexpr void setInjectedFromParent(bool injectedFromParent) {
        _injectedFromParent = injectedFromParent;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    // clang-tidy seems to not understand bitfields
    // NOLINTNEXTLINE(modernize-use-default-member-init)
    AttributeId _attributeId : 24;
    bool _injectedFromParent = false;
    Value _attributeValue;
};

class StartAnimations : public EntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::StartAnimations;

    StartAnimations();
    ~StartAnimations();

    constexpr AnimationOptions& getAnimationOptions() {
        return _options;
    }
    constexpr const AnimationOptions& getAnimationOptions() const {
        return _options;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    AnimationOptions _options;
};

class EndAnimations : public EntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::EndAnimations;

    EndAnimations();
    ~EndAnimations() = default;

    void serialize(const AttributeIds& attributeIds, ValueMap& value);
};

class CancelAnimation : public EntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::CancelAnimation;

    CancelAnimation();
    ~CancelAnimation() = default;

    constexpr int getToken() const {
        return _token;
    }

    void setToken(int token) {
        _token = token;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    int _token;
};

class OnLayoutComplete : public EntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::OnLayoutComplete;

    OnLayoutComplete();
    ~OnLayoutComplete();

    constexpr const Ref<ValueFunction>& getCallback() const {
        return _callback;
    }

    constexpr void setCallback(Ref<ValueFunction>&& callback) {
        _callback = std::move(callback);
    }

    constexpr void setCallback(const Ref<ValueFunction>& callback) {
        _callback = callback;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    Ref<ValueFunction> _callback;
};

class OnNextDraw : public EntryBase {
public:
    constexpr static RenderRequestEntryType kType = RenderRequestEntryType::OnNextDraw;

    OnNextDraw();
    ~OnNextDraw();

    constexpr const Ref<ValueFunction>& getCallback() const {
        return _callback;
    }

    constexpr void setCallback(Ref<ValueFunction>&& callback) {
        _callback = std::move(callback);
    }

    constexpr void setCallback(const Ref<ValueFunction>& callback) {
        _callback = callback;
    }

    void serialize(const AttributeIds& attributeIds, ValueMap& value);

private:
    Ref<ValueFunction> _callback;
};

template<typename Visitor>
inline auto visitEntry(EntryBase& entry, Visitor&& visitor) {
    switch (entry.getType()) {
        case RenderRequestEntryType::CreateElement: {
            return visitor(reinterpret_cast<CreateElement&>(entry));
        } break;
        case RenderRequestEntryType::DestroyElement: {
            return visitor(reinterpret_cast<DestroyElement&>(entry));
        } break;
        case RenderRequestEntryType::MoveElementToParent: {
            return visitor(reinterpret_cast<MoveElementToParent&>(entry));
        } break;
        case RenderRequestEntryType::SetRootElement: {
            return visitor(reinterpret_cast<SetRootElement&>(entry));
        } break;
        case RenderRequestEntryType::SetElementAttribute: {
            return visitor(reinterpret_cast<SetElementAttribute&>(entry));
        } break;
        case RenderRequestEntryType::StartAnimations: {
            return visitor(reinterpret_cast<StartAnimations&>(entry));
        } break;
        case RenderRequestEntryType::EndAnimations: {
            return visitor(reinterpret_cast<EndAnimations&>(entry));
        } break;
        case RenderRequestEntryType::CancelAnimation: {
            return visitor(reinterpret_cast<CancelAnimation&>(entry));
        } break;
        case RenderRequestEntryType::OnLayoutComplete: {
            return visitor(reinterpret_cast<OnLayoutComplete&>(entry));
        } break;
        case RenderRequestEntryType::OnNextDraw: {
            return visitor(reinterpret_cast<OnNextDraw&>(entry));
        } break;
        default:
            std::abort();
            break;
    }
}

Value serializeEntry(EntryBase& entry, const AttributeIds& attributeIds);

} // namespace RenderRequestEntries

} // namespace Valdi
