#include "utils/encoding/Base64Utils.hpp"

#include <openssl/base64.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

// We wrap the BoringSSL Base64 encoding/decoding functions
namespace snap::utils::encoding {

std::string binaryToBase64(const uint8_t* data, size_t size) {
    size_t maxOutSize = 0;
    if (size == 0 || EVP_EncodedLength(&maxOutSize, size) != 1 || maxOutSize == 0) {
        return "";
    }

    std::string result(maxOutSize, '\0');
    EVP_EncodeBlock(reinterpret_cast<uint8_t*>(result.data()), data, size);
    result.resize(result.size() - 1); // remove the trailing \0 that EVP_EncodeBlock always writes
    return result;
}

std::string binaryToBase64(const std::vector<uint8_t>& binary) {
    return binaryToBase64(binary.data(), binary.size());
}

std::string binaryToBase64(const std::string& binary) {
    return binaryToBase64(reinterpret_cast<const uint8_t*>(binary.data()), binary.size());
}

std::string uint64ToBase64(uint64_t data) {
    std::array<uint8_t, sizeof(data)> bytes;
    for (size_t i = 0; i < 8; i++) {
        bytes.at(i) = (data >> (i * 8)) & 0xFF;
    }
    return binaryToBase64(bytes.data(), bytes.size());
}

static bool decodePreparedBase64(const std::string& preparedBase64, std::vector<uint8_t>* ret) {
    // Determine the max array length we'll need given the string length
    size_t maxOutSize = 0;
    if (EVP_DecodedLength(&maxOutSize, preparedBase64.length()) != 1 || maxOutSize == 0) {
        return false;
    }

    ret->resize(maxOutSize);
    size_t outSize = 0;
    if (EVP_DecodeBase64(reinterpret_cast<uint8_t*>(ret->data()),
                         &outSize,
                         maxOutSize,
                         reinterpret_cast<const uint8_t*>(preparedBase64.c_str()),
                         preparedBase64.length()) != 1) {
        // make it empty to indicate error
        ret->resize(0);
        return false;
    }

    ret->resize(outSize);
    return true;
}

static bool base64ToBinaryInternal(const char* encodedString, size_t inSize, std::vector<uint8_t>* ret) {
    // Strip out newlines since the decoder returns an error code (0) if it finds them within the encoded string.
    std::string noNewlines;
    noNewlines.reserve(inSize);
    noNewlines.append(encodedString, inSize);
    noNewlines.erase(std::remove(noNewlines.begin(), noNewlines.end(), '\n'), noNewlines.end());
    noNewlines.erase(std::remove(noNewlines.begin(), noNewlines.end(), '\r'), noNewlines.end());

    return decodePreparedBase64(noNewlines, ret);
}

std::vector<uint8_t> base64ToBinary(std::string_view base64) {
    std::vector<uint8_t> decodedData;
    base64ToBinaryInternal(base64.data(), base64.size(), &decodedData);
    return decodedData;
}

bool base64ToBinary(std::string_view base64, std::vector<uint8_t>& decodedData) {
    return base64ToBinaryInternal(base64.data(), base64.size(), &decodedData);
}

bool base64UrlToBinary(std::string_view base64url, std::vector<uint8_t>& decodedData) {
    std::string standardBase64;
    standardBase64.reserve(base64url.size() + 4);
    standardBase64.append(base64url.data(), base64url.size());
    standardBase64.erase(std::remove(standardBase64.begin(), standardBase64.end(), '\n'), standardBase64.end());
    standardBase64.erase(std::remove(standardBase64.begin(), standardBase64.end(), '\r'), standardBase64.end());
    base64UrlToBase64InPlace(standardBase64);
    if (standardBase64.empty()) {
        decodedData.clear();
        return true;
    }
    return decodePreparedBase64(standardBase64, &decodedData);
}

uint64_t base64ToUInt64(const std::string& base64) {
    std::vector<uint8_t> bytes = base64ToBinary(base64);
    uint64_t retVal = 0;

    for (int i = std::min(static_cast<int>(bytes.size()), 8) - 1; i >= 0; i--) {
        retVal <<= 8;
        retVal |= bytes[i];
    }

    return retVal;
}

std::string base64UrlToBase64(const std::string& base64url) {
    std::string temp(base64url);
    base64UrlToBase64InPlace(temp);
    return temp;
}

void base64UrlToBase64InPlace(std::string& base64url) {
    // change Base64 alphabet from urlsafe version to standard
    for (auto& c : base64url) {
        if (c == '-') {
            c = '+';
        } else if (c == '_') {
            c = '/';
        }
    }

    // add padding
    if ((base64url.size() % 4) != 0u) {
        auto toAppend = 4 - static_cast<int>(base64url.size() % 4);
        base64url.append(toAppend, '=');
    }
}

std::string base64ToBase64Url(const std::string& base64) {
    std::string temp(base64);
    base64ToBase64UrlInPlace(temp);
    return temp;
}

void base64ToBase64UrlInPlace(std::string& base64) {
    // remove padding
    size_t found = base64.find_last_not_of('=');
    if (found == std::string::npos) {
        base64.clear();
        return;
    }

    // change Base64 alphabet from standard version to urlsafe
    for (auto& c : base64) {
        if (c == '+') {
            c = '-';
        } else if (c == '/') {
            c = '_';
        }
    }

    base64.resize(found + 1);
}

} // namespace snap::utils::encoding
