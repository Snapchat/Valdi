#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

namespace snap::valdi::hello_world {

class CppModule : public valdi_core::ModuleFactory {
public:
    CppModule() = default;
    ~CppModule() override = default;

    Valdi::StringBox getModulePath() final {
        return Valdi::StringBox::fromCString("hello_world/src/CppModule");
    }

    Valdi::Value loadModule() final {
        return Valdi::Value().setMapValue("onRootComponentCreated",
                                          Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                                              [](const Valdi::ValueFunctionCallContext& callContext) -> Valdi::Value {
                                                  VALDI_INFO(Valdi::ConsoleLogger::getLogger(),
                                                             "From C++: Root component created with contextId '{}'",
                                                             callContext.getParameter(0).toString(false));
                                                  return Valdi::Value();
                                              })));
    }
};

auto kRegisterModule = Valdi::RegisterModuleFactory::registerTyped<CppModule>();

} // namespace snap::valdi::hello_world