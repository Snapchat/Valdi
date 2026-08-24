#pragma once

#include "utils/debugging/Assert.hpp"
#include "utils/platform/TargetPlatform.hpp"
#include "utils/time/BoottimeClock.hpp"
#include "utils/time/UptimeClock.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <type_traits>

namespace snap::profiling {

#if defined(__APPLE__) || defined(SC_WASM)
// align with iOS mach_absolute_time. Also use uptime for wasm, as boot-time is not supported in browsers.
using TraceClock = utils::time::UptimeClock;
#else
// match Android for elapsedRealtime.
using TraceClock = utils::time::BoottimeClock;
#endif

// Raise this above the default 0 to allow only traces initialized with greater
// levels to pass
extern std::atomic<int> gTraceLevel;

// -------- Parameter types

struct TraceBegin { // used by the begin() method of emitters
    std::string_view name;
    TraceClock::time_point startTs;
};

struct TraceEnd { // used by the end() method of emitters
    std::string_view name;
    TraceClock::time_point startTs;
    TraceClock::time_point endTs;
};

template<typename T>
struct TraceStep {
    std::string_view name;
    T step;
    TraceClock::time_point ts;
};

// -------- Enum trace types/steps to string
// Client code to overload with custom enum types
// By default enums are converted to integers
template<typename T>
std::enable_if_t<std::is_enum_v<T>, std::string> toString(const T& e) {
    return std::to_string(static_cast<int>(e));
}

// -------- Emitter classes

template<typename EnumT, size_t kSteps>
class LatencyRecorder {
public:
    void begin(const TraceBegin& traceBegin) {
        _steps[0] = traceBegin.startTs;
    }
    void step(const TraceStep<EnumT>& traceStep) {
        SC_ASSERT(static_cast<size_t>(traceStep.step) < kSteps);
        _steps[static_cast<size_t>(traceStep.step) + 1] = traceStep.ts;
    }
    void end(const TraceEnd& traceEnd) {
        _steps[kSteps + 1] = traceEnd.endTs;
    }
    TraceClock::duration stepLatency(EnumT step) const {
        SC_ASSERT(static_cast<size_t>(step) < kSteps);
        auto tsBegin = _steps[static_cast<size_t>(step)];
        auto tsEnd = _steps[static_cast<size_t>(step) + 1];
        return tsEnd - tsBegin;
    }

private:
    TraceClock::time_point _steps[kSteps + 2]; // add begin/end steps
};

class NullEmitter {
public:
    void begin(const TraceBegin& /*traceBegin*/) {}
    void end(const TraceEnd& /*traceEnd*/) {}
};

#if defined(SC_ANDROID)
// Wraps ATrace_beginSection
class ATraceSectionEmitter {
public:
    ATraceSectionEmitter() = default;
    ATraceSectionEmitter(ATraceSectionEmitter&&) = delete;

    void begin(const TraceBegin& traceBegin);
    void end(const TraceEnd& traceEnd);
};

// Wraps ATrace_beginAsyncSection
class ATraceAsyncSectionEmitter {
public:
    ATraceAsyncSectionEmitter();

    void begin(const TraceBegin& traceBegin);
    void end(const TraceEnd& traceEnd);

protected:
    int32_t _cookie = 0;
};
#endif

#if defined(SC_IOS)
// Wraps os_signpost_interval_begin
class IosSignpostEmitter {
public:
    IosSignpostEmitter();

    void begin(const TraceBegin& traceBegin);
    void step(const TraceStep<std::string_view>& traceStep);
    // if called with types other than string_view, expect a toString overload
    // that converts it to string.
    template<typename T>
    void step(const TraceStep<T>& traceStep) {
        step({traceStep.name, toString(traceStep.step), traceStep.ts});
    }
    void end(const TraceEnd& traceEnd);

protected:
    uint64_t _signpostId = 0;
};
#endif

// -------- Trace driver class

template<typename... T>
class TraceDriver : private T... {
    using Expander = int[];

    // Check if emitter has a step() method callable with `TraceStep<StepType>`
    template<typename U, typename StepType, typename = void>
    struct HasSteps : std::false_type {};

    template<typename U, typename StepType>
    struct HasSteps<U, StepType, decltype(std::declval<U>().step(std::declval<TraceStep<StepType>>()))>
        : std::true_type {};

    // Call the emitter's step() method when it's available
    template<typename U, typename StepType>
    void emitStep(const TraceStep<StepType>& traceStep) {
        if constexpr (HasSteps<U, StepType>::value) {
            U::step(traceStep);
        }
    }

public:
    // The goal is to avoid allocation and copying when the parameter is a
    // string literal. This is done by matching a char array with static
    // size. It's probably not 100% bullet proof, but at least prevents most
    // const char* and std::string.
    template<size_t kSize>
    explicit TraceDriver(const char (&name)[kSize], int level = 0) noexcept : _name(name, kSize - 1), _level(level) {
        begin();
    }

    template<size_t kSize>
    explicit TraceDriver(char (&name)[kSize], int level = 0) = delete;

    // If the caller passes a dynamic string, then we have no choice but copy
    // the string. We have to support this use case because it's quite common in
    // Snap codebase.
    explicit TraceDriver(std::string name, int level = 0) noexcept
        : _nameStorage(std::move(name)), _name(_nameStorage), _level(level) {
        begin();
    }

    TraceDriver(const TraceDriver&) = delete; // no copy

    TraceDriver(TraceDriver&& other) noexcept // support move
        : T(std::move(other))...,
          _ended(other._ended),
          _startTs(other._startTs),
          _endTs(other._endTs),
          _level(other._level) {
        other._ended = true;
        if (other._name.data() == other._nameStorage.data()) {
            _nameStorage = std::move(other._nameStorage);
            _name = _nameStorage;
        } else {
            _name = other._name;
        }
    }

    ~TraceDriver() {
        end();
    }

    // Forward to `end()` methods on emitters
    void end() noexcept {
        if constexpr (sizeof...(T) > 0) {
            if (!_ended) {
                _ended = true;
                _endTs = TraceClock::now();
                (void)Expander{(T::end({_name, _startTs, _endTs}), 0)...};
            }
        }
    }

    // Forward to `step()` methods on emitters
    template<typename StepType>
    void step(StepType step) {
        if constexpr (sizeof...(T) > 0) {
            if (!_ended) {
                auto ts = TraceClock::now();
                (void)Expander{(emitStep<T, StepType>({_name, step, ts}), 0)...};
            }
        }
    }

    TraceClock::duration latency() const {
        SC_ASSERT(_ended);
        return _endTs - _startTs;
    }

    // Return the result from the first emitter that implements a `stepLatency()` method
    template<typename StepType>
    auto stepLatency(StepType step) const {
        return stepLatencyImpl<StepType, T...>(step, 0);
    }

private:
    bool _ended = false;
    std::string _nameStorage;
    std::string_view _name;
    TraceClock::time_point _startTs;
    TraceClock::time_point _endTs;
    const int _level;

    // Forward to `begin()` methods on emitters
    void begin() noexcept {
        if constexpr (sizeof...(T) > 0) {
            if (_level >= gTraceLevel.load(std::memory_order_relaxed)) {
                _startTs = TraceClock::now();
                (void)Expander{(T::begin({_name, _startTs}), 0)...};
            } else {
                _ended = true;
            }
        }
    }

    // Iterate through the emitter list and return the first `steps()` method
    template<typename StepType, typename U, typename... Rest>
    auto stepLatencyImpl(StepType step, ...) const {
        return stepLatencyImpl<StepType, Rest...>(step, 0);
    }
    template<typename StepType, typename U, typename...>
    decltype(U::stepLatency(std::declval<StepType>())) stepLatencyImpl(StepType step, int /*unused*/) const {
        return U::stepLatency(step);
    }
};

// Specialize for the empty emitter list case for a NO-OP trace driver
template<>
class TraceDriver<> {
public:
    template<size_t kSize>
    explicit constexpr TraceDriver(const char (& /*name*/)[kSize]) noexcept {}
    explicit constexpr TraceDriver(const std::string& /*name*/) noexcept {}
    explicit constexpr TraceDriver(std::string_view /*name*/) noexcept {}
    // A const char* converts to std::string and string_view with equal rank, so without this
    // overload such calls are ambiguous.
    explicit constexpr TraceDriver(const char* /*name*/) noexcept {}
    // Mirrors TraceSdkScopedTrace's parent-span constructor, so a scope aliased to whichever of
    // the two a build selects can be constructed the same way in both.
    constexpr TraceDriver(std::string_view /*name*/, uint64_t /*parentSpanId*/) noexcept {}
    TraceDriver(const TraceDriver&) = delete;
    TraceDriver(TraceDriver&& other) noexcept {}
    // If we change the next line to '~TraceDriver() = default;',
    // we need to add '[[maybe_unused]]' in each use of TraceDriver<>,
    // which is uglier than this NOLINT.
    ~TraceDriver() noexcept {} // NOLINT(modernize-use-equals-default)
    void end() noexcept {}
    template<typename StepType>
    void step(StepType /*step*/) noexcept {}
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    TraceClock::duration latency() const {
        return {};
    }
    template<typename StepType>
    auto stepLatency(StepType /*step*/) const {
        return TraceClock::duration();
    }
};

template<typename... T>
using SCTrace = typename std::conditional<snap::kTracingEnabled, TraceDriver<T...>, TraceDriver<>>::type;

// Do not include OS Trace Emitters in production-like builds as they incur significant overhead.
#if defined(SC_ANDROID)
using OsTraceEmitter =
    typename std::conditional<snap::kIsDevBuild, profiling::ATraceSectionEmitter, profiling::NullEmitter>::type;
using OsAsyncTraceEmitter =
    typename std::conditional<snap::kIsDevBuild, profiling::ATraceAsyncSectionEmitter, profiling::NullEmitter>::type;
#elif defined(SC_IOS)
using OsTraceEmitter = profiling::NullEmitter;
using OsAsyncTraceEmitter =
    typename std::conditional<snap::kIsDevBuild, profiling::IosSignpostEmitter, profiling::NullEmitter>::type;
#else
using OsTraceEmitter = profiling::NullEmitter;
using OsAsyncTraceEmitter = profiling::NullEmitter;
#endif

namespace detail {
// An internal interface for the Profiling module to inject an object that
// provides TraceSDK based begin()/end() calls.
struct TraceSdkScopedTraceSupport {
    virtual ~TraceSdkScopedTraceSupport() = default;
    virtual int64_t beginSync(std::string_view label) = 0;
    virtual void endSync(int64_t cookie) = 0;
    virtual int64_t beginAsync(std::string_view label) = 0;
    virtual void endAsync(int64_t cookie) = 0;
    virtual void traceCounter(const std::string& name, int64_t count) = 0;

    /// Id of the innermost span open on this thread, or 0 when none is open or tracing is idle.
    /// Captured where work is scheduled and handed to beginSyncWithFlow where it runs, so the
    /// two spans can be linked.
    virtual uint64_t getCurrentSpanId() {
        return 0;
    }

    /// beginSync, plus a link back to the span that scheduled this work. A default-implemented
    /// forward to beginSync keeps existing backends working unchanged.
    virtual int64_t beginSyncWithFlow(std::string_view label, uint64_t parentSpanId) {
        return beginSync(label);
    }
};
} // namespace detail

extern std::atomic<detail::TraceSdkScopedTraceSupport*> scopedTraceSupportInstance;

// Implement the SCTrace methods by forwarding to TraceSdkScopedTraceSupport.
class TraceSdkScopedTrace {
public:
    explicit TraceSdkScopedTrace(const char* name) noexcept;
    explicit TraceSdkScopedTrace(const std::string& name) noexcept : TraceSdkScopedTrace(name.data()) {}
    // Avoids the strlen() the const char* overload pays, and lets callers hand over a name they
    // do not own a std::string for.
    explicit TraceSdkScopedTrace(std::string_view name) noexcept;
    // Same, plus the id of the span that scheduled this work, so the two get linked. Taking it
    // here rather than in a separate scope type keeps one span implementation.
    TraceSdkScopedTrace(std::string_view name, uint64_t parentSpanId) noexcept;
    TraceSdkScopedTrace(const TraceSdkScopedTrace&) = delete;

    virtual ~TraceSdkScopedTrace();

    void end() noexcept;

    static void initialize(detail::TraceSdkScopedTraceSupport* scopedTraceSupport);

private:
    detail::TraceSdkScopedTraceSupport* _scopedTraceSupport;
    int64_t _cookie;
};

} // namespace snap::profiling

// Alias to utils::debugging namespace for compatibility
namespace snap::utils::debugging {
using ScopedTrace = typename std::
    conditional<snap::kTracingEnabled, profiling::TraceSdkScopedTrace, ::snap::profiling::TraceDriver<>>::type;

// Reads the enclosing span's id where work is scheduled, so the span around that work can link
// back to it. Sits beside ScopedTrace because the two are used together: capture the id at the
// scheduling site, hand it to the scope where the work runs. Null-safe, and compiled out
// alongside the spans themselves.
inline uint64_t getCurrentSpanId() {
    if constexpr (snap::kTracingEnabled) {
        auto* support =
            std::atomic_load_explicit(&::snap::profiling::scopedTraceSupportInstance, std::memory_order_relaxed);
        return support != nullptr ? support->getCurrentSpanId() : 0;
    } else {
        return 0;
    }
}

} // namespace snap::utils::debugging
