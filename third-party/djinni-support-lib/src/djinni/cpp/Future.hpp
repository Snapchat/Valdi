/**
 * Copyright 2021 Snap, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "expected.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <condition_variable>
#include <mutex>
#include <cassert>
#include <exception>

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

// C++20 standard coroutines
#if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)
    #include <coroutine>
    namespace djinni::detail {
        template <typename Promise = void> using CoroutineHandle = std::coroutine_handle<Promise>;
        using SuspendNever = std::suspend_never;
    }
    #define DJINNI_FUTURE_HAS_COROUTINE_SUPPORT 1
#endif

namespace djinni {

template <typename T>
class Future;

struct BrokenPromiseException final : public std::exception {
    inline const char* what() const noexcept final {
        return "djinni::Promise was destructed before setting a result";
    }
};

namespace detail {

// A wrapper object to support both void and non-void result types in
// Promise/Future
template <typename T>
struct ValueHolder {
    using type = T;
    std::optional<T> value;
    T getValueUnsafe() {return std::move(*value);}
};
template <>
struct ValueHolder<void> {
    using type = bool;
    std::optional<bool> value;
    void getValueUnsafe() const {}
};

template<typename T>
struct SharedState;

// A simple type erased function container. It would be nice if std::function<>
// supports move only lambdas.
template <typename T>
struct ValueHandlerBase {
    virtual ~ValueHandlerBase() = default;
    virtual void call(const std::shared_ptr<SharedState<T>>&) = 0;
};
template <typename T, typename F>
class ValueHandler : public ValueHandlerBase<T> {
public:
    ValueHandler(F&& f) : _f(std::forward<F>(f)) {}
    ValueHandler(ValueHandler&& other) : _f(std::move(other._f)) {}
    void call (const std::shared_ptr<SharedState<T>>& s) override {
        _f(s);
    }
private:
    std::decay_t<F> _f;
};
template<typename T, typename FUNC>
static auto createValueHandler(FUNC&& f) {
    return std::make_unique<ValueHandler<T, FUNC>>(std::forward<FUNC>(f));
}

// The shared state object that links the promise and future objects
template<typename T>
struct SharedState: ValueHolder<T> {
    std::condition_variable cv;
    std::mutex mutex;
    std::exception_ptr exception;
    std::unique_ptr<ValueHandlerBase<T>> handler;

    bool isReady() const {
        return this->value.has_value() || exception != nullptr;
    }
};

template<typename T>
using SharedStatePtr = std::shared_ptr<SharedState<T>>;

// Common promise base class, shared by both `void` and `T` results.
template <typename T>
class PromiseBase {
public:
    virtual ~PromiseBase() noexcept {
        if (_sharedState) {
            setException(BrokenPromiseException{});
        }
    }
    PromiseBase() = default;

    // moveable
    PromiseBase(PromiseBase&&) noexcept = default;
    PromiseBase& operator= (PromiseBase&& other) noexcept {
        std::swap(other._sharedState, _sharedState);
        std::swap(other._sharedStateReadOnly, _sharedStateReadOnly);
        return *this;
    }

    // not copyable
    PromiseBase(const PromiseBase&) = delete;
    PromiseBase& operator= (const PromiseBase&) = delete;

    Future<T> getFuture();

    // Use to immediately resolve a promise and return the resulting future.
    // Useful for returning from trivial cases in functions that return djinni::Future<>
    // Examples:
    //   return Promise<int>::resolve(4);
    //   return Promise<std::string>::resolve(std::move(someStr));
    [[nodiscard]] static Future<T> resolve(const typename ValueHolder<T>::type& val) {
        PromiseBase<T> promise;
        promise.setValue(val);
        return promise.getFuture();
    }
    [[nodiscard]] static Future<T> resolve(typename ValueHolder<T>::type&& val) {
        PromiseBase<T> promise;
        promise.setValue(std::move(val));
        return promise.getFuture();
    }

    // Use to immediately reject a promise and return the resulting future.
    // Useful for returning from synchronous error handling in functions that return djinni::Future<>
    // Examples:
    //   return Promise<int>::reject(std::runtime_error("something bad"));
    //
    //   try {
    //     throwingFunc();
    //   } catch (const std::exception& e) {
    //     return Promise<std::string>::reject(e);
    //   }
    //
    //   try {
    //     throwingFunc();
    //   } catch (...) {
    //     return Promise<std::string>::reject(std::current_exception());
    //   }
    [[nodiscard]] static Future<T> reject(const std::exception& e) {
        PromiseBase<T> promise;
        promise.setException(e);
        return promise.getFuture();
    }
    [[nodiscard]] static Future<T> reject(std::exception_ptr ex) {
        PromiseBase<T> promise;
        promise.setException(ex);
        return promise.getFuture();
    }

protected:
    // `setValue()` or `setException()` can only be called once. After which the
    // shared state is set to null and further calls to `setValue()` or
    // `setException()` will fail.  If at the moment of calling `setValue()` or
    // `setException()`, the `then()` method is already called on the future
    // object, then the handler routine specified by `then()` will immediately
    // be called in the current thread.
    void setValue(const typename ValueHolder<T>::type& val) {
        updateAndCallResultHandler([&] (const detail::SharedStatePtr<T>& sharedState) {
            sharedState->value = val;
        });
    }
    void setValue(typename ValueHolder<T>::type&& val) {
        updateAndCallResultHandler([&] (const detail::SharedStatePtr<T>& sharedState) {
            sharedState->value = std::move(val);
        });
    }
    template <typename E>
    void setException(const E& ex) {
        // Let's try to prevent throwing non-exception types.
        static_assert(std::is_convertible_v<E, std::exception>, "setException() called with type that is not convertible to std::exception");

        setException(std::make_exception_ptr(ex));
    }
    void setException(std::exception_ptr ex) {
        updateAndCallResultHandler([&] (const detail::SharedStatePtr<T>& sharedState) {
            sharedState->exception = ex;
        });
    }
private:
    detail::SharedStatePtr<T> _sharedState = std::make_shared<SharedState<T>>();
    detail::SharedStatePtr<T> _sharedStateReadOnly = _sharedState; // allow calling getFuture() after setValue()

    template <typename UpdateFunc>
    void updateAndCallResultHandler(UpdateFunc&& updater) {
        detail::SharedStatePtr<T> sharedState;
        sharedState = std::atomic_exchange(&_sharedState, sharedState);
        assert(sharedState);    // a second call will trigger assertion
        std::unique_ptr<ValueHandlerBase<T>> handler;
        {
            std::lock_guard lk(sharedState->mutex);
            updater(sharedState);
            handler = std::move(sharedState->handler);
        }
        if (handler) {
            // handler already assigned, call it inline
            handler->call(sharedState);
        } else {
            // otherwise unblock potential waiters
            sharedState->cv.notify_all();
        }
    }
};
} // namespace detail

// Promise with non-void result type `T`
template <typename T>
class Promise: public detail::PromiseBase<T> {
public:
    using detail::PromiseBase<T>::setValue;
    using detail::PromiseBase<T>::setException;
    using detail::PromiseBase<T>::PromiseBase;
};

// Promise with a void result
template <>
class Promise<void>: public detail::PromiseBase<void> {
public:
    void setValue() {setValue(true);}
    using detail::PromiseBase<void>::setException;
    using detail::PromiseBase<void>::PromiseBase;
    
private:
    // hide the bool version
    void setValue(const bool&) {detail::PromiseBase<void>::setValue(true);}
    void setValue(bool&&) {detail::PromiseBase<void>::setValue(true);}
};

template<typename T>
class Future {
    template<typename U>
    friend class detail::PromiseBase;
    // not user constructable
    Future(detail::SharedStatePtr<T> sharedState) : _sharedState(sharedState) {}
public:
    // moveable
    Future(Future&& other) noexcept = default;
    Future& operator= (Future&& other) noexcept = default;
    // not copyable
    Future(const Future& other) = delete;
    Future& operator= (const Future& other) = delete;
    // Future becomes invalid after `then()` is called on it
    bool isValid() const {
        return std::atomic_load(&_sharedState) != nullptr;
    }
    // returns true if the result can be `get()` without blocking
    bool isReady() const {
        auto sharedState = std::atomic_load(&_sharedState);
        assert(sharedState);    // call on invalid future will trigger assertion
        std::unique_lock lk(sharedState->mutex);
        return sharedState->isReady();
    }
    // wait until future becomes `isReady()`
    void wait() const {
        auto sharedState = std::atomic_load(&_sharedState);
        assert(sharedState);    // call on invalid future will trigger assertion
        std::unique_lock lk(sharedState->mutex);
#if defined(__EMSCRIPTEN__)
        assert(sharedState->isReady()); // in wasm we must not block and wait
#else
        sharedState->cv.wait(lk, [state = sharedState] {return state->isReady();});
#endif
    }
    // wait until future becomes `isReady()` and return the result. This can
    // only be called once.
    auto get() {
        detail::SharedStatePtr<T> sharedState;
        sharedState = std::atomic_exchange(&_sharedState, sharedState);
        assert(sharedState);    // call on invalid future will trigger assertion
        std::unique_lock lk(sharedState->mutex);
#if defined(__EMSCRIPTEN__)
        assert(sharedState->isReady()); // in wasm we must not block and wait
#else
        sharedState->cv.wait(lk, [state = sharedState] {return state->isReady();});
#endif
        if (!sharedState->exception) {
            return sharedState->getValueUnsafe();
        } else {
            std::rethrow_exception(sharedState->exception);
        }
    }
    // If at the moment of calling `then()`, the result (or exception) is
    // already available, then the handler routine will immediately be called in
    // the current thread. Returns a new future that wraps the return value of
    // the handler routine. The current future becomes invalid after this call.
    template<typename FUNC>
    auto then(FUNC&& handler) {
        using HandlerReturnType = std::invoke_result_t<FUNC, Future<T>>;
        detail::SharedStatePtr<T> sharedState;
        sharedState = std::atomic_exchange(&_sharedState, sharedState);
        assert(sharedState);    // a second call will trigger assertion
        auto nextPromise = std::make_unique<Promise<HandlerReturnType>>();
        auto nextFuture = nextPromise->getFuture();
        auto continuation = [handler = std::forward<FUNC>(handler), nextPromise = std::move(nextPromise)] (detail::SharedStatePtr<T> x) mutable {
            try {
                if constexpr(std::is_void_v<HandlerReturnType>) {
                    handler(Future<T>(x));
                    nextPromise->setValue();
                } else {
                    nextPromise->setValue(handler(Future<T>(x)));
                }
            } catch (const std::exception& e) {
                nextPromise->setException(std::current_exception());
            }
        };
        detail::SharedStatePtr<T> sharedStateForReadyFuture;
        {
            std::unique_lock lk(sharedState->mutex);
            if (sharedState->isReady()) {
                // result already available
                sharedStateForReadyFuture = std::move(sharedState);
            } else {
                // result not yet available
                sharedState->handler = detail::createValueHandler<T>(std::move(continuation));
            }
        }
        if (sharedStateForReadyFuture) {
            continuation(sharedStateForReadyFuture);
        }
        return nextFuture;
    }

private:
    detail::SharedStatePtr<T> _sharedState;

#if defined(DJINNI_FUTURE_HAS_COROUTINE_SUPPORT)
public:
    bool await_ready() {
        return isReady();
    }
    auto await_resume() {
        // after resuming from await, the future should be in an invalid state
        // (_sharedState is null)
        detail::SharedStatePtr<T> sharedState;
        sharedState = std::atomic_exchange(&_sharedState, sharedState);
        return Future<T>(sharedState).get();
    }
    bool await_suspend(detail::CoroutineHandle<> h) {
        this->then([h, this] (Future<T> x) mutable {
            std::atomic_store(&_sharedState, std::atomic_load(&x._sharedState));
            h();
        });
        return true;
    }

    struct PromiseTypeBase {
        Promise<T> _promise;
        std::optional<djinni::expected<T, std::exception_ptr>> _result{};

        struct FinalAwaiter {
            constexpr bool await_ready() const noexcept {
                return false;
            }
            template <typename P>
            bool await_suspend(detail::CoroutineHandle<P> finished) const noexcept {
                static_assert(std::is_convertible_v<P*, PromiseTypeBase*>);
                auto& promise_type = finished.promise();
                if (*promise_type._result) {
                    if constexpr (std::is_void_v<T>) {
                        promise_type._promise.setValue();
                    } else {
                        promise_type._promise.setValue(std::move(**promise_type._result));
                    }
                } else {
                    promise_type._promise.setException(std::move(promise_type._result->error()));
                }
                return false;
            }
            constexpr void await_resume() const noexcept {}
        };

        constexpr detail::SuspendNever initial_suspend() const noexcept { return {}; }
        FinalAwaiter final_suspend() const noexcept { return {}; }

        Future<T> get_return_object() noexcept {
            return _promise.getFuture();
        }
        void unhandled_exception() {
            _result.emplace(djinni::unexpect, std::current_exception());
        }
    };

    struct PromiseType: PromiseTypeBase {
        template <typename V, typename = std::enable_if_t<std::is_convertible_v<V, T>>>
        void return_value(V&& value) {
            this->_result.emplace(std::forward<V>(value));
        }
        void return_value(T&& value) {
            this->_result.emplace(std::move(value));
        }
        void return_value(const T& value) {
            this->_result.emplace(value);
        }
    };
    using promise_type = PromiseType;
#endif
};

#if defined(DJINNI_FUTURE_HAS_COROUTINE_SUPPORT)
template<>
struct Future<void>::PromiseType : PromiseTypeBase {
    void return_void() {
        _result.emplace();
    }
};
#endif

template <typename T>
Future<T> detail::PromiseBase<T>::getFuture() {
    return Future<T>(_sharedStateReadOnly);
}

template <typename U>
Future<void> combine(U&& futures, size_t c) {
    struct Context {
        std::atomic<size_t> counter;
        Promise<void> promise;
        Context(size_t c) : counter(c) {}
    };
    auto context = std::make_shared<Context>(c);
    auto future = context->promise.getFuture();
    if (futures.empty()) {
        context->promise.setValue();
        return future;
    }
    for (auto& f: futures) {
        f.then([context] (auto) {
            if (--(context->counter) == 0) {
                context->promise.setValue();
            }
        });
    }
    return future;
}

template <typename U>
Future<void> whenAll(U&& futures) {
    return combine(std::forward<U>(futures), futures.size());
}

template <typename U>
Future<void> whenAny(U&& futures) {
    return combine(std::forward<U>(futures), 1);
}

} // namespace djinni
