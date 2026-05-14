#pragma once

#include <variant>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <locale>
#include <codecvt>
#include <unordered_map>
#include <unordered_set>

namespace djinni::swift {
// -------- Intermediate data structure and functions to manipulate them from Swift
struct VoidValue{};
// I32 covers bool, i8, i16, i32, enum
using I32Value = int32_t;
using I64Value = int64_t;
// double covers float too
using DoubleValue = double;
using StringValue = std::string;
using BinaryValue = std::vector<uint8_t>;
struct RangeValue {
    const void* bytes;
    size_t size;
};
using DateValue = std::chrono::system_clock::time_point;
struct ErrorValue: public std::exception {
    std::string msg;
    std::exception_ptr cause;

    ErrorValue(const std::string& msg)
        :msg(msg), cause(nullptr) {}
    ErrorValue(const std::string& msg, std::exception_ptr cause)
        :msg(msg), cause(cause) {}
    ErrorValue(const ErrorValue& other) = default;
    ErrorValue(ErrorValue&& other) = default;

    ErrorValue& operator=(const ErrorValue& other) = default;
    ErrorValue& operator=(ErrorValue&& other) = default;
    
    const char* what() const noexcept override {
        return msg.c_str();
    }
};

class ProtocolWrapper;
struct InterfaceValue {
    std::shared_ptr<void> ptr;
    std::shared_ptr<ProtocolWrapper> sptr;
    template<typename T>
    InterfaceValue(const std::shared_ptr<T>& p) : ptr(p), sptr(std::dynamic_pointer_cast<ProtocolWrapper>(p)) {}
};

struct CompositeValue;
using CompositeValuePtr = std::shared_ptr<CompositeValue>;

struct OpaqueValue {
    virtual ~OpaqueValue() = default;
};
using OpaqueValuePtr = std::shared_ptr<OpaqueValue>;

using AnyValue = std::variant<VoidValue, I32Value, I64Value, DoubleValue,
    StringValue, BinaryValue, DateValue, ErrorValue, OpaqueValuePtr, RangeValue,
    InterfaceValue, CompositeValuePtr>;

// Base for opaque values that can be invoked as a provider (returns AnyValue).
// Used by callProviderFunction so Swift can call into C++/Swift provider holders.
struct CallableProvider : OpaqueValue {
    virtual AnyValue call() = 0;
};
using CallableProviderPtr = std::shared_ptr<CallableProvider>;

struct CompositeValue {
    virtual ~CompositeValue() = default;
    std::vector<AnyValue> _elems;

    void addValue(const AnyValue& v) {
        _elems.push_back(v);
    }
    void addValue(AnyValue&& v) {
        _elems.push_back(std::move(v));
    }
    AnyValue getValue(size_t i) const { return _elems[i]; }
    size_t getSize() const { return _elems.size(); }
};

struct ParameterList: CompositeValue {};

// Swift callable functions
AnyValue makeStringValue(const char* bytes, size_t size);
AnyValue makeBinaryValue(const void* bytes, size_t size);
RangeValue getBinaryRange(const AnyValue& v);
AnyValue makeRangeValue(const void* bytes, size_t size);
size_t getSize(const AnyValue* v);
AnyValue getMember(const AnyValue* v, size_t i);
void addMember(AnyValue* c, const AnyValue& v);
AnyValue getMember(const ParameterList* v, size_t i);
void setReturnValue(AnyValue* ret, const AnyValue& v);
void setErrorValue(AnyValue* ret, const ErrorValue& v);
void setErrorMessage(AnyValue* ret, const std::string& s);
AnyValue makeVoidValue();
bool isVoidValue(const AnyValue* c);
AnyValue makeCompositeValue();
bool isError(const AnyValue* ret);
ErrorValue getError(const AnyValue* ret);

// Provider bridge: call a provider held in AnyValue (C++ or Swift), or create one from a callback.
AnyValue callProviderFunction(const AnyValue& providerValue);
AnyValue makeProviderFunction(AnyValue (*callback)(void*), void* context, void (*releaseContext)(void*) = nullptr);

struct InterfaceInfo {
    void* cppPointer;
    void* ctxPointer;
};
InterfaceInfo getInterfaceInfo(const AnyValue* v);

using WeakSwiftProxy = std::weak_ptr<ProtocolWrapper>;
WeakSwiftProxy weakify(const AnyValue& v);
AnyValue strongify(const WeakSwiftProxy& v);

// -------- Swift protocol trampoline
typedef void (*DispatchFunc)(void* ctx, int idx /* -1 for cleanup*/, const ParameterList* params, AnyValue* ret);
class ProtocolWrapper {
protected:
    void* _ctx;
    DispatchFunc _dispatcher;
    ProtocolWrapper(void* ctx, DispatchFunc dispatcher);
    AnyValue callProtocol(int idx, const ParameterList* params);
public:
    virtual ~ProtocolWrapper();
    void* ctx() const { return _ctx; }
};

// -------- Built-in marshallers (conversion between C++ types and intermediate types)
template <typename T /*C++ type*/, typename U/*storage type*/>
struct Number {
    using CppType = T;
    static AnyValue fromCpp(T v) {
        return {static_cast<U>(v)};
    }
    static T toCpp(const AnyValue& v) {
        auto i = std::get<U>(v);
        return static_cast<T>(i);
    }
};

using Bool = Number<bool, I32Value>;
using I8 = Number<int8_t, I32Value>;
using I16 = Number<int16_t, I32Value>;
using I32 = Number<int32_t, I32Value>;
using I64 = Number<int64_t, I64Value>;
using F32 = Number<float, DoubleValue>;
using F64 = Number<double, DoubleValue>;

template <typename T>
struct Enum {
    using CppType = T;
    static AnyValue fromCpp(T v) {
        return {static_cast<int32_t>(v)};
    }
    static T toCpp(const AnyValue& v) {
        auto i = std::get<I32Value>(v);
        return static_cast<T>(i);
    }
};

struct String {
    using CppType = std::string;
    static AnyValue fromCpp(const CppType& c) {
        return c;
    }
    static CppType toCpp(const AnyValue& v) {
        return std::get<StringValue>(v);
    }
};

struct WString {
    using Utf8Converter = std::wstring_convert<std::codecvt_utf8<wchar_t>>;
    using CppType = std::wstring;
    static AnyValue fromCpp(const CppType& c) {
        return Utf8Converter().to_bytes(c);
    }
    static CppType toCpp(const AnyValue& v) {
        auto utf8str = std::get<StringValue>(v);
        return Utf8Converter{}.from_bytes(utf8str);
    }
};

template<typename T>
struct List {
    using CppType = std::vector<typename T::CppType>;
    static AnyValue fromCpp(const CppType& v) {
        auto ret = std::make_shared<CompositeValue>();
        for (const auto& e: v) {
            ret->addValue(T::fromCpp(e));
        }
        return {ret};
    }
    static CppType toCpp(const AnyValue& v) {
        CppType vec;
        auto c = std::get<CompositeValuePtr>(v);
        for (const auto& item: c->_elems) {
            vec.push_back(T::toCpp(item));
        }
        return vec;
    }
};

template<typename T> using Array = List<T>;

template<typename T>
struct Set {
    using CppType = std::unordered_set<typename T::CppType>;
    static AnyValue fromCpp(const CppType& v) {
        auto ret = std::make_shared<CompositeValue>();
        for (const auto& e: v) {
            ret->addValue(T::fromCpp(e));
        }
        return {ret};
    }
    static CppType toCpp(const AnyValue& v) {
        CppType vec;
        auto c = std::get<CompositeValuePtr>(v);
        for (const auto& item: c->_elems) {
            vec.insert(T::toCpp(item));
        }
        return vec;
    }
};

template<typename T, typename U>
struct Map {
    using CppType = std::unordered_map<typename T::CppType, typename U::CppType>;
    static AnyValue fromCpp(const CppType& c) {
        auto ret = std::make_shared<CompositeValue>();
        for (const auto& [k, v]: c) {
            ret->addValue(T::fromCpp(k));
            ret->addValue(U::fromCpp(v));
        }
        return {ret};
    }
    static CppType toCpp(const AnyValue& v) {
        CppType map;
        auto c = std::get<CompositeValuePtr>(v);
        for (auto i = c->_elems.begin(); i != c->_elems.end(); i += 2) {
            map.insert_or_assign(T::toCpp(*i), U::toCpp(*(i+1)));
        }
        return map;
    }
};

template<template<class> class OptionalType, class T>
struct Optional {
    template<typename C>
    static OptionalType<typename C::CppType> opt_type(...);
    template<typename C>
    static typename C::CppOptType opt_type(typename C::CppOptType*);
    using CppType = decltype(opt_type<T>(nullptr));

    static AnyValue fromCpp(const OptionalType<typename T::CppType>& c) {
        return c ? T::fromCpp(*c) : makeVoidValue();
    }
    static AnyValue fromCpp(OptionalType<typename T::CppType>&& c) {
        return c ? T::fromCpp(std::move(*c)) : makeVoidValue();
    }
    static CppType toCpp(const AnyValue& v) {
        if (std::holds_alternative<VoidValue>(v)) {
            return CppType{};
        } else {
            return T::toCpp(v);
        }
    }
    template<typename C = T>
    static AnyValue fromCpp(const typename C::CppOptType& cppOpt) {
        return cppOpt ? T::fromCppOpt(cppOpt) : makeVoidValue();
    }
};

class Date {
public:
    using CppType = std::chrono::system_clock::time_point;
    
    static CppType toCpp(const AnyValue& v) {
        auto millisecondsSinceEpoch = std::chrono::milliseconds(std::get<I64Value>(v));
        return CppType(std::chrono::duration_cast<std::chrono::system_clock::duration>(millisecondsSinceEpoch));
    }
    static AnyValue fromCpp(const CppType& c) {
        auto millisecondsSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(c.time_since_epoch());
        return {static_cast<I64Value>(millisecondsSinceEpoch.count())};
    }
};

class Binary {
public:
    using CppType = std::vector<uint8_t>;

    static AnyValue fromCpp(const CppType& c) {
        return makeBinaryValue(c.data(), c.size());
    }
    static CppType toCpp(const AnyValue& v) {
        return std::get<BinaryValue>(v);
    }
};

class Void {
public:
    using CppType = void;
};

template <typename T>
struct Interface {
    using CppType = std::shared_ptr<T>;
    using CppOptType = std::shared_ptr<T>;
    static AnyValue fromCpp(const CppType& c) {
        return {c};
    }
    static CppType toCpp(const AnyValue& v) {
        auto i = std::get<InterfaceValue>(v);
        return std::reinterpret_pointer_cast<T>(i.ptr);
    }
    static AnyValue fromCppOpt(const CppOptType& c) {
        return {c};
    }
};

template <typename CPP_PROTO>
class Protobuf {
public:
    using CppType = CPP_PROTO;
    static CppType toCpp(const AnyValue& s) {
        auto bin = std::get<BinaryValue>(s);
        CPP_PROTO cppProto;
        bool success = cppProto.ParseFromArray(bin.data(), bin.size());
        if (!success) {
            throw ErrorValue(std::string("failed to parse protobuf type ") + cppProto.GetTypeName());
        }
        return cppProto;
    }
    static AnyValue fromCpp(const CppType& c) {
        BinaryValue bin(c.ByteSizeLong());
        bool success = c.SerializeToArray(bin.data(), static_cast<int>(bin.size()));
        if (!success) {
            return ErrorValue("failed to serialize protobuf type " + c.GetTypeName());
        }
        return {bin};
    }
};

}
