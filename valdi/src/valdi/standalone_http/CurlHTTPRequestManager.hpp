#pragma once

#include "valdi_core/HTTPRequestManager.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

/** Matches NSURLSession's timeoutIntervalForRequest default. */
constexpr int32_t kDefaultIdleTimeoutSeconds = 60;

/**
 * An HTTPRequestManager backed by libcurl, for integrations with no platform user agent of their
 * own — the standalone runtime and the CLI apps built on it.
 *
 * caBundlePath selects the trust store used to verify server certificates. When empty, the
 * CURL_CA_BUNDLE and SSL_CERT_FILE environment variables are consulted — libcurl does not read
 * either itself — and then the few distribution paths the @curl build defaults do not name. When
 * nothing here matches, libcurl falls back to the trust store compiled into it, which is what
 * covers macOS and the Debian family; set --@curl//:ca_bundle to point that elsewhere.
 *
 * idleTimeoutSeconds fails a request that goes this long without transferring anything, so that a
 * server which accepts a connection and then stalls cannot leave a caller waiting forever. It is
 * deliberately an inactivity timeout rather than an overall one: a large asset download is slow
 * but never idle, and a hard cap would cut it off. Zero disables it.
 */
Shared<snap::valdi_core::HTTPRequestManager> makeCurlHTTPRequestManager(
    const StringBox& caBundlePath = StringBox(), int32_t idleTimeoutSeconds = kDefaultIdleTimeoutSeconds);

} // namespace Valdi
