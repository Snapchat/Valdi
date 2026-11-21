//
//  ValdiStandaloneMain.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/15/19.
//

#pragma once

#include "valdi_core/cpp/Context/PlatformType.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include <memory>
#include <vector>

namespace snap::valdi_core {
class HTTPRequestManager;
class ModuleFactoriesProvider;
} // namespace snap::valdi_core

namespace Valdi {
class IJavaScriptBridge;
class RuntimeManager;
class Arguments;
class StringBox;
class ValdiStandaloneRuntime;

struct StandaloneArguments {
    StringBox scriptPath;
    std::vector<StringBox> directorySearchPaths;
    std::vector<StringBox> jsArguments;
    LogType logLevel = LogTypeInfo;
    IJavaScriptBridge* jsBridge = nullptr;
    std::vector<std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider>> moduleFactoriesProviders;
    std::shared_ptr<snap::valdi_core::HTTPRequestManager> requestManager = nullptr;
    bool enableDebuggerService = false;
    bool enableHotReloader = false;
    bool enableTSN = false;
    // The platform the JS runtime is told it runs on. CLI applications built for a terminal pass macOS or Linux.
    PlatformType platformType = PlatformTypeIOS;
};

Ref<ValdiStandaloneRuntime> createValdiStandaloneRuntime(const StandaloneArguments& arguments);

int runValdiStandalone(const StandaloneArguments& arguments);

} // namespace Valdi
