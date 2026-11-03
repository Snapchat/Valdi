#pragma once

#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi_core/cpp/Context/PlatformType.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"

#include <memory>
#include <vector>

namespace snap::valdi_core {
class HTTPRequestManager;
class ModuleFactoriesProvider;
} // namespace snap::valdi_core

namespace Valdi {

class ValdiStandaloneRuntime;

/** What a CLI application's main() wires into the standalone runtime. */
struct CLIRunOptions {
    // Null for an application built without an HTTP backend. Each cli_main template says which one it is.
    std::shared_ptr<snap::valdi_core::HTTPRequestManager> requestManager;
    // Native modules the application links, such as the terminal backend.
    std::vector<std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider>> moduleFactoriesProviders;
    // The platform the JS runtime is told it runs on.
    PlatformType platformType = PlatformTypeIOS;
    // Runs once the runtime exists and the crash handlers are installed, before the script is evaluated.
    Function<void(ValdiStandaloneRuntime&)> beforeRun;
};

int valdiCLIRun(const char* scriptPath, int argc, const char** argv, const CLIRunOptions& options);

// requestManager is null for an application built without an HTTP backend. Not defaulted, so that
// each cli_main template says which one it is.
int valdiCLIRun(const char* scriptPath,
                int argc,
                const char** argv,
                const std::shared_ptr<snap::valdi_core::HTTPRequestManager>& requestManager);

} // namespace Valdi
