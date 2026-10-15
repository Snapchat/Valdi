//
//  KeyValueStore.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 01/26/24.
//

#include "valdi/runtime/Resources/KeyValueStore.hpp"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/Parser.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <algorithm>

namespace Valdi {

struct KeyValueStoreEntryManifest {
    uint64_t mutationId;
    uint64_t expirationDate;
    uint64_t weight;
};

struct KeyValueStoreManifest {
    uint64_t version;
    uint64_t mutationIdSequence;
    KeyValueStoreEntryManifest manifest[0];
};

constexpr uint64_t kKeyValueStoreVersion = 2;

STRING_CONST(manifestEntryName, "__manifest__")

KeyValueStoreEntry::KeyValueStoreEntry() = default;
KeyValueStoreEntry::KeyValueStoreEntry(uint64_t mutationId,
                                       uint64_t expirationDate,
                                       uint64_t weight,
                                       const BytesView& data)
    : mutationId(mutationId), expirationDate(expirationDate), weight(weight), data(data) {}

KeyValueStore::KeyValueStore() = default;
KeyValueStore::~KeyValueStore() = default;

void KeyValueStore::store(const StringBox& key, const BytesView& blob, uint64_t ttlSeconds, uint64_t weight) {
    uint64_t expirationDateSeconds = 0;
    if (ttlSeconds != 0) {
        expirationDateSeconds = currentTimeSeconds() + ttlSeconds;
    }

    _entries[key] = KeyValueStoreEntry(++_mutationId, expirationDateSeconds, weight, blob);
}

std::optional<BytesView> KeyValueStore::fetch(const StringBox& key, bool updateSequence) {
    auto it = _entries.find(key);
    if (it == _entries.end()) {
        return std::nullopt;
    }

    if (isEntryExpired(it->second)) {
        _entries.erase(it);
        return std::nullopt;
    }

    if (updateSequence) {
        it->second.mutationId = ++_mutationId;
    }

    return {it->second.data};
}

std::optional<BytesView> KeyValueStore::fetch(const StringBox& key) {
    return fetch(key, true);
}

std::vector<std::pair<StringBox, KeyValueStoreEntry>> KeyValueStore::fetchAll() {
    return collectEntries();
}

bool KeyValueStore::exists(const StringBox& key) {
    return fetch(key, false).has_value();
}

bool KeyValueStore::remove(const StringBox& key) {
    const auto& it = _entries.find(key);
    if (it == _entries.end()) {
        return false;
    }

    _entries.erase(it);
    return true;
}

void KeyValueStore::removeAll() {
    _entries.clear();
}

void KeyValueStore::setMaxWeight(uint64_t maxWeight) {
    _maxWeight = maxWeight;
}

uint64_t KeyValueStore::getMaxWeight() const {
    return _maxWeight;
}

std::vector<std::pair<StringBox, KeyValueStoreEntry>> KeyValueStore::collectEntries() {
    std::vector<std::pair<StringBox, KeyValueStoreEntry>> entries;
    entries.reserve(_entries.size());

    auto it = _entries.begin();
    while (it != _entries.end()) {
        if (!isEntryExpired(it->second)) {
            entries.emplace_back(it->first, it->second);
            it++;
        } else {
            it = _entries.erase(it);
        }
    }

    // Sort once rather than inserting into the middle of the vector per entry (O(n^2));
    // this runs on every save of the store. mutationIds are unique, so no stable sort needed.
    std::sort(entries.begin(),
              entries.end(),
              [](const std::pair<StringBox, KeyValueStoreEntry>& left,
                 const std::pair<StringBox, KeyValueStoreEntry>& right) {
                  return left.second.mutationId < right.second.mutationId;
              });

    if (_maxWeight > 0) {
        evictEntriesIfNeeded(entries);
    }

    return entries;
}

void KeyValueStore::evictEntriesIfNeeded(std::vector<std::pair<StringBox, KeyValueStoreEntry>>& collectedEntries) {
    // Compute current weight
    uint64_t currentWeight = 0;
    for (const auto& it : collectedEntries) {
        currentWeight += it.second.weight;
    }

    // Keep evicting from the start until our weight gets below maxWeight
    size_t i = 0;
    while (currentWeight > _maxWeight && i < collectedEntries.size()) {
        currentWeight -= collectedEntries[i].second.weight;
        i++;
    }

    if (i > 0) {
        // Remove all evicted entries from the entries map
        for (size_t j = 0; j < i; j++) {
            const auto& it = _entries.find(collectedEntries[j].first);
            if (it != _entries.end()) {
                _entries.erase(it);
            }
        }

        // Also remove them from the collected entries
        collectedEntries.erase(collectedEntries.begin(), collectedEntries.begin() + i);
    }
}

void KeyValueStore::buildManifest(const std::vector<std::pair<StringBox, KeyValueStoreEntry>>& entries,
                                  ByteBuffer& output) const {
    KeyValueStoreManifest manifest;
    manifest.version = kKeyValueStoreVersion;
    manifest.mutationIdSequence = _mutationId;

    output.append(reinterpret_cast<const Byte*>(&manifest),
                  reinterpret_cast<const Byte*>(&manifest) + sizeof(manifest));

    for (const auto& it : entries) {
        KeyValueStoreEntryManifest entry;
        entry.mutationId = it.second.mutationId;
        entry.expirationDate = it.second.expirationDate;
        entry.weight = it.second.weight;

        output.append(reinterpret_cast<const Byte*>(&entry), reinterpret_cast<const Byte*>(&entry) + sizeof(entry));
    }
}

Result<BytesView> KeyValueStore::serialize() {
    ValdiArchiveBuilder builder;

    auto entries = collectEntries();
    ByteBuffer manifest;
    buildManifest(entries, manifest);
    builder.addEntry(ValdiArchiveEntry(manifestEntryName(), manifest.data(), manifest.size()));

    for (const auto& it : entries) {
        builder.addEntry(ValdiArchiveEntry(it.first, it.second.data.data(), it.second.data.size()));
    }

    return builder.build()->toBytesView();
}

bool KeyValueStore::isEntryExpired(const KeyValueStoreEntry& entry) const {
    if (entry.expirationDate == 0) {
        return false;
    }

    return currentTimeSeconds() >= entry.expirationDate;
}

Result<Parser<Byte>> parseManifest(const ValdiArchiveEntry& entry, uint64_t* idSequence) {
    if (entry.filePath != manifestEntryName()) {
        return Error("First entry in store should be the manifest");
    }

    auto manifestParser = Parser(entry.data, entry.data + entry.dataLength);
    auto manifestHeaderResult = manifestParser.parseStruct<KeyValueStoreManifest>();
    if (!manifestHeaderResult) {
        return manifestHeaderResult.moveError();
    }

    if (manifestHeaderResult.value()->version != kKeyValueStoreVersion) {
        return Error("Incompatible KeyValueStore version");
    }

    *idSequence = manifestHeaderResult.value()->mutationIdSequence;

    return manifestParser;
}

Result<KeyValueStoreEntry> parseEntry(Parser<Byte>& manifestParser,
                                      const ValdiArchiveEntry& entry,
                                      const Ref<RefCountable>& source) {
    auto result = manifestParser.parseStruct<KeyValueStoreEntryManifest>();
    if (!result) {
        return result.moveError();
    }

    const auto* manifestEntry = result.value();

    return KeyValueStoreEntry(manifestEntry->mutationId,
                              manifestEntry->expirationDate,
                              manifestEntry->weight,
                              BytesView(source, entry.data, entry.dataLength));
}

Result<Void> KeyValueStore::populate(const BytesView& data) {
    auto manifestParser = Parser<Byte>(nullptr, nullptr);

    uint64_t idSequence = 0;

    auto archive = ValdiArchive(data.begin(), data.end());
    auto entries = archive.getEntries();
    if (!entries) {
        return entries.error();
    }

    for (const auto& entry : entries.value()) {
        if (manifestParser.getBegin() == nullptr) {
            auto manifestResult = parseManifest(entry, &idSequence);
            if (!manifestResult) {
                return manifestResult.error();
            }

            manifestParser = manifestResult.moveValue();
        } else {
            auto entryResult = parseEntry(manifestParser, entry, data.getSource());
            if (!entryResult) {
                return entryResult.error();
            }

            if (!isEntryExpired(entryResult.value())) {
                _entries[entry.filePath] = entryResult.moveValue();
            }
        }
    }

    _mutationId = idSequence;
    return Void();
}

uint64_t KeyValueStore::getMutationId() const {
    return _mutationId;
}

uint64_t KeyValueStore::currentTimeSeconds() const {
    if (_currentTimeSeconds != 0) {
        return _currentTimeSeconds;
    }

    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void KeyValueStore::setCurrentTimeSeconds(uint64_t timeSeconds) {
    _currentTimeSeconds = timeSeconds;
}

} // namespace Valdi
