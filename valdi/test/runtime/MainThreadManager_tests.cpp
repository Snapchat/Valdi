#include "valdi/runtime/Utils/MainThreadManager.hpp"

#include "valdi_test_utils.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <thread>

using namespace Valdi;

namespace ValdiTest {

TEST(MainThreadManagerTest, flushUpToNowRunsDispatchedTasks) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    bool ran = false;
    manager->dispatch(nullptr, [&ran]() { ran = true; });
    ASSERT_FALSE(ran);

    manager->flushUpToNow();
    ASSERT_TRUE(ran);

    mainQueue->flush();
}

TEST(MainThreadManagerTest, flushUpToNowSkipsTasksEnqueuedWhileFlushing) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    bool reenqueuedRan = false;
    manager->dispatch(nullptr, [&]() { manager->dispatch(nullptr, [&reenqueuedRan]() { reenqueuedRan = true; }); });

    // The re-enqueued task must wait for its own scheduled flush, or a task that
    // re-enqueues itself could keep the pump alive forever.
    manager->flushUpToNow();
    ASSERT_FALSE(reenqueuedRan);

    mainQueue->flush();
    ASSERT_TRUE(reenqueuedRan);
}

TEST(MainThreadManagerTest, flushUpToNowLeavesTaskEnqueuedByBatchedTaskForNextFlush) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    MainThreadBatchAllowScope allowScope;
    manager->beginBatch();

    bool reenqueuedRan = false;
    manager->dispatch(nullptr, [&]() { manager->dispatch(nullptr, [&reenqueuedRan]() { reenqueuedRan = true; }); });

    // An open batch stamps new tasks with the *current* flush id — the same id this flush caps
    // at — so unlike the unbatched case the cap alone would let them run in this same pump.
    manager->flushUpToNow();
    ASSERT_FALSE(reenqueuedRan);

    manager->endBatch();
    ASSERT_TRUE(reenqueuedRan);

    mainQueue->flush();
}

TEST(MainThreadManagerTest, flushUpToNowTerminatesWhenBatchedTaskReenqueuesItself) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    MainThreadBatchAllowScope allowScope;
    manager->beginBatch();

    int ranCount = 0;
    std::function<void()> reenqueueSelf;
    reenqueueSelf = [&]() {
        ranCount++;
        // Bounded so a regression reports as a failed assertion instead of hanging the suite.
        if (ranCount < 100) {
            manager->dispatch(nullptr, [&reenqueueSelf]() { reenqueueSelf(); });
        }
    };
    manager->dispatch(nullptr, [&reenqueueSelf]() { reenqueueSelf(); });

    manager->flushUpToNow();
    ASSERT_EQ(1, ranCount);

    manager->endBatch();
    mainQueue->flush();
}

TEST(MainThreadManagerTest, batchParksTasksUntilExplicitlyFlushed) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    MainThreadBatchAllowScope allowScope;
    manager->beginBatch();

    bool ran = false;
    manager->dispatch(nullptr, [&ran]() { ran = true; });

    // An open batch parks the task without scheduling a flush, so pumping the
    // dispatcher queue must not run it.
    mainQueue->flush();
    ASSERT_FALSE(ran);

    manager->flushUpToNow();
    ASSERT_TRUE(ran);

    manager->endBatch();
    mainQueue->flush();
}

TEST(MainThreadManagerTest, waitUntilQuiescentReturnsImmediatelyWhenIdle) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    bool quiescent = false;
    std::thread waiter([&]() { quiescent = manager->waitUntilQuiescent(std::chrono::milliseconds(1000)); });
    waiter.join();
    ASSERT_TRUE(quiescent);
}

TEST(MainThreadManagerTest, waitUntilQuiescentTimesOutWhileBatchParksTasks) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    MainThreadBatchAllowScope allowScope;
    manager->beginBatch();
    manager->dispatch(nullptr, []() {});

    bool quiescent = true;
    std::thread waiter([&]() { quiescent = manager->waitUntilQuiescent(std::chrono::milliseconds(20)); });
    waiter.join();
    ASSERT_FALSE(quiescent);

    manager->endBatch();
    mainQueue->flush();
}

TEST(MainThreadManagerTest, waitUntilQuiescentReleasesWhenBatchEnds) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    manager->markCurrentThreadIsMainThread();

    MainThreadBatchAllowScope allowScope;
    manager->beginBatch();
    bool ran = false;
    manager->dispatch(nullptr, [&ran]() { ran = true; });

    bool quiescent = false;
    std::thread waiter([&]() { quiescent = manager->waitUntilQuiescent(std::chrono::milliseconds(5000)); });

    // endBatch flushes the parked task inline and clears the batch, which is the
    // transition the waiter fences on.
    manager->endBatch();
    waiter.join();

    ASSERT_TRUE(quiescent);
    ASSERT_TRUE(ran);
    mainQueue->flush();
}

TEST(MainThreadManagerTest, preRasterFenceKillswitchDefaultsToFenceEnabled) {
    ASSERT_FALSE(MainThreadManager::isPreRasterFenceDisabled());

    MainThreadManager::setPreRasterFenceDisabled(true);
    ASSERT_TRUE(MainThreadManager::isPreRasterFenceDisabled());

    MainThreadManager::setPreRasterFenceDisabled(false);
    ASSERT_FALSE(MainThreadManager::isPreRasterFenceDisabled());
}

TEST(MainThreadManagerTest, waitUntilQuiescentCountsExecutingTasks) {
    auto mainQueue = makeShared<MainQueue>();
    auto manager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());

    std::atomic_bool started{false};
    std::atomic_bool release{false};
    manager->dispatch(nullptr, [&]() {
        started = true;
        while (!release) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread runner([&]() { manager->flushUpToNow(); });
    while (!started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // The queue is empty while the task body runs, but the manager is not quiescent:
    // the task may still be mutating views.
    ASSERT_FALSE(manager->waitUntilQuiescent(std::chrono::milliseconds(50)));

    release = true;
    runner.join();
    ASSERT_TRUE(manager->waitUntilQuiescent(std::chrono::milliseconds(1000)));

    mainQueue->flush();
}

} // namespace ValdiTest
