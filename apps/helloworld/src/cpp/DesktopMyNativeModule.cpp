#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"

namespace snap::valdi::hello_world {

class DesktopMyNativeModule : public valdi_core::ModuleFactory {
public:
    DesktopMyNativeModule() = default;
    ~DesktopMyNativeModule() override = default;

    Valdi::StringBox getModulePath() final {
        return Valdi::StringBox::fromCString("hello_world/src/NativeModule");
    }

    Valdi::Value loadModule() final {
        return Valdi::Value().setMapValue("APP_NAME", Valdi::Value("Valdi Desktop"));
    }
};

auto kRegisterDesktopModule = Valdi::RegisterModuleFactory::registerTyped<DesktopMyNativeModule>();

} // namespace snap::valdi::hello_world