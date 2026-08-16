#pragma once

#include "valdi_core/HTTPRequestManager.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

/**
 * An HTTPRequestManager backed by libcurl, for integrations with no platform user agent of their
 * own — the standalone runtime and the CLI apps built on it.
 *
 * caBundlePath selects the trust store used to verify server certificates. When empty, common
 * system locations are probed. If none is found, TLS requests fail rather than silently skipping
 * verification.
 */
Shared<snap::valdi_core::HTTPRequestManager> makeCurlHTTPRequestManager(const StringBox& caBundlePath = StringBox());

} // namespace Valdi
