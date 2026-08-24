#include "valdi_core/cpp/Utils/TrackedLock.hpp"

#include <thread>

namespace Valdi {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"

thread_local TrackedLock* kTopTrackedLock = nullptr;

TrackedLock::TrackedLock() = default;

TrackedLock::TrackedLock(RecursiveMutex& mutex) : _parent(kTopTrackedLock), _mutex(&mutex), _owns(true) {
    kTopTrackedLock = this;

    mutex.lock();
}

TrackedLock::TrackedLock(RecursiveMutex& mutex, const std::chrono::steady_clock::time_point& deadline)
    : _parent(kTopTrackedLock), _mutex(&mutex), _owns(false) {
    kTopTrackedLock = this;

    // Poll rather than block: std::recursive_mutex has no timed acquire, and swapping the type
    // would tax every RecursiveMutex user. 1ms granularity is negligible against the input
    // deadlines this serves.
    while (!(_owns = mutex.try_lock())) {
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TrackedLock::TrackedLock(TrackedLock&& other) noexcept : _mutex(other._mutex), _owns(other._owns) {
    other._mutex = nullptr;
    other._owns = false;
}

TrackedLock::~TrackedLock() {
    kTopTrackedLock = _parent;

    if (_owns) {
        _owns = false;
        _mutex->unlock();
    }
}

void TrackedLock::lock() {
    if (!_owns) {
        _owns = true;
        _mutex->lock();
    }
}

void TrackedLock::unlock() {
    if (_owns) {
        _owns = false;
        _mutex->unlock();
    }
}

void TrackedLock::suspendLock() {
    if (_owns) {
        _suspended = true;
        unlock();
    }
}

void TrackedLock::resumeLock() {
    if (_suspended) {
        _suspended = false;
        lock();
    }
}

bool TrackedLock::owns() const {
    return _owns;
}

TrackedLock& TrackedLock::operator=(TrackedLock&& other) noexcept {
    if (this != &other) {
        if (_owns) {
            _mutex->unlock();
        }

        _mutex = other._mutex;
        _owns = other._owns;

        other._mutex = nullptr;
        other._owns = false;
    }

    return *this;
}

#pragma clang diagnostic pop

DropAllTrackedLocks::DropAllTrackedLocks() {
    auto* current = kTopTrackedLock;
    while (current != nullptr) {
        current->suspendLock();
        current = current->_parent;
    }
}

void DropAllTrackedLocks::resumeOutermostFirst(TrackedLock* lock) {
    if (lock == nullptr) {
        return;
    }
    resumeOutermostFirst(lock->_parent);
    lock->resumeLock();
}

DropAllTrackedLocks::~DropAllTrackedLocks() {
    // Reacquire in original acquisition order (outermost first). Resuming innermost-first would
    // invert the order for threads holding locks on distinct mutexes: this thread would hold the
    // inner lock while blocking on the outer one, deadlocking against a thread acquiring them in
    // the normal order.
    resumeOutermostFirst(kTopTrackedLock);
}

} // namespace Valdi