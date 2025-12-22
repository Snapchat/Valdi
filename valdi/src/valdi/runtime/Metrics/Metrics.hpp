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
    virtual void emitRuntimeInitLatency(const MetricsDuration& duration) = 0;
    virtual void emitUserSessionReadyLatency(const MetricsDuration& duration) = 0;

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
