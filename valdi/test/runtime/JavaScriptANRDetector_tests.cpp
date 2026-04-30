#include <gtest/gtest.h>

#include "TestANRDetectorListener.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"

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

    bool isInJsThread() override {
        return false;
    }

    Valdi::Ref<Valdi::Context> getLastDispatchedContext() const override {
        return nullptr;
    }

    std::vector<JavaScriptCapturedStacktrace> captureStackTraces(std::chrono::steady_clock::duration timeout) override {
        return {JavaScriptCapturedStacktrace(
            JavaScriptCapturedStacktrace::Status::RUNNING, STRING_LITERAL("A fake stacktrace"), nullptr)};
    }

    int getLastTaskId() {
        return _taskIdSequence;
    }

    void setShouldSimulateANR() {
        _shouldSimulateANR = true;
    }

private:
    std::atomic_bool _shouldSimulateANR = false;
    std::atomic_int _taskIdSequence = 0;
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

} // namespace ValdiTest
