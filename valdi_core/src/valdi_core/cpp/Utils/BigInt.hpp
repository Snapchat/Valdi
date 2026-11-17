#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace Valdi {

class ExceptionTracker;
template<typename T, typename Item>
struct InlineContainerAllocator;

/** An immutable integer. Large decimal payloads share the object's allocation. */
class BigInt final : public SimpleRefCountable {
public:
    ~BigInt() override;

    BigInt(const BigInt&) = delete;
    BigInt& operator=(const BigInt&) = delete;

    static Ref<BigInt> make(int64_t value);
    static Ref<BigInt> make(std::string_view decimal, ExceptionTracker& exceptionTracker);

    std::optional<int64_t> toInt64() const;
    std::string toString() const;

    bool operator==(const BigInt& other) const noexcept;
    bool operator!=(const BigInt& other) const noexcept;
    bool operator<(const BigInt& other) const noexcept;
    bool operator<=(const BigInt& other) const noexcept;
    bool operator>(const BigInt& other) const noexcept;
    bool operator>=(const BigInt& other) const noexcept;
    size_t hash() const noexcept;

private:
    union {
        int64_t value;
        size_t length;
    } _storage;
    bool _isInt64;

    explicit BigInt(int64_t value);
    BigInt(std::string_view digits, bool negative);
    std::string_view decimal() const;
    int compare(const BigInt& other) const noexcept;

    friend InlineContainerAllocator<BigInt, char>;
    friend std::ostream& operator<<(std::ostream& stream, const BigInt& value);
};

std::ostream& operator<<(std::ostream& stream, const BigInt& value);

} // namespace Valdi

namespace std {

template<>
struct hash<Valdi::BigInt> {
    size_t operator()(const Valdi::BigInt& value) const noexcept;
};

} // namespace std
