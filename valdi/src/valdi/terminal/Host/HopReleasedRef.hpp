#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"

#include <utility>

namespace Valdi::Terminal {

/**
 * A strong reference taken on a thread that must not run the pointee's destructor, such as the terminal
 * reader thread. When it goes away the reference is handed to `release`, which drops it on the right thread,
 * so the object is destroyed there if this was the last reference. `release` must take ownership of the
 * reference; it may drop it in place when there is nowhere to hop to.
 */
template<typename T>
class HopReleasedRef {
public:
    using Release = void (*)(Ref<T>);

    HopReleasedRef(Ref<T> ref, Release release) : _ref(std::move(ref)), _release(release) {}

    HopReleasedRef(HopReleasedRef&& other) noexcept : _ref(std::move(other._ref)), _release(other._release) {}

    HopReleasedRef& operator=(HopReleasedRef&& other) noexcept {
        if (this != &other) {
            releaseNow();
            _ref = std::move(other._ref);
            _release = other._release;
        }
        return *this;
    }

    HopReleasedRef(const HopReleasedRef&) = delete;
    HopReleasedRef& operator=(const HopReleasedRef&) = delete;

    ~HopReleasedRef() {
        releaseNow();
    }

    T* operator->() const {
        return _ref.get();
    }

    T& operator*() const {
        return *_ref;
    }

private:
    Ref<T> _ref;
    Release _release;

    void releaseNow() {
        if (_ref != nullptr) {
            _release(std::move(_ref));
        }
    }
};

} // namespace Valdi::Terminal
