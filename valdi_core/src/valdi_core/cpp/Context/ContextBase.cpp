//
//  ContextBase.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 11/12/23.
//  Copyright © 2023 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Context/ContextBase.hpp"
#include <fmt/format.h>

namespace Valdi {

ContextBase::ContextBase(ContextId contextId, const Attribution& attribution, const ComponentPath& path,
                         const StringBox& scopeName)
    : _contextId(contextId), _attribution(attribution), _path(path), _scopeName(scopeName) {}

ContextBase::~ContextBase() = default;

ContextId ContextBase::getContextId() const {
    return _contextId;
}

const ComponentPath& ContextBase::getPath() const {
    return _path;
}

const StringBox& ContextBase::getScopeName() const {
    return _scopeName;
}

std::string ContextBase::getIdAndPathString() const {
    if (_path.isEmpty()) {
        if (!_scopeName.isEmpty()) {
            return fmt::format("id {} in scope '{}'", _contextId, _scopeName.slowToString());
        }
        return fmt::format("id {}", _contextId);
    } else {
        return fmt::format("id {} with component path {}", _contextId, _path.toString());
    }
}

void ContextBase::withAttribution(const AttributionFunctionCallback& handler) const {
    _attribution.fn(handler);
}

const Attribution& ContextBase::getAttribution() const {
    return _attribution;
}

RefCountable* ContextBase::getWeakReferenceTable() const {
    return _weakReferenceTable.get();
}

void ContextBase::setWeakReferenceTable(const Ref<RefCountable>& weakReferenceTable) {
    _weakReferenceTable = weakReferenceTable;
}

RefCountable* ContextBase::getStrongReferenceTable() const {
    return _strongReferenceTable.get();
}

void ContextBase::setStrongReferenceTable(const Ref<RefCountable>& strongReferenceTable) {
    _strongReferenceTable = strongReferenceTable;
}

thread_local ContextBase* tCurrentContext = nullptr;

void ContextBase::setCurrent(ContextBase* currentContext) {
    auto* oldContext = tCurrentContext;
    tCurrentContext = unsafeRetain(currentContext);
    unsafeRelease(oldContext);
}

ContextBase* ContextBase::current() {
    return tCurrentContext;
}

VALDI_CLASS_IMPL(ContextBase)

} // namespace Valdi
