#include "valdi/runtime/JavaScript/JavaScriptWorker.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

namespace Valdi {

VALDI_CLASS_IMPL(JavaScriptWorker);

JavaScriptWorker::JavaScriptWorker(Ref<JavaScriptRuntime> hostRuntime,
                                   Ref<JavaScriptRuntime> workerRuntime,
                                   const StringBox& url)
    : _hostRuntime(hostRuntime), _workerRuntime(std::move(workerRuntime)), _url(url) {
    VALDI_INFO(_workerRuntime->getLogger(), "Created JS Worker with URL: {}", url);
}

JavaScriptWorker::~JavaScriptWorker() {
    VALDI_INFO(_workerRuntime->getLogger(), "Destroying JS Worker with URL: {}", _url);
    _workerRuntime->requestExecutionTermination();
    _workerRuntime->requestFullTeardown();
}

Ref<JavaScriptRuntime> JavaScriptWorker::getWorkerRuntime() const {
    return _workerRuntime;
}

static JSValueRef getGlobalOnMessage(JavaScriptEntryParameters& entry) {
    auto globalObj = entry.jsContext.getGlobalObject(entry.exceptionTracker);
    auto onMessageKey = STRING_LITERAL("onmessage");
    auto onmessage =
        entry.jsContext.getObjectProperty(globalObj.get(), onMessageKey.toStringView(), entry.exceptionTracker);
    return entry.jsContext.isValueFunction(onmessage.get()) ? std::move(onmessage) : entry.jsContext.newUndefined();
}

void JavaScriptWorker::postInit() {
    _workerRuntime->dispatchOnJsThread(
        nullptr, JavaScriptTaskScheduleTypeDefault, 0, [self = strongSmallRef(this)](JavaScriptEntryParameters& entry) {
            if (self->isRunning()) {
                self->doPostInit();
            }
        });
}

void JavaScriptWorker::setHostOnMessage(Shared<JSValueRefHolder> func) {
    std::lock_guard<Mutex> lock(_mutex);
    if (_state == State::Running) {
        _hostOnMessage = std::move(func);
    }
}

Shared<JSValueRefHolder> JavaScriptWorker::getHostOnMessage() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _hostOnMessage;
}

void JavaScriptWorker::postMessage(const Ref<JavaScriptMessage>& message) {
    _workerRuntime->dispatchOnJsThread(nullptr,
                                       JavaScriptTaskScheduleTypeDefault,
                                       0,
                                       [self = strongSmallRef(this), message](JavaScriptEntryParameters& entry) {
                                           self->doPostMessage(entry, message);
                                       });
}

void JavaScriptWorker::close() {
    doClose();
}

void JavaScriptWorker::terminate() {
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (_state != State::Running) {
            return;
        }
        _state = State::Terminated;
        _hostOnMessage = nullptr;
    }

    _workerRuntime->requestExecutionTermination();
    _workerRuntime->requestFullTeardown();
}

void JavaScriptWorker::doPostInit() {
    auto weakSelf = weakRef(this);
    // Set up globals in the worker runtime
    // - onmessage
    // - postMessage
    // - close
    // - location, https://developer.mozilla.org/en-US/docs/Web/API/WorkerLocation, only href and search are populated
    _workerRuntime->setValueToGlobalObject(STRING_LITERAL("onmessage"), Value::undefined());
    auto postMessageFunc = [weakSelf](const ValueFunctionCallContext& callContext) -> Value {
        auto self = weakSelf.lock();
        if (self && self->isRunning()) {
            auto hostRuntime = Ref<JavaScriptRuntime>(self->_hostRuntime.lock());
            if (hostRuntime == nullptr) {
                return Value::undefined();
            }
            auto transfer = callContext.getParametersSize() > 1 ? callContext.getParameter(1) : Value::undefinedRef();
            auto message = JavaScriptMessage::make(callContext.getParameter(0), transfer, hostRuntime, nullptr);
            if (!message) {
                callContext.getExceptionTracker().onError(message.moveError());
                return Value::undefined();
            }
            message.value()->dispatchHandler(self->getHostOnMessage(), [weakSelf]() {
                auto self = weakSelf.lock();
                return self != nullptr && self->canDeliverPendingMessage();
            });
        }
        return Value::undefined();
    };
    _workerRuntime->setValueToGlobalObject(STRING_LITERAL("postMessage"),
                                           Value(makeShared<ValueFunctionWithCallable>(postMessageFunc)));
    auto closeFunc = [weakSelf](const ValueFunctionCallContext& callContext) -> Value {
        auto self = weakSelf.lock();
        if (self) {
            self->close();
        }
        return Value::undefined();
    };

    _workerRuntime->setValueToGlobalObject(STRING_LITERAL("close"),
                                           Value(makeShared<ValueFunctionWithCallable>(closeFunc)));

    auto queryStartIndex = _url.indexOf('?');
    auto scriptUrl = queryStartIndex.has_value() ? _url.substring(0, queryStartIndex.value()) : _url;
    auto location = makeShared<ValueMap>();
    (*location)[STRING_LITERAL("href")] = Value(_url);
    (*location)[STRING_LITERAL("search")] =
        Value(queryStartIndex.has_value() ? _url.substring(queryStartIndex.value()) : STRING_LITERAL(""));
    _workerRuntime->setValueToGlobalObject(STRING_LITERAL("location"), Value(location));

    // Evaluate the worker script
    auto result = _workerRuntime->evalModuleSync(scriptUrl, false);
}

void JavaScriptWorker::doPostMessage(JavaScriptEntryParameters& entry, const Ref<JavaScriptMessage>& message) const {
    if (isRunning()) {
        auto onMessage = getGlobalOnMessage(entry);
        if (entry.jsContext.isValueFunction(onMessage.get())) {
            message->callHandler(entry, onMessage.get());
        }
    }
}

void JavaScriptWorker::doClose() {
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (_state != State::Running) {
            return;
        }
        _state = State::Closed;
        _hostOnMessage = nullptr;
    }
    _workerRuntime->requestFullTeardown();
}

bool JavaScriptWorker::isRunning() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _state == State::Running;
}

bool JavaScriptWorker::canDeliverPendingMessage() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _state != State::Terminated;
}

} // namespace Valdi
