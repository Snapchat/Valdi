#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Text/TextLayoutBuilder.hpp"

#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"

#include "benchmark/benchmark.h"

using namespace snap::drawing;

static void doBenchmark(benchmark::State& state, Valdi::Function<void(const Ref<FontManager>&)>&& benchmarkFn) {
    auto disableCache = state.range(0) == 0;
    auto fontManager = Valdi::makeShared<FontManager>(Valdi::ConsoleLogger::getLogger(), !disableCache);
    fontManager->load();

    auto shouldClearCacheAfterEachIteration = state.range(0) == 1;
    for (auto _ : state) {
        benchmarkFn(fontManager);

        if (shouldClearCacheAfterEachIteration) {
            fontManager->getTextShaper()->clearCache();
        }
    }
}

static void TextLayoutSimpleTextSingleLine(benchmark::State& state) {
    doBenchmark(state, [&](const auto& fontManager) {
        auto font = fontManager->getDefaultFont().moveValue();

        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, Size::make(5000, 5000), 1, fontManager, false, 1.0f);

        builder.append("Hello World!", font, TextLayoutLineHeight::multiple(1.0f), 0.0f, TextDecorationNone);

        benchmark::DoNotOptimize(builder.build());
    });
}

static void TextLayoutLongTextSingleLine(benchmark::State& state) {
    doBenchmark(state, [&](const auto& fontManager) {
        auto font = fontManager->getDefaultFont().moveValue();

        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, Size::make(50000, 5000), 0, fontManager, false, 1.0f);

        builder.append("Hello World! This string might be pretty long, and because of that we will have to lay it out "
                       "on multiple lines",
                       font,
                       TextLayoutLineHeight::multiple(1.0f),
                       0.0f,
                       TextDecorationNone);

        benchmark::DoNotOptimize(builder.build());
    });
}

static void TextLayoutLongTextMultiLine(benchmark::State& state) {
    doBenchmark(state, [&](const auto& fontManager) {
        auto font = fontManager->getDefaultFont().moveValue();

        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, Size::make(100, 5000), 0, fontManager, false, 1.0f);

        builder.append("Hello World! This string might be pretty long, and because of that we will have to lay it out "
                       "on multiple lines",
                       font,
                       TextLayoutLineHeight::multiple(1.0f),
                       0.0f,
                       TextDecorationNone);

        benchmark::DoNotOptimize(builder.build());
    });
}

static void TextLayoutEmojiText(benchmark::State& state) {
    doBenchmark(state, [&](const auto& fontManager) {
        auto font = fontManager->getDefaultFont().moveValue();

        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, Size::make(5000, 5000), 1, fontManager, false, 1.0f);

        builder.append("Hello World 😀😃😄! We like emojis here 😍😍😍.", font, TextLayoutLineHeight::multiple(1.0f), 0.0f, TextDecorationNone);

        benchmark::DoNotOptimize(builder.build());
    });
}

static void TextLayoutArabicText(benchmark::State& state) {
    doBenchmark(state, [&](const auto& fontManager) {
        auto font = fontManager->getDefaultFont().moveValue();

        TextLayoutBuilder builder(TextAlignLeft, TextOverflowEllipsis, Size::make(5000, 5000), 1, fontManager, false, 1.0f);

        builder.append("قرأ Wikipedia™ طوال اليوم.", font, TextLayoutLineHeight::multiple(1.0), 0.0, TextDecorationNone);

        benchmark::DoNotOptimize(builder.build());
    });
}

BENCHMARK(TextLayoutSimpleTextSingleLine)->Arg(0)->Arg(1)->Arg(2);
BENCHMARK(TextLayoutLongTextSingleLine)->Arg(0)->Arg(1)->Arg(2);
BENCHMARK(TextLayoutLongTextMultiLine)->Arg(0)->Arg(1)->Arg(2);
BENCHMARK(TextLayoutEmojiText)->Arg(0)->Arg(1)->Arg(2);
BENCHMARK(TextLayoutArabicText)->Arg(0)->Arg(1)->Arg(2);
BENCHMARK_MAIN();
