#include "utils/crypto/AesEncryptor.hpp"

#include "utils/crypto/CryptoHelpers.hpp"
#include "utils/debugging/Assert.hpp"
#include "utils/logging/Log.hpp"

#include <openssl/aead.h>
#include <openssl/span.h>

namespace snap::utils::crypto {

constexpr auto kTag = "[utils][AesEncryptor]";

AesEncryptor::AesEncryptor(const AesEncryptor::Key& key, const AesEncryptor::Iv& iv)
    : _cipher(EVP_aead_aes_256_gcm()), _key(key), _iv(iv) {
    SC_ASSERT(_key.size() == EVP_AEAD_key_length(_cipher));
    SC_ASSERT(_iv.size() == EVP_AEAD_nonce_length(_cipher));
}

AesEncryptor::AesEncryptor(const bssl::Span<uint8_t>& key, const bssl::Span<uint8_t>& iv)
    : _cipher(EVP_aead_aes_256_gcm()) {
    SC_ASSERT(_key.size() == EVP_AEAD_key_length(_cipher));
    SC_ASSERT(_iv.size() == EVP_AEAD_nonce_length(_cipher));
    std::copy(key.begin(), key.end(), _key.begin());
    std::copy(iv.begin(), iv.end(), _iv.begin());
}

AesEncryptor::AesEncryptor(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv)
    : _cipher(EVP_aead_aes_256_gcm()) {
    SC_ASSERT(_key.size() == EVP_AEAD_key_length(_cipher));
    SC_ASSERT(_iv.size() == EVP_AEAD_nonce_length(_cipher));
    std::copy(key.begin(), key.end(), _key.begin());
    std::copy(iv.begin(), iv.end(), _iv.begin());
}

std::optional<std::vector<uint8_t>> AesEncryptor::encrypt(const uint8_t* plainText, size_t len) {
    return encryptWithAd(plainText, len, nullptr, 0);
}

std::optional<std::vector<uint8_t>> AesEncryptor::decrypt(const uint8_t* cipherText, size_t len) {
    return decryptWithAd(cipherText, len, nullptr, 0);
}

std::optional<std::vector<uint8_t>> AesEncryptor::encryptWithAd(const uint8_t* plainText,
                                                                size_t len,
                                                                const uint8_t* additionalData,
                                                                size_t adLen) {
    SC_ASSERT_NOTNULL(plainText);
    SC_ASSERT(len > 0);
    EVP_AEAD_CTX ctx;
    if (EVP_AEAD_CTX_init(&ctx, _cipher, _key.data(), _key.size(), EVP_AEAD_max_tag_len(_cipher), nullptr) == 0) {
        SCLOGE(kTag, "Failed to initialize encryption key");
        logCryptoErrors();
        SC_ASSERT(false, "Bad key");
        return {};
    }
    std::vector<uint8_t> cipherText(len + EVP_AEAD_max_overhead(_cipher), 0);
    size_t outLen;
    if (EVP_AEAD_CTX_seal(&ctx,
                          cipherText.data(),
                          &outLen,
                          cipherText.capacity(),
                          _iv.data(),
                          _iv.size(),
                          plainText,
                          len,
                          additionalData,
                          adLen) == 0) {
        SCLOGE(kTag, "Failed encryption");
        logCryptoErrors();
        return {};
    }
    SC_ASSERT(outLen == cipherText.size());
    EVP_AEAD_CTX_cleanup(&ctx);
    return {cipherText};
}

std::optional<std::vector<uint8_t>> AesEncryptor::decryptWithAd(const uint8_t* cipherText,
                                                                size_t len,
                                                                const uint8_t* additionalData,
                                                                size_t adLen) {
    SC_ASSERT_NOTNULL(cipherText);
    EVP_AEAD_CTX ctx;
    if (EVP_AEAD_CTX_init(&ctx, _cipher, _key.data(), _key.size(), EVP_AEAD_max_tag_len(_cipher), nullptr) == 0) {
        SCLOGE(kTag, "Failed to initialize decryption key");
        logCryptoErrors();
        SC_ASSERT(false, "Bad key");
        return std::nullopt;
    }
    const auto maxOverheadLen = EVP_AEAD_max_overhead(_cipher);
    if (len < maxOverheadLen) {
        SCLOGW(kTag, "Invalid cipherText length {}. Min expected: {}", len, maxOverheadLen);
        return std::nullopt;
    }
    std::vector<uint8_t> plainText(len - maxOverheadLen, 0);
    size_t outLen;
    if (EVP_AEAD_CTX_open(&ctx,
                          plainText.data(),
                          &outLen,
                          plainText.capacity(),
                          _iv.data(),
                          _iv.size(),
                          cipherText,
                          len,
                          additionalData,
                          adLen) == 0) {
        SCLOGE(kTag, "Failed decryption");
        logCryptoErrors();
        return {};
    }
    SC_ASSERT(outLen == plainText.size());
    EVP_AEAD_CTX_cleanup(&ctx);
    return {plainText};
}

std::optional<std::vector<uint8_t>> AesEncryptor::encrypt(const std::vector<uint8_t>& plainText) {
    return encrypt(plainText.data(), plainText.size());
}

std::optional<std::vector<uint8_t>> AesEncryptor::encrypt(const std::string& plainText) {
    return encrypt(reinterpret_cast<const uint8_t*>(plainText.data()), plainText.size());
}

std::optional<std::vector<uint8_t>> AesEncryptor::decrypt(const std::vector<uint8_t>& cipherText) {
    return decrypt(cipherText.data(), cipherText.size());
}

std::optional<std::vector<uint8_t>> AesEncryptor::decrypt(const std::string& cipherText) {
    return decrypt(reinterpret_cast<const uint8_t*>(cipherText.data()), cipherText.size());
}

std::optional<std::vector<uint8_t>> AesEncryptor::encrypt(const bssl::Span<uint8_t>& plainText) {
    return encrypt(plainText.data(), plainText.size());
}
std::optional<std::vector<uint8_t>> AesEncryptor::decrypt(const bssl::Span<uint8_t>& cipherText) {
    return decrypt(cipherText.data(), cipherText.size());
}

std::optional<std::vector<uint8_t>> AesEncryptor::encrypt(const bssl::Span<uint8_t>& plainText,
                                                          const bssl::Span<uint8_t>& addData) {
    return encryptWithAd(plainText.data(), plainText.size(), addData.data(), addData.size());
}
std::optional<std::vector<uint8_t>> AesEncryptor::decrypt(const bssl::Span<uint8_t>& cipherText,
                                                          const bssl::Span<uint8_t>& addData) {
    return decryptWithAd(cipherText.data(), cipherText.size(), addData.data(), addData.size());
}

} // namespace snap::utils::crypto
