#include "valdi_modules/integration_test_app/integration_test_app.hpp"

#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <string>

namespace snap::valdi_modules::integration_test_app {

class IntegrationTestHostModuleImpl : public IntegrationTestHostModule {
public:
    Valdi::StringBox getPlatform() final {
        return Valdi::StringBox::fromCString("macos");
    }

    Valdi::StringBox getOutputPath() final {
        return Valdi::StringBox::fromCString("/tmp/valdi-integration-test/results.json");
    }

    void markFinished(Valdi::StringBox /*path*/) final {}

    void writeTextFile(Valdi::StringBox path, Valdi::StringBox contents) final {
        Valdi::Path filePath(path.toStringView());
        if (filePath.getComponents().size() > 1) {
            auto directory = filePath.removingLastComponent();
            if (!Valdi::DiskUtils::isDirectory(directory)) {
                Valdi::DiskUtils::makeDirectory(directory, true);
            }
        }
        Valdi::DiskUtils::store(filePath, contents.toStringView());
    }

    Valdi::StringBox submitTouchSequence(Valdi::Value /*node*/, Valdi::StringBox /*sequenceJson*/) final {
        return Valdi::StringBox::fromCString("macos C++ host does not synthesize SnapDrawing touch input yet");
    }

    Valdi::StringBox focusTextInput(Valdi::Value /*node*/) final {
        return Valdi::StringBox::fromCString("macos C++ host text focus not implemented");
    }

    Valdi::StringBox replaceText(Valdi::Value /*node*/, Valdi::StringBox value) final {
        return Valdi::StringBox::fromString("macos C++ host accepted text length=" + std::to_string(value.length()));
    }

    Valdi::StringBox pressReturn(Valdi::Value /*node*/) final {
        return Valdi::StringBox::fromCString("macos C++ host return key not implemented");
    }

    Valdi::StringBox pressBackspace(Valdi::Value /*node*/) final {
        return Valdi::StringBox::fromCString("macos C++ host backspace key not implemented");
    }
};

class IntegrationTestHostModuleFactoryImpl : public IntegrationTestHostModuleFactory {
public:
    Valdi::Ref<IntegrationTestHostModule> onLoadModule() final {
        return Valdi::makeShared<IntegrationTestHostModuleImpl>();
    }
};

auto registerIntegrationTestHostModule = Valdi::RegisterModuleFactory::registerTyped<IntegrationTestHostModuleFactoryImpl>();

} // namespace snap::valdi_modules::integration_test_app
