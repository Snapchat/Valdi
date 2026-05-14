#pragma once

#include "djinni_support.hpp"
#include "../cpp/DataView.hpp"
#include "../cpp/DataRef.hpp"

namespace djinni::swift {

class DataViewAdaptor
{
public:
    using CppType = DataView;
    static CppType toCpp(const AnyValue& s);
    static AnyValue fromCpp(CppType c);
};

class DataRefAdaptor
{
public:
    using CppType = DataRef;
    static CppType toCpp(const AnyValue& s);
    static AnyValue fromCpp(const CppType& c);
};

}
