// Microbenchmarks for Valdi runtime hot paths: attribute value diffing, deferred view transaction
// ops and KeyValueStore serialization. Uses only long-standing public APIs so the same source can
// measure a build before and after a change.

#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi/runtime/Resources/KeyValueStore.hpp"
#include "valdi/runtime/Views/DeferredViewTransaction.hpp"
#include "valdi/runtime/Views/View.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

using namespace Valdi;

namespace {

// ---------------------------------------------------------------------------
// Value equality / hash (attribute diffing, CSS matching, enum unmarshalling)
// ---------------------------------------------------------------------------

void Value_EqInternedVsStaticUTF8(benchmark::State& state) {
    auto interned = Value(STRING_LITERAL("background-color-primary"));
    auto staticStr = Value(StaticString::makeUTF8(std::string_view("background-color-primary")));
    for (auto _ : state) {
        benchmark::DoNotOptimize(interned == staticStr);
    }
}
BENCHMARK(Value_EqInternedVsStaticUTF8);

void Value_EqInternedVsStaticUTF16(benchmark::State& state) {
    auto interned = Value(STRING_LITERAL("background-color-primary"));
    std::u16string utf16 = u"background-color-primary";
    auto staticStr = Value(StaticString::makeUTF16(utf16.data(), utf16.size()));
    for (auto _ : state) {
        benchmark::DoNotOptimize(interned == staticStr);
    }
}
BENCHMARK(Value_EqInternedVsStaticUTF16);

void Value_EqInternedVsStaticUTF16DifferentLength(benchmark::State& state) {
    auto interned = Value(STRING_LITERAL("background-color-primary"));
    std::u16string utf16 = u"center";
    auto staticStr = Value(StaticString::makeUTF16(utf16.data(), utf16.size()));
    for (auto _ : state) {
        benchmark::DoNotOptimize(interned == staticStr);
    }
}
BENCHMARK(Value_EqInternedVsStaticUTF16DifferentLength);

void Value_EqInternedVsInternedDifferent(benchmark::State& state) {
    auto a = Value(STRING_LITERAL("center"));
    auto b = Value(STRING_LITERAL("flex-start"));
    for (auto _ : state) {
        benchmark::DoNotOptimize(a == b);
    }
}
BENCHMARK(Value_EqInternedVsInternedDifferent);

void Value_HashInterned(benchmark::State& state) {
    auto a = Value(STRING_LITERAL("center"));
    for (auto _ : state) {
        benchmark::DoNotOptimize(a.hash());
    }
}
BENCHMARK(Value_HashInterned);

// Contended variant: the global StringCache lock is what the old mixed-type compare took, so
// measure it with several threads diffing at once (JS thread + main + workers in production).
void Value_EqInternedVsStaticUTF8_Threads(benchmark::State& state) {
    static auto interned = Value(STRING_LITERAL("background-color-primary"));
    auto staticStr = Value(StaticString::makeUTF8(std::string_view("background-color-primary")));
    for (auto _ : state) {
        benchmark::DoNotOptimize(interned == staticStr);
    }
}
BENCHMARK(Value_EqInternedVsStaticUTF8_Threads)->Threads(4);

// ---------------------------------------------------------------------------
// Deferred view transaction op construction (per frame change / per op per render)
// ---------------------------------------------------------------------------

void DeferredOp_SetViewFrameCapture(benchmark::State& state) {
    Ref<View> view;
    Ref<Animator> animator;
    Frame frame(1, 2, 3, 4);
    bool rtl = false;
    std::vector<DeferredViewTransactionOperation> ops;
    ops.reserve(1000);
    for (auto _ : state) {
        for (int i = 0; i < 1000; i++) {
            ops.emplace_back([view, frame, rtl, animator](IViewTransaction& transaction) {
                transaction.setViewFrame(view, frame, rtl, animator);
            });
        }
        benchmark::DoNotOptimize(ops.data());
        ops.clear();
    }
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(DeferredOp_SetViewFrameCapture);

// Closer to DeferredViewTransaction::flush(): it moves _operations out, so each flush regrows the
// vector from empty (move-constructing every element on reallocation), and the captured refs are
// non-null, so every copy pays the refcount atomics.
void DeferredOp_SetViewFrameFlush(benchmark::State& state) {
    Ref<View> view = makeShared<StandaloneView>(STRING_LITERAL("View"));
    auto animator = makeShared<Animator>(nullptr);
    Frame frame(1, 2, 3, 4);
    bool rtl = false;
    for (auto _ : state) {
        std::vector<DeferredViewTransactionOperation> ops;
        for (int i = 0; i < 1000; i++) {
            ops.emplace_back([view, frame, rtl, animator](IViewTransaction& transaction) {
                transaction.setViewFrame(view, frame, rtl, animator);
            });
        }
        benchmark::DoNotOptimize(ops.data());
    }
    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(DeferredOp_SetViewFrameFlush);

// ---------------------------------------------------------------------------
// KeyValueStore::serialize (PersistentStore save, 50 ms after any write)
// ---------------------------------------------------------------------------

void KeyValueStore_Serialize(benchmark::State& state) {
    auto count = static_cast<size_t>(state.range(0));
    std::vector<StringBox> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; i++) {
        keys.emplace_back(StringCache::getGlobal().makeString("persistent_store_key_" + std::to_string(i)));
    }
    static const std::string kBlob(64, 'x');
    auto blob = BytesView(nullptr, reinterpret_cast<const Byte*>(kBlob.data()), kBlob.size());

    KeyValueStore store;
    for (const auto& key : keys) {
        store.store(key, blob, 0, 0);
    }

    for (auto _ : state) {
        auto result = store.serialize();
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(count));
}
BENCHMARK(KeyValueStore_Serialize)->Arg(100)->Arg(1000)->Arg(10000)->Unit(benchmark::kMicrosecond);

} // namespace

BENCHMARK_MAIN();
