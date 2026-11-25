#pragma once

#include "valdi/runtime/JavaScript/JavaScriptANRAttribution.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRSampleTree.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Valdi {

class JavaScriptTaskScheduler;

/** How the ANR detector samples attribution while a probe waits, from the VALDI_ANR_SAMPLING_* tweaks. */
struct JavaScriptANRSamplingConfig {
    bool enabled = false;
    /** Time between samples. Shorter intervals give more precise times and wake the detector more often. */
    std::chrono::steady_clock::duration interval = std::chrono::milliseconds(50);
    /** Most samples kept per ANR window. Bounds the sampler's memory; sampling stops once reached. */
    size_t maxSamples = 128;
};

/**
 * Samples the breadcrumb stack a JS thread is in while an ANR probe waits behind its work, and reduces the samples to
 * the time spent in each distinct stack. buildSampleTree turns the result into the tree an ANR report shows.
 *
 * Each sample adds the time since the previous sample to the stack it found, so a late sampling tick still counts for
 * the time it covers. Each distinct stack is kept once, down to its innermost frame, and a stack is only copied when
 * the scheduler reports that it changed, so memory and per-sample work stay bounded by maxSamples however long the
 * stall lasts. Not thread-safe: the ANR detector drives it from its own queue.
 */
class JavaScriptANRSampler {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;

    /** `interval` is the time the first sample covers, since nothing precedes it. */
    JavaScriptANRSampler(size_t maxSamples, Duration interval);
    ~JavaScriptANRSampler();

    JavaScriptANRSampler(const JavaScriptANRSampler&) = delete;
    JavaScriptANRSampler& operator=(const JavaScriptANRSampler&) = delete;

    /** Samples the stack `taskScheduler` is in right now. No-op once maxSamples have been taken. */
    void sample(const JavaScriptTaskScheduler& taskScheduler, TimePoint now);

    /**
     * Records one sample of `stack`. `generation` identifies the stack's state: a non-zero generation equal to the
     * previous sample's means the stack has not changed since, and `stack` is not read.
     */
    void addSample(uint64_t generation, const JavaScriptANRAttribution& stack, TimePoint now);

    bool isFull() const;
    size_t getSampleCount() const;

    /** Whether there are enough samples to outvote the deadline stack, the single read the report already has. */
    bool hasEnoughSamples() const;

    /** Each distinct stack sampled so far with its total sampled time. */
    std::vector<JavaScriptANRSampledStack> getSampledStacks() const;

private:
    struct Stack {
        std::array<StringBox, JavaScriptANRAttribution::kMaxFrames> labels;
        size_t size = 0;

        bool operator==(const Stack& other) const;
    };

    struct StackHash {
        size_t operator()(const Stack& stack) const;
    };

    std::unordered_map<Stack, Duration, StackHash> _timeByStack;
    // Map values keep their address across rehashes, so this stays valid while the map grows.
    Duration* _previousStackTime = nullptr;
    uint64_t _previousGeneration = 0;
    std::optional<TimePoint> _previousSampleTime;
    Duration _interval;
    size_t _sampleCount = 0;
    size_t _maxSamples;

    bool addRepeatSample(uint64_t generation, TimePoint now);
    Duration takeSampleTime(TimePoint now);
};

} // namespace Valdi
