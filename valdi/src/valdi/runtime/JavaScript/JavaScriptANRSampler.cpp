#include "valdi/runtime/JavaScript/JavaScriptANRSampler.hpp"

#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"

#include <algorithm>

namespace Valdi {

namespace {

constexpr size_t kMinSamples = 5;

} // namespace

bool JavaScriptANRSampler::Stack::operator==(const Stack& other) const {
    return size == other.size && std::equal(labels.begin(), labels.begin() + size, other.labels.begin());
}

size_t JavaScriptANRSampler::StackHash::operator()(const Stack& stack) const {
    size_t hash = stack.size;
    for (size_t i = 0; i < stack.size; ++i) {
        hash ^= stack.labels[i].hash() + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    }
    return hash;
}

JavaScriptANRSampler::JavaScriptANRSampler(size_t maxSamples, Duration interval)
    : _interval(interval), _maxSamples(maxSamples) {}

JavaScriptANRSampler::~JavaScriptANRSampler() = default;

void JavaScriptANRSampler::sample(const JavaScriptTaskScheduler& taskScheduler, TimePoint now) {
    if (isFull() || addRepeatSample(taskScheduler.getANRAttributionGeneration(), now)) {
        return;
    }

    JavaScriptANRAttribution stack;
    auto generation = taskScheduler.copyANRAttribution(stack);
    addSample(generation, stack, now);
}

void JavaScriptANRSampler::addSample(uint64_t generation, const JavaScriptANRAttribution& stack, TimePoint now) {
    if (isFull() || addRepeatSample(generation, now)) {
        return;
    }

    Stack key;
    key.size = stack.size();
    for (size_t i = 0; i < key.size; ++i) {
        key.labels[i] = stack.getLabel(i);
    }

    auto& time = _timeByStack[std::move(key)];
    time += takeSampleTime(now);
    _previousStackTime = &time;
    _previousGeneration = generation;
}

bool JavaScriptANRSampler::addRepeatSample(uint64_t generation, TimePoint now) {
    if (generation == 0 || generation != _previousGeneration || _previousStackTime == nullptr) {
        return false;
    }

    *_previousStackTime += takeSampleTime(now);
    return true;
}

JavaScriptANRSampler::Duration JavaScriptANRSampler::takeSampleTime(TimePoint now) {
    auto time = _previousSampleTime.has_value() ? now - _previousSampleTime.value() : _interval;
    _previousSampleTime = now;
    ++_sampleCount;
    return time;
}

bool JavaScriptANRSampler::isFull() const {
    return _sampleCount >= _maxSamples;
}

size_t JavaScriptANRSampler::getSampleCount() const {
    return _sampleCount;
}

bool JavaScriptANRSampler::hasEnoughSamples() const {
    return _sampleCount >= kMinSamples;
}

std::vector<JavaScriptANRSampledStack> JavaScriptANRSampler::getSampledStacks() const {
    std::vector<JavaScriptANRSampledStack> stacks;
    stacks.reserve(_timeByStack.size());
    for (const auto& [stack, time] : _timeByStack) {
        auto& sampled = stacks.emplace_back();
        sampled.labels.assign(stack.labels.begin(), stack.labels.begin() + stack.size);
        sampled.time = time;
    }
    return stacks;
}

} // namespace Valdi
