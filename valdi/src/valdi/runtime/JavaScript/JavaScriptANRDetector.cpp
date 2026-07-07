//
//  JavaScriptANRDetector.cpp
//  valdi
//
//  Created by Simon Corsin on 07/30/2024.
//  Copyright © 2024 Snap Inc. All rights reserved.
//

#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"

#include "valdi_core/cpp/Threading/DispatchQueue.hpp"

#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi_core/cpp/Utils/ContainerUtils.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"

#include "utils/time/Duration.hpp"
#include <fmt/format.h>

namespace Valdi {

constexpr size_t kDefaultTickIntervalSecond = 1;
constexpr size_t kCaptureStacktraceTimeoutSeconds = 2;

static bool hasRunningStacktrace(const std::vector<JavaScriptCapturedStacktrace>& capturedStacktraces) {
    for (const auto& stacktrace : capturedStacktraces) {
        if (stacktrace.getStatus() == JavaScriptCapturedStacktrace::Status::RUNNING) {
            return true;
        }
    }

    return false;
}

JavaScriptANR::JavaScriptANR(std::string message,
                             std::vector<JavaScriptCapturedStacktrace> capturedStacktraces,
                             const StringBox& moduleName)
    : _message(std::move(message)), _capturedStacktraces(std::move(capturedStacktraces)), _moduleName(moduleName) {}

JavaScriptANR::~JavaScriptANR() = default;

const std::string& JavaScriptANR::getMessage() const {
    return _message;
}

const std::vector<JavaScriptCapturedStacktrace>& JavaScriptANR::getCapturedStacktraces() const {
    return _capturedStacktraces;
}

const StringBox& JavaScriptANR::getModuleName() const {
    return _moduleName;
}

bool JavaScriptANR::hasRunningStacktrace() const {
    return Valdi::hasRunningStacktrace(getCapturedStacktraces());
}

struct EntryToProcess {
    Ref<JavaScriptTaskScheduler> taskScheduler;
    Ref<JavaScriptANRDetector::Entry> entry;
};

JavaScriptANRDetector::JavaScriptANRDetector(const Ref<ILogger>& logger)
    : _logger(logger),
      _dispatchQueue(
          DispatchQueue::create(STRING_LITERAL("com.snap.valdi.ANRDetector"), ThreadQoSClass::ThreadQoSClassNormal)),
      _tickInterval(std::chrono::seconds(kDefaultTickIntervalSecond)) {}

JavaScriptANRDetector::~JavaScriptANRDetector() {
    _dispatchQueue->fullTeardown();
}

void JavaScriptANRDetector::start(std::chrono::steady_clock::duration detectionThreshold) {
    std::lock_guard<Mutex> lock(_mutex);
    _started = true;
    _detectionThreshold = detectionThreshold;
    scheduleNextTick();
}

void JavaScriptANRDetector::stop() {
    std::lock_guard<Mutex> lock(_mutex);
    _started = false;
    cancelNextTick();
}

void JavaScriptANRDetector::onEnterBackground() {
    std::lock_guard<Mutex> lock(_mutex);
    _isInForeground = false;
    cancelNextTick();
}

void JavaScriptANRDetector::onEnterForeground() {
    std::lock_guard<Mutex> lock(_mutex);
    _isInForeground = true;
    scheduleNextTick();
}

void JavaScriptANRDetector::cancelNextTick() {
    if (_nextTickTask != 0) {
        _dispatchQueue->cancel(_nextTickTask);
        _nextTickTask = 0;
    }
}

void JavaScriptANRDetector::setTickInterval(std::chrono::steady_clock::duration tickInterval) {
    std::lock_guard<Mutex> lock(_mutex);
    _tickInterval = tickInterval;
}

void JavaScriptANRDetector::onNextTick(DispatchFunction nextTickCallback) {
    std::lock_guard<Mutex> lock(_mutex);
    _nextTickCallbacks.emplace_back(std::move(nextTickCallback));
}

void JavaScriptANRDetector::scheduleNextTick() {
    if (_nextTickTask != 0 || !_started || !_isInForeground) {
        return;
    }
    _nextTickTask = _dispatchQueue->asyncAfter(
        [self = Valdi::weakRef(this)]() {
            auto strongSelf = self.lock();
            if (strongSelf != nullptr) {
                strongSelf->checkForANRs();
            }
        },
        _tickInterval);
}

bool JavaScriptANRDetector::onANR(JavaScriptTaskScheduler& taskScheduler,
                                  std::chrono::steady_clock::duration detectionThreshold,
                                  const std::atomic<bool>& ack) {
    auto stacktraces = taskScheduler.captureStackTraces(std::chrono::seconds(kCaptureStacktraceTimeoutSeconds));
    StringBox moduleName;
    std::string message;

    if (!stacktraces.empty() && stacktraces[0].getContext() != nullptr) {
        moduleName = stacktraces[0].getContext()->getPath().getResourceId().bundleName;
    }

    if (!hasRunningStacktrace(stacktraces) && ack.load()) {
        // The ack was submitted while waiting to capture stacktraces.
        // We consider that task scheduler has recovered
        return true;
    }

    auto detectionThresholdString =
        snap::utils::time::Duration<std::chrono::steady_clock>(detectionThreshold).toString();
    if (moduleName.isEmpty()) {
        message = fmt::format("Detected unattributed ANR after {}", detectionThresholdString);
    } else {
        message = fmt::format("Detected ANR in '{}' after {}", moduleName, detectionThresholdString);
    }

    if (stacktraces.empty()) {
        message += " but unable to capture stack traces.";
    }

    // getCurrentModuleLoadInfo() reads saved native state, not JS, so it is safe to call while the
    // ANR has the JS thread stuck.
    auto moduleLoadInfo = taskScheduler.getCurrentModuleLoadInfo();
    if (!moduleLoadInfo.empty()) {
        message += " [module-load: " + moduleLoadInfo + "]";
    }

    VALDI_ERROR(*_logger, "{}", message);
    for (const auto& stacktrace : stacktraces) {
        if (stacktrace.getStatus() == JavaScriptCapturedStacktrace::Status::RUNNING) {
            VALDI_ERROR(*_logger, "ANR Stacktrace:\n{}", stacktrace.getStackTrace());
        }
    }

    Ref<Metrics> metrics;
    Ref<IJavaScriptANRDetectorListener> listener;
    {
        std::lock_guard<Mutex> lock(_mutex);
        metrics = _metrics;
        listener = _listener;
    }

    if (metrics != nullptr) {
        if (moduleName.isEmpty()) {
            metrics->emitANR();
        } else {
            metrics->emitANR(moduleName);
        }
    }

    JavaScriptANRBehavior behavior = JavaScriptANRBehavior::KEEP_GOING;

    if (listener != nullptr) {
        behavior = listener->onANR(JavaScriptANR(std::move(message), std::move(stacktraces), moduleName));
    }

    if (behavior == JavaScriptANRBehavior::CRASH) {
        auto context = taskScheduler.getLastDispatchedContext();
        if (context != nullptr) {
            context->withAttribution([]() { std::abort(); });
        } else {
            std::abort();
        }
    }

    return false;
}

void JavaScriptANRDetector::checkForANRs() {
    auto timepoint = std::chrono::steady_clock::now();
    std::vector<EntryToProcess> entriesWithANRToProcess;
    std::chrono::steady_clock::duration detectionThreshold;

    {
        std::lock_guard<Mutex> lock(_mutex);
        detectionThreshold = _detectionThreshold;
        for (const auto& entry : _entries) {
            if (entry->didANR) {
                continue;
            }

            if (!entry->synScheduledTime || entry->ack) {
                entry->synScheduledTime = timepoint;
                entry->ack = false;
                entry->taskScheduler->dispatchOnJsThreadAsync(nullptr,
                                                              [entry](const auto& /*jsEntry*/) { entry->ack = true; });
            } else if (entry->synScheduledTime.value() + detectionThreshold < timepoint) {
                entry->didANR = true;
                auto& entryToProcess = entriesWithANRToProcess.emplace_back();
                entryToProcess.entry = entry;
                entryToProcess.taskScheduler = Ref(entry->taskScheduler);
            } else if (_nudgeEnabled) {
                entry->taskScheduler->dispatchOnJsThreadAsync(nullptr, [](const auto& /*jsEntry*/) {});
            }
        }
    }

    for (const auto& entryWithANR : entriesWithANRToProcess) {
        bool recovered = onANR(*entryWithANR.taskScheduler, detectionThreshold, entryWithANR.entry->ack);

        if (recovered) {
            std::lock_guard<Mutex> lock(_mutex);
            entryWithANR.entry->didANR = false;
        }
    }

    std::vector<DispatchFunction> nextTickCallbacks;
    {
        std::lock_guard<Mutex> lock(_mutex);
        nextTickCallbacks = std::move(_nextTickCallbacks);
        _nextTickTask = 0;
        scheduleNextTick();
    }

    for (const auto& cb : nextTickCallbacks) {
        cb();
    }
}

void JavaScriptANRDetector::appendTaskScheduler(JavaScriptTaskScheduler* taskScheduler) {
    std::lock_guard<Mutex> lock(_mutex);
    auto entry = makeShared<JavaScriptANRDetector::Entry>();
    entry->taskScheduler = taskScheduler;
    _entries.emplace_back(std::move(entry));
}

void JavaScriptANRDetector::removeTaskScheduler(JavaScriptTaskScheduler* taskScheduler) {
    std::lock_guard<Mutex> lock(_mutex);
    eraseIf(_entries, [&](const auto& entry) { return entry->taskScheduler == taskScheduler; });
}

void JavaScriptANRDetector::setListener(const Ref<IJavaScriptANRDetectorListener>& listener) {
    std::lock_guard<Mutex> lock(_mutex);
    _listener = listener;
}

Ref<IJavaScriptANRDetectorListener> JavaScriptANRDetector::getListener() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _listener;
}

void JavaScriptANRDetector::setMetrics(const Ref<Metrics>& metrics) {
    std::lock_guard<Mutex> lock(_mutex);
    _metrics = metrics;
}

void JavaScriptANRDetector::setNudgeEnabled(bool nudgeEnabled) {
    std::lock_guard<Mutex> lock(_mutex);
    _nudgeEnabled = nudgeEnabled;
}

} // namespace Valdi
