//
//  TCPConnectionImpl.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/20.
//

#include "valdi/runtime/Debugger/TCPConnectionImpl.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"

namespace Valdi {

constexpr size_t kBufferSize = 8192;
using ArrayBuffer = std::array<Byte, kBufferSize>;

void cleanUpBuffer(ArrayBuffer& /*arrayBuffer*/) {}

// This just wraps an std::array provided from an ObjectPool
class Buffer : public SharedPtrRefCountable {
public:
    Buffer() : _array(ObjectPool<std::array<Byte, kBufferSize>>::get().getOrCreate(&cleanUpBuffer)) {}

    ~Buffer() override = default;

    Byte* data() {
        return _array->data();
    }

    size_t size() {
        return _array->size();
    }

private:
    ObjectPoolEntry<ArrayBuffer, void (*)(ArrayBuffer&)> _array;
};

TCPConnectionImpl::TCPConnectionImpl(boost::asio::io_service& ioService) : _socket(ioService) {}

TCPConnectionImpl::~TCPConnectionImpl() = default;

boost::asio::ip::tcp::socket& TCPConnectionImpl::getSocket() {
    return _socket;
}

void TCPConnectionImpl::submitData(const BytesView& bytes) {
    std::lock_guard<Mutex> guard(_mutex);
    _pendingPackets.emplace_back(bytes);

    if (_pendingPackets.size() == 1) {
        lockFreeDoSend();
    }
}

void TCPConnectionImpl::lockFreeDoSend() {
    if (_pendingPackets.empty()) {
        return;
    }

    auto bytes = _pendingPackets.front();

    boost::asio::async_write(_socket,
                             boost::asio::buffer(bytes.data(), bytes.size()),
                             [strongSelf = strongSmallRef(this)](boost::system::error_code ec, std::size_t /*length*/) {
                                 if (ec.failed()) {
                                     strongSelf->close(errorFromBoostError(ec));
                                     return;
                                 }

                                 std::lock_guard<Mutex> guard(strongSelf->_mutex);
                                 strongSelf->_pendingPackets.pop_front();
                                 strongSelf->lockFreeDoSend();
                             });
}

void TCPConnectionImpl::close(const Error& error) {
    Shared<ITCPConnectionDisconnectListener> listener;

    {
        std::lock_guard<Mutex> guard(_mutex);

        if (_closed) {
            return;
        }

        _closed = true;

        boost::system::error_code ec;
        _socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        _socket.close(ec);

        listener = _disconnectListener;
    }

    // Call listener outside the lock to avoid potential deadlocks
    if (listener != nullptr) {
        listener->onDisconnected(strongSmallRef(this), error);
    }
}

void TCPConnectionImpl::setDataListener(Shared<ITCPConnectionDataListener> listener) {
    std::lock_guard<Mutex> guard(_mutex);
    _dataListener = std::move(listener);
}

Shared<ITCPConnectionDataListener> TCPConnectionImpl::getDataListener() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _dataListener;
}

void TCPConnectionImpl::setDisconnectListener(Shared<ITCPConnectionDisconnectListener> listener) {
    std::lock_guard<Mutex> guard(_mutex);
    _disconnectListener = std::move(listener);
}

Shared<ITCPConnectionDisconnectListener> TCPConnectionImpl::getDisconnectListener() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _disconnectListener;
}

std::string TCPConnectionImpl::resolveAddress() {
    boost::system::error_code ec;
    auto endpoint = _socket.remote_endpoint(ec);
    if (ec.failed()) {
        return errorFromBoostError(ec).toString();
    }

    auto address = endpoint.address().to_string(ec);
    if (ec.failed()) {
        return errorFromBoostError(ec).toString();
    }

    return address;
}

const std::string& TCPConnectionImpl::getAddress() const {
    return _address;
}

void TCPConnectionImpl::doRead() {
    auto buffer = Valdi::makeShared<Buffer>();
    _socket.async_read_some(
        boost::asio::buffer(buffer->data(), buffer->size()),
        [buffer, strongSelf = strongSmallRef(this)](boost::system::error_code ec, std::size_t length) {
            if (ec.failed()) {
                strongSelf->close(errorFromBoostError(ec));
            } else {
                strongSelf->doRead();

                auto listener = strongSelf->getDataListener();
                if (listener != nullptr) {
                    listener->onDataReceived(strongSelf, BytesView(buffer, buffer->data(), length));
                }
            }
        });
}

void TCPConnectionImpl::onReady() {
    _address = resolveAddress();
    doRead();
}

} // namespace Valdi
