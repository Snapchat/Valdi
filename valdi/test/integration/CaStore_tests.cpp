#include "valdi/standalone_http/CaStore.hpp"

#include "valdi_core/cpp/Utils/StringBox.hpp"

#include "gtest/gtest.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

// Every variable the resolution consults, cleared around each case so that whatever the developer
// or the CI image happens to have set cannot decide what these assert.
const char* const kVariables[] = {
    "CURL_CA_BUNDLE",
    "SSL_CERT_FILE",
    "CURL_CA_PATH",
    "SSL_CERT_DIR",
};

class CaStoreFixture : public ::testing::Test {
protected:
    void SetUp() override {
        for (const char* variable : kVariables) {
            const char* value = std::getenv(variable);
            _saved.emplace_back(variable,
                                value != nullptr ? std::optional<std::string>(value) : std::nullopt);
            ::unsetenv(variable);
        }
    }

    void TearDown() override {
        for (const auto& entry : _saved) {
            if (entry.second) {
                ::setenv(entry.first, entry.second->c_str(), 1);
            } else {
                ::unsetenv(entry.first);
            }
        }
    }

private:
    std::vector<std::pair<const char*, std::optional<std::string>>> _saved;
};

} // namespace

TEST_F(CaStoreFixture, prefersTheConfiguredPathOverTheEnvironment) {
    ::setenv("SSL_CERT_FILE", "/from/the/environment.pem", 1);

    auto store = requestedCaStore(StringBox::fromCString("/from/the/caller.pem"));
    EXPECT_EQ(store.file, "/from/the/caller.pem");
    EXPECT_EQ(store.path, "");
}

TEST_F(CaStoreFixture, readsTheCurlCommandLineToolsBundleVariable) {
    ::setenv("CURL_CA_BUNDLE", "/curl/bundle.pem", 1);

    auto store = requestedCaStore(StringBox());
    EXPECT_EQ(store.file, "/curl/bundle.pem") << "libcurl does not read this itself, so a caller "
                                                 "pointing at their own trust store is ignored "
                                                 "without it";
}

TEST_F(CaStoreFixture, readsOpenSslsCertificateFileVariable) {
    ::setenv("SSL_CERT_FILE", "/openssl/bundle.pem", 1);

    auto store = requestedCaStore(StringBox());
    EXPECT_EQ(store.file, "/openssl/bundle.pem");
}

TEST_F(CaStoreFixture, prefersCurlsBundleVariableOverOpenSsls) {
    ::setenv("CURL_CA_BUNDLE", "/curl/bundle.pem", 1);
    ::setenv("SSL_CERT_FILE", "/openssl/bundle.pem", 1);

    auto store = requestedCaStore(StringBox());
    EXPECT_EQ(store.file, "/curl/bundle.pem");
}

// The trust store on Alpine and on any image that points at a hashed directory rather than a single
// bundle. There is no file to hand CURLOPT_CAINFO, so a resolution that only ever produces one
// leaves those machines with an empty store and every HTTPS request failing.
TEST_F(CaStoreFixture, readsOpenSslsCertificateDirectoryVariable) {
    ::setenv("SSL_CERT_DIR", "/etc/ssl/certs", 1);

    auto store = requestedCaStore(StringBox());
    EXPECT_EQ(store.path, "/etc/ssl/certs")
        << "SSL_CERT_DIR went unread, so a caller whose trust store is a hashed directory has no "
           "way to say so and every certificate is rejected";
    EXPECT_EQ(store.file, "");
}

TEST_F(CaStoreFixture, readsTheCurlCommandLineToolsPathVariable) {
    ::setenv("CURL_CA_PATH", "/curl/certs", 1);

    auto store = requestedCaStore(StringBox());
    EXPECT_EQ(store.path, "/curl/certs");
}

TEST_F(CaStoreFixture, reportsBothAFileAndADirectoryWhenBothAreSet) {
    ::setenv("SSL_CERT_FILE", "/openssl/bundle.pem", 1);
    ::setenv("SSL_CERT_DIR", "/openssl/certs", 1);

    auto store = requestedCaStore(StringBox());
    EXPECT_EQ(store.file, "/openssl/bundle.pem");
    EXPECT_EQ(store.path, "/openssl/certs") << "curl takes a CAINFO and a CAPATH together, and the "
                                               "tool these variables come from honours both";
}

TEST_F(CaStoreFixture, findsNothingWhenNothingIsAskedFor) {
    auto store = requestedCaStore(StringBox());
    EXPECT_TRUE(store.empty());
}

// @curl compiles in Debian's bundle path on every Linux, so on RHEL, Fedora or openSUSE what this
// finds is the only thing standing between BoringSSL and rejecting every certificate. All four are
// listed because no one of them serves the others.
TEST_F(CaStoreFixture, namesTheTrustStoreOfTheCommonDistributions) {
    // Not a filesystem probe: this asserts the list, since the machine running the test has at most
    // one of these and the point is that all of them are covered.
    for (const char* expected : {
             "/etc/ssl/certs/ca-certificates.crt", // Debian, Ubuntu, Alpine, Gentoo
             "/etc/pki/tls/certs/ca-bundle.crt",   // RHEL, Fedora, CentOS
             "/etc/ssl/ca-bundle.pem",             // openSUSE
             "/etc/ssl/cert.pem",                  // Alpine, FreeBSD
         }) {
        struct stat info;
        if (stat(expected, &info) != 0 || !S_ISREG(info.st_mode)) {
            continue;
        }
        EXPECT_EQ(installedCaStore().file, expected)
            << expected << " exists on this machine but is not the bundle that was resolved, so a "
            << "build on this distribution verifies against nothing";
        return;
    }

    GTEST_SKIP() << "no distribution bundle on this machine to resolve";
}

// The reason the resolution checks existence rather than trusting what curl reports. A build on RHEL
// or Fedora carries Debian's bundle path, and taking that as a trust store skips the probe that would
// have found the one this machine actually has.
TEST_F(CaStoreFixture, doesNotCountABundleThatIsNotThere) {
    EXPECT_FALSE(caStoreExists({"/no/such/bundle.pem", ""}));
    EXPECT_FALSE(caStoreExists({"", "/no/such/certs"}));
    EXPECT_FALSE(caStoreExists({}));
}

TEST_F(CaStoreFixture, countsAStoreThatIsThere) {
    EXPECT_TRUE(caStoreExists({"", "/etc"})) << "a directory that exists was not taken as a CAPATH";

    // Whichever of the two kinds this machine has; both branches matter and neither is guaranteed.
    auto installed = installedCaStore();
    if (installed.empty()) {
        GTEST_SKIP() << "no distribution trust store on this machine to check against";
    }
    EXPECT_TRUE(caStoreExists(installed));
}

// A bundle is a file and a path is a directory, so the kinds are not interchangeable: handing curl a
// directory as CAINFO fails to load, and a file as CAPATH is ignored.
TEST_F(CaStoreFixture, doesNotAcceptADirectoryAsABundle) {
    EXPECT_FALSE(caStoreExists({"/etc", ""}));
}

} // namespace ValdiTest
