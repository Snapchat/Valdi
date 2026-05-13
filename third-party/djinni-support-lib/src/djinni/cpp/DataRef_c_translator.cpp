#include "DataRef_c_translator.hpp"
#include "djinni_c_types.hpp"

namespace djinni::c_api {

class BinaryWithDataRef : public ::djinni::Binary {
public:
  BinaryWithDataRef(const ::djinni::DataRef &dataRef)
      : ::djinni::Binary(dataRef.mutableBuf(), dataRef.len()),
        _dataRef(dataRef) {}

  ~BinaryWithDataRef() override = default;

private:
  djinni::DataRef _dataRef;
};

class DataRefImpl : public ::djinni::DataRef::Impl {
public:
  DataRefImpl(djinni_binary_ref binary) : _binary(binary) {
    djinni_ref_retain(binary);
  }

  ~DataRefImpl() override { djinni_ref_release(_binary); }

  const uint8_t *buf() const override {
    return djinni_binary_get_data(_binary);
  }

  size_t len() const override { return djinni_binary_get_length(_binary); }

  uint8_t *mutableBuf() override { return djinni_binary_get_data(_binary); }

private:
  djinni_binary_ref _binary;
};

::djinni::DataRef DataRefTranslator::toCpp(djinni_binary_ref binary) {
  return ::djinni::DataRef(std::make_shared<DataRefImpl>(binary));
}

djinni_binary_ref DataRefTranslator::fromCpp(const ::djinni::DataRef &dataRef) {
  Object *obj = new BinaryWithDataRef(dataRef);
  return reinterpret_cast<djinni_binary_ref>(obj);
}

} // namespace djinni::c_api