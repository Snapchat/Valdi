//
//  Mutex.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/4/19.
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

namespace Valdi {

#if defined(__clang__)
#define VALDI_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define VALDI_THREAD_ANNOTATION_ATTRIBUTE__(x) // no-op
#endif

#define VALDI_CAPABILITY(x) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define VALDI_SCOPED_CAPABILITY VALDI_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define VALDI_GUARDED_BY(x) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define VALDI_PT_GUARDED_BY(x) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define VALDI_ACQUIRED_BEFORE(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define VALDI_ACQUIRED_AFTER(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define VALDI_REQUIRES(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define VALDI_REQUIRES_SHARED(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define VALDI_ACQUIRE(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define VALDI_ACQUIRE_SHARED(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define VALDI_RELEASE(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define VALDI_RELEASE_SHARED(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define VALDI_TRY_ACQUIRE(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define VALDI_TRY_ACQUIRE_SHARED(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define VALDI_EXCLUDES(...) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define VALDI_ASSERT_APABILITY(x) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define VALDI_ASSERT_SHARED_CAPABILITY(x) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define VALDI_RETURN_CAPABILITY(x) VALDI_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define VALDI_NO_THREAD_SAFETY_ANALYSIS VALDI_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#if defined(_LIBCPP_HAS_THREAD_SAFETY_ANNOTATIONS)
#define VALDI_STD_ACQUIRE(__mu__) VALDI_ACQUIRE(__mu__)
#define VALDI_STD_RELEASE(__mu__) VALDI_RELEASE(__mu__)
#else
#define VALDI_STD_ACQUIRE(__mu__) // no-op
#define VALDI_STD_RELEASE(__mu__) // no-op
#endif

class MutexThreadChecker {
public:
    MutexThreadChecker();
    ~MutexThreadChecker();

    void onLock();
    void onUnlock();
    void onLockRefCounted();
    void onUnlockRefCounted();

    void assertNotLocked();
    void assertIsLocked();

private:
    struct Storage {
        std::atomic<std::thread::id> threadIdWithLock;
        int refCount = 0;
    };
    std::unique_ptr<Storage> _storage;
};

/**
 A Mutex wrapper which asserts against deadlocks when compiled in debug.
 */
class VALDI_CAPABILITY("mutex") Mutex {
public:
    friend class ConditionVariable;

    Mutex();
    ~Mutex();

    void lock() VALDI_ACQUIRE();

    void unlock() VALDI_RELEASE();

    void assertIsLocked();

private:
    mutable std::mutex _innerMutex;
    MutexThreadChecker _threadChecker;

    void onLockAcquired();
};

/**
 A RecursiveMutex which in debug builds, can detects when the current thread has the lock, making possible
 to assert at runtime that the lock is acquired before performing some operations that need to be thread safe.
 */
class VALDI_CAPABILITY("mutex") RecursiveMutex {
public:
    RecursiveMutex();
    ~RecursiveMutex();

    void lock() VALDI_ACQUIRE();

    bool try_lock() VALDI_TRY_ACQUIRE(true);

    void unlock() VALDI_RELEASE();

    void assertIsLocked();

private:
    mutable std::recursive_mutex _innerMutex;
    MutexThreadChecker _threadChecker;
};

/**
 A ConditionVariable wrapper which is compatible with Valdi::Mutex.
 */
class ConditionVariable {
public:
    void notifyAll();
    void notifyOne();

    void wait(std::unique_lock<Mutex>& lock);
    std::cv_status waitFor(std::unique_lock<Mutex>& lock, const std::chrono::steady_clock::duration& duration);
    std::cv_status waitUntil(std::unique_lock<Mutex>& lock, const std::chrono::steady_clock::time_point& time);

private:
    std::condition_variable _condition;

    static std::unique_lock<std::mutex> toStdLock(std::unique_lock<Mutex>& lock);
    static std::unique_lock<Mutex> toValdiLock(std::unique_lock<std::mutex>& lock, Mutex* mutex);
};

} // namespace Valdi
