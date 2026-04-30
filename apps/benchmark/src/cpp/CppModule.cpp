#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <ctime>
#include <iostream>
#include <string>

namespace snap::valdi::benchmark {

#ifdef __APPLE__
#define BENCHMARK_CLOCK CLOCK_UPTIME_RAW
#else
#define BENCHMARK_CLOCK CLOCK_MONOTONIC
#endif

class CppHelperModule : public valdi_core::ModuleFactory {
public:
    Valdi::StringBox getModulePath() final {
        return Valdi::StringBox::fromCString("benchmark/src/cpp");
    }

    Valdi::Value loadModule() final {
        return Valdi::Value()
            .setMapValue("now",
                         Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                             [](const Valdi::ValueFunctionCallContext& callContext) -> Valdi::Value {
                                 struct timespec ts;
                                 clock_gettime(BENCHMARK_CLOCK, &ts);
                                 uint64_t nano_since_epoch = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
                                 return Valdi::Value(nano_since_epoch / 1000000.0);
                             })))
            .setMapValue("question",
                         Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                             [](const Valdi::ValueFunctionCallContext& callContext) -> Valdi::Value {
                                 auto q = callContext.getParameter(0).toString(false);
                                 std::cout << q;
                                 std::string line;
                                 std::getline(std::cin, line);
                                 return Valdi::Value(line);
                             })))
            .setMapValue("print",
                         Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                             [](const Valdi::ValueFunctionCallContext& callContext) -> Valdi::Value {
                                 auto s = callContext.getParameter(0).toString(false);
                                 std::cout << s << std::endl;
                                 return Valdi::Value();
                             })));
    }
};

auto kRegisterCppHelperModule = Valdi::RegisterModuleFactory::registerTyped<CppHelperModule>();

} // namespace snap::valdi::benchmark
