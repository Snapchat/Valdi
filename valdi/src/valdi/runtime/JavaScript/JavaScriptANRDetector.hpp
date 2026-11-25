//
//  JavaScriptANRDetector.hpp
//  valdi
//
//  Created by Simon Corsin on 07/30/2024.
//  Copyright © 2024 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/JavaScript/JavaScriptANRSampler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <memory>
#include <vector>

namespace Valdi {

class DispatchQueue;
class ILogger;
class Metrics;

class JavaScriptANR {
public:
    JavaScriptANR(std::string message,
                  std::vector<JavaScriptCapturedStacktrace> capturedStacktraces,
                  const StringBox& moduleName);
    ~JavaScriptANR();

    const std::string& getMessage() const;
    const std::vector<JavaScriptCapturedStacktrace>& getCapturedStacktraces() const;
    const StringBox& getModuleName() const;

    bool hasRunningStacktrace() const;

private:
    std::string _message;
    std::vector<JavaScriptCapturedStacktrace> _capturedStacktraces;
    StringBox _moduleName;
};

enum class JavaScriptANRBehavior {
    KEEP_GOING,
    CRASH,
};

class IJavaScriptANRDetectorListener : public SimpleRefCountable {
public:
    virtual JavaScriptANRBehavior onANR(JavaScriptANR anr) = 0;
};

class JavaScriptANRDetector : public SharedPtrRefCountable {
public:
    explicit JavaScriptANRDetector(const Ref<ILogger>& logger);
    ~JavaScriptANRDetector() override;

    void start(std::chrono::steady_clock::duration detectionThreshold);
    void stop();

    void onEnterBackground();
    void onEnterForeground();

    void setTickInterval(std::chrono::steady_clock::duration tickInterval);
    void onNextTick(DispatchFunction nextTickCallback);

    void appendTaskScheduler(JavaScriptTaskScheduler* taskScheduler);
    void removeTaskScheduler(JavaScriptTaskScheduler* taskScheduler);

    void setListener(const Ref<IJavaScriptANRDetectorListener>& listener);
    Ref<IJavaScriptANRDetectorListener> getListener() const;
    void setMetrics(const Ref<Metrics>& metrics);

    void setNudgeEnabled(bool nudgeEnabled);
    void setSamplingConfig(const JavaScriptANRSamplingConfig& samplingConfig);

    struct Entry : public SimpleRefCountable {
        JavaScriptTaskScheduler* taskScheduler = nullptr;
        std::atomic<bool> ack = 0;
        std::optional<std::chrono::steady_clock::time_point> synScheduledTime;
        bool didANR = false;
        // Samples the scheduler's attribution while its syn waits past a tick. Reset with each new syn.
        std::unique_ptr<JavaScriptANRSampler> sampler;
    };

private:
    mutable Mutex _mutex;
    Ref<ILogger> _logger;
    std::vector<Ref<Entry>> _entries;
    std::vector<DispatchFunction> _nextTickCallbacks;
    Ref<DispatchQueue> _dispatchQueue;
    Ref<Metrics> _metrics;
    Ref<IJavaScriptANRDetectorListener> _listener;
    std::chrono::steady_clock::duration _detectionThreshold;
    std::chrono::steady_clock::duration _tickInterval;
    JavaScriptANRSamplingConfig _samplingConfig;
    task_id_t _nextTickTask = 0;
    task_id_t _nextSampleTask = 0;
    bool _isInForeground = false;
    bool _started = false;
    bool _nudgeEnabled = false;

    void checkForANRs();
    void cancelNextTick();
    void scheduleNextTick();
    void rearmPendingSyns();

    void startSampling(Entry& entry, std::chrono::steady_clock::time_point now);
    void sampleWaitingEntries();
    void scheduleNextSample();
    void cancelNextSample();

    bool onANR(JavaScriptTaskScheduler& taskScheduler,
               std::chrono::steady_clock::duration detectionThreshold,
               const std::atomic<bool>& ack,
               const JavaScriptANRSampler* sampler);
};

} // namespace Valdi
