#include "utils/time/StopWatch.hpp"
#include "valdi/RuntimeMessageHandler.hpp"
#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/standalone_runtime/Arguments.hpp"
#include "valdi/standalone_runtime/ArgumentsParser.hpp"
#include "valdi/standalone_runtime/SignalHandler.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <cstdlib>
#include <iostream>

namespace Valdi {

struct BenchmarkRuntimeMessageHandler : public snap::valdi::RuntimeMessageHandler {
    BenchmarkRuntimeMessageHandler() = default;

    bool hasError() const {
        return _hasError.load();
    }

    void onUncaughtJsError(int32_t errorCode,
                           const Valdi::StringBox& moduleName,
                           const std::string& errorMessage,
                           const std::string& stackTrace) override {
        _hasError = true;
    }

    void onJsCrash(const Valdi::StringBox& moduleName,
                   const std::string& errorMessage,
                   const std::string& stackTrace,
                   bool isANR) override {
        _hasError = true;
    }

    void onDebugMessage(int32_t level, const std::string& message) override {}

private:
    std::atomic_bool _hasError = false;
};

int createRuntimeAndRenderComponent(const StandaloneArguments& standaloneArguments,
                                    const Function<void()>& onComplete) {
    auto standaloneRuntime = Valdi::createValdiStandaloneRuntime(standaloneArguments);
    auto benchmarkRuntimeMessageHandler = makeShared<BenchmarkRuntimeMessageHandler>();
    standaloneRuntime->getRuntime().setRuntimeMessageHandler(benchmarkRuntimeMessageHandler);

    auto tree = standaloneRuntime->getRuntime().createViewNodeTreeAndContext(
        standaloneRuntime->getViewManagerContext(),
        StringBox::fromCString("StartupBenchmark@startup_benchmark/src/StartupBenchmark"));

    tree->setRootViewWithDefaultViewClass();

    tree->getContext()->waitUntilAllUpdatesCompleted(
        makeShared<ValueFunctionWithCallable>([&](const auto& /*callContext*/) -> Value {
            onComplete();
            standaloneRuntime->getMainQueue()->exit(0);
            return Value();
        }));

    auto exitCode = standaloneRuntime->getMainQueue()->runIndefinitely();
    if (exitCode == 0 && benchmarkRuntimeMessageHandler->hasError()) {
        exitCode = 1;
    }
    return exitCode;
}

} // namespace Valdi

int main(int argc, const char** argv) {
    Valdi::SignalHandler::install();

    Valdi::ArgumentsParser parser;

    auto engineArgument = parser.addArgument("--js_engine")
                              ->setDescription("The JavaScript engine to use")
                              ->setChoices({"auto", "quickjs", "jscore", "v8", "hermes", "quickjs_tsn"});

    Valdi::Arguments arguments(argc, argv);
    arguments.next(); // skip the executable path

    auto parseResult = parser.parse(arguments);
    if (!parseResult) {
        std::cerr << parseResult.error().toString() << std::endl;
        std::cerr << std::endl << parser.getUsageString("Valdi Standalone");
        return EXIT_FAILURE;
    }

    Valdi::StandaloneArguments standaloneArguments;
    standaloneArguments.logLevel = Valdi::LogTypeDebug;
    standaloneArguments.enableHotReloader = false;
    standaloneArguments.enableDebuggerService = false;
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
        } else if (engineArgument->value() == "quickjs_tsn") {
            engineType = snap::valdi_core::JavaScriptEngineType::QuickJS;
            standaloneArguments.enableTSN = true;
        } else {
            std::abort();
        }
    }
    standaloneArguments.jsBridge = Valdi::JavaScriptBridge::get(engineType);

    snap::utils::time::StopWatch sw;
    sw.start();

    auto exitCode = createRuntimeAndRenderComponent(standaloneArguments, [&]() { sw.stop(); });

    if (exitCode != 0) {
        std::cerr << "Startup benchmark failed" << std::endl;
        return EXIT_FAILURE;
    }

    auto time = sw.elapsed();
    std::cout << "Startup benchmark took " << time.toString() << std::endl;

    return EXIT_SUCCESS;
}
