//
//  layout_benchmark.cpp
//  valdi
//
//  Hot-path microbenchmark for the Yoga/flexbox layout pass, paired with the
//  deterministic heap-allocation counter (HeapAllocationCounter.hpp).
//
//  A representative view tree (~2000 nodes: header + 20 scroll rows of 100 text
//  cards, see BenchViewTree.hpp) is built fresh for each measured iteration and
//  laid out once via ViewNodeTree::measureLayout. A fresh tree is used every
//  iteration on purpose: Yoga caches each node's layout keyed on its constraints,
//  so re-laying out the SAME tree with unchanged constraints returns a cache hit
//  (~0 allocs) rather than the layout cost (measureLayout's forceLayout only
//  bypasses Valdi's own early-exit, not Yoga's per-node cache). The allocation
//  snapshot brackets only the layout call, so the (untimed-for-allocs) tree
//  build/render isn't counted — allocs/op and bytes/op reflect just the layout
//  code path, a deterministic function of it and stable across runs even as
//  wall-time fluctuates.
//

// Emit the replaceable global operator new/delete overrides in this single TU.
#define VALDI_BENCH_DEFINE_HEAP_COUNTER_OVERRIDES
#include "HeapAllocationCounter.hpp"

#include "BenchViewTree.hpp"

#include <benchmark/benchmark.h>

using namespace Valdi;

namespace {

// Fixed layout constraint (a phone viewport). The tree is rebuilt cold each
// iteration, so the layout runs in full regardless of the constraint value.
constexpr float kLayoutWidth = 375.0f;
constexpr float kLayoutHeight = 812.0f;

Size runLayout(ViewNodeTree& tree) {
    return tree.measureLayout(kLayoutWidth, MeasureModeExactly, kLayoutHeight, MeasureModeExactly, LayoutDirectionLTR);
}

} // namespace

static void LayoutDeepTree(benchmark::State& state) {
    valdi_bench::BenchDependencies deps;

    // Warm up global one-time caches (interned attribute names, style interning,
    // Yoga config) on a throwaway tree so they don't skew the first measurement.
    {
        valdi_bench::BenchRenderState warmState(deps);
        auto warmRequest = makeShared<RenderRequest>();
        valdi_bench::populateRenderRequest(*warmRequest, warmState, valdi_bench::createElementTree());
        auto warm = deps.createTree();
        valdi_bench::renderUnderLock(*warm, *warmRequest);
        for (int i = 0; i < 4; i++) {
            benchmark::DoNotOptimize(runLayout(*warm));
        }
        deps.destroyTree(warm);
    }

    unsigned long long totalAllocs = 0;
    unsigned long long totalBytes = 0;

    for (auto _ : state) {
        // Fresh, never-laid-out tree each iteration so runLayout is a real cold
        // layout, not a Yoga cache hit. Only the layout call is inside the alloc
        // snapshot, so the tree build/render is excluded from allocs/op.
        valdi_bench::BenchRenderState renderState(deps);
        auto request = makeShared<RenderRequest>();
        valdi_bench::populateRenderRequest(*request, renderState, valdi_bench::createElementTree());
        auto tree = deps.createTree();
        valdi_bench::renderUnderLock(*tree, *request);

        auto before = valdi_bench::snapshotHeapAllocations();
        auto size = runLayout(*tree);
        auto after = valdi_bench::snapshotHeapAllocations();
        benchmark::DoNotOptimize(size);

        auto d = valdi_bench::delta(before, after);
        totalAllocs += d.allocations;
        totalBytes += d.bytes;

        deps.destroyTree(tree);
    }

    state.counters["allocs/op"] =
        benchmark::Counter(static_cast<double>(totalAllocs), benchmark::Counter::kAvgIterations);
    state.counters["bytes/op"] =
        benchmark::Counter(static_cast<double>(totalBytes), benchmark::Counter::kAvgIterations);
}
BENCHMARK(LayoutDeepTree);

BENCHMARK_MAIN();
