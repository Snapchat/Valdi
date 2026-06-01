//
//  ZStdUtils.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/8/19.
//

#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi/runtime/Resources/MmapBuffer.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "zstd.h"
#include <cstdio>
#include <cstdlib>
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
Result<Ref<MmapBuffer>> ZStdUtils::decompressToMmap(const Byte* input,
                                                    size_t len,
                                                    const Path& filePath,
                                                    bool* outPublishFailed) {
    auto firstFrameSize = ZSTD_findFrameCompressedSize(input, len);
    bool isSingleFrame = !ZSTD_isError(firstFrameSize) && firstFrameSize == len;

    if (!isSingleFrame) {
        return Error("decompressToMmap requires a single ZStd frame");
    }

    auto contentSize = ZSTD_getFrameContentSize(input, len);
    if (contentSize == ZSTD_CONTENTSIZE_UNKNOWN || contentSize == ZSTD_CONTENTSIZE_ERROR) {
        return Error("ZStd frame does not encode the decompressed content size");
    }

    if (contentSize > kMaxSinglePassDecompressSize) {
        return Error(
            STRING_FORMAT("Decompressed size {} exceeds maximum {}", contentSize, kMaxSinglePassDecompressSize));
    }

    // mkstemp atomically creates a uniquely-named file and returns an open fd.
    // We deliberately place the tmp file in the SAME directory as the final
    // target rather than under std::filesystem::temp_directory_path():
    //   1. On Android, temp_directory_path() throws filesystem_error when
    //      TMPDIR is unset and /tmp doesn't exist.
    //   2. Even when it returns a valid path, it's typically on a different
    //      mount point than the app's cache dir, which makes std::rename
    //      fail with EXDEV and silently drop the file.
    // Placing the tmp next to the target guarantees same-filesystem atomic
    // rename. The parent directory is already ensured to exist by
    // ValdiModuleArchive::decompress before we get here.
    std::string tmpPathStr = filePath.toString() + ".XXXXXX";
    int tmpFd = mkstemp(tmpPathStr.data());
    if (tmpFd < 0) {
        return Error(STRING_FORMAT("mkstemp failed for template '{}': {}", tmpPathStr, strerror(errno)));
    }

    struct TmpFileGuard {
        std::string path;
        bool dismissed = false;
        ~TmpFileGuard() {
            if (!dismissed) {
                unlink(path.c_str());
            }
        }
    } tmpGuard{tmpPathStr};

    // createWritable(fd, ...) takes ownership of tmpFd and closes it on every path.
    auto bufferResult = MmapBuffer::createWritable(tmpFd, static_cast<size_t>(contentSize), tmpPathStr);
    if (!bufferResult) {
        return bufferResult.error().rethrow("Failed to create mmap buffer");
    }

    auto buffer = bufferResult.moveValue();

    auto result = ZSTD_decompress(const_cast<Byte*>(buffer->data()), buffer->size(), input, len);
    if (ZSTD_isError(result) != 0) {
        return Error(STRING_FORMAT("Could not decompress to mmap: {}", ZSTD_getErrorName(result)));
    }

    auto readOnlyResult = buffer->makeReadOnly();
    if (!readOnlyResult) {
        return readOnlyResult.error().rethrow("Failed to make mmap read-only");
    }

    if (std::rename(tmpPathStr.c_str(), filePath.toString().c_str()) == 0) {
        tmpGuard.dismissed = true;
    } else {
        // The mapping (and its data) survive via the unlinked inode that
        // TmpFileGuard is about to drop; the in-session buffer is fully
        // usable. Only the on-disk publish to the cache dir failed, so we
        // surface that for telemetry rather than discarding the buffer.
        if (outPublishFailed != nullptr) {
            *outPublishFailed = true;
        }
    }

    return buffer;
}

} // namespace Valdi
