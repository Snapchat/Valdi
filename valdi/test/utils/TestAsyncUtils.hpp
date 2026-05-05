//
//  TestAsyncUtils.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 9/26/19.
//

#pragma once

#include "utils/base/Function.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <utility>

using namespace Valdi;

namespace Valdi {
class Promise;
}

namespace ValdiTest {

template<typename T>
class Atomic {
public:
    T load() {
        std::lock_guard<Mutex> guard(_mutex);
        return _value;
    }

    void set(T value) {
        std::lock_guard<Mutex> guard(_mutex);
        _value = std::move(value);
    }

    template<typename F>
    void mutate(F&& mutateFn) {
        std::lock_guard<Mutex> guard(_mutex);
        mutateFn(_value);
    }

private:
    T _value;
    Mutex _mutex;
};

template<typename T>
class ResultHolder : public Valdi::SharedPtrRefCountable {
public:
    static constexpr auto kWaitTimeout = 5;

    snap::CopyableFunction<void(Result<T>)> makeCompletion() {
        _group.enter();

        auto weakThis = Valdi::weakRef(this);

        return [=](Result<T> result) {
            auto strongThis = weakThis.lock();
            if (strongThis != nullptr) {
                strongThis->_result.set(std::move(result));
                strongThis->_group.leave();
            }
        };
    }

    Result<T> waitForResult() {
        auto success = _group.blockingWaitWithTimeout(std::chrono::seconds(kWaitTimeout));
        if (!success) {
            throw Exception(STRING_FORMAT("Failed to wait for result in {} seconds", kWaitTimeout));
        }

        auto result = _result.load();

        if (result.failure()) {
            VALDI_DEBUG(ConsoleLogger::getLogger(), "Request failed: {}", result.error().toString());
        }

        return result;
    }

    static Ref<ResultHolder<T>> make() {
        return Valdi::makeShared<ResultHolder<T>>();
    }

private:
    AsyncGroup _group;
    Atomic<Result<T>> _result;
};

Result<Value> waitForPromise(const Ref<Promise>& promise);

} // namespace ValdiTest
