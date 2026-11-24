#include "valdi_core/cpp/Utils/BigInt.hpp"

#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"

#include <charconv>
#include <cstring>
#include <limits>

namespace Valdi {

BigInt::BigInt(int64_t value) : _isInt64(true) {
    _storage.value = value;
}

BigInt::BigInt(std::string_view digits, bool negative) : _isInt64(false) {
    _storage.length = digits.size() + static_cast<size_t>(negative);
    auto* data = InlineContainerAllocator<BigInt, char>().getContainerStartPtr(this);
    if (negative) {
        *data++ = '-';
    }
    std::memcpy(data, digits.data(), digits.size());
}

BigInt::~BigInt() = default;

Ref<BigInt> BigInt::make(int64_t value) {
    return InlineContainerAllocator<BigInt, char>().allocate(0, value);
}

Ref<BigInt> BigInt::make(std::string_view decimal, ExceptionTracker& exceptionTracker) {
    bool negative = false;
    if (!decimal.empty() && (decimal.front() == '-' || decimal.front() == '+')) {
        negative = decimal.front() == '-';
        decimal.remove_prefix(1);
    }
    if (decimal.empty()) {
        exceptionTracker.onError("Invalid BigInt decimal string");
        return nullptr;
    }
    for (char digit : decimal) {
        if (digit < '0' || digit > '9') {
            exceptionTracker.onError("Invalid BigInt decimal string");
            return nullptr;
        }
    }
    while (decimal.size() > 1 && decimal.front() == '0') {
        decimal.remove_prefix(1);
    }

    const uint64_t limit = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + static_cast<uint64_t>(negative);
    uint64_t magnitude = 0;
    bool fits = true;
    for (char digit : decimal) {
        const auto value = static_cast<uint64_t>(digit - '0');
        if (magnitude > (limit - value) / 10) {
            fits = false;
            break;
        }
        magnitude = magnitude * 10 + value;
    }
    if (fits) {
        // Avoid negating INT64_MIN or converting an out-of-range unsigned value.
        const auto value =
            negative && magnitude != 0 ? -static_cast<int64_t>(magnitude - 1) - 1 : static_cast<int64_t>(magnitude);
        return make(value);
    }
    return InlineContainerAllocator<BigInt, char>().allocate(
        decimal.size() + static_cast<size_t>(negative), decimal, negative);
}

std::optional<int64_t> BigInt::toInt64() const {
    if (_isInt64) {
        return _storage.value;
    }
    return std::nullopt;
}

std::string_view BigInt::decimal() const {
    return {InlineContainerAllocator<BigInt, char>().getContainerStartPtr(this), _storage.length};
}

std::string BigInt::toString() const {
    if (_isInt64) {
        return std::to_string(_storage.value);
    }
    return std::string(decimal());
}

bool BigInt::operator==(const BigInt& other) const noexcept {
    return compare(other) == 0;
}

bool BigInt::operator!=(const BigInt& other) const noexcept {
    return compare(other) != 0;
}

bool BigInt::operator<(const BigInt& other) const noexcept {
    return compare(other) < 0;
}

bool BigInt::operator<=(const BigInt& other) const noexcept {
    return compare(other) <= 0;
}

bool BigInt::operator>(const BigInt& other) const noexcept {
    return compare(other) > 0;
}

bool BigInt::operator>=(const BigInt& other) const noexcept {
    return compare(other) >= 0;
}

size_t BigInt::hash() const noexcept {
    if (_isInt64) {
        return std::hash<int64_t>()(_storage.value);
    }
    return std::hash<std::string_view>()(decimal());
}

int BigInt::compare(const BigInt& other) const noexcept {
    if (_isInt64 && other._isInt64) {
        return (_storage.value > other._storage.value) - (_storage.value < other._storage.value);
    }
    const bool negative = _isInt64 ? _storage.value < 0 : decimal().front() == '-';
    const bool otherNegative = other._isInt64 ? other._storage.value < 0 : other.decimal().front() == '-';
    if (negative != otherNegative) {
        return negative ? -1 : 1;
    }
    int magnitudeOrder;
    if (_isInt64 != other._isInt64) {
        magnitudeOrder = _isInt64 ? -1 : 1;
    } else {
        const auto left = decimal();
        const auto right = other.decimal();
        if (left.size() == right.size()) {
            const auto comparison = left.compare(right);
            magnitudeOrder = (comparison > 0) - (comparison < 0);
        } else {
            magnitudeOrder = left.size() < right.size() ? -1 : 1;
        }
    }
    return negative ? -magnitudeOrder : magnitudeOrder;
}

std::ostream& operator<<(std::ostream& stream, const BigInt& value) {
    if (value._isInt64) {
        char decimal[std::numeric_limits<int64_t>::digits10 + 2];
        const auto result = std::to_chars(decimal, decimal + sizeof(decimal), value._storage.value);
        return stream << std::string_view(decimal, result.ptr - decimal);
    }
    return stream << value.decimal();
}

} // namespace Valdi

namespace std {

size_t hash<Valdi::BigInt>::operator()(const Valdi::BigInt& value) const noexcept {
    return value.hash();
}

} // namespace std
