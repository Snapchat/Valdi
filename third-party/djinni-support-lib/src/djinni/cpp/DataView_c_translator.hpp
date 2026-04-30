#pragma once

#include "DataView.hpp"
#include "djinni_c.h"

namespace djinni::c_api {

struct DataViewTranslator {
  using CppType = ::djinni::DataView;
  using CType = djinni_binary_ref;

  static CppType toCpp(CType binary);
  static CType fromCpp(const CppType &dataRef);
};

} // namespace djinni::c_api