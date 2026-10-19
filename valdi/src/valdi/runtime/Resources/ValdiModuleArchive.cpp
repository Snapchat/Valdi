//
//  ValdiModuleArchive.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 3/6/19.
//

#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"
#include "valdi_core/cpp/Resources/ValdiPacket.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"
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

Result<ValdiModuleArchive> ValdiModuleArchive::fromDirectory(
    BytesView decompressedContent, const std::vector<std::tuple<StringBox, size_t, size_t>>& directory) {
    // Only the 8-byte packet header is read; the entries' own headers are never touched.
    auto dataSection = ValdiPacket::read(decompressedContent.data(), decompressedContent.size());
    if (!dataSection) {
        return dataSection.moveError();
    }

    FlatMap<StringBox, ValdiModuleArchiveEntry> entries;
    std::vector<StringBox> orderedEntryPaths;
    orderedEntryPaths.reserve(directory.size());
    for (const auto& [path, offset, size] : directory) {
        if (offset > decompressedContent.size() || size > decompressedContent.size() - offset) {
            return Error(STRING_FORMAT("Archive entry '{}' is out of bounds", path));
        }
        entries[path] = (ValdiModuleArchiveEntry){.data = decompressedContent.data() + offset, .size = size};
        orderedEntryPaths.emplace_back(path);
    }

    return ValdiModuleArchive(std::move(decompressedContent), std::move(entries), std::move(orderedEntryPaths));
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
