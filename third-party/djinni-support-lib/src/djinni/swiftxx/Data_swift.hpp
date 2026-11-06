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

// A DataRef crosses between C++ and Swift as a heap-held DataRef (sharing its buffer), which Swift's
// DJDataRef owns. These let Swift make, share, read and release one.
const void* makeDataRef(const void* bytes, size_t size);
const void* retainDataRef(const void* holder);
const void* dataRefBytes(const void* holder);
size_t dataRefSize(const void* holder);
void releaseDataRef(const void* holder);

}
