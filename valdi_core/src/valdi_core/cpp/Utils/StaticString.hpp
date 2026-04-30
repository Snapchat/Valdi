//
//  StaticString.hpp
//  valdi_core-android
//
//  Created by Simon Corsin on 2/22/23.
//

#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"
#include <ostream>
#include <string>

namespace Valdi {

template<typename T, typename ValueType>
struct InlineContainerAllocator;

struct StaticStringUTF8Storage {
    const char* data;
    size_t length;

    constexpr StaticStringUTF8Storage(const char* data, size_t length) : data(data), length(length) {}

    inline std::string_view toStringView() const {
        return std::string_view(data, length);
    }
};

struct StaticStringUTF16Storage {
    const char16_t* data;
    size_t length;

    constexpr StaticStringUTF16Storage(const char16_t* data, size_t length) : data(data), length(length) {}

    inline std::u16string_view toStringView() const {
        return std::u16string_view(data, length);
    }
};

struct StaticStringUTF32Storage {
    const char32_t* data;
    size_t length;

    constexpr StaticStringUTF32Storage(const char32_t* data, size_t length) : data(data), length(length) {}

    inline std::u32string_view toStringView() const {
        return std::u32string_view(data, length);
    }
};

/**
 A ref counted, heap allocated non resizable string
 that can hold either UTF8, UTF16 or UTF32 characters.
 */
class StaticString : public SimpleRefCountable {
public:
    enum class Encoding { UTF8, UTF16, UTF32 };

    ~StaticString() override;

    size_t size() const;
    Encoding encoding() const;

    std::string toStdString() const;
    std::wstring toStdWString() const;

    size_t hash() const;

    const void* data() const;
    void* data();

    inline char* utf8Data() {
        return reinterpret_cast<char*>(data());
    }

    inline const char* utf8Data() const {
        return reinterpret_cast<const char*>(data());
    }

    inline char16_t* utf16Data() {
        return reinterpret_cast<char16_t*>(data());
    }

    inline const char16_t* utf16Data() const {
        return reinterpret_cast<const char16_t*>(data());
    }

    inline char32_t* utf32Data() {
        return reinterpret_cast<char32_t*>(data());
    }

    inline const char32_t* utf32Data() const {
        return reinterpret_cast<const char32_t*>(data());
    }

    std::string_view utf8StringView() const;
    std::u16string_view utf16StringView() const;
    std::u32string_view utf32StringView() const;

    /**
     Return an object exposing the underlying string data as UTF8.
     If the string had to be converted, the returned storage will
     be backed by a thread local variable that will be overriden on
     subsequent calls of utf8Storage(). The data should be therefore
     immediately copied in another buffer.
     */
    StaticStringUTF8Storage utf8Storage() const;

    /**
     Return an object exposing the underlying string data as UTF16.
     If the string had to be converted, the returned storage will
     be backed by a thread local variable that will be overriden on
     subsequent calls of utf16Storage(). The data should be therefore
     immediately copied in another buffer.
     */
    StaticStringUTF16Storage utf16Storage() const;

    /**

     Return an object exposing the underlying string data as UTF32.
     If the string had to be converted, the returned storage will
     be backed by a thread local variable that will be overriden on
     subsequent calls of utf32Storage(). The data should be therefore
     immediately copied in another buffer.
     */
    StaticStringUTF32Storage utf32Storage() const;

    bool operator==(const StaticString& other) const;
    bool operator!=(const StaticString& other) const;

    static Ref<StaticString> makeUTF8(size_t size);
    static Ref<StaticString> makeUTF16(size_t size);
    static Ref<StaticString> makeUTF32(size_t size);

    static Ref<StaticString> makeUTF8(const char* data, size_t size);
    static Ref<StaticString> makeUTF8(std::string_view str);
    static Ref<StaticString> makeUTF16(const char16_t* data, size_t size);
    static Ref<StaticString> makeUTF32(const char32_t* data, size_t size);
    static Ref<StaticString> makeWithWideChars(const wchar_t* data, size_t size);

    static std::wstring utf8ToWString(std::string_view str);

private:
    size_t _size;
    Encoding _encoding;

    StaticString(size_t size, Encoding encoding);

    friend InlineContainerAllocator<StaticString, uint8_t>;
};

std::ostream& operator<<(std::ostream& os, const StaticString& value);

} // namespace Valdi
