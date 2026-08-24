#pragma once

#include "valdi_core/cpp/Utils/Mutex.hpp"

#include <chrono>

namespace Valdi {

class DropAllTrackedLocks;

/**
A lock implementation for Valdi::RecursiveMutex which uses a thread local storage
to track all the locks acquired so that it can unlock them.
 */
class TrackedLock {
public:
    TrackedLock();
    TrackedLock(RecursiveMutex& mutex);
    /**
     Try to acquire the mutex until the deadline, polling instead of blocking, so the calling
     thread can never park on the mutex indefinitely. A deadline in the past attempts exactly
     once. Check owns() for the outcome.
     */
    TrackedLock(RecursiveMutex& mutex, const std::chrono::steady_clock::time_point& deadline);
    TrackedLock(TrackedLock&& other) noexcept;

    ~TrackedLock();

    void lock();
    void unlock();

    bool owns() const;

    TrackedLock& operator=(TrackedLock&& other) noexcept;

private:
    friend DropAllTrackedLocks;

    TrackedLock* _parent = nullptr;
    RecursiveMutex* _mutex = nullptr;
    bool _owns = false;
    bool _suspended = false;

    void suspendLock();
    void resumeLock();
};

class DropAllTrackedLocks {
public:
    DropAllTrackedLocks();
    ~DropAllTrackedLocks();

private:
    static void resumeOutermostFirst(TrackedLock* lock);
};

} // namespace Valdi