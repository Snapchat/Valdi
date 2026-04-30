//
//  DebuggerService.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/27/19.
//

#include "valdi/runtime/Debugger/DebuggerService.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Debugger/IDebuggerServiceListener.hpp"
#include "valdi/runtime/Debugger/TCPServer.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include "valdi/RuntimeMessageHandler.hpp"

#include "valdi_core/cpp/Utils/ContainerUtils.hpp"

namespace Valdi {

class DaemonClientTCPDataSender : public DaemonClientDataSender {
public:
    explicit DaemonClientTCPDataSender(Ref<ITCPConnection> connection) : _connection(std::move(connection)) {}
    ~DaemonClientTCPDataSender() override = default;

    bool requiresPacketEncoding() const override {
        return true;
    }

    void submitData(const BytesView& data) override {
        _connection->submitData(data);
    }

    const Ref<ITCPConnection>& getConnection() const {
        return _connection;
    }

private:
    Ref<ITCPConnection> _connection;
};

class DaemonClientTCPListener : public ITCPConnectionDataListener {
public:
    explicit DaemonClientTCPListener(Weak<DaemonClient> daemonClient) : _daemonClient(std::move(daemonClient)) {}
    ~DaemonClientTCPListener() override = default;

    void onDataReceived(const Ref<ITCPConnection>& /*connection*/, const BytesView& bytes) override {
        auto daemonClient = getDaemonClient();
        if (daemonClient != nullptr) {
            daemonClient->onDataReceived(bytes);
        }
    }

    Shared<DaemonClient> getDaemonClient() const {
        return _daemonClient.lock();
    }

private:
    Weak<DaemonClient> _daemonClient;
};

constexpr uint32_t kStandaloneDebuggerPort = 13591;
constexpr uint32_t kMobileDebuggerPort = 13592;

DebuggerService::DebuggerService(const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
                                 snap::valdi_core::Platform platformType,
                                 uint32_t debuggerPort,
                                 bool disableHotReloader,
                                 const Ref<ILogger>& logger)
    : _runtimeMessageHandler(runtimeMessageHandler),
      _platformType(platformType),
      _logger(logger),
      _bridgeLogger(makeShared<BridgeLogger>(logger)),
      _debuggerPort(debuggerPort),
      _disableHotReloader(disableHotReloader) {
    _dispatchQueue = DispatchQueue::create(STRING_LITERAL("Valdi Debugger Service"), ThreadQoSClassNormal);
}

DebuggerService::~DebuggerService() {
    stop();
    _dispatchQueue->flushAndTeardown();
}

void DebuggerService::postInit() {
    _bridgeLogger->setListener(weakRef(this));
}

void DebuggerService::start() {
    _dispatchQueue->async([this]() {
        if (_started) {
            // Already started
            return;
        }

        _started = true;
        this->doStart(/* attemptNumber */ 1);
    });
}

void DebuggerService::doStart(int attemptNumber) {
    if (_tcpServer != nullptr) {
        return;
    }

    if (!_started) {
        return;
    }

    _tcpServer = TCPServer::create(_debuggerPort, this);
    auto result = _tcpServer->start();
    if (result) {
        VALDI_INFO(*_logger, "Started Valdi debugger service on {}", _debuggerPort);
    } else {
        [[maybe_unused]] auto attemptLogType = attemptNumber > 3 ? Valdi::LogTypeDebug : Valdi::LogTypeWarn;
        VALDI_LOG(*_logger,
                  attemptLogType,
                  "Failed to start Valdi debugger service (attempt #{}): {}",
                  attemptNumber,
                  result.error());
        _tcpServer = nullptr;

        long retryDelay = attemptNumber < 5 ? attemptNumber : 5;
        _dispatchQueue->asyncAfter([this, attemptNumber]() { this->doStart(attemptNumber + 1); },
                                   std::chrono::seconds(retryDelay));
    }
}

void DebuggerService::stop() {
    _dispatchQueue->async([this]() {
        _started = false;
        if (_tcpServer != nullptr) {
            _tcpServer->stop();
            _tcpServer = nullptr;
            VALDI_INFO(*_logger, "Stopped Valdi debugger service");
        }
    });
}

uint16_t DebuggerService::getBoundPort() {
    uint16_t boundPort = 0;
    _dispatchQueue->sync([&]() {
        if (_tcpServer != nullptr) {
            boundPort = _tcpServer->getBoundPort();
        }
    });

    return boundPort;
}

void DebuggerService::addListener(IDebuggerServiceListener* listener) {
    std::lock_guard<Mutex> guard(_mutex);

    _listeners.emplace_back(weakRef(listener));

    if (!_cachedReloadedResources.empty()) {
        std::vector<Shared<Resource>> previouslyReloadedResources;
        previouslyReloadedResources.reserve(_cachedReloadedResources.size());
        for (const auto& kv : _cachedReloadedResources) {
            previouslyReloadedResources.emplace_back(kv.second);
        }

        listener->receivedUpdatedResources(previouslyReloadedResources);
    }

    for (const auto& daemonClient : _clients) {
        listener->daemonClientConnected(daemonClient.toShared());
    }
}

void DebuggerService::removeListener(IDebuggerServiceListener* listener) {
    std::lock_guard<Mutex> guard(_mutex);

    auto it = _listeners.begin();
    while (it != _listeners.end()) {
        auto existingListener = it->lock();
        if (existingListener == nullptr || existingListener.get() == listener) {
            it = _listeners.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& daemonClient : _clients) {
        listener->daemonClientDisconnected(daemonClient.toShared());
    }
}

void DebuggerService::onClientConnected(const Ref<ITCPConnection>& client) {
    VALDI_INFO(*_logger, "Client {} connected", client->getAddress());
    auto daemonClient = addDaemonClient(client);

    submitDaemonClientConfiguration(daemonClient, getApplicationId());
}

void DebuggerService::submitDaemonClientConfiguration(const Ref<DaemonClient>& daemonClient,
                                                      const StringBox& applicationId) {
    daemonClient->sendConfiguration(applicationId, _platformType, _disableHotReloader);
}

void DebuggerService::setApplicationId(const StringBox& applicationId) {
    std::lock_guard<Mutex> guard(_mutex);
    if (_applicationId != applicationId) {
        _applicationId = applicationId;

        for (const auto& daemonClient : _clients) {
            submitDaemonClientConfiguration(daemonClient, applicationId);
        }
    }
}

StringBox DebuggerService::getApplicationId() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _applicationId;
}

void DebuggerService::onClientDisconnected(const Ref<ITCPConnection>& client, const Error& error) {
    VALDI_INFO(*_logger, "Client {} disconnected: {}", client->getAddress(), error);
    auto daemonClient = dynamic_cast<DaemonClientTCPListener*>(client->getDataListener().get())->getDaemonClient();
    if (daemonClient != nullptr) {
        daemonClient->closeClient(error);
    }
}

void DebuggerService::willLog(LogType type, const std::string& message) {
    std::lock_guard<Mutex> guard(_mutex);
    for (const auto& daemonClient : _clients) {
        daemonClient->sendLog(type, message);
    }
}

void DebuggerService::didReceiveUpdatedResources(const SharedVector<Shared<Resource>>& resources) {
    std::lock_guard<Mutex> lock(_mutex);
    for (const auto& resource : *resources) {
        _cachedReloadedResources[resource->resourceId] = resource;
    }

    for (const auto& it : _listeners) {
        auto listener = it.lock();
        if (listener != nullptr) {
            listener->receivedUpdatedResources(*resources);
        }
    }
}

void DebuggerService::didReceiveClientPayload(DaemonClient* daemonClient,
                                              int senderClientId,
                                              const BytesView& payload) {
    std::lock_guard<Mutex> lock(_mutex);
    for (const auto& it : _listeners) {
        auto listener = it.lock();
        if (listener != nullptr) {
            listener->daemonClientDidReceiveClientPayload(strongRef(daemonClient), senderClientId, payload);
        }
    }
}

Ref<DaemonClient> DebuggerService::addDaemonClient(const Ref<ITCPConnection>& tcpClient) {
    if (_runtimeMessageHandler != nullptr) {
        _runtimeMessageHandler->onDebugMessage(LogTypeInfo, "Connected to Valdi Hot Reloader!");
    }

    std::lock_guard<Mutex> guard(_mutex);

    auto daemonClient = Valdi::makeShared<DaemonClient>(this, ++_connectionIdSequence, _logger);
    tcpClient->setDataListener(Valdi::makeShared<DaemonClientTCPListener>(daemonClient));
    daemonClient->setDataSender(Valdi::makeShared<DaemonClientTCPDataSender>(tcpClient));
    _clients.emplace_back(daemonClient);

    for (const auto& it : _listeners) {
        auto listener = it.lock();
        if (listener != nullptr) {
            listener->daemonClientConnected(daemonClient.toShared());
        }
    }

    return daemonClient;
}

void DebuggerService::daemonClientDidDisconnect(DaemonClient* daemonClient, const Error& error) {
    bool erased = false;
    auto ref = strongRef(daemonClient);
    {
        std::lock_guard<Mutex> guard(_mutex);

        erased = Valdi::eraseFirstIf(_clients, [=](const auto& client) { return client.get() == daemonClient; });

        if (erased) {
            for (const auto& it : _listeners) {
                auto listener = it.lock();
                if (listener != nullptr) {
                    listener->daemonClientDisconnected(ref);
                }
            }
        }
    }

    if (erased) {
        if (_runtimeMessageHandler != nullptr) {
            _runtimeMessageHandler->onDebugMessage(LogTypeInfo, "Disconnected from Valdi Hot Reloader");
        }

        auto* dataSender = dynamic_cast<DaemonClientTCPDataSender*>(daemonClient->getDataSender().get());
        if (dataSender != nullptr) {
            dataSender->getConnection()->close(error);
        }
    }
}

Ref<ILogger> DebuggerService::getBridgeLogger() const {
    return _bridgeLogger;
}

uint32_t DebuggerService::resolveDebuggerPort(bool isStandalone) {
    return isStandalone ? kStandaloneDebuggerPort : kMobileDebuggerPort;
}

} // namespace Valdi
