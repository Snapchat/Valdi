//
//  TCPServerImpl.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 10/2/19.
//

#include "valdi/runtime/Debugger/TCPServerImpl.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

class TCPServerDisconnectHandler : public SharedPtrRefCountable, public ITCPConnectionDisconnectListener {
public:
    explicit TCPServerDisconnectHandler(TCPServerImpl* server) : _server(weakRef(server)) {}
    ~TCPServerDisconnectHandler() override = default;

    void onDisconnected(const Ref<ITCPConnection>& connection, const Error& error) override {
        auto server = getServer();
        if (server != nullptr) {
            server->onDisconnected(castOrNull<TCPServerConnectionImpl>(connection), error);
        }
    }

    Shared<TCPServerImpl> getServer() const {
        return _server.lock();
    }

private:
    Weak<TCPServerImpl> _server;
};

TCPServerImpl::TCPServerImpl(uint32_t port, ITCPServerListener* listener)
    : _port(port), _listener(listener), _acceptor(_ioService), _started(false) {}

TCPServerImpl::~TCPServerImpl() = default;

Result<Void> TCPServerImpl::start() {
    std::lock_guard<Mutex> guard(_mutex);

    if (_asioThread != nullptr) {
        return Void();
    }

    boost::system::error_code ec;
    _acceptor.open(boost::asio::ip::tcp::v4(), ec);
    if (ec.failed()) {
        return errorFromBoostError(ec);
    }

    _acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec.failed()) {
        return errorFromBoostError(ec);
    }

    _acceptor.bind({boost::asio::ip::tcp::v4(), static_cast<unsigned short>(_port)}, ec);
    if (ec.failed()) {
        return errorFromBoostError(ec);
    }

    _acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec.failed()) {
        return errorFromBoostError(ec);
    }

    _started = true;
    // Allow io_service to be reused after a previous stop()
    _ioService.reset();
    // Use a work guard to keep run() active until handler returns to
    // prevent possible deadlock if asio thread exits (eg. error or other thread closure)
    // and we post and wait indefinitely for the non-existent thread to complete.
    _work = std::make_shared<boost::asio::io_service::work>(_ioService);

    auto threadResult =
        Thread::create(STRING_LITERAL("Valdi TCP Server"), ThreadQoSClassNormal, [this]() { runIOService(); });
    if (!threadResult) {
        _work.reset();
        return threadResult.moveError();
    }
    _asioThread = threadResult.moveValue();

    return Void();
}

void TCPServerImpl::runIOService() {
    listenNext();

    boost::system::error_code ec;
    _ioService.run(ec);
    if (ec.failed()) {
        return;
    }
}

uint16_t TCPServerImpl::getBoundPort() {
    boost::system::error_code ec;
    auto endpoint = _acceptor.local_endpoint(ec);
    if (ec.failed()) {
        return 0;
    }

    return endpoint.port();
}

std::vector<std::string> TCPServerImpl::getAvailableAddresses() {
    return Valdi::getAllAvailableAddresses(_ioService);
}

void TCPServerImpl::stop() {
    _started = false;

    Ref<Thread> thread;
    {
        std::lock_guard<Mutex> guard(_mutex);
        thread = _asioThread;
        // Don't clear _asioThread yet: keeping it non-null prevents
        // a concurrent start() from creating a new service while
        // shutdown is still in progress.
    }

    if (thread == nullptr) {
        boost::system::error_code ec;
        _acceptor.close(ec);
        return;
    }

    // COMPOSER-5531: Close sockets on the Asio thread *before* stopping io_service
    // because socket::close requires a valid service and will crash otherwise.
    // Since the Asio thread owns the socket, post cleanup to the thread to prevent
    // concurrent socket access; only Asio thread should access socket during closure
    std::atomic<bool> shutdownDone{false};

    _ioService.post([this, &shutdownDone]() {
        closeAllConnections(Error("Server stopped"));

        boost::system::error_code ec;
        _acceptor.close(ec);
        _ioService.stop();
        _work.reset();
        shutdownDone.store(true);
    });

    // join() blocks until run() returns: either because posted lambda
    // called stop() or because run() exited due to an exception.
    thread->join();

    // If run() exited before posted lambda could execute, clean up directly.
    // The Asio thread is dead (just joined it), so no concurrent socket access.
    if (!shutdownDone.load()) {
        closeAllConnections(Error("Server stopped"));

        boost::system::error_code ec;
        _acceptor.close(ec);
        _ioService.stop();
        _work.reset();
    }

    // Drain stale handlers (e.g. cancelled async_accept/async_read) that
    // were queued but not dispatched before io_service was stopped.
    // The asio thread is joined, so there's no concurrent access.
    _ioService.reset();
    _ioService.poll();

    // Shutdown fully complete; now allow start() to proceed.
    {
        std::lock_guard<Mutex> guard(_mutex);
        _asioThread = nullptr;
    }
}

void TCPServerImpl::closeAllConnections(const Error& error) {
    while (true) {
        Ref<TCPServerConnectionImpl> connection;

        {
            std::lock_guard<Mutex> guard(_mutex);
            if (_connections.empty()) {
                return;
            }
            connection = *_connections.begin();
        }

        connection->close(error);
    }
}

void TCPServerImpl::appendConnection(const Ref<TCPServerConnectionImpl>& connection) {
    std::lock_guard<Mutex> guard(_mutex);
    _connections.emplace_back(connection);
}

bool TCPServerImpl::removeConnection(const Ref<TCPServerConnectionImpl>& connection) {
    std::lock_guard<Mutex> guard(_mutex);

    auto it = _connections.begin();
    while (it != _connections.end()) {
        if (*it == connection) {
            _connections.erase(it);
            return true;
        } else {
            ++it;
        }
    }

    return false;
}

void TCPServerImpl::onAcceptCompleted(const Ref<TCPServerConnectionImpl>& client,
                                      const boost::system::error_code& errorCode) {
    if (!_started.load()) {
        return;
    }

    listenNext();
    if (errorCode.failed()) {
        return;
    }

    appendConnection(client);
    client->onReady();

    _listener->onClientConnected(client);
}

void TCPServerImpl::listenNext() {
    auto connection = Valdi::makeShared<TCPServerConnectionImpl>(this);
    connection->setDisconnectListener(makeShared<TCPServerDisconnectHandler>(this).toShared());

    _acceptor.async_accept(connection->getSocket(), [this, connection](const boost::system::error_code& errorCode) {
        this->onAcceptCompleted(connection, errorCode);
    });
}

void TCPServerImpl::onDisconnected(const Ref<TCPServerConnectionImpl>& connection, const Error& error) {
    if (removeConnection(connection)) {
        _listener->onClientDisconnected(connection, error);
    }
}

boost::asio::io_service& TCPServerImpl::getIoService() {
    return _ioService;
}

std::vector<Ref<ITCPConnection>> TCPServerImpl::getConnectedClients() {
    std::lock_guard<Mutex> guard(_mutex);

    std::vector<Ref<ITCPConnection>> allConnections;
    allConnections.reserve(_connections.size());

    for (const auto& connection : _connections) {
        allConnections.emplace_back(connection);
    }

    return allConnections;
}

bool TCPServerImpl::ownsClient(const Ref<ITCPConnection>& client) const {
    auto disconnectHandler = castOrNull<TCPServerDisconnectHandler>(client->getDisconnectListener());
    if (disconnectHandler == nullptr) {
        return false;
    }
    return disconnectHandler->getServer().get() == this;
}

} // namespace Valdi
