//
//  Error.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/19.
//

#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "utils/debugging/Assert.hpp"

namespace Valdi {

ResolvablePromise::ResolvablePromise() = default;
ResolvablePromise::~ResolvablePromise() = default;

void ResolvablePromise::fulfillWithPromiseResult(const Ref<Promise>& promise) {
    promise->onComplete([self = strongSmallRef(this)](const auto& result) mutable {
        if (self != nullptr) {
            self->fulfill(result);
            self = {};
        }
    });

    if (promise->isCancelable()) {
        setCancelCallback([promise]() {
            // This captures a strong ref reference, which will form a cycle.
            // When this promise is fulfilled by upstream promise,
            // the callback is removed, and the cycle will break
            if (promise != nullptr) {
                promise->cancel();
            }
        });
    }
}

void ResolvablePromise::fulfill(Result<Value> result) {
    std::unique_lock<Mutex> lock(_mutex);
    SC_ASSERT(_result.empty(), _result.description());

    _result = result;
    // clear after move because moved-from function is in "valid but unspecified" state
    auto callbacks = std::move(_callbacks);
    _callbacks = {};
    auto cancelCallback = std::move(_cancelCallback);
    _cancelCallback = {};

    lock.unlock();

    for (const auto& callback : callbacks) {
        if (result) {
            callback->onSuccess(result.value());
        } else {
            callback->onFailure(result.error());
        }
    }
}

void ResolvablePromise::onComplete(const Ref<PromiseCallback>& callback) {
    std::unique_lock<Mutex> lock(_mutex);
    if (_canceled) {
        return;
    }

    if (!_result.empty()) {
        auto result = _result;
        lock.unlock();

        if (result) {
            callback->onSuccess(result.value());
        } else {
            callback->onFailure(result.error());
        }

        return;
    }

    _callbacks.emplace_back(callback);
}

bool ResolvablePromise::isCancelable() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _cancelCallback;
}

void ResolvablePromise::cancel() {
    std::unique_lock<Mutex> lock(_mutex);
    if (_canceled) {
        return;
    }
    _canceled = true;
    // Drop the callbacks without invoking them, matching onComplete, which never forwards
    // once canceled. Holding on to them would leak: a callback can retain this promise, and
    // after cancel nothing ever drains _callbacks. Released below with the mutex unlocked.
    auto callbacks = std::move(_callbacks);
    _callbacks = {}; // clear after move
    auto cancelCallback = std::move(_cancelCallback);
    _cancelCallback = {}; // clear after move
    lock.unlock();

    if (cancelCallback) {
        cancelCallback();
    }
}

void ResolvablePromise::setCancelCallback(DispatchFunction cancelCallback) {
    std::unique_lock<Mutex> lock(_mutex);
    if (!_result.empty()) {
        lock.unlock();
        return;
    }

    if (_canceled) {
        lock.unlock();

        if (cancelCallback) {
            cancelCallback();
        }
        return;
    }

    auto oldCancelCallback = std::move(_cancelCallback);
    _cancelCallback = std::move(cancelCallback);
    lock.unlock();
}

} // namespace Valdi
