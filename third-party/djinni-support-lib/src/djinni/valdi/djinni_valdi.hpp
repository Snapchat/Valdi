/**
 * Copyright 2024 Snap, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaTypeResolver.hpp"
#include "valdi_core/cpp/Utils/DjinniUtils.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"

#include <fmt/format.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace djinni::valdi {

// -------- Helper functions and classes
class JsException : public std::runtime_error {
public:
    // Input must be an instanceof an Error Type
    JsException(Valdi::Error e) : std::runtime_error(e.toString()), _jsEx(std::move(e)) {}
    const Valdi::Error& cause() const noexcept {
        return _jsEx;
    }

private:
    Valdi::Error _jsEx;
};

template<typename T>
struct ExceptionHandlingTraits {
    static Valdi::Value handleNativeException(const std::exception& e,
                                              const Valdi::ValueFunctionCallContext& callContext) noexcept {
        // store C++ exception in JS Error and raise in JS runtime
        auto msg = STRING_FORMAT("C++: {}", e.what());
        callContext.getExceptionTracker().onError(Valdi::Error(std::move(msg)));
        return Valdi::Value::undefined();
    }
    static Valdi::Value handleNativeException(const JsException& e,
                                              const Valdi::ValueFunctionCallContext& callContext) noexcept {
        // JS error passthrough
        callContext.getExceptionTracker().onError(e.cause());
        return Valdi::Value::undefined();
    }
};

template<typename T, typename F>
Valdi::Value tsFunc(F&& f) noexcept {
    return Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
        [f = std::forward<F>(f)](const Valdi::ValueFunctionCallContext& callContext) noexcept {
            try {
                return f(callContext);
            } catch (const JsException& e) {
                return ExceptionHandlingTraits<T>::handleNativeException(e, callContext);
            } catch (const std::exception& e) {
                return ExceptionHandlingTraits<T>::handleNativeException(e, callContext);
            }
            return Valdi::Value::undefined();
        }));
}

void checkForNull(void* ptr, const char* context);

Valdi::ValueSchema resolveSchema(const Valdi::ValueSchema& unresolved,
                                 std::function<void()> registerSchemaFunc) noexcept;

void registerSchemaImpl(const Valdi::ValueSchema& schema, bool resolve) noexcept;

template<typename T>
Valdi::ValueSchema getResolvedSchema(const Valdi::StringBox& typeName) noexcept {
    T::registerSchema(false);
    T::registerSchema(true);
    auto registry = Valdi::ValueSchemaRegistry::sharedInstance();
    auto result = registry->getSchemaForTypeName(typeName);
    return result.value();
}

template<class T, class = void>
struct hasSchemaRef : std::false_type {};
template<class T>
struct hasSchemaRef<T, std::void_t<decltype(T::schemaRef)>> : std::true_type {};

template<typename T>
const Valdi::ValueSchema& schemaOrRef() noexcept {
    if constexpr (hasSchemaRef<T>::value) {
        return T::schemaRef();
    } else {
        return T::schema();
    }
}

// -------- Builtin data type marshallers
template<typename T, typename U = T>
class Primitive {
public:
    using CppType = T;
    using ValdiType = Valdi::Value;

    using Boxed = Primitive;

    static CppType toCpp(const ValdiType& v) noexcept {
        return static_cast<T>(v.to<U>());
    }
    static ValdiType fromCpp(const CppType& c) noexcept {
        return ValdiType(static_cast<U>(c));
    }
    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::primitiveType<U>();
        return schema;
    }
};

using Bool = Primitive<bool>;
using I8 = Primitive<int8_t, int32_t>;
using I16 = Primitive<int16_t, int32_t>;
using I32 = Primitive<int32_t>;
using I64 = Primitive<int64_t>;
using F32 = Primitive<float, double>;
using F64 = Primitive<double>;
using String = Primitive<std::string>;
template<typename T>
using Enum = Primitive<T, int32_t>;

class WString {
public:
    using CppType = std::wstring;
    using ValdiType = Valdi::Value;

    using Boxed = WString;

    static CppType toCpp(const ValdiType& v) {
        if (v.isInternedString()) {
            return Valdi::StaticString::utf8ToWString(v.toStringBox().toStringView());
        } else {
            return v.getStaticString()->toStdWString();
        }
    }

    static ValdiType fromCpp(const CppType& c) {
        return ValdiType(Valdi::StaticString::makeWithWideChars(c.data(), c.size()));
    }

    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::string();
        return schema;
    }
};

class Binary {
public:
    using CppType = std::vector<uint8_t>;
    using ValdiType = Valdi::Value;
    using Boxed = Binary;

    static CppType toCpp(const ValdiType& j) noexcept;
    static ValdiType fromCpp(const CppType& c) noexcept;
    static const Valdi::ValueSchema& schema() noexcept;
};

class Date {
public:
    using CppType = std::chrono::system_clock::time_point;
    using ValdiType = Valdi::Value;
    using Boxed = Date;

    static CppType toCpp(const ValdiType& v) noexcept;
    static ValdiType fromCpp(const CppType& c) noexcept;
    static const Valdi::ValueSchema& schema() noexcept;
};

template<template<class> class OptionalType, class T>
struct Optional {
    template<typename C>
    static OptionalType<typename C::CppType> opt_type(...);
    template<typename C>
    static typename C::CppOptType opt_type(typename C::CppOptType*);
    using CppType = decltype(opt_type<T>(nullptr));
    using ValdiType = Valdi::Value;
    using Boxed = Optional;

    static CppType toCpp(const ValdiType& j) {
        if (j.isUndefined() || j.isNull()) {
            return CppType{};
        } else {
            return T::Boxed::toCpp(j);
        }
    }
    static ValdiType fromCpp(const OptionalType<typename T::CppType>& c) {
        return c ? T::Boxed::fromCpp(*c) : Valdi::Value::undefined();
    }
    static ValdiType fromCpp(OptionalType<typename T::CppType>&& c) {
        return c ? T::Boxed::fromCpp(std::move(*c)) : Valdi::Value::undefined();
    }
    template<typename C = T>
    static ValdiType fromCpp(const typename C::CppOptType& cppOpt) {
        return T::Boxed::fromCppOpt(cppOpt);
    }
    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = schemaOrRef<T>().asOptional();
        return schema;
    }
};

template<typename T>
class List {
    using ECppType = typename T::CppType;
    using EValdiType = typename T::Boxed::ValdiType;

public:
    using CppType = std::vector<ECppType>;
    using ValdiType = Valdi::Value;
    using Boxed = List;

    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::array(schemaOrRef<T>());
        return schema;
    }

    static CppType toCpp(const ValdiType& v) {
        CppType c;
        const auto* a = v.getArray();
        if (a != nullptr) {
            c.reserve(a->size());
            for (size_t i = 0; i < a->size(); ++i) {
                c.push_back(T::Boxed::toCpp((*a)[i]));
            }
        }
        return c;
    }
    static ValdiType fromCpp(const CppType& c) {
        auto newArray = Valdi::ValueArray::make(c.size());
        for (size_t i = 0; i < c.size(); ++i) {
            (*newArray)[i] = T::Boxed::fromCpp(c[i]);
        }
        return Valdi::Value(newArray);
    }
};

template<typename T>
class Set {
    using ECppType = typename T::CppType;
    using EValdiType = typename T::Boxed::ValdiType;

public:
    using CppType = std::unordered_set<ECppType>;
    using ValdiType = Valdi::Value;
    using Boxed = Set;

    static CppType toCpp(const ValdiType& v) {
        auto es6set = castOrNull<Valdi::ES6Set>(v.getValdiObject());
        CppType cppSet;
        for (auto i = es6set->entries.begin(); i != es6set->entries.end(); i++) {
            cppSet.insert(T::toCpp(*i));
        }
        return cppSet;
    }
    static ValdiType fromCpp(const CppType& c) {
        auto es6set = Valdi::makeShared<Valdi::ES6Set>();
        for (const auto& k : c) {
            es6set->entries.push_back(T::fromCpp(k));
        }
        return Valdi::Value(es6set);
    }
    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::es6set(schemaOrRef<T>());
        return schema;
    }
};

template<typename Key, typename Value>
class Map {
    using CppKeyType = typename Key::CppType;
    using CppValueType = typename Value::CppType;
    using ValdiKeyType = typename Key::Boxed::ValdiType;
    using ValdiValueType = typename Value::Boxed::ValdiType;

public:
    using CppType = std::unordered_map<CppKeyType, CppValueType>;
    using ValdiType = Valdi::Value;
    using Boxed = Map;
    static CppType toCpp(const ValdiType& v) {
        auto es6map = castOrNull<Valdi::ES6Map>(v.getValdiObject());
        CppType cppMap;
        for (auto i = es6map->entries.begin(); i != es6map->entries.end();) {
            auto k = Key::toCpp(*i++);
            auto v = Value::toCpp(*i++);
            cppMap.insert_or_assign(k, std::move(v));
        }
        return cppMap;
    }
    static ValdiType fromCpp(const CppType& c) {
        auto es6Map = Valdi::makeShared<Valdi::ES6Map>();
        for (const auto& [k, v] : c) {
            es6Map->entries.push_back(Key::fromCpp(k));
            es6Map->entries.push_back(Value::fromCpp(v));
        }
        return Valdi::Value(es6Map);
    }
    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::es6map(schemaOrRef<Key>(), schemaOrRef<Value>());
        return schema;
    }
};

class Void {
public:
    using CppType = void;
    using ValdiType = Valdi::Value;
    using Boxed = Void;
    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::untyped();
        return schema;
    }
};

template<size_t N>
struct CTS {
    char data[N];
};
template<size_t N>
CTS(const char (&)[N]) -> CTS<N>;

template<typename CppProto, CTS... JsClassName>
class Protobuf {
public:
    using CppType = CppProto;
    using ValdiType = Valdi::Value;
    using Boxed = Protobuf;

    static CppType toCpp(ValdiType v) {
        auto array = v.getTypedArrayRef();
        auto buffer = array->getBuffer();
        CppProto ret;
        ret.ParseFromArray(buffer.data(), static_cast<int>(buffer.size()));
        return ret;
    }

    static ValdiType fromCpp(const CppType& c) {
        std::vector<uint8_t> cbuf(c.ByteSizeLong());
        c.SerializeToArray(cbuf.data(), static_cast<int>(cbuf.size()));
        auto bytes = Valdi::makeShared<Valdi::Bytes>();
        bytes->assignVec(std::move(cbuf));
        auto array = Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType, bytes);
        return Valdi::Value(array);
    }

    static const Valdi::ValueSchema& schema() noexcept {
        constexpr std::array<const std::string_view, sizeof...(JsClassName)> jsClassName = {JsClassName.data...};
        static auto schema = Valdi::ValueSchema::proto(jsClassName.begin(), jsClassName.end());
        return schema;
    }
};

template<typename T>
struct Array {
    using CppType = std::vector<typename T::CppType>;
    using ValdiType = Valdi::Value;
    using Boxed = Array;

    static CppType toCpp(const ValdiType& v) {
        return List<T>::toCpp(v);
    }
    static ValdiType fromCpp(const CppType& c) {
        return List<T>::fromCpp(c);
    }

    static const Valdi::ValueSchema& schema() noexcept {
        return List<T>::schema();
    }
};

template<typename T, typename U = Array<T>>
struct PrimitiveArray {
    using CppType = std::vector<typename T::CppType>;
    using ValdiType = Valdi::Value;
    using Boxed = PrimitiveArray;
    using CppElemType = typename T::CppType;

    static CppType toCpp(const ValdiType& v) noexcept {
        auto arr = v.getTypedArrayRef();
        const auto* bytes = arr->getBuffer().data();
        const auto byteSize = arr->getBuffer().size();
        const CppElemType* typedData = reinterpret_cast<const CppElemType*>(bytes);
        const auto typedSize = byteSize / sizeof(CppElemType);
        return CppType(typedData, typedData + typedSize);
    }
    static ValdiType fromCpp(const CppType& c) noexcept {
        auto bytes = Valdi::makeShared<Valdi::Bytes>();
        bytes->assignData(reinterpret_cast<const Valdi::Byte*>(c.data()), c.size() * sizeof(CppElemType));
        auto arr = Valdi::makeShared<Valdi::ValueTypedArray>(U::getArrayType(), bytes);
        return Valdi::Value(arr);
    }
    static const Valdi::ValueSchema& schema() noexcept {
        static auto schema = Valdi::ValueSchema::valueTypedArray();
        return schema;
    }
};
template<>
struct Array<I8> : PrimitiveArray<I8> {
    static constexpr Valdi::TypedArrayType getArrayType() noexcept {
        return Valdi::TypedArrayType::Int8Array;
    }
};
template<>
struct Array<I16> : PrimitiveArray<I16> {
    static constexpr Valdi::TypedArrayType getArrayType() noexcept {
        return Valdi::TypedArrayType::Int16Array;
    }
};
template<>
struct Array<I32> : PrimitiveArray<I32> {
    static constexpr Valdi::TypedArrayType getArrayType() noexcept {
        return Valdi::TypedArrayType::Int32Array;
    }
};
template<>
struct Array<I64> : PrimitiveArray<I64> {
    // Valdi::TypedArrayType does not support BigInt64Array
};
template<>
struct Array<F32> : PrimitiveArray<F32> {
    static constexpr Valdi::TypedArrayType getArrayType() noexcept {
        return Valdi::TypedArrayType::Float32Array;
    }
};
template<>
struct Array<F64> : PrimitiveArray<F64> {
    static constexpr Valdi::TypedArrayType getArrayType() noexcept {
        return Valdi::TypedArrayType::Float64Array;
    }
};

// -------- Interface support
using ValdiProxyId = uint32_t;
struct CppProxyCacheEntry {
    Valdi::Weak<Valdi::ValueTypedProxyObject> ref;
    int count;
};
class ValdiProxyBase;
extern std::unordered_map<ValdiProxyId, std::weak_ptr<ValdiProxyBase>> jsProxyCache;
extern std::unordered_map<void*, CppProxyCacheEntry> cppProxyCache;
extern std::mutex jsProxyCacheMutex;
extern std::mutex cppProxyCacheMutex;

class ValdiProxyBase {
protected:
    Valdi::Ref<Valdi::ValueTypedProxyObject> _js;
    std::vector<Valdi::Ref<Valdi::ValueFunction>> _methods;

public:
    ValdiProxyBase(Valdi::Ref<Valdi::ValueTypedProxyObject> js)
        : _js(js), _methods(_js->getTypedObject()->getPropertiesSize()) {}
    virtual ~ValdiProxyBase() {
        std::lock_guard lk(jsProxyCacheMutex);
        jsProxyCache.erase(_js->getId());
    }
    Valdi::Ref<Valdi::ValueTypedProxyObject> getProxy() noexcept {
        return _js;
    }
    Valdi::Value callJsMethod(size_t i, std::initializer_list<Valdi::Value> parameters) {
        if (_js->expired()) {
            throw JsException(Valdi::Error("proxy expired"));
        }
        if (_methods[i] == nullptr) {
            _methods[i] = _js->getTypedObject()->getProperty(i).getFunctionRef();
        }
        constexpr auto flags = static_cast<Valdi::ValueFunctionFlags>(Valdi::ValueFunctionFlagsCallSync |
                                                                      Valdi::ValueFunctionFlagsPropagatesError);
        auto res = _methods[i]->call(flags, parameters);
        if (res.success()) {
            return res.moveValue();
        } else {
            // Throw JS Error as C++ exception
            throw JsException(res.moveError());
        }
    }
};

template<typename T>
class DjinniCppProxyObject : public Valdi::ValueTypedProxyObject {
public:
    DjinniCppProxyObject(const Valdi::Ref<Valdi::ValueTypedObject>& typedObject, const std::shared_ptr<T>& impl)
        : Valdi::ValueTypedProxyObject(typedObject), _impl(impl) {}
    ~DjinniCppProxyObject() override {
        std::lock_guard lk(cppProxyCacheMutex);
        auto i = cppProxyCache.find(_impl.get());
        assert(i != cppProxyCache.end());
        if (--(i->second.count) == 0) {
            cppProxyCache.erase(_impl.get());
        }
    }
    std::string_view getType() const noexcept final {
        return "Djinni C++ Proxy";
    }
    std::shared_ptr<T> getImpl() const noexcept {
        return _impl;
    }

private:
    std::shared_ptr<T> _impl = nullptr;
};

template<typename I, typename Self>
class JsInterface {
public:
    using CppType = std::shared_ptr<I>;
    using CppOptType = std::shared_ptr<I>;
    using ValdiType = Valdi::Value;
    using Boxed = Self;

    static CppType toCpp(const ValdiType& v) {
        return _fromJs(v);
    }
    static ValdiType fromCppOpt(const CppOptType& c) {
        return {_toJs(c)};
    }
    static ValdiType fromCpp(const CppType& c) {
        ::djinni::valdi::checkForNull(c.get(), typeid(Self).name());
        return fromCppOpt(c);
    }

private:
    template<typename, typename>
    struct GetOrCreateJsProxy {
        std::shared_ptr<I> operator()(const Valdi::Value& js) noexcept {
            assert(false && "Attempting to pass JS object but interface lacks +p");
            return {};
        }
    };
    template<typename T>
    struct GetOrCreateJsProxy<T, std::void_t<typename T::ValdiProxy>> {
        std::shared_ptr<I> operator()(const Valdi::Value& js) noexcept {
            auto proxy = js.getTypedProxyObjectRef();
            auto obj = proxy->getTypedObject();
            std::lock_guard lk(jsProxyCacheMutex);
            // check prsence of proxy id in js object
            ValdiProxyId id = proxy->getId();
            auto i = jsProxyCache.find(id);
            if (i != jsProxyCache.end()) {
                auto strongProxyRef = i->second.lock();
                if (strongProxyRef != nullptr) {
                    return std::dynamic_pointer_cast<typename Self::ValdiProxy>(strongProxyRef);
                }
            }
            // not found or cache entry expired
            // create new js proxy and store it in cache
            auto newproxy = std::make_shared<typename Self::ValdiProxy>(proxy);
            jsProxyCache.emplace(id, newproxy);
            return newproxy;
        }
    };

    static std::shared_ptr<I> _fromJs(const Valdi::Value& v) {
        // null object
        if (v.isUndefined() || v.isNull()) {
            return {};
        } else if (auto cppproxy = dynamic_cast<DjinniCppProxyObject<I>*>(v.getTypedProxyObjectRef().get());
                   cppproxy != nullptr) {
            // existing cpp proxy
            return cppproxy->getImpl();
        } else {
            return GetOrCreateJsProxy<Self, void>()(v);
        }
    }

    // enable this only when the derived class has `toValdi` defined
    // (interface +c)
    template<typename, typename>
    struct GetOrCreateCppProxy {
        Valdi::Value operator()(const std::shared_ptr<I>& c) noexcept {
            assert(false && "Attempting to pass C++ object but interface lacks +c");
            return Valdi::Value::undefined();
        }
    };
    template<typename T>
    struct GetOrCreateCppProxy<T, std::void_t<decltype(T::toValdi)>> {
        Valdi::Value operator()(const std::shared_ptr<I>& c) noexcept {
            // look up in cpp proxy cache
            std::lock_guard lk(cppProxyCacheMutex);
            auto i = cppProxyCache.find(c.get());
            int jsRefCount = 1;
            if (i != cppProxyCache.end()) {
                // found existing cpp proxy
                jsRefCount += i->second.count;
                if (auto strong = Valdi::strongRef(i->second.ref); strong != nullptr) {
                    // and it's not expired
                    return Valdi::Value(Valdi::Ref(strong));
                }
            }
            // not found or cache entry expired
            // create a new cpp proxy and store it in cache
            auto o = Self::toValdi(c);
            if (i == cppProxyCache.end()) {
                cppProxyCache.emplace(c.get(), CppProxyCacheEntry{o.toWeak(), jsRefCount});
            } else {
                i->second = CppProxyCacheEntry{o.toWeak(), jsRefCount};
            }
            return Valdi::Value(o);
        }
    };

    static Valdi::Value _toJs(const std::shared_ptr<I>& c) {
        if (c == nullptr) {
            // null object
            return Valdi::Value::undefined();
        } else if (auto* p = dynamic_cast<ValdiProxyBase*>(c.get())) {
            // unwrap existing js proxy
            return Valdi::Value(p->getProxy());
        } else {
            return GetOrCreateCppProxy<Self, void>()(c);
        }
    }
};

} // namespace djinni::valdi
