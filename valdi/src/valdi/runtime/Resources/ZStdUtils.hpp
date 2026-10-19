//
//  ZStdUtils.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/8/19.
//

#pragma once

#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Valdi {

/**
 * Layout of a ZStd input made of one data frame optionally followed by skippable frames, which is what
 * the Valdi compiler emits (the data frame, then a content-hash trailer). Inputs with more than one
 * data frame, or with bytes that are not a well-formed skippable frame after the first frame, are not
 * described by this struct.
 */
struct ZStdSingleFrameInfo {
    // Compressed size of the first (data) frame; everything after it is skippable frames.
    size_t dataFrameSize = 0;
    // Decompressed size encoded in the data frame header.
    uint64_t contentSize = 0;
    // SHA-256 of the decompressed content, if a Valdi content-hash trailer is present.
    std::optional<std::array<Byte, 32>> contentSha256;
};

class ZStdUtils {
public:
    [[nodiscard]] static Result<Ref<ByteBuffer>> decompress(const Byte* input, size_t len);
    static bool isZstdFile(const Byte* input, size_t length);

    /**
     * Inspects a ZStd input without decompressing it. Returns nullopt unless the input is exactly one
     * data frame whose header encodes the content size, followed only by skippable frames.
     */
    static std::optional<ZStdSingleFrameInfo> inspectSingleFrame(const Byte* input, size_t len);

    /**
     * Skippable frame carrying the SHA-256 of the decompressed content: 4-byte magic, 4-byte payload
     * size (36), then the 4-byte tag and the 32-byte digest. Appended by the Valdi compiler.
     */
    static constexpr uint32_t kContentHashFrameMagic = 0x184D2A50;
    static constexpr uint32_t kContentHashTag = 0x31484356; // "VCH1" little-endian
    static constexpr size_t kContentHashFrameSize = 8 + 4 + 32;
};

} // namespace Valdi
