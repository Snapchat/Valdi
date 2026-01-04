#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using EVP_AEAD = struct evp_aead_st;

namespace bssl {
template<typename T>
class Span;
}

namespace snap::utils::crypto {

/**
 * Convenience class for encrypting/decrypting bytes using AES-256-GCM (256-bit keys and 96-bit nonce/iv).
 * Once initialized with a key, this class should only be used for one round of encryption.
 * Keys and IVs MUST be generated with secure randomness. See utils::generateSecureRandomBytes.
 * DO NOT encrypt more than once with the same key and IV.
 *
 * The optional results of encrypt/decrypt are used to indicate success and failure. If the value is present,
 * the operation was successful. If the value is not present, the operation failed.
 *
 * For encrypt, the output is cipher text. This will always be the size of the input plus a 16-byte authentication tag.
 * The tag is opaque--just consider it part of the cipher text.
 *
 * For decrypt, the output is the original plain text. This will be the size of the cipher text minus the 16-byte
 * authentication tag.
 */
class AesEncryptor {
public:
    using Key = std::array<uint8_t, 32>;
    using Iv = std::array<uint8_t, 12>;

    // TODO: Allow re-use of keys and setting the IV.
    AesEncryptor(const Key& key, const Iv& iv);
    AesEncryptor(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv);
    AesEncryptor(const bssl::Span<uint8_t>& key, const bssl::Span<uint8_t>& iv);
    virtual ~AesEncryptor() = default;

    // Standard encrypt/decrypt for a vector of bytes.
    std::optional<std::vector<uint8_t>> encrypt(const std::vector<uint8_t>& plainText);
    std::optional<std::vector<uint8_t>> decrypt(const std::vector<uint8_t>& cipherText);

    // Protobuf-friendly encrypt/decrypt. Protobuf uses std::string for byte buffers.
    std::optional<std::vector<uint8_t>> encrypt(const std::string& plainText);
    std::optional<std::vector<uint8_t>> decrypt(const std::string& cipherText);

    // bssl::Span verions to avoid copies.
    std::optional<std::vector<uint8_t>> encrypt(const bssl::Span<uint8_t>& plainText);
    std::optional<std::vector<uint8_t>> decrypt(const bssl::Span<uint8_t>& cipherText);

    // Authentically encrypt/decrypt with additional data.
    std::optional<std::vector<uint8_t>> encrypt(const bssl::Span<uint8_t>& plainText,
                                                const bssl::Span<uint8_t>& addData);
    std::optional<std::vector<uint8_t>> decrypt(const bssl::Span<uint8_t>& cipherText,
                                                const bssl::Span<uint8_t>& addData);

private:
    std::optional<std::vector<uint8_t>> encrypt(const uint8_t* plainText, size_t len);
    std::optional<std::vector<uint8_t>> decrypt(const uint8_t* cipherText, size_t len);
    std::optional<std::vector<uint8_t>> encryptWithAd(const uint8_t* plainText,
                                                      size_t len,
                                                      const uint8_t* additionalData,
                                                      size_t adLen);
    std::optional<std::vector<uint8_t>> decryptWithAd(const uint8_t* cipherText,
                                                      size_t len,
                                                      const uint8_t* additionalData,
                                                      size_t adLen);

private:
    const EVP_AEAD* _cipher;
    Key _key;
    Iv _iv;
};

} // namespace snap::utils::crypto
