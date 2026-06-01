//
//  ValdiModuleArchive.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/6/19.
//

#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi/runtime/Resources/MmapBuffer.hpp"
#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"

#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Parser.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>

namespace Valdi {

ValdiModuleArchive::ValdiModuleArchive() = default;

ValdiModuleArchive::ValdiModuleArchive(BytesView decompressedContent,
                                       FlatMap<StringBox, ValdiModuleArchiveEntry> entries,
                                       std::vector<StringBox> orderedEntryPaths)
    : _decompressedContent(std::move(decompressedContent)),
      _entries(std::move(entries)),
      _orderedEntryPaths(std::move(orderedEntryPaths)) {}

ValdiModuleArchive::~ValdiModuleArchive() = default;

bool ValdiModuleArchive::containsEntry(const Valdi::StringBox& path) const {
    return _entries.find(path) != _entries.end();
}

std::optional<ValdiModuleArchiveEntry> ValdiModuleArchive::getEntry(const Valdi::StringBox& path) const {
    const auto& it = _entries.find(path);
    if (it == _entries.end()) {
        return std::nullopt;
    }

    return {it->second};
}

const std::vector<StringBox>& ValdiModuleArchive::getAllEntryPaths() const {
    return _orderedEntryPaths;
}

ValdiModuleArchiveEntry ValdiModuleArchive::getEntryForIndex(size_t index) const {
    SC_ASSERT(index < _orderedEntryPaths.size());
    auto entry = getEntry(_orderedEntryPaths[index]);
    SC_ASSERT(entry);
    return entry.value();
}

bool ValdiModuleArchive::operator==(const ValdiModuleArchive& other) const {
    return _decompressedContent == other._decompressedContent;
}

bool ValdiModuleArchive::operator!=(const ValdiModuleArchive& other) const {
    return !(*this == other);
}

const BytesView& ValdiModuleArchive::getDecompressedContent() const {
    return _decompressedContent;
}

Result<ValdiModuleArchive> ValdiModuleArchive::decompress(const Byte* data, size_t len) {
    if (ZStdUtils::isZstdFile(data, len)) {
        auto decompressed = ZStdUtils::decompress(data, len);
        if (!decompressed) {
            return decompressed.moveError();
        }

        return ValdiModuleArchive::deserialize(decompressed.value()->toBytesView());
    } else {
        return ValdiModuleArchive::deserialize(BytesView(nullptr, data, len));
    }
}

// Future optimization: reuse cached mmap files across launches.
// The compiler can append a ZStd skippable frame (magic 0x184D2A50, 8-byte header + 32-byte
// SHA-256) to the end of each .valdimodule file. The runtime reads the last 40 bytes to extract
// the hash, uses it as the cache filename, and opens the existing file via MmapBuffer::openReadOnly
// instead of decompressing. Skippable frames are backward-compatible: old clients decompress
// normally (streaming path handles them), new clients get a free staleness check.
Result<ValdiModuleArchive> ValdiModuleArchive::decompress(
    const Byte* data, size_t len, const Path& mmapFilePath, bool* outUsedMmap, bool* outMmapPublishFailed) {
    if (!ZStdUtils::isZstdFile(data, len)) {
        // Plain (uncompressed) archives don't touch either decompress backend.
        // Treat as "heap" for the purposes of the mmap A/B since no mmap region
        // is created.
        if (outUsedMmap != nullptr) {
            *outUsedMmap = false;
        }
        return ValdiModuleArchive::deserialize(BytesView(nullptr, data, len));
    }

    auto parentDir = mmapFilePath.removingLastComponent();
    DiskUtils::makeDirectory(parentDir, true);

    auto mmapResult = ZStdUtils::decompressToMmap(data, len, mmapFilePath, outMmapPublishFailed);
    if (mmapResult) {
        if (outUsedMmap != nullptr) {
            *outUsedMmap = true;
        }
        return ValdiModuleArchive::deserialize(mmapResult.value()->toBytesView());
    }

    auto heapResult = ZStdUtils::decompress(data, len);
    if (!heapResult) {
        return heapResult.moveError();
    }

    if (outUsedMmap != nullptr) {
        *outUsedMmap = false;
    }
    return ValdiModuleArchive::deserialize(heapResult.value()->toBytesView());
}

Result<ValdiModuleArchive> ValdiModuleArchive::deserialize(BytesView decompressedContent) {
    auto module = ValdiArchive(decompressedContent.data(), decompressedContent.data() + decompressedContent.size());

    FlatMap<StringBox, ValdiModuleArchiveEntry> entries;
    std::vector<StringBox> orderedEntryPaths;

    auto allEntries = module.getEntries();
    if (!allEntries) {
        return allEntries.moveError();
    }

    for (const auto& moduleEntry : allEntries.value()) {
        entries[moduleEntry.filePath] =
            (ValdiModuleArchiveEntry){.data = moduleEntry.data, .size = moduleEntry.dataLength};
        orderedEntryPaths.emplace_back(moduleEntry.filePath);
    }

    return ValdiModuleArchive(std::move(decompressedContent), std::move(entries), std::move(orderedEntryPaths));
}

} // namespace Valdi
