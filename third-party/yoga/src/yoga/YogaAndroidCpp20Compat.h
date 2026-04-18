/*
 * Compatibility shims for Android NDK libc++ versions that can compile C++20
 * syntax but do not expose the C++20 library APIs used by upstream Yoga.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <type_traits>

#if defined(__ANDROID__)

namespace std {

#ifndef __cpp_lib_bitops
template <typename T>
constexpr int bit_width(T value) noexcept {
  using UnsignedT = make_unsigned_t<T>;
  return numeric_limits<UnsignedT>::digits -
      countl_zero(static_cast<UnsignedT>(value));
}
#endif

#ifndef __cpp_lib_bit_cast
template <typename To, typename From>
To bit_cast(const From& from) noexcept {
  static_assert(sizeof(To) == sizeof(From));
  static_assert(is_trivially_copyable_v<To>);
  static_assert(is_trivially_copyable_v<From>);

  To to;
  memcpy(&to, &from, sizeof(To));
  return to;
}
#endif

template <typename T>
concept floating_point = is_floating_point_v<T>;

template <typename I>
concept input_iterator = requires(I i) {
  typename I::iterator_category;
  { *i };
  ++i;
} && is_base_of_v<input_iterator_tag, typename I::iterator_category>;

} // namespace std

#endif // defined(__ANDROID__)
