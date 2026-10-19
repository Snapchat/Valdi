//
//  ModuleArchiveCache_tests.cpp
//  valdi-pc
//

#include <gtest/gtest.h>

#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Resources/ModuleArchiveCache.hpp"
#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi/runtime/Utils/BytesUtils.hpp"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "zstd.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        char dir[] = "/tmp/.valdi_archive_pack.XXXXXX";
        if (mkdtemp(dir) == nullptr) {
            throw Exception(STRING_FORMAT("mkdtemp failed: {}", strerror(errno)));
        }
        _path = std::string(dir);
    }
    ~TemporaryDirectory() {
        DiskUtils::remove(path());
    }
    Path path() const {
        return Path(std::string_view(_path));
    }
    std::string child(std::string_view name) const {
        return _path + "/" + std::string(name);
    }

private:
    std::string _path;
};

class RecordingMetrics : public Metrics {
public:
    std::atomic<int> publishFail{0};
    std::atomic<int> noContentHash{0};
    std::atomic<int> ban{0};
    std::atomic<int64_t> packSizeKb{-1};
    std::atomic<int> launchMissesReports{0};
    std::atomic<int64_t> launchMisses{-1};
    std::mutex mutex;
    std::vector<std::string> resets;

    void emitModuleArchiveCachePublishFail(const StringBox&) override {
        ++publishFail;
    }
    void emitModuleArchiveCacheNoContentHash(const StringBox&) override {
        ++noContentHash;
    }
    void emitModuleArchiveCacheReset(const StringBox& reason) override {
        std::lock_guard<std::mutex> guard(mutex);
        resets.emplace_back(reason.slowToString());
    }
    void emitModuleArchiveCacheBan(const StringBox&) override {
        ++ban;
    }
    void emitModuleArchivePackSize(int64_t kilobytes) override {
        packSizeKb = kilobytes;
    }
    void emitModuleArchiveLaunchMisses(int64_t misses) override {
        ++launchMissesReports;
        launchMisses = misses;
    }

    std::vector<std::string> getResets() {
        std::lock_guard<std::mutex> guard(mutex);
        return resets;
    }

    void emitInitialRenderLatency(const StringBox&, const MetricsDuration&) override {}
    void emitOnViewModelUpdatedLatency(const StringBox&, const MetricsDuration&) override {}
    void emitOnCreateLatency(const StringBox&, const MetricsDuration&) override {}
    void emitOnDestroyLatency(const StringBox&, const MetricsDuration&) override {}
    void emitDestroyContextLatency(const StringBox&, const MetricsDuration&) override {}
    void emitCalculateLayoutLatency(const StringBox&, const StringBox&, const MetricsDuration&) override {}
    void emitCalculateLazyLayoutLatency(const StringBox&, const StringBox&, const MetricsDuration&) override {}
    void emitCalculateLayoutLatencyMeasure(const StringBox&, const StringBox&, const MetricsDuration&) override {}
    void emitCalculateLazyLayoutLatencyMeasure(const StringBox&, const StringBox&, const MetricsDuration&) override {}
    void emitProcessRequestLatency(const StringBox&, const MetricsDuration&) override {}
    void emitSessionTime(const StringBox&, const MetricsDuration&) override {}
    void emitANR(const StringBox&) override {}
    void emitANR() override {}
    void emitRuntimeManagerInitLatency(const MetricsDuration&) override {}
    void emitRuntimeManagerXpatInitLatency(const MetricsDuration&) override {}
    void emitRuntimeManagerIosInitLatency(const MetricsDuration&) override {}
    void emitRuntimePreInitLatency(const MetricsDuration&) override {}
    void emitRuntimeInitLatency(const MetricsDuration&) override {}
    void emitUserSessionReadyLatency(const MetricsDuration&) override {}
    void emitAssetsDownloadSuccess(const StringBox&) override {}
    void emitAssetsDownloadFailure(const StringBox&) override {}
    void emitAssetsCacheHit(const StringBox&) override {}
    void emitAssetsCacheMiss(const StringBox&) override {}
    void emitUncaughtError(const StringBox&) override {}
    void emitUncaughtError() override {}
    void emitOnScrollLatency(const StringBox&, const StringBox&, const MetricsDuration&) override {}
    void emitSlowAsyncJsCall(const StringBox&, const MetricsDuration&) override {}
    void emitSlowSyncJsCallThreshold(const StringBox&, const MetricsDuration&) override {}
};

std::vector<Byte> makeArchive(std::string_view fileContent) {
    ValdiArchiveBuilder builder;
    builder.addEntry(ValdiArchiveEntry(STRING_LITERAL("index.js"), StringCache::getGlobal().makeString(fileContent)));
    auto bytes = builder.build();
    return std::vector<Byte>(bytes->begin(), bytes->end());
}

std::vector<Byte> compress(const std::vector<Byte>& src, bool encodeContentSize = true) {
    auto* cctx = ZSTD_createCCtx();
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, encodeContentSize ? 1 : 0);
    std::vector<Byte> out(ZSTD_compressBound(src.size()));
    auto size = ZSTD_compress2(cctx, out.data(), out.size(), src.data(), src.size());
    EXPECT_FALSE(ZSTD_isError(size));
    out.resize(size);
    ZSTD_freeCCtx(cctx);
    return out;
}

void appendLittleEndian32(std::vector<Byte>& out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out.push_back(static_cast<Byte>((value >> (8 * i)) & 0xFF));
    }
}

void appendContentHashTrailer(std::vector<Byte>& compressed, const Byte* digest) {
    appendLittleEndian32(compressed, ZStdUtils::kContentHashFrameMagic);
    appendLittleEndian32(compressed, 36);
    appendLittleEndian32(compressed, ZStdUtils::kContentHashTag);
    compressed.insert(compressed.end(), digest, digest + 32);
}

// Compressed archive with a valid content-hash trailer, as emitted by the compiler.
std::vector<Byte> compressWithTrailer(const std::vector<Byte>& archive) {
    auto compressed = compress(archive);
    auto digest = BytesUtils::sha256(archive.data(), archive.size());
    appendContentHashTrailer(compressed, digest->data());
    return compressed;
}

void writeFile(const std::string& path, std::string_view content) {
    FILE* f = std::fopen(path.c_str(), "w");
    ASSERT_NE(nullptr, f);
    std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
}

std::string readFile(const std::string& path) {
    std::string out;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return out;
    }
    char buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
        out.append(buffer, n);
    }
    std::fclose(f);
    return out;
}

template<typename Predicate>
bool waitUntil(Predicate predicate) {
    for (int i = 0; i < 200 && !predicate(); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

uint64_t fileSize(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_size);
}

void appendBytes(const std::string& path, std::string_view bytes) {
    FILE* f = std::fopen(path.c_str(), "ab");
    ASSERT_NE(nullptr, f);
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

void overwriteBytes(const std::string& path, uint64_t offset, std::string_view bytes) {
    int fd = open(path.c_str(), O_WRONLY);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(static_cast<ssize_t>(bytes.size()), pwrite(fd, bytes.data(), bytes.size(), static_cast<off_t>(offset)));
    close(fd);
}

std::string_view entryContent(const ValdiModuleArchive& archive) {
    auto entry = archive.getEntry(STRING_LITERAL("index.js"));
    EXPECT_TRUE(entry.has_value());
    if (!entry) {
        return {};
    }
    return std::string_view(reinterpret_cast<const char*>(entry->data), entry->size);
}

ModuleContentIdentity apkIdentity(std::string_view value) {
    return ModuleContentIdentity{ModuleContentIdentity::Kind::BundledContainer, std::string(value)};
}

class ModuleArchiveCacheFixture : public ::testing::Test {
protected:
    TemporaryDirectory _root;
    Ref<RecordingMetrics> _metrics = makeShared<RecordingMetrics>();
    Ref<DispatchQueue> _queue =
        DispatchQueue::createThreaded(STRING_LITERAL("ModuleArchiveCacheTests"), ThreadQoSClassNormal);
    std::vector<Ref<ModuleArchiveCache>> _caches;

    void SetUp() override {
        ModuleArchiveCache::setApplicationInForeground(true);
    }

    void TearDown() override {
        for (auto& cache : _caches) {
            cache->close();
        }
        ModuleArchiveCache::resetSharedInstancesForTesting();
        ModuleArchiveCache::setApplicationInForeground(false);
        _queue->fullTeardown();
    }

    ModuleArchiveCache::Options options() const {
        ModuleArchiveCache::Options options;
        // Tests drive publishing and maintenance explicitly.
        options.quietPeriod = std::chrono::hours(1);
        options.maxPublishDelay = std::chrono::hours(1);
        options.maintenanceDelay = std::chrono::hours(1);
        options.removalDelay = std::chrono::milliseconds(0);
        options.queue = _queue;
        return options;
    }

    // A new instance stands in for a new process. Earlier instances are closed so that they release the
    // pack lock, as a dead process would.
    Ref<ModuleArchiveCache> openCache(std::string buildVersion = "build1",
                                      std::optional<ModuleArchiveCache::Options> customOptions = std::nullopt) {
        for (auto& cache : _caches) {
            cache->close();
        }
        auto cache = makeShared<ModuleArchiveCache>(
            _root.path(), std::move(buildVersion), customOptions ? *customOptions : options());
        _caches.push_back(cache);
        return cache;
    }

    Result<ValdiModuleArchive> load(ModuleArchiveCache& cache,
                                    std::string_view moduleName,
                                    const std::vector<Byte>& compressed,
                                    bool& hit,
                                    std::optional<ModuleContentIdentity> identity = std::nullopt) const {
        return cache.load(StringCache::getGlobal().makeString(moduleName),
                          identity,
                          compressed.data(),
                          compressed.size(),
                          _metrics,
                          ConsoleLogger::getLogger(),
                          hit);
    }

    std::optional<ValdiModuleArchive> lookup(ModuleArchiveCache& cache,
                                             std::string_view moduleName,
                                             const ModuleContentIdentity& identity,
                                             std::string* outEntryKey = nullptr) const {
        return cache.lookup(StringCache::getGlobal().makeString(moduleName),
                            identity,
                            _metrics,
                            ConsoleLogger::getLogger(),
                            outEntryKey);
    }

    std::string dataPath(const ModuleArchiveCache& cache) const {
        return cache.getGenerationDirectory().appending(std::string_view("data")).toString();
    }

    std::string indexPath(const ModuleArchiveCache& cache) const {
        return cache.getGenerationDirectory().appending(std::string_view("index")).toString();
    }

    // Loads `compressed` as `moduleName` in a fresh process and publishes it.
    void publish(std::string_view moduleName,
                 const std::vector<Byte>& compressed,
                 std::optional<ModuleContentIdentity> identity = std::nullopt,
                 std::string buildVersion = "build1") {
        auto cache = openCache(std::move(buildVersion));
        bool hit = true;
        auto result = load(*cache, moduleName, compressed, hit, identity);
        ASSERT_TRUE(result) << result.description();
        ASSERT_FALSE(hit);
        cache->flushPendingPublishes();
    }
};

} // namespace

TEST(ModuleArchiveIdentity, UsesContentHashTrailer) {
    auto archive = makeArchive("console.log('hello')");
    auto compressed = compressWithTrailer(archive);

    auto identity = ModuleArchiveCache::computeIdentity(compressed.data(), compressed.size());
    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(ModuleContentIdentity::Kind::ContentSha256, identity->kind);
    auto digest = BytesUtils::sha256(archive.data(), archive.size());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(digest->data()), 32), identity->value);
}

TEST(ModuleArchiveIdentity, FallsBackToHashOfCompressedBytes) {
    auto compressed = compress(makeArchive("console.log('hello')"));

    auto identity = ModuleArchiveCache::computeIdentity(compressed.data(), compressed.size());
    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(ModuleContentIdentity::Kind::CompressedHash, identity->kind);
    EXPECT_EQ(16u, identity->value.size());
    auto again = ModuleArchiveCache::computeIdentity(compressed.data(), compressed.size());
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(identity->value, again->value);
}

TEST(ModuleArchiveIdentity, DiffersWhenContentDiffers) {
    auto a = compressWithTrailer(makeArchive("version 1"));
    auto b = compressWithTrailer(makeArchive("version 2"));
    auto identityA = ModuleArchiveCache::computeIdentity(a.data(), a.size());
    auto identityB = ModuleArchiveCache::computeIdentity(b.data(), b.size());
    ASSERT_TRUE(identityA && identityB);
    EXPECT_NE(identityA->value, identityB->value);
}

TEST(ModuleArchiveIdentity, RejectsIneligibleInputs) {
    auto archive = makeArchive("content");

    auto noContentSize = compress(archive, /*encodeContentSize=*/false);
    EXPECT_FALSE(ModuleArchiveCache::computeIdentity(noContentSize.data(), noContentSize.size()).has_value());

    auto twoFrames = compress(archive);
    auto second = compress(archive);
    twoFrames.insert(twoFrames.end(), second.begin(), second.end());
    EXPECT_FALSE(ModuleArchiveCache::computeIdentity(twoFrames.data(), twoFrames.size()).has_value());

    EXPECT_FALSE(ModuleArchiveCache::computeIdentity(archive.data(), archive.size()).has_value())
        << "uncompressed archives are not eligible";
}

TEST_F(ModuleArchiveCacheFixture, MissIsServedFromHeapAndHitsInTheNextProcess) {
    auto compressed = compressWithTrailer(makeArchive("console.log('hello')"));

    auto cache = openCache();
    bool hit = true;
    auto first = load(*cache, "my_module", compressed, hit);
    ASSERT_TRUE(first) << first.description();
    EXPECT_FALSE(hit);
    EXPECT_EQ("console.log('hello')", entryContent(first.value()));
    EXPECT_EQ(std::vector<std::string>{"new_build"}, _metrics->getResets());
    EXPECT_EQ(0u, fileSize(dataPath(*cache))) << "nothing is written before the batch";

    cache->flushPendingPublishes();
    EXPECT_LT(0u, fileSize(dataPath(*cache)));

    // Entries appended by this process are not mapped by it, and are never queued twice.
    auto again = load(*cache, "my_module", compressed, hit);
    ASSERT_TRUE(again);
    EXPECT_FALSE(hit);
    auto dataSize = fileSize(dataPath(*cache));
    cache->flushPendingPublishes();
    EXPECT_EQ(dataSize, fileSize(dataPath(*cache)));

    auto next = openCache();
    auto second = load(*next, "my_module", compressed, hit);
    ASSERT_TRUE(second) << second.description();
    EXPECT_TRUE(hit);
    EXPECT_EQ("console.log('hello')", entryContent(second.value()));
}

TEST_F(ModuleArchiveCacheFixture, UnpublishedMissesAreLostWithTheProcess) {
    auto compressed = compressWithTrailer(makeArchive("console.log('hello')"));
    auto cache = openCache();
    bool hit = true;
    ASSERT_TRUE(load(*cache, "my_module", compressed, hit));

    auto next = openCache();
    ASSERT_TRUE(load(*next, "my_module", compressed, hit));
    EXPECT_FALSE(hit);
}

TEST_F(ModuleArchiveCacheFixture, LoaderIdentityHitsWithoutTheModuleBytes) {
    auto compressed = compress(makeArchive("bundled"));
    // Real APK identities are long (path, size and mtime of every APK); the index stores their hash.
    std::string longIdentity = std::string(200, 'p') + ":12345:678";
    publish("bundled_module", compressed, apkIdentity(longIdentity));
    EXPECT_LT(fileSize(indexPath(*openCache())), 8u + 150u) << "header plus one record with a 16-byte identity";

    auto next = openCache();
    EXPECT_TRUE(lookup(*next, "bundled_module", apkIdentity(longIdentity)).has_value());
    EXPECT_FALSE(lookup(*next, "bundled_module", apkIdentity(longIdentity + "9")).has_value());
    auto archive = lookup(*next, "bundled_module", apkIdentity("apk-1"));
    EXPECT_FALSE(archive.has_value());
    publish("bundled_module", compressed, apkIdentity("apk-1"));
    next = openCache();
    archive = lookup(*next, "bundled_module", apkIdentity("apk-1"));
    ASSERT_TRUE(archive.has_value());
    EXPECT_EQ("bundled", entryContent(*archive));
    EXPECT_EQ(0, _metrics->noContentHash.load()) << "a loader identity needs no hashing";

    EXPECT_FALSE(lookup(*next, "bundled_module", apkIdentity("apk-2")).has_value()) << "new install, new identity";
    EXPECT_FALSE(lookup(*next, "other_module", apkIdentity("apk-1")).has_value());
}

TEST_F(ModuleArchiveCacheFixture, TwoVariantsOfOneModuleBothHit) {
    auto v1 = compressWithTrailer(makeArchive("variant 1"));
    auto v2 = compressWithTrailer(makeArchive("variant 2"));
    publish("delivered", v1);
    publish("delivered", v2);

    auto next = openCache();
    bool hit = false;
    auto a = load(*next, "delivered", v1, hit);
    ASSERT_TRUE(a);
    EXPECT_TRUE(hit);
    EXPECT_EQ("variant 1", entryContent(a.value()));
    auto b = load(*next, "delivered", v2, hit);
    ASSERT_TRUE(b);
    EXPECT_TRUE(hit);
    EXPECT_EQ("variant 2", entryContent(b.value()));
}

TEST_F(ModuleArchiveCacheFixture, EntriesStartOn16ByteBoundaries) {
    publish("a", compress(makeArchive("x")));
    publish("b", compress(makeArchive("yy")));
    publish("c", compress(makeArchive("zzz")));

    auto next = openCache();
    for (const auto& [name, content] :
         std::vector<std::pair<std::string, std::string>>{{"a", "x"}, {"b", "yy"}, {"c", "zzz"}}) {
        bool hit = false;
        auto archive = load(*next, name, compress(makeArchive(content)), hit);
        ASSERT_TRUE(archive);
        EXPECT_TRUE(hit) << name;
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(archive.value().getDecompressedContent().data()) % 16) << name;
    }
}

TEST_F(ModuleArchiveCacheFixture, ArchiveWithNoFilesHits) {
    auto emptyBytes = ValdiArchiveBuilder().build();
    auto empty = compress(std::vector<Byte>(emptyBytes->begin(), emptyBytes->end()));
    auto after = compress(makeArchive("after"));
    publish("empty", empty);
    publish("after", after);

    auto next = openCache();
    bool hit = false;
    auto archive = load(*next, "empty", empty, hit);
    ASSERT_TRUE(archive) << archive.description();
    EXPECT_TRUE(hit);
    EXPECT_TRUE(archive.value().getAllEntryPaths().empty());
    auto following = load(*next, "after", after, hit);
    ASSERT_TRUE(following) << following.description();
    EXPECT_TRUE(hit);
    EXPECT_EQ("after", entryContent(following.value()));
    EXPECT_EQ(0, _metrics->ban.load());
}

TEST_F(ModuleArchiveCacheFixture, FrameWithNoContentIsNotCached) {
    auto compressed = compress(std::vector<Byte>());
    auto cache = openCache();
    bool hit = true;
    EXPECT_FALSE(load(*cache, "nothing", compressed, hit));
    EXPECT_FALSE(hit);
    cache->flushPendingPublishes();
    EXPECT_EQ(0u, fileSize(dataPath(*cache)));
    EXPECT_EQ(0, _metrics->ban.load());
}

TEST_F(ModuleArchiveCacheFixture, TornTailIsIgnoredAndTruncated) {
    auto compressed = compress(makeArchive("survivor"));
    publish("survivor", compressed);

    auto cache = openCache();
    auto indexSize = fileSize(indexPath(*cache));
    auto dataSize = fileSize(dataPath(*cache));
    cache->close();
    // A later batch that died mid-write: data without records, and a partial record.
    appendBytes(dataPath(*cache), std::string(4096, 'x'));
    appendBytes(indexPath(*cache), std::string("\x40\x00\x00\x00garbage", 11));

    auto next = openCache();
    bool hit = false;
    auto archive = load(*next, "survivor", compressed, hit);
    ASSERT_TRUE(archive);
    EXPECT_TRUE(hit);
    EXPECT_EQ(indexSize, fileSize(indexPath(*next)));
    EXPECT_EQ(dataSize, fileSize(dataPath(*next)));
}

TEST_F(ModuleArchiveCacheFixture, CorruptRecordLengthIsIgnoredAndTruncated) {
    auto compressed = compress(makeArchive("survivor"));
    publish("survivor", compressed);

    auto cache = openCache();
    auto indexSize = fileSize(indexPath(*cache));
    cache->close();
    // The largest length a record can claim; a bounds check that adds it to the position wraps on 32-bit.
    appendBytes(indexPath(*cache), std::string("\xff\xff\xff\xffgarbage", 11));

    auto next = openCache();
    bool hit = false;
    ASSERT_TRUE(load(*next, "survivor", compressed, hit));
    EXPECT_TRUE(hit);
    EXPECT_EQ(indexSize, fileSize(indexPath(*next)));
}

TEST_F(ModuleArchiveCacheFixture, RecordPastTheEndOfTheDataIsIgnored) {
    auto compressed = compress(makeArchive("lost"));
    publish("lost", compressed);
    auto cache = openCache();
    cache->close();
    ASSERT_EQ(0, truncate(dataPath(*cache).c_str(), 8));

    auto next = openCache();
    bool hit = true;
    ASSERT_TRUE(load(*next, "lost", compressed, hit));
    EXPECT_FALSE(hit);
}

TEST_F(ModuleArchiveCacheFixture, HitUsesTheIndexedFileListInsteadOfTheEntryHeaders) {
    // Two entries whose name and data lengths are multiples of 4, so there is no padding.
    std::string first(64, 'a');
    std::string second(32, 'b');
    ValdiArchiveBuilder builder;
    builder.addEntry(ValdiArchiveEntry(STRING_LITERAL("a.js"), StringCache::getGlobal().makeString(first)));
    builder.addEntry(ValdiArchiveEntry(STRING_LITERAL("b.js"), StringCache::getGlobal().makeString(second)));
    auto built = builder.build();
    std::vector<Byte> archiveBytes(built->begin(), built->end());
    auto compressed = compress(archiveBytes);
    publish("two_files", compressed);

    // Corrupt the second entry's name length. Walking the headers can no longer parse the archive.
    auto secondHeaderOffset = 8 + 4 + 4 + 4 + first.size();
    auto cache = openCache();
    cache->close();
    auto archiveStart = readFile(dataPath(*cache)).find(std::string(archiveBytes.begin(), archiveBytes.end()));
    ASSERT_NE(std::string::npos, archiveStart);
    overwriteBytes(dataPath(*cache), archiveStart + secondHeaderOffset, std::string(4, '\xff'));
    auto damaged = archiveBytes;
    std::fill(damaged.begin() + secondHeaderOffset, damaged.begin() + secondHeaderOffset + 4, 0xff);
    ASSERT_FALSE(ValdiModuleArchive::deserialize(BytesView(nullptr, damaged.data(), damaged.size())));

    auto next = openCache();
    bool hit = false;
    auto archive = load(*next, "two_files", compressed, hit);
    ASSERT_TRUE(archive) << archive.description();
    EXPECT_TRUE(hit);
    auto a = archive.value().getEntry(STRING_LITERAL("a.js"));
    auto b = archive.value().getEntry(STRING_LITERAL("b.js"));
    ASSERT_TRUE(a && b);
    EXPECT_EQ(first, std::string(reinterpret_cast<const char*>(a->data), a->size));
    EXPECT_EQ(second, std::string(reinterpret_cast<const char*>(b->data), b->size));
    EXPECT_EQ((std::vector<StringBox>{STRING_LITERAL("a.js"), STRING_LITERAL("b.js")}),
              archive.value().getAllEntryPaths());
}

TEST_F(ModuleArchiveCacheFixture, UnreadableEntryIsBannedUntilTheNextBuild) {
    auto compressed = compress(makeArchive("doomed"));
    publish("doomed", compressed);
    auto cache = openCache();
    cache->close();
    overwriteBytes(dataPath(*cache), 0, std::string(16, '\xff'));

    auto damaged = openCache();
    bool hit = true;
    auto archive = load(*damaged, "doomed", compressed, hit);
    ASSERT_TRUE(archive) << "falls back to decompressing";
    EXPECT_FALSE(hit);
    EXPECT_EQ("doomed", entryContent(archive.value()));
    EXPECT_EQ(1, _metrics->ban.load());
    damaged->flushPendingPublishes();

    auto sameBuild = openCache();
    ASSERT_TRUE(load(*sameBuild, "doomed", compressed, hit));
    EXPECT_FALSE(hit) << "the ban is sticky within the build";
    sameBuild->flushPendingPublishes();

    publish("doomed", compressed, std::nullopt, "build2");
    auto nextBuild = openCache("build2");
    ASSERT_TRUE(load(*nextBuild, "doomed", compressed, hit));
    EXPECT_TRUE(hit) << "a new build starts a new pack";
}

TEST_F(ModuleArchiveCacheFixture, ContentVerificationCatchesCorruptionThatStillParses) {
    auto compressed = compress(makeArchive("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
    publish("subtle", compressed);
    auto cache = openCache();
    cache->close();
    auto archiveBytes = makeArchive("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    auto contentOffset = readFile(dataPath(*cache)).find("AAAA");
    ASSERT_NE(std::string::npos, contentOffset);
    overwriteBytes(dataPath(*cache), contentOffset, "B");

    auto verifying = options();
    verifying.verifyContentOnHit = true;
    auto next = openCache("build1", verifying);
    bool hit = true;
    auto archive = load(*next, "subtle", compressed, hit);
    ASSERT_TRUE(archive);
    EXPECT_FALSE(hit);
    EXPECT_EQ("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", entryContent(archive.value()));
    EXPECT_EQ(1, _metrics->ban.load());
}

TEST_F(ModuleArchiveCacheFixture, ParseFailureBansTheEntryServedFromThePack) {
    auto compressed = compress(makeArchive("syntax error"));
    publish("bad_js", compressed, apkIdentity("apk"));

    auto cache = openCache();
    std::string entryKey;
    ASSERT_TRUE(lookup(*cache, "bad_js", apkIdentity("apk"), &entryKey).has_value());
    ASSERT_FALSE(entryKey.empty());
    cache->reportParseFailure(STRING_LITERAL("bad_js"), entryKey, _metrics);
    cache->reportParseFailure(STRING_LITERAL("bad_js"), entryKey, _metrics);
    cache->reportParseFailure(STRING_LITERAL("other"), entryKey, _metrics);
    EXPECT_EQ(1, _metrics->ban.load()) << "banned once, and only under its own module name";
    cache->flushPendingPublishes();

    auto next = openCache();
    EXPECT_FALSE(lookup(*next, "bad_js", apkIdentity("apk")).has_value());
}

TEST_F(ModuleArchiveCacheFixture, ParseFailureBansOnlyTheVariantThatFailed) {
    publish("module", compress(makeArchive("variant a")), apkIdentity("a"));
    publish("module", compress(makeArchive("variant b")), apkIdentity("b"));

    auto cache = openCache();
    std::string keyA;
    std::string keyB;
    ASSERT_TRUE(lookup(*cache, "module", apkIdentity("a"), &keyA).has_value());
    ASSERT_TRUE(lookup(*cache, "module", apkIdentity("b"), &keyB).has_value()) << "served last";
    cache->reportParseFailure(STRING_LITERAL("module"), keyA, _metrics);
    cache->flushPendingPublishes();

    auto next = openCache();
    EXPECT_FALSE(lookup(*next, "module", apkIdentity("a")).has_value());
    auto b = lookup(*next, "module", apkIdentity("b"));
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ("variant b", entryContent(*b));
}

TEST_F(ModuleArchiveCacheFixture, TrailerThatDoesNotMatchContentIsNeverAppended) {
    auto archive = makeArchive("real content");
    auto compressed = compress(archive);
    std::array<Byte, 32> wrongDigest{};
    appendContentHashTrailer(compressed, wrongDigest.data());

    publish("lying", compressed);
    EXPECT_EQ(1, _metrics->publishFail.load());

    auto next = openCache();
    bool hit = true;
    ASSERT_TRUE(load(*next, "lying", compressed, hit));
    EXPECT_FALSE(hit);
}

TEST_F(ModuleArchiveCacheFixture, SizeCapStopsAppendingAndResetsAnOversizedPack) {
    auto small = options();
    small.maxPackSize = 200;
    auto cache = openCache("build1", small);
    bool hit = true;
    ASSERT_TRUE(load(*cache, "a", compress(makeArchive(std::string(100, 'a'))), hit));
    ASSERT_TRUE(load(*cache, "b", compress(makeArchive(std::string(100, 'b'))), hit));
    cache->flushPendingPublishes();
    EXPECT_LE(fileSize(dataPath(*cache)), 200u);

    auto oversized = openCache();
    ASSERT_TRUE(load(*oversized, "c", compress(makeArchive(std::string(300, 'c'))), hit));
    oversized->flushPendingPublishes();
    ASSERT_GT(fileSize(dataPath(*oversized)), 200u);

    auto next = openCache("build1", small);
    ASSERT_TRUE(load(*next, "c", compress(makeArchive(std::string(300, 'c'))), hit));
    EXPECT_FALSE(hit);
    auto resets = _metrics->getResets();
    EXPECT_EQ("cap", resets.back());
}

TEST_F(ModuleArchiveCacheFixture, NewBuildStartsFreshAndMaintenanceDeletesOldFiles) {
    auto compressed = compress(makeArchive("content"));
    publish("module", compressed, std::nullopt, "build1");
    auto oldGeneration = openCache("build1")->getGenerationDirectory();

    DiskUtils::makeDirectory(_root.path().appending(std::string_view("v2")), true);
    writeFile(_root.child("v2/1-abc"), "old");
    std::string legacyName(64, 'a');
    writeFile(_root.child(legacyName), "legacy");
    writeFile(_root.child(legacyName + ".Xy12Ab"), "legacy temporary");
    writeFile(_root.child("unrelated.txt"), "keep");

    auto cache = openCache("build2");
    bool hit = true;
    ASSERT_TRUE(load(*cache, "module", compressed, hit));
    EXPECT_FALSE(hit);
    cache->runMaintenance();

    EXPECT_FALSE(DiskUtils::isDirectory(oldGeneration));
    EXPECT_TRUE(DiskUtils::isDirectory(cache->getGenerationDirectory()));
    EXPECT_FALSE(DiskUtils::isDirectory(_root.path().appending(std::string_view("v2"))));
    EXPECT_FALSE(DiskUtils::isFile(Path(std::string_view(_root.child(legacyName)))));
    EXPECT_FALSE(DiskUtils::isFile(Path(std::string_view(_root.child(legacyName + ".Xy12Ab")))));
    EXPECT_TRUE(DiskUtils::isFile(Path(std::string_view(_root.child("unrelated.txt")))));
}

TEST_F(ModuleArchiveCacheFixture, SecondInstanceOnTheSamePackStaysOnTheHeap) {
    auto compressed = compress(makeArchive("content"));
    auto owner = makeShared<ModuleArchiveCache>(_root.path(), "build1", options());
    auto other = makeShared<ModuleArchiveCache>(_root.path(), "build1", options());
    _caches.push_back(owner);
    _caches.push_back(other);

    bool hit = true;
    ASSERT_TRUE(load(*owner, "owned", compressed, hit));
    ASSERT_TRUE(load(*other, "other", compress(makeArchive("other")), hit));
    owner->flushPendingPublishes();
    other->flushPendingPublishes();

    auto next = openCache();
    ASSERT_TRUE(load(*next, "owned", compressed, hit));
    EXPECT_TRUE(hit);
    ASSERT_TRUE(load(*next, "other", compress(makeArchive("other")), hit));
    EXPECT_FALSE(hit) << "only the lock owner appends";
}

TEST_F(ModuleArchiveCacheFixture, DisabledBuildKeepsTheCacheOff) {
    auto compressed = compress(makeArchive("content"));
    publish("module", compressed);
    for (auto& cache : _caches) {
        cache->close();
    }
    ModuleArchiveCache::disableForBuild(_root.path(), "build1");

    auto cache = openCache();
    bool hit = true;
    ASSERT_TRUE(load(*cache, "module", compressed, hit));
    EXPECT_FALSE(hit);
    cache->flushPendingPublishes();
    EXPECT_EQ("crash_loop", _metrics->getResets().back());
    EXPECT_FALSE(cache->isUsable(_metrics, ConsoleLogger::getLogger()));

    publish("module", compressed, std::nullopt, "build2");
    auto warm = openCache("build2");
    ASSERT_TRUE(load(*warm, "module", compressed, hit));
    EXPECT_TRUE(hit) << "the marker only applies to its build";
}

TEST_F(ModuleArchiveCacheFixture, ConcurrentLoadsOfOneModuleAppendItOnce) {
    auto compressed = compress(makeArchive(std::string(10000, 'q')));
    auto cache = openCache();
    std::vector<std::thread> threads;
    std::atomic<int> succeeded{0};
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&]() {
            bool hit = true;
            auto archive = load(*cache, "shared", compressed, hit);
            if (archive && entryContent(archive.value()) == std::string(10000, 'q')) {
                ++succeeded;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(4, succeeded.load());
    cache->flushPendingPublishes();
    EXPECT_LT(fileSize(dataPath(*cache)), 2u * 10000u);

    auto next = openCache();
    bool hit = false;
    ASSERT_TRUE(load(*next, "shared", compressed, hit));
    EXPECT_TRUE(hit);
}

TEST_F(ModuleArchiveCacheFixture, BatchRunsOnceMissesGoQuiet) {
    auto fast = options();
    fast.quietPeriod = std::chrono::milliseconds(50);
    fast.maxPublishDelay = std::chrono::seconds(10);
    auto cache = openCache("build1", fast);
    bool hit = true;
    ASSERT_TRUE(load(*cache, "timed", compress(makeArchive("timed")), hit));
    auto indexSize = fileSize(indexPath(*cache));

    for (int i = 0; i < 100 && (_metrics->launchMissesReports.load() == 0 || fileSize(indexPath(*cache)) == indexSize);
         i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_LT(indexSize, fileSize(indexPath(*cache))) << "the batch committed a record";
    EXPECT_EQ(1, _metrics->launchMissesReports.load());
    EXPECT_EQ(1, _metrics->launchMisses.load());
}

TEST_F(ModuleArchiveCacheFixture, BatchWaitsForTheForeground) {
    ModuleArchiveCache::setApplicationInForeground(false);
    auto fast = options();
    fast.quietPeriod = std::chrono::milliseconds(50);
    fast.maxPublishDelay = std::chrono::milliseconds(100);
    auto cache = ModuleArchiveCache::getShared(_root.path(), "build1", fast);
    bool hit = true;
    ASSERT_TRUE(load(*cache, "background", compress(makeArchive("background")), hit));
    auto indexSize = fileSize(indexPath(*cache));

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_EQ(indexSize, fileSize(indexPath(*cache))) << "no batch in the background";

    ModuleArchiveCache::setApplicationInForeground(true);
    for (int i = 0; i < 100 && fileSize(indexPath(*cache)) == indexSize; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_LT(indexSize, fileSize(indexPath(*cache)));
}

TEST_F(ModuleArchiveCacheFixture, RemovalKeepsThisBuildsCrashLoopMarker) {
    auto compressed = compress(makeArchive("content"));
    publish("module", compressed);
    publish("module", compressed, std::nullopt, "build2");
    ModuleArchiveCache::disableForBuild(_root.path(), "build1");

    auto otherBuild = openCache("build2")->getGenerationDirectory();
    auto cache = openCache();
    cache->scheduleRemoval();
    EXPECT_TRUE(waitUntil([&]() { return !DiskUtils::isDirectory(otherBuild); }));

    // Turned back on later in the same build: still off.
    auto next = openCache();
    bool hit = true;
    ASSERT_TRUE(load(*next, "module", compressed, hit));
    EXPECT_FALSE(hit);
    next->flushPendingPublishes();
    EXPECT_EQ(0u, fileSize(dataPath(*next)));
    EXPECT_EQ("crash_loop", _metrics->getResets().back());
}

TEST_F(ModuleArchiveCacheFixture, ScheduledRemovalDeletesThePacksAndEarlierFormats) {
    publish("module", compress(makeArchive("content")));
    std::string legacyName(64, 'b');
    writeFile(_root.child(legacyName), "legacy");
    writeFile(_root.child("unrelated.txt"), "keep");
    auto cache = openCache();
    auto generation = cache->getGenerationDirectory();
    cache->close();
    cache->scheduleRemoval();
    EXPECT_TRUE(waitUntil([&]() { return !DiskUtils::isFile(Path(std::string_view(_root.child(legacyName)))); }));
    EXPECT_FALSE(DiskUtils::isDirectory(generation));
    EXPECT_TRUE(DiskUtils::isFile(Path(std::string_view(_root.child("unrelated.txt")))));
}

TEST_F(ModuleArchiveCacheFixture, RemovalIsCancelledByALoadThatOpensThePack) {
    auto compressed = compress(makeArchive("content"));
    publish("module", compressed);

    auto delayed = options();
    delayed.removalDelay = std::chrono::milliseconds(100);
    auto cache = openCache("build1", delayed);
    cache->scheduleRemoval();
    bool hit = false;
    ASSERT_TRUE(load(*cache, "module", compressed, hit));
    EXPECT_TRUE(hit) << "the switch read true before the removal ran";
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(DiskUtils::isDirectory(cache->getGenerationDirectory()));
}

TEST_F(ModuleArchiveCacheFixture, RemovalKeepsThePackOffForTheRestOfTheProcess) {
    auto compressed = compress(makeArchive("content"));
    publish("module", compressed);

    auto cache = openCache();
    cache->scheduleRemoval();
    EXPECT_TRUE(waitUntil([&]() { return !DiskUtils::isDirectory(cache->getGenerationDirectory()); }));
    bool hit = true;
    ASSERT_TRUE(load(*cache, "module", compressed, hit));
    EXPECT_FALSE(hit);
    cache->flushPendingPublishes();
    EXPECT_FALSE(DiskUtils::isDirectory(cache->getGenerationDirectory()));
}

TEST_F(ModuleArchiveCacheFixture, AFullPackKeepsServingItsArchives) {
    auto small = options();
    small.maxPackSize = 200;
    auto a = compress(makeArchive(std::string(100, 'a')));
    auto b = compress(makeArchive(std::string(100, 'b')));
    auto cache = openCache("build1", small);
    bool hit = true;
    ASSERT_TRUE(load(*cache, "a", a, hit));
    ASSERT_TRUE(load(*cache, "b", b, hit)) << "no room left, served from the heap";
    cache->flushPendingPublishes();

    auto next = openCache("build1", small);
    ASSERT_TRUE(load(*next, "a", a, hit));
    EXPECT_TRUE(hit);
    ASSERT_TRUE(load(*next, "b", b, hit));
    EXPECT_FALSE(hit);
    EXPECT_EQ(std::vector<std::string>{"new_build"}, _metrics->getResets()) << "a full pack is not reset";
}

TEST_F(ModuleArchiveCacheFixture, CrashLoopResetIsReportedOncePerRecovery) {
    auto compressed = compress(makeArchive("content"));
    ModuleArchiveCache::disableForBuild(_root.path(), "build1");
    auto countCrashLoops = [&]() {
        auto resets = _metrics->getResets();
        return std::count(resets.begin(), resets.end(), "crash_loop");
    };

    bool hit = true;
    ASSERT_TRUE(load(*openCache(), "module", compressed, hit));
    ASSERT_TRUE(load(*openCache(), "module", compressed, hit));
    EXPECT_EQ(1, countCrashLoops()) << "a later launch of the disabled build reports nothing new";

    for (auto& cache : _caches) {
        cache->close();
    }
    ModuleArchiveCache::disableForBuild(_root.path(), "build1");
    ASSERT_TRUE(load(*openCache(), "module", compressed, hit));
    EXPECT_EQ(2, countCrashLoops());
}

} // namespace ValdiTest
