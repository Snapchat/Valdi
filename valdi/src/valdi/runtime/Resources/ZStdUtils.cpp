//
//  ZStdUtils.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/8/19.
//

#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "zstd.h"
#include <fmt/format.h>
#include <fmt/ostream.h>

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

Result<Ref<ByteBuffer>> ZStdUtils::decompress(const Byte* input, size_t len) {
    auto firstFrameSize = ZSTD_findFrameCompressedSize(input, len);
    bool isSingleFrame = !ZSTD_isError(firstFrameSize) && firstFrameSize == len;

    if (isSingleFrame) {
        auto contentSize = ZSTD_getFrameContentSize(input, len);
        if (contentSize != ZSTD_CONTENTSIZE_UNKNOWN && contentSize != ZSTD_CONTENTSIZE_ERROR &&
            contentSize <= kMaxSinglePassDecompressSize) {
            auto output = makeShared<ByteBuffer>();
            output->resize(static_cast<size_t>(contentSize));

            auto result = ZSTD_decompress(output->data(), output->size(), input, len);
            if (ZSTD_isError(result) != 0) {
                return Error(STRING_FORMAT("Could not decompress: {}", ZSTD_getErrorName(result)));
            }

            return output;
        }
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
