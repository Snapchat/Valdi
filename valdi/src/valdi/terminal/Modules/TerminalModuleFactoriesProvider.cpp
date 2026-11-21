#include "valdi/terminal/Modules/TerminalModuleFactoriesProvider.hpp"

#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/terminal/Host/TerminalHost.hpp"
#include "valdi/terminal/Modules/TerminalBackendModule.hpp"
#include "valdi/terminal/TerminalRuntime.hpp"

namespace Valdi {

TerminalModuleFactoriesProvider::TerminalModuleFactoriesProvider(Terminal::TerminalHost* host) : _host(host) {}

TerminalModuleFactoriesProvider::~TerminalModuleFactoriesProvider() = default;

std::vector<std::shared_ptr<snap::valdi_core::ModuleFactory>> TerminalModuleFactoriesProvider::createModuleFactories(
    const Value& runtimeManagerValue) {
    auto runtimeManager = runtimeManagerValue.getTypedRef<RuntimeManager>();
    if (runtimeManager == nullptr) {
        return {};
    }

    // The view manager must report the platform the JS runtime was told it runs on, otherwise
    // the class names JSX resolves (SCValdiView vs com.snap.valdi.views.ValdiView) will not match.
    auto terminalRuntime = makeShared<TerminalRuntime>(*runtimeManager, runtimeManager->getPlatformType());
    auto module = makeShared<TerminalBackendModule>(terminalRuntime, _host);
    if (_host != nullptr) {
        _host->setClient(weakRef(module.get()));
    }
    return {module.toShared()};
}

std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider> makeTerminalModuleFactoriesProvider(
    Terminal::TerminalHost* host) {
    return makeShared<TerminalModuleFactoriesProvider>(host).toShared();
}

} // namespace Valdi
