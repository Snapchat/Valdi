#pragma once

#include "valdi/runtime/JavaScript/JavaScriptMessagePort.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

namespace Valdi {

class JavaScriptWorker : public ValdiObject {
public:
    VALDI_CLASS_HEADER(JavaScriptWorker);

    JavaScriptWorker(Ref<JavaScriptRuntime> hostRuntime, Ref<JavaScriptRuntime> workerRuntime, const StringBox& url);
    ~JavaScriptWorker() override;

    Ref<JavaScriptRuntime> getWorkerRuntime() const;
    /** Identifies a worker boundary by script path, excluding URL query arguments. */
    static StringBox makeANRAttribution(const char* operation, const StringBox& url);
    const StringBox& getSendANRAttribution() const;
    void postInit();
    void setHostOnMessage(Shared<JSValueRefHolder> func);
    void postMessage(const Ref<JavaScriptMessage>& message);
    void close();
    void terminate();

private:
    enum class State {
        Running,
        Closed,
        Terminated,
    };

    Weak<JavaScriptRuntime> _hostRuntime;
    Ref<JavaScriptRuntime> _workerRuntime;
    const StringBox _url;
    const StringBox _sendANRAttribution;
    const StringBox _receiveANRAttribution;
    mutable Mutex _mutex;
    Shared<JSValueRefHolder> _hostOnMessage;
    State _state = State::Running;

    // Called from JS runtime thread
    void doPostInit();
    void doPostMessage(JavaScriptEntryParameters& entry, const Ref<JavaScriptMessage>& message) const;
    void doClose();

    bool isRunning() const;
    bool canDeliverPendingMessage() const;
    Shared<JSValueRefHolder> getHostOnMessage() const;
};

} // namespace Valdi
