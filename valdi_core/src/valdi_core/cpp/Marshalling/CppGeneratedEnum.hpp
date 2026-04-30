#pragma once

#include <array>

#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

namespace Valdi {

class RegisteredCppGeneratedClass;
class CppGeneratedEnum {
public:
    static RegisteredCppGeneratedClass* registerEnumSchema(const char* schemaString);
};

template<typename T, size_t kSize>
class CppIntEnumMarshaller {
public:
    explicit CppIntEnumMarshaller(std::array<int32_t, kSize> enumValues) : _enumValues(enumValues) {}

    void marshall(ExceptionTracker& exceptionTracker, T value, Value& out) const {
        auto index = static_cast<size_t>(value);
        if (index >= kSize) {
            exceptionTracker.onError(fmt::format("Invalid enum value: {}", value));
            return;
        }
        out = Value(_enumValues[index]);
    }

    void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, T& out) const {
        auto intValue = value.toInt();
        for (auto enumValue : _enumValues) {
            if (enumValue == intValue) {
                out = static_cast<T>(enumValue);
                return;
            }
        }

        exceptionTracker.onError(fmt::format("Invalid enum value: {}", intValue));
    }

private:
    std::array<int32_t, kSize> _enumValues;
};

template<typename T, size_t kSize>
class CppStringEnumMarshaller {
public:
    explicit CppStringEnumMarshaller(std::array<const char*, kSize> enumValues) {
        for (size_t i = 0; i < kSize; i++) {
            _enumValues[i] = StringBox::fromCString(enumValues[i]);
        }
    }

    void marshall(ExceptionTracker& exceptionTracker, T value, Value& out) const {
        auto index = static_cast<size_t>(value);
        if (index >= kSize) {
            exceptionTracker.onError(fmt::format("Invalid enum value: {}", value));
            return;
        }
        out = Value(_enumValues[index]);
    }

    void unmarshall(ExceptionTracker& exceptionTracker, const Value& value, T& out) const {
        auto stringValue = value.toStringBox();
        size_t index = 0;
        for (auto enumValue : _enumValues) {
            if (enumValue == stringValue) {
                out = static_cast<T>(index);
                return;
            }
            index++;
        }

        exceptionTracker.onError(fmt::format("Invalid enum value: {}", stringValue));
    }

private:
    std::array<StringBox, kSize> _enumValues;
};

} // namespace Valdi