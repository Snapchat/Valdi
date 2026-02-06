//
//  GCDDispatchQueue.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 4/9/19.
//

#if __APPLE__

#include "valdi_core/cpp/Threading/GCDDispatchQueue.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include <chrono>

namespace Valdi {

const char* kDispatchQueueSpecific = "specific";

class GCDDispatchQueueSyncWorkItem {
public:
    GCDDispatchQueueSyncWorkItem(GCDDispatchQueue* queue, const DispatchFunction& function)
        : _queue(queue), _function(function) {}

    void perform() {
        _queue->syncEnter(_function);
    }

private:
    GCDDispatchQueue* _queue;
    const DispatchFunction& _function;
};

class GCDDispatchQueueAsyncWorkItem {
public:
    GCDDispatchQueueAsyncWorkItem(Shared<GCDDispatchQueue> queue, DispatchFunction function)
        : _queue(std::move(queue)), _function(std::move(function)) {}

    void perform() {
        _queue->asyncEnter(_function);
    }

private:
    Shared<GCDDispatchQueue> _queue;
    DispatchFunction _function;
};

class GCDDispatchQueueAsyncAfterWorkItem {
public:
    GCDDispatchQueueAsyncAfterWorkItem(Shared<GCDDispatchQueue> queue, DispatchFunction function, task_id_t taskId)
        : _queue(std::move(queue)), _function(std::move(function)), _taskId(taskId) {}

    void perform() {
        _queue->asyncAfterEnter(_function, _taskId);
    }

private:
    Shared<GCDDispatchQueue> _queue;
    DispatchFunction _function;
    task_id_t _taskId;
};

void GCDDispatchQueueSyncCallback(void* context) {
    GCDDispatchQueueSyncWorkItem* workItem = reinterpret_cast<GCDDispatchQueueSyncWorkItem*>(context);

    workItem->perform();
}

void GCDDispatchQueueAsyncCallback(void* context) {
    GCDDispatchQueueAsyncWorkItem* workItem = reinterpret_cast<GCDDispatchQueueAsyncWorkItem*>(context);

    workItem->perform();

    delete workItem;
}

void GCDDispatchQueueAsyncAfterCallback(void* context) {
    GCDDispatchQueueAsyncAfterWorkItem* workItem = reinterpret_cast<GCDDispatchQueueAsyncAfterWorkItem*>(context);

    workItem->perform();

    delete workItem;
}

GCDDispatchQueue::GCDDispatchQueue(const StringBox& name, ThreadQoSClass qosClass)
    : _pendingTasksCount(0), _hasListener(false), _destroyed(false) {
    _queue = dispatch_queue_create(name.getCStr(),
                                   dispatch_queue_attr_make_with_qos_class(nullptr, ValdiQoSClassToGCD(qosClass), 0));
    dispatch_queue_set_specific(_queue, kDispatchQueueSpecific, this, nullptr);
}

GCDDispatchQueue::GCDDispatchQueue(void* queue)
    : _queue(reinterpret_cast<dispatch_queue_t>(queue)), _pendingTasksCount(0), _hasListener(false), _destroyed(false) {
    dispatch_queue_set_specific(_queue, kDispatchQueueSpecific, this, nullptr);
    dispatch_retain(_queue);
}

GCDDispatchQueue::~GCDDispatchQueue() {
    teardown();

    dispatch_queue_set_specific(_queue, kDispatchQueueSpecific, nullptr, nullptr);
    dispatch_release(_queue);
    _queue = nullptr;
}

void GCDDispatchQueue::sync(const DispatchFunction& function) {
    GCDDispatchQueueSyncWorkItem workItem(this, function);
    dispatch_sync_f(_queue, &workItem, &GCDDispatchQueueSyncCallback);
}

void GCDDispatchQueue::syncEnter(const DispatchFunction& function) {
    _runningSync = true;
    function();
    _runningSync = false;
}

void GCDDispatchQueue::async(DispatchFunction function) {
    incrementPendingTaskCount();
    auto workItem = new GCDDispatchQueueAsyncWorkItem(strongRef(this), std::move(function));
    dispatch_async_f(_queue, workItem, &GCDDispatchQueueAsyncCallback);
}

void GCDDispatchQueue::asyncEnter(const DispatchFunction& function) {
    // Check if destroyed before acquiring execution lock
    if (wasDestroyed()) {
        decrementPendingTaskCount();
        return;
    }

    // Acquire lock to protect function execution
    std::lock_guard<std::timed_mutex> lock(_executionMutex);

    // Double-check after acquiring lock (in case teardown happened between checks)
    if (wasDestroyed()) {
        decrementPendingTaskCount();
        return;
    }

    // Safe to execute - teardown will wait for lock to be released
    function();

    decrementPendingTaskCount();
}

bool GCDDispatchQueue::removeTaskId(task_id_t id) {
    std::lock_guard<Mutex> guard(_mutex);
    const auto& it = _activeTasks.find(id);
    if (it == _activeTasks.end()) {
        return false;
    }
    _activeTasks.erase(it);
    return true;
}

task_id_t GCDDispatchQueue::asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) {
    incrementPendingTaskCount();

    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(delay).count();

    std::lock_guard<Mutex> guard(_mutex);
    auto id = ++_sequence;
    _activeTasks.insert(id);

    auto workItem = new GCDDispatchQueueAsyncAfterWorkItem(strongRef(this), std::move(function), id);
    dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(nanoseconds)),
                     _queue,
                     workItem,
                     &GCDDispatchQueueAsyncAfterCallback);

    return id;
}

void GCDDispatchQueue::asyncAfterEnter(const DispatchFunction& function, task_id_t taskId) {
    if (!removeTaskId(taskId)) {
        return;
    }

    // Check if destroyed before acquiring execution lock
    if (wasDestroyed()) {
        decrementPendingTaskCount();
        return;
    }

    // Acquire lock to protect function execution
    std::lock_guard<std::timed_mutex> lock(_executionMutex);

    // Double-check after acquiring lock (in case teardown happened between checks)
    if (wasDestroyed()) {
        decrementPendingTaskCount();
        return;
    }

    // Safe to execute - teardown will wait for lock to be released
    function();

    decrementPendingTaskCount();
}

void GCDDispatchQueue::cancel(task_id_t taskId) {
    if (removeTaskId(taskId)) {
        decrementPendingTaskCount();
    }
}

bool GCDDispatchQueue::isCurrent() const {
    return GCDDispatchQueue::getCurrent() == this;
}

void GCDDispatchQueue::teardown() {
    _destroyed = true;
    std::lock_guard<Mutex> guard(_mutex);
    _activeTasks.clear();
}

void GCDDispatchQueue::setListener(const Shared<IQueueListener>& listener) {
    std::lock_guard<Mutex> guard(_mutex);
    _listener = listener;
    _hasListener = listener != nullptr;
}

Shared<IQueueListener> GCDDispatchQueue::getListener() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _listener;
};

void GCDDispatchQueue::incrementPendingTaskCount() {
    auto previous = _pendingTasksCount.fetch_add(1);

    if (previous == 0 && _hasListener.load()) {
        std::lock_guard<Mutex> guard(_mutex);
        if (_listener != nullptr) {
            _listener->onQueueNonEmpty();
        }
    }
}

void GCDDispatchQueue::decrementPendingTaskCount() {
    auto previous = _pendingTasksCount.fetch_sub(1);
    if (previous == 1 && _hasListener.load()) {
        std::lock_guard<Mutex> guard(_mutex);
        if (_listener != nullptr) {
            _listener->onQueueEmpty();
        }
    }
}

void GCDDispatchQueue::fullTeardown() {
    teardown();

    // Try to acquire the execution lock with timeout to ensure no tasks are executing
    // If we can acquire it, all tasks have completed. If timeout, proceed anyway to
    // avoid indefinite hang (preserves COMPOSER-837 JSCore deadlock workaround).
    if (!_executionMutex.try_lock_for(std::chrono::seconds(2))) {
        // Timeout - log and proceed anyway
        // This should be rare and indicates tasks are taking too long to complete
    } else {
        // Successfully acquired lock - all tasks complete, release it
        _executionMutex.unlock();
    }
}

bool GCDDispatchQueue::wasDestroyed() const {
    return _destroyed.load();
}

GCDDispatchQueue* GCDDispatchQueue::getCurrent() {
    return reinterpret_cast<GCDDispatchQueue*>(dispatch_get_specific(kDispatchQueueSpecific));
}

Ref<GCDDispatchQueue> GCDDispatchQueue::fromGCDDispatchQueue(void* queue) {
    auto* key = dispatch_queue_get_specific(reinterpret_cast<dispatch_queue_t>(queue), kDispatchQueueSpecific);
    if (key != nullptr) {
        return Ref(reinterpret_cast<GCDDispatchQueue*>(key));
    }

    return makeShared<GCDDispatchQueue>(queue);
}

} // namespace Valdi

#endif
