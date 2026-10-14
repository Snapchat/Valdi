//
//  HeapAllocationCounter.hpp
//  valdi
//
//  A deterministic, noise-free heap-allocation counter for Google-Benchmark
//  microbenchmarks. It replaces the global operator new/delete so that every
//  C++ heap allocation on the current thread bumps a thread-local counter for
//  both allocation count and total bytes. Unlike wall-clock time, these counts
//  are a deterministic function of the code path exercised, which makes them a
//  stable primary signal for catching allocation regressions in CI.
//
//  Usage:
//    - Include this header from the benchmark translation unit.
//    - In EXACTLY ONE translation unit of the binary, define
//      VALDI_BENCH_DEFINE_HEAP_COUNTER_OVERRIDES before including it so the
//      replaceable global operator new/delete are emitted once.
//    - Snapshot valdi_bench::snapshotHeapAllocations() before and after the
//      measured region and report the delta via state.counters[...].
//
//  Scope note: this counts C++ operator-new allocations only. Engines that
//  allocate through malloc/free directly (e.g. QuickJS's internal allocator)
//  are intentionally NOT counted, so the metric isolates the C++
//  bridge/marshalling allocations under test.
//

#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>

namespace valdi_bench {

struct HeapAllocationSnapshot {
    unsigned long long allocations = 0;
    unsigned long long bytes = 0;
};

// One counter per thread. `inline thread_local` gives a single merged
// definition across translation units (C++17), so the override TU and any
// benchmark TU observe the same per-thread counters.
inline thread_local unsigned long long tlHeapAllocationCount = 0;
inline thread_local unsigned long long tlHeapAllocationBytes = 0;

inline HeapAllocationSnapshot snapshotHeapAllocations() noexcept {
    return {tlHeapAllocationCount, tlHeapAllocationBytes};
}

inline HeapAllocationSnapshot delta(const HeapAllocationSnapshot& before,
                                    const HeapAllocationSnapshot& after) noexcept {
    return {after.allocations - before.allocations, after.bytes - before.bytes};
}

inline void recordAllocation(std::size_t size) noexcept {
    tlHeapAllocationCount++;
    tlHeapAllocationBytes += size;
}

} // namespace valdi_bench

#ifdef VALDI_BENCH_DEFINE_HEAP_COUNTER_OVERRIDES

// Replaceable global allocation functions. Non-aligned variants only: the
// over-aligned (std::align_val_t) forms are deliberately left as the default
// implementation so an aligned allocation is never freed through std::free.

void* operator new(std::size_t size) {
    valdi_bench::recordAllocation(size);
    void* ptr = std::malloc(size != 0 ? size : 1);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](std::size_t size) {
    valdi_bench::recordAllocation(size);
    void* ptr = std::malloc(size != 0 ? size : 1);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    valdi_bench::recordAllocation(size);
    return std::malloc(size != 0 ? size : 1);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    valdi_bench::recordAllocation(size);
    return std::malloc(size != 0 ? size : 1);
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    std::free(ptr);
}

#endif // VALDI_BENCH_DEFINE_HEAP_COUNTER_OVERRIDES
