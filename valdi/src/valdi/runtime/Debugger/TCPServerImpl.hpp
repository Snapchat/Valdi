//
//  TCPServerImpl.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 10/2/19.
//

#pragma once

#include "valdi/runtime/Debugger/BoostAsioUtils.hpp"
#include "valdi/runtime/Debugger/ITCPServer.hpp"
#include "valdi/runtime/Debugger/TCPServerConnectionImpl.hpp"
#include "valdi_core/cpp/Threading/Thread.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include <atomic>
#include <memory>
#include <vector>

namespace Valdi {

class TCPServerDisconnectHandler;

class TCPServerImpl : public ITCPServer {
public:
    TCPServerImpl(uint32_t port, ITCPServerListener* listener);
    ~TCPServerImpl() override;

    Result<Void> start() override;
    void stop() override;

    uint16_t getBoundPort() override;
    std::vector<std::string> getAvailableAddresses() override;

    friend TCPServerConnectionImpl;

    boost::asio::io_service& getIoService();

    bool ownsClient(const Ref<ITCPConnection>& client) const override;

    std::vector<Ref<ITCPConnection>> getConnectedClients() override;

private:
    uint32_t _port;
    ITCPServerListener* _listener;
    boost::asio::io_service _ioService;
    boost::asio::ip::tcp::acceptor _acceptor;
    std::vector<Ref<TCPServerConnectionImpl>> _connections;
    Mutex _mutex;
    Ref<Thread> _asioThread;
    std::shared_ptr<boost::asio::io_service::work> _work;
    std::atomic_bool _started;

    friend TCPServerDisconnectHandler;

    void onAcceptCompleted(const Ref<TCPServerConnectionImpl>& client, const boost::system::error_code& errorCode);
    void runIOService();
    void listenNext();

    bool removeConnection(const Ref<TCPServerConnectionImpl>& connection);
    void appendConnection(const Ref<TCPServerConnectionImpl>& connection);

    void onDisconnected(const Ref<TCPServerConnectionImpl>& connection, const Error& error);

    void closeAllConnections(const Error& error);
};

} // namespace Valdi
