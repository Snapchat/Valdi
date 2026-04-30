//
//  GCDDispatchQueue.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 4/9/19.
//

#if __APPLE__

#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Threading/GCDUtils.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include <atomic>
#include <dispatch/dispatch.h>
#include <mutex>

namespace Valdi {

class GCDDispatchQueue;

class GCDDispatchQueueSyncWorkItem;
class GCDDispatchQueueAsyncWorkItem;
class GCDDispatchQueueAsyncAfterWorkItem;

class GCDDispatchQueue : public DispatchQueue {
public:
    GCDDispatchQueue(const StringBox& name, ThreadQoSClass qosClass);
    GCDDispatchQueue(void* queue);
    ~GCDDispatchQueue() override;

    void sync(const DispatchFunction& function) override;
    void async(DispatchFunction function) override;
    task_id_t asyncAfter(DispatchFunction function, std::chrono::steady_clock::duration delay) override;
    void cancel(task_id_t taskId) override;

    bool isCurrent() const override;

    void fullTeardown() override;

    void setListener(const Shared<IQueueListener>& listener) override;

    static GCDDispatchQueue* getCurrent();

    // For Testing Only
    Shared<IQueueListener> getListener() const override;

    static Ref<GCDDispatchQueue> fromGCDDispatchQueue(void* queue);

private:
    dispatch_queue_t _queue;
    mutable Mutex _mutex;
    FlatSet<task_id_t> _activeTasks;
    task_id_t _sequence = 0;
    std::atomic<int> _pendingTasksCount;
    std::timed_mutex _executionMutex;
    std::atomic_bool _hasListener;
    std::atomic_bool _destroyed;
    Shared<IQueueListener> _listener;

    bool removeTaskId(task_id_t id);

    void teardown();

    void incrementPendingTaskCount();
    void decrementPendingTaskCount();

    friend GCDDispatchQueueSyncWorkItem;
    void syncEnter(const DispatchFunction& function);

    friend GCDDispatchQueueAsyncWorkItem;
    void asyncEnter(const DispatchFunction& function);

    friend GCDDispatchQueueAsyncAfterWorkItem;
    void asyncAfterEnter(const DispatchFunction& function, task_id_t taskId);

    bool wasDestroyed() const;
};

} // namespace Valdi

#endif
