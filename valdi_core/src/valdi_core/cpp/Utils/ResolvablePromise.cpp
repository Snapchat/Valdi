//
//  Error.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/19.
//

#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

namespace {

// Interned once: STRING_LITERAL hashes and takes the global StringCache mutex on every call, and
// cancel() runs on teardown paths.
STRING_CONST(promiseCanceledMessage, "Promise canceled")

} // namespace

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
    if (!_result.empty()) {
        // A producer may legitimately settle asynchronously after cancel() already recorded the
        // synthetic canceled error; drop its result. Settling twice without a cancel is still a bug.
        SC_ASSERT(_canceled, _result.description());
        return;
    }

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
    // A registrant that lands between cancel()'s two phases (canceled, result not recorded yet) is
    // appended and drained by cancel()'s second phase or by a synchronous producer settle.
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
    if (_canceled || !_result.empty()) {
        return;
    }
    _canceled = true;
    auto cancelCallback = std::move(_cancelCallback);
    _cancelCallback = {}; // clear after move

    // _callbacks intentionally stays populated across the cancel callback: the producer may settle
    // synchronously from it, and fulfill() then drains the callbacks with the real result.
    lock.unlock();

    if (cancelCallback) {
        cancelCallback();
    }

    lock.lock();
    if (!_result.empty()) {
        // The producer settled during cancellation; the consumers already got the real result.
        // Unlock before returning so cancelCallback — which can hold the last ref to the upstream
        // promise, and on iOS bottom out in an ObjC -dealloc — is destroyed with the mutex released.
        lock.unlock();
        return;
    }
    _result = Error(promiseCanceledMessage(), kPromiseCanceledErrorCode);
    auto canceledError = _result.error();
    auto callbacks = std::move(_callbacks);
    _callbacks = {}; // clear after move
    lock.unlock();

    for (const auto& callback : callbacks) {
        callback->onFailure(canceledError);
    }
}

void ResolvablePromise::setCancelCallback(DispatchFunction cancelCallback) {
    std::unique_lock<Mutex> lock(_mutex);
    // Check _canceled before _result: cancel() records a result, and a producer attaching to an
    // already-canceled promise must still be told to cancel rather than silently dropped.
    if (_canceled) {
        lock.unlock();

        if (cancelCallback) {
            cancelCallback();
        }
        return;
    }

    if (!_result.empty()) {
        lock.unlock();
        return;
    }

    auto oldCancelCallback = std::move(_cancelCallback);
    _cancelCallback = std::move(cancelCallback);
    lock.unlock();
}

} // namespace Valdi
