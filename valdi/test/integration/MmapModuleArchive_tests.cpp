//
//  MmapModuleArchive_tests.cpp
//
//  Integration tests for loading module archives through ResourceManager with the module archive pack on
//  and off. Each successful load emits exactly one of Module_Archive_Mmap_Success (served from the pack) or
//  Module_Archive_Heap, alongside Module_Decompress_Latency.
//

#include "RuntimeTestsUtils.hpp"

#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

constexpr auto kPackTweakKey = "VALDI_MMAP_MODULE_ARCHIVES_CACHE_REUSE";
constexpr auto kModuleName = "test";

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

// Tweak provider that exposes the module archive pack switch read by ValdiRuntimeTweaks.
class MmapTweakProvider : public SharedPtrRefCountable,
                          public snap::valdi_core::ModuleFactory,
                          public ITweakValueProvider {
public:
    void setPackEnabled(bool enabled) {
        _config.setMapValue(kPackTweakKey, Value(static_cast<bool>(enabled)));
    }

    StringBox getModulePath() override {
        return STRING_LITERAL("Tweak");
    }
    Value loadModule() override {
        return Value();
    }
    StringBox getString(const StringBox& key, const StringBox&) override {
        return _config.getMapValue(key).toStringBox();
    }
    bool getBool(const StringBox& key, bool) override {
        return _config.getMapValue(key).toBool();
    }
    float getFloat(const StringBox& key, float) override {
        return _config.getMapValue(key).toFloat();
    }
    int32_t getInt(const StringBox& key, int32_t) override {
        return _config.getMapValue(key).toInt();
    }
    Value getBinary(const StringBox& key, const Value&) override {
        return _config.getMapValue(key);
    }

private:
    Value _config;
};

// Records the module archive load counters. All other Metrics methods are no-ops.
class RecordingMetrics : public Metrics {
public:
    std::atomic<int> mmapSuccess{0};
    std::atomic<int> heap{0};
    std::atomic<int> decompressLatencyCalls{0};

    void emitModuleArchiveMmapSuccess(const StringBox& modulePath) override {
        ++mmapSuccess;
        recordModule(_mmapModules, modulePath);
    }
    void emitModuleArchiveHeap(const StringBox& modulePath) override {
        ++heap;
        recordModule(_heapModules, modulePath);
    }
    void emitModuleDecompressLatency(const StringBox&, const MetricsDuration&) override {
        ++decompressLatencyCalls;
    }

    // A module load can pull in dependency bundles, so counter totals alone can't tell which module took
    // which path.
    bool moduleTookHeapPath(const char* module) const {
        std::lock_guard<std::mutex> lock(_modulesMutex);
        return std::find(_heapModules.begin(), _heapModules.end(), module) != _heapModules.end();
    }
    bool moduleTookMmapPath(const char* module) const {
        std::lock_guard<std::mutex> lock(_modulesMutex);
        return std::find(_mmapModules.begin(), _mmapModules.end(), module) != _mmapModules.end();
    }

    int totalPathCounters() const {
        return mmapSuccess + heap;
    }

private:
    void recordModule(std::vector<std::string>& modules, const StringBox& modulePath) {
        std::lock_guard<std::mutex> lock(_modulesMutex);
        modules.emplace_back(modulePath.slowToString());
    }

    mutable std::mutex _modulesMutex;
    std::vector<std::string> _heapModules;
    std::vector<std::string> _mmapModules;

public:
    // ---- No-op overrides for the remaining pure-virtuals. -------------------
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

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        char dir[] = "/tmp/.valdi_mmap_integ.XXXXXX";
        if (mkdtemp(dir) == nullptr) {
            throw Exception(STRING_FORMAT("mkdtemp failed: {}", strerror(errno)));
        }
        _path = STRING_LITERAL(dir);
    }
    ~TemporaryDirectory() {
        DiskUtils::remove(Path(_path.toStringView()));
    }
    Path path() const {
        return Path(_path.toStringView());
    }

private:
    StringBox _path;
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class MmapModuleArchiveFixture : public ::testing::Test {
protected:
    void SetUp() override {
        _tweakProvider = makeShared<MmapTweakProvider>();
        _metrics = makeShared<RecordingMetrics>();
    }

    void TearDown() override {
        if (_wrapper) {
            _wrapper->teardown();
            _wrapper.reset();
        }
        ModuleArchiveCache::resetSharedInstancesForTesting();
    }

    // Lazily construct the wrapper so the test can set the tweak before the
    // runtime queries it during module load. Uses a real JS bridge because
    // ResourceManager::loadModuleAsync runs JS initialization downstream of
    // the decompress path we're trying to exercise.
    RuntimeWrapper& makeWrapper(bool enablePack) {
        _tweakProvider->setPackEnabled(enablePack);
        auto* jsBridge = Valdi::JavaScriptBridge::get(snap::valdi_core::JavaScriptEngineType::QuickJS);
        _wrapper = std::make_unique<RuntimeWrapper>(jsBridge, TSNMode::Disabled);
        _wrapper->runtime->setMetrics(_metrics);
        _wrapper->runtime->setRuntimeTweaks(makeShared<ValdiRuntimeTweaks>(_tweakProvider.toShared()));
        return *_wrapper;
    }

    // Stands in for the end of a process: publishes what this launch missed and closes the pack.
    void endLaunch() {
        _wrapper->runtime->getResourceManager().flushPendingArchiveCachePublishes();
        _wrapper->teardown();
        _wrapper.reset();
        ModuleArchiveCache::resetSharedInstancesForTesting();
    }

    static Path generationDirectory(const TemporaryDirectory& tmp) {
        auto directory = ModuleArchiveCache::getShared(tmp.path(), "test")->getGenerationDirectory();
        ModuleArchiveCache::resetSharedInstancesForTesting();
        return directory;
    }

    // False when nothing was appended, e.g. because the test module isn't ZStd-compressed in this build.
    static bool packHasData(const Path& generation) {
        struct stat st{};
        auto dataPath = generation.appending(std::string_view("data")).toString();
        return stat(dataPath.c_str(), &st) == 0 && st.st_size > 0;
    }

    Ref<MmapTweakProvider> _tweakProvider;
    Ref<RecordingMetrics> _metrics;
    std::unique_ptr<RuntimeWrapper> _wrapper;
};

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(MmapModuleArchiveFixture, EveryLoadTakesTheHeapWhenThePackIsOff) {
    TemporaryDirectory tmp;
    auto& wrapper = makeWrapper(/*enablePack=*/false);
    wrapper.runtime->setMmapCacheDirectory(tmp.path(), "test");

    auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    EXPECT_EQ(0, _metrics->mmapSuccess.load());
    EXPECT_GE(_metrics->heap.load(), 1);
    EXPECT_EQ(_metrics->heap.load(), _metrics->decompressLatencyCalls.load())
        << "Latency timer should fire once per successful load";
}

TEST_F(MmapModuleArchiveFixture, EveryLoadTakesTheHeapWithoutACacheDirectory) {
    auto& wrapper = makeWrapper(/*enablePack=*/true);

    auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    EXPECT_EQ(0, _metrics->mmapSuccess.load());
    EXPECT_GE(_metrics->heap.load(), 1);
    EXPECT_EQ(_metrics->heap.load(), _metrics->decompressLatencyCalls.load());
}

// The first launch misses, is served from the heap and appends the archive to the pack; the next launch
// serves it from the pack instead of decompressing.
TEST_F(MmapModuleArchiveFixture, PackServesTheNextLaunch) {
    TemporaryDirectory tmp;
    {
        auto& wrapper = makeWrapper(/*enablePack=*/true);
        wrapper.runtime->setMmapCacheDirectory(tmp.path(), "test");
        auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
        ASSERT_TRUE(result) << result.description();
        EXPECT_EQ(0, _metrics->mmapSuccess.load()) << "a new pack has nothing to serve";
        EXPECT_TRUE(_metrics->moduleTookHeapPath(kModuleName));
        endLaunch();
    }
    if (!packHasData(generationDirectory(tmp))) {
        GTEST_SKIP() << "Test module isn't ZStd-compressed in this build; the pack is not exercised";
    }
    auto heapLoadsBefore = _metrics->heap.load();
    auto pathCountersBefore = _metrics->totalPathCounters();
    auto latencyCallsBefore = _metrics->decompressLatencyCalls.load();

    auto& nextLaunch = makeWrapper(/*enablePack=*/true);
    nextLaunch.runtime->setMmapCacheDirectory(tmp.path(), "test");
    auto result = nextLaunch.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    EXPECT_TRUE(_metrics->moduleTookMmapPath(kModuleName));
    EXPECT_EQ(heapLoadsBefore, _metrics->heap.load());
    EXPECT_EQ(_metrics->totalPathCounters() - pathCountersBefore,
              _metrics->decompressLatencyCalls.load() - latencyCallsBefore);
}

// Turning the pack off deletes it, so its files don't outlive the experiment.
TEST_F(MmapModuleArchiveFixture, PackIsDeletedWhenTurnedOff) {
    TemporaryDirectory tmp;
    {
        auto& wrapper = makeWrapper(/*enablePack=*/true);
        wrapper.runtime->setMmapCacheDirectory(tmp.path(), "test");
        ASSERT_TRUE(wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources));
        endLaunch();
    }
    auto generation = generationDirectory(tmp);
    if (!packHasData(generation)) {
        GTEST_SKIP() << "Test module isn't ZStd-compressed in this build; the pack is not exercised";
    }

    // The runtime picks up this shared instance; without the delay the test does not wait for the default.
    ModuleArchiveCache::Options immediate;
    immediate.removalDelay = std::chrono::milliseconds(0);
    auto cache = ModuleArchiveCache::getShared(tmp.path(), "test", immediate);
    auto mmapLoadsBefore = _metrics->mmapSuccess.load();
    auto heapLoadsBefore = _metrics->heap.load();
    auto& wrapper = makeWrapper(/*enablePack=*/false);
    wrapper.runtime->setMmapCacheDirectory(tmp.path(), "test");
    ASSERT_TRUE(wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources));
    EXPECT_EQ(mmapLoadsBefore, _metrics->mmapSuccess.load()) << "the pack is off, so nothing is served from it";
    EXPECT_GT(_metrics->heap.load(), heapLoadsBefore);

    for (int i = 0; i < 100 && DiskUtils::isDirectory(generation); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_FALSE(DiskUtils::isDirectory(generation));
}

} // namespace ValdiTest
