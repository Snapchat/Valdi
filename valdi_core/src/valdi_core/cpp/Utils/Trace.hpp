//
//  Trace.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 6/25/19.
//

#pragma once

#include "utils/debugging/Trace.hpp"
#include "utils/time/StopWatch.hpp"
#include "valdi_core/cpp/Threading/ThreadBase.hpp"
#include "valdi_core/cpp/Utils/Defer.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"

#include <atomic>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

namespace Valdi {

class StringBox;

std::string getTraceName(std::string_view prefix, std::string_view suffix);
std::string getTraceName(std::string_view prefix, const StringBox& suffix);

using TraceDuration = snap::utils::time::Duration<std::chrono::steady_clock>;
using TraceTimePoint = std::chrono::steady_clock::time_point;

struct RecordedTrace {
    inline RecordedTrace(std::string&& trace,
                         const TraceTimePoint& start,
                         const TraceTimePoint& end,
                         const ThreadId& threadId,
                         size_t recordingSequence)
        : trace(std::move(trace)), start(start), end(end), threadId(threadId), recordingSequence(recordingSequence) {}

    std::string trace;
    TraceTimePoint start;
    TraceTimePoint end;
    ThreadId threadId;
    size_t recordingSequence;

    TraceDuration duration() const;
};

class Tracer {
public:
    Tracer();
    ~Tracer();

    inline bool isRecording() const {
        return _recording;
    }

    size_t startRecording();
    std::vector<RecordedTrace> stopRecording(size_t recordingIdentifier);

    void append(std::string&& trace, const TraceTimePoint& start, const TraceTimePoint& end);

    static Tracer& shared();

private:
    std::mutex _mutex;
    std::atomic_bool _recording = false;
    size_t _recordingSequence = 0;
    std::vector<RecordedTrace> _pendingTraces;
    std::vector<size_t> _recorders;
};

class ScopedTrace {
public:
    /// Names that are string literals are carried as a view, so no owning string is built. Matching
    /// a fixed-size char array keeps this off dynamic strings, which need to be owned.
    template<size_t kSize>
    explicit ScopedTrace(const char (&trace)[kSize]) : _trace(trace, kSize - 1), _snapTrace(_trace) {
        start();
    }

    /// A mutable buffer can be reassigned or freed while the span is open, so it has to go through
    /// the owning overload instead.
    template<size_t kSize>
    explicit ScopedTrace(char (&trace)[kSize]) = delete;

    explicit ScopedTrace(const StringBox& trace);
    explicit ScopedTrace(std::string&& trace);
    ~ScopedTrace();

protected:
    /// Empty unless the name had to be copied, in which case _trace views into it.
    std::string _ownedTrace;
    std::string_view _trace;
    std::optional<TraceTimePoint> _startTime;
    snap::utils::debugging::ScopedTrace _snapTrace;
    snap::profiling::OsTraceEmitter _osEmitter;

private:
    void start();
    void begin();
    void end();
};

/// True while a span can still reach a consumer: an installed TraceSDK sink, an active Valdi
/// recording, or a dev build, where the OS trace emitters are compiled in. Short-circuits to a
/// constant on dev builds, so only production evaluates the two atomic loads.
inline bool isTracingActive() {
    return snap::kIsDevBuild ||
           std::atomic_load_explicit(&snap::profiling::scopedTraceSupportInstance, std::memory_order_relaxed) !=
               nullptr ||
           Tracer::shared().isRecording();
}

} // namespace Valdi

#if !SC_TRACING_COMPILED_IN()

#define VALDI_TRACE(name)
#define VALDI_TRACE_META(name, meta)

#else

// The `meta` expression is only evaluated when something is listening, so callers can pass a name
// that costs real work to build (see JSFunctionWithValueFunction::getFunctionName).
#define VALDI_TRACE(name)                                                                                              \
    std::optional<Valdi::ScopedTrace> ___scopedTrace;                                                                  \
    if (Valdi::isTracingActive()) {                                                                                    \
        ___scopedTrace.emplace(name);                                                                                  \
    }
#define VALDI_TRACE_META(name, meta)                                                                                   \
    std::optional<Valdi::ScopedTrace> ___scopedTrace;                                                                  \
    if (Valdi::isTracingActive()) {                                                                                    \
        ___scopedTrace.emplace(Valdi::getTraceName(name, meta));                                                       \
    }

#endif

namespace Valdi {
constexpr auto kTracingEnabled = snap::kTracingEnabled;
}
