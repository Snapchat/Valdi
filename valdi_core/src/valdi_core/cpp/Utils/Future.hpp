#pragma once

#include "djinni/cpp/Future.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include <future>

namespace Valdi {

template<typename T>
class PromiseState : public SimpleRefCountable {
public:
    PromiseState() = default;
    ~PromiseState() override = default;

    void onResult(Function<void(Result<T>)> handler) {
        std::unique_lock<Mutex> lock(_mutex);
        if (!_result.empty()) {
            auto result = _result;
            lock.unlock();
            handler(std::move(result));
            return;
        }
        _handlers.emplace_back(std::move(handler));
    }

    void setResult(Result<T> result) {
        SmallVector<Function<void(Result<T>)>, 1> handlers;
        {
            std::lock_guard<Mutex> lock(_mutex);
            SC_ASSERT(_result.empty(), _result.description());
            _result = result;
            handlers = std::move(_handlers);
        }

        for (auto& handler : handlers) {
            handler(result);
        }
    }

private:
    Mutex _mutex;
    Result<T> _result;
    SmallVector<Function<void(Result<T>)>, 1> _handlers;
};

template<typename T>
class Future {
public:
    Future() = default;
    explicit Future(const Ref<PromiseState<T>>& state) : _state(state) {}

    ~Future() = default;

    bool empty() const {
        return _state == nullptr;
    }

    void then(Function<void(Result<T>)> handler) const {
        SC_ASSERT_NOTNULL(_state);
        _state->onResult(std::move(handler));
    }

    Result<T> get() const {
        std::promise<Result<T>> promise;
        auto future = promise.get_future();

        then([&](auto result) { promise.set_value(result); });

        return future.get();
    }

private:
    Ref<PromiseState<T>> _state;
};

template<>
class Future<void> {
public:
    Future() = default;
    explicit Future(const Ref<PromiseState<Void>>& state) : _state(state) {}

    ~Future() = default;

    bool empty() const {
        return _state == nullptr;
    }

    void then(Function<void(Result<Void>)> handler) const {
        SC_ASSERT_NOTNULL(_state);
        _state->onResult(std::move(handler));
    }

    Result<Void> get() const {
        std::promise<Result<Void>> promise;
        auto future = promise.get_future();

        then([&](auto result) { promise.set_value(result); });

        return future.get();
    }

private:
    Ref<PromiseState<Void>> _state;
};

template<typename T>
class TypedPromise {
public:
    TypedPromise() {
        _state = makeShared<PromiseState<T>>();
    }

    ~TypedPromise() = default;

    Future<T> getFuture() const {
        return Future<T>(_state);
    }

    void setValue(const T& value) const {
        _state->setResult(value);
    }

    void setError(const Error& error) const {
        _state->setResult(error);
    }

private:
    Ref<PromiseState<T>> _state;
};

template<>
class TypedPromise<void> {
public:
    TypedPromise() {
        _state = makeShared<PromiseState<Void>>();
    }

    ~TypedPromise() = default;

    Future<void> getFuture() const {
        return Future<void>(_state);
    }

    void setValue() const {
        _state->setResult(Void());
    }

    void setError(const Error& error) const {
        _state->setResult(error);
    }

private:
    Ref<PromiseState<Void>> _state;
};

} // namespace Valdi
