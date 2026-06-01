//
//  ValdiModuleArchive.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/6/19.
//

#pragma once

#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <vector>

namespace Valdi {

struct ValdiModuleArchiveEntry {
    const Byte* data;
    size_t size;
};

class ValdiModuleArchive : public SharedPtrRefCountable {
public:
    ValdiModuleArchive();
    ~ValdiModuleArchive() override;

    std::optional<ValdiModuleArchiveEntry> getEntry(const Valdi::StringBox& path) const;
    ValdiModuleArchiveEntry getEntryForIndex(size_t index) const;

    bool containsEntry(const Valdi::StringBox& path) const;

    const std::vector<StringBox>& getAllEntryPaths() const;

    const BytesView& getDecompressedContent() const;

    [[nodiscard]] static Result<ValdiModuleArchive> decompress(const Byte* data, size_t len);

    /**
     * Decompress using mmap-backed storage when possible.
     * Falls back to heap allocation if the mmap path fails (e.g. legacy modules
     * without encoded content size).
     *
     * If outUsedMmap is non-null, it is set to true iff the returned archive is
     * backed by an MmapBuffer and false iff the call fell back to a heap
     * ByteBuffer. Useful for A/B telemetry. Untouched on error.
     *
     * If outMmapPublishFailed is non-null, it is set to true iff the mmap path
     * succeeded in-memory but failed to publish the cache file to disk (the
     * returned archive is still valid). Untouched on the heap path or on error.
     */
    [[nodiscard]] static Result<ValdiModuleArchive> decompress(const Byte* data,
                                                               size_t len,
                                                               const Path& mmapFilePath,
                                                               bool* outUsedMmap = nullptr,
                                                               bool* outMmapPublishFailed = nullptr);

    [[nodiscard]] static Result<ValdiModuleArchive> deserialize(BytesView decompressedContent);

    bool operator==(const ValdiModuleArchive& other) const;
    bool operator!=(const ValdiModuleArchive& other) const;

private:
    BytesView _decompressedContent;
    FlatMap<StringBox, ValdiModuleArchiveEntry> _entries;
    std::vector<StringBox> _orderedEntryPaths;

    ValdiModuleArchive(BytesView decompressedContent,
                       FlatMap<StringBox, ValdiModuleArchiveEntry> entries,
                       std::vector<StringBox> orderedEntryPaths);
};

} // namespace Valdi
