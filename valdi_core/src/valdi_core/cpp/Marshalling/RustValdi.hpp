#pragma once

#include "valdi_core/cpp/Marshalling/RustValdi.h"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"

#include <atomic>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

struct ValdiRustHandle {
    std::atomic<size_t> refCount{1};
    void* value;
    void (*destroy)(ValdiRustHandle*) noexcept;
};

namespace Valdi {

class Value;

using RustModuleCreateFunction = ValdiRustModuleCreateFunction;
using RustModuleDropFunction = ValdiRustModuleDropFunction;
using RustModuleInvokeFunction = ValdiRustModuleInvokeFunction;

#if defined(VALDI_RUST_TRACK_HANDLES)
void rustHandleCreated() noexcept;
void rustHandleDestroyed() noexcept;
#endif

template<typename T>
struct RustHandleStorage final : ValdiRustHandle {
    template<typename U>
    explicit RustHandleStorage(U&& initialValue) : storedValue(std::forward<U>(initialValue)) {
#if defined(VALDI_RUST_TRACK_HANDLES)
        rustHandleCreated();
#endif
        value = &storedValue;
        destroy = [](ValdiRustHandle* handle) noexcept {
            delete static_cast<RustHandleStorage*>(handle);
#if defined(VALDI_RUST_TRACK_HANDLES)
            rustHandleDestroyed();
#endif
        };
    }

    T storedValue;
};

template<typename T>
ValdiRustHandle* makeRustHandle(T&& value) {
    using StoredType = std::decay_t<T>;
    return new RustHandleStorage<StoredType>(std::forward<T>(value));
}

template<typename T>
T& getRustHandleValue(ValdiRustHandle* handle) {
    return *static_cast<T*>(handle->value);
}

template<typename T>
const T& getRustHandleValue(const ValdiRustHandle* handle) {
    return *static_cast<const T*>(handle->value);
}

template<typename T>
T takeRustHandle(ValdiRustHandle* handle) {
    T value = std::move(getRustHandleValue<T>(handle));
    valdi_rust_handle_release(handle);
    return value;
}

inline ValdiRustSlice makeRustSlice(std::string_view value) noexcept {
    return {reinterpret_cast<const uint8_t*>(value.data()), value.size()};
}

BytesView makeRustBytesView(ValdiRustSlice value);
BytesView takeRustBytes(ValdiRustBuffer value);

ValdiRustStatus makeRustProxyValue(RegisteredCppGeneratedClass* schema,
                                   size_t methodCount,
                                   void* instance,
                                   ValdiRustProxyInvokeFunction invoke,
                                   ValdiRustValueDropFunction drop,
                                   ValdiRustHandle** out) noexcept;

ValdiRustStatus makeRustErrorStatus(std::string_view message) noexcept;

} // namespace Valdi
