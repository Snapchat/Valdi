//
//  ValdiLinuxMain.cpp
//  valdi-linux
//

#include "valdi/linux/Bootstrap/ValdiLinuxMain.hpp"
#include "valdi/linux/LinuxRuntime.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace ValdiLinux {

int valdiLinuxMain(const char* rootComponentPath, int argc, const char** argv) {
    auto runtime = createLinuxRuntime(/* enableDebuggerService */ false,
                                      /* disableHotReloader */ true);

    runtime->getResourceLoader().addModuleSearchDirectory(STRING_LITERAL("."));

    std::vector<Valdi::StringBox> jsArguments;
    for (int i = 1; i < argc; ++i) {
        jsArguments.push_back(Valdi::StringBox::fromCString(argv[i]));
    }

    runtime->evalScript(Valdi::StringBox::fromCString(rootComponentPath), jsArguments);

    return runtime->getMainQueue()->runIndefinitely();
}

} // namespace ValdiLinux
