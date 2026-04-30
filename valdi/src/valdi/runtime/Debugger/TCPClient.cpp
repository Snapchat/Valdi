//
//  TCPClient.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/20.
//

#include "valdi/runtime/Debugger/TCPClient.hpp"
#include "valdi/runtime/Debugger/BoostAsioUtils.hpp"
#include "valdi/runtime/Debugger/TCPConnectionImpl.hpp"
#include "valdi_core/cpp/Threading/Thread.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <atomic>
#include <memory>
#include <thread>

namespace Valdi {

class TCPClientImpl;

class TCPClientDisconnectListenerImpl : public SharedPtrRefCountable, public ITCPConnectionDisconnectListener {
public:
    TCPClientDisconnectListenerImpl(const Ref<TCPClientImpl>& client, const Shared<ITCPClientListener>& listener)
        : _client(client), _listener(listener) {}

    ~TCPClientDisconnectListenerImpl() override = default;

    void onDisconnected(const Ref<ITCPConnection>& connection, const Error& error) override;

private:
    Ref<TCPClientImpl> _client;
    Shared<ITCPClientListener> _listener;
};

class TCPClientImpl : public SharedPtrRefCountable {
public:
    TCPClientImpl() = default;

    ~TCPClientImpl() override {
        stopServices();
    }

    void clearConnection() {
        std::lock_guard<Mutex> lock(_mutex);
        _connection = nullptr;
    }

    void connect(const std::string& address, int32_t port, const Shared<ITCPClientListener>& listener) {
        auto strongSelf = strongRef(this);
        auto connection = makeShared<TCPConnectionImpl>(_ioService);
        connection->getSocket().async_connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(address), static_cast<uint16_t>(port)),
            [strongSelf, connection, listener](const boost::system::error_code& errorCode) {
                if (errorCode.failed()) {
                    strongSelf->stopServices();
                    listener->onDisconnected(errorFromBoostError(errorCode));
                } else {
                    {
                        std::lock_guard<Mutex> lock(strongSelf->_mutex);
                        strongSelf->_connection = connection;
                    }
                    connection->setDisconnectListener(
                        makeShared<TCPClientDisconnectListenerImpl>(strongSelf, listener).toShared());
                    connection->onReady();
                    listener->onConnected(connection);
                }
            });

        auto result = startServices();
        if (!result) {
            listener->onDisconnected(result.error());
            return;
        }
    }

    void stopServices() {
        _started = false;

        Ref<TCPConnectionImpl> connectionToClose;
        Ref<Thread> asioThread;
        std::thread::id asioThreadId;
        {
            std::lock_guard<Mutex> lock(_mutex);
            connectionToClose = _connection;
            _connection = nullptr;
            asioThread = _asioThread;
            asioThreadId = _asioThreadId;
            // Don't clear _asioThread/_asioThreadId yet: keeping them non-null
            // prevents a concurrent startServices() from creating a new service
            // while shutdown is still in progress.
        }

        if (asioThread == nullptr) {
            return;
        }

        // If this is the asio thread (eg. connect failed), close and stop inline;
        // posting would deadlock since we'd be waiting for our own handler.
        if (std::this_thread::get_id() == asioThreadId) {
            if (connectionToClose != nullptr) {
                connectionToClose->close(Error("Disconnected"));
            }
            _ioService.stop();
            _work.reset();
            // Note: stale handlers will be drained in startServices() via
            // reset() before the next run(). We can't drain here because
            // we're still inside a handler on the asio thread; run() will
            // return after this handler completes.
            {
                std::lock_guard<Mutex> lock(_mutex);
                _asioThread = nullptr;
                _asioThreadId = std::thread::id();
            }
            return;
        }

        // COMPOSER-5531: Close sockets on the Asio thread *before* stopping io_service
        // because socket::close requires a valid service and will crash otherwise.
        // Since the Asio thread owns the socket, post cleanup to the thread to prevent
        // concurrent socket access; only Asio thread should access socket during closure
        std::atomic<bool> shutdownDone{false};

        _ioService.post([this, connectionToClose, &shutdownDone]() {
            if (connectionToClose != nullptr) {
                connectionToClose->close(Error("Disconnected"));
            }
            _ioService.stop();
            _work.reset();
            shutdownDone.store(true);
        });

        // join() blocks until run() returns: either because posted lambda
        // called stop() or because run() exited due to an exception.
        asioThread->join();

        // If run() exited before posted lambda could execute, clean up directly.
        // The Asio thread is dead (just joined it), so no concurrent socket access.
        if (!shutdownDone.load()) {
            if (connectionToClose != nullptr) {
                connectionToClose->close(Error("Disconnected"));
            }
            _work.reset();
            _ioService.stop();
        }

        // Drain stale handlers (e.g. cancelled async_read from prior session)
        // that were queued but not dispatched before io_service was stopped.
        // The asio thread is joined, so there's no concurrent access.
        _ioService.reset();
        _ioService.poll();

        // Shutdown fully complete; now allow startServices() to proceed.
        {
            std::lock_guard<Mutex> lock(_mutex);
            _asioThread = nullptr;
            _asioThreadId = std::thread::id();
        }
    }

private:
    Mutex _mutex;
    boost::asio::io_service _ioService;
    Ref<Thread> _asioThread;
    std::thread::id _asioThreadId;
    Ref<TCPConnectionImpl> _connection;
    std::shared_ptr<boost::asio::io_service::work> _work;
    std::atomic_bool _started = {false};

    Result<Void> startServices() {
        std::lock_guard<Mutex> lock(_mutex);

        if (_asioThread != nullptr) {
            return Void();
        }

        _started = true;
        // Allow io_service to be reused after a previous stop()
        _ioService.reset();
        // Use a work guard to keep run() active until handler returns to
        // prevent possible deadlock if asio thread exits (eg. error or other thread closure)
        // and we post and wait indefinitely for the non-existent thread to complete.
        _work = std::make_shared<boost::asio::io_service::work>(_ioService);

        auto threadResult =
            Thread::create(STRING_LITERAL("TCP Client"), ThreadQoSClassNormal, [this]() { this->runIOService(); });
        if (!threadResult) {
            _work.reset();
            return threadResult.moveError();
        }
        _asioThread = threadResult.moveValue();
        return Void();
    }

    void runIOService() {
        {
            std::lock_guard<Mutex> lock(_mutex);
            _asioThreadId = std::this_thread::get_id();
        }
        boost::system::error_code ec;
        _ioService.run(ec);
    }
};

void TCPClientDisconnectListenerImpl::onDisconnected(const Ref<ITCPConnection>& /*connection*/, const Error& error) {
    auto client = _client;
    if (client != nullptr) {
        client->clearConnection();
    }
    _listener->onDisconnected(error);
    _client = nullptr;
}

TCPClient::TCPClient() : _impl(makeShared<TCPClientImpl>()) {}

TCPClient::~TCPClient() {
    disconnect();
}

void TCPClient::connect(const std::string& address, int32_t port, const Shared<ITCPClientListener>& listener) {
    _impl->connect(address, port, listener);
}

void TCPClient::disconnect() {
    _impl->stopServices();
}

} // namespace Valdi
