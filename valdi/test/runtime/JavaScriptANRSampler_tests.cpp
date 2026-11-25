#include "valdi/runtime/JavaScript/JavaScriptANRSampler.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <string>

using namespace Valdi;

namespace ValdiTest {

namespace {

constexpr std::chrono::milliseconds kInterval(50);

JavaScriptANRAttribution makeStack(std::initializer_list<const char*> labels) {
    JavaScriptANRAttribution stack;
    for (const auto* label : labels) {
        stack.push(StringBox::fromCString(label));
    }
    return stack;
}

/** Sampled milliseconds by stack, written "a -> b" ("" for no breadcrumb). */
std::map<std::string, int64_t> timeByStack(const JavaScriptANRSampler& sampler) {
    std::map<std::string, int64_t> result;
    for (const auto& stack : sampler.getSampledStacks()) {
        std::string key;
        for (size_t i = 0; i < stack.labels.size(); ++i) {
            key += (i == 0 ? "" : " -> ") + stack.labels[i].slowToString();
        }
        result[key] += std::chrono::duration_cast<std::chrono::milliseconds>(stack.time).count();
    }
    return result;
}

class SamplerBuilder {
public:
    explicit SamplerBuilder(size_t maxSamples = 1000) : _sampler(maxSamples, kInterval) {}

    /** Adds `count` samples of `stack`, `gap` apart, each with a new generation as if the stack changed in between. */
    SamplerBuilder& add(size_t count,
                        const JavaScriptANRAttribution& stack,
                        std::chrono::milliseconds gap = kInterval) {
        for (size_t i = 0; i < count; ++i) {
            _now += gap;
            _sampler.addSample(++_generation, stack, _now);
        }
        return *this;
    }

    JavaScriptANRSampler& get() {
        return _sampler;
    }

private:
    JavaScriptANRSampler _sampler;
    JavaScriptANRSampler::TimePoint _now;
    uint64_t _generation = 0;
};

} // namespace

TEST(JavaScriptANRSampler, keepsEachDistinctStackWholeWithItsTotalTime) {
    SamplerBuilder builder;
    builder.add(2, makeStack({"render"}))
        .add(1, makeStack({"render", "runtime.loadJsModule", "runtime.loadJsModule(a)"}))
        .add(3, makeStack({"render"}))
        .add(1, JavaScriptANRAttribution());

    EXPECT_EQ((std::map<std::string, int64_t>{
                  {"", 50}, {"render", 250}, {"render -> runtime.loadJsModule -> runtime.loadJsModule(a)", 50}}),
              timeByStack(builder.get()));
}

TEST(JavaScriptANRSampler, weightsSamplesByTheTimeSinceThePreviousOne) {
    // Four on-time samples of A, then a sampling tick 400 ms late lands in B: B covers more of the window.
    SamplerBuilder builder;
    builder.add(4, makeStack({"A"})).add(1, makeStack({"B"}), std::chrono::milliseconds(400));

    EXPECT_EQ((std::map<std::string, int64_t>{{"A", 200}, {"B", 400}}), timeByStack(builder.get()));
}

TEST(JavaScriptANRSampler, addsTimeForUnchangedGenerationsWithoutReadingTheStack) {
    JavaScriptANRSampler sampler(100, kInterval);
    JavaScriptANRSampler::TimePoint now;
    sampler.addSample(7, makeStack({"createRoot(Profile)"}), now);
    for (int i = 0; i < 9; ++i) {
        now += kInterval;
        // The stack argument is ignored when the generation matches the previous sample.
        sampler.addSample(7, makeStack({"ignored"}), now);
    }

    EXPECT_EQ(10u, sampler.getSampleCount());
    EXPECT_EQ((std::map<std::string, int64_t>{{"createRoot(Profile)", 500}}), timeByStack(sampler));
}

TEST(JavaScriptANRSampler, treatsGenerationZeroAsUntracked) {
    JavaScriptANRSampler sampler(100, kInterval);
    JavaScriptANRSampler::TimePoint now;
    for (int i = 0; i < 5; ++i) {
        now += kInterval;
        sampler.addSample(0, makeStack({"A"}), now);
    }
    for (int i = 0; i < 6; ++i) {
        now += kInterval;
        sampler.addSample(0, makeStack({"B"}), now);
    }

    EXPECT_EQ((std::map<std::string, int64_t>{{"A", 250}, {"B", 300}}), timeByStack(sampler));
}

TEST(JavaScriptANRSampler, stopsAtMaxSamples) {
    SamplerBuilder builder(10);
    builder.add(25, makeStack({"A"}));

    EXPECT_TRUE(builder.get().isFull());
    EXPECT_EQ(10u, builder.get().getSampleCount());
    EXPECT_EQ((std::map<std::string, int64_t>{{"A", 500}}), timeByStack(builder.get()));
}

TEST(JavaScriptANRSampler, needsEnoughSamplesToOutvoteTheDeadline) {
    SamplerBuilder builder;
    builder.add(4, makeStack({"A"}));
    EXPECT_FALSE(builder.get().hasEnoughSamples());

    builder.add(1, makeStack({"A"}));
    EXPECT_TRUE(builder.get().hasEnoughSamples());
}

} // namespace ValdiTest
