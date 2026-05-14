//
//  RenderRequest.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 4/14/19.
//

#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

class SerializeEntryVisitor {
public:
    SerializeEntryVisitor(const AttributeIds& attributeIds, ValueArray& array)
        : _attributeIds(attributeIds), _array(array) {}

    template<typename T>
    void visit(T& entry) {
        auto& map = append();
        entry.serialize(_attributeIds, map);
    }

private:
    const AttributeIds& _attributeIds;
    ValueArray& _array;
    size_t _index = 0;

    ValueMap& append() {
        auto out = makeShared<ValueMap>();
        _array.emplace(_index++, Value(out));
        return *out;
    }
};

struct DestructEntryVisitor {
    template<typename T>
    void visit(T& entry) {
        entry.~T();
    }
};

RenderRequest::~RenderRequest() {
    DestructEntryVisitor visitor;
    visitEntries(visitor);
}

ContextId RenderRequest::getContextId() const {
    return _contextId;
}

void RenderRequest::setContextId(ContextId contextId) {
    _contextId = contextId;
}

void RenderRequest::setVisibilityObserverCallback(const Value& visibilityObserverCallback) {
    _visibilityObserverCallback = visibilityObserverCallback;
}

const Value& RenderRequest::getVisibilityObserverCallback() const {
    return _visibilityObserverCallback;
}

void RenderRequest::setFrameObserverCallback(const Value& frameObserverCallback) {
    _frameObserverCallback = frameObserverCallback;
}

const Value& RenderRequest::getFrameObserverCallback() const {
    return _frameObserverCallback;
}

size_t RenderRequest::getEntriesSize() const {
    return _entriesSize;
}

size_t RenderRequest::getEntriesBytesCount() const {
    return _entries.size();
}

RenderRequestEntries::CreateElement* RenderRequest::appendCreateElement() {
    return appendEntry<RenderRequestEntries::CreateElement>();
}

RenderRequestEntries::DestroyElement* RenderRequest::appendDestroyElement() {
    return appendEntry<RenderRequestEntries::DestroyElement>();
}

RenderRequestEntries::MoveElementToParent* RenderRequest::appendMoveElementToParent() {
    return appendEntry<RenderRequestEntries::MoveElementToParent>();
}

RenderRequestEntries::SetRootElement* RenderRequest::appendSetRootElement() {
    return appendEntry<RenderRequestEntries::SetRootElement>();
}

RenderRequestEntries::SetElementAttribute* RenderRequest::appendSetElementAttribute() {
    return appendEntry<RenderRequestEntries::SetElementAttribute>();
}

RenderRequestEntries::StartAnimations* RenderRequest::appendStartAnimations() {
    return appendEntry<RenderRequestEntries::StartAnimations>();
}

RenderRequestEntries::EndAnimations* RenderRequest::appendEndAnimations() {
    return appendEntry<RenderRequestEntries::EndAnimations>();
}

RenderRequestEntries::CancelAnimation* RenderRequest::appendCancelAnimation() {
    return appendEntry<RenderRequestEntries::CancelAnimation>();
}

RenderRequestEntries::OnLayoutComplete* RenderRequest::appendOnLayoutComplete() {
    return appendEntry<RenderRequestEntries::OnLayoutComplete>();
}

RenderRequestEntries::OnNextDraw* RenderRequest::appendOnNextDraw() {
    return appendEntry<RenderRequestEntries::OnNextDraw>();
}

RenderRequestEntries::EntryBase* RenderRequest::doAppendEntry(size_t size) {
    auto* buffer = _entries.appendWritable(size);
    _entriesSize++;
    return reinterpret_cast<RenderRequestEntries::EntryBase*>(buffer);
}

Value RenderRequest::serialize(const AttributeIds& attributeIds) const {
    static auto kContextId = STRING_LITERAL("contextID");
    static auto kEntries = STRING_LITERAL("entries");

    auto out = makeShared<ValueMap>();
    (*out)[kContextId] = Value(static_cast<int32_t>(_contextId));

    auto entries = ValueArray::make(getEntriesSize());

    SerializeEntryVisitor visitor(attributeIds, *entries);
    visitEntries(visitor);
    (*out)[kEntries] = Value(entries);

    return Value(out);
}

} // namespace Valdi
