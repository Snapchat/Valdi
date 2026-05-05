//
//  JSCoreDebuggerProxy.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 6/17/20.
//

#pragma once

#include "valdi/runtime/Debugger/ITCPServer.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace ValdiJSCore {

/**
 The Debugger Proxy hosts 2 TCP servers:
 - one which is meant to host a single client which should be JSCore itself.
 - one which can receive external connections.

 The proxy manages the communication between JSCore and external clients.
 */
class DebuggerProxy : public Valdi::ITCPServerListener {
public:
    DebuggerProxy();

    void onClientConnected(const Valdi::Ref<Valdi::ITCPConnection>& client) override;

    void onClientDisconnected(const Valdi::Ref<Valdi::ITCPConnection>& client, const Valdi::Error& error) override;

    uint16_t getExternalPort() const;
    uint16_t getInternalPort() const;

    static DebuggerProxy* start();

private:
    Valdi::Ref<Valdi::ITCPServer> _jsHost;
    Valdi::Ref<Valdi::ITCPServer> _externHost;
};

/**
 An abstract class representing a connection to either JSCore or an external client.
 */
class DebuggerConnection : public Valdi::SharedPtrRefCountable, public Valdi::ITCPConnectionDataListener {
public:
    explicit DebuggerConnection(Valdi::ITCPConnection& client);

    void onDataReceived(const Valdi::Ref<Valdi::ITCPConnection>& connection, const Valdi::BytesView& bytes) override;

    void submitMessage(const Valdi::Value& value);
    void submitJson(const std::string_view& json);

    virtual void processMessage(const Valdi::StringBox& message) = 0;

private:
    Valdi::ITCPConnection& _client;
    Valdi::Mutex _mutex;
    Valdi::Bytes _buffer;

    bool consumeBuffer();
};

/**
 Represents a connection to JSCore.
 Messages from JSCore will be forwarded to any connected
 external clients.
 */
class JSDebuggerConnection : public DebuggerConnection {
public:
    JSDebuggerConnection(Valdi::ITCPServer& externHost, Valdi::ITCPConnection& client);
    ~JSDebuggerConnection() override;

    void processMessage(const Valdi::StringBox& message) override;

    int getTargetId() const;

private:
    Valdi::ITCPServer& _externHost;
    int _targetId = 0;

    void sendMessageToFrontend(const Valdi::StringBox& message);

    void setTargetList(const Valdi::StringBox& message);
};

/**
 Represents a connection to an external client.
 Messages received from external clients will be forwarded
 to any connected JSCore clients.
 */
class ExternalDebuggerConnection : public DebuggerConnection {
public:
    ExternalDebuggerConnection(Valdi::ITCPServer& jsHost, Valdi::ITCPConnection& client);

    ~ExternalDebuggerConnection() override;

    void processMessage(const Valdi::StringBox& message) override;

    void submitMessageToJs(const Valdi::StringBox& event, const Valdi::StringBox& message);

private:
    Valdi::ITCPServer& _jsHost;
};

} // namespace ValdiJSCore