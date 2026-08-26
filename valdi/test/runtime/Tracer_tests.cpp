#include "valdi_core/cpp/Utils/Trace.hpp"
#include <gtest/gtest.h>

#include <type_traits>

using namespace Valdi;

namespace ValdiTest {

using LegacyStopRecordingSignature = std::vector<RecordedTrace> (Tracer::*)(size_t);
static_assert(std::is_same<decltype(&Tracer::stopRecording), LegacyStopRecordingSignature>::value,
              "Tracer::stopRecording must preserve its public source and ABI contract");

struct LegacyTracerLayout {
    std::mutex mutex;
    std::atomic_bool recording = false;
    size_t recordingSequence = 0;
    std::vector<RecordedTrace> pendingTraces;
    std::vector<size_t> recorders;
};

static_assert(sizeof(Tracer) == sizeof(LegacyTracerLayout), "Tracer must preserve its public object size");
static_assert(alignof(Tracer) == alignof(LegacyTracerLayout), "Tracer must preserve its public object alignment");

TraceTimePoint appendMs(const TraceTimePoint& start, int ms) {
    return start + std::chrono::milliseconds(ms);
}

TEST(Tracer, canRecordOperations) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();

    auto id = tracer.startRecording();

    tracer.append("hello", start, appendMs(start, 50));
    tracer.append("world", start, appendMs(start, 100));

    auto result = tracer.stopRecording(id);

    ASSERT_EQ(static_cast<size_t>(2), result.size());
    ASSERT_EQ("hello", result[0].trace);
    ASSERT_EQ("world", result[1].trace);
    ASSERT_EQ(50.0, result[0].duration().milliseconds());
    ASSERT_EQ(100.0, result[1].duration().milliseconds());
}

TEST(Tracer, returnsEmptyOnInvalidRecordId) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();

    auto id = tracer.startRecording();

    tracer.append("hello", start, appendMs(start, 50));
    tracer.append("world", start, appendMs(start, 100));

    auto result = tracer.stopRecording(id + 1);
    ASSERT_TRUE(result.empty());
}

TEST(Tracer, canRecordOperationsConcurrentlyWithSequentialOrdering) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();

    auto id = tracer.startRecording();

    tracer.append("hello", start, appendMs(start, 50));

    auto id2 = tracer.startRecording();

    tracer.append("world", start, appendMs(start, 100));

    auto result = tracer.stopRecording(id);
    auto result2 = tracer.stopRecording(id2);

    // Result1 should have both traces
    ASSERT_EQ(static_cast<size_t>(2), result.size());
    ASSERT_EQ("hello", result[0].trace);
    ASSERT_EQ("world", result[1].trace);
    ASSERT_EQ(50.0, result[0].duration().milliseconds());
    ASSERT_EQ(100.0, result[1].duration().milliseconds());

    // Result2 should have only 1
    ASSERT_EQ(static_cast<size_t>(1), result2.size());
    ASSERT_EQ("world", result2[0].trace);
    ASSERT_EQ(100.0, result2[0].duration().milliseconds());
}

TEST(Tracer, canRecordOperationsConcurrentlyWithRandomOrdering) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();

    auto id = tracer.startRecording();

    tracer.append("hello", start, appendMs(start, 50));

    auto id2 = tracer.startRecording();

    tracer.append("world", start, appendMs(start, 100));

    auto result2 = tracer.stopRecording(id2);
    auto result = tracer.stopRecording(id);

    // Result1 should have both traces
    ASSERT_EQ(static_cast<size_t>(2), result.size());
    ASSERT_EQ("hello", result[0].trace);
    ASSERT_EQ("world", result[1].trace);
    ASSERT_EQ(50.0, result[0].duration().milliseconds());
    ASSERT_EQ(100.0, result[1].duration().milliseconds());

    // Result2 should have only 1
    ASSERT_EQ(static_cast<size_t>(1), result2.size());
    ASSERT_EQ("world", result2[0].trace);
    ASSERT_EQ(100.0, result2[0].duration().milliseconds());
}

TEST(Tracer, onlyReturnsRecordingWhenLastRecorderEnd) {
    Tracer tracer;

    ASSERT_FALSE(tracer.isRecording());

    auto id1 = tracer.startRecording();

    ASSERT_TRUE(tracer.isRecording());

    auto id2 = tracer.startRecording();

    ASSERT_TRUE(tracer.isRecording());

    tracer.stopRecording(id2);

    ASSERT_TRUE(tracer.isRecording());

    tracer.stopRecording(id1);

    ASSERT_FALSE(tracer.isRecording());
}

TEST(Tracer, boundsRecordedTraceCountBeforeSerialization) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();
    auto id = tracer.startRecording();

    for (size_t i = 0; i < Tracer::kMaxRecordedTraceCount + 1; i++) {
        tracer.append("trace", start, appendMs(start, 1));
    }

    auto result = tracer.stopRecordingWithStats(id);
    ASSERT_EQ(Tracer::kMaxRecordedTraceCount, result.traces.size());
    ASSERT_EQ(static_cast<size_t>(1), result.droppedTraceEventCount);
}

TEST(Tracer, doesNotReportTruncationAtExactlyTheRecordedTraceLimit) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();
    auto id = tracer.startRecording();

    for (size_t i = 0; i < Tracer::kMaxRecordedTraceCount; i++) {
        tracer.append("trace", start, appendMs(start, 1));
    }

    auto result = tracer.stopRecordingWithStats(id);
    ASSERT_EQ(Tracer::kMaxRecordedTraceCount, result.traces.size());
    ASSERT_EQ(static_cast<size_t>(0), result.droppedTraceEventCount);
}

TEST(Tracer, dropsOversizedTraceNamesBeforeSerialization) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();
    auto id = tracer.startRecording();

    tracer.append(std::string(Tracer::kMaxRecordedTraceNameLengthBytes + 1, 'x'), start, appendMs(start, 1));
    tracer.append("valid", start, appendMs(start, 1));

    auto result = tracer.stopRecordingWithStats(id);
    ASSERT_EQ(static_cast<size_t>(1), result.traces.size());
    ASSERT_EQ(static_cast<size_t>(1), result.droppedTraceEventCount);
    ASSERT_EQ("valid", result.traces[0].trace);
}

TEST(Tracer, boundsAggregateTraceNameBytesBeforeSerialization) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();
    auto id = tracer.startRecording();
    const auto traceName = std::string(Tracer::kMaxRecordedTraceNameLengthBytes, 'x');
    const auto expectedCount = Tracer::kMaxRecordedTraceNameBytes / traceName.size();

    for (size_t i = 0; i < expectedCount + 1; i++) {
        tracer.append(std::string(traceName), start, appendMs(start, 1));
    }

    auto result = tracer.stopRecordingWithStats(id);
    ASSERT_EQ(expectedCount, result.traces.size());
    ASSERT_EQ(static_cast<size_t>(1), result.droppedTraceEventCount);
}

TEST(Tracer, reportsDroppedEventsToEachConcurrentRecorderThatObservedThem) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();
    const auto oversizedTraceName = std::string(Tracer::kMaxRecordedTraceNameLengthBytes + 1, 'x');
    auto firstId = tracer.startRecording();
    tracer.append(std::string(oversizedTraceName), start, appendMs(start, 1));
    auto secondId = tracer.startRecording();
    tracer.append(std::string(oversizedTraceName), start, appendMs(start, 1));

    auto firstResult = tracer.stopRecordingWithStats(firstId);
    auto secondResult = tracer.stopRecordingWithStats(secondId);

    ASSERT_EQ(static_cast<size_t>(2), firstResult.droppedTraceEventCount);
    ASSERT_EQ(static_cast<size_t>(1), secondResult.droppedTraceEventCount);
}

TEST(Tracer, retainsConcurrentDroppedCountsWhenTheNewestRecorderStopsFirst) {
    Tracer tracer;
    auto start = std::chrono::steady_clock::now();
    const auto oversizedTraceName = std::string(Tracer::kMaxRecordedTraceNameLengthBytes + 1, 'x');
    auto firstId = tracer.startRecording();
    tracer.append(std::string(oversizedTraceName), start, appendMs(start, 1));
    auto secondId = tracer.startRecording();
    tracer.append(std::string(oversizedTraceName), start, appendMs(start, 1));

    auto secondResult = tracer.stopRecordingWithStats(secondId);
    auto firstResult = tracer.stopRecordingWithStats(firstId);

    ASSERT_EQ(static_cast<size_t>(1), secondResult.droppedTraceEventCount);
    ASSERT_EQ(static_cast<size_t>(2), firstResult.droppedTraceEventCount);
}

} // namespace ValdiTest
