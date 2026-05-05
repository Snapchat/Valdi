#pragma once

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#include <utility>
#include <type_traits>

// C++23 introduced std::expected. When available, use the standard version.
// Otherwise, fall back to the third-party tl::expected implementation.
#ifdef __cpp_lib_expected

#include <expected>

namespace djinni {

using ::std::expected;
using ::std::unexpected;
inline constexpr ::std::unexpect_t unexpect{};

}

#else

#include "tl_expected.hpp"

namespace djinni {

using ::tl::unexpected;
using ::tl::expected;
inline constexpr ::tl::unexpect_t unexpect{};

}

#endif

namespace djinni {

template <class E>
djinni::unexpected<typename std::decay<E>::type> make_unexpected(E &&e) {
    return djinni::unexpected<typename std::decay<E>::type>(std::forward<E>(e));
}

}
