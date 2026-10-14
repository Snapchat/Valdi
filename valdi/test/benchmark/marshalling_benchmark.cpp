//
//  marshalling_benchmark.cpp
//  valdi
//
//  First Valdi hot-path microbenchmark for the JS<->native marshalling bridge,
//  paired with a deterministic heap-allocation counter (HeapAllocationCounter.hpp).
//
//  Each measured iteration marshals a representative payload (numbers, strings,
//  an array, a nested object and a callback) native->JS via valueToJSValue and
//  back JS->native via jsValueToValue, driven through a real in-process JS
//  engine (QuickJS on macOS/Linux). Alongside wall-clock time the benchmark
//  reports allocs/op and bytes/op, which are a deterministic function of the
//  code path and therefore stable across runs even as time fluctuates.
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
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/JavaScriptEngineType.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include <benchmark/benchmark.h>

using namespace Valdi;
using snap::valdi_core::JavaScriptEngineType;

namespace {

// Minimal single-threaded JavaScriptTaskScheduler. Marshalling of plain data and
// function values never dispatches JS-thread work, but the context requires a
// scheduler; this one runs any dispatched task synchronously in place.
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

// RAII entry into the JS VM for a scope, mirroring the runtime's own entry/exit.
struct JsEntry {
    IJavaScriptContext& context;
    JSExceptionTracker exceptionTracker;
    JavaScriptContextEntry contextEntry;

    JsEntry(IJavaScriptContext& context, const Ref<Context>& valdiContext)
        : context(context), exceptionTracker(context), contextEntry(valdiContext) {
        context.willEnterVM();
    }

    ~JsEntry() {
        context.willExitVM(exceptionTracker);
    }
};

// Stands up a real JS engine in-process with no module loading.
struct BenchJsRuntime {
    Ref<Context> valdiContext;
    Ref<BenchTaskScheduler> scheduler;
    Ref<IJavaScriptContext> jsContext;

    explicit BenchJsRuntime(JavaScriptEngineType engine) {
        ConsoleLogger::getLogger().setMinLogType(LogTypeWarn);

        valdiContext = makeShared<Context>(1, strongSmallRef(&ConsoleLogger::getLogger()));
        valdiContext->retainDisposables();

        scheduler = makeShared<BenchTaskScheduler>();
        scheduler->valdiContext = valdiContext;

        auto* bridge = JavaScriptBridge::get(engine);
        jsContext = bridge->createJsContext(scheduler.get(), ConsoleLogger::getLogger());
        scheduler->jsContext = jsContext.get();

        JsEntry entry(*jsContext, valdiContext);
        jsContext->initialize(IJavaScriptContextConfig(), entry.exceptionTracker);
    }

    ~BenchJsRuntime() {
        valdiContext->releaseDisposables();
        jsContext = nullptr;
    }

    template<typename F>
    void withEntry(F&& func) {
        JsEntry entry(*jsContext, valdiContext);
        func(*jsContext, entry.exceptionTracker);
    }
};

// A representative bridge payload: two number kinds, strings, a bool, a
// heterogeneous array, a nested object and a JS callback.
Value makePayload(IJavaScriptContext& context, JSExceptionTracker& exceptionTracker) {
    ReferenceInfoBuilder referenceInfoBuilder;

    auto callbackRef = context.evaluate("(function doubleIt(x) { return x * 2; })", "callback.js", exceptionTracker);
    Value callback = jsValueToValue(context, callbackRef.get(), referenceInfoBuilder, exceptionTracker);

    auto tags = ValueArray::make({Value(1), Value(2), Value(3), Value("alpha"), Value("beta")});

    Value location;
    location.setMapValue("lat", Value(37.7749));
    location.setMapValue("lng", Value(-122.4194));
    location.setMapValue("label", Value("Snap HQ"));

    Value payload;
    payload.setMapValue("id", Value(static_cast<int32_t>(12345)));
    payload.setMapValue("ratio", Value(0.5));
    payload.setMapValue("name", Value("valdi-marshalling-benchmark"));
    payload.setMapValue("enabled", Value(true));
    payload.setMapValue("tags", Value(tags));
    payload.setMapValue("location", location);
    payload.setMapValue("onEvent", callback);
    return payload;
}

// Marshal native->JS and back JS->native. Returns the per-iteration allocation
// delta so the caller can confirm it is constant.
void runRoundTrip(JavaScriptEngineType engine, benchmark::State& state) {
    if (!JavaScriptBridge::isAvailable(engine)) {
        state.SkipWithError("Requested JS engine is not compiled into this build");
        return;
    }

    BenchJsRuntime runtime(engine);

    runtime.withEntry([&](IJavaScriptContext& context, JSExceptionTracker& exceptionTracker) {
        ReferenceInfoBuilder referenceInfoBuilder;
        Value payload = makePayload(context, exceptionTracker);

        // Warm caches (interned property names, cached engine values) so every
        // measured iteration performs an identical set of C++ allocations.
        for (int i = 0; i < 16; i++) {
            auto jsValue = valueToJSValue(context, payload, referenceInfoBuilder, exceptionTracker);
            auto roundTripped = jsValueToValue(context, jsValue.get(), referenceInfoBuilder, exceptionTracker);
            benchmark::DoNotOptimize(roundTripped);
        }

        unsigned long long totalAllocs = 0;
        unsigned long long totalBytes = 0;

        for (auto _ : state) {
            auto before = valdi_bench::snapshotHeapAllocations();

            auto jsValue = valueToJSValue(context, payload, referenceInfoBuilder, exceptionTracker);
            auto roundTripped = jsValueToValue(context, jsValue.get(), referenceInfoBuilder, exceptionTracker);

            auto after = valdi_bench::snapshotHeapAllocations();

            benchmark::DoNotOptimize(jsValue);
            benchmark::DoNotOptimize(roundTripped);

            auto d = valdi_bench::delta(before, after);
            totalAllocs += d.allocations;
            totalBytes += d.bytes;
        }

        // kAvgIterations divides by the iteration count, so a constant
        // per-iteration allocation profile reports the same value every run.
        state.counters["allocs/op"] =
            benchmark::Counter(static_cast<double>(totalAllocs), benchmark::Counter::kAvgIterations);
        state.counters["bytes/op"] =
            benchmark::Counter(static_cast<double>(totalBytes), benchmark::Counter::kAvgIterations);
    });
}

// Marshal native->JS only, to show a second, smaller deterministic profile.
void runNativeToJs(JavaScriptEngineType engine, benchmark::State& state) {
    if (!JavaScriptBridge::isAvailable(engine)) {
        state.SkipWithError("Requested JS engine is not compiled into this build");
        return;
    }

    BenchJsRuntime runtime(engine);

    runtime.withEntry([&](IJavaScriptContext& context, JSExceptionTracker& exceptionTracker) {
        ReferenceInfoBuilder referenceInfoBuilder;
        Value payload = makePayload(context, exceptionTracker);

        for (int i = 0; i < 16; i++) {
            auto jsValue = valueToJSValue(context, payload, referenceInfoBuilder, exceptionTracker);
            benchmark::DoNotOptimize(jsValue);
        }

        unsigned long long totalAllocs = 0;
        unsigned long long totalBytes = 0;

        for (auto _ : state) {
            auto before = valdi_bench::snapshotHeapAllocations();

            auto jsValue = valueToJSValue(context, payload, referenceInfoBuilder, exceptionTracker);

            auto after = valdi_bench::snapshotHeapAllocations();

            benchmark::DoNotOptimize(jsValue);

            auto d = valdi_bench::delta(before, after);
            totalAllocs += d.allocations;
            totalBytes += d.bytes;
        }

        state.counters["allocs/op"] =
            benchmark::Counter(static_cast<double>(totalAllocs), benchmark::Counter::kAvgIterations);
        state.counters["bytes/op"] =
            benchmark::Counter(static_cast<double>(totalBytes), benchmark::Counter::kAvgIterations);
    });
}

} // namespace

static void MarshalRoundTripQuickJS(benchmark::State& state) {
    runRoundTrip(JavaScriptEngineType::QuickJS, state);
}
BENCHMARK(MarshalRoundTripQuickJS);

static void MarshalNativeToJsQuickJS(benchmark::State& state) {
    runNativeToJs(JavaScriptEngineType::QuickJS, state);
}
BENCHMARK(MarshalNativeToJsQuickJS);

BENCHMARK_MAIN();
