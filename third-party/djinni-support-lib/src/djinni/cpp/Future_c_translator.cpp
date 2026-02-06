#include "Future_c_translator.hpp"

namespace djinni::c_api {

FutureException::FutureException(djinni_string_ref error) : _error(error) {}
FutureException::~FutureException() = default;

const char *FutureException::what() const noexcept {
  return djinni_string_get_data(_error.get());
}

} // namespace djinni::c_api