//
//  Threading_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 03/05/23
//

#include "valdi_core/cpp/Threading/TaskQueue.hpp"
#include "valdi_core/cpp/Threading/ThreadedDispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/TrackedLock.hpp"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace Valdi;

namespace ValdiTest {

TEST(ThreadedDispatchQueue, startsThreadOnFirstTaskEnqueued) {
    auto dispatchQueue = makeShared<ThreadedDispatchQueue>(STRING_LITERAL("Test Queue"), ThreadQoSClassNormal);

    ASSERT_FALSE(dispatchQueue->hasThreadRunning());

    bool didRun = false;
    dispatchQueue->sync([&]() { didRun = true; });

    ASSERT_TRUE(didRun);
    // Sync calls should not trigger the thread creation
    ASSERT_FALSE(dispatchQueue->hasThreadRunning());

    dispatchQueue->async([]() {});

    ASSERT_TRUE(dispatchQueue->hasThreadRunning());
}

struct TestDtor {
    DispatchFunction cb;

    TestDtor(DispatchFunction cb) : cb(std::move(cb)) {}

    ~TestDtor() {
        if (cb) {
            cb();
        }
    }
};

TEST(TaskQueue, canBeInteractedWithInDestructor) {
    size_t dtorCallCount = 0;

    {
        TaskQueue taskQueue;
        auto id1 = taskQueue.enqueue([]() {}).id;

        DispatchFunction dtor = [&]() {
            taskQueue.cancel(id1);
            dtorCallCount++;
        };

        auto id2 = taskQueue.enqueue([myDtor = makeShared<TestDtor>(std::move(dtor))] {}).id;

        ASSERT_EQ(static_cast<size_t>(0), dtorCallCount);

        taskQueue.cancel(id2);

        ASSERT_EQ(static_cast<size_t>(1), dtorCallCount);
    }
    ASSERT_EQ(static_cast<size_t>(1), dtorCallCount);
}

TEST(TaskQueue, barrierDoesNotAllowOtherTaskToRun) {
    auto executeTime = std::chrono::steady_clock::now();
    TaskQueue taskQueue;
    bool innerTaskRan = false;

    taskQueue.barrier([&]() {
        auto thread = Thread::create(STRING_LITERAL("Test Thread"), ThreadQoSClassMax, [&]() {
                          taskQueue.enqueue([&]() { innerTaskRan = true; },
                                            executeTime /* Pass an execute time that is before us */);

                          ASSERT_FALSE(taskQueue.runNextTask());
                      }).value();
        thread->join();
    });

    ASSERT_FALSE(innerTaskRan);
    ASSERT_TRUE(taskQueue.runNextTask());
    ASSERT_TRUE(innerTaskRan);
}

TEST(TrackedLock, canDropTrackedLocks) {
    auto mutex = makeShared<RecursiveMutex>();
    TrackedLock lock1(*mutex);
    TrackedLock lock2(*mutex);
    TrackedLock lock3(*mutex);

    lock2.unlock();

    ASSERT_TRUE(lock1.owns());
    ASSERT_FALSE(lock2.owns());
    ASSERT_TRUE(lock3.owns());

    {
        DropAllTrackedLocks dropAllTrackedLocks;
        ASSERT_FALSE(lock1.owns());
        ASSERT_FALSE(lock2.owns());
        ASSERT_FALSE(lock3.owns());
    }

    ASSERT_TRUE(lock1.owns());
    ASSERT_FALSE(lock2.owns());
    ASSERT_TRUE(lock3.owns());
}

TEST(TrackedLock, resumesLocksInAcquisitionOrder) {
    // Regression test: ~DropAllTrackedLocks must reacquire outermost-first. If it resumed
    // innermost-first, this thread would grab `inner` and then block on `outer` (held by the
    // helper thread below) while keeping `inner` locked -- the helper's try_lock of `inner`
    // observes exactly that inversion.
    auto outerMutex = makeShared<RecursiveMutex>();
    auto innerMutex = makeShared<RecursiveMutex>();

    TrackedLock outer(*outerMutex);
    TrackedLock inner(*innerMutex);

    std::atomic_bool helperHoldsOuter(false);
    std::atomic_bool innerWasFree(false);
    std::atomic_bool helperDone(false);

    std::thread helper;
    {
        DropAllTrackedLocks dropAllTrackedLocks;
        ASSERT_FALSE(outer.owns());
        ASSERT_FALSE(inner.owns());

        helper = std::thread([&]() {
            outerMutex->lock();
            helperHoldsOuter = true;
            // Give the main thread time to reach the resume path and block on `outer`.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            // Outermost-first resume means `inner` must still be free at this point.
            if (innerMutex->try_lock()) {
                innerWasFree = true;
                innerMutex->unlock();
            }
            outerMutex->unlock();
            helperDone = true;
        });

        while (!helperHoldsOuter) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Destructor resumes the locks here, blocking on `outer` until the helper releases it.
    }

    helper.join();
    ASSERT_TRUE(helperDone);
    ASSERT_TRUE(innerWasFree);
    ASSERT_TRUE(outer.owns());
    ASSERT_TRUE(inner.owns());
}

} // namespace ValdiTest
