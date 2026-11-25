#include <gtest/gtest.h>

#include "TestANRDetectorListener.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRAttribution.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRSampler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

class TestANRJavaScriptTaskScheduler : public JavaScriptTaskScheduler {
public:
    TestANRJavaScriptTaskScheduler() = default;
    ~TestANRJavaScriptTaskScheduler() override = default;

    JavaScriptEntryParameters* entryParametersPtr = nullptr;

    void dispatchOnJsThread(Ref<Context> ownerContext,
                            JavaScriptTaskScheduleType scheduleType,
                            uint32_t delayMs,
                            JavaScriptThreadTask&& function) override {
        if (_shouldSimulateANR) {
            return;
        } else {
            _taskIdSequence++;
            // Ptr is always null, but it's fine for this test
            function(*entryParametersPtr);
        }
    }

    void dispatchOnJsThread(JsThreadDispatchReason,
                            JavaScriptTaskScheduleType scheduleType,
                            uint32_t delayMs,
                            JavaScriptThreadTask&& function) override {
        dispatchOnJsThread(Ref<Context>(), scheduleType, delayMs, std::move(function));
    }

    bool isInJsThread() override {
        return false;
    }

    Valdi::Ref<Valdi::Context> getLastDispatchedContext() const override {
        return nullptr;
    }

    std::vector<JavaScriptCapturedStacktrace> captureStackTraces(std::chrono::steady_clock::duration timeout) override {
        if (_clearAttributionOnCapture) {
            _anrAttributionInfo.clear();
        }
        return {JavaScriptCapturedStacktrace(JavaScriptCapturedStacktrace::Status::RUNNING,
                                             STRING_LITERAL("A fake stacktrace"),
                                             nullptr,
                                             _stackBundleName)};
    }

    std::string getANRAttributionInfo() const override {
        if (!_anrAttributionInfo.empty()) {
            return _anrAttributionInfo;
        }
        // A test that drives the simulated chain reports it, like the runtime reports its live chain.
        std::lock_guard<Mutex> lock(_chainMutex);
        return _chain.format();
    }

    bool anrDiagnosticsEnabled() const override {
        return _anrDiagnosticsEnabled;
    }

    uint64_t getANRAttributionGeneration() const override {
        std::lock_guard<Mutex> lock(_chainMutex);
        return _chainGeneration;
    }

    uint64_t copyANRAttribution(JavaScriptANRAttribution& attribution) const override {
        std::lock_guard<Mutex> lock(_chainMutex);
        attribution = _chain;
        return _chainGeneration;
    }

    /** Simulates the JS thread moving into the scope named `label`. */
    void setChain(const char* label) {
        std::lock_guard<Mutex> lock(_chainMutex);
        _chain = JavaScriptANRAttribution();
        _chain.push(StringBox::fromCString(label));
        ++_chainGeneration;
        _anrDiagnosticsEnabled = true;
    }

    bool isReadyForANRDetection() const override {
        return _readyForANRDetection;
    }

    int getLastTaskId() {
        return _taskIdSequence;
    }

    void setShouldSimulateANR() {
        _shouldSimulateANR = true;
    }

    void setANRAttributionInfo(std::string anrAttributionInfo) {
        _anrAttributionInfo = std::move(anrAttributionInfo);
    }

    void setReadyForANRDetection(bool ready) {
        _readyForANRDetection = ready;
    }

    /** Simulates the native call returning while the detector waits on the capture. */
    void setClearAttributionOnCapture() {
        _clearAttributionOnCapture = true;
    }

    /** Simulates the symbolicator naming a feature bundle in the captured stack. */
    void setStackBundleName(StringBox stackBundleName) {
        _stackBundleName = std::move(stackBundleName);
    }

private:
    std::atomic_bool _shouldSimulateANR = false;
    std::atomic_bool _readyForANRDetection = true;
    std::atomic_bool _clearAttributionOnCapture = false;
    std::atomic_int _taskIdSequence = 0;
    std::atomic_bool _anrDiagnosticsEnabled = false;
    std::string _anrAttributionInfo;
    StringBox _stackBundleName;
    mutable Mutex _chainMutex;
    JavaScriptANRAttribution _chain;
    uint64_t _chainGeneration = 1;
};

struct ANRDetectorTestHelper {
    Ref<TestANRDetectorListener> listener;
    Ref<JavaScriptANRDetector> anrDetector;
    Ref<TestANRJavaScriptTaskScheduler> taskScheduler;

    ANRDetectorTestHelper() {
        listener = makeShared<TestANRDetectorListener>();
        anrDetector = makeShared<JavaScriptANRDetector>(Ref<ILogger>(&ConsoleLogger::getLogger()));
        anrDetector->setTickInterval(std::chrono::milliseconds(1));
        taskScheduler = makeShared<TestANRJavaScriptTaskScheduler>();
        anrDetector->appendTaskScheduler(taskScheduler.get());
        anrDetector->setListener(listener);
    }

    ~ANRDetectorTestHelper() {
        anrDetector->removeTaskScheduler(taskScheduler.get());
        anrDetector->stop();
    }

    void waitForNextTick() {
        auto group = makeShared<AsyncGroup>();
        group->enter();
        anrDetector->onNextTick([group]() { group->leave(); });

        if (!group->blockingWaitWithTimeout(std::chrono::seconds(5))) {
            throw Exception("Failed to wait for next tick");
        }
    }

    std::optional<JavaScriptANR> getLastANR() {
        return listener->getLastANR();
    }

    std::optional<JavaScriptANR> waitForANR() {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!getLastANR().has_value() && std::chrono::steady_clock::now() < deadline) {
            waitForNextTick();
        }
        return getLastANR();
    }
};

TEST(ANRDetector, doesNotDetectANRWhenTaskAreNotHanging) {
    ANRDetectorTestHelper helper;

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(1));

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    auto anr = helper.getLastANR();
    ASSERT_FALSE(anr.has_value());

    auto taskId = helper.taskScheduler->getLastTaskId();
    // TaskId should have increased at least 3 times
    ASSERT_TRUE(taskId > 2);
}

TEST(ANRDetector, detectsANRWhenTaskAreHanging) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(1));

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    auto anr = helper.getLastANR();
    ASSERT_TRUE(anr.has_value());

    ASSERT_EQ(static_cast<size_t>(1), anr->getCapturedStacktraces().size());
    ASSERT_EQ(STRING_LITERAL("A fake stacktrace"), anr->getCapturedStacktraces()[0].getStackTrace());
    ASSERT_EQ("Detected unattributed ANR after 1.0 ms", anr->getMessage());
}

TEST(ANRDetector, doesNotDetectANRWhileSchedulerIsNotReadyForDetection) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();
    helper.taskScheduler->setReadyForANRDetection(false);

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(1));

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    ASSERT_FALSE(helper.getLastANR().has_value());

    helper.taskScheduler->setReadyForANRDetection(true);

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    auto anr = helper.getLastANR();
    ASSERT_TRUE(anr.has_value());
    ASSERT_EQ("Detected unattributed ANR after 1.0 ms", anr->getMessage());
}

TEST(ANRDetector, givesStaleInFlightSynAFreshBudgetOnEnterForeground) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(100));

    // Arms a syn probe that will never be acked.
    helper.waitForNextTick();

    helper.anrDetector->onEnterBackground();
    // Simulate a process suspension outlasting the detection threshold: the clock keeps running
    // while ticks are paused, so the armed probe goes stale.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    helper.anrDetector->onEnterForeground();

    // Without the foreground re-arm this tick would report an ANR measured across the suspension.
    helper.waitForNextTick();
    ASSERT_FALSE(helper.getLastANR().has_value());

    // A hang that persists after resume is still detected once a full budget elapses in
    // foreground.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    helper.waitForNextTick();
    ASSERT_TRUE(helper.getLastANR().has_value());
}

TEST(ANRDetector, includesANRAttributionInfoInMessageWhenSet) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();
    helper.taskScheduler->setANRAttributionInfo(
        " [stuck-in: Graphene.partitionMakeMetric]"
        " [attribution: Search.load -> SharedWrapper.load -> Graphene.partitionMakeMetric] [module: search_v2]");

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(1));

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    auto anr = helper.getLastANR();
    ASSERT_TRUE(anr.has_value());

    ASSERT_EQ("Detected unattributed ANR after 1.0 ms [stuck-in: Graphene.partitionMakeMetric]"
              " [attribution: Search.load -> SharedWrapper.load -> Graphene.partitionMakeMetric] [module: search_v2]",
              anr->getMessage());
}

TEST(ANRDetector, keepsANRAttributionInfoWhenNativeCallEndsDuringCapture) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();
    helper.taskScheduler->setANRAttributionInfo(" [stuck-in: runtime.loadJsModule(coreui/src/InitSemanticColors)]");
    helper.taskScheduler->setClearAttributionOnCapture();

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(1));

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    auto anr = helper.getLastANR();
    ASSERT_TRUE(anr.has_value());

    ASSERT_EQ("Detected unattributed ANR after 1.0 ms [stuck-in: runtime.loadJsModule(coreui/src/InitSemanticColors)]",
              anr->getMessage());
}

TEST(ANRDetector, attributesANRToBundleNamedByStackWhenContextHasNone) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();
    helper.taskScheduler->setStackBundleName(STRING_LITERAL("send_to"));
    helper.taskScheduler->setANRAttributionInfo(" [stuck-in: friendmojiProvider.forUsers]");

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(1));

    helper.waitForNextTick();
    helper.waitForNextTick();
    helper.waitForNextTick();

    auto anr = helper.getLastANR();
    ASSERT_TRUE(anr.has_value());

    ASSERT_EQ("Detected ANR in 'send_to' after 1.0 ms [stuck-in: friendmojiProvider.forUsers]", anr->getMessage());
    ASSERT_EQ(STRING_LITERAL("send_to"), anr->getModuleName());
}

TEST(ANRDetector, addsWhereTheTimeWentWhenSampling) {
    ANRDetectorTestHelper helper;
    JavaScriptANRSamplingConfig samplingConfig;
    samplingConfig.enabled = true;
    samplingConfig.interval = std::chrono::milliseconds(1);
    samplingConfig.maxSamples = 10000;
    helper.anrDetector->setSamplingConfig(samplingConfig);

    helper.taskScheduler->setShouldSimulateANR();
    // The backlog holds the thread for the whole window while the deadline reports a render, so neither line depends
    // on when the test thread or the detector thread gets scheduled.
    helper.taskScheduler->setChain("Backlog.item");
    helper.taskScheduler->setANRAttributionInfo(" [stuck-in: UI.render]");

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(200));

    auto anr = helper.waitForANR();
    ASSERT_TRUE(anr.has_value());
    // The first line keeps naming what ran at the deadline; the tree below it leads with the backlog. Its times
    // vary by run.
    const auto& message = anr->getMessage();
    auto lines = std::vector<std::string>();
    for (size_t start = 0; start <= message.size();) {
        auto end = std::min(message.find('\n', start), message.size());
        lines.emplace_back(message.substr(start, end - start));
        start = end + 1;
    }
    ASSERT_GE(lines.size(), 3u) << message;
    EXPECT_EQ("Detected unattributed ANR after 200.0 ms [stuck-in: UI.render]", lines[0]);
    EXPECT_EQ("Sampled ", lines[1].substr(0, 8)) << message;
    EXPECT_NE(std::string::npos, lines[1].find(" ms of the 200 ms ANR window")) << message;
    EXPECT_NE(std::string::npos, lines[2].find(" ms  Backlog.item")) << message;
    EXPECT_EQ("", lines.back()) << message;
}

TEST(ANRDetector, keepsTheDeadlineAttributionWhenSamplingIsOff) {
    ANRDetectorTestHelper helper;

    helper.taskScheduler->setShouldSimulateANR();
    helper.taskScheduler->setChain("Backlog.item");
    helper.taskScheduler->setANRAttributionInfo(" [stuck-in: UI.render]");

    helper.anrDetector->onEnterForeground();
    helper.anrDetector->start(std::chrono::milliseconds(50));

    auto anr = helper.waitForANR();
    ASSERT_TRUE(anr.has_value());
    ASSERT_EQ("Detected unattributed ANR after 50.0 ms [stuck-in: UI.render]", anr->getMessage());
}

} // namespace ValdiTest
