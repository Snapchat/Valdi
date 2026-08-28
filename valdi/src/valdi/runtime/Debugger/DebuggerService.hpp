//
//  DebuggerService.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/27/19.
//

#pragma once

#include "valdi/runtime/Debugger/DaemonClient.hpp"
#include "valdi/runtime/Debugger/ITCPServer.hpp"
#include "valdi/runtime/Resources/Resource.hpp"
#include "valdi/runtime/Utils/BridgeLogger.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include "valdi_core/Platform.hpp"

#include "utils/platform/BuildOptions.hpp"

#include <optional>
#include <vector>

namespace snap::valdi {
class RuntimeMessageHandler;
}

namespace Valdi {

constexpr bool kDebuggerServiceEnabled = (snap::kIsGoldBuild || snap::kIsDevBuild);

class ILogger;
class DaemonClientListener;
class DaemonClient;
class DebuggerService;
class Runtime;

class IDebuggerServiceListener;

enum class DebuggerPortEnvironmentError {
    Malformed,
    OutOfRange,
};

struct DebuggerPortResolution {
    uint32_t port;
    std::optional<uint32_t> rejectedRequestedPort;
    std::optional<DebuggerPortEnvironmentError> environmentError;
};

class DebuggerService : public SharedPtrRefCountable,
                        protected ITCPServerListener,
                        public DaemonClientListener,
                        public BridgeLoggerListener {
public:
    DebuggerService(const Shared<snap::valdi::RuntimeMessageHandler>& runtimeMessageHandler,
                    snap::valdi_core::Platform platformType,
                    uint32_t debuggerPort,
                    bool disableHotReloader,
                    const Ref<ILogger>& logger);

    ~DebuggerService() override;

    void postInit();

    void start();
    void stop();

    Ref<ILogger> getBridgeLogger() const;

    void addListener(IDebuggerServiceListener* listener);
    void removeListener(IDebuggerServiceListener* listener);

    // DaemonClientListener
    void didReceiveUpdatedResources(const SharedVector<Shared<Resource>>& resources) override;
    void didReceiveClientPayload(DaemonClient* daemonClient, int senderClientId, const BytesView& payload) override;
    void daemonClientDidDisconnect(DaemonClient* daemonClient, const Error& error) override;

    StringBox getApplicationId() const;
    void setApplicationId(const StringBox& applicationId);

    uint32_t getConfiguredPort() const;
    uint16_t getBoundPort();

    static uint32_t resolveDebuggerPort(bool isStandalone);
    static DebuggerPortResolution resolveDebuggerPortWithDiagnostics(bool isStandalone,
                                                                     std::optional<uint32_t> requestedPort);

protected:
    // TCPServerListener
    void onClientConnected(const Ref<ITCPConnection>& client) override;
    void onClientDisconnected(const Ref<ITCPConnection>& client, const Error& error) override;

    // BridgeLoggerListener
    void willLog(LogType type, const std::string& message) override;

private:
    Shared<snap::valdi::RuntimeMessageHandler> _runtimeMessageHandler;
    snap::valdi_core::Platform _platformType;
    Ref<ILogger> _logger;
    Ref<BridgeLogger> _bridgeLogger;

    Ref<ITCPServer> _tcpServer;

    std::vector<Ref<DaemonClient>> _clients;
    std::vector<Weak<IDebuggerServiceListener>> _listeners;
    FlatMap<ResourceId, Shared<Resource>> _cachedReloadedResources;

    mutable Mutex _mutex;
    Ref<DispatchQueue> _dispatchQueue;
    uint32_t _debuggerPort;
    StringBox _applicationId;
    int _connectionIdSequence = 0;

    bool _started = false;
    bool _disableHotReloader = false;
    void doStart(int attemptNumber);

    Ref<DaemonClient> addDaemonClient(const Ref<ITCPConnection>& tcpClient);

    void submitDaemonClientConfiguration(const Ref<DaemonClient>& daemonClient, const StringBox& applicationId);
};

} // namespace Valdi
