//
//  ModuleArchiveCache.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi/runtime/Interfaces/IResourceLoader.hpp"
#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Valdi {

class ILogger;
class Metrics;
class MmapBuffer;

/**
 * Persistent cache of decompressed module archives, shared by every Runtime of the process.
 *
 * All entries live in one append-only data file, mapped once per process, and are described by an
 * append-only index log. A hit is a lookup in the in-memory index and a view into that mapping: no file
 * operation, no read and no decompression. A miss decompresses to the heap exactly like the heap path and
 * queues the buffer; queued buffers are appended in batches on a low-priority queue once misses go quiet,
 * and only become visible to later processes once their data is durable.
 *
 * Entries are keyed by module name and content identity (see ModuleContentIdentity), so two variants of
 * one module are two entries. Files live in `<root>/v3/<generation>/`, where the generation is derived
 * from the build string the host passes; a new build starts a new pack and deletes the old ones.
 */
class ModuleArchiveCache : public SimpleRefCountable {
public:
    struct Options {
        // A publish batch runs once no miss has arrived for this long...
        std::chrono::steady_clock::duration quietPeriod = std::chrono::seconds(4);
        // ...or at the latest this long after the first pending miss.
        std::chrono::steady_clock::duration maxPublishDelay = std::chrono::seconds(30);
        std::chrono::steady_clock::duration maintenanceDelay = std::chrono::seconds(10);
        // How long scheduleRemoval() waits before deleting. Early loads can read the switch before the host
        // has its value; a load that opens the pack within this time cancels the removal.
        std::chrono::steady_clock::duration removalDelay = std::chrono::seconds(30);
        size_t maxPackSize = 48 * 1024 * 1024;
        // Hashes every entry served from the pack and treats a mismatch as corruption. Too slow for
        // release builds; the hash is still recorded for diagnostics.
        bool verifyContentOnHit = false;
        // Overrides the shared background queue; used by tests.
        Ref<DispatchQueue> queue;
    };

    ModuleArchiveCache(Path root, std::string buildVersion, Options options);
    ~ModuleArchiveCache() override;

    /**
     * Returns the process-wide cache for `root` and `buildVersion`, creating it on first use. Every
     * Runtime must use this instance: the pack has a single writer.
     */
    static Ref<ModuleArchiveCache> getShared(const Path& root, const std::string& buildVersion);
    // `options` only apply if this call creates the instance; tests only.
    static Ref<ModuleArchiveCache> getShared(const Path& root, const std::string& buildVersion, const Options& options);

    /**
     * Forgets the process-wide instances, so that the next getShared() opens the pack again as a new
     * process would. Tests only.
     */
    static void resetSharedInstancesForTesting();

    /**
     * Publish batches only run while the app is in the foreground: the background transition has a shared,
     * hard time budget. Batches that come due in the background wait for the next foreground. Process-wide;
     * the app starts in the background state.
     */
    static void setApplicationInForeground(bool inForeground);

    /**
     * Deletes the pack of `buildVersion` and keeps the cache off for that build. Called by the host's
     * crash-loop recovery: a corrupt entry the runtime cannot detect would otherwise come back on every
     * launch.
     */
    static void disableForBuild(const Path& root, const std::string& buildVersion);

    /**
     * Returns the cached archive for `moduleName` with the given identity, or nullopt on a miss. Does
     * not need the module's bytes. On a hit, `outEntryKey` (if non-null) receives the key of the entry that
     * served it, for reportParseFailure.
     */
    std::optional<ValdiModuleArchive> lookup(const StringBox& moduleName,
                                             const ModuleContentIdentity& identity,
                                             const Ref<Metrics>& metrics,
                                             ILogger& logger,
                                             std::string* outEntryKey = nullptr);

    /**
     * Returns the archive for the compressed module `data`. `sourceIdentity` is the identity the loader
     * reported for these bytes, if any; otherwise one is computed from the bytes. On a miss the archive is
     * decompressed to the heap and queued for the pack. `outHit` is set to whether the archive came from the
     * pack; on a hit, `outEntryKey` (if non-null) receives the key of the entry that served it.
     */
    [[nodiscard]] Result<ValdiModuleArchive> load(const StringBox& moduleName,
                                                  const std::optional<ModuleContentIdentity>& sourceIdentity,
                                                  const Byte* data,
                                                  size_t len,
                                                  const Ref<Metrics>& metrics,
                                                  ILogger& logger,
                                                  bool& outHit,
                                                  std::string* outEntryKey = nullptr);

    /**
     * Returns whether the pack can serve and store archives in this process, opening it on first use.
     */
    bool isUsable(const Ref<Metrics>& metrics, ILogger& logger);

    /**
     * Reports that the JS engine failed to parse a file of `moduleName`, which was served from the pack entry
     * `entryKey` (as returned by lookup or load). That entry is banned for the rest of the build; other
     * variants of the module are not affected.
     */
    void reportParseFailure(const StringBox& moduleName, const std::string& entryKey, const Ref<Metrics>& metrics);

    /**
     * Synchronously appends every queued miss to the pack and commits it.
     */
    void flushPendingPublishes();

    /**
     * Synchronously deletes other generations and the files of earlier cache formats.
     */
    void runMaintenance();

    /**
     * Releases the pack and its lock; later lookups miss and nothing more is appended.
     */
    void close();

    /**
     * Deletes every pack, and the files of earlier formats, in the background after
     * Options::removalDelay, at most once per instance. Used when the pack is off so that the files don't
     * outlive the experiment. Skipped if a load opened the pack in the meantime; once it runs, the pack stays
     * off for the rest of the process.
     */
    void scheduleRemoval();

    /**
     * Content identity computed from the compressed bytes: the content-hash trailer if present, else the
     * XXH3-128 of the compressed bytes. Nullopt if the input is not a single ZStd frame with a known content
     * size.
     */
    static std::optional<ModuleContentIdentity> computeIdentity(const Byte* data, size_t len);

    const Path& getGenerationDirectory() const;

private:
    struct IndexEntry {
        // The archive's file list, which lets a hit skip walking the entry headers in the archive.
        uint64_t fileListOffset = 0;
        uint32_t fileListLength = 0;
        uint64_t fileListChecksum = 0;
        uint64_t archiveOffset = 0;
        uint64_t size = 0;
        std::array<Byte, 16> contentHash{};
    };

    struct PendingEntry {
        std::string key;
        StringBox moduleName;
        ModuleContentIdentity identity;
        BytesView content;
        // Digest claimed by a content-hash trailer; checked before the entry is appended.
        std::optional<std::array<Byte, 32>> expectedContentSha256;
        // Encoded file list, written to the data file right before the archive.
        std::string directory;
    };

    struct PendingBan {
        std::string key;
        StringBox moduleName;
        ModuleContentIdentity identity;
    };

    enum class ResetReason { NewBuild, Cap, BadIndex, CrashLoop };

    Path _root;
    std::string _buildVersion;
    Path _generationDirectory;
    Options _options;
    Ref<DispatchQueue> _queue;

    Mutex _mutex;
    bool _opened = false;
    // Set when a load (not close() or removal) opened the pack.
    bool _openedForLoads = false;
    bool _usable = false;
    bool _removalScheduled = false;
    bool _maintenanceScheduled = false;
    int _dataFd = -1;
    int _indexFd = -1;
    Ref<MmapBuffer> _mapping;
    uint64_t _mappedSize = 0;
    // Where the next batch appends. Only advanced by the publish batch.
    uint64_t _dataEnd = 0;
    uint64_t _indexEnd = 0;
    std::unordered_map<std::string, IndexEntry> _entries;
    std::unordered_set<std::string> _bans;
    // Keys queued or appended by this process; never queued twice.
    std::unordered_set<std::string> _queuedKeys;
    std::vector<PendingEntry> _pendingEntries;
    std::vector<PendingBan> _pendingBans;
    uint64_t _pendingBytes = 0;
    bool _publishScheduled = false;
    std::chrono::steady_clock::time_point _firstPendingTime;
    std::chrono::steady_clock::time_point _lastMissTime;
    // Launch_Misses: reported once, when module loads first go quiet.
    bool _launchMissesReported = false;
    bool _launchQuietCheckScheduled = false;
    size_t _launchMisses = 0;
    std::chrono::steady_clock::time_point _lastLoadTime;
    Ref<Metrics> _metrics;

    static std::string makeKey(const StringBox& moduleName, const ModuleContentIdentity& identity);
    static StringBox resetReasonName(ResetReason reason);

    void openIfNeeded(const Ref<Metrics>& metrics, ILogger& logger);
    bool openPack(const Ref<Metrics>& metrics, ILogger& logger);
    void closePack();
    bool resetPack(ResetReason reason, const Ref<Metrics>& metrics);
    std::optional<ValdiModuleArchive> lookupLocked(std::unique_lock<Mutex>& lock,
                                                   const StringBox& moduleName,
                                                   const std::string& key,
                                                   const ModuleContentIdentity& identity,
                                                   const Ref<Metrics>& metrics,
                                                   ILogger& logger);
    void banLocked(const std::string& key, const StringBox& moduleName, const ModuleContentIdentity& identity);
    void noteLoadLocked(bool miss, const Ref<Metrics>& metrics);
    void schedulePublishLocked(std::chrono::steady_clock::duration delay);
    void onPublishTimer();
    void onEnterForeground();
    void scheduleLaunchQuietCheckLocked(std::chrono::steady_clock::duration delay);
    void onLaunchQuietTimer();
    void removeAllGenerations();
    void publishBatch();
    void scheduleMaintenanceLocked();
};

} // namespace Valdi
