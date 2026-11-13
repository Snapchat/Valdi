#include "valdi/cli_runner/CLIRunner.hpp"

#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/standalone_runtime/Arguments.hpp"
#include "valdi/standalone_runtime/SignalHandler.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"

#include "utils/platform/BuildOptions.hpp"

namespace Valdi {

int valdiCLIRun(const char* scriptPath, int argc, const char** argv, const CLIRunOptions& options) {
    SignalHandler::install();

    StandaloneArguments standaloneArguments;
    standaloneArguments.scriptPath = StringBox::fromCString(scriptPath);
    standaloneArguments.enableHotReloader = false;
    standaloneArguments.requestManager = options.requestManager;
    standaloneArguments.moduleFactoriesProviders = options.moduleFactoriesProviders;
    standaloneArguments.platformType = options.platformType;

    if constexpr (snap::kIsDevBuild) {
        standaloneArguments.enableDebuggerService = true;
        standaloneArguments.logLevel = LogTypeDebug;
    } else {
        standaloneArguments.enableDebuggerService = false;
        standaloneArguments.logLevel = LogTypeError;
    }

    Arguments arguments(argc, argv);
    while (arguments.hasNext()) {
        standaloneArguments.jsArguments.emplace_back(arguments.next());
    }
    standaloneArguments.jsBridge = JavaScriptBridge::get(snap::valdi_core::JavaScriptEngineType::Auto);

    auto runtime = createValdiStandaloneRuntime(standaloneArguments);
    if (options.beforeRun) {
        options.beforeRun(*runtime);
    }
    runtime->evalScript(standaloneArguments.scriptPath, standaloneArguments.jsArguments);
    return runtime->getMainQueue()->runIndefinitely();
}

int valdiCLIRun(const char* scriptPath,
                int argc,
                const char** argv,
                const std::shared_ptr<snap::valdi_core::HTTPRequestManager>& requestManager) {
    CLIRunOptions options;
    options.requestManager = requestManager;
    return valdiCLIRun(scriptPath, argc, argv, options);
}

} // namespace Valdi
