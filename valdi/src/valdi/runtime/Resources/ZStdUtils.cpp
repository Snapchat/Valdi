//
//  ZStdUtils.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/8/19.
//

#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "zstd.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <unistd.h>

namespace Valdi {

static constexpr size_t kMaxSinglePassDecompressSize = 128 * 1024 * 1024;

bool ZStdUtils::isZstdFile(const Byte* input, size_t length) {
    if (length < 4) {
        return false;
    }
    uint32_t output;
    std::memcpy(&output, input, sizeof(uint32_t));
    return output == ZSTD_MAGICNUMBER;
}

static uint32_t readLittleEndian32(const Byte* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

std::optional<ZStdSingleFrameInfo> ZStdUtils::inspectSingleFrame(const Byte* input, size_t len) {
    if (!isZstdFile(input, len)) {
        return std::nullopt;
    }

    auto dataFrameSize = ZSTD_findFrameCompressedSize(input, len);
    if (ZSTD_isError(dataFrameSize) != 0 || dataFrameSize > len) {
        return std::nullopt;
    }

    auto contentSize = ZSTD_getFrameContentSize(input, dataFrameSize);
    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN || contentSize == ZSTD_CONTENTSIZE_ERROR) {
        return std::nullopt;
    }

    ZStdSingleFrameInfo info;
    info.dataFrameSize = dataFrameSize;
    info.contentSize = contentSize;

    constexpr size_t kSkippableHeaderSize = 8;
    size_t offset = dataFrameSize;
    while (offset < len) {
        if (len - offset < kSkippableHeaderSize) {
            return std::nullopt;
        }
        auto magic = readLittleEndian32(input + offset);
        if ((magic & ZSTD_MAGIC_SKIPPABLE_MASK) != ZSTD_MAGIC_SKIPPABLE_START) {
            return std::nullopt;
        }
        auto payloadSize = static_cast<size_t>(readLittleEndian32(input + offset + 4));
        if (payloadSize > len - offset - kSkippableHeaderSize) {
            return std::nullopt;
        }

        const auto* payload = input + offset + kSkippableHeaderSize;
        if (magic == kContentHashFrameMagic && payloadSize == kContentHashFrameSize - kSkippableHeaderSize &&
            readLittleEndian32(payload) == kContentHashTag) {
            std::array<Byte, 32> digest{};
            std::memcpy(digest.data(), payload + 4, digest.size());
            info.contentSha256 = digest;
        }

        offset += kSkippableHeaderSize + payloadSize;
    }

    return info;
}

Result<Ref<ByteBuffer>> ZStdUtils::decompress(const Byte* input, size_t len) {
    auto frameInfo = inspectSingleFrame(input, len);
    if (frameInfo && frameInfo->contentSize <= kMaxSinglePassDecompressSize) {
        auto output = makeShared<ByteBuffer>();
        output->resize(static_cast<size_t>(frameInfo->contentSize));

        auto result = ZSTD_decompress(output->data(), output->size(), input, frameInfo->dataFrameSize);
        if (ZSTD_isError(result) != 0) {
            return Error(STRING_FORMAT("Could not decompress: {}", ZSTD_getErrorName(result)));
        }

        return output;
    }

    auto* dstream = ZSTD_createDStream();
    if (dstream == nullptr) {
        return Error("Could not create ZSTD stream");
    }

    auto initResult = ZSTD_initDStream(dstream);
    if (ZSTD_isError(initResult) != 0) {
        ZSTD_freeDStream(dstream);
        return Error(STRING_FORMAT("Could not initialize stream: {}", ZSTD_getErrorName(initResult)));
    }

    auto bufferSize = ZSTD_DStreamOutSize();
    ByteBuffer buffer;
    buffer.resize(bufferSize);

    ZSTD_inBuffer inBuffer;
    inBuffer.src = input;
    inBuffer.size = len;
    inBuffer.pos = 0;

    ZSTD_outBuffer outBuffer;
    outBuffer.dst = buffer.data();
    outBuffer.pos = 0;
    outBuffer.size = bufferSize;

    auto output = makeShared<ByteBuffer>();

    while (inBuffer.pos < len) {
        auto result = ZSTD_decompressStream(dstream, &outBuffer, &inBuffer);
        if (ZSTD_isError(result) != 0) {
            ZSTD_freeDStream(dstream);
            return Error(STRING_FORMAT("Could not decompress stream: {}", ZSTD_getErrorName(result)));
        }

        output->append(buffer.begin(), buffer.begin() + outBuffer.pos);
        outBuffer.pos = 0;
    }

    ZSTD_freeDStream(dstream);
    output->shrinkToFit();

    return output;
}
} // namespace Valdi
