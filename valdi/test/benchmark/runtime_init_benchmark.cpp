//
//  runtime_init_benchmark.cpp
//  valdi
//
//  Hot-path microbenchmark for runtime bring-up (cf. COMPOSER-5007 startup cost),
//  paired with the deterministic heap-allocation counter (HeapAllocationCounter.hpp).
//
//  Each measured iteration stands up a fresh in-process runtime from scratch: a
//  Valdi Context, a JS task scheduler, a real JS engine context (QuickJS on
//  macOS/Linux) and its initialize() pass. This is the per-context init the app
//  pays every time a new Valdi surface is created. Alongside wall-clock time the
//  benchmark reports allocs/op and bytes/op.
//
//  Scope note: the heap counter records C++ operator-new allocations only; the
//  JS engine's own malloc-based heap is intentionally excluded (see
//  HeapAllocationCounter.hpp), so this isolates the C++ runtime-init allocations.
//
//  TODO(microbench): extend this to also load a compiled .valdi module and drive
//  its first render, to cover the module-load half of startup. That requires the
//  standalone runtime (ValdiStandaloneRuntime + resource loader) and the testdata
//  module targets used by :test_integration, which are heavier deps than this
//  self-contained binary pulls in today; left out deliberately rather than faked.
//

// Emit the replaceable global operator new/delete overrides in this single TU.
#define VALDI_BENCH_DEFINE_HEAP_COUNTER_OVERRIDES
#include "HeapAllocationCounter.hpp"

#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi_core/JavaScriptEngineType.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include <benchmark/benchmark.h>

using namespace Valdi;
using snap::valdi_core::JavaScriptEngineType;

namespace {

// Minimal single-threaded scheduler; runtime init never dispatches JS-thread work
// but the context requires a scheduler. Runs any dispatched task in place.
struct BenchTaskScheduler : public JavaScriptTaskScheduler {
    IJavaScriptContext* jsContext = nullptr;
    Ref<Context> valdiContext;

    void dispatchOnJsThread(Ref<Context> ownerContext,
                            JavaScriptTaskScheduleType /*scheduleType*/,
                            uint32_t /*delayMs*/,
                            JavaScriptThreadTask&& function) override {
        auto executeContext = ownerContext != nullptr ? ownerContext : valdiContext;
        JSExceptionTracker exceptionTracker(*jsContext);
        JavaScriptEntryParameters params(*jsContext, exceptionTracker, executeContext);
        function(params);
    }

    void dispatchOnJsThread(JsThreadDispatchReason /*reason*/,
                            JavaScriptTaskScheduleType scheduleType,
                            uint32_t delayMs,
                            JavaScriptThreadTask&& function) override {
        dispatchOnJsThread(Ref<Context>(), scheduleType, delayMs, std::move(function));
    }

    bool isInJsThread() override {
        return true;
    }

    Ref<Context> getLastDispatchedContext() const override {
        return nullptr;
    }

    std::vector<JavaScriptCapturedStacktrace> captureStackTraces(
        std::chrono::steady_clock::duration /*timeout*/) override {
        return {};
    }
};

// Build and initialize a fresh runtime. This is the operation under test.
void initRuntimeOnce(JavaScriptEngineType engine) {
    auto valdiContext = makeShared<Context>(1, strongSmallRef(&ConsoleLogger::getLogger()));
    valdiContext->retainDisposables();

    auto scheduler = makeShared<BenchTaskScheduler>();
    scheduler->valdiContext = valdiContext;

    auto* bridge = JavaScriptBridge::get(engine);
    auto jsContext = bridge->createJsContext(scheduler.get(), ConsoleLogger::getLogger());
    scheduler->jsContext = jsContext.get();

    {
        JSExceptionTracker exceptionTracker(*jsContext);
        JavaScriptContextEntry contextEntry(valdiContext);
        jsContext->willEnterVM();
        jsContext->initialize(IJavaScriptContextConfig(), exceptionTracker);
        jsContext->willExitVM(exceptionTracker);
    }

    benchmark::DoNotOptimize(jsContext.get());

    jsContext = nullptr;
    valdiContext->releaseDisposables();
}

void runRuntimeInit(JavaScriptEngineType engine, benchmark::State& state) {
    if (!JavaScriptBridge::isAvailable(engine)) {
        state.SkipWithError("Requested JS engine is not compiled into this build");
        return;
    }

    ConsoleLogger::getLogger().setMinLogType(LogTypeWarn);

    // Warm up so first-touch statics / allocator arenas are established.
    for (int i = 0; i < 4; i++) {
        initRuntimeOnce(engine);
    }

    unsigned long long totalAllocs = 0;
    unsigned long long totalBytes = 0;

    for (auto _ : state) {
        auto before = valdi_bench::snapshotHeapAllocations();
        initRuntimeOnce(engine);
        auto after = valdi_bench::snapshotHeapAllocations();

        auto d = valdi_bench::delta(before, after);
        totalAllocs += d.allocations;
        totalBytes += d.bytes;
    }

    state.counters["allocs/op"] =
        benchmark::Counter(static_cast<double>(totalAllocs), benchmark::Counter::kAvgIterations);
    state.counters["bytes/op"] =
        benchmark::Counter(static_cast<double>(totalBytes), benchmark::Counter::kAvgIterations);
}

} // namespace

static void RuntimeInitQuickJS(benchmark::State& state) {
    runRuntimeInit(JavaScriptEngineType::QuickJS, state);
}
BENCHMARK(RuntimeInitQuickJS);

BENCHMARK_MAIN();
