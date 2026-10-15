//
//  Value.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Utils/Value.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include <cstdlib>
#include <fmt/format.h>
#include <iostream>
#include <sstream>

#include <boost/functional/hash.hpp>

namespace Valdi {

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

const std::string& trueString() {
    static std::string kValue = "true";
    return kValue;
}

const std::string& oneString() {
    static std::string kValue = "1";
    return kValue;
}

std::string valueTypeToString(ValueType weakValueType) {
    switch (weakValueType) {
        case ValueType::Null: {
            return "null";
        }
        case ValueType::Undefined: {
            return "undefined";
        }
        case ValueType::InternedString: {
            return "interned string";
        }
        case ValueType::StaticString: {
            return "string";
        }
        case ValueType::Int: {
            return "int";
        }
        case ValueType::Long: {
            return "long";
        }
        case ValueType::Double: {
            return "double";
        }
        case ValueType::Bool: {
            return "bool";
        }
        case ValueType::Map: {
            return "map";
        }
        case ValueType::Array: {
            return "array";
        }
        case ValueType::TypedArray: {
            return "typed array";
        }
        case ValueType::Function: {
            return "function";
        }
        case ValueType::Error: {
            return "error";
        }
        case ValueType::TypedObject: {
            return "typed object";
        }
        case ValueType::ProxyTypedObject: {
            return "proxy object";
        }
        case ValueType::ValdiObject: {
            return "valdi object";
        }
    }
}

/**
 Constructors
 */

Value::Value() noexcept {
    _data.l = 0;
}

Value::Value(const Undefined& /*undefined*/) noexcept : _type(ValueType::Undefined) {
    _data.l = 0;
}

Value::Value(const Void& /*v*/) noexcept : _type(ValueType::Undefined) {
    _data.l = 0;
}

Value::~Value() noexcept {
    if (_isShared) {
        unsafeRelease(_data.p);
    }
}

Value::Value(int32_t anInt) noexcept : _type(ValueType::Int) {
    _data.i = anInt;
}

Value::Value(int64_t aLong) noexcept : _type(ValueType::Long) {
    _data.l = aLong;
}

Value::Value(double aDouble) noexcept : _type(ValueType::Double) {
    _data.d = aDouble;
}

Value::Value(bool aBool) noexcept : _type(ValueType::Bool) {
    // Use a ternary to make sure we have either true or false, not some other integer
    // NOLINTNEXTLINE(readability-simplify-boolean-expr)
    _data.b = aBool ? true : false;
}

Value::Value(std::string_view aString) noexcept
    : Value(StringCache::getGlobal().makeString(aString).getInternedString()) {}

Value::Value(const StringBox& aString) noexcept : Value(aString.getInternedString()) {}

Value::Value(const Error& anError) noexcept : Value(anError.getStorage().get(), ValueType::Error) {}

Value::Value(const char* aString) noexcept
    : Value(StringCache::getGlobal().makeStringFromLiteral(aString).getInternedString()) {}

Value::Value(const Ref<ValueFunction>& aFunction) noexcept : Value(aFunction.get(), ValueType::Function) {}

Value::Value(const Value& other) noexcept : _data(other._data), _type(other._type), _isShared(other._isShared) {
    if (_isShared) {
        unsafeRetain(_data.p);
    }
}

Value::Value(const Ref<InternedStringImpl>& aString) noexcept : Value(aString.get(), ValueType::InternedString) {
    _type = ValueType::InternedString;
}

Value::Value(const Ref<ValueMap>& aMap) noexcept : Value(aMap.get(), ValueType::Map) {}

Value::Value(const Ref<ValueTypedObject>& aTypedObject) noexcept : Value(aTypedObject.get(), ValueType::TypedObject) {}

Value::Value(const Ref<ValueTypedProxyObject>& aProxyObject) noexcept
    : Value(aProxyObject.get(), ValueType::ProxyTypedObject) {}

Value::Value(const Ref<ValdiObject>& aValdiObject) noexcept : Value(aValdiObject.get(), ValueType::ValdiObject) {}

Value::Value(const Ref<ValueArray>& anArray) noexcept : Value(anArray.get(), ValueType::Array) {}

Value::Value(const Ref<ValueTypedArray>& aTypedArray) noexcept : Value(aTypedArray.get(), ValueType::TypedArray) {}

Value::Value(const Ref<StaticString>& aStaticString) noexcept : Value(aStaticString.get(), ValueType::StaticString) {}

Value::Value(RefCountable* refCountable, ValueType valueType) noexcept {
    if (refCountable != nullptr) {
        _data.p = refCountable;
        _type = valueType;
        _isShared = true;

        unsafeRetain(refCountable);
    } else {
        _data.l = 0;
    }
}

Value::Value(Value&& other) noexcept : _data(other._data), _type(other._type), _isShared(other._isShared) {
    other._data.l = 0;
    other._type = ValueType::Null;
    other._isShared = false;
}

const Value& Value::undefinedRef() noexcept {
    static auto undefined = Value::undefined();
    return undefined;
}

/**
 Operator overloads
 */

Value& Value::operator=(Value&& other) noexcept {
    if (this != &other) {
        if (_isShared) {
            unsafeRelease(_data.p);
        }

        _data = other._data;
        _type = other._type;
        _isShared = other._isShared;

        other._data.l = 0;
        other._type = ValueType::Null;
        other._isShared = false;
    }

    return *this;
}

Value& Value::operator=(const Value& other) noexcept {
    if (this != &other) {
        if (_isShared) {
            unsafeRelease(_data.p);
        }

        _data = other._data;
        _type = other._type;
        _isShared = other._isShared;

        if (_isShared) {
            unsafeRetain(_data.p);
        }
    }

    return *this;
}

namespace {

bool staticStringEqualsUTF8(const StaticString& staticString, std::string_view utf8) {
    switch (staticString.encoding()) {
        case StaticString::Encoding::UTF8:
            return staticString.utf8StringView() == utf8;
        case StaticString::Encoding::UTF16:
            // n UTF-16 code units encode to n..3n UTF-8 bytes; reject on length before transcoding.
            if (utf8.size() < staticString.size() || utf8.size() > staticString.size() * 3) {
                return false;
            }
            // utf8Storage() transcodes into a thread-local buffer: no lock.
            return staticString.utf8Storage().toStringView() == utf8;
        case StaticString::Encoding::UTF32:
            if (utf8.size() < staticString.size() || utf8.size() > staticString.size() * 4) {
                return false;
            }
            return staticString.utf8Storage().toStringView() == utf8;
    }
    return false;
}

} // namespace

bool Value::operator==(const Value& other) const noexcept {
    if (this == &other) {
        return true;
    }

    if (_type != other._type) {
        if (isNumber() && other.isNumber()) {
            // Automatically converts for number representable objects
            return toDouble() == other.toDouble();
        }
        if (isString() && other.isString()) {
            // Strings can be stored as InternedString or StaticString (e.g. comparing a
            // JS-sourced value to a schema's compile-time string literal). Compare bytes
            // directly rather than interning the StaticString side: interning hashes the
            // whole string and takes the global StringCache lock on every compare.
            const auto* interned =
                isInternedString() ? toPointer<InternedStringImpl>() : other.toPointer<InternedStringImpl>();
            const auto* staticString = isStaticString() ? getStaticString() : other.getStaticString();
            // A null interned pointer is the empty string (see StringBox).
            auto internedView = interned != nullptr ? interned->toStringView() : std::string_view();
            return staticStringEqualsUTF8(*staticString, internedView);
        }

        return false;
    }

    switch (getType()) {
        case ValueType::Null:
            return true;
        case ValueType::Undefined:
            return true;
        case ValueType::InternedString:
            // Same as StringBox::operator==, without the two StringBox temporaries (4 refcount atomics).
            return _data.p == other._data.p;
        case ValueType::StaticString:
            return *getStaticString() == *other.getStaticString();
        case ValueType::Int:
            return _data.i == other._data.i;
        case ValueType::Long:
            return _data.l == other._data.l;
        case ValueType::Double:
            return _data.d == other._data.d;
        case ValueType::Bool:
            return _data.b == other._data.b;
        case ValueType::Map:
            return *getMap() == *other.getMap();
        case ValueType::Array:
            return *getArray() == *other.getArray();
        case ValueType::TypedArray:
            return *getTypedArray() == *other.getTypedArray();
        case ValueType::Function:
            return getFunction() == other.getFunction();
        case ValueType::Error:
            return getError() == other.getError();
        case ValueType::TypedObject:
            return *getTypedObject() == *other.getTypedObject();
        case ValueType::ProxyTypedObject:
            return getProxyObject() == other.getProxyObject();
        case ValueType::ValdiObject:
            return getValdiObject().get() == other.getValdiObject().get();
    }
}

bool Value::operator!=(const Value& other) const noexcept {
    return !(*this == other);
}

bool Value::operator<(const Value& other) const noexcept {
    // TODO(simon): Implementation is incomplete
    switch (_type) {
        case ValueType::Null:
            return !other.isNull();
        case ValueType::Undefined:
            return !other.isUndefined();
        case ValueType::InternedString:
            return toStringBox().toStringView() < other.toStringBox().toStringView();
        case ValueType::StaticString:
            return getStaticString() < other.getStaticString();
        case ValueType::Int:
            return toInt() < other.toInt();
        case ValueType::Long:
            return toLong() < other.toLong();
        case ValueType::Double:
            return toDouble() < other.toDouble();
        case ValueType::Bool:
            return static_cast<int>(toBool()) < static_cast<int>(other.toBool());
        case ValueType::Map:
            return getMap() < other.getMap();
        case ValueType::Array:
            return getArray() < other.getArray();
        case ValueType::TypedArray:
            return getTypedArray() < other.getTypedArray();
        case ValueType::Function:
            return getFunction() < other.getFunction();
        case ValueType::Error:
            return getError().toString() < other.getError().toString();
        case ValueType::TypedObject:
            return getTypedObject() < other.getTypedObject();
        case ValueType::ProxyTypedObject:
            return getProxyObject() < other.getProxyObject();
        case ValueType::ValdiObject:
            return getValdiObject().get() < other.getValdiObject().get();
    }
}

/**
 Public getters and conversions
 */

const ValueMap* Value::getMap() const noexcept {
    if (getType() == ValueType::Map) {
        return toPointer<ValueMap>();
    }

    return nullptr;
}

const ValueArray* Value::getArray() const noexcept {
    if (getType() == ValueType::Array) {
        return toPointer<ValueArray>();
    }

    return nullptr;
}

const ValueTypedArray* Value::getTypedArray() const noexcept {
    if (getType() == ValueType::TypedArray) {
        return toPointer<ValueTypedArray>();
    }

    return nullptr;
}

Ref<ValueTypedArray> Value::getTypedArrayRef() const noexcept {
    if (getType() == ValueType::TypedArray) {
        return getUnsafeTypedRefPointer<ValueTypedArray>();
    }

    return nullptr;
}

ValueFunction* Value::getFunction() const noexcept {
    if (getType() == ValueType::Function) {
        return toPointer<ValueFunction>();
    }

    return nullptr;
}

const ValueTypedObject* Value::getTypedObject() const noexcept {
    if (getType() == ValueType::TypedObject) {
        return toPointer<ValueTypedObject>();
    }

    return nullptr;
}

const ValueTypedProxyObject* Value::getProxyObject() const noexcept {
    if (getType() == ValueType::ProxyTypedObject) {
        return toPointer<ValueTypedProxyObject>();
    }

    return nullptr;
}

const StaticString* Value::getStaticString() const noexcept {
    if (getType() == ValueType::StaticString) {
        return toPointer<StaticString>();
    }

    return nullptr;
}

Error Value::getError() const noexcept {
    if (isError()) {
        return Error(getUnsafeTypedRefPointer<ErrorStorage>());
    }

    return Error();
}

Ref<ValueFunction> Value::getFunctionRef() const noexcept {
    if (getType() == ValueType::Function) {
        return getUnsafeTypedRefPointer<ValueFunction>();
    }

    return nullptr;
}

Ref<ValueArray> Value::getArrayRef() const noexcept {
    if (getType() == ValueType::Array) {
        return getUnsafeTypedRefPointer<ValueArray>();
    }

    return nullptr;
}

Ref<ValueMap> Value::getMapRef() const noexcept {
    if (getType() == ValueType::Map) {
        return getUnsafeTypedRefPointer<ValueMap>();
    }

    return nullptr;
}

Ref<ValueTypedObject> Value::getTypedObjectRef() const noexcept {
    if (getType() == ValueType::TypedObject) {
        return getUnsafeTypedRefPointer<ValueTypedObject>();
    }

    return nullptr;
}

Ref<ValueTypedProxyObject> Value::getTypedProxyObjectRef() const noexcept {
    if (getType() == ValueType::ProxyTypedObject) {
        return getUnsafeTypedRefPointer<ValueTypedProxyObject>();
    }

    return nullptr;
}

Ref<ValdiObject> Value::getValdiObject() const noexcept {
    if (getType() == ValueType::ValdiObject) {
        return getUnsafeTypedRefPointer<ValdiObject>();
    }

    return nullptr;
}

Ref<StaticString> Value::getStaticStringRef() const noexcept {
    if (getType() == ValueType::StaticString) {
        return getUnsafeTypedRefPointer<StaticString>();
    }

    return nullptr;
}

StringBox Value::toStringBox() const {
    if (isInternedString()) {
        return StringBox(Ref<InternedStringImpl>(toPointer<InternedStringImpl>()));
    } else if (isStaticString()) {
        const auto* staticString = getStaticString();
        switch (staticString->encoding()) {
            case StaticString::Encoding::UTF8:
                return StringCache::getGlobal().makeString(staticString->utf8StringView());
            case StaticString::Encoding::UTF16:
                return StringCache::getGlobal().makeStringFromUTF16(staticString->utf16Data(), staticString->size());
            case StaticString::Encoding::UTF32: {
                auto storage = staticString->utf8Storage();
                return StringCache::getGlobal().makeString(storage.data, storage.length);
            }
        }
    }

    return StringCache::getGlobal().makeString(toString());
}

std::vector<StringBox> Value::sortedMapKeys() const {
    std::vector<StringBox> keys;

    const auto* map = getMap();
    if (map != nullptr) {
        keys.reserve(map->size());

        for (const auto& it : *map) {
            keys.emplace_back(it.first);
        }

        std::sort(keys.begin(), keys.end(), [](const StringBox& left, const StringBox& right) -> bool {
            return left.toStringView() < right.toStringView();
        });
    }

    return keys;
}

bool Value::toBool() const noexcept {
    switch (getType()) {
        case ValueType::Bool:
            return _data.b;
        case ValueType::Int:
            return toInt() != 0;
        case ValueType::Double:
            return toDouble() != 0.0;
        case ValueType::InternedString:
        case ValueType::StaticString: {
            auto str = toStringBox();
            return str == "true" || str == "1";
        }

        default:
            return false;
    }
}

std::string Value::toString() const {
    return toString(false);
}

// Convert value to string
std::string Value::toString(bool indent) const {
    switch (getType()) {
        case ValueType::InternedString:
            return toStringBox().slowToString();
        case ValueType::StaticString:
            return getStaticString()->toStdString();
        case ValueType::Null:
            return "<null>";
        case ValueType::Undefined:
            return "<undefined>";
        case ValueType::Int:
            return std::to_string(toInt());
        case ValueType::Long:
            return std::to_string(toLong());
        case ValueType::Double:
            return std::to_string(toDouble());
        case ValueType::Bool:
            return toBool() ? "true" : "false";
        case ValueType::Array: {
            std::string out;
            if (indent) {
                out.append("[\n");
            } else {
                out.append("[ ");
            }

            auto hasAppended = false;

            for (const Value& item : *getArray()) {
                if (hasAppended) {
                    if (indent) {
                        out.append(",\n");
                    } else {
                        out.append(", ");
                    }
                }

                auto itemString = item.toString(indent);
                if (indent) {
                    itemString = Valdi::indentString(itemString);
                }

                out.append(itemString);
                hasAppended = true;
            }

            if (indent) {
                out.append("\n]");
            } else {
                out.append(" ]");
            }

            return out;
        }
        case ValueType::TypedArray: {
            const auto* typedArray = getTypedArray();
            return fmt::format("<TypedArray: type={} size={}>",
                               stringForTypedArrayType(typedArray->getType()),
                               typedArray->getBuffer().size());
        }
        case ValueType::Map: {
            std::string out;
            if (indent) {
                out.append("{\n");
            } else {
                out.append("{ ");
            }

            const auto* map = getMap();

            std::vector<std::pair<StringBox, Value>> values;
            values.reserve(map->size());

            for (const auto& it : *map) {
                auto insertionIt =
                    std::upper_bound(values.begin(), values.end(), it, [=](const auto& left, const auto& right) {
                        return left.first.toStringView() < right.first.toStringView();
                    });

                values.emplace(insertionIt, it);
            }

            auto hasAppended = false;
            for (const auto& it : values) {
                if (hasAppended) {
                    if (indent) {
                        out.append(",\n");
                    } else {
                        out.append(", ");
                    }
                }

                auto element = it.first.slowToString();
                element.append(": ");
                element.append(it.second.toString(indent));

                if (indent) {
                    element = Valdi::indentString(element);
                }

                out.append(element);

                hasAppended = true;
            }

            if (indent) {
                out.append("\n}");
            } else {
                out.append("} ");
            }
            return out;
        }
        case ValueType::Function: {
            auto sharedFunction = getFunctionRef();
            std::ostringstream out;
            out << "<function ";
            out << sharedFunction->getDebugDescription();
            out << ">";
            return out.str();
        }
        case ValueType::Error: {
            std::ostringstream out;
            out << "<error: ";
            out << getError();
            out << ">";
            return out.str();
        }
        case ValueType::TypedObject: {
            const auto* typedObject = getTypedObject();
            std::ostringstream out;
            out << "<typedObject " << typedObject->getClassName().toStringView() << ": ";

            out << Value(getTypedObject()->toValueMap(true)).toString(indent);

            out << ">";
            return out.str();
        }
        case ValueType::ProxyTypedObject: {
            const auto* proxyObject = getProxyObject();
            std::ostringstream out;
            out << "<proxyObject " << proxyObject->getType() << " on "
                << proxyObject->getTypedObject()->getClassName().toStringView() << ": ";

            out << Value(proxyObject->getTypedObject()->toValueMap(true)).toString(indent);

            out << ">";
            return out.str();
        }
        case ValueType::ValdiObject: {
            std::ostringstream out;
            out << "<";
            out << toPointer<ValdiObject>()->getDebugDescription();
            out << ">";
            return out.str();
        }
    }
}

double Value::toDouble() const noexcept {
    switch (getType()) {
        case ValueType::Double:
            return _data.d;
        case ValueType::InternedString:
        case ValueType::StaticString:
            return atof(toStringBox().getCStr()); // NOLINT(cert-err34-c)
        default:
            return static_cast<double>(toLong());
    }
}

float Value::toFloat() const noexcept {
    return static_cast<float>(toDouble());
}

int32_t Value::toInt() const noexcept {
    switch (getType()) {
        case ValueType::Int:
            return _data.i;
        case ValueType::Double:
            return static_cast<int32_t>(toDouble());
        case ValueType::Long:
            return static_cast<int32_t>(_data.l);
        case ValueType::InternedString:
        case ValueType::StaticString:
            return atoi(toStringBox().getCStr()); // NOLINT(cert-err34-c)
        case ValueType::Bool:
            return static_cast<int32_t>(_data.i);
        default:
            return 0;
    }
}

int64_t Value::toLong() const noexcept {
    switch (getType()) {
        case ValueType::Long:
            return _data.l;
        case ValueType::Int:
            return static_cast<int64_t>(_data.i);
        case ValueType::Double:
            return static_cast<int64_t>(toDouble());
        case ValueType::InternedString:
        case ValueType::StaticString:
            return atoll(toStringBox().getCStr()); // NOLINT(cert-err34-c)
        case ValueType::Bool:
            return static_cast<int64_t>(_data.b);
        default:
            return 0;
    }
}

ValueType Value::getType() const noexcept {
    return _type;
}

bool Value::isNumber() const noexcept {
    return _type == ValueType::Int || _type == ValueType::Double || _type == ValueType::Long ||
           _type == ValueType::Bool;
}

bool Value::isNull() const noexcept {
    return _type == ValueType::Null;
}

bool Value::isUndefined() const noexcept {
    return _type == ValueType::Undefined;
}

bool Value::isNullOrUndefined() const noexcept {
    return isNull() || isUndefined();
}

bool Value::isBool() const noexcept {
    return _type == ValueType::Bool;
}

bool Value::isDouble() const noexcept {
    return _type == ValueType::Double;
}

bool Value::isInt() const noexcept {
    return _type == ValueType::Int;
}

bool Value::isLong() const noexcept {
    return _type == ValueType::Long;
}

bool Value::isString() const noexcept {
    return isInternedString() || isStaticString();
}

bool Value::isInternedString() const noexcept {
    return _type == ValueType::InternedString;
}

bool Value::isStaticString() const noexcept {
    return _type == ValueType::StaticString;
}

bool Value::isMap() const noexcept {
    return _type == ValueType::Map;
}

bool Value::isArray() const noexcept {
    return _type == ValueType::Array;
}

bool Value::isError() const noexcept {
    return _type == ValueType::Error;
}

bool Value::isTypedArray() const noexcept {
    return _type == ValueType::TypedArray;
}

bool Value::isFunction() const noexcept {
    return _type == ValueType::Function;
}

bool Value::isTypedObject() const noexcept {
    return _type == ValueType::TypedObject;
}

bool Value::isProxyObject() const noexcept {
    return _type == ValueType::ProxyTypedObject;
}

bool Value::isValdiObject() const noexcept {
    return _type == ValueType::ValdiObject;
}

size_t Value::hash() const {
    switch (_type) {
        case ValueType::Null:
        case ValueType::Undefined:
            return 0;
        case ValueType::InternedString:
            // Same as StringBox::hash(), without the StringBox temporary.
            return reinterpret_cast<size_t>(_data.p);
        case ValueType::StaticString:
            return getStaticString()->hash();
        case ValueType::Int:
            return static_cast<size_t>(toInt());
        case ValueType::Long:
            return static_cast<size_t>(toLong());
        case ValueType::Double:
            return static_cast<size_t>(toDouble());
        case ValueType::Bool:
            return static_cast<size_t>(toBool());
        case ValueType::Map: {
            std::size_t hash = 0;

            for (const auto& it : *getMap()) {
                boost::hash_combine(hash, it.first.hash());
                boost::hash_combine(hash, it.second.hash());
            }
            return hash;
        }
        case ValueType::Array: {
            std::size_t hash = 0;

            for (const auto& it : *getArray()) {
                boost::hash_combine(hash, it.hash());
            }
            return hash;
        }
        case ValueType::TypedArray:
            return getTypedArray()->getBuffer().hash();
        case ValueType::Function:
            return reinterpret_cast<size_t>(getFunction());
        case ValueType::Error:
            return getError().hash();
        case ValueType::TypedObject:
            return getTypedObject()->hash();
        case ValueType::ProxyTypedObject:
            return reinterpret_cast<size_t>(getProxyObject());
        case ValueType::ValdiObject:
            return reinterpret_cast<size_t>(getValdiObject().get());
    }
}

Error Value::invalidTypeError(ValueType expectedType, ValueType actualType) {
    return Error(STRING_FORMAT(
        "Cannot convert type '{}' to type '{}'", valueTypeToString(actualType), valueTypeToString(expectedType)));
}

void Value::onTypedValdiObjectError(const Ref<ValdiObject>& valdiObject, ExceptionTracker& exceptionTracker) {
    exceptionTracker.onError(
        fmt::format("Unexpected found incompatible object of class '{}'", valdiObject->getClassName()));
}

Value Value::getMapValue(std::string_view str) const {
    return getMapValue(StringCache::getGlobal().makeString(str));
}

Value Value::getMapValue(const char* str) const {
    return getMapValue(std::string_view(str));
}

Value Value::getMapValue(const StringBox& str) const {
    const auto* map = getMap();
    if (map == nullptr) {
        return Value::undefined();
    }

    auto it = map->find(str);
    if (it == map->end()) {
        return Value::undefined();
    }

    return it->second;
}

Value& Value::setMapValue(std::string_view str, const Value& value) {
    return setMapValue(StringCache::getGlobal().makeString(str), value);
}

Value& Value::setMapValue(const StringBox& str, const Value& value) {
    auto map = getMapRef();

    if (map == nullptr) {
        map = makeShared<ValueMap>();
        *this = Value(map);
    }

    (*map)[str] = value;

    return *this;
}

std::ostream& operator<<(std::ostream& os, const Value& value) {
    return os << "\n" << value.toString(true);
}

std::ostream& operator<<(std::ostream& os, const ValueType& valueType) {
    return os << valueTypeToString(valueType);
}

template<>
int32_t Value::to() const {
    return toInt();
}

template<>
int64_t Value::to() const {
    return toLong();
}

template<>
bool Value::to() const {
    return toBool();
}

template<>
double Value::to() const {
    return toDouble();
}

template<>
StringBox Value::to() const {
    return toStringBox();
}

template<>
std::string Value::to() const {
    return toString();
}

template<>
Ref<ValueMap> Value::to() const {
    return getMapRef();
}

template<>
Ref<ValueFunction> Value::to() const {
    return getFunctionRef();
}

template<>
Value Value::to() const {
    return *this;
}

template<>
int32_t Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isNumber()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Int, getType()));
        return 0;
    }

    return toInt();
}

template<>
int64_t Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isNumber()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Long, getType()));
        return 0;
    }

    return toLong();
}

template<>
bool Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isNumber()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Bool, getType()));
        return false;
    }

    return toBool();
}

template<>
double Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isNumber()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Double, getType()));
        return 0.0;
    }

    return toDouble();
}

template<>
StringBox Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isString()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::InternedString, getType()));
        return StringBox();
    }

    return toStringBox();
}

template<>
Ref<StaticString> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isStaticString()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::StaticString, getType()));
        return nullptr;
    }

    return getStaticStringRef();
}

template<>
Ref<ValueArray> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isArray()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Array, getType()));
        return nullptr;
    }

    return getArrayRef();
}

template<>
Ref<ValueFunction> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isFunction()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Function, getType()));
        return nullptr;
    }

    return getFunctionRef();
}

template<>
Ref<ValueMap> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isMap()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::Map, getType()));
        return nullptr;
    }

    return getMapRef();
}

template<>
Ref<ValueTypedArray> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isTypedArray()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::TypedArray, getType()));
        return nullptr;
    }

    return getTypedArrayRef();
}

template<>
Ref<ValdiObject> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isValdiObject()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::ValdiObject, getType()));
        return nullptr;
    }

    return getValdiObject();
}

template<>
Ref<ValueTypedObject> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isTypedObject()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::TypedObject, getType()));
        return nullptr;
    }

    return getTypedObjectRef();
}

template<>
Ref<ValueTypedProxyObject> Value::checkedTo(ExceptionTracker& exceptionTracker) const {
    if (!isProxyObject()) {
        exceptionTracker.onError(Value::invalidTypeError(ValueType::ProxyTypedObject, getType()));
        return nullptr;
    }

    return getTypedProxyObjectRef();
}

template<>
Void Value::checkedTo(ExceptionTracker& /*exceptionTracker*/) const {
    return Void();
}

// NOLINTEND(cppcoreguidelines-pro-type-union-access)

} // namespace Valdi

namespace std {

std::size_t hash<Valdi::Value>::operator()(const Valdi::Value& k) const noexcept {
    return k.hash();
}

} // namespace std
