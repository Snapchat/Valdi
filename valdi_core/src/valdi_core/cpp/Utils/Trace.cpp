//
//  Trace.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/8/19.
//

#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <memory>
#include <unordered_map>

namespace Valdi {

namespace {

struct DroppedTraceEventCount {
    size_t recordingSequence;
    size_t count;
};

struct TracerRecordingState {
    size_t pendingTraceNameBytes = 0;
    std::vector<DroppedTraceEventCount> pendingDroppedTraceEventCounts;
};

struct TracerRecordingStateRegistry {
    std::mutex mutex;
    std::unordered_map<const Tracer*, std::unique_ptr<TracerRecordingState>> states;
};

TracerRecordingStateRegistry& getTracerRecordingStateRegistry() {
    // Tracer::shared() intentionally outlives process teardown. Keeping its auxiliary registry on
    // the same lifetime avoids static-destruction ordering hazards while preserving Tracer's public
    // object layout for existing native clients.
    static auto* registry = new TracerRecordingStateRegistry();
    return *registry;
}

void registerTracerRecordingState(const Tracer* tracer) {
    auto& registry = getTracerRecordingStateRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto [it, inserted] = registry.states.emplace(tracer, std::make_unique<TracerRecordingState>());
    if (!inserted) {
        it->second = std::make_unique<TracerRecordingState>();
    }
}

TracerRecordingState* getTracerRecordingState(const Tracer* tracer) {
    auto& registry = getTracerRecordingStateRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto it = registry.states.find(tracer);
    if (it == registry.states.end()) {
        auto insertedIt = registry.states.emplace(tracer, std::make_unique<TracerRecordingState>()).first;
        return insertedIt->second.get();
    }
    return it->second.get();
}

void unregisterTracerRecordingState(const Tracer* tracer) {
    auto& registry = getTracerRecordingStateRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.states.erase(tracer);
}

} // namespace

std::string getTraceName(std::string_view prefix, const StringBox& suffix) {
    return getTraceName(prefix, suffix.toStringView());
}

std::string getTraceName(std::string_view prefix, std::string_view suffix) {
    std::string out;
    out.reserve(prefix.size() + suffix.size() + 1);

    out += prefix;
    out.append(1, '.');
    out += suffix;
    return out;
}

TraceDuration RecordedTrace::duration() const {
    return TraceDuration(end - start);
}

ScopedTrace::ScopedTrace(const StringBox& trace)
    : _ownedTrace(trace.toStringView()), _trace(_ownedTrace), _snapTrace(_trace) {
    start();
}

ScopedTrace::ScopedTrace(std::string&& trace) : _ownedTrace(std::move(trace)), _trace(_ownedTrace), _snapTrace(_trace) {
    start();
}

ScopedTrace::~ScopedTrace() {
    std::optional<TraceTimePoint> endTime;
    if (_startTime) {
        endTime = std::chrono::steady_clock::now();
    }

    // Runs before the name is handed to the Tracer, which may move _ownedTrace out from under
    // _trace.
    end();

    if (_startTime) {
        std::string trace;
        if (_ownedTrace.empty()) {
            // Literal name: only worth materializing now that we know a recorder wants it.
            trace = std::string(_trace);
        } else {
            // _trace views _ownedTrace, so this must not read _trace afterwards.
            trace = std::move(_ownedTrace);
        }

        Tracer::shared().append(std::move(trace), _startTime.value(), endTime.value());
    }
}

void ScopedTrace::start() {
    begin();

    if (Tracer::shared().isRecording()) {
        _startTime = {std::chrono::steady_clock::now()};
    }
}

void ScopedTrace::begin() {
    snap::profiling::TraceBegin traceBegin;
    traceBegin.name = _trace;

    _osEmitter.begin(traceBegin);
}

void ScopedTrace::end() {
    snap::profiling::TraceEnd traceEnd;
    traceEnd.name = _trace;

    _osEmitter.end(traceEnd);
}

Tracer::Tracer() {
    registerTracerRecordingState(this);
}

Tracer::~Tracer() {
    unregisterTracerRecordingState(this);
}

Tracer& Tracer::shared() {
    static auto* kInstance = new Tracer();
    return *kInstance;
}

void Tracer::append(std::string&& trace, const TraceTimePoint& start, const TraceTimePoint& end) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_recorders.empty()) {
        return;
    }

    auto* recordingState = getTracerRecordingState(this);
    const auto traceNameBytes = trace.size();
    if (traceNameBytes > kMaxRecordedTraceNameLengthBytes || _pendingTraces.size() >= kMaxRecordedTraceCount ||
        traceNameBytes > kMaxRecordedTraceNameBytes - recordingState->pendingTraceNameBytes) {
        if (recordingState->pendingDroppedTraceEventCounts.empty() ||
            recordingState->pendingDroppedTraceEventCounts.back().recordingSequence != _recordingSequence) {
            recordingState->pendingDroppedTraceEventCounts.push_back({_recordingSequence, 1});
        } else {
            recordingState->pendingDroppedTraceEventCounts.back().count++;
        }
        return;
    }

    _pendingTraces.emplace_back(std::move(trace), start, end, getCurrentThreadId(), _recordingSequence);
    recordingState->pendingTraceNameBytes += traceNameBytes;
}

size_t Tracer::startRecording() {
    std::lock_guard<std::mutex> lock(_mutex);
    auto sequence = ++_recordingSequence;
    _recording = true;

    _recorders.emplace_back(sequence);

    return sequence;
}

std::vector<RecordedTrace> Tracer::stopRecording(size_t recordingIdentifier) {
    return stopRecordingWithStats(recordingIdentifier).traces;
}

TraceRecordingResult Tracer::stopRecordingWithStats(size_t recordingIdentifier) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = std::find(_recorders.begin(), _recorders.end(), recordingIdentifier);
    if (it == _recorders.end()) {
        return {};
    }

    _recorders.erase(it);

    // Simple case, we only have one recorder we can return all the recorded traces
    if (_recorders.empty()) {
        _recording = false;
        auto* recordingState = getTracerRecordingState(this);
        recordingState->pendingTraceNameBytes = 0;
        TraceRecordingResult result;
        result.traces = std::move(_pendingTraces);
        for (const auto& droppedTraceEventCount : recordingState->pendingDroppedTraceEventCounts) {
            if (droppedTraceEventCount.recordingSequence >= recordingIdentifier) {
                result.droppedTraceEventCount += droppedTraceEventCount.count;
            }
        }
        _pendingTraces.clear();
        recordingState->pendingDroppedTraceEventCounts.clear();
        return result;
    }

    // We still have one active recorder. We collect the traces that ocurreded with or after
    // this identifier

    TraceRecordingResult result;

    for (const auto& trace : _pendingTraces) {
        if (trace.recordingSequence >= recordingIdentifier) {
            result.traces.emplace_back(trace);
        }
    }
    auto* recordingState = getTracerRecordingState(this);
    for (const auto& droppedTraceEventCount : recordingState->pendingDroppedTraceEventCounts) {
        if (droppedTraceEventCount.recordingSequence >= recordingIdentifier) {
            result.droppedTraceEventCount += droppedTraceEventCount.count;
        }
    }

    auto lowestRecordingIdentifier = *_recorders.begin();
    if (lowestRecordingIdentifier > recordingIdentifier) {
        // If the next lowest recording identifier is above the ending identifier,
        // we might have dangling traces to remove.
        // Remove all the traces that occured before the new lowest recording identifier.
        auto newStartIt = _pendingTraces.begin();
        while (newStartIt != _pendingTraces.end() && newStartIt->recordingSequence < lowestRecordingIdentifier) {
            newStartIt++;
        }
        for (auto it = _pendingTraces.begin(); it != newStartIt; ++it) {
            recordingState->pendingTraceNameBytes -= it->trace.size();
        }
        _pendingTraces.erase(_pendingTraces.begin(), newStartIt);

        auto newDroppedStartIt = recordingState->pendingDroppedTraceEventCounts.begin();
        while (newDroppedStartIt != recordingState->pendingDroppedTraceEventCounts.end() &&
               newDroppedStartIt->recordingSequence < lowestRecordingIdentifier) {
            newDroppedStartIt++;
        }
        recordingState->pendingDroppedTraceEventCounts.erase(recordingState->pendingDroppedTraceEventCounts.begin(),
                                                              newDroppedStartIt);
    }

    return result;
}

} // namespace Valdi
