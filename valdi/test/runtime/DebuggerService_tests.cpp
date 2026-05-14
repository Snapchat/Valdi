//
//  DebuggerService_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 9/20/21.
//

#include "valdi/valdi.pb.h"

#include "valdi/runtime/Debugger/DaemonClient.hpp"
#include "valdi/runtime/Debugger/DebuggerService.hpp"
#include "valdi/runtime/Debugger/IDebuggerServiceListener.hpp"
#include "valdi/runtime/Debugger/TCPClient.hpp"
#include "valdi/runtime/Debugger/TCPServer.hpp"
#include "valdi_core/cpp/Threading/TaskQueue.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

#include "valdi_core/cpp/Resources/ValdiPacket.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

enum DebuggerServiceEventType {
    DebuggerServiceEventTypeReceivedUpdatedResources,
    DebuggerServiceEventTypeDaemonClientConnected,
    DebuggerServiceEventTypeDaemonClientDisconnected,
    DebuggerServiceEventTypeDidReceiveClientPayload
};

struct DebuggerServiceEvent {
    DebuggerServiceEventType type;
    std::vector<Shared<Resource>> resources;
    Shared<IDaemonClient> daemonClient;
    int senderClientId = 0;
    BytesView payload;
};

enum TCPClientEventType {
    TCPClientEventTypeConnected,
    TCPClientEventTypeDisconnected,
};

struct TCPClientEvent {
    TCPClientEventType type;
    Ref<ITCPConnection> connection;
    std::optional<Error> error;
};

template<typename T>
class MockListener {
public:
    T dequeueNextEvent() {
        if (!_taskQueue.runNextTask(std::chrono::steady_clock::now() + std::chrono::seconds(1)) ||
            !_currentEvent.has_value()) {
            throw Exception("Operation timed out");
        }

        auto event = std::move(_currentEvent.value());
        _currentEvent = std::nullopt;
        return event;
    }

protected:
    void enqueueEvent(T event) {
        _taskQueue.enqueue([this, event]() { _currentEvent = {event}; });
    }

private:
    TaskQueue _taskQueue;
    std::optional<T> _currentEvent;
};

struct MockDebuggerServiceListener : public IDebuggerServiceListener, public MockListener<DebuggerServiceEvent> {
    void receivedUpdatedResources(const std::vector<Shared<Resource>>& resources) override {
        DebuggerServiceEvent event;
        event.type = DebuggerServiceEventTypeReceivedUpdatedResources;
        event.resources = resources;
        enqueueEvent(event);
    }

    void daemonClientConnected(const Shared<IDaemonClient>& daemonClient) override {
        DebuggerServiceEvent event;
        event.type = DebuggerServiceEventTypeDaemonClientConnected;
        event.daemonClient = daemonClient;
        enqueueEvent(event);
    }

    void daemonClientDisconnected(const Shared<IDaemonClient>& daemonClient) override {
        DebuggerServiceEvent event;
        event.type = DebuggerServiceEventTypeDaemonClientDisconnected;
        event.daemonClient = daemonClient;
        enqueueEvent(event);
    }

    void daemonClientDidReceiveClientPayload(const Shared<IDaemonClient>& daemonClient,
                                             int senderClientId,
                                             const BytesView& payload) override {
        DebuggerServiceEvent event;
        event.type = DebuggerServiceEventTypeDidReceiveClientPayload;
        event.daemonClient = daemonClient;
        event.senderClientId = senderClientId;
        event.payload = payload;
        enqueueEvent(event);
    }
};

struct MockTCPConnectionDataListener : public ITCPConnectionDataListener, public MockListener<BytesView> {
    void onDataReceived(const Ref<ITCPConnection>& connection, const BytesView& bytes) override {
        enqueueEvent(bytes);
    }

    Result<Value> dequeueSubmittedPayload() {
        auto bytes = dequeueNextEvent();

        auto packet = ValdiPacket::read(bytes.data(), bytes.size());
        if (!packet) {
            return packet.moveError();
        }

        return jsonToValue(packet.value().data(), static_cast<int>(packet.value().size()));
    }
};

struct MockTCPClientListener : public ITCPClientListener, public MockListener<TCPClientEvent> {
    Shared<MockTCPConnectionDataListener> dataListener;

    MockTCPClientListener() {
        dataListener = makeShared<MockTCPConnectionDataListener>();
    }

    void onConnected(const Ref<ITCPConnection>& connection) override {
        connection->setDataListener(dataListener);

        TCPClientEvent event;
        event.type = TCPClientEventTypeConnected;
        event.connection = connection;
        enqueueEvent(event);
    }

    void onDisconnected(const Error& error) override {
        TCPClientEvent event;
        event.type = TCPClientEventTypeDisconnected;
        event.error = {error};
        enqueueEvent(event);
    }
};

struct DebuggerServiceWrapper {
    Ref<MockDebuggerServiceListener> listener;
    Shared<DebuggerService> service;
    std::vector<Ref<ITCPConnection>> connections;

    DebuggerServiceWrapper() {
        listener = makeShared<MockDebuggerServiceListener>();

        service =
            makeShared<DebuggerService>(
                nullptr, snap::valdi_core::Platform::Ios, 0, false, Valdi::strongSmallRef(&ConsoleLogger::getLogger()))
                .toShared();
        service->postInit();
    }

    ~DebuggerServiceWrapper() {
        for (const auto& connection : connections) {
            connection->close(Error("Shutting down"));
        }

        connections.clear();
    }

    void connectToService(const Ref<TCPClient>& client,
                          const Shared<MockTCPClientListener>& tcpListener,
                          Ref<ITCPConnection>& connection) {
        auto boundPort = service->getBoundPort();

        ASSERT_TRUE(boundPort != 0);

        client->connect("127.0.0.1", static_cast<int32_t>(boundPort), tcpListener);

        auto clientConnectEvent = tcpListener->dequeueNextEvent();

        ASSERT_EQ(TCPClientEventTypeConnected, clientConnectEvent.type);

        auto daemonClientConnected = listener->dequeueNextEvent();

        ASSERT_EQ(DebuggerServiceEventTypeDaemonClientConnected, daemonClientConnected.type);

        connection = clientConnectEvent.connection;
        connections.emplace_back(connection);
    }
};

TEST(DebuggerService, canConnectAndDisconnect) {
    DebuggerServiceWrapper wrapper;

    wrapper.service->addListener(wrapper.listener.get());

    wrapper.service->start();

    auto client = makeShared<TCPClient>();
    auto tcpListener = makeShared<MockTCPClientListener>();

    Ref<ITCPConnection> connection;

    wrapper.connectToService(client, tcpListener, connection);

    ASSERT_TRUE(connection != nullptr);

    connection->close(Error("Disconnect"));

    auto daemonClientDisconnected = wrapper.listener->dequeueNextEvent();

    ASSERT_EQ(DebuggerServiceEventTypeDaemonClientDisconnected, daemonClientDisconnected.type);

    client->disconnect();
}

TEST(DebuggerService, canReceiveUpdatedResources) {
    DebuggerServiceWrapper wrapper;

    wrapper.service->addListener(wrapper.listener.get());

    wrapper.service->start();

    auto client = makeShared<TCPClient>();
    auto tcpListener = makeShared<MockTCPClientListener>();

    Ref<ITCPConnection> connection;

    wrapper.connectToService(client, tcpListener, connection);

    ASSERT_TRUE(connection != nullptr);

    auto updatedResources = ValueArray::make(1);
    Value resource1;
    resource1.setMapValue("data", Value("hello"))
        .setMapValue("bundle_name", Value("bundle1"))
        .setMapValue("file_path_within_bundle", Value("src/file1.js"));
    updatedResources->emplace(0, resource1);

    Value resources;
    resources.setMapValue("resources", Value(updatedResources));
    Value event;
    event.setMapValue("updated_resources", resources);
    Value payload;
    payload.setMapValue("event", event);

    auto output = valueToJson(payload);
    auto out = makeShared<ByteBuffer>();
    ValdiPacket::write(reinterpret_cast<const Byte*>(output->data()), output->size(), *out);

    connection->submitData(out->toBytesView());

    auto dequeuedResources = wrapper.listener->dequeueNextEvent();
    ASSERT_EQ(DebuggerServiceEventTypeReceivedUpdatedResources, dequeuedResources.type);
    ASSERT_EQ(static_cast<size_t>(1), dequeuedResources.resources.size());
}

TEST(DebuggerService, submitConfigurationOnConnect) {
    DebuggerServiceWrapper wrapper;

    wrapper.service->addListener(wrapper.listener.get());

    wrapper.service->start();

    auto client = makeShared<TCPClient>();
    auto tcpListener = makeShared<MockTCPClientListener>();

    Ref<ITCPConnection> connection;

    wrapper.connectToService(client, tcpListener, connection);

    ASSERT_TRUE(connection != nullptr);

    auto result = tcpListener->dataListener->dequeueSubmittedPayload();

    ASSERT_TRUE(result.success()) << result.description();

    auto& sentPayload = result.value();

    ASSERT_FALSE(sentPayload.getMapValue("request").isNullOrUndefined());
    const auto& request = sentPayload.getMapValue("request");
    ASSERT_FALSE(request.getMapValue("configure").isNullOrUndefined());
}

TEST(DebuggerService, canForwardLogs) {
    DebuggerServiceWrapper wrapper;

    wrapper.service->addListener(wrapper.listener.get());

    wrapper.service->start();

    auto client = makeShared<TCPClient>();
    auto tcpListener = makeShared<MockTCPClientListener>();

    Ref<ITCPConnection> connection;

    wrapper.connectToService(client, tcpListener, connection);

    ASSERT_TRUE(connection != nullptr);

    // Ignore the configuration payload
    tcpListener->dataListener->dequeueSubmittedPayload();

    // Submit a log through the bridge logger - text will appear in test result log
    wrapper.service->getBridgeLogger()->log(Valdi::LogTypeInfo, "This is a great log");

    auto result = tcpListener->dataListener->dequeueSubmittedPayload();

    ASSERT_TRUE(result.success()) << result.description();

    auto& sentPayload = result.value();

    ASSERT_FALSE(sentPayload.getMapValue("event").isNullOrUndefined());
    const auto& event = sentPayload.getMapValue("event");
    ASSERT_FALSE(event.getMapValue("new_logs").isNullOrUndefined());
    const auto& logs = event.getMapValue("new_logs").getArray();
    ASSERT_EQ(static_cast<size_t>(1), logs->size());

    const auto& log = (*logs)[0];

    ASSERT_EQ(STRING_LITERAL("info"), log.getMapValue("severity").toStringBox());
    ASSERT_EQ(STRING_LITERAL("This is a great log"), log.getMapValue("log").toStringBox());
}

enum TCPServerEventType {
    TCPServerEventTypeClientConnected,
    TCPServerEventTypeClientDisconnected,
};

struct TCPServerEvent {
    TCPServerEventType type;
    Ref<ITCPConnection> client;
    std::optional<Error> error;
};

struct MockTCPServerListener : public ITCPServerListener, public MockListener<TCPServerEvent> {
    void onClientConnected(const Ref<ITCPConnection>& client) override {
        TCPServerEvent event;
        event.type = TCPServerEventTypeClientConnected;
        event.client = client;
        enqueueEvent(event);
    }

    void onClientDisconnected(const Ref<ITCPConnection>& client, const Error& error) override {
        TCPServerEvent event;
        event.type = TCPServerEventTypeClientDisconnected;
        event.client = client;
        event.error = {error};
        enqueueEvent(event);
    }
};

TEST(TCPServer, canRestartAndAcceptConnections) {
    auto serverListener = makeShared<MockTCPServerListener>();
    auto server = TCPServer::create(0, serverListener.get());

    auto result = server->start();
    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_NE(0, server->getBoundPort());

    server->stop();

    result = server->start();
    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_NE(0, server->getBoundPort());

    auto client = makeShared<TCPClient>();
    auto clientListener = makeShared<MockTCPClientListener>();
    client->connect("127.0.0.1", static_cast<int32_t>(server->getBoundPort()), clientListener);

    auto clientEvent = clientListener->dequeueNextEvent();
    ASSERT_EQ(TCPClientEventTypeConnected, clientEvent.type);

    auto serverEvent = serverListener->dequeueNextEvent();
    ASSERT_EQ(TCPServerEventTypeClientConnected, serverEvent.type);

    clientEvent.connection->close(Error("Done"));

    auto disconnectEvent = serverListener->dequeueNextEvent();
    ASSERT_EQ(TCPServerEventTypeClientDisconnected, disconnectEvent.type);

    client->disconnect();
    server->stop();
}

TEST(TCPClient, canReconnectAfterDisconnect) {
    auto serverListener = makeShared<MockTCPServerListener>();
    auto server = TCPServer::create(0, serverListener.get());

    auto result = server->start();
    ASSERT_TRUE(result.success()) << result.description();

    auto client = makeShared<TCPClient>();
    auto port = static_cast<int32_t>(server->getBoundPort());

    // First connection
    auto listener1 = makeShared<MockTCPClientListener>();
    client->connect("127.0.0.1", port, listener1);

    auto connectEvent1 = listener1->dequeueNextEvent();
    ASSERT_EQ(TCPClientEventTypeConnected, connectEvent1.type);
    serverListener->dequeueNextEvent(); // server sees connect

    connectEvent1.connection->close(Error("Disconnect"));
    serverListener->dequeueNextEvent(); // server sees disconnect
    client->disconnect();

    // Reconnect with same client
    auto listener2 = makeShared<MockTCPClientListener>();
    client->connect("127.0.0.1", port, listener2);

    auto connectEvent2 = listener2->dequeueNextEvent();
    ASSERT_EQ(TCPClientEventTypeConnected, connectEvent2.type);

    auto serverEvent2 = serverListener->dequeueNextEvent();
    ASSERT_EQ(TCPServerEventTypeClientConnected, serverEvent2.type);

    connectEvent2.connection->close(Error("Done"));
    serverListener->dequeueNextEvent(); // server sees disconnect
    client->disconnect();
    server->stop();
}

TEST(TCPServer, rapidStopStartDoesNotCrash) {
    auto serverListener = makeShared<MockTCPServerListener>();
    auto server = TCPServer::create(0, serverListener.get());

    for (int i = 0; i < 20; i++) {
        auto result = server->start();
        ASSERT_TRUE(result.success()) << "Iteration " << i << ": " << result.description();
        ASSERT_NE(0, server->getBoundPort());
        server->stop();
    }
}

TEST(TCPClient, rapidDisconnectReconnectDoesNotCrash) {
    auto serverListener = makeShared<MockTCPServerListener>();
    auto server = TCPServer::create(0, serverListener.get());

    auto result = server->start();
    ASSERT_TRUE(result.success()) << result.description();

    auto client = makeShared<TCPClient>();
    auto port = static_cast<int32_t>(server->getBoundPort());

    for (int i = 0; i < 10; i++) {
        auto listener = makeShared<MockTCPClientListener>();
        client->connect("127.0.0.1", port, listener);

        auto connectEvent = listener->dequeueNextEvent();
        ASSERT_EQ(TCPClientEventTypeConnected, connectEvent.type) << "Iteration " << i;
        serverListener->dequeueNextEvent(); // server sees connect

        connectEvent.connection->close(Error("Cycle"));
        serverListener->dequeueNextEvent(); // server sees disconnect
        client->disconnect();
    }

    server->stop();
}

} // namespace ValdiTest
