#pragma once

#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <deque>
#include <optional>
#include <vector>

namespace Valdi {

class JavaScriptRuntime;
class JSFunctionNativeCallContext;
class JavaScriptMessagePort;
class JavaScriptMessagePortEndpoint;

class JavaScriptMessage final : public SharedPtrRefCountable {
public:
    static Result<Ref<JavaScriptMessage>> make(const Value& data,
                                               const Value& transfer,
                                               const Ref<JavaScriptRuntime>& targetRuntime,
                                               const JavaScriptMessagePort* sourcePort);

    JavaScriptMessage(Value data,
                      std::vector<Ref<ValdiObject>> transferredPortSources,
                      std::vector<Ref<JavaScriptMessagePortEndpoint>> transferredPorts);

    const Value& getData() const;
    const std::vector<Ref<ValdiObject>>& getTransferredPortSources() const;
    const std::vector<Ref<JavaScriptMessagePortEndpoint>>& getTransferredPorts() const;

    void callHandler(JavaScriptEntryParameters& entry, const JSValue& handler);
    void dispatchHandler(const Shared<JSValueRefHolder>& handler, Function<bool()>&& shouldDispatch);

private:
    Value _data;
    std::vector<Ref<ValdiObject>> _transferredPortSources;
    std::vector<Ref<JavaScriptMessagePortEndpoint>> _transferredPorts;
};

class JavaScriptMessagePortEndpoint final : public SharedPtrRefCountable {
public:
    void setPeer(const Ref<JavaScriptMessagePortEndpoint>& peer);

    void attach(const Ref<JavaScriptMessagePort>& handle, const Ref<JavaScriptRuntime>& runtime);
    Result<Void> validateTransfer(const JavaScriptMessagePort& handle) const;
    void transfer(const JavaScriptMessagePort& handle, const Ref<JavaScriptRuntime>& runtime);

    Ref<JavaScriptRuntime> getOwnerRuntime() const;
    Ref<JavaScriptMessagePortEndpoint> getPeer() const;

    void start(const JavaScriptMessagePort& handle);
    void close(const JavaScriptMessagePort& handle);
    void enqueue(const Ref<JavaScriptMessage>& message);

private:
    mutable Mutex _mutex;
    Weak<JavaScriptMessagePortEndpoint> _peer;
    Weak<JavaScriptRuntime> _ownerRuntime;
    Weak<JavaScriptMessagePort> _handle;
    std::deque<Ref<JavaScriptMessage>> _messages;
    std::optional<uint64_t> _scheduledGeneration;
    uint64_t _generation = 0;
    bool _started = false;
    bool _closed = false;

    void scheduleNextMessage(std::unique_lock<Mutex>& lock);
    void dispatchNextMessage(JavaScriptEntryParameters& entry, uint64_t generation);
};

class JavaScriptMessagePort final : public ValdiObject {
public:
    VALDI_CLASS_HEADER(JavaScriptMessagePort);

    static JSValueRef makeClass(IJavaScriptContext& context, JSExceptionTracker& exceptionTracker);

    static JSValueRef make(IJavaScriptContext& context,
                           JSExceptionTracker& exceptionTracker,
                           const Ref<JavaScriptMessagePortEndpoint>& endpoint);

    explicit JavaScriptMessagePort(Ref<JavaScriptMessagePortEndpoint> endpoint);
    ~JavaScriptMessagePort() override;

    bool isActive() const;
    Result<Void> validateTransfer(const JavaScriptMessagePort* sourcePort) const;
    Ref<JavaScriptMessagePortEndpoint> transfer(const Ref<JavaScriptRuntime>& runtime);

    Ref<JavaScriptRuntime> getPeerRuntime() const;
    void postMessage(const Ref<JavaScriptMessage>& message);
    void start();
    void close();

    void setOnMessage(Shared<JSValueRefHolder> callback);
    Shared<JSValueRefHolder> getOnMessage() const;
    void dispatchMessage(JavaScriptEntryParameters& entry, const Ref<JavaScriptMessage>& message);

private:
    Ref<JavaScriptMessagePortEndpoint> _endpoint;
    mutable Mutex _mutex;
    Shared<JSValueRefHolder> _onMessage;
    bool _active = true;

    JSValueRef postMessageFromJavaScript(Value data, Value transfer, JSFunctionNativeCallContext& callContext);
    void startFromJavaScript(JSFunctionNativeCallContext& callContext);
    void closeFromJavaScript(JSFunctionNativeCallContext& callContext);
    JSValueRef getOnMessageForJavaScript(JSFunctionNativeCallContext& callContext);
    void setOnMessageForJavaScript(JSValue value, JSFunctionNativeCallContext& callContext);
};

class JavaScriptMessageChannel final : public SimpleRefCountable {
public:
    static JSValueRef makeClass(IJavaScriptContext& context, JSExceptionTracker& exceptionTracker);

private:
    static Ref<RefCountable> construct(RefCountable* classOpaque, JSFunctionNativeCallContext& callContext) noexcept;
};

} // namespace Valdi
