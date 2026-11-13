#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/standalone_runtime/Arguments.hpp"
#include "valdi/standalone_runtime/ArgumentsParser.hpp"
#include "valdi/standalone_runtime/SignalHandler.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi/terminal/Modules/TerminalModuleFactoriesProvider.hpp"

#include <iostream>

int main(int argc, const char** argv) {
    Valdi::SignalHandler::install();

    Valdi::ArgumentsParser parser;

    auto engineArgument = parser.addArgument("--js_engine")
                              ->setDescription("The JavaScript engine to use")
                              ->setChoices({"auto", "quickjs", "jscore", "v8", "hermes"});

    auto modulePathArgument = parser.addArgument("--module_path")
                                  ->setDescription("A file or directory path where .valdimodule files can be found")
                                  ->setAllowsMultipleValues();
    auto scriptPathArgument =
        parser.addArgument("--script_path")
            ->setDescription("The path of the script to eval. Should be part of one of the module in the module paths")
            ->setRequired();
    auto logLevelArgument = parser.addArgument("--log_level")
                                ->setDescription("Minimum log level")
                                ->setChoices({"debug",
                                              "info",
                                              "warn"
                                              "error"});
    auto hotReloadArgument = parser.addArgument("--hot_reload")
                                 ->setDescription("Whether to enable the hot reloader. When enabled, the runtime will "
                                                  "first wait for the hotreloader files before starting the tests")
                                 ->setAsFlag();
    auto debuggerServiceArgument =
        parser.addArgument("--debugger_service")->setDescription("Whether to enable the debugger service")->setAsFlag();

    auto remainderArgument = parser.addArgument("--")
                                 ->setDescription("Delimiter for arguments which will be passed to the JS context")
                                 ->setAsRemainder();

    Valdi::Ref<Valdi::Argument> snapTokenArgument;

    Valdi::Arguments arguments(argc, argv);
    auto executablePath = arguments.next();

    auto parseResult = parser.parse(arguments);
    if (!parseResult) {
        std::cerr << parseResult.error().toString() << std::endl;
        std::cerr << std::endl << parser.getUsageString("Valdi Standalone");
        return EXIT_FAILURE;
    }

    Valdi::StandaloneArguments standaloneArguments;
    standaloneArguments.scriptPath = scriptPathArgument->value();
    standaloneArguments.directorySearchPaths = modulePathArgument->values();
    standaloneArguments.enableHotReloader = hotReloadArgument->hasValue();
    standaloneArguments.enableDebuggerService =
        debuggerServiceArgument->hasValue() || standaloneArguments.enableHotReloader;
    standaloneArguments.jsArguments.emplace_back(executablePath);
    standaloneArguments.jsArguments.insert(
        standaloneArguments.jsArguments.end(), remainderArgument->values().begin(), remainderArgument->values().end());

    auto engineType = snap::valdi_core::JavaScriptEngineType::QuickJS;
    if (engineArgument->hasValue()) {
        if (engineArgument->value() == "auto") {
            engineType = snap::valdi_core::JavaScriptEngineType::Auto;
        } else if (engineArgument->value() == "quickjs") {
            engineType = snap::valdi_core::JavaScriptEngineType::QuickJS;
        } else if (engineArgument->value() == "jscore") {
            engineType = snap::valdi_core::JavaScriptEngineType::JSCore;
        } else if (engineArgument->value() == "v8") {
            engineType = snap::valdi_core::JavaScriptEngineType::V8;
        } else if (engineArgument->value() == "hermes") {
            engineType = snap::valdi_core::JavaScriptEngineType::Hermes;
        } else {
            std::abort();
        }
    }
    standaloneArguments.jsBridge = Valdi::JavaScriptBridge::get(engineType);
    // Lets valdi_tui specs render through the terminal backend with an in-memory sink.
    standaloneArguments.moduleFactoriesProviders.emplace_back(Valdi::makeTerminalModuleFactoriesProvider());

    return Valdi::runValdiStandalone(standaloneArguments);
}
