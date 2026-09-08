//
//  BytesUtils.hpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/20/22.
//

#pragma once

#include "include/core/SkData.h"
#include "valdi_core/cpp/Utils/Bytes.hpp"

#include <string>
#include <string_view>

namespace snap::drawing {

enum DataConversionMode {
    DataConversionModeCopyIfUnsafe,
    DataConversionModeAlwaysCopy,
    DataConversionModeNeverCopy,
};

Valdi::BytesView bytesFromSkData(const sk_sp<SkData>& data);
sk_sp<SkData> skDataFromBytes(const Valdi::BytesView& bytes, DataConversionMode conversionMode);

/// Formats a payload's length and leading bytes as `bytes=<n> magic=<hex>`.
///
/// This string is a parsing contract, not a debug aid: report tooling for the `SNAP_RENDERER_FAILURE`
/// non-fatal groups matches on these keys to tell an unsupported container apart from a truncated,
/// substituted, or still-encoded body. The magic run is lowercase hex capped at 12 bytes, which is
/// what covers the ISO-BMFF brand at offset 4 to 11. Do not change the cap, the casing, or the key
/// names without updating that tooling and every call site that composes this string.
std::string describePayloadBytes(const Valdi::BytesView& bytes);

/// Builds the error message for a payload that no image codec accepted.
///
/// Names the payload kind when its leading bytes identify something that is never a valid image (an
/// HTML document, a raw HTTP response, a JSON object), and uses `fallbackLabel` otherwise. Always
/// ends with the payload length and its leading bytes, hex encoded, so a report can distinguish an
/// unsupported format from a truncated, substituted, or still-encoded body.
std::string describeUndecodablePayload(const Valdi::BytesView& bytes, std::string_view fallbackLabel);

} // namespace snap::drawing
