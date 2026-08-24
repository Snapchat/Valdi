//
//  TaskQueue.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/28/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi_core/cpp/Threading/TaskQueue.hpp"

#include "valdi_core/cpp/Constants.hpp"

#include "utils/debugging/Trace.hpp"

#include <pthread.h>

namespace Valdi {

namespace {
constexpr auto kTaskTraceLabel = "Valdi.TaskQueue";
} // namespace

TaskQueue::Task::Task(task_id_t id,
                      DispatchFunction function,
                      std::chrono::steady_clock::time_point executeTime,
                      bool isBarrier,
                      uint64_t parentSpanId)
    : id(id),
      function(std::move(function)),
      executeTime(executeTime),
      isBarrier(isBarrier),
      parentSpanId(parentSpanId) {}

TaskQueue::TaskQueue() : _disposed(false) {}

TaskQueue::~TaskQueue() {
    dispose();
}

void TaskQueue::dispose() {
    if (!_disposed) {
        _disposed = true;
        std::deque<Task> toDelete;
        _mutex.lock();
        toDelete.swap(_tasks);
        _mutex.unlock();
        _condition.notifyAll();
    }
}

void TaskQueue::sync(const DispatchFunction& function) {
    barrier(function);
}

void TaskQueue::async(DispatchFunction function) {
    enqueue(std::move(function));
}

task_id_t TaskQueue::asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) {
    return enqueue(std::move(function), delay).id;
}

EnqueuedTask TaskQueue::enqueue(Valdi::DispatchFunction function) {
    return enqueue(std::move(function), std::chrono::steady_clock::now());
}

EnqueuedTask TaskQueue::enqueue(Valdi::DispatchFunction function, std::chrono::steady_clock::duration delay) {
    return enqueue(std::move(function), std::chrono::steady_clock::now() + delay);
}

task_id_t TaskQueue::insertTask(DispatchFunction&& function,
                                std::chrono::steady_clock::time_point executeTime,
                                bool isBarrier,
                                uint64_t parentSpanId) {
    auto id = ++_taskIdCounter;

    // Keep the tasks sorted by execute time
    Task task(id, std::move(function), executeTime, isBarrier, parentSpanId);

    auto it = std::upper_bound(_tasks.begin(), _tasks.end(), task, [](const Task& a, const Task& b) {
        if (a.executeTime == b.executeTime) {
            return a.id < b.id;
        }
        return a.executeTime < b.executeTime;
    });

    _tasks.emplace(it, std::move(task));

    return id;
}

EnqueuedTask TaskQueue::enqueue(Valdi::DispatchFunction function, std::chrono::steady_clock::time_point executeTime) {
    EnqueuedTask enqueuedTask;

    if (_disposed) {
        return enqueuedTask;
    }

    const uint64_t parentSpanId = ::snap::utils::debugging::getCurrentSpanId();

    {
        std::lock_guard<Mutex> lockGuard(_mutex);

        enqueuedTask.id = insertTask(std::move(function), executeTime, false, parentSpanId);
        enqueuedTask.isFirst = _first;
        _first = false;

        if (_empty) {
            _empty = false;
            if (_listener != nullptr) {
                _listener->onQueueNonEmpty();
            }
        }
    }

    _condition.notifyAll();
    return enqueuedTask;
}

void TaskQueue::cancel(Valdi::task_id_t taskId) {
    {
        DispatchFunction toDelete;
        std::lock_guard<Mutex> lockGuard(_mutex);
        toDelete = lockFreeRemoveTask(taskId);
    }

    _condition.notifyAll();
}

DispatchFunction TaskQueue::lockFreeRemoveTask(task_id_t taskId) {
    for (auto i = _tasks.begin(); i != _tasks.end(); ++i) {
        if (i->id == taskId) {
            auto task = std::move(*i);
            _tasks.erase(i);
            return std::move(task.function);
        }
    }

    return DispatchFunction();
}

void TaskQueue::barrier(const DispatchFunction& function) {
    auto executeTime = std::chrono::steady_clock::now();

    std::unique_lock<Mutex> lockGuard(_mutex);
    auto id = insertTask(DispatchFunction(), executeTime, true, 0);

    while (!_tasks.empty()) {
        // Wait until we have no currently running tasks, and that the task at the front is our barrier task
        if (_currentRunningTasks != 0 || _tasks.front().id != id) {
            _condition.wait(lockGuard);
            continue;
        }

        // We have no running tasks, and our barrier task is at the front.
        // We can now execute our barrier
        _currentRunningTasks++;
        lockGuard.unlock();
        function();
        lockGuard.lock();
        auto toDelete = lockFreeRemoveTask(id);
        _currentRunningTasks--;
        lockGuard.unlock();
        _condition.notifyAll();
        return;
    }
}

DispatchFunction TaskQueue::nextTask(std::chrono::steady_clock::time_point maxTime,
                                     bool* shouldRun,
                                     uint64_t* parentSpanId) {
    std::unique_lock<Mutex> lockGuard(_mutex);
    bool hasTask = false;

    while (!_disposed) {
        if (_tasks.empty()) {
            if (!_empty) {
                _empty = true;
                if (_listener != nullptr) {
                    _listener->onQueueEmpty();
                }
            }
            // Wait until there's at least one task
            auto result = _condition.waitUntil(lockGuard, maxTime);
            if (result == std::cv_status::timeout) {
                // We timed out waiting for tasks, so we break now
                break;
            } else {
                // Try again.
                continue;
            }
        }

        if (_currentRunningTasks >= _maxConcurrentTasks) {
            // Wait until there are no more pending running tasks
            auto result = _condition.waitUntil(lockGuard, maxTime);
            if (result == std::cv_status::timeout) {
                // We timed out waiting for tasks, so we break now
                break;
            } else {
                // Try again.
                continue;
            }
        }

        // Wait until the next task is ready to run
        const auto& nextTask = _tasks.front();
        if (VALDI_UNLIKELY(nextTask.isBarrier)) {
            auto result = _condition.waitUntil(lockGuard, maxTime);

            if (result == std::cv_status::timeout) {
                break;
            } else {
                continue;
            }
        } else if (nextTask.executeTime > std::chrono::steady_clock::now()) {
            auto maxTimeToWait = std::min(maxTime, nextTask.executeTime);

            auto result = _condition.waitUntil(lockGuard, maxTimeToWait);

            // If we reached the given maxTime, we should abort.
            if (maxTimeToWait == maxTime && result == std::cv_status::timeout) {
                break;
            } else {
                continue;
            }
        }

        // The next task is ready
        hasTask = true;
        break;
    }

    DispatchFunction nextTaskFunction;
    if (_disposed || !hasTask) {
        *shouldRun = false;
    } else {
        nextTaskFunction = std::move(_tasks.front().function);
        *parentSpanId = _tasks.front().parentSpanId;
        _currentRunningTasks++;
        _tasks.pop_front();
    }
    return nextTaskFunction;
}

size_t TaskQueue::flush() {
    size_t ranTasks = 0;
    while (runNextTask()) {
        ranTasks++;
    }
    return ranTasks;
}

size_t TaskQueue::flushUpToNow() {
    auto maxTime = std::chrono::steady_clock::now();
    size_t ranTasks = 0;
    while (runNextTask(maxTime)) {
        ranTasks++;
    }
    return ranTasks;
}

bool TaskQueue::runNextTask() {
    return runNextTask(std::chrono::steady_clock::now());
}

bool TaskQueue::runNextTask(std::chrono::steady_clock::time_point maxTime) {
    auto shouldRun = true;
    uint64_t parentSpanId = 0;
    auto task = nextTask(maxTime, &shouldRun, &parentSpanId);
    if (shouldRun) {
        {
            ::snap::utils::debugging::ScopedTrace taskTrace(kTaskTraceLabel, parentSpanId);
            task();
        }
        // Dispose the task before notifying the condition,
        // to ensure that all retained objects by the task
        // are released.
        task = DispatchFunction();

        {
            std::lock_guard<Mutex> lockGuard(_mutex);
            _currentRunningTasks--;
        }

        _condition.notifyAll();
    }
    return shouldRun;
}

bool TaskQueue::isDisposed() const {
    return _disposed;
}

void TaskQueue::setMaxConcurrentTasks(size_t maxConcurrentTasks) {
    {
        std::lock_guard<Mutex> lockGuard(_mutex);
        if (_maxConcurrentTasks == maxConcurrentTasks) {
            return;
        }
        _maxConcurrentTasks = maxConcurrentTasks;
    }
    _condition.notifyAll();
}

void TaskQueue::setListener(const Shared<IQueueListener>& listener) {
    std::lock_guard<Mutex> lockGuard(_mutex);
    _listener = listener;
}

Shared<IQueueListener> TaskQueue::getListener() const {
    std::lock_guard<Mutex> lockGuard(_mutex);
    return _listener;
}

} // namespace Valdi
