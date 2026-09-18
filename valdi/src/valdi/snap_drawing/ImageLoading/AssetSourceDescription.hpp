//
//  AssetSourceDescription.hpp
//  valdi-desktop-apple
//

#pragma once

#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <string>

namespace snap::drawing {

/**
 Describes an asset url for a decode-error message using only its host and file extension. A media
 url's path and query can carry media ids and decryption keys, which must not reach an error string
 that ships to the crash reporting pipeline. A `composer-encrypted-*` wrapper url is described by
 its inner `url=` target, so the reported host is the CDN actually serving the bytes rather than the
 wrapper scheme. Declared here so the sanitizing is directly testable, and shared by every asset
 loader that reports a decode failure.

 The output is `host=<host> ext=<ext>`, plus ` wrapper=<scheme>` only for a wrapper url. Report
 tooling parses those keys alongside the payload group from `describePayloadBytes`, so treat them as
 a contract: do not rename them or add caps without updating that tooling.
 */
std::string describeAssetSource(const Valdi::StringBox& url);

} // namespace snap::drawing
