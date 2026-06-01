//
//  MmapModuleArchive_tests.cpp
//
//  Integration tests for the mmap module archive A/B telemetry path.
//  Verifies that ResourceManager dispatches to the mmap-backed decompressor
//  when the COF is on and a cache directory is configured, and falls back
//  to the heap decompressor in all other cases. Exactly one of the three
//  Module_Archive_* counters must be emitted per archive load, and the
//  Module_Decompress_Latency timer must be emitted alongside it.
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

#include <atomic>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <stdlib.h>
#include <string>

using namespace Valdi;

namespace ValdiTest {

namespace {

constexpr auto kTweakKey = "VALDI_ENABLE_MMAP_MODULE_ARCHIVES";
constexpr auto kModuleName = "test";

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

// Tweak provider that exposes a single boolean COF used by ValdiRuntimeTweaks.
class MmapTweakProvider : public SharedPtrRefCountable,
                          public snap::valdi_core::ModuleFactory,
                          public ITweakValueProvider {
public:
    void setEnabled(bool enabled) {
        _config.setMapValue(kTweakKey, Value(static_cast<bool>(enabled)));
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

// Metrics implementation that records counter / timer calls for the four
// metrics added in this PR. All other Metrics methods are no-ops; gtest
// fails loudly via SC_ASSERT if a pure-virtual were missed.
class RecordingMetrics : public Metrics {
public:
    std::atomic<int> mmapSuccess{0};
    std::atomic<int> mmapFallback{0};
    std::atomic<int> heap{0};
    std::atomic<int> decompressLatencyCalls{0};
    std::atomic<int> mmapPublishFail{0};

    void emitModuleArchiveMmapSuccess(const StringBox&) override {
        ++mmapSuccess;
    }
    void emitModuleArchiveMmapFallback(const StringBox&) override {
        ++mmapFallback;
    }
    void emitModuleArchiveHeap(const StringBox&) override {
        ++heap;
    }
    void emitModuleDecompressLatency(const StringBox&, const MetricsDuration&) override {
        ++decompressLatencyCalls;
    }
    void emitModuleArchiveMmapPublishFail(const StringBox&) override {
        ++mmapPublishFail;
    }

    int totalPathCounters() const {
        return mmapSuccess + mmapFallback + heap;
    }

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
    }

    // Lazily construct the wrapper so the test can set the COF before the
    // runtime queries it during module load. Uses a real JS bridge because
    // ResourceManager::loadModuleAsync runs JS initialization downstream of
    // the decompress path we're trying to exercise.
    RuntimeWrapper& makeWrapper(bool enableCof) {
        _tweakProvider->setEnabled(enableCof);
        auto* jsBridge = Valdi::JavaScriptBridge::get(snap::valdi_core::JavaScriptEngineType::QuickJS);
        _wrapper = std::make_unique<RuntimeWrapper>(jsBridge, TSNMode::Disabled);
        _wrapper->runtime->setMetrics(_metrics);
        _wrapper->runtime->setRuntimeTweaks(makeShared<ValdiRuntimeTweaks>(_tweakProvider.toShared()));
        return *_wrapper;
    }

    Ref<MmapTweakProvider> _tweakProvider;
    Ref<RecordingMetrics> _metrics;
    std::unique_ptr<RuntimeWrapper> _wrapper;
};

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// COF off → heap counter fires, mmap counters do not, latency timer fires.
TEST_F(MmapModuleArchiveFixture, EmitsHeapCounterWhenCofDisabled) {
    auto& wrapper = makeWrapper(/*enableCof=*/false);
    // Cache dir intentionally NOT set; with COF off it doesn't matter anyway.

    auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    EXPECT_EQ(0, _metrics->mmapSuccess.load());
    EXPECT_EQ(0, _metrics->mmapFallback.load());
    EXPECT_GE(_metrics->heap.load(), 1);
    EXPECT_EQ(_metrics->heap.load(), _metrics->totalPathCounters()) << "Exactly the heap counter should have fired";
    EXPECT_EQ(_metrics->heap.load(), _metrics->decompressLatencyCalls.load())
        << "Latency timer should fire once per successful decompress";
    EXPECT_EQ(0, _metrics->mmapPublishFail.load()) << "Publish-failure counter is for the mmap path only";
}

// COF on but no cache dir → ResourceManager short-circuits to the heap path
// (because _mmapCacheDirectory is empty), so heap counter fires.
TEST_F(MmapModuleArchiveFixture, EmitsHeapCounterWhenCofOnButNoCacheDir) {
    auto& wrapper = makeWrapper(/*enableCof=*/true);
    // Deliberately do NOT call setMmapCacheDirectory.

    auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    EXPECT_EQ(0, _metrics->mmapSuccess.load());
    EXPECT_EQ(0, _metrics->mmapFallback.load());
    EXPECT_GE(_metrics->heap.load(), 1);
    EXPECT_EQ(_metrics->heap.load(), _metrics->decompressLatencyCalls.load());
    EXPECT_EQ(0, _metrics->mmapPublishFail.load());
}

// COF on + cache dir set → mmap path is dispatched. We don't assert which
// arm wins (success vs fallback) because that depends on whether the test
// module artifact is ZStd-compressed by the build pipeline; we only require
// that the heap counter is NOT used and that exactly one mmap counter fires
// per decompress, alongside the latency timer.
TEST_F(MmapModuleArchiveFixture, DispatchesToMmapPathWhenCofOnAndCacheDirSet) {
    TemporaryDirectory tmp;
    auto& wrapper = makeWrapper(/*enableCof=*/true);
    wrapper.runtime->setMmapCacheDirectory(tmp.path());

    auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    const int mmapTotal = _metrics->mmapSuccess.load() + _metrics->mmapFallback.load();
    EXPECT_GE(mmapTotal, 1) << "At least one mmap-path counter should fire";
    EXPECT_EQ(0, _metrics->heap.load()) << "Heap counter must not fire when COF is on and cache dir is set";
    EXPECT_EQ(mmapTotal, _metrics->totalPathCounters()) << "Only mmap counters should fire";
    EXPECT_EQ(mmapTotal, _metrics->decompressLatencyCalls.load()) << "Latency timer should fire once per decompress";
    // Happy-path expectation: nothing should report a publish failure when
    // tmp file and cache dir live in the same writable filesystem.
    EXPECT_EQ(0, _metrics->mmapPublishFail.load());
}

// COF on + cache dir set + mmap success → file backing must exist at the
// expected hashed-flat path under the cache directory. Only meaningful when
// the test module artifact is ZStd-compressed; if it isn't, this test
// degenerates to a no-op assertion (skip via GTEST_SKIP).
TEST_F(MmapModuleArchiveFixture, MmapSuccessProducesFileInCacheDir) {
    TemporaryDirectory tmp;
    auto& wrapper = makeWrapper(/*enableCof=*/true);
    wrapper.runtime->setMmapCacheDirectory(tmp.path());

    auto result = wrapper.loadModule(STRING_LITERAL(kModuleName), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();

    if (_metrics->mmapSuccess.load() == 0) {
        GTEST_SKIP() << "Test module isn't ZStd-compressed in this build; mmap_success "
                        "path not exercised. Counters: success="
                     << _metrics->mmapSuccess.load() << " fallback=" << _metrics->mmapFallback.load()
                     << " heap=" << _metrics->heap.load();
    }

    // At least one file should exist in the cache dir. We don't open or compare
    // bytes — the unit tests in ZStdUtils_tests.cpp cover the contents and the
    // atomic rename; here we just verify dispatch produced an on-disk artifact.
    auto pathStr = tmp.path().toString();
    DIR* d = opendir(pathStr.c_str());
    ASSERT_NE(nullptr, d) << "Cache dir vanished: " << pathStr;
    int regularFiles = 0;
    while (auto* entry = readdir(d)) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        ++regularFiles;
    }
    closedir(d);
    EXPECT_GE(regularFiles, 1) << "Expected the rename target to leave a file in the cache dir";
}

} // namespace ValdiTest
