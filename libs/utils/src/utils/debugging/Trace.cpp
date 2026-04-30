#include "utils/debugging/Trace.hpp"

#include <atomic>

// for trace functions
#if defined(SC_ANDROID)
#include <android/trace.h>
#include <dlfcn.h>
#elif defined(SC_IOS)
#include <os/signpost.h>
#endif

namespace snap::profiling {

std::atomic<int> gTraceLevel = 0;

// ------------------------------------------------------

#if defined(SC_ANDROID)
struct TraceFunctions {
    using TraceBeginSection = void (*)(const char* sectionName);
    using TraceEndSection = void (*)();
    using TraceBeginAsyncSection = void (*)(const char* sectionName, int32_t cookie);
    using TraceEndAsyncSection = void (*)(const char* sectionName, int32_t cookie);

    TraceBeginSection beginSection = [](const char* sectionName) {};
    TraceEndSection endSection = [] {};
    TraceBeginAsyncSection beginAsyncSection = [](const char* sectionName, int32_t cookie) {};
    TraceEndAsyncSection endAsyncSection = [](const char* sectionName, int32_t cookie) {};

    TraceFunctions() {
        // TODO: remove dlopen() hacks when minAPI is bumped up to 23
        void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (lib != nullptr) {
            auto pBeginSection = reinterpret_cast<TraceBeginSection>(dlsym(lib, "ATrace_beginSection"));
            auto pEndSection = reinterpret_cast<TraceEndSection>(dlsym(lib, "ATrace_endSection"));
            auto pBeginAsyncSection = reinterpret_cast<TraceBeginAsyncSection>(dlsym(lib, "ATrace_beginAsyncSection"));
            auto pEndAsyncSection = reinterpret_cast<TraceEndAsyncSection>(dlsym(lib, "ATrace_endAsyncSection"));
            if (pBeginSection && pEndSection) {
                beginSection = pBeginSection;
                endSection = pEndSection;
            }
            if (pBeginAsyncSection && pEndAsyncSection) {
                beginAsyncSection = pBeginAsyncSection;
                endAsyncSection = pEndAsyncSection;
            }
        }
    }

    static TraceFunctions& get() {
        static TraceFunctions traceFunctions;
        return traceFunctions;
    }
};

static std::atomic<int32_t> gCookie{1};

// ------------------------------------------------------

// NOLINTNEXTLINE(readability-convert-member-functions-to-static, readability-make-member-function-const)
void ATraceSectionEmitter::begin(const TraceBegin& traceBegin) {
    TraceFunctions::get().beginSection(traceBegin.name.data());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static, readability-make-member-function-const)
void ATraceSectionEmitter::end(const TraceEnd& /*traceEnd*/) {
    TraceFunctions::get().endSection();
}

// ------------------------------------------------------

ATraceAsyncSectionEmitter::ATraceAsyncSectionEmitter() {
    _cookie = gCookie.fetch_add(1, std::memory_order_relaxed);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void ATraceAsyncSectionEmitter::begin(const TraceBegin& traceBegin) {
    TraceFunctions::get().beginAsyncSection(traceBegin.name.data(), _cookie);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void ATraceAsyncSectionEmitter::end(const TraceEnd& traceEnd) {
    TraceFunctions::get().endAsyncSection(traceEnd.name.data(), _cookie);
}

// ------------------------------------------------------

#elif defined(SC_IOS)

static os_log_t nativeSignpostLogger() {
    if (__builtin_available(iOS 12.0, *)) {
        static os_log_t logger = os_log_create("com.snap.native.signpost", "SCClient");
        return logger;
    } else {
        return nullptr;
    }
}

IosSignpostEmitter::IosSignpostEmitter() {
    if (__builtin_available(iOS 12.0, *)) {
        _signpostId = os_signpost_id_generate(nativeSignpostLogger());
    }
}

void IosSignpostEmitter::begin(const TraceBegin& traceBegin) {
    if (__builtin_available(iOS 12.0, *)) {
        os_signpost_interval_begin(
            nativeSignpostLogger(), _signpostId, "AsyncTrace", "%{public}s", traceBegin.name.data());
    }
}

void IosSignpostEmitter::step(const TraceStep<std::string_view>& traceStep) {
    if (__builtin_available(iOS 12.0, *)) {
        os_signpost_event_emit(nativeSignpostLogger(),
                               _signpostId,
                               "AsyncTrace",
                               "%{public}s:%{public}s",
                               traceStep.name.data(),
                               traceStep.step.data());
    }
}

void IosSignpostEmitter::end(const TraceEnd& traceEnd) {
    if (__builtin_available(iOS 12.0, *)) {
        os_signpost_interval_end(nativeSignpostLogger(), _signpostId, "AsyncTrace", "%{public}s", traceEnd.name.data());
    }
}

#endif

std::atomic<detail::TraceSdkScopedTraceSupport*> scopedTraceSupportInstance = nullptr;

void TraceSdkScopedTrace::initialize(detail::TraceSdkScopedTraceSupport* scopedTraceSupport) {
    std::atomic_store_explicit(&scopedTraceSupportInstance, scopedTraceSupport, std::memory_order_relaxed);
}

TraceSdkScopedTrace::TraceSdkScopedTrace(const char* name) noexcept
    : _scopedTraceSupport(std::atomic_load_explicit(&scopedTraceSupportInstance, std::memory_order_relaxed)) {
    if (_scopedTraceSupport) {
        _cookie = _scopedTraceSupport->beginSync(name);
    }
}

TraceSdkScopedTrace::~TraceSdkScopedTrace() {
    if (_scopedTraceSupport) {
        _scopedTraceSupport->endSync(_cookie);
    }
}

void TraceSdkScopedTrace::end() noexcept {
    if (_scopedTraceSupport) {
        _scopedTraceSupport->endSync(_cookie);
        _scopedTraceSupport = nullptr;
    }
}

} // namespace snap::profiling
