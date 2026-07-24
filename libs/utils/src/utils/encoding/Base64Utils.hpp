#pragma once

#include <string>
#include <vector>

namespace snap::utils::encoding {

/**
 * @brief Base64 encode the uint8_t buffer and return the result as a string.
 * No newlines are inserted into the result.
 */
std::string binaryToBase64(const uint8_t* data, size_t size);

/**
 * @brief Base64 encode the uint8_t vector and return the result as a string.
 * No newlines are inserted into the result.
 */
std::string binaryToBase64(const std::vector<uint8_t>& binary);

/**
 * @brief Base64 encode the bytes in the string and return the result as a string.
 * No newlines are inserted into the result.
 */
std::string binaryToBase64(const std::string& binary);

/**
 * @brief Interpret the uint64_t as an 8-byte binary value, Base64 encode it and
 * return the result as a string. No newlines are inserted into the result.
 */
std::string uint64ToBase64(uint64_t data);

/**
 * @brief Base64 decode the string and store the resulting bytes in the returned vector.
 * Newlines are stripped from the string before decoding.
 * Returns empty vector if failed to decode.
 */
std::vector<uint8_t> base64ToBinary(std::string_view base64);

/**
 * @brief Base64 decode the string and store the resulting bytes in the |decoded| vector.
 * Newlines are stripped from the string before decoding.
 * Returns false if failed to decode.
 */
bool base64ToBinary(std::string_view base64, std::vector<uint8_t>& decoded);

/**
 * @brief Base64-url decode the string and store the resulting bytes in the |decoded| vector.
 * Newlines are stripped from the string before decoding. Missing padding is accepted.
 * Returns false if failed to decode.
 */
bool base64UrlToBinary(std::string_view base64url, std::vector<uint8_t>& decoded);

/**
 * @brief Base64 decode the string and store the first 8-bytes of the result in the
 * returned uint64_t. If the result is more than 8-bytes then it will be truncated.
 * If the result is less than 8-bytes then the remaining bytes in the uint64_t will
 * be 0x00.
 */
uint64_t base64ToUInt64(const std::string& base64);

/**
 * @brief Convert the Base64 URL string to standard Base64.
 * See RFC 4648 'Table 2: The "URL and Filename safe" Base 64 Alphabet'
 */
std::string base64UrlToBase64(const std::string& base64url);

/**
 * @brief Convert the Base64 URL string to standard Base64 in place.
 * See RFC 4648 'Table 2: The "URL and Filename safe" Base 64 Alphabet'
 */
void base64UrlToBase64InPlace(std::string& base64url);

/**
 * @brief Convert the Base64 string to a Base64 URL string.
 * See RFC 4648 'Table 2: The "URL and Filename safe" Base 64 Alphabet'
 */
std::string base64ToBase64Url(const std::string& base64);

/**
 * @brief Convert the Base64 string to a Base64 URL string in place.
 * See RFC 4648 'Table 2: The "URL and Filename safe" Base 64 Alphabet'
 */
void base64ToBase64UrlInPlace(std::string& base64);

} // namespace snap::utils::encoding
