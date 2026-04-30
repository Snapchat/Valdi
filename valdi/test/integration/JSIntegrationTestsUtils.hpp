#include "utils/debugging/Assert.hpp"
#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Threading/TaskQueue.hpp"
#include "valdi_core/cpp/Threading/Thread.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

namespace ValdiTest {

struct JSEntry {
    Valdi::IJavaScriptContext& context;
    Valdi::JSExceptionTracker exceptionTracker;
    Valdi::JavaScriptContextEntry contextEntry;

    bool exited = false;

    JSEntry(Valdi::IJavaScriptContext& context, Valdi::Ref<Valdi::Context> valdiContext)
        : context(context), exceptionTracker(context), contextEntry(std::move(valdiContext)) {
        context.willEnterVM();
    }

    ~JSEntry() {
        willExitVM();
    }

    void willExitVM() {
        if (!exited) {
            exited = true;
            context.willExitVM(exceptionTracker);
        }
    }

    void checkException() {
        if (!exceptionTracker) {
            throw Valdi::Exception(exceptionTracker.extractError().toStringBox());
        }
    }
};

struct TaskSchedulerImpl : public Valdi::JavaScriptTaskScheduler {
    Valdi::DispatchQueue* dispatchQueue;
    Valdi::Ref<Valdi::Context> valdiContext;
    Valdi::IJavaScriptContext* jsContext;
    Valdi::Ref<Valdi::TaskQueue> taskQueue;

    TaskSchedulerImpl(Valdi::DispatchQueue* dispatchQueue, const Valdi::Ref<Valdi::Context>& valdiContext)
        : dispatchQueue(dispatchQueue), valdiContext(valdiContext), taskQueue(Valdi::makeShared<Valdi::TaskQueue>()) {}

    JSEntry makeJsEntry() const {
        return makeJsEntry(valdiContext);
    }

    JSEntry makeJsEntry(const Valdi::Ref<Valdi::Context>& valdiContext) const {
        return JSEntry(*jsContext, valdiContext);
    }

    void dispatchOnJsThread(Valdi::Ref<Valdi::Context> ownerContext,
                            Valdi::JavaScriptTaskScheduleType scheduleType,
                            uint32_t delayMs,
                            Valdi::JavaScriptThreadTask&& function) override {
        auto executeValdiContext = ownerContext != nullptr ? ownerContext : valdiContext;

        Valdi::DispatchFunction dispatchFn =
            [this, executeValdiContext = std::move(executeValdiContext), function = std::move(function)]() {
                auto jsEntry = makeJsEntry(executeValdiContext);
                auto entryParameters =
                    Valdi::JavaScriptEntryParameters(jsEntry.context, jsEntry.exceptionTracker, executeValdiContext);
                function(entryParameters);
                jsEntry.checkException();
            };

        if (dispatchQueue == nullptr) {
            taskQueue->enqueue(std::move(dispatchFn), std::chrono::milliseconds(delayMs));
            if (scheduleType == Valdi::JavaScriptTaskScheduleTypeAlwaysSync) {
                taskQueue->flushUpToNow();
            }
        } else {
            if (scheduleType == Valdi::JavaScriptTaskScheduleTypeAlwaysSync) {
                dispatchQueue->sync(std::move(dispatchFn));
            } else {
                dispatchQueue->asyncAfter(std::move(dispatchFn), std::chrono::milliseconds(delayMs));
            }
        }
    }

    bool isInJsThread() override {
        return dispatchQueue == nullptr || dispatchQueue->isCurrent();
    }

    Valdi::Ref<Valdi::Context> getLastDispatchedContext() const override {
        return nullptr;
    }

    std::vector<Valdi::JavaScriptCapturedStacktrace> captureStackTraces(
        std::chrono::steady_clock::duration timeout) override {
        return {};
    }

    template<typename F>
    void withContext(bool synchronous, F&& func) const {
        if (dispatchQueue == nullptr || dispatchQueue->isCurrent()) {
            {
                auto jsEntry = makeJsEntry();
                func(jsEntry.context, jsEntry.exceptionTracker);
                jsEntry.checkException();
            }
        } else {
            if (synchronous) {
                dispatchQueue->sync([&]() { withContext(true, func); });
            } else {
                dispatchQueue->async([this, func = std::move(func)]() { withContext(true, func); });
            }
        }
    }
};

class JSContextWrapper {
public:
    Valdi::Ref<Valdi::Context> valdiContext;
    Valdi::Ref<TaskSchedulerImpl> taskScheduler;

    JSContextWrapper(Valdi::IJavaScriptBridge* jsBridge, Valdi::DispatchQueue* dispatchQueue) {
        valdiContext = Valdi::makeShared<Valdi::Context>(1, Valdi::strongSmallRef(&Valdi::ConsoleLogger::getLogger()));
        valdiContext->retainDisposables();
        taskScheduler = Valdi::makeShared<TaskSchedulerImpl>(dispatchQueue, valdiContext);

        if (dispatchQueue == nullptr) {
            initContext(jsBridge);
        } else {
            dispatchQueue->sync([&]() { initContext(jsBridge); });
        }

        taskScheduler->jsContext = context.get();
    }

    ~JSContextWrapper() {
        valdiContext->releaseDisposables();
        taskScheduler->taskQueue->flushUpToNow();
        taskScheduler->taskQueue->dispose();
        if (taskScheduler->dispatchQueue == nullptr) {
            context = nullptr;
        } else {
            taskScheduler->dispatchQueue->sync([&]() { context = nullptr; });
            taskScheduler->dispatchQueue->sync([&]() {});
            taskScheduler->dispatchQueue->fullTeardown();
        }
    }

    template<typename F>
    void withContext(F&& func) const {
        taskScheduler->withContext(true, std::move(func));
    }

    JSEntry makeJsEntry() const {
        return taskScheduler->makeJsEntry();
    }

    template<typename F>
    Valdi::Value withContextRet(F&& func) const {
        auto retValue = Valdi::Value::undefined();

        withContext([&](auto& context, auto& exceptionTracker) {
            auto result = func(context, exceptionTracker);
            if (exceptionTracker) {
                retValue =
                    Valdi::jsValueToValue(context, result.get(), Valdi::ReferenceInfoBuilder(), exceptionTracker);
            }
        });

        return retValue;
    }

    Valdi::Value evaluateScript(const std::string& src, std::string_view filename) {
        return withContextRet([&](auto& context, auto& exceptionTracker) {
            std::string updatedSrc = fmt::format("(function() {{ return {}; }})();", src);

            return context.evaluate(updatedSrc, filename, exceptionTracker);
        });
    }

    Valdi::Value evaluateScript(const std::string& src) {
        return evaluateScript(src, "unnamed.js");
    }

    Valdi::DispatchQueue* getDispatchQueue() const {
        return taskScheduler->dispatchQueue;
    }

private:
    Valdi::Ref<Valdi::IJavaScriptContext> context;

    void initContext(Valdi::IJavaScriptBridge* jsBridge) {
        context = jsBridge->createJsContext(taskScheduler.get(), Valdi::ConsoleLogger::getLogger());

        JSEntry jsEntry(*context, valdiContext);
        context->initialize(Valdi::IJavaScriptContextConfig(), jsEntry.exceptionTracker);
        jsEntry.checkException();
    }
};

Valdi::Ref<Valdi::ValueFunction> toFunction(const Valdi::Value& value);
Valdi::Result<Valdi::Value> callFunction(const Valdi::Ref<Valdi::ValueFunction>& function,
                                         const std::vector<Valdi::Value>& params);
Valdi::Result<Valdi::Value> callFunction(const Valdi::Ref<Valdi::ValueFunction>& function);

} // namespace ValdiTest
