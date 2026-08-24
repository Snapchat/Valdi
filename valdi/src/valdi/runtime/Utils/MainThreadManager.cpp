//
//  MainThreadManager.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 8/26/18.
//

#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "utils/debugging/Assert.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Threading/ThreadBase.hpp"
#include "valdi_core/cpp/Utils/ContainerUtils.hpp"

namespace Valdi {

thread_local size_t kMainTreachBatchAllowScopeCounter = 0;

MainThreadBatchAllowScope::MainThreadBatchAllowScope() {
    kMainTreachBatchAllowScopeCounter++;
}

MainThreadBatchAllowScope::~MainThreadBatchAllowScope() {
    kMainTreachBatchAllowScopeCounter--;
}

bool MainThreadBatchAllowScope::isAllowed() {
    return kMainTreachBatchAllowScopeCounter > 0;
}

MainThreadTask::MainThreadTask(size_t flushId, const Ref<Context>& context, DispatchFunction&& function)
    : flushId(flushId), context(context), function(std::move(function)) {}

MainThreadTask::~MainThreadTask() = default;

MainThreadManager::MainThreadManager(const Ref<IMainThreadDispatcher>& mainThreadDispatcher)
    : _mainThreadDispatcher(mainThreadDispatcher),
      _mainThreadId(std::atomic<std::thread::id>(std::thread::id())),
      _tornDown(false) {}

MainThreadManager::~MainThreadManager() = default;

void MainThreadManager::postInit() {
    _mainThreadDispatcher->dispatch(
        new DispatchFunction([self = strongSmallRef(this)]() { self->markCurrentThreadIsMainThread(); }), false);
}

bool MainThreadManager::shouldAllowBatchFromCurrentThread() {
    return MainThreadBatchAllowScope::isAllowed();
}

void MainThreadManager::dispatch(const Ref<Context>& context, DispatchFunction function) {
    doDispatch(context, std::move(function), false);
}

void MainThreadManager::dispatchSync(const Ref<Context>& context, DispatchFunction function) {
    doDispatch(context, std::move(function), true);
}

void MainThreadManager::doDispatch(const Ref<Context>& context, DispatchFunction&& function, bool sync) {
    if (_tornDown) {
        return;
    }

    std::unique_lock<std::mutex> lockGuard(_mutex);

    _tasksSequence++;
    size_t flushId;
    bool shouldDispatch;

    if (_batchCount > 0 && shouldAllowBatchFromCurrentThread()) {
        // We are in a batch, schedule the task with the current flushId
        flushId = _flushIdSequence;
        _batchFlushId = flushId;
        _pendingTasks.emplace_back(flushId, context, std::move(function));
        shouldDispatch = false;
    } else {
        // Outside of a batch, we schedule the task for the next flush
        flushId = ++_flushIdSequence;
        _pendingTasks.emplace_back(flushId, context, std::move(function));
        shouldDispatch = true;
    }
    lockGuard.unlock();

    if (shouldDispatch || sync) {
        scheduleFlush(flushId, sync);
    }
}

void MainThreadManager::scheduleFlush(size_t flushId, bool sync) {
    auto dispatchFn =
        new DispatchFunction([self = strongSmallRef(this), flushId]() { self->flushTasksWithId(flushId); });
    _mainThreadDispatcher->dispatch(dispatchFn, sync);
}

bool MainThreadManager::runNextTask() {
    std::unique_lock<std::mutex> lock(_mutex);
    size_t flushIdSequence = _flushIdSequence;
    return runNextTaskWithId(flushIdSequence, lock);
}

bool MainThreadManager::runNextTaskWithId(size_t flushId) {
    std::unique_lock<std::mutex> lock(_mutex);
    return runNextTaskWithId(flushId, lock);
}

void MainThreadManager::flushUpToNow() {
    size_t flushId;
    {
        std::lock_guard<std::mutex> lockGuard(_mutex);
        // Increase the flush id sequence for the same reason endBatch() does: a dispatch that
        // happens while we are flushing would otherwise be stamped with the id we are flushing
        // (the batch branch of doDispatch reuses _flushIdSequence as-is), so a task that
        // re-enqueues itself would keep this pump alive forever.
        flushId = _flushIdSequence++;
    }
    flushTasksWithId(flushId);
}

bool MainThreadManager::waitUntilQuiescent(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(_mutex);
    return _quiescenceCondition.wait_for(lock, timeout, [this]() {
        return _tornDown || (_batchCount == 0 && _executingTaskCount == 0 && _pendingTasks.empty());
    });
}

std::atomic<bool> MainThreadManager::s_preRasterFenceDisabled{false};

bool MainThreadManager::isPreRasterFenceDisabled() {
    return s_preRasterFenceDisabled.load(std::memory_order_relaxed);
}

void MainThreadManager::setPreRasterFenceDisabled(bool disabled) {
    s_preRasterFenceDisabled.store(disabled, std::memory_order_relaxed);
}

bool MainThreadManager::runNextTaskWithId(size_t flushId, std::unique_lock<std::mutex>& lock) {
    if (_pendingTasks.empty() || _pendingTasks.front().flushId > flushId) {
        return false;
    }

    auto task = std::move(_pendingTasks.front());
    _pendingTasks.pop_front();
    _tasksSequence++;
    // Executing tasks count against quiescence too: the queue is already empty while the
    // task function runs, and it may still be mutating views.
    _executingTaskCount++;
    lock.unlock();

    bool ran = false;
    if (!_tornDown) {
        if (task.context != nullptr) {
            task.context->withAttribution(task.function);
        } else {
            task.function();
        }
        ran = true;
    }

    lock.lock();
    _executingTaskCount--;
    if (_executingTaskCount == 0 && _pendingTasks.empty()) {
        _quiescenceCondition.notify_all();
    }

    return ran;
}

void MainThreadManager::clearAndTeardown() {
    if (!_tornDown) {
        _tornDown = true;
        std::lock_guard<std::mutex> lockGuard(_mutex);
        _pendingTasks.clear();
        _quiescenceCondition.notify_all();
    }
}

void MainThreadManager::flushTasksWithId(size_t flushId) {
    while (runNextTaskWithId(flushId)) {
    }
}

void MainThreadManager::beginBatch() {
    std::lock_guard<std::mutex> lockGuard(_mutex);

    _batchCount++;
}

void MainThreadManager::endBatch() {
    size_t flushId;

    // See if we are the last endBatch() call, otherwise decrement the batchCount
    {
        std::lock_guard<std::mutex> lockGuard(_mutex);
        SC_ASSERT(_batchCount > 0, "Unbalanced beginBatch() endBatch() call");
        if (_batchCount > 1) {
            // Only the last endBatch() call will flush the TaskQueue
            _batchCount--;
            return;
        }

        flushId = _batchFlushId;
        // Increase flush id sequence, so that if there is a beginBatch() call
        // that occurs while we are flushing the tasks of this batch, the tasks
        // will be flushed on the next main thread tick.
        _flushIdSequence++;
    }

    // Flush all pending tasks.
    flushTasksWithId(flushId);

    // Disable batching
    std::lock_guard<std::mutex> lockGuard(_mutex);
    SC_ASSERT(_batchCount == 1, "Unbalanced beginBatch() endBatch() call");
    _batchCount = 0;
    _quiescenceCondition.notify_all();

    if (flushId != _batchFlushId) {
        // Our batch flush id changed, which means that a main thread batch task
        // has been enqueued while we were flushing the tasks. We should flush
        // them on the next main thread tick.
        scheduleFlush(_batchFlushId, false);
    }
}

void MainThreadManager::onIdle(const Ref<ValueFunction>& callback) {
    std::lock_guard<std::mutex> lockGuard(_mutex);
    _onIdleCallbacks.emplace_back(callback);

    if (!_idleFlushScheduled) {
        _idleFlushScheduled = true;

        scheduleFlushIdleCallbacks();
    }
}

void MainThreadManager::scheduleFlushIdleCallbacks() {
    auto sequence = _tasksSequence;
    _mainThreadDispatcher->dispatch(
        new DispatchFunction([self = strongSmallRef(this), sequence]() { self->flushNextIdleCallback(sequence); }),
        false);
}

void MainThreadManager::flushNextIdleCallback(uint64_t previousTasksSequence) {
    Ref<ValueFunction> callback;

    {
        std::lock_guard<std::mutex> lockGuard(_mutex);

        if (_tasksSequence != previousTasksSequence) {
            // We have pending tasks, something else was scheduled
            // since the idle flush was scheduled
            scheduleFlushIdleCallbacks();
        } else {
            if (!_onIdleCallbacks.empty()) {
                callback = std::move(_onIdleCallbacks.front());
                _onIdleCallbacks.pop_front();
            }

            if (_onIdleCallbacks.empty()) {
                // We finished flushing the callbacks
                _idleFlushScheduled = false;
            } else {
                // We still have more, schedule for next frame
                scheduleFlushIdleCallbacks();
            }
        }
    }

    if (callback != nullptr) {
        (*callback)();
    }
}

MainThreadBatch MainThreadManager::scopedBatch() {
    beginBatch();
    return MainThreadBatch(this);
}

// This check only makes sense inside the main thread
bool MainThreadManager::hasBatch() const {
    std::lock_guard<std::mutex> lockGuard(_mutex);
    return _batchCount > 0;
}

bool MainThreadManager::currentThreadIsMainThread() const {
    return std::this_thread::get_id() == _mainThreadId.load(std::memory_order_relaxed);
}

void MainThreadManager::markCurrentThreadIsMainThread() {
    // Initialize the threadId counter on the main thread as early as possible
    Valdi::getCurrentThreadId();
    _mainThreadId = std::this_thread::get_id();
}

MainThreadBatch::MainThreadBatch() = default;
MainThreadBatch::MainThreadBatch(MainThreadManager* manager) : _manager(manager) {}
MainThreadBatch::~MainThreadBatch() {
    if (_manager != nullptr) {
        _manager->endBatch();
    }
}

} // namespace Valdi
