#pragma once

#include "valdi_core/ModuleFactoriesProvider.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <memory>

namespace Valdi::Terminal {
class TerminalHost;
}

namespace Valdi {

/**
 * Registers the native module behind valdi_tui, `valdi_tui/src/TerminalBackendBridge`. The module
 * creates contexts that render through the terminal view manager and exposes their frames,
 * input and sizing to JS.
 */
class TerminalModuleFactoriesProvider : public SharedPtrRefCountable, public snap::valdi_core::ModuleFactoriesProvider {
public:
    /**
     * @param host Owns the process terminal for `tty` roots (raw mode, input, resize, Ctrl+C). Pass nullptr, as
     * the module test runners do, to leave the terminal alone and keep `tty` roots output-only.
     */
    explicit TerminalModuleFactoriesProvider(Terminal::TerminalHost* host = nullptr);
    ~TerminalModuleFactoriesProvider() override;

    std::vector<std::shared_ptr<snap::valdi_core::ModuleFactory>> createModuleFactories(
        const Value& runtimeManager) override;

private:
    Terminal::TerminalHost* _host;
};

/** Convenience for hosts that register the provider with RuntimeManager::registerModuleFactoriesProvider(). */
std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider> makeTerminalModuleFactoriesProvider(
    Terminal::TerminalHost* host = nullptr);

} // namespace Valdi
