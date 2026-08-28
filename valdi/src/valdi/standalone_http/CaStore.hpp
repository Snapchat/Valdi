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

/** Whether a store is actually on this machine: a bundle that is a file, or a path that is a directory. */
bool caStoreExists(const CaStore& store);

/**
 * Where the distributions keep their trust stores, for when the one curl compiled in is not on this
 * machine. @curl hardcodes that per OS — Debian's bundle path on every Linux — so a build on RHEL or
 * openSUSE is pointed at a file that does not exist, and BoringSSL, which is the backend on macOS
 * too, would verify against an empty store. Compare with caStoreExists before relying on it.
 */
CaStore installedCaStore();

} // namespace Valdi
