#pragma once

#include "DataRef.hpp"
#include "djinni_c.h"

namespace djinni::c_api {

struct DataRefTranslator {
  using CppType = ::djinni::DataRef;
  using CType = djinni_binary_ref;

  static CppType toCpp(CType binary);
  static CType fromCpp(const CppType &dataRef);
};

} // namespace djinni::c_api