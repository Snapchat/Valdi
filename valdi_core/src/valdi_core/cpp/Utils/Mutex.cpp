//
//  Mutex.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/4/19.
//

#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "utils/debugging/Assert.hpp"

namespace Valdi {

#ifdef DEBUG
MutexThreadChecker::MutexThreadChecker() : _storage(std::make_unique<MutexThreadChecker::Storage>()) {}

MutexThreadChecker::~MutexThreadChecker() = default;

void MutexThreadChecker::onLock() {
    _storage->threadIdWithLock.store(std::this_thread::get_id());
}

void MutexThreadChecker::onUnlock() {
    _storage->threadIdWithLock.store(std::thread::id());
}

void MutexThreadChecker::onLockRefCounted() {
    _storage->threadIdWithLock.store(std::this_thread::get_id());
    _storage->refCount++;
}

void MutexThreadChecker::onUnlockRefCounted() {
    if (--_storage->refCount == 0) {
        _storage->threadIdWithLock.store(std::thread::id());
    }
}

void MutexThreadChecker::assertIsLocked() {
    auto currentThreadId = std::this_thread::get_id();
    SC_ASSERT(_storage->threadIdWithLock.load() == currentThreadId, "Mutex is not acquired by current thread");
}

void MutexThreadChecker::assertNotLocked() {
    auto currentThreadId = std::this_thread::get_id();
    SC_ASSERT(_storage->threadIdWithLock.load() != currentThreadId, "Mutex is already locked by current thread");
}

#else
MutexThreadChecker::MutexThreadChecker() = default;
MutexThreadChecker::~MutexThreadChecker() = default;

void MutexThreadChecker::onLock() {}
void MutexThreadChecker::onUnlock() {}
void MutexThreadChecker::onLockRefCounted() {}
void MutexThreadChecker::onUnlockRefCounted() {}
void MutexThreadChecker::assertIsLocked() {}
void MutexThreadChecker::assertNotLocked() {}

#endif

Mutex::Mutex() = default;
Mutex::~Mutex() = default;

void Mutex::lock() {
    _threadChecker.assertNotLocked();
    _innerMutex.lock();
    _threadChecker.onLock();
}

void Mutex::unlock() {
    _threadChecker.onUnlock();
    _innerMutex.unlock();
}

void Mutex::assertIsLocked() {
    _threadChecker.assertIsLocked();
}

void Mutex::onLockAcquired() {
    _threadChecker.onLock();
}

RecursiveMutex::RecursiveMutex() = default;
RecursiveMutex::~RecursiveMutex() = default;

void RecursiveMutex::lock() {
    _innerMutex.lock();
    _threadChecker.onLockRefCounted();
}

bool RecursiveMutex::try_lock() {
    if (!_innerMutex.try_lock()) {
        return false;
    }
    _threadChecker.onLockRefCounted();
    return true;
}

void RecursiveMutex::unlock() {
    _threadChecker.onUnlockRefCounted();
    _innerMutex.unlock();
}

void RecursiveMutex::assertIsLocked() {
    _threadChecker.assertIsLocked();
}

std::unique_lock<std::mutex> ConditionVariable::toStdLock(std::unique_lock<Mutex>& lock) {
    if (lock.owns_lock()) {
        return std::unique_lock<std::mutex>(lock.release()->_innerMutex, std::adopt_lock_t());
    } else {
        return std::unique_lock<std::mutex>(lock.release()->_innerMutex, std::defer_lock_t());
    }
}

std::unique_lock<Mutex> ConditionVariable::toValdiLock(std::unique_lock<std::mutex>& lock, Mutex* mutex) {
    if (lock.owns_lock()) {
        lock.release();
        mutex->onLockAcquired();
        return std::unique_lock<Mutex>(*mutex, std::adopt_lock_t());
    } else {
        lock.release();
        return std::unique_lock<Mutex>(*mutex, std::defer_lock_t());
    }
}

void ConditionVariable::wait(std::unique_lock<Mutex>& lock) {
    auto* mutexPtr = lock.mutex();
    auto convertedLock = toStdLock(lock);
    _condition.wait(convertedLock);
    lock = toValdiLock(convertedLock, mutexPtr);
}

std::cv_status ConditionVariable::waitFor(std::unique_lock<Mutex>& lock,
                                          const std::chrono::steady_clock::duration& duration) {
    auto* mutexPtr = lock.mutex();
    auto convertedLock = toStdLock(lock);
    auto result = _condition.wait_for(convertedLock, duration);
    lock = toValdiLock(convertedLock, mutexPtr);
    return result;
}

std::cv_status ConditionVariable::waitUntil(std::unique_lock<Mutex>& lock,
                                            const std::chrono::steady_clock::time_point& time) {
    auto* mutexPtr = lock.mutex();
    auto convertedLock = toStdLock(lock);
    auto result = _condition.wait_until(convertedLock, time);
    lock = toValdiLock(convertedLock, mutexPtr);
    return result;
}

void ConditionVariable::notifyAll() {
    _condition.notify_all();
}

void ConditionVariable::notifyOne() {
    _condition.notify_one();
}

} // namespace Valdi
