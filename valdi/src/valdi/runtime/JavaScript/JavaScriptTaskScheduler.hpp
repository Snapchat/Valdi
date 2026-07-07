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
    virtual bool isInJsThread() = 0;

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

    // The module being imported/evaluated on the JS thread, or empty if none or diagnostics are off.
    // Readable from another thread without running JS, for attributing ANRs.
    virtual std::string getCurrentModuleLoadInfo() const {
        return {};
    }
};

} // namespace Valdi
