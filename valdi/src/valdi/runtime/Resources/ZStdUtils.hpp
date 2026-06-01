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
#include <vector>

namespace Valdi {

class MmapBuffer;

class ZStdUtils {
public:
    [[nodiscard]] static Result<Ref<ByteBuffer>> decompress(const Byte* input, size_t len);
    static bool isZstdFile(const Byte* input, size_t length);

    /**
     * Decompress directly into a file-backed mmap region.
     * Requires that the ZStd frame encodes the content size.
     * Returns an error if the content size is unknown or decompression fails.
     *
     * On success the returned MmapBuffer is always valid for in-session use,
     * even if writing the final cache file failed. If outPublishFailed is
     * non-null it is set to true iff the tmp->final rename failed; in that
     * case the buffer still works (POSIX keeps the inode alive while mapped)
     * but no on-disk cache file is published.
     */
    [[nodiscard]] static Result<Ref<MmapBuffer>> decompressToMmap(const Byte* input,
                                                                  size_t len,
                                                                  const Path& filePath,
                                                                  bool* outPublishFailed = nullptr);
};

} // namespace Valdi
