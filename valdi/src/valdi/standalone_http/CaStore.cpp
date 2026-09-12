#include "valdi/standalone_http/CaStore.hpp"

#include <cstdlib>
#include <initializer_list>
#include <sys/stat.h>

namespace Valdi {

namespace {

std::string firstSetVariable(std::initializer_list<const char*> variables) {
    for (const char* variable : variables) {
        const char* value = std::getenv(variable);
        if (value != nullptr && *value != '\0') {
            return value;
        }
    }
    return {};
}

bool isKind(const char* candidate, mode_t kind) {
    struct stat info;
    return stat(candidate, &info) == 0 && (info.st_mode & S_IFMT) == kind;
}

std::string firstMatching(std::initializer_list<const char*> candidates, mode_t kind) {
    for (const char* candidate : candidates) {
        if (isKind(candidate, kind)) {
            return candidate;
        }
    }
    return {};
}

} // namespace

CaStore requestedCaStore(const StringBox& configured) {
    if (!configured.isEmpty()) {
        return {std::string(configured.toStringView()), {}};
    }

    // Both, not the first that answers: curl takes a CAINFO and a CAPATH together.
    return {firstSetVariable({"CURL_CA_BUNDLE", "SSL_CERT_FILE"}),
            firstSetVariable({"CURL_CA_PATH", "SSL_CERT_DIR"})};
}

bool caStoreExists(const CaStore& store) {
    return (!store.file.empty() && isKind(store.file.c_str(), S_IFREG)) ||
           (!store.path.empty() && isKind(store.path.c_str(), S_IFDIR));
}

CaStore installedCaStore() {
    return {firstMatching(
                {
                    "/etc/ssl/certs/ca-certificates.crt", // Debian, Ubuntu, Alpine, Gentoo
                    "/etc/pki/tls/certs/ca-bundle.crt",   // RHEL, Fedora, CentOS
                    "/etc/ssl/ca-bundle.pem",             // openSUSE
                    "/etc/ssl/cert.pem",                  // Alpine, FreeBSD, macOS
                },
                S_IFREG),
            firstMatching(
                {
                    "/etc/ssl/certs",
                    "/etc/pki/tls/certs",
                },
                S_IFDIR)};
}

} // namespace Valdi
