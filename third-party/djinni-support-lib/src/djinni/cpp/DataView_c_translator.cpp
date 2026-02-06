#include "DataView_c_translator.hpp"

namespace djinni::c_api {

::djinni::DataView DataViewTranslator::toCpp(djinni_binary_ref binary) {
  return djinni::DataView(djinni_binary_get_data(binary),
                          djinni_binary_get_length(binary));
}

djinni_binary_ref
DataViewTranslator::fromCpp(const ::djinni::DataView &dataRef) {
  return djinni_binary_new_with_bytes(dataRef.buf(), dataRef.len(), nullptr,
                                      nullptr);
}

} // namespace djinni::c_api