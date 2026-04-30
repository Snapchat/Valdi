#pragma once

#include "djinni_c.h"

namespace djinni::c_api {

template <typename T> class Ref {
public:
  struct AdoptRef {};

  Ref(T ref, AdoptRef adoptRef) : _ref(ref) {}

  Ref(T &&ref) : _ref(ref) {}
  Ref(const T &ref) : _ref(ref) { djinni_ref_retain(_ref); }

  Ref(const Ref<T> &other) : _ref(other._ref) { djinni_ref_retain(other._ref); }

  Ref(Ref<T> &&other) : _ref(other._ref) { other._ref = nullptr; }

  ~Ref() { djinni_ref_release(_ref); }

  Ref &operator=(const Ref<T> &other) {
    if (&other != this) {
      auto old = _ref;
      _ref = other._ref;

      djinni_ref_retain(_ref);
      djinni_ref_release(old);
    }
    return *this;
  }

  Ref &operator=(Ref<T> &&other) {
    if (&other != this) {
      auto old = _ref;
      _ref = other._ref;
      other._ref = nullptr;

      djinni_ref_release(old);
    }
    return *this;
  }

  const T& get() const { return _ref; }

private:
  T _ref;
};

} // namespace djinni::c_api