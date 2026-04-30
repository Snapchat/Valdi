//
//  RenderRequest.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 4/14/19.
//

#pragma once

#include "valdi/runtime/Rendering/RenderRequestEntries.hpp"

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi_core/cpp/Context/ContextId.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"

#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

namespace Valdi {

template<typename... Types>
constexpr size_t getAlignmentValue() {
    return std::max({alignof(Types)...});
}

constexpr size_t kEntryAlignment = getAlignmentValue<RenderRequestEntries::CreateElement,
                                                     RenderRequestEntries::DestroyElement,
                                                     RenderRequestEntries::MoveElementToParent,
                                                     RenderRequestEntries::SetRootElement,
                                                     RenderRequestEntries::SetElementAttribute,
                                                     RenderRequestEntries::StartAnimations,
                                                     RenderRequestEntries::EndAnimations,
                                                     RenderRequestEntries::CancelAnimation,
                                                     RenderRequestEntries::OnLayoutComplete,
                                                     RenderRequestEntries::OnNextDraw>();

template<typename T>
constexpr size_t getEntryAllocSize() {
    return alignUp(sizeof(T), kEntryAlignment);
}

template<typename T>
constexpr size_t getEntryAllocSize(const T& /*placeholder*/) {
    return getEntryAllocSize<T>();
}

/**
 A render request used to render components that are not backed by a Valdi document
 */
class RenderRequest : public SimpleRefCountable {
public:
    ~RenderRequest() override;

    ContextId getContextId() const;
    void setContextId(ContextId contextId);

    void setVisibilityObserverCallback(const Value& visibilityObserverCallback);
    const Value& getVisibilityObserverCallback() const;

    void setFrameObserverCallback(const Value& frameObserverCallback);
    const Value& getFrameObserverCallback() const;

    Value serialize(const AttributeIds& attributeIds) const;

    size_t getEntriesSize() const;
    size_t getEntriesBytesCount() const;

    RenderRequestEntries::CreateElement* appendCreateElement();
    RenderRequestEntries::DestroyElement* appendDestroyElement();
    RenderRequestEntries::MoveElementToParent* appendMoveElementToParent();
    RenderRequestEntries::SetRootElement* appendSetRootElement();
    RenderRequestEntries::SetElementAttribute* appendSetElementAttribute();
    RenderRequestEntries::StartAnimations* appendStartAnimations();
    RenderRequestEntries::EndAnimations* appendEndAnimations();
    RenderRequestEntries::CancelAnimation* appendCancelAnimation();
    RenderRequestEntries::OnLayoutComplete* appendOnLayoutComplete();
    RenderRequestEntries::OnNextDraw* appendOnNextDraw();

    template<typename Visitor>
    void visitEntries(Visitor& visitor) const {
        const_cast<RenderRequest*>(this)->visitEntries(visitor);
    }

    template<typename Visitor>
    void visitEntries(Visitor& visitor) {
        auto* current = _entries.data();
        auto* end = current + _entries.size();

        while (current != end) {
            current += RenderRequestEntries::visitEntry(*reinterpret_cast<RenderRequestEntries::EntryBase*>(current),
                                                        [&](auto& entry) {
                                                            visitor.visit(entry);
                                                            return getEntryAllocSize(entry);
                                                        });
        }
    }

private:
    ContextId _contextId = ContextIdNull;
    ByteBuffer _entries;
    Value _visibilityObserverCallback;
    Value _frameObserverCallback;
    size_t _entriesSize = 0;

    RenderRequestEntries::EntryBase* doAppendEntry(size_t size);

    template<typename T>
    T* appendEntry() {
        auto* entry = reinterpret_cast<T*>(doAppendEntry(getEntryAllocSize<T>()));
        new (entry) T();
        return entry;
    }
};

} // namespace Valdi
