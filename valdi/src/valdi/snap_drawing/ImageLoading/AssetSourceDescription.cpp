//
//  AssetSourceDescription.cpp
//  valdi-desktop-apple
//

#include "valdi/snap_drawing/ImageLoading/AssetSourceDescription.hpp"

#include <string>
#include <string_view>

namespace snap::drawing {

namespace {

constexpr size_t kMaxExtensionLength = 8;
constexpr size_t kMaxHostLength = 64;
constexpr size_t kMaxEncodedUrlLength = 1024;
constexpr std::string_view kWrapperSchemePrefix = "composer-encrypted";

int hexDigitValue(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

std::string percentDecoded(std::string_view value) {
    if (value.size() > kMaxEncodedUrlLength) {
        value = value.substr(0, kMaxEncodedUrlLength);
    }

    std::string decoded;
    decoded.reserve(value.size());

    for (size_t i = 0; i < value.size(); i++) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto high = hexDigitValue(value[i + 1]);
            const auto low = hexDigitValue(value[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>(high * 16 + low));
                i += 2;
                continue;
            }
        }
        decoded.push_back(value[i]);
    }

    return decoded;
}

std::string_view wrappedUrlParameter(std::string_view raw) {
    for (auto start = raw.find("url="); start != std::string_view::npos; start = raw.find("url=", start + 1)) {
        if (start == 0 || raw[start - 1] == '?' || raw[start - 1] == '&') {
            const auto value = raw.substr(start + 4);
            const auto valueEnd = value.find_first_of("&#");
            return valueEnd == std::string_view::npos ? value : value.substr(0, valueEnd);
        }
    }

    return {};
}

std::string describeHostAndExtension(std::string_view raw) {
    const auto schemeEnd = raw.find("://");
    if (schemeEnd == std::string_view::npos) {
        return "host=none";
    }

    const auto authority = raw.substr(schemeEnd + 3);
    const auto hostEnd = authority.find_first_of("/?#");
    const auto host = hostEnd == std::string_view::npos ? authority : authority.substr(0, hostEnd);

    auto path = hostEnd == std::string_view::npos ? std::string_view() : authority.substr(hostEnd);
    const auto pathEnd = path.find_first_of("?#");
    if (pathEnd != std::string_view::npos) {
        path = path.substr(0, pathEnd);
    }

    const auto lastSlash = path.rfind('/');
    const auto extensionStart = path.rfind('.');
    const auto hasExtension =
        extensionStart != std::string_view::npos && (lastSlash == std::string_view::npos || extensionStart > lastSlash);
    const auto extension =
        hasExtension ? path.substr(extensionStart + 1, kMaxExtensionLength) : std::string_view("none");

    return "host=" + std::string(host.substr(0, kMaxHostLength)) + " ext=" + std::string(extension);
}

} // namespace

std::string describeAssetSource(const Valdi::StringBox& url) {
    const auto raw = url.toStringView();
    const auto schemeEnd = raw.find("://");
    if (schemeEnd == std::string_view::npos) {
        return "host=none";
    }

    const auto scheme = raw.substr(0, schemeEnd);
    if (scheme.rfind(kWrapperSchemePrefix, 0) != 0) {
        return describeHostAndExtension(raw);
    }

    const auto wrapper = " wrapper=" + std::string(scheme);
    const auto inner = wrappedUrlParameter(raw.substr(schemeEnd + 3));
    if (inner.empty()) {
        return "host=none" + wrapper;
    }

    return describeHostAndExtension(percentDecoded(inner)) + wrapper;
}
} // namespace snap::drawing
