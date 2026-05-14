//
//  View.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 6/26/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Views/ViewTransactionScope.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/View.hpp"

namespace Valdi {

ViewTransactionScope::ViewTransactionScope(IViewManager* viewManager,
                                           const Ref<MainThreadManager>& mainThreadManager,
                                           bool useMainThreadTransaction)
    : _viewManager(viewManager),
      _mainThreadManager(mainThreadManager),
      _useMainThreadTransaction(useMainThreadTransaction) {}

ViewTransactionScope::~ViewTransactionScope() = default;

void ViewTransactionScope::setRootView(Ref<View> rootView) {
    if (_rootView == rootView) {
        return;
    }

    _rootView = std::move(rootView);

    if (_transaction != nullptr && _rootView != nullptr) {
        _transaction->willUpdateRootView(_rootView);
    }
}

void ViewTransactionScope::submit() {
    if (_next != nullptr) {
        _next->submit();
    }

    auto transaction = std::move(_transaction);
    if (transaction != nullptr) {
        if (_rootView != nullptr) {
            transaction->didUpdateRootView(_rootView, _layoutDidBecomeDirty);
        }

        transaction->flush(/* sync */ false);
    }

    _layoutDidBecomeDirty = false;
}

void ViewTransactionScope::flushNow(bool sync) {
    if (_next != nullptr) {
        _next->flushNow(sync);
    }

    if (_transaction != nullptr) {
        _transaction->flush(sync);
    }
}

void ViewTransactionScope::setLayoutDidBecomeDirty(bool layoutDidBecomeDirty) {
    transaction();
    _layoutDidBecomeDirty = true;
}

IViewTransaction& ViewTransactionScope::transaction() {
    if (_transaction == nullptr) {
        createTransaction();
    }
    return *_transaction;
}

void ViewTransactionScope::createTransaction() {
    SC_ASSERT(_viewManager != nullptr);
    _transaction = _viewManager->createViewTransaction(_mainThreadManager, _useMainThreadTransaction);
    if (_rootView != nullptr) {
        _transaction->willUpdateRootView(_rootView);
    }
}

ViewTransactionScope& ViewTransactionScope::withViewManager(IViewManager& viewManager) {
    if (_viewManager == &viewManager) {
        return *this;
    }
    if (_next != nullptr) {
        return _next->withViewManager(viewManager);
    }

    _next = makeShared<ViewTransactionScope>(&viewManager, _mainThreadManager, _useMainThreadTransaction);
    return *_next;
}

const Ref<MainThreadManager>& ViewTransactionScope::getMainThreadManager() const {
    return _mainThreadManager;
}

} // namespace Valdi
