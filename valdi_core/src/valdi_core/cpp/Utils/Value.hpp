//
//  Value.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Utils/BigInt.hpp"
#include "valdi_core/cpp/Utils/Error.hpp"
#include "valdi_core/cpp/Utils/InternedStringImpl.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace Valdi {

class Value;
class ValueTypedArray;
class ValueFunction;
class ValueMap;
class ValdiObject;
class ValueArray;
class ValueTypedObject;
class ValueTypedProxyObject;
class StaticString;
class ExceptionTracker;
struct Void;

enum class ValueType : uint8_t {
    Null,
    Undefined,
    InternedString,
    StaticString,
    Int,
    Long,
    Double,
    Bool,
    Map,
    Array,
    TypedArray,
    Function,
    Error,
    /**
     A typed object that is associated with a ClassSchema
     */
    TypedObject,
    /**
     A proxy object that holds a typed object.
     */
    ProxyTypedObject,
    /**
     * A ValdiObject pointer which can be any class inheriting
     * ValdiObject. It could be a C++ object, platform specific
     * like Java or Objective-C, or a JS value.
     */
    ValdiObject,
    BigInt,
};

std::string valueTypeToString(ValueType weakValueType);

struct Undefined {};

/**
A weakly typed container for a 64bits value/pointer. A value can be backed by a string, map, function, number (bool,
double, int) or an arbitrary pointer. It also offers easy ways of converting to a native type. Because in Valdi, an
attribute value can be anything, this allows C++ to store those values into weakly typed maps. Value is memory safe when
constructed using a shared_ptr, otherwise it is the responsibility of the caller to ensure that the backed ptr will
outlive the Value instance.
 */
class Value {
public:
    Value() noexcept;
    Value(const Undefined& undefined) noexcept;
    Value(const Void& v) noexcept;
    ~Value() noexcept;

    explicit Value(int32_t anInt) noexcept;
    explicit Value(int64_t aLong) noexcept;
    explicit Value(const Ref<BigInt>& value) noexcept;
    explicit Value(double aDouble) noexcept;
    explicit Value(bool aBool) noexcept;

    explicit Value(std::string_view aString) noexcept;
    explicit Value(const StringBox& aString) noexcept;
    explicit Value(const Error& anError) noexcept;

    explicit Value(const char* aString) noexcept;

    explicit Value(const Ref<InternedStringImpl>& aString) noexcept;
    explicit Value(const Ref<ValueFunction>& aFunction) noexcept;
    explicit Value(const Ref<ValueMap>& aMap) noexcept;
    explicit Value(const Ref<ValueTypedArray>& aTypedArray) noexcept;
    explicit Value(const Ref<ValueArray>& anArray) noexcept;
    explicit Value(const Ref<ValueTypedObject>& aTypedObject) noexcept;
    explicit Value(const Ref<ValueTypedProxyObject>& aProxyObject) noexcept;
    explicit Value(const Ref<ValdiObject>& aValdiObject) noexcept;
    explicit Value(const Ref<StaticString>& aStaticString) noexcept;

    Value(const Value& other) noexcept;
    Value(Value&& other) noexcept;

    inline static Value undefined() noexcept {
        return Value(Undefined());
    }

    static const Value& undefinedRef() noexcept;

    ValueType getType() const noexcept;
    /**
     Whether the type is number representable, e.g. Int, Double or Bool
     */
    bool isNumber() const noexcept;

    bool isNull() const noexcept;
    bool isUndefined() const noexcept;
    bool isNullOrUndefined() const noexcept;
    bool isBool() const noexcept;
    bool isInt() const noexcept;
    bool isLong() const noexcept;
    bool isBigInt() const noexcept;
    const BigInt* getBigInt() const noexcept;
    Ref<BigInt> getBigIntRef() const noexcept;
    bool isDouble() const noexcept;
    bool isString() const noexcept;
    bool isInternedString() const noexcept;
    bool isStaticString() const noexcept;
    bool isArray() const noexcept;
    bool isMap() const noexcept;
    bool isError() const noexcept;
    bool isTypedArray() const noexcept;
    bool isFunction() const noexcept;
    bool isTypedObject() const noexcept;
    bool isProxyObject() const noexcept;
    bool isValdiObject() const noexcept;

    const ValueMap* getMap() const noexcept;
    const ValueArray* getArray() const noexcept;
    const ValueTypedArray* getTypedArray() const noexcept;
    const ValueTypedObject* getTypedObject() const noexcept;
    const ValueTypedProxyObject* getProxyObject() const noexcept;
    const StaticString* getStaticString() const noexcept;
    Ref<ValdiObject> getValdiObject() const noexcept;

    Error getError() const noexcept;

    ValueFunction* getFunction() const noexcept;
    Ref<ValueFunction> getFunctionRef() const noexcept;
    Ref<ValueArray> getArrayRef() const noexcept;
    Ref<ValueMap> getMapRef() const noexcept;
    Ref<ValueTypedArray> getTypedArrayRef() const noexcept;
    Ref<ValueTypedObject> getTypedObjectRef() const noexcept;
    Ref<ValueTypedProxyObject> getTypedProxyObjectRef() const noexcept;
    Ref<StaticString> getStaticStringRef() const noexcept;

    template<typename T>
    Ref<T> getTypedRef() const noexcept {
        if (!_isShared) {
            return nullptr;
        }

        return strongSmallRef(dynamic_cast<T*>(_data.p));
    }

    template<typename T>
    Ref<T> getUnsafeTypedRefPointer() const noexcept {
        if (!_isShared) {
            return nullptr;
        }

        return Ref<T>(static_cast<T*>(_data.p));
    }

    /**
     Returns the keys of the backing map, sorted by alphabetical order.
     If the value is not a map, it will return an empty array;
     */
    std::vector<StringBox> sortedMapKeys() const;

    // Convert value to bool
    bool toBool() const noexcept;

    // Convert value to string. Unlike GetString(), this method
    // always creates a new string and can convert from any ValueType.
    std::string toString() const;

    std::string toString(bool indent) const;

    // Convert value to string box. No allocation/conversion will be made if
    // the underlying type is String, otherwise conversion will occur.
    StringBox toStringBox() const;

    double toDouble() const noexcept;

    float toFloat() const noexcept;

    int32_t toInt() const noexcept;

    int64_t toLong() const noexcept;

    Value& operator=(Value&& other) noexcept;
    Value& operator=(const Value& other) noexcept;

    bool operator==(const Value& other) const noexcept;
    bool operator!=(const Value& other) const noexcept;

    bool operator<(const Value& other) const noexcept;

    size_t hash() const;

    // Convenience methods to retrieve a value from a map when the
    // Value is a Map object. Returns Undefined otherwise.
    Value getMapValue(std::string_view str) const;
    Value getMapValue(const char* str) const;
    Value getMapValue(const StringBox& str) const;

    // Convenience methods to set a value inside a Map.
    // If the receiver is not a map, it will be converted into one.
    Value& setMapValue(std::string_view str, const Value& value);
    Value& setMapValue(const StringBox& str, const Value& value);

    template<typename T>
    T to() const = delete;

    template<>
    int32_t to() const;

    template<>
    int64_t to() const;

    template<>
    bool to() const;

    template<>
    double to() const;

    template<>
    StringBox to() const;

    template<>
    std::string to() const;

    template<>
    Ref<ValueMap> to() const;

    template<>
    Ref<ValueFunction> to() const;

    template<>
    Value to() const;

    template<typename T>
    T checkedTo(ExceptionTracker& exceptionTracker) const = delete;

    template<>
    int32_t checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    int64_t checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<BigInt> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    bool checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    double checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    StringBox checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<StaticString> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValueArray> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValueFunction> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValueMap> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValueTypedArray> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Void checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValdiObject> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValueTypedObject> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<>
    Ref<ValueTypedProxyObject> checkedTo(ExceptionTracker& exceptionTracker) const;

    template<typename T>
    Ref<T> checkedToValdiObject(ExceptionTracker& exceptionTracker) const {
        auto valdiObject = checkedTo<Ref<ValdiObject>>(exceptionTracker);
        if (valdiObject == nullptr) {
            return nullptr;
        }
        auto object = castOrNull<T>(valdiObject);
        if (object == nullptr) {
            onTypedValdiObjectError(valdiObject, exceptionTracker);
        }

        return object;
    }

    static Error invalidTypeError(ValueType expectedType, ValueType actualType);

private:
    union {
        bool b;
        float f;
        double d;
        int32_t i;
        int64_t l;
        RefCountable* p;
    } _data;
    ValueType _type = ValueType::Null;
    bool _isShared = false;

    Value(RefCountable* refCountable, ValueType type) noexcept;

    template<class T>
    T* toPointer() const noexcept {
        return static_cast<T*>(_data.p);
    }

    static void onTypedValdiObjectError(const Ref<ValdiObject>& valdiObject, ExceptionTracker& exceptionTracker);
};

std::ostream& operator<<(std::ostream& os, const Value& value);
std::ostream& operator<<(std::ostream& os, const ValueType& valueType);

} // namespace Valdi

namespace std {

template<>
struct hash<Valdi::Value> {
    std::size_t operator()(const Valdi::Value& k) const noexcept;
};

} // namespace std

#include "valdi_core/cpp/Utils/ValueArray.hpp"
