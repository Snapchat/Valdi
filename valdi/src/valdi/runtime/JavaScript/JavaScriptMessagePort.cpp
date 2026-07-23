#include "valdi/runtime/JavaScript/JavaScriptMessagePort.hpp"

#include "valdi/runtime/JavaScript/JSNativeClassBinder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"

#include <unordered_set>

namespace Valdi {

JavaScriptMessage::JavaScriptMessage(Value data,
                                     std::vector<Ref<ValdiObject>> transferredPortSources,
                                     std::vector<Ref<JavaScriptMessagePortEndpoint>> transferredPorts)
    : _data(std::move(data)),
      _transferredPortSources(std::move(transferredPortSources)),
      _transferredPorts(std::move(transferredPorts)) {}

const Value& JavaScriptMessage::getData() const {
    return _data;
}

const std::vector<Ref<ValdiObject>>& JavaScriptMessage::getTransferredPortSources() const {
    return _transferredPortSources;
}

const std::vector<Ref<JavaScriptMessagePortEndpoint>>& JavaScriptMessage::getTransferredPorts() const {
    return _transferredPorts;
}

void JavaScriptMessagePortEndpoint::setPeer(const Ref<JavaScriptMessagePortEndpoint>& peer) {
    std::lock_guard<Mutex> lock(_mutex);
    _peer = peer;
}

void JavaScriptMessagePortEndpoint::attach(const Ref<JavaScriptMessagePort>& handle,
                                           const Ref<JavaScriptRuntime>& runtime) {
    std::lock_guard<Mutex> lock(_mutex);
    SC_ASSERT(!_closed);
    SC_ASSERT(_handle.expired());
    auto ownerRuntime = _ownerRuntime.lock();
    SC_ASSERT(ownerRuntime == nullptr || ownerRuntime.get() == runtime.get());
    _ownerRuntime = runtime;
    _handle = handle;
}

Result<Void> JavaScriptMessagePortEndpoint::validateTransfer(const JavaScriptMessagePort& handle) const {
    std::lock_guard<Mutex> lock(_mutex);
    auto currentHandle = _handle.lock();
    if (_closed || currentHandle == nullptr || currentHandle.get() != &handle) {
        return Error("MessagePort in transfer list is already detached");
    }
    return Void();
}

void JavaScriptMessagePortEndpoint::transfer(const JavaScriptMessagePort& handle,
                                             const Ref<JavaScriptRuntime>& runtime) {
    std::lock_guard<Mutex> lock(_mutex);
    auto currentHandle = _handle.lock();
    SC_ASSERT(!_closed && currentHandle != nullptr && currentHandle.get() == &handle);
    _generation++;
    _scheduledGeneration.reset();
    _started = false;
    _handle.reset();
    _ownerRuntime = runtime;
}

Ref<JavaScriptRuntime> JavaScriptMessagePortEndpoint::getOwnerRuntime() const {
    std::lock_guard<Mutex> lock(_mutex);
    return Ref<JavaScriptRuntime>(_ownerRuntime.lock());
}

Ref<JavaScriptMessagePortEndpoint> JavaScriptMessagePortEndpoint::getPeer() const {
    std::lock_guard<Mutex> lock(_mutex);
    return Ref<JavaScriptMessagePortEndpoint>(_peer.lock());
}

void JavaScriptMessagePortEndpoint::start(const JavaScriptMessagePort& handle) {
    std::unique_lock<Mutex> lock(_mutex);
    auto currentHandle = _handle.lock();
    if (_closed || currentHandle == nullptr || currentHandle.get() != &handle) {
        return;
    }
    _started = true;
    scheduleNextMessage(lock);
}

void JavaScriptMessagePortEndpoint::close(const JavaScriptMessagePort& handle) {
    std::lock_guard<Mutex> lock(_mutex);
    auto currentHandle = _handle.lock();
    if (_closed || currentHandle == nullptr || currentHandle.get() != &handle) {
        return;
    }
    _closed = true;
    _generation++;
    _scheduledGeneration.reset();
    _handle.reset();
    _ownerRuntime.reset();
    _messages.clear();
}

void JavaScriptMessagePortEndpoint::enqueue(const Ref<JavaScriptMessage>& message) {
    for (const auto& transferredPort : message->getTransferredPorts()) {
        if (transferredPort.get() == this) {
            // Transferring the destination port makes the message undeliverable.
            return;
        }
    }

    std::unique_lock<Mutex> lock(_mutex);
    if (_closed) {
        return;
    }
    _messages.emplace_back(message);
    scheduleNextMessage(lock);
}

void JavaScriptMessagePortEndpoint::scheduleNextMessage(std::unique_lock<Mutex>& lock) {
    if (_closed || !_started || _messages.empty() || _handle.expired() || _scheduledGeneration.has_value()) {
        return;
    }

    auto runtime = Ref<JavaScriptRuntime>(_ownerRuntime.lock());
    if (runtime == nullptr) {
        return;
    }

    auto generation = _generation;
    _scheduledGeneration = generation;
    auto self = strongSmallRef(this);
    lock.unlock();
    runtime->dispatchOnJsThread(
        nullptr, JavaScriptTaskScheduleTypeAlwaysAsync, 0, [self, generation](JavaScriptEntryParameters& entry) {
            self->dispatchNextMessage(entry, generation);
        });
}

void JavaScriptMessagePortEndpoint::dispatchNextMessage(JavaScriptEntryParameters& entry, uint64_t generation) {
    Ref<JavaScriptMessagePort> handle;
    Ref<JavaScriptMessage> message;
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (_closed || generation != _generation || _scheduledGeneration != generation || !_started ||
            _messages.empty()) {
            return;
        }
        _scheduledGeneration.reset();
        handle = Ref<JavaScriptMessagePort>(_handle.lock());
        if (handle == nullptr) {
            return;
        }
        message = std::move(_messages.front());
        _messages.pop_front();
    }

    handle->dispatchMessage(entry, message);

    std::unique_lock<Mutex> lock(_mutex);
    scheduleNextMessage(lock);
}

VALDI_CLASS_IMPL(JavaScriptMessagePort);

JavaScriptMessagePort::JavaScriptMessagePort(Ref<JavaScriptMessagePortEndpoint> endpoint)
    : _endpoint(std::move(endpoint)) {}

JavaScriptMessagePort::~JavaScriptMessagePort() {
    close();
}

bool JavaScriptMessagePort::isActive() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _active;
}

Result<Void> JavaScriptMessagePort::validateTransfer(const JavaScriptMessagePort* sourcePort) const {
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (!_active) {
            return Error("MessagePort in transfer list is already detached");
        }
        if (sourcePort == this) {
            return Error("Transfer list contains source MessagePort");
        }
    }
    return _endpoint->validateTransfer(*this);
}

Ref<JavaScriptMessagePortEndpoint> JavaScriptMessagePort::transfer(const Ref<JavaScriptRuntime>& runtime) {
    {
        std::lock_guard<Mutex> lock(_mutex);
        SC_ASSERT(_active);
        _active = false;
        _onMessage.reset();
    }
    _endpoint->transfer(*this, runtime);
    return _endpoint;
}

Ref<JavaScriptRuntime> JavaScriptMessagePort::getPeerRuntime() const {
    auto peer = _endpoint->getPeer();
    return peer != nullptr ? peer->getOwnerRuntime() : nullptr;
}

void JavaScriptMessagePort::postMessage(const Ref<JavaScriptMessage>& message) {
    auto peer = _endpoint->getPeer();
    if (peer != nullptr) {
        peer->enqueue(message);
    }
}

void JavaScriptMessagePort::start() {
    if (isActive()) {
        _endpoint->start(*this);
    }
}

void JavaScriptMessagePort::close() {
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (!_active) {
            return;
        }
        _active = false;
        _onMessage.reset();
    }
    _endpoint->close(*this);
}

void JavaScriptMessagePort::setOnMessage(Shared<JSValueRefHolder> callback) {
    const auto shouldStart = callback != nullptr;
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (!_active) {
            return;
        }
        _onMessage = std::move(callback);
    }
    if (shouldStart) {
        _endpoint->start(*this);
    }
}

Shared<JSValueRefHolder> JavaScriptMessagePort::getOnMessage() const {
    std::lock_guard<Mutex> lock(_mutex);
    return _onMessage;
}

void JavaScriptMessagePort::dispatchMessage(JavaScriptEntryParameters& entry, const Ref<JavaScriptMessage>& message) {
    auto callback = getOnMessage();
    if (callback == nullptr) {
        return;
    }
    auto handler = callback->getJsValue(entry.jsContext, entry.exceptionTracker);
    if (!entry.exceptionTracker) {
        return;
    }
    message->callHandler(entry, handler);
}

Result<Ref<JavaScriptMessage>> JavaScriptMessage::make(const Value& data,
                                                       const Value& transfer,
                                                       const Ref<JavaScriptRuntime>& targetRuntime,
                                                       const JavaScriptMessagePort* sourcePort) {
    std::vector<Ref<JavaScriptMessagePort>> ports;
    if (!transfer.isUndefined()) {
        if (!transfer.isArray()) {
            return Error("MessagePort transfer list must be an array");
        }
        const auto* values = transfer.getArray();
        ports.reserve(values->size());
        std::unordered_set<const JavaScriptMessagePort*> uniquePorts;
        for (const auto& value : *values) {
            if (!value.isValdiObject()) {
                return Error("Transfer list contains an unsupported value");
            }
            auto port = castOrNull<JavaScriptMessagePort>(value.getValdiObject());
            if (port == nullptr) {
                return Error("Only MessagePort values can be transferred");
            }
            if (!uniquePorts.insert(port.get()).second) {
                return Error("Transfer list contains duplicate MessagePort");
            }
            auto validation = port->validateTransfer(sourcePort);
            if (!validation) {
                return validation.moveError();
            }
            ports.emplace_back(std::move(port));
        }
    }

    std::vector<Ref<ValdiObject>> transferredPortSources;
    transferredPortSources.reserve(ports.size());
    std::vector<Ref<JavaScriptMessagePortEndpoint>> transferredPorts;
    transferredPorts.reserve(ports.size());
    for (const auto& port : ports) {
        transferredPortSources.emplace_back(port);
        transferredPorts.emplace_back(port->transfer(targetRuntime));
    }
    return makeShared<JavaScriptMessage>(data, std::move(transferredPortSources), std::move(transferredPorts));
}

JSValueRef JavaScriptMessagePort::makeClass(IJavaScriptContext& context, JSExceptionTracker& exceptionTracker) {
    auto definition = JSNativeClassBinder<JavaScriptMessagePort>("MessagePort")
                          .bindMethod<&JavaScriptMessagePort::postMessageFromJavaScript>("postMessage")
                          .bindMethod<&JavaScriptMessagePort::startFromJavaScript>("start")
                          .bindMethod<&JavaScriptMessagePort::closeFromJavaScript>("close")
                          .bindAccessor<&JavaScriptMessagePort::getOnMessageForJavaScript,
                                        &JavaScriptMessagePort::setOnMessageForJavaScript>("onmessage", true, true)
                          .extractClassDefinition();
    return context.newNativeClass(nullptr, definition, exceptionTracker);
}

JSValueRef JavaScriptMessagePort::make(IJavaScriptContext& context,
                                       JSExceptionTracker& exceptionTracker,
                                       const Ref<JavaScriptMessagePortEndpoint>& endpoint) {
    auto* runtime = dynamic_cast<JavaScriptRuntime*>(context.getTaskScheduler());
    SC_ASSERT(runtime != nullptr);

    auto port = makeShared<JavaScriptMessagePort>(endpoint);
    endpoint->attach(port, strongSmallRef(runtime));
    auto portClass = context.getPropertyFromGlobalObjectCached(STRING_LITERAL("MessagePort"), exceptionTracker);
    if (!exceptionTracker) {
        return context.newUndefined();
    }
    return context.newObjectFromNativeClass(port, portClass.get(), exceptionTracker);
}

JSValueRef JavaScriptMessagePort::postMessageFromJavaScript(Value data,
                                                            Value transfer,
                                                            JSFunctionNativeCallContext& callContext) {
    if (!isActive()) {
        return callContext.getContext().newUndefined();
    }

    auto targetRuntime = getPeerRuntime();
    if (targetRuntime == nullptr) {
        return callContext.getContext().newUndefined();
    }

    auto message = JavaScriptMessage::make(data, transfer, targetRuntime, this);
    if (!message) {
        return callContext.throwError(message.moveError());
    }
    postMessage(message.moveValue());
    return callContext.getContext().newUndefined();
}

void JavaScriptMessagePort::startFromJavaScript(JSFunctionNativeCallContext& /*callContext*/) {
    start();
}

void JavaScriptMessagePort::closeFromJavaScript(JSFunctionNativeCallContext& /*callContext*/) {
    close();
}

JSValueRef JavaScriptMessagePort::getOnMessageForJavaScript(JSFunctionNativeCallContext& callContext) {
    auto callback = getOnMessage();
    if (callback == nullptr) {
        return callContext.getContext().newNull();
    }

    auto value = callback->getJsValue(callContext.getContext(), callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return callContext.getContext().newUndefined();
    }
    return JSValueRef::makeRetained(callContext.getContext(), value);
}

void JavaScriptMessagePort::setOnMessageForJavaScript(JSValue value, JSFunctionNativeCallContext& callContext) {
    if (!callContext.getContext().isValueFunction(value)) {
        setOnMessage(nullptr);
        return;
    }

    auto callback =
        JSValueRefHolder::makeRetainedCallback(callContext.getContext(),
                                               value,
                                               ReferenceInfoBuilder().withProperty(STRING_LITERAL("onmessage")),
                                               callContext.getExceptionTracker());
    if (!callContext.getExceptionTracker()) {
        return;
    }
    setOnMessage(std::move(callback));
}

JSValueRef JavaScriptMessageChannel::makeClass(IJavaScriptContext& context, JSExceptionTracker& exceptionTracker) {
    auto definition = JSNativeClassBinder<JavaScriptMessageChannel>("MessageChannel").extractClassDefinition();
    definition.setConstructor(&JavaScriptMessageChannel::construct);
    return context.newNativeClass(nullptr, definition, exceptionTracker);
}

Ref<RefCountable> JavaScriptMessageChannel::construct(RefCountable* /*classOpaque*/,
                                                      JSFunctionNativeCallContext& callContext) noexcept {
    auto firstEndpoint = makeShared<JavaScriptMessagePortEndpoint>();
    auto secondEndpoint = makeShared<JavaScriptMessagePortEndpoint>();
    firstEndpoint->setPeer(secondEndpoint);
    secondEndpoint->setPeer(firstEndpoint);

    auto firstPort =
        JavaScriptMessagePort::make(callContext.getContext(), callContext.getExceptionTracker(), firstEndpoint);
    if (!callContext.getExceptionTracker()) {
        return nullptr;
    }
    auto secondPort =
        JavaScriptMessagePort::make(callContext.getContext(), callContext.getExceptionTracker(), secondEndpoint);
    if (!callContext.getExceptionTracker()) {
        return nullptr;
    }

    auto& context = callContext.getContext();
    auto& exceptionTracker = callContext.getExceptionTracker();
    context.setObjectProperty(callContext.getThisValue(), "port1", firstPort.get(), exceptionTracker);
    context.setObjectProperty(callContext.getThisValue(), "port2", secondPort.get(), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    return makeShared<JavaScriptMessageChannel>();
}

class TransferredPortJSValueResolver final : public JSValueForNativeObjectResolver {
public:
    TransferredPortJSValueResolver(const JavaScriptMessage& message, const std::vector<JSValueRef>& transferredPorts)
        : _message(message), _transferredPorts(transferredPorts) {}

    std::optional<JSValue> getJSValueForNativeObject(RefCountable* object) final {
        const auto& transferredPortSources = _message.getTransferredPortSources();
        for (size_t i = 0; i < transferredPortSources.size(); i++) {
            if (object == transferredPortSources[i].get()) {
                return _transferredPorts[i].get();
            }
        }
        return std::nullopt;
    }

private:
    const JavaScriptMessage& _message;
    const std::vector<JSValueRef>& _transferredPorts;
};

static JSValueRef makeJavaScriptMessageEvent(JavaScriptEntryParameters& entry, const JavaScriptMessage& message) {
    auto ports = entry.jsContext.newArray(message.getTransferredPorts().size(), entry.exceptionTracker);
    if (!entry.exceptionTracker) {
        return entry.jsContext.newUndefined();
    }
    std::vector<JSValueRef> transferredPorts;
    transferredPorts.reserve(message.getTransferredPorts().size());
    size_t index = 0;
    for (const auto& endpoint : message.getTransferredPorts()) {
        auto port = JavaScriptMessagePort::make(entry.jsContext, entry.exceptionTracker, endpoint);
        if (!entry.exceptionTracker) {
            return entry.jsContext.newUndefined();
        }
        entry.jsContext.setObjectPropertyIndex(ports.get(), index++, port.get(), entry.exceptionTracker);
        if (!entry.exceptionTracker) {
            return entry.jsContext.newUndefined();
        }
        transferredPorts.emplace_back(std::move(port));
    }

    TransferredPortJSValueResolver nativeObjectResolver(message, transferredPorts);
    auto data = valueToJSValue(entry.jsContext,
                               message.getData(),
                               &nativeObjectResolver,
                               ReferenceInfoBuilder().withProperty(STRING_LITERAL("data")),
                               entry.exceptionTracker);
    if (!entry.exceptionTracker) {
        return entry.jsContext.newUndefined();
    }

    auto event = entry.jsContext.newObject(entry.exceptionTracker);
    entry.jsContext.setObjectProperty(event.get(), "data", data.get(), entry.exceptionTracker);
    entry.jsContext.setObjectProperty(event.get(), "ports", ports.get(), entry.exceptionTracker);
    return event;
}

void JavaScriptMessage::callHandler(JavaScriptEntryParameters& entry, const JSValue& handler) {
    auto event = makeJavaScriptMessageEvent(entry, *this);
    if (!entry.exceptionTracker) {
        return;
    }
    JSValueRef parameters[] = {std::move(event)};
    JSFunctionCallContext callContext(entry.jsContext, parameters, 1, entry.exceptionTracker);
    entry.jsContext.callObjectAsFunction(handler, callContext);
}

void JavaScriptMessage::dispatchHandler(const Shared<JSValueRefHolder>& handler, Function<bool()>&& shouldDispatch) {
    if (handler == nullptr) {
        return;
    }
    auto scheduler = handler->getTaskScheduler();
    if (scheduler == nullptr) {
        return;
    }
    auto self = strongSmallRef(this);
    scheduler->dispatchOnJsThreadAsync(
        handler->getContext(),
        [handler, self, shouldDispatch = std::move(shouldDispatch)](JavaScriptEntryParameters& entry) {
            if (!shouldDispatch()) {
                return;
            }
            auto function = handler->getJsValue(entry.jsContext, entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                return;
            }
            self->callHandler(entry, function);
        });
}

} // namespace Valdi
