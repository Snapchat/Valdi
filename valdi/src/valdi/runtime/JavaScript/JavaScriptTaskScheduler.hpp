//
//  JavaScriptTaskScheduler.hpp
//  valdi
//
//  Created by Simon Corsin on 5/12/21.
//

#pragma once

#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptCapturedStacktrace.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <atomic>
#include <string>
#include <vector>

namespace Valdi {

struct JavaScriptEntryParameters {
    IJavaScriptContext& jsContext;
    JSExceptionTracker& exceptionTracker;
    const Ref<Context>& valdiContext;

    JavaScriptEntryParameters(IJavaScriptContext& jsContext,
                              JSExceptionTracker& exceptionTracker,
                              const Ref<Context>& valdiContext);
    ~JavaScriptEntryParameters();
};

struct JavaScriptThreadTask : public Valdi::Function<void(JavaScriptEntryParameters&)> {
    using Valdi::Function<void(JavaScriptEntryParameters&)>::Function;
};

/** Identifies ownerless JS-thread work in ANR [stuck-in:] diagnostics. */
enum class JsThreadDispatchReason {
    SetValueToGlobalObject,
    PerformGc,
    DumpMemoryStatistics,
    UnloadAllModules,
    UnloadUnusedModules,
    ReevalUnloadedModules,
    UnloadModulesAndDependents,
    IsJsModuleLoaded,
    EvaluateScript,
    EvalModuleSync,
    RegisterModuleFactory,
    RegisterTypeConverter,
    CallModuleFunction,
    PushModuleToMarshaller,
    AddModuleUnloadObserver,
    PreloadModule,
    PreloadModules,
    ScheduleWorkItem,
    WarmUpValueMarshaller,
    ExclusiveJsThreadLock,
    WorkerPostInit,
    WorkerPostMessage,
    MessagePortReleaseHandle,
    MessagePortDispatch,
    ErrorStackTrace,
    TypedArrayConversion,
    JavaScriptRunLoopFlush,
    DebuggerTickle,
    ANRDetectorAcknowledgement,
    ANRDetectorNudge,
    HotReloadStashData,
    HotReloadRestoreData,
    LockAllJSContexts,
    DaemonClientConnected,
    DaemonClientDisconnected,
    DaemonClientPayload,
    SetDefaultViewManagerContext,
    DumpLogs,
    DumpHeap,
    StartProfiling,
    StopProfiling,
};

enum JavaScriptTaskScheduleType {
    // Will be sync if the JS thread is current or the call is made
    // from the main thread and a main thread batch is current, async otherwise
    JavaScriptTaskScheduleTypeDefault,
    // Will always be sync
    JavaScriptTaskScheduleTypeAlwaysSync,
    // Will always be async
    JavaScriptTaskScheduleTypeAlwaysAsync,
};

class JavaScriptTaskScheduler : public SharedPtrRefCountable {
public:
    virtual void dispatchOnJsThread(Ref<Context> ownerContext,
                                    JavaScriptTaskScheduleType scheduleType,
                                    uint32_t delayMs,
                                    JavaScriptThreadTask&& function) = 0;
    /** Schedules JS-thread work without an owning component context and records why it was scheduled. */
    virtual void dispatchOnJsThread(JsThreadDispatchReason reason,
                                    JavaScriptTaskScheduleType scheduleType,
                                    uint32_t delayMs,
                                    JavaScriptThreadTask&& function) = 0;
    /** Schedules a callback with its owning context and an optional ANR boundary label. */
    virtual void dispatchOnJsThread(Ref<Context> ownerContext,
                                    const StringBox& /*attribution*/,
                                    JavaScriptTaskScheduleType scheduleType,
                                    uint32_t delayMs,
                                    JavaScriptThreadTask&& function) {
        dispatchOnJsThread(std::move(ownerContext), scheduleType, delayMs, std::move(function));
    }
    virtual bool isInJsThread() = 0;

    inline void dispatchOnJsThreadAsync(Ref<Context> ownerContext,
                                        const StringBox& attribution,
                                        JavaScriptThreadTask&& function) {
        dispatchOnJsThread(
            std::move(ownerContext), attribution, JavaScriptTaskScheduleTypeDefault, 0, std::move(function));
    }

    inline void dispatchOnJsThreadSync(Ref<Context> ownerContext,
                                       const StringBox& attribution,
                                       JavaScriptThreadTask&& function) {
        dispatchOnJsThread(
            std::move(ownerContext), attribution, JavaScriptTaskScheduleTypeAlwaysSync, 0, std::move(function));
    }

    inline void dispatchOnJsThreadAsync(Ref<Context> ownerContext, JavaScriptThreadTask&& function) {
        dispatchOnJsThread(std::move(ownerContext), JavaScriptTaskScheduleTypeDefault, 0, std::move(function));
    }

    inline void dispatchOnJsThreadAsyncAfter(Ref<Context> ownerContext,
                                             uint32_t delayMs,
                                             JavaScriptThreadTask&& function) {
        dispatchOnJsThread(
            std::move(ownerContext), JavaScriptTaskScheduleTypeAlwaysAsync, delayMs, std::move(function));
    }

    inline void dispatchOnJsThreadSync(Ref<Context> ownerContext, JavaScriptThreadTask&& function) {
        dispatchOnJsThread(std::move(ownerContext), JavaScriptTaskScheduleTypeAlwaysSync, 0, std::move(function));
    }

    inline void dispatchOnJsThreadAsync(JsThreadDispatchReason reason, JavaScriptThreadTask&& function) {
        dispatchOnJsThread(reason, JavaScriptTaskScheduleTypeDefault, 0, std::move(function));
    }

    inline void dispatchOnJsThreadAsyncAfter(JsThreadDispatchReason reason,
                                             uint32_t delayMs,
                                             JavaScriptThreadTask&& function) {
        dispatchOnJsThread(reason, JavaScriptTaskScheduleTypeAlwaysAsync, delayMs, std::move(function));
    }

    inline void dispatchOnJsThreadSync(JsThreadDispatchReason reason, JavaScriptThreadTask&& function) {
        dispatchOnJsThread(reason, JavaScriptTaskScheduleTypeAlwaysSync, 0, std::move(function));
    }

    virtual bool isDisposed() const {
        return false;
    }

    virtual Ref<Context> getLastDispatchedContext() const = 0;

    /**
     Capture the stacktraces of all running threads.
     The timeout parameter specifies how long the runtime will wait to get an interrupt
     handler called in order to capture the stacktraces.
     */
    virtual std::vector<JavaScriptCapturedStacktrace> captureStackTraces(
        std::chrono::steady_clock::duration timeout) = 0;

    // Extra attribution for ANR messages (" [stuck-in: <native call>] [module: <module>]"), or empty
    // if unavailable or ANR diagnostics are off. Reads saved native state without running JS, so it
    // is safe to call while the JS thread is stuck.
    virtual std::string getANRAttributionInfo() const {
        return {};
    }

    /**
     Whether the scheduler is far enough along in its lifecycle for unresponsiveness to be
     reported as an ANR. Runtime bootstrap (JS context creation plus core bundle evaluation)
     legitimately occupies the JS thread past the detection threshold on slow devices, so the
     detector excludes that window from ANR accounting. Must be safe to call from any thread
     while the JS thread is busy.
     */
    virtual bool isReadyForANRDetection() const {
        return true;
    }

    /**
     Bookkeeping for deadline-bounded sync calls into this scheduler's JS thread (see
     ValueFunctionWithJSValue::dispatchAndWaitOnJsThread). A call that timed out while its task
     is still queued is "overdue". While any call is overdue the JS thread is provably not keeping
     up, so further bounded calls fail fast instead of each parking the caller for a full deadline.
     The overdue count only drops when the JS thread finally runs the task, which is the only
     evidence that it is responsive again.
     */
    bool hasOverdueDeadlineCalls() const {
        return _overdueDeadlineCalls.load(std::memory_order_acquire) > 0;
    }
    void onDeadlineCallTimedOut() {
        _overdueDeadlineCalls.fetch_add(1, std::memory_order_acq_rel);
    }
    /** Returns the number of calls still overdue. */
    uint32_t onOverdueDeadlineCallCompleted() {
        return _overdueDeadlineCalls.fetch_sub(1, std::memory_order_acq_rel) - 1;
    }
    void onDeadlineCallFailedFast() {
        _fastFailedDeadlineCalls.fetch_add(1, std::memory_order_relaxed);
    }
    uint32_t takeFastFailedDeadlineCalls() {
        return _fastFailedDeadlineCalls.exchange(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> _overdueDeadlineCalls{0};
    std::atomic<uint32_t> _fastFailedDeadlineCalls{0};
};

} // namespace Valdi
