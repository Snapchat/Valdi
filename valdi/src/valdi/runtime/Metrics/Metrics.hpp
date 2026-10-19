//
//  Metrics.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 4/20/20.
//

#pragma once

#include "utils/base/NonCopyable.hpp"
#include "utils/time/StopWatch.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <chrono>
#include <cstdint>

namespace Valdi {

constexpr size_t kEmitProcessRequestLatencyEntriesThreshold = 20;

class ScopedMetrics;
using MetricsDuration = snap::utils::time::Duration<std::chrono::steady_clock>;

class Metrics : public SimpleRefCountable {
public:
    virtual void emitInitialRenderLatency(const StringBox& module, const MetricsDuration& duration) = 0;

    virtual void emitOnViewModelUpdatedLatency(const StringBox& module, const MetricsDuration& duration) = 0;
    virtual void emitOnCreateLatency(const StringBox& module, const MetricsDuration& duration) = 0;
    virtual void emitOnDestroyLatency(const StringBox& module, const MetricsDuration& duration) = 0;

    virtual void emitDestroyContextLatency(const StringBox& module, const MetricsDuration& duration) = 0;
    virtual void emitCalculateLayoutLatency(const StringBox& module,
                                            const StringBox& backend,
                                            const MetricsDuration& duration) = 0;
    virtual void emitCalculateLazyLayoutLatency(const StringBox& module,
                                                const StringBox& backend,
                                                const MetricsDuration& duration) = 0;
    virtual void emitCalculateLayoutLatencyMeasure(const StringBox& module,
                                                   const StringBox& backend,
                                                   const MetricsDuration& duration) = 0;
    virtual void emitCalculateLazyLayoutLatencyMeasure(const StringBox& module,
                                                       const StringBox& backend,
                                                       const MetricsDuration& duration) = 0;
    virtual void emitProcessRequestLatency(const StringBox& module, const MetricsDuration& duration) = 0;

    virtual void emitSessionTime(const StringBox& module, const MetricsDuration& duration) = 0;

    virtual void emitANR(const StringBox& module) = 0;
    virtual void emitANR() = 0;

    virtual void emitRuntimeManagerInitLatency(const MetricsDuration& duration) = 0;
    virtual void emitRuntimeManagerXpatInitLatency(const MetricsDuration& duration) = 0;
    virtual void emitRuntimeManagerIosInitLatency(const MetricsDuration& duration) = 0;
    virtual void emitRuntimePreInitLatency(const MetricsDuration& duration) = 0;
    virtual void emitRuntimeInitLatency(const MetricsDuration& duration) = 0;
    virtual void emitUserSessionReadyLatency(const MetricsDuration& duration) = 0;
    // Time from RuntimeManager init until setUserSession first attaches a non-empty user session.
    virtual void emitUserSessionAttachLatency(const MetricsDuration& duration) {};

    virtual void emitAssetsDownloadSuccess(const StringBox& module) = 0;
    virtual void emitAssetsDownloadFailure(const StringBox& module) = 0;
    virtual void emitAssetsCacheHit(const StringBox& module) = 0;
    virtual void emitAssetsCacheMiss(const StringBox& module) = 0;

    virtual void emitUncaughtError(const StringBox& module) = 0;
    virtual void emitUncaughtError() = 0;

    virtual void emitOnScrollLatency(const StringBox& module,
                                     const StringBox& backend,
                                     const MetricsDuration& duration) = 0;

    virtual void emitSlowAsyncJsCall(const StringBox& module, const MetricsDuration& duration) = 0;

    virtual void emitSlowSyncJsCallThreshold(const StringBox& module, const MetricsDuration& duration) = 0;

    virtual void emitLoadModuleMemory(const StringBox& module, int64_t totalMemory, int64_t ownMemory) {};
    virtual void emitLoadModuleDuration(const StringBox& module, int64_t totalDuration, int64_t ownDuration) {};
    virtual void emitRunUpdatesInnerTimePercentage(const StringBox& module, int64_t percentage) {};

    // Module archive loads. Each successful load emits exactly one of MmapSuccess (served from the module
    // archive pack) or Heap (decompressed to the heap, including pack misses), plus the latency.
    virtual void emitModuleArchiveMmapSuccess(const StringBox& module) {};
    virtual void emitModuleArchiveHeap(const StringBox& module) {};
    // Wall-clock time spent obtaining a module archive's decompressed content: the pack lookup on a hit,
    // the read and decompression otherwise.
    virtual void emitModuleDecompressLatency(const StringBox& module, const MetricsDuration& duration) {};

    // Module archive pack.
    // A miss whose decompressed archive could not be written or failed hash verification, so it was not
    // added to the pack; the session still uses the heap copy.
    virtual void emitModuleArchiveCachePublishFail(const StringBox& module) {};
    // The archive has no content-hash trailer, so its identity was computed by hashing the compressed bytes.
    virtual void emitModuleArchiveCacheNoContentHash(const StringBox& module) {};
    // The pack was started fresh. `reason` is one of new_build, cap, bad_index, crash_loop.
    virtual void emitModuleArchiveCacheReset(const StringBox& reason) {};
    // An entry served from the pack was corrupt and is banned until the next build.
    virtual void emitModuleArchiveCacheBan(const StringBox& module) {};
    // Size of the pack when the process opened it.
    virtual void emitModuleArchivePackSize(int64_t kilobytes) {};
    // Cache misses of this launch, reported once when module loads first go quiet.
    virtual void emitModuleArchiveLaunchMisses(int64_t misses) {};
    // View pool usage of one ViewNodeTree since the last emission, emitted when the tree is torn down
    // and when the app pauses. hits: views reused from the pool; misses: views created fresh; drops:
    // views from the factory released with pooling disabled via canBeReused (e.g. through a platform
    // accessor that may retain the view). Views the platform refuses to pool are not drops.
    virtual void emitViewPoolStats(const StringBox& module, int64_t hits, int64_t misses, int64_t drops) {};

    // Wall time of one synchronous view-tree inflation pass (ViewNode::updateViewTree),
    // with the node/view counts the pass already computes. The counts let a slow pass be
    // attributed to one expensive platform view (low createdViews) vs a wide inflation
    // (high createdViews) vs a deep no-op traversal (high visitedNodes, low createdViews).
    virtual void emitUpdateViewTreeLatency(const StringBox& module,
                                           const MetricsDuration& duration,
                                           int64_t visitedNodes,
                                           int64_t createdViews) {};

    static ScopedMetrics scopedOnScrollLatency(const Ref<Metrics>& metrics,
                                               const StringBox& module,
                                               const StringBox& backend);
    static ScopedMetrics scopedOnCreateLatency(const Ref<Metrics>& metrics, const StringBox& module);
    static ScopedMetrics scopedOnDestroyLatency(const Ref<Metrics>& metrics, const StringBox& module);
    static ScopedMetrics scopedOnViewModelUpdatedLatency(const Ref<Metrics>& metrics, const StringBox& module);
    static ScopedMetrics scopedCalculateLayoutLatency(const Ref<Metrics>& metrics,
                                                      const StringBox& module,
                                                      const StringBox& backend);
    static ScopedMetrics scopedCalculateLazyLayoutLatency(const Ref<Metrics>& metrics,
                                                          const StringBox& module,
                                                          const StringBox& backend);
    static ScopedMetrics scopedDestroyContextLatency(const Ref<Metrics>& metrics, const StringBox& module);
    static ScopedMetrics thresholdedScopedSlowSyncJsCall(const Ref<Metrics>& metrics, const StringBox& module);
    static ScopedMetrics thresholdedScopedSlowAsyncJsCall(const Ref<Metrics>& metrics, const StringBox& module);
};

using ScopedMetricsCompletion = Valdi::Function<void(const MetricsDuration&)>;

class MetricsStopWatch {
public:
    MetricsStopWatch();
    MetricsDuration elapsed() const;

private:
    snap::utils::time::StopWatch _sw;
};

class ScopedMetrics : public snap::NonCopyable {
public:
    ScopedMetrics();
    explicit ScopedMetrics(ScopedMetricsCompletion&& completionFunction, MetricsDuration threshold = {});
    ~ScopedMetrics();

    MetricsDuration elapsed() const;

private:
    ScopedMetricsCompletion _completionFunction;
    MetricsStopWatch _sw;
    MetricsDuration _threshold;

    friend Metrics;
};

} // namespace Valdi
