//
//  StaticString.cpp
//  valdi_core-android
//
//  Created by Simon Corsin on 2/22/23.
//

#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Text/UTF16Utils.hpp"
#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"

namespace Valdi {

using StaticStringAllocator = InlineContainerAllocator<StaticString, uint8_t>;

StaticString::StaticString(size_t size, Encoding encoding) : _size(size), _encoding(encoding) {}
StaticString::~StaticString() = default;

size_t StaticString::size() const {
    return _size;
}

StaticString::Encoding StaticString::encoding() const {
    return _encoding;
}

const void* StaticString::data() const {
    StaticStringAllocator allocator;
    return allocator.getContainerStartPtr(this);
}

void* StaticString::data() {
    StaticStringAllocator allocator;
    return allocator.getContainerStartPtr(this);
}

std::string_view StaticString::utf8StringView() const {
    return std::string_view(utf8Data(), size());
}

std::u16string_view StaticString::utf16StringView() const {
    return std::u16string_view(utf16Data(), size());
}

std::u32string_view StaticString::utf32StringView() const {
    return std::u32string_view(utf32Data(), size());
}

size_t StaticString::hash() const {
    switch (encoding()) {
        case StaticString::Encoding::UTF8:
            return std::hash<std::string_view>()(utf8StringView());
        case StaticString::Encoding::UTF16:
            return std::hash<std::u16string_view>()(utf16StringView());
        case StaticString::Encoding::UTF32:
            return std::hash<std::u32string_view>()(utf32StringView());
    }
}

bool StaticString::operator==(const StaticString& other) const {
    if (this == &other) {
        return true;
    }

    switch (encoding()) {
        case StaticString::Encoding::UTF8: {
            if (other.encoding() != StaticString::Encoding::UTF8) {
                return false;
            }

            return utf8StringView() == other.utf8StringView();
        }
        case StaticString::Encoding::UTF16: {
            if (other.encoding() != StaticString::Encoding::UTF16) {
                return false;
            }
            return utf16StringView() == other.utf16StringView();
        }
        case StaticString::Encoding::UTF32: {
            if (other.encoding() != StaticString::Encoding::UTF32) {
                return false;
            }
            return utf32StringView() == other.utf32StringView();
        }
    }
}

bool StaticString::operator!=(const StaticString& other) const {
    return !(*this == other);
}

std::string StaticString::toStdString() const {
    auto asUTF8 = utf8Storage();
    return std::string(asUTF8.data, asUTF8.length);
}

std::wstring StaticString::toStdWString() const {
    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "wchar_t must be 2 or 4 bytes");
    if constexpr (sizeof(wchar_t) == 2) {
        auto asUTF16 = utf16Storage();
        return std::wstring(reinterpret_cast<const wchar_t*>(asUTF16.data), asUTF16.length);
    } else {
        auto asUTF32 = utf32Storage();
        return std::wstring(reinterpret_cast<const wchar_t*>(asUTF32.data), asUTF32.length);
    }
}

StaticStringUTF8Storage StaticString::utf8Storage() const {
    switch (encoding()) {
        case StaticString::Encoding::UTF8:
            return StaticStringUTF8Storage(utf8Data(), size());
        case StaticString::Encoding::UTF16: {
            auto pair = utf16ToUtf8(utf16Data(), size());
            return StaticStringUTF8Storage(pair.first, pair.second);
        }
        case StaticString::Encoding::UTF32: {
            auto pair = utf32ToUtf8(reinterpret_cast<const uint32_t*>(utf32Data()), size());
            return StaticStringUTF8Storage(pair.first, pair.second);
        }
    }
}

StaticStringUTF16Storage StaticString::utf16Storage() const {
    switch (encoding()) {
        case StaticString::Encoding::UTF8: {
            auto pair = utf8ToUtf16(utf8Data(), size());
            return StaticStringUTF16Storage(pair.first, pair.second);
        }
        case StaticString::Encoding::UTF16:
            return StaticStringUTF16Storage(utf16Data(), size());
        case StaticString::Encoding::UTF32: {
            auto pair = utf32ToUtf16(reinterpret_cast<const uint32_t*>(utf32Data()), size());
            return StaticStringUTF16Storage(pair.first, pair.second);
        }
    }
}

StaticStringUTF32Storage StaticString::utf32Storage() const {
    switch (encoding()) {
        case StaticString::Encoding::UTF8: {
            auto pair = utf8ToUtf32(utf8Data(), size());
            return StaticStringUTF32Storage(reinterpret_cast<const char32_t*>(pair.first), pair.second);
        }
        case StaticString::Encoding::UTF16: {
            auto pair = utf16ToUtf32(utf16Data(), size());
            return StaticStringUTF32Storage(reinterpret_cast<const char32_t*>(pair.first), pair.second);
        }
        case StaticString::Encoding::UTF32:
            return StaticStringUTF32Storage(utf32Data(), size());
    }
}

Ref<StaticString> StaticString::makeUTF8(size_t size) {
    StaticStringAllocator allocator;
    auto ref = allocator.allocate((size * sizeof(char)) + 1, size, Encoding::UTF8);
    // Always null terminate UTF8 strings
    ref->utf8Data()[size] = 0;
    return ref;
}

Ref<StaticString> StaticString::makeUTF16(size_t size) {
    StaticStringAllocator allocator;
    return allocator.allocate(size * sizeof(char16_t), size, Encoding::UTF16);
}

Ref<StaticString> StaticString::makeUTF32(size_t size) {
    StaticStringAllocator allocator;
    return allocator.allocate(size * sizeof(char32_t), size, Encoding::UTF32);
}

Ref<StaticString> StaticString::makeUTF8(const char* data, size_t size) {
    auto out = makeUTF8(size);

    std::memcpy(out->data(), data, size * sizeof(char));

    return out;
}

Ref<StaticString> StaticString::makeUTF8(std::string_view str) {
    return makeUTF8(str.data(), str.size());
}

Ref<StaticString> StaticString::makeUTF16(const char16_t* data, size_t size) {
    auto out = makeUTF16(size);

    std::memcpy(out->data(), data, size * sizeof(char16_t));

    return out;
}

Ref<StaticString> StaticString::makeUTF32(const char32_t* data, size_t size) {
    auto out = makeUTF32(size);

    std::memcpy(out->data(), data, size * sizeof(char32_t));

    return out;
}

Ref<StaticString> StaticString::makeWithWideChars(const wchar_t* data, size_t size) {
    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "wchar_t must be 2 or 4 bytes");
    if constexpr (sizeof(wchar_t) == 2) {
        return makeUTF16(reinterpret_cast<const char16_t*>(data), size);
    } else {
        return makeUTF32(reinterpret_cast<const char32_t*>(data), size);
    }
}

std::wstring StaticString::utf8ToWString(std::string_view str) {
    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "wchar_t must be 2 or 4 bytes");
    if constexpr (sizeof(wchar_t) == 2) {
        auto pair = utf8ToUtf16(str.data(), str.size());
        return std::wstring(reinterpret_cast<const wchar_t*>(pair.first), pair.second);
    } else {
        auto pair = utf8ToUtf32(str.data(), str.size());
        return std::wstring(reinterpret_cast<const wchar_t*>(pair.first), pair.second);
    }
}

std::ostream& operator<<(std::ostream& os, const StaticString& value) {
    auto storage = value.utf8Storage();
    return os << std::string_view(storage.data, storage.length);
}

} // namespace Valdi
