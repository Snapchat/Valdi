#pragma once

#include "djinni_c.h"
#include "djinni_c_ref.hpp"
#include "djinni_c_types.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace djinni::c_api {

struct VoidTranslator {
  using CppType = void;
  using CType = void;

  static CType fromCpp() {}
  static CppType toCpp() {}
};

struct StringTranslator {
  using CppType = std::string;
  using CType = djinni_string_ref;

  static CType fromCpp(CppType &&str);
  static CType fromCpp(const CppType &str);
  static CppType toCpp(CType str);
};

struct DateTranslator {
  using CppType = std::chrono::system_clock::time_point;
  using CType = djinni_date_ref;
  static CType fromCpp(const CppType &date);
  static CppType toCpp(CType date);
};

template <typename T> struct UIntTranslator {
  using CppType = T;
  using CType = djinni_number_ref;

  static CType fromCpp(CppType value) {
    return djinni_number_uint64_new(static_cast<uint64_t>(value));
  }
  static CppType toCpp(CType value) {
    return static_cast<T>(djinni_number_get_uint64(value));
  }
};

template <typename T> struct SignedIntTranslator {
  using CppType = T;
  using CType = djinni_number_ref;

  static CType fromCpp(CppType value) {
    return djinni_number_int64_new(static_cast<int64_t>(value));
  }

  static CppType toCpp(CType value) {
    return static_cast<T>(djinni_number_get_int64(value));
  }
};

template <typename T> struct FloatingPointTranslator {
  using CppType = T;
  using CType = djinni_number_ref;

  static CType fromCpp(CppType value) {
    return djinni_number_double_new(static_cast<double>(value));
  }

  static CppType toCpp(CType value) {
    return static_cast<T>(djinni_number_get_double(value));
  }
};

struct UInt8Translator : public UIntTranslator<uint8_t> {};
struct UInt16Translator : public UIntTranslator<uint16_t> {};
struct UInt32Translator : public UIntTranslator<uint32_t> {};
struct UInt64Translator : public UIntTranslator<uint64_t> {};

struct Int8Translator : public SignedIntTranslator<int8_t> {};
struct Int16Translator : public SignedIntTranslator<int16_t> {};
struct Int32Translator : public SignedIntTranslator<int32_t> {};
struct Int64Translator : public SignedIntTranslator<int64_t> {};

struct FloatTranslator : public FloatingPointTranslator<float> {};
struct DoubleTranslator : public FloatingPointTranslator<float> {};

template <typename CppOpt, typename COpt> struct PrimitiveOptionalTranslator {
  using CppType = CppOpt;
  using CType = COpt;

  static CType fromCpp(CppType value) {
    CType out;

    if (value) {
      out.has_value = true;
      out.value = value.value();
    } else {
      out.has_value = false;
      out.value = 0;
    }

    return out;
  }

  static CppType toCpp(CType value) {
    if (value.has_value) {
      return CppType(value.value);
    } else {
      return CppType();
    }
  }
};

template <typename CppOpt, typename Tr> struct OptionalTranslator {
  using CppType = CppOpt;
  using CType = djinni_ref;

  static CType fromCpp(const CppType &value) {
    if (!value) {
      return nullptr;
    } else {
      return Tr::fromCpp(value.value());
    }
  }

  static CType fromCpp(CppType &&value) {
    if (!value) {
      return nullptr;
    } else {
      return Tr::fromCpp(std::move(value.value()));
    }
  }

  static CppType toCpp(djinni_ref ptr) {
    if (ptr == nullptr) {
      return CppType();
    } else {
      return CppType(Tr::toCpp(ptr));
    }
  }
};

template <typename Tr> struct OptionalPtrTranslator {
  using CppType = typename Tr::CppType;
  using CType = djinni_ref;

  static CType fromCpp(const CppType &value) {
    if (value == nullptr) {
      return nullptr;
    } else {
      return Tr::fromCpp(value);
    }
  }

  static CType fromCpp(CppType &&value) {
    if (value == nullptr) {
      return nullptr;
    } else {
      return Tr::fromCpp(std::move(value));
    }
  }

  static CppType toCpp(CType value) {
    if (value == nullptr) {
      return nullptr;
    } else {
      return Tr::toCpp(value);
    }
  }
};

template <typename Tr> struct ListTranslator {
  using CppType = std::vector<typename Tr::CppType>;
  using CType = djinni_array_ref;

  static CppType toCpp(CType value) {
    CppType output;
    auto length = djinni_array_get_length(value);
    output.reserve(length);

    for (size_t i = 0; i < length; i++) {
      auto item = djinni_array_get_value(value, i);
      output.emplace_back(Tr::toCpp(item));
      djinni_ref_release(item);
    }

    return output;
  }

  static CType fromCpp(const CppType &values) {
    djinni_array_ref output = djinni_array_new(values.size());
    size_t index = 0;

    for (const auto &value : values) {
      auto converted = Tr::fromCpp(value);
      djinni_array_set_value(output, index++, converted);
      djinni_ref_release(converted);
    }

    return output;
  }
};

template <typename Tr> struct SetTranslator {
  using CppType = std::unordered_set<typename Tr::CppType>;
  using CType = djinni_array_ref;

  static CppType toCpp(CType value) {
    CppType output;
    auto length = djinni_array_get_length(value);
    output.reserve(length);

    for (size_t i = 0; i < length; i++) {
      auto item = djinni_array_get_value(value, i);
      output.emplace(Tr::toCpp(item));
      djinni_ref_release(item);
    }

    return output;
  }

  static CType fromCpp(const CppType &values) {
    djinni_array_ref output = djinni_array_new(values.size());
    size_t index = 0;

    for (const auto &value : values) {
      auto converted = Tr::fromCpp(value);
      djinni_array_set_value(output, index++, converted);
      djinni_ref_release(converted);
    }

    return output;
  }
};

template <typename TrK, typename TrV> struct MapTranslator {
  using CppType =
      std::unordered_map<typename TrK::CppType, typename TrV::CppType>;
  using CType = djinni_keyval_array_ref;

  static CppType toCpp(djinni_keyval_array_ref key_values) {
    CppType output;
    auto length = djinni_keyval_array_get_length(key_values);
    output.reserve(length);

    for (size_t i = 0; i < length; i++) {
      auto key = djinni_keyval_array_get_key(key_values, i);
      auto value = djinni_keyval_array_get_value(key_values, i);
      output.try_emplace(TrK::toCpp(key), TrV::toCpp(value));
      djinni_ref_release(key);
      djinni_ref_release(value);
    }

    return output;
  }

  static CType fromCpp(const CppType &map) {
    djinni_keyval_array_ref output = djinni_keyval_array_new(map.size());
    size_t index = 0;
    for (const auto &it : map) {
      auto key = TrK::fromCpp(it.first);
      auto value = TrV::fromCpp(it.second);

      djinni_keyval_array_set_entry(output, index++, key, value);

      djinni_ref_release(key);
      djinni_ref_release(value);
    }
    return output;
  }
};

template <typename T> struct RecordTranslator {
  using CppType = T;
  using CType = djinni_record_ref;

  template <typename... Args> static djinni_record_ref make(Args &&...args) {
    auto *obj = new RecordHolder<T>(T(std::forward<Args>(args)...));
    return toC(obj);
  }

  static CppType &toCpp(djinni_record_ref ref) {
    auto *record = fromC<RecordHolder<T>>(ref);
    if (record == nullptr) {
      std::abort();
    }

    return record->data();
  }

  static CType fromCpp(CppType &&value) { return make(std::move(value)); }

  static CType fromCpp(const CppType &value) { return make(value); }
};

template <typename T> struct InterfaceTranslator {
  using CppType = std::shared_ptr<T>;
  using CType = djinni_interface_ref;

  static const CppType &toCpp(CType ref) {
    auto *i = fromC<InterfaceHolder<T>>(ref);
    if (i == nullptr) {
      std::abort();
    }

    return i->data();
  }

  static CType fromCpp(CppType value) {
    Object *obj = new InterfaceHolder<T>(std::move(value));
    return toC(obj);
  }

  template <typename PT>
  static djinni_proxy_class_ref
  makeProxyClass(const PT *methodDefs,
                 djinni_opaque_deallocator opaqueDeallocator) {
    auto *proxyClass =
        new ::djinni::ProxyClass<PT>(*methodDefs, opaqueDeallocator);
    return toC(proxyClass);
  }

  template <typename PT, typename P>
  static djinni_interface_ref makeProxy(djinni_proxy_class_ref proxyClassRef,
                                        void *opaque) {
    auto *proxyClass = fromC<::djinni::ProxyClass<PT>>(proxyClassRef);

    return fromCpp(std::make_shared<P>(proxyClass, opaque));
  }
};

template <typename Cpp, typename C> struct EnumTranslator {
  using CppType = Cpp;
  using CType = C;

  static CppType toCpp(CType value) { return static_cast<CppType>(value); }
  static CType fromCpp(CppType value) { return static_cast<CType>(value); }

  static Cpp toCppBoxed(djinni_number_ref value) {
    return static_cast<Cpp>(djinni_number_get_int64(value));
  }

  static djinni_number_ref fromCppBoxed(Cpp value) {
    return djinni_number_int64_new(static_cast<int64_t>(value));
  }
};

template <typename Cpp> struct BoxedEnumTranslator {
  using CppType = Cpp;
  using CType = djinni_number_ref;

  static CppType toCpp(CType value) {
    return static_cast<CppType>(djinni_number_get_int64(value));
  }

  static CType fromCpp(CppType value) {
    return djinni_number_int64_new(static_cast<int64_t>(value));
  }
};

struct BinaryTranslator {
  using CppType = std::vector<uint8_t>;
  using CType = djinni_binary_ref;

  static CppType toCpp(CType binary);
  static CType fromCpp(CppType &&binary);
  static CType fromCpp(const CppType &binary);
};

template <typename T> struct ProtobufTranslator {
  using CppType = T;
  using CType = djinni_binary_ref;

  static CppType toCpp(CType binary) {
    CppType output;
    output.ParseFromArray(djinni_binary_get_data(binary),
                          djinni_binary_get_length(binary));
    return output;
  }

  static CType fromCpp(const CppType &proto) {
    auto length = proto.ByteSizeLong();
    auto output = djinni_binary_new(static_cast<size_t>(length));

    proto.SerializeToArray(djinni_binary_get_data(output), length);

    return output;
  }
};

template <typename T> T getNullValue() { return T{}; }

#define DJINNI_HANDLE_EXCEPTION_PROLOGUE try {
#define DJINNI_HANDLE_EXCEPTION_EPILOGUE(__ret_type__)                         \
  }                                                                            \
  catch (const std::exception &exc) {                                          \
    djinni_exception_notify(exc.what());                                       \
    if constexpr (!std::is_void_v<__ret_type__>) {                             \
      return ::djinni::c_api::getNullValue<__ret_type__>();                    \
    }                                                                          \
  }

} // namespace djinni::c_api