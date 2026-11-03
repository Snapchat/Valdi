#pragma once

#include "valdi_core/ModuleFactoriesProvider.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <memory>

namespace Valdi {

/**
 * Registers the native module behind valdi_tui, `valdi_tui/src/TerminalBackendBridge`. The module
 * creates contexts that render through the terminal view manager and exposes their frames,
 * input and sizing to JS.
 */
class TerminalModuleFactoriesProvider : public SharedPtrRefCountable, public snap::valdi_core::ModuleFactoriesProvider {
public:
    TerminalModuleFactoriesProvider();
    ~TerminalModuleFactoriesProvider() override;

    std::vector<std::shared_ptr<snap::valdi_core::ModuleFactory>> createModuleFactories(
        const Value& runtimeManager) override;
};

/** Convenience for hosts that register the provider with RuntimeManager::registerModuleFactoriesProvider(). */
std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider> makeTerminalModuleFactoriesProvider();

} // namespace Valdi
