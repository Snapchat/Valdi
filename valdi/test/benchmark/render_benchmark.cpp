//
//  render_benchmark.cpp
//  valdi
//
//  Hot-path microbenchmarks for the render / tree-diff path, paired with the
//  deterministic heap-allocation counter (HeapAllocationCounter.hpp).
//
//  Two profiles are measured against the representative tree in BenchViewTree.hpp:
//
//    RenderInitialTree  - apply a full RenderRequest to a fresh ViewNodeTree,
//                         i.e. the cost of materialising a component tree
//                         (element creation + attribute application + parenting)
//                         the way the runtime does when the JS side first
//                         renders a component.
//
//    RenderStateUpdate  - keep one tree alive and apply an incremental attribute
//                         update to the (already-mounted) root element, i.e. the
//                         tree-diff/re-render cost of a state change. The update
//                         alternates between two values every iteration so each
//                         iteration performs a real, identical amount of work,
//                         which keeps the per-op allocation count deterministic.
//
//  Alongside wall-clock time both report allocs/op and bytes/op, which are a
//  deterministic function of the code path and stable across runs.
//

// Emit the replaceable global operator new/delete overrides in this single TU.
#define VALDI_BENCH_DEFINE_HEAP_COUNTER_OVERRIDES
#include "HeapAllocationCounter.hpp"

#include "BenchViewTree.hpp"

#include <benchmark/benchmark.h>

using namespace Valdi;

namespace {

Ref<RenderRequest> buildInitialRequest(valdi_bench::BenchDependencies& deps) {
    valdi_bench::BenchRenderState renderState(deps);
    auto request = makeShared<RenderRequest>();
    valdi_bench::populateRenderRequest(*request, renderState, valdi_bench::createElementTree());
    return request;
}

// A minimal incremental update: re-set two attributes on the mounted root
// element (id 1). This drives the attribute-diff and layout-invalidation path
// without recreating any nodes.
Ref<RenderRequest> buildRootAttributeUpdate(valdi_bench::BenchDependencies& deps,
                                            double width,
                                            const char* background) {
    auto request = makeShared<RenderRequest>();

    auto* widthEntry = request->appendSetElementAttribute();
    widthEntry->setElementId(1);
    widthEntry->setAttributeId(deps.attributeIds.getIdForName(STRING_LITERAL("width")));
    widthEntry->setAttributeValue(Value(width));

    auto* backgroundEntry = request->appendSetElementAttribute();
    backgroundEntry->setElementId(1);
    backgroundEntry->setAttributeId(deps.attributeIds.getIdForName(STRING_LITERAL("backgroundColor")));
    backgroundEntry->setAttributeValue(Value(STRING_LITERAL(background)));

    return request;
}

} // namespace

static void RenderInitialTree(benchmark::State& state) {
    valdi_bench::BenchDependencies deps;
    auto request = buildInitialRequest(deps);

    // Warm up allocator/caches with a couple of full render+destroy cycles.
    for (int i = 0; i < 4; i++) {
        auto warmTree = deps.createTree();
        valdi_bench::renderUnderLock(*warmTree, *request);
        deps.destroyTree(warmTree);
    }

    unsigned long long totalAllocs = 0;
    unsigned long long totalBytes = 0;

    for (auto _ : state) {
        auto tree = deps.createTree();

        // ViewNodeRenderer requires the tree lock + a transaction scope; withLock
        // supplies both. Snapshot inside so only render() is measured.
        tree->withLock([&]() {
            ViewNodeRenderer renderer(*tree, ConsoleLogger::getLogger(), false);

            auto before = valdi_bench::snapshotHeapAllocations();
            renderer.render(*request);
            auto after = valdi_bench::snapshotHeapAllocations();

            auto d = valdi_bench::delta(before, after);
            totalAllocs += d.allocations;
            totalBytes += d.bytes;
        });

        benchmark::DoNotOptimize(tree);
        deps.destroyTree(tree);
    }

    state.counters["allocs/op"] =
        benchmark::Counter(static_cast<double>(totalAllocs), benchmark::Counter::kAvgIterations);
    state.counters["bytes/op"] =
        benchmark::Counter(static_cast<double>(totalBytes), benchmark::Counter::kAvgIterations);
}
BENCHMARK(RenderInitialTree);

static void RenderStateUpdate(benchmark::State& state) {
    valdi_bench::BenchDependencies deps;
    auto initialRequest = buildInitialRequest(deps);

    auto tree = deps.createTree();
    valdi_bench::renderUnderLock(*tree, *initialRequest);

    // Two updates with values of identical types; alternating between them makes
    // every iteration a real state change with the same allocation profile.
    auto updateA = buildRootAttributeUpdate(deps, 100.0, "white");
    auto updateB = buildRootAttributeUpdate(deps, 120.0, "black");

    for (int i = 0; i < 16; i++) {
        valdi_bench::renderUnderLock(*tree, *((i & 1) ? updateB : updateA));
    }

    unsigned long long totalAllocs = 0;
    unsigned long long totalBytes = 0;
    bool useB = false;

    for (auto _ : state) {
        auto& update = useB ? *updateB : *updateA;
        useB = !useB;

        tree->withLock([&]() {
            ViewNodeRenderer renderer(*tree, ConsoleLogger::getLogger(), false);

            auto before = valdi_bench::snapshotHeapAllocations();
            renderer.render(update);
            auto after = valdi_bench::snapshotHeapAllocations();

            auto d = valdi_bench::delta(before, after);
            totalAllocs += d.allocations;
            totalBytes += d.bytes;
        });
    }

    state.counters["allocs/op"] =
        benchmark::Counter(static_cast<double>(totalAllocs), benchmark::Counter::kAvgIterations);
    state.counters["bytes/op"] =
        benchmark::Counter(static_cast<double>(totalBytes), benchmark::Counter::kAvgIterations);

    deps.destroyTree(tree);
}
BENCHMARK(RenderStateUpdate);

BENCHMARK_MAIN();
