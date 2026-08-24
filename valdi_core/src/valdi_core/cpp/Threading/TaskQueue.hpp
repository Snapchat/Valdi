//
//  TaskQueue.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/28/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi_core/cpp/Threading/IDispatchQueue.hpp"
#include "valdi_core/cpp/Threading/IQueueListener.hpp"
#include "valdi_core/cpp/Threading/TaskId.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include <chrono>
#include <queue>

namespace Valdi {

struct EnqueuedTask {
    task_id_t id = 0;
    bool isFirst = false;
};

class TaskQueue : public IDispatchQueue {
public:
    TaskQueue();
    TaskQueue(const TaskQueue& other) = delete;
    ~TaskQueue() override;

    void dispose();

    EnqueuedTask enqueue(DispatchFunction function);
    EnqueuedTask enqueue(DispatchFunction function, std::chrono::steady_clock::duration delay);
    EnqueuedTask enqueue(DispatchFunction function, std::chrono::steady_clock::time_point executeTime);

    void barrier(const DispatchFunction& function);

    void sync(const DispatchFunction& function) final;
    void async(DispatchFunction function) final;
    task_id_t asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) final;
    void cancel(task_id_t taskId) final;

    bool runNextTask(std::chrono::steady_clock::time_point maxTime);
    bool runNextTask();
    size_t flush();
    size_t flushUpToNow();

    bool isDisposed() const;
    void setListener(const Shared<IQueueListener>& listener);

    void setMaxConcurrentTasks(size_t maxConcurrentTasks);

    // For Testing Only
    Shared<IQueueListener> getListener() const;

private:
    struct Task {
        task_id_t id;
        DispatchFunction function;
        std::chrono::steady_clock::time_point executeTime;
        bool isBarrier;
        // Connects the task's execution slice to its submit site (snap::profiling task
        // tracing; 0 when tracing is idle).
        uint64_t parentSpanId;

        Task(task_id_t id,
             DispatchFunction function,
             std::chrono::steady_clock::time_point executeTime,
             bool isBarrier,
             uint64_t parentSpanId);
    };

    std::atomic_bool _disposed;
    mutable Mutex _mutex;
    ConditionVariable _condition;
    task_id_t _taskIdCounter{0};
    std::deque<Task> _tasks;
    bool _empty = true;
    bool _first = true;
    size_t _currentRunningTasks = 0;
    size_t _maxConcurrentTasks = 1;
    Shared<IQueueListener> _listener;

    DispatchFunction nextTask(std::chrono::steady_clock::time_point maxTime, bool* shouldRun, uint64_t* parentSpanId);

    task_id_t insertTask(DispatchFunction&& function,
                         std::chrono::steady_clock::time_point executeTime,
                         bool isBarrier,
                         uint64_t parentSpanId);

    DispatchFunction lockFreeRemoveTask(task_id_t taskId);
};

} // namespace Valdi
