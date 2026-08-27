#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <string>

namespace Valdi {

/** A bundle file for CURLOPT_CAINFO and a hashed directory for CURLOPT_CAPATH. */
struct CaStore {
    std::string file;
    std::string path;

    bool empty() const {
        return file.empty() && path.empty();
    }
};

/**
 * The configured path, or else CURL_CA_BUNDLE, SSL_CERT_FILE, CURL_CA_PATH and SSL_CERT_DIR.
 * libcurl reads none of those itself.
 */
CaStore requestedCaStore(const StringBox& configured);

/**
 * Where the distributions keep their trust stores. Load bearing on Linux: @curl compiles in no CA
 * bundle unless --@curl//:ca_bundle says so and is built without CURL_CA_FALLBACK, so BoringSSL
 * would otherwise verify against an empty store. macOS gets SecureTransport and the keychain.
 */
CaStore installedCaStore();

} // namespace Valdi
