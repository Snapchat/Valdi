//
//  TCPConnectionImpl.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/20.
//

#include "valdi/runtime/Debugger/BoostAsioUtils.hpp"
#include "valdi/runtime/Debugger/ITCPConnection.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include <array>
#include <deque>

namespace Valdi {

class TCPServerImpl;

class TCPConnectionImpl : public ITCPConnection {
public:
    explicit TCPConnectionImpl(boost::asio::io_service& ioService);
    ~TCPConnectionImpl() override;

    boost::asio::ip::tcp::socket& getSocket();

    void submitData(const BytesView& bytes) override;

    void setDataListener(Shared<ITCPConnectionDataListener> listener) override;
    Shared<ITCPConnectionDataListener> getDataListener() const override;

    void setDisconnectListener(Shared<ITCPConnectionDisconnectListener> listener) override;
    Shared<ITCPConnectionDisconnectListener> getDisconnectListener() const override;

    const std::string& getAddress() const override;

    void close(const Error& error) override;

    void onReady();

private:
    boost::asio::ip::tcp::socket _socket;
    mutable Mutex _mutex;
    Shared<ITCPConnectionDataListener> _dataListener;
    Shared<ITCPConnectionDisconnectListener> _disconnectListener = nullptr;
    std::string _address;
    std::deque<BytesView> _pendingPackets;
    bool _closed = false;

    void doRead();
    void lockFreeDoSend();

    std::string resolveAddress();
};

} // namespace Valdi
