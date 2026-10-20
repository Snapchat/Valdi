//
//  ModuleArchiveCache.cpp
//  ValdiRuntime
//

#include "valdi/runtime/Resources/ModuleArchiveCache.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Resources/MmapBuffer.hpp"
#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi/runtime/Utils/BytesUtils.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
// Inlined so the link never pulls xxhash_internal's object: its XXH32/XXH64 collide with the
// unprefixed copies in prebuilt libzstd.a (looksery) and ld64.lld rejects the duplicates.
#define XXH_INLINE_ALL
#include "xxhash.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Valdi {

namespace {

constexpr std::string_view kFormatDirectory = "v3";
constexpr std::string_view kPreviousFormatDirectory = "v2";
constexpr std::string_view kDataFileName = "data";
constexpr std::string_view kIndexFileName = "index";
constexpr std::string_view kDisabledMarkerName = "disabled";
constexpr std::string_view kDisabledReportedMarkerName = "disabled_reported";
constexpr size_t kGenerationHexLength = 16;
constexpr uint64_t kEntryAlignment = 16;

constexpr uint32_t kIndexMagic = 0x49504D56; // "VMPI" little-endian
// Version 3: each archive's file list sits in the data file right before the archive, and checksums, content
// hashes and long identities use XXH3.
constexpr uint32_t kIndexVersion = 3;
constexpr size_t kIndexHeaderSize = 8;

enum class RecordType : uint8_t { Data = 1, Ban = 2 };

// type, identity kind, name length, identity length, file list length, file list offset, archive size, file
// list checksum (XXH3-64), archive content hash (XXH3-128). The name and the identity follow, then the record
// checksum (XXH3-64). In the data file the file list starts at the offset and the archive at the next
// 16-byte boundary after it.
constexpr size_t kRecordFixedSize = 1 + 1 + 2 + 2 + 4 + 8 + 8 + 8 + 16;
// path length, offset in the archive, size.
constexpr size_t kDirectoryEntryFixedSize = 2 + 4 + 4;
constexpr size_t kRecordChecksumSize = 8;
// Identities longer than this are stored as their XXH3-128.
constexpr size_t kMaxIdentityLength = 16;

uint64_t alignUp(uint64_t value) {
    return (value + kEntryAlignment - 1) & ~(kEntryAlignment - 1);
}

uint64_t checksum64(const Byte* data, size_t len) {
    return XXH3_64bits(data, len);
}

std::array<Byte, 16> hash128(const Byte* data, size_t len) {
    XXH128_canonical_t canonical;
    XXH128_canonicalFromHash(&canonical, XXH3_128bits(data, len));
    std::array<Byte, 16> out{};
    std::memcpy(out.data(), canonical.digest, out.size());
    return out;
}

void appendLE(std::vector<Byte>& out, uint64_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; i++) {
        out.push_back(static_cast<Byte>((value >> (8 * i)) & 0xFF));
    }
}

uint64_t readLE(const Byte* data, size_t bytes) {
    uint64_t value = 0;
    for (size_t i = 0; i < bytes; i++) {
        value |= static_cast<uint64_t>(data[i]) << (8 * i);
    }
    return value;
}

// File list: entry count, then per entry the path length, offset and size, and the path.
std::optional<std::string> encodeDirectory(const ValdiModuleArchive& archive, const BytesView& content) {
    std::vector<Byte> out;
    const auto& paths = archive.getAllEntryPaths();
    appendLE(out, paths.size(), 4);
    for (const auto& path : paths) {
        auto entry = archive.getEntry(path);
        auto view = path.toStringView();
        if (!entry || view.size() > 0xFFFF || entry->data < content.data() ||
            entry->data + entry->size > content.data() + content.size() ||
            static_cast<uint64_t>(entry->data - content.data()) > 0xFFFFFFFFULL || entry->size > 0xFFFFFFFFULL) {
            return std::nullopt;
        }
        appendLE(out, view.size(), 2);
        appendLE(out, static_cast<uint64_t>(entry->data - content.data()), 4);
        appendLE(out, entry->size, 4);
        out.insert(out.end(), view.begin(), view.end());
    }
    return std::string(reinterpret_cast<const char*>(out.data()), out.size());
}

std::optional<std::vector<std::tuple<StringBox, size_t, size_t>>> decodeDirectory(std::string_view encoded) {
    const auto* data = reinterpret_cast<const Byte*>(encoded.data());
    size_t position = 4;
    if (encoded.size() < position) {
        return std::nullopt;
    }
    auto count = static_cast<size_t>(readLE(data, 4));
    std::vector<std::tuple<StringBox, size_t, size_t>> directory;
    directory.reserve(std::min(count, encoded.size() / kDirectoryEntryFixedSize));
    for (size_t i = 0; i < count; i++) {
        if (position + kDirectoryEntryFixedSize > encoded.size()) {
            return std::nullopt;
        }
        auto pathLength = static_cast<size_t>(readLE(data + position, 2));
        auto offset = static_cast<size_t>(readLE(data + position + 2, 4));
        auto size = static_cast<size_t>(readLE(data + position + 6, 4));
        position += kDirectoryEntryFixedSize;
        if (position + pathLength > encoded.size()) {
            return std::nullopt;
        }
        directory.emplace_back(StringCache::getGlobal().makeString(encoded.substr(position, pathLength)), offset, size);
        position += pathLength;
    }
    if (position != encoded.size()) {
        return std::nullopt;
    }
    return directory;
}

struct RecordFields {
    RecordType type = RecordType::Data;
    uint64_t fileListOffset = 0;
    uint32_t fileListLength = 0;
    uint64_t fileListChecksum = 0;
    uint64_t archiveSize = 0;
    std::array<Byte, 16> contentHash{};
};

void appendRecord(std::vector<Byte>& out,
                  const RecordFields& fields,
                  std::string_view moduleName,
                  const ModuleContentIdentity& identity) {
    auto start = out.size();
    auto bodyLength = kRecordFixedSize + moduleName.size() + identity.value.size() + kRecordChecksumSize;
    appendLE(out, bodyLength, 4);
    out.push_back(static_cast<Byte>(fields.type));
    out.push_back(static_cast<Byte>(identity.kind));
    appendLE(out, moduleName.size(), 2);
    appendLE(out, identity.value.size(), 2);
    appendLE(out, fields.fileListLength, 4);
    appendLE(out, fields.fileListOffset, 8);
    appendLE(out, fields.archiveSize, 8);
    appendLE(out, fields.fileListChecksum, 8);
    out.insert(out.end(), fields.contentHash.begin(), fields.contentHash.end());
    out.insert(out.end(), moduleName.begin(), moduleName.end());
    out.insert(out.end(), identity.value.begin(), identity.value.end());
    appendLE(out, checksum64(out.data() + start, out.size() - start), 8);
}

ModuleContentIdentity normalizeIdentity(ModuleContentIdentity identity) {
    if (identity.value.size() > kMaxIdentityLength) {
        auto digest = hash128(reinterpret_cast<const Byte*>(identity.value.data()), identity.value.size());
        identity.value.assign(reinterpret_cast<const char*>(digest.data()), digest.size());
    }
    return identity;
}

bool writeAll(int fd, const Byte* data, size_t len, uint64_t offset) {
    while (len > 0) {
        auto written = pwrite(fd, data, len, static_cast<off_t>(offset));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data += written;
        len -= static_cast<size_t>(written);
        offset += static_cast<uint64_t>(written);
    }
    return true;
}

bool readAll(int fd, std::vector<Byte>& out, size_t len) {
    out.resize(len);
    size_t done = 0;
    while (done < len) {
        auto n = pread(fd, out.data() + done, len - done, static_cast<off_t>(done));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        done += static_cast<size_t>(n);
    }
    return true;
}

// fsync hands the data to the storage device, but on Apple platforms neither orders it against later writes
// nor flushes the device cache. A barrier after the data fsync guarantees that entries are persisted before
// the index records that make them visible. F_FULLFSYNC is the stronger fallback.
bool orderDataBeforeLaterWrites(int fd) {
#if defined(__APPLE__)
    if (fcntl(fd, F_BARRIERFSYNC) == 0) {
        return true;
    }
    return fcntl(fd, F_FULLFSYNC) == 0;
#else
    // fsync on Linux already waits for the data to reach stable storage.
    (void)fd;
    return true;
#endif
}

// Only used to check a content-hash trailer, which the compiler defines as SHA-256.
std::array<Byte, 32> sha256Array(const Byte* data, size_t len) {
    std::array<Byte, 32> out{};
    auto digest = BytesUtils::sha256(data, len);
    std::memcpy(out.data(), digest->data(), out.size());
    return out;
}

std::string generationName(const std::string& buildVersion) {
    auto hash = BytesUtils::sha256String(reinterpret_cast<const Byte*>(buildVersion.data()), buildVersion.size());
    hash.resize(kGenerationHexLength);
    return hash;
}

Path generationDirectoryFor(const Path& root, const std::string& buildVersion) {
    return root.appending(kFormatDirectory).appending(std::string_view(generationName(buildVersion)));
}

bool isLowerHex(std::string_view str) {
    return !str.empty() &&
           std::all_of(str.begin(), str.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

// Earlier formats: the v2 pack directory, and the `<sha256(moduleName)>` files (plus `.XXXXXX` temporaries)
// that the per-module mmap path wrote.
void removeEarlierFormats(const Path& root) {
    DiskUtils::remove(root.appending(kPreviousFormatDirectory));
    for (const auto& file : DiskUtils::listDirectory(root)) {
        auto name = file.getLastComponent();
        auto stem = name.substr(0, name.find('.'));
        if (stem.size() == 64 && isLowerHex(stem) && DiskUtils::isFile(file)) {
            DiskUtils::remove(file);
        }
    }
}

Ref<DispatchQueue> sharedQueue() {
    static auto* kQueue = []() {
        auto queue = DispatchQueue::create(STRING_LITERAL("Valdi Module Archive Cache"), ThreadQoSClassLowest);
        return new Ref<DispatchQueue>(std::move(queue));
    }();
    return *kQueue;
}

struct SharedInstances {
    Mutex mutex;
    std::unordered_map<std::string, Ref<ModuleArchiveCache>> byKey;
};

std::atomic<bool> gApplicationInForeground{false};

SharedInstances& sharedInstances() {
    static auto* kInstances = new SharedInstances();
    return *kInstances;
}

ModuleArchiveCache::Options defaultOptions() {
    ModuleArchiveCache::Options options;
#ifndef NDEBUG
    options.verifyContentOnHit = true;
#endif
    return options;
}

} // namespace

ModuleArchiveCache::ModuleArchiveCache(Path root, std::string buildVersion, Options options)
    : _root(std::move(root)),
      _buildVersion(std::move(buildVersion)),
      _generationDirectory(generationDirectoryFor(_root, _buildVersion)),
      _options(std::move(options)),
      _queue(_options.queue != nullptr ? _options.queue : sharedQueue()) {}

ModuleArchiveCache::~ModuleArchiveCache() {
    closePack();
}

Ref<ModuleArchiveCache> ModuleArchiveCache::getShared(const Path& root, const std::string& buildVersion) {
    return getShared(root, buildVersion, defaultOptions());
}

Ref<ModuleArchiveCache> ModuleArchiveCache::getShared(const Path& root,
                                                      const std::string& buildVersion,
                                                      const Options& options) {
    auto& instances = sharedInstances();
    auto key = root.toString() + '\n' + buildVersion;
    std::lock_guard<Mutex> guard(instances.mutex);
    auto& instance = instances.byKey[key];
    if (instance == nullptr) {
        instance = makeShared<ModuleArchiveCache>(root, buildVersion, options);
    }
    return instance;
}

void ModuleArchiveCache::resetSharedInstancesForTesting() {
    std::unordered_map<std::string, Ref<ModuleArchiveCache>> instances;
    {
        auto& shared = sharedInstances();
        std::lock_guard<Mutex> guard(shared.mutex);
        instances.swap(shared.byKey);
    }
    // Timers on the shared queue may keep an instance alive; close it so that it releases the pack lock.
    for (auto& [key, instance] : instances) {
        instance->close();
    }
}

void ModuleArchiveCache::setApplicationInForeground(bool inForeground) {
    if (gApplicationInForeground.exchange(inForeground) == inForeground || !inForeground) {
        return;
    }
    std::vector<Ref<ModuleArchiveCache>> instances;
    {
        auto& shared = sharedInstances();
        std::lock_guard<Mutex> guard(shared.mutex);
        for (const auto& [key, instance] : shared.byKey) {
            instances.push_back(instance);
        }
    }
    for (const auto& instance : instances) {
        instance->onEnterForeground();
    }
}

void ModuleArchiveCache::onEnterForeground() {
    std::lock_guard<Mutex> guard(_mutex);
    if (_publishScheduled || (_pendingEntries.empty() && _pendingBans.empty())) {
        return;
    }
    // Resuming is itself a burst of loads; give it a fresh quiet period.
    _firstPendingTime = std::chrono::steady_clock::now();
    _lastMissTime = _firstPendingTime;
    _publishScheduled = true;
    auto self = strongSmallRef(this);
    _queue->asyncAfter([self]() { self->onPublishTimer(); }, _options.quietPeriod);
}

void ModuleArchiveCache::close() {
    {
        std::lock_guard<Mutex> guard(_mutex);
        _opened = true;
        _usable = false;
    }
    // A batch that already started writes through the descriptors; wait for it before closing them.
    _queue->sync([]() {});
    std::lock_guard<Mutex> guard(_mutex);
    closePack();
}

void ModuleArchiveCache::disableForBuild(const Path& root, const std::string& buildVersion) {
    auto directory = generationDirectoryFor(root, buildVersion);
    DiskUtils::remove(directory);
    DiskUtils::makeDirectory(directory, true);
    (void)DiskUtils::store(directory.appending(kDisabledMarkerName), std::string_view("1"));
}

const Path& ModuleArchiveCache::getGenerationDirectory() const {
    return _generationDirectory;
}

std::optional<ModuleContentIdentity> ModuleArchiveCache::computeIdentity(const Byte* data, size_t len) {
    auto frameInfo = ZStdUtils::inspectSingleFrame(data, len);
    if (!frameInfo || frameInfo->contentSize == 0 ||
        frameInfo->contentSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return std::nullopt;
    }

    ModuleContentIdentity identity;
    if (frameInfo->contentSha256) {
        identity.kind = ModuleContentIdentity::Kind::ContentSha256;
        identity.value.assign(reinterpret_cast<const char*>(frameInfo->contentSha256->data()),
                              frameInfo->contentSha256->size());
    } else {
        identity.kind = ModuleContentIdentity::Kind::CompressedHash;
        auto digest = hash128(data, len);
        identity.value.assign(reinterpret_cast<const char*>(digest.data()), digest.size());
    }
    return identity;
}

StringBox ModuleArchiveCache::resetReasonName(ResetReason reason) {
    switch (reason) {
        case ResetReason::NewBuild:
            return STRING_LITERAL("new_build");
        case ResetReason::Cap:
            return STRING_LITERAL("cap");
        case ResetReason::BadIndex:
            return STRING_LITERAL("bad_index");
        case ResetReason::CrashLoop:
            return STRING_LITERAL("crash_loop");
    }
    return STRING_LITERAL("unknown");
}

std::string ModuleArchiveCache::makeKey(const StringBox& moduleName, const ModuleContentIdentity& identity) {
    auto name = moduleName.toStringView();
    std::string key;
    key.reserve(name.size() + 2 + identity.value.size());
    key.append(name);
    key.push_back('\0');
    key.push_back(static_cast<char>(identity.kind));
    key.append(identity.value);
    return key;
}

void ModuleArchiveCache::openIfNeeded(const Ref<Metrics>& metrics, ILogger& logger) {
    if (_metrics == nullptr && metrics != nullptr) {
        _metrics = metrics;
    }
    if (_opened) {
        return;
    }
    _opened = true;
    _openedForLoads = true;
    VALDI_TRACE("Valdi.archiveCache.open");
    _usable = openPack(metrics, logger);
    if (!_usable) {
        closePack();
    }
    scheduleMaintenanceLocked();
}

bool ModuleArchiveCache::isUsable(const Ref<Metrics>& metrics, ILogger& logger) {
    std::lock_guard<Mutex> guard(_mutex);
    openIfNeeded(metrics, logger);
    return _usable;
}

bool ModuleArchiveCache::openPack(const Ref<Metrics>& metrics, ILogger& logger) {
    if (!DiskUtils::makeDirectory(_generationDirectory, true) && !DiskUtils::isDirectory(_generationDirectory)) {
        return false;
    }

    if (DiskUtils::isFile(_generationDirectory.appending(kDisabledMarkerName))) {
        // Reported once per recovery rather than on every launch the build stays disabled.
        auto reported = _generationDirectory.appending(kDisabledReportedMarkerName);
        if (!DiskUtils::isFile(reported)) {
            (void)DiskUtils::store(reported, std::string_view("1"));
            if (metrics != nullptr) {
                metrics->emitModuleArchiveCacheReset(resetReasonName(ResetReason::CrashLoop));
            }
        }
        return false;
    }

    auto indexPath = _generationDirectory.appending(kIndexFileName).toString();
    auto dataPath = _generationDirectory.appending(kDataFileName).toString();

    bool indexExisted = access(indexPath.c_str(), F_OK) == 0;
    _indexFd = open(indexPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (_indexFd < 0) {
        return false;
    }
    // One writer per pack. Another process using the same directory keeps the heap path.
    if (flock(_indexFd, LOCK_EX | LOCK_NB) != 0) {
        VALDI_WARN(logger, "Module archive cache is locked by another process: {}", strerror(errno));
        return false;
    }
    _dataFd = open(dataPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (_dataFd < 0) {
        return false;
    }

    struct stat dataStat{};
    struct stat indexStat{};
    if (fstat(_dataFd, &dataStat) != 0 || fstat(_indexFd, &indexStat) != 0) {
        return false;
    }
    auto dataSize = static_cast<uint64_t>(dataStat.st_size);
    auto indexSize = static_cast<size_t>(indexStat.st_size);

    if (!indexExisted) {
        return resetPack(ResetReason::NewBuild, metrics);
    }
    if (dataSize >= _options.maxPackSize) {
        return resetPack(ResetReason::Cap, metrics);
    }

    std::vector<Byte> index;
    if (!readAll(_indexFd, index, indexSize) || indexSize < kIndexHeaderSize ||
        readLE(index.data(), 4) != kIndexMagic || readLE(index.data() + 4, 4) != kIndexVersion) {
        return resetPack(ResetReason::BadIndex, metrics);
    }

    size_t position = kIndexHeaderSize;
    uint64_t dataEnd = 0;
    while (position + 4 <= index.size()) {
        auto bodyLength = static_cast<size_t>(readLE(index.data() + position, 4));
        // Subtraction: the length is read before the checksum, and the sum can wrap where size_t is 32 bits.
        if (bodyLength < kRecordFixedSize + kRecordChecksumSize || index.size() - position - 4 < bodyLength) {
            break;
        }
        const auto* record = index.data() + position;
        auto checksumOffset = 4 + bodyLength - kRecordChecksumSize;
        if (checksum64(record, checksumOffset) != readLE(record + checksumOffset, 8)) {
            break;
        }

        const auto* body = record + 4;
        auto type = static_cast<RecordType>(body[0]);
        ModuleContentIdentity identity;
        identity.kind = static_cast<ModuleContentIdentity::Kind>(body[1]);
        auto nameLength = static_cast<size_t>(readLE(body + 2, 2));
        auto identityLength = static_cast<size_t>(readLE(body + 4, 2));
        auto fileListLength = readLE(body + 6, 4);
        auto fileListOffset = readLE(body + 10, 8);
        auto archiveSize = readLE(body + 18, 8);
        auto fileListChecksum = readLE(body + 26, 8);
        if (kRecordFixedSize + nameLength + identityLength + kRecordChecksumSize != bodyLength) {
            break;
        }
        const auto* name = body + kRecordFixedSize;
        identity.value.assign(reinterpret_cast<const char*>(name + nameLength), identityLength);
        auto key = makeKey(
            StringCache::getGlobal().makeString(std::string_view(reinterpret_cast<const char*>(name), nameLength)),
            identity);

        if (type == RecordType::Data) {
            auto archiveOffset = alignUp(fileListOffset + fileListLength);
            if (archiveSize == 0 || fileListOffset % kEntryAlignment != 0 || archiveOffset + archiveSize > dataSize) {
                break;
            }
            IndexEntry entry;
            entry.fileListOffset = fileListOffset;
            entry.fileListLength = static_cast<uint32_t>(fileListLength);
            entry.fileListChecksum = fileListChecksum;
            entry.archiveOffset = archiveOffset;
            entry.size = archiveSize;
            std::memcpy(entry.contentHash.data(), body + 34, entry.contentHash.size());
            _entries[key] = entry;
            dataEnd = std::max(dataEnd, archiveOffset + archiveSize);
        } else if (type == RecordType::Ban) {
            _bans.insert(key);
        } else {
            break;
        }
        position += 4 + bodyLength;
    }

    if (position < index.size()) {
        (void)ftruncate(_indexFd, static_cast<off_t>(position));
    }
    if (dataSize > dataEnd) {
        // A batch was cut off between writing its data and committing its records.
        (void)ftruncate(_dataFd, static_cast<off_t>(dataEnd));
    }

    _indexEnd = position;
    _dataEnd = dataEnd;
    _mappedSize = dataEnd;
    if (dataEnd > 0) {
        auto mapping = MmapBuffer::mapReadOnly(_dataFd, static_cast<size_t>(dataEnd), dataPath);
        if (!mapping) {
            VALDI_WARN(logger, "Failed to map module archive pack: {}", mapping.error());
            return false;
        }
        _mapping = mapping.moveValue();
    }

    if (metrics != nullptr) {
        metrics->emitModuleArchivePackSize(static_cast<int64_t>(dataEnd / 1024));
    }
    return true;
}

bool ModuleArchiveCache::resetPack(ResetReason reason, const Ref<Metrics>& metrics) {
    _entries.clear();
    _bans.clear();
    _mapping = nullptr;
    _mappedSize = 0;
    _dataEnd = 0;
    std::vector<Byte> header;
    appendLE(header, kIndexMagic, 4);
    appendLE(header, kIndexVersion, 4);
    bool ok = ftruncate(_dataFd, 0) == 0 && ftruncate(_indexFd, 0) == 0 &&
              writeAll(_indexFd, header.data(), header.size(), 0);
    _indexEnd = ok ? header.size() : 0;
    if (metrics != nullptr) {
        metrics->emitModuleArchiveCacheReset(resetReasonName(reason));
    }
    // On failure the pack is unusable for this process; the next open sees a bad index and starts over.
    return ok;
}

void ModuleArchiveCache::closePack() {
    _usable = false;
    _mapping = nullptr;
    _mappedSize = 0;
    if (_dataFd >= 0) {
        ::close(_dataFd);
        _dataFd = -1;
    }
    if (_indexFd >= 0) {
        // Closing the last descriptor releases the flock.
        ::close(_indexFd);
        _indexFd = -1;
    }
}

std::optional<ValdiModuleArchive> ModuleArchiveCache::lookup(const StringBox& moduleName,
                                                             const ModuleContentIdentity& identity,
                                                             const Ref<Metrics>& metrics,
                                                             ILogger& logger,
                                                             std::string* outEntryKey) {
    auto stored = normalizeIdentity(identity);
    auto key = makeKey(moduleName, stored);
    std::unique_lock<Mutex> lock(_mutex);
    openIfNeeded(metrics, logger);
    auto hit = lookupLocked(lock, moduleName, key, stored, metrics, logger);
    if (hit && outEntryKey != nullptr) {
        *outEntryKey = std::move(key);
    }
    return hit;
}

std::optional<ValdiModuleArchive> ModuleArchiveCache::lookupLocked(std::unique_lock<Mutex>& lock,
                                                                   const StringBox& moduleName,
                                                                   const std::string& key,
                                                                   const ModuleContentIdentity& identity,
                                                                   const Ref<Metrics>& metrics,
                                                                   ILogger& logger) {
    if (!_usable || _bans.count(key) != 0) {
        return std::nullopt;
    }
    auto it = _entries.find(key);
    if (it == _entries.end() || _mapping == nullptr || it->second.archiveOffset + it->second.size > _mappedSize) {
        return std::nullopt;
    }
    auto entry = it->second;
    auto mapping = _mapping;

    lock.unlock();
    const auto* base = mapping->data();
    BytesView content(mapping, base + static_cast<size_t>(entry.archiveOffset), static_cast<size_t>(entry.size));
    // The file list sits right before the archive, usually on the archive's first page, so the entry headers
    // spread through the archive are never touched and a hit only faults in the files that get used.
    std::string_view fileList(reinterpret_cast<const char*>(base + static_cast<size_t>(entry.fileListOffset)),
                              entry.fileListLength);
    std::optional<ValdiModuleArchive> archive;
    bool corrupt = false;
    if (checksum64(reinterpret_cast<const Byte*>(fileList.data()), fileList.size()) != entry.fileListChecksum) {
        corrupt = true;
        VALDI_WARN(logger, "Module archive cache entry for '{}' has a damaged file list", moduleName);
    }
    if (!corrupt && _options.verifyContentOnHit && hash128(content.data(), content.size()) != entry.contentHash) {
        corrupt = true;
        VALDI_WARN(logger, "Module archive cache entry for '{}' does not match its content hash", moduleName);
    }
    if (!corrupt) {
        auto directory = decodeDirectory(fileList);
        auto built = directory ? ValdiModuleArchive::fromDirectory(content, *directory) :
                                 Result<ValdiModuleArchive>(Error("Invalid file list"));
        if (built) {
            archive = built.moveValue();
        } else {
            corrupt = true;
            VALDI_WARN(logger, "Module archive cache entry for '{}' is unreadable: {}", moduleName, built.error());
        }
    }
    lock.lock();

    if (corrupt) {
        banLocked(key, moduleName, identity);
        if (metrics != nullptr) {
            metrics->emitModuleArchiveCacheBan(moduleName);
        }
        return std::nullopt;
    }

    noteLoadLocked(false, metrics);
    return archive;
}

Result<ValdiModuleArchive> ModuleArchiveCache::load(const StringBox& moduleName,
                                                    const std::optional<ModuleContentIdentity>& sourceIdentity,
                                                    const Byte* data,
                                                    size_t len,
                                                    const Ref<Metrics>& metrics,
                                                    ILogger& logger,
                                                    bool& outHit,
                                                    std::string* outEntryKey) {
    outHit = false;
    auto frameInfo = ZStdUtils::inspectSingleFrame(data, len);
    if (!frameInfo || frameInfo->contentSize == 0 ||
        frameInfo->contentSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return Error("Module archive is not a single ZStd frame with a known content size");
    }

    auto identity = sourceIdentity;
    if (!identity) {
        identity = computeIdentity(data, len);
        if (!identity) {
            return Error("Module archive has no usable identity");
        }
        if (metrics != nullptr && identity->kind == ModuleContentIdentity::Kind::CompressedHash) {
            metrics->emitModuleArchiveCacheNoContentHash(moduleName);
        }
    }
    identity = normalizeIdentity(std::move(*identity));
    auto key = makeKey(moduleName, *identity);

    std::unique_lock<Mutex> lock(_mutex);
    openIfNeeded(metrics, logger);
    // A loader-provided identity was already probed by the caller before reading the bytes.
    if (!sourceIdentity) {
        auto hit = lookupLocked(lock, moduleName, key, *identity, metrics, logger);
        if (hit) {
            outHit = true;
            if (outEntryKey != nullptr) {
                *outEntryKey = key;
            }
            return std::move(*hit);
        }
    }
    noteLoadLocked(true, metrics);
    lock.unlock();

    VALDI_TRACE("Valdi.archiveCache.miss");
    auto decompressed = ZStdUtils::decompress(data, len);
    if (!decompressed) {
        return decompressed.moveError();
    }
    auto content = decompressed.value()->toBytesView();
    auto archive = ValdiModuleArchive::deserialize(content);
    if (!archive) {
        return archive.moveError();
    }

    // The index stores name lengths in 16 bits.
    if (moduleName.toStringView().size() > 0xFFFF) {
        return archive;
    }
    auto directory = encodeDirectory(archive.value(), content);
    if (!directory || directory->size() > 0xFFFFFFFFULL) {
        return archive;
    }
    auto reservedBytes = alignUp(directory->size()) + alignUp(content.size());

    lock.lock();
    // A full pack stops taking archives but keeps serving the ones it has; the next build starts a new pack.
    if (!_usable || _bans.count(key) != 0 || _queuedKeys.count(key) != 0 || _entries.count(key) != 0 ||
        _dataEnd + _pendingBytes + reservedBytes > _options.maxPackSize) {
        return archive;
    }
    _queuedKeys.insert(key);
    PendingEntry pending;
    pending.key = key;
    pending.moduleName = moduleName;
    pending.identity = *identity;
    pending.content = content;
    pending.expectedContentSha256 = frameInfo->contentSha256;
    pending.directory = std::move(*directory);
    _pendingBytes += reservedBytes;
    _pendingEntries.emplace_back(std::move(pending));
    schedulePublishLocked(_options.quietPeriod);
    return archive;
}

void ModuleArchiveCache::reportParseFailure(const StringBox& moduleName,
                                            const std::string& entryKey,
                                            const Ref<Metrics>& metrics) {
    std::lock_guard<Mutex> guard(_mutex);
    // The key is the module name, a separator, the identity kind and the identity bytes.
    auto separator = moduleName.toStringView().size();
    if (entryKey.size() < separator + 2 || entryKey.compare(0, separator, moduleName.toStringView()) != 0 ||
        entryKey[separator] != '\0' || _bans.count(entryKey) != 0) {
        return;
    }
    ModuleContentIdentity identity;
    identity.kind = static_cast<ModuleContentIdentity::Kind>(entryKey[separator + 1]);
    identity.value = entryKey.substr(separator + 2);
    banLocked(entryKey, moduleName, identity);
    if (metrics != nullptr) {
        metrics->emitModuleArchiveCacheBan(moduleName);
    }
}

void ModuleArchiveCache::banLocked(const std::string& key,
                                   const StringBox& moduleName,
                                   const ModuleContentIdentity& identity) {
    if (!_bans.insert(key).second || !_usable) {
        return;
    }
    _pendingBans.push_back(PendingBan{key, moduleName, identity});
    schedulePublishLocked(_options.quietPeriod);
}

void ModuleArchiveCache::noteLoadLocked(bool miss, const Ref<Metrics>& metrics) {
    if (_metrics == nullptr && metrics != nullptr) {
        _metrics = metrics;
    }
    _lastLoadTime = std::chrono::steady_clock::now();
    if (miss) {
        _launchMisses++;
    }
    if (!_launchMissesReported && !_launchQuietCheckScheduled) {
        scheduleLaunchQuietCheckLocked(_options.quietPeriod);
    }
}

void ModuleArchiveCache::schedulePublishLocked(std::chrono::steady_clock::duration delay) {
    auto now = std::chrono::steady_clock::now();
    // Called right after queueing, so exactly one pending item means this is the first of the batch.
    if (_pendingEntries.size() + _pendingBans.size() == 1) {
        _firstPendingTime = now;
    }
    _lastMissTime = now;
    if (_publishScheduled) {
        return;
    }
    _publishScheduled = true;
    auto self = strongSmallRef(this);
    _queue->asyncAfter([self]() { self->onPublishTimer(); }, delay);
}

void ModuleArchiveCache::onPublishTimer() {
    {
        std::lock_guard<Mutex> guard(_mutex);
        if (_pendingEntries.empty() && _pendingBans.empty()) {
            _publishScheduled = false;
            return;
        }
        if (!gApplicationInForeground) {
            // Picked up again by onEnterForeground().
            _publishScheduled = false;
            return;
        }
        auto deadline = std::min(_lastMissTime + _options.quietPeriod, _firstPendingTime + _options.maxPublishDelay);
        auto now = std::chrono::steady_clock::now();
        if (now < deadline) {
            auto self = strongSmallRef(this);
            _queue->asyncAfter([self]() { self->onPublishTimer(); }, deadline - now);
            return;
        }
    }
    publishBatch();
}

void ModuleArchiveCache::scheduleLaunchQuietCheckLocked(std::chrono::steady_clock::duration delay) {
    _launchQuietCheckScheduled = true;
    auto self = strongSmallRef(this);
    _queue->asyncAfter([self]() { self->onLaunchQuietTimer(); }, delay);
}

void ModuleArchiveCache::onLaunchQuietTimer() {
    Ref<Metrics> metrics;
    size_t misses = 0;
    {
        std::lock_guard<Mutex> guard(_mutex);
        auto quietUntil = _lastLoadTime + _options.quietPeriod;
        auto now = std::chrono::steady_clock::now();
        if (now < quietUntil) {
            auto self = strongSmallRef(this);
            _queue->asyncAfter([self]() { self->onLaunchQuietTimer(); }, quietUntil - now);
            return;
        }
        _launchQuietCheckScheduled = false;
        _launchMissesReported = true;
        metrics = _metrics;
        misses = _launchMisses;
    }
    if (metrics != nullptr) {
        metrics->emitModuleArchiveLaunchMisses(static_cast<int64_t>(misses));
    }
}

void ModuleArchiveCache::flushPendingPublishes() {
    auto self = strongSmallRef(this);
    _queue->sync([self]() { self->publishBatch(); });
}

void ModuleArchiveCache::publishBatch() {
    std::vector<PendingEntry> entries;
    std::vector<PendingBan> bans;
    int dataFd = -1;
    int indexFd = -1;
    uint64_t dataEnd = 0;
    uint64_t indexEnd = 0;
    Ref<Metrics> metrics;
    {
        std::lock_guard<Mutex> guard(_mutex);
        entries.swap(_pendingEntries);
        bans.swap(_pendingBans);
        _pendingBytes = 0;
        _publishScheduled = false;
        if (!_usable || (entries.empty() && bans.empty())) {
            return;
        }
        dataFd = _dataFd;
        indexFd = _indexFd;
        dataEnd = _dataEnd;
        indexEnd = _indexEnd;
        metrics = _metrics;
    }
    VALDI_TRACE("Valdi.archiveCache.publish");

    auto failPublish = [&](const PendingEntry& entry) {
        if (metrics != nullptr) {
            metrics->emitModuleArchiveCachePublishFail(entry.moduleName);
        }
    };

    std::vector<Byte> records;
    std::vector<const PendingEntry*> written;
    bool writeFailed = false;
    auto offset = alignUp(dataEnd);
    for (const auto& entry : entries) {
        if (writeFailed) {
            failPublish(entry);
            continue;
        }
        if (entry.expectedContentSha256 &&
            sha256Array(entry.content.data(), entry.content.size()) != *entry.expectedContentSha256) {
            failPublish(entry);
            continue;
        }
        RecordFields fields;
        fields.fileListOffset = offset;
        fields.fileListLength = static_cast<uint32_t>(entry.directory.size());
        fields.fileListChecksum =
            checksum64(reinterpret_cast<const Byte*>(entry.directory.data()), entry.directory.size());
        fields.archiveSize = entry.content.size();
        fields.contentHash = hash128(entry.content.data(), entry.content.size());
        auto archiveOffset = alignUp(offset + entry.directory.size());
        if (archiveOffset + entry.content.size() > _options.maxPackSize) {
            failPublish(entry);
            continue;
        }
        if (!writeAll(dataFd, reinterpret_cast<const Byte*>(entry.directory.data()), entry.directory.size(), offset) ||
            !writeAll(dataFd, entry.content.data(), entry.content.size(), archiveOffset)) {
            // Usually ENOSPC. Everything written so far is still committed below.
            writeFailed = true;
            failPublish(entry);
            continue;
        }
        appendRecord(records, fields, entry.moduleName.toStringView(), entry.identity);
        written.push_back(&entry);
        dataEnd = archiveOffset + entry.content.size();
        offset = alignUp(dataEnd);
    }

    if (!written.empty() && (fsync(dataFd) != 0 || !orderDataBeforeLaterWrites(dataFd))) {
        // Records must never become durable before their data.
        for (const auto* entry : written) {
            failPublish(*entry);
        }
        records.clear();
        writeFailed = true;
    }

    for (const auto& ban : bans) {
        RecordFields fields;
        fields.type = RecordType::Ban;
        appendRecord(records, fields, ban.moduleName.toStringView(), ban.identity);
    }

    bool committed =
        !records.empty() && writeAll(indexFd, records.data(), records.size(), indexEnd) && fsync(indexFd) == 0;

    std::lock_guard<Mutex> guard(_mutex);
    if (committed) {
        _indexEnd = indexEnd + records.size();
        _dataEnd = dataEnd;
    }
    if (writeFailed || (!records.empty() && !committed)) {
        // A partial index write would hide every later record; stop appending until the next process,
        // whose open truncates the tail.
        _usable = false;
    }
}

void ModuleArchiveCache::scheduleMaintenanceLocked() {
    if (_maintenanceScheduled) {
        return;
    }
    _maintenanceScheduled = true;
    auto self = strongSmallRef(this);
    _queue->asyncAfter([self]() { self->runMaintenance(); }, _options.maintenanceDelay);
}

void ModuleArchiveCache::runMaintenance() {
    auto currentGeneration = _generationDirectory.getLastComponent();
    for (const auto& generation : DiskUtils::listDirectory(_root.appending(kFormatDirectory))) {
        if (generation.getLastComponent() != currentGeneration) {
            DiskUtils::remove(generation);
        }
    }
    removeEarlierFormats(_root);
}

void ModuleArchiveCache::scheduleRemoval() {
    {
        std::lock_guard<Mutex> guard(_mutex);
        if (_removalScheduled) {
            return;
        }
        _removalScheduled = true;
    }
    auto self = strongSmallRef(this);
    _queue->asyncAfter(
        [self]() {
            {
                std::lock_guard<Mutex> guard(self->_mutex);
                if (self->_openedForLoads) {
                    return;
                }
                // Off for the rest of this process, so nothing opens the files being deleted.
                self->_opened = true;
                self->_usable = false;
            }
            self->removeAllGenerations();
        },
        _options.removalDelay);
}

void ModuleArchiveCache::removeAllGenerations() {
    auto currentGeneration = _generationDirectory.getLastComponent();
    for (const auto& generation : DiskUtils::listDirectory(_root.appending(kFormatDirectory))) {
        // This build's crash-loop marker outlives the pack being turned off, so turning it back on later in the
        // build does not re-enable a build that was disabled.
        if (generation.getLastComponent() == currentGeneration &&
            DiskUtils::isFile(generation.appending(kDisabledMarkerName))) {
            continue;
        }
        DiskUtils::remove(generation);
    }
    removeEarlierFormats(_root);
}

} // namespace Valdi
