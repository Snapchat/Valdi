//
//  BytesUtils.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/20/22.
//

#include "snap_drawing/cpp/Utils/BytesUtils.hpp"

#include <algorithm>
#include <cstdint>

namespace snap::drawing {

class SkiaDataHolder : public Valdi::SimpleRefCountable {
public:
    explicit SkiaDataHolder(const sk_sp<SkData>& data) : _data(data) {}

    ~SkiaDataHolder() override = default;

    const sk_sp<SkData>& getData() const {
        return _data;
    }

private:
    sk_sp<SkData> _data;
};

Valdi::BytesView bytesFromSkData(const sk_sp<SkData>& data) {
    if (data == nullptr) {
        return Valdi::BytesView();
    }

    return Valdi::BytesView(
        Valdi::makeShared<SkiaDataHolder>(data), reinterpret_cast<const Valdi::Byte*>(data->data()), data->size());
}

static void releaseData(const void* /*ptr*/, void* context) {
    Valdi::unsafeBridgeRelease(context);
}

sk_sp<SkData> skDataFromBytes(const Valdi::BytesView& bytes, DataConversionMode conversionMode) {
    auto dataHolder = Valdi::castOrNull<SkiaDataHolder>(bytes.getSource());

    switch (conversionMode) {
        case DataConversionModeCopyIfUnsafe: {
            if (dataHolder != nullptr) {
                return dataHolder->getData();
            }

            if (bytes.empty()) {
                return SkData::MakeEmpty();
            }

            if (bytes.getSource() == nullptr) {
                return SkData::MakeWithCopy(bytes.data(), bytes.size());
            } else {
                auto* source = Valdi::unsafeBridgeRetain(bytes.getSource().get());
                return SkData::MakeWithProc(bytes.data(), bytes.size(), &releaseData, source);
            }
        }
        case DataConversionModeAlwaysCopy: {
            return SkData::MakeWithCopy(bytes.data(), bytes.size());
        }
        case DataConversionModeNeverCopy: {
            if (dataHolder != nullptr) {
                return dataHolder->getData();
            }

            if (bytes.getSource() == nullptr) {
                return SkData::MakeWithoutCopy(bytes.data(), bytes.size());
            } else {
                auto* source = Valdi::unsafeBridgeRetain(bytes.getSource().get());
                return SkData::MakeWithProc(bytes.data(), bytes.size(), &releaseData, source);
            }
        };
    }

    return nullptr;
}

namespace {

constexpr size_t kMaxMagicBytes = 12;
constexpr size_t kMaxSniffBytes = 64;

std::string toHex(const Valdi::Byte* data, size_t length) {
    static constexpr char kHexDigits[] = "0123456789abcdef";

    std::string hex;
    hex.reserve(length * 2);
    for (size_t i = 0; i < length; i++) {
        const auto byte = static_cast<uint8_t>(data[i]);
        hex.push_back(kHexDigits[byte >> 4]);
        hex.push_back(kHexDigits[byte & 0x0F]);
    }

    return hex;
}

bool startsWithIgnoringCase(std::string_view value, std::string_view lowercasePrefix) {
    if (value.size() < lowercasePrefix.size()) {
        return false;
    }

    for (size_t i = 0; i < lowercasePrefix.size(); i++) {
        const auto character = static_cast<unsigned char>(value[i]);
        const auto lowered =
            static_cast<char>(character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character);
        if (lowered != lowercasePrefix[i]) {
            return false;
        }
    }

    return true;
}

std::string_view payloadKind(const Valdi::BytesView& bytes) {
    if (bytes.data() == nullptr || bytes.size() == 0) {
        return "Empty payload";
    }

    std::string_view leading(reinterpret_cast<const char*>(bytes.data()), std::min(bytes.size(), kMaxSniffBytes));
    const auto contentStart = leading.find_first_not_of(" \t\r\n");
    if (contentStart == std::string_view::npos) {
        return {};
    }
    leading = leading.substr(contentStart);

    if (startsWithIgnoringCase(leading, "<!doctype html") || startsWithIgnoringCase(leading, "<html")) {
        return "Non-image HTML body";
    }

    if (startsWithIgnoringCase(leading, "http/")) {
        return "Non-image HTTP response body";
    }

    if (leading.front() == '{') {
        return "Non-image JSON body";
    }

    return {};
}

} // namespace

std::string describePayloadBytes(const Valdi::BytesView& bytes) {
    const size_t magicLength = bytes.data() == nullptr ? 0 : std::min(bytes.size(), kMaxMagicBytes);

    std::string description("bytes=");
    description += std::to_string(bytes.size());
    description += " magic=";
    description += toHex(bytes.data(), magicLength);

    return description;
}

std::string describeUndecodablePayload(const Valdi::BytesView& bytes, std::string_view fallbackLabel) {
    const auto kind = payloadKind(bytes);

    std::string message(kind.empty() ? fallbackLabel : kind);
    message += " (";
    message += describePayloadBytes(bytes);
    message += ")";

    return message;
}

} // namespace snap::drawing
