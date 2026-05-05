//
//  ValdiStandaloneMain.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/15/19.
//

#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/standalone_runtime/Arguments.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <iostream>

namespace Valdi {

namespace {
class TestTweakValueProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    Value config;

    StringBox getString(const StringBox& key, const StringBox& fallback) override {
        return config.getMapValue(key).toStringBox();
    }

    bool getBool(const StringBox& key, bool fallback) override {
        return config.getMapValue(key).toBool();
    }

    float getFloat(const StringBox& key, float fallback) override {
        return config.getMapValue(key).toFloat();
    }

    int32_t getInt(const StringBox& key, int32_t fallback) override {
        return config.getMapValue(key).toInt();
    }

    Value getBinary(const StringBox& key, const Value& fallback) override {
        return config.getMapValue(key);
    }
};
} // namespace

Ref<ValdiStandaloneRuntime> createValdiStandaloneRuntime(const StandaloneArguments& arguments) {
    ConsoleLogger::getLogger().setMinLogType(arguments.logLevel);

    auto resourceLoader = Valdi::makeShared<StandaloneResourceLoader>();
    for (const auto& searchPath : arguments.directorySearchPaths) {
        resourceLoader->addModuleSearchDirectory(searchPath);
    }

    auto mainQueue = Valdi::makeShared<StandaloneMainQueue>();
    auto tweakValueProvider = Valdi::makeShared<TestTweakValueProvider>();
    if (arguments.enableTSN) {
        tweakValueProvider->config.setMapValue(StringBox::fromCString("VALDI_ENABLE_TSN"), Value(true));
    }

    auto runtime = ValdiStandaloneRuntime::create(arguments.enableDebuggerService,
                                                  !arguments.enableHotReloader,
                                                  false,
                                                  true,
                                                  false,
                                                  arguments.jsBridge,
                                                  mainQueue,
                                                  Valdi::makeShared<InMemoryDiskCache>(),
                                                  nullptr,
                                                  resourceLoader,
                                                  tweakValueProvider.toShared());

    for (const auto& moduleFactoriesProvider : arguments.moduleFactoriesProviders) {
        runtime->getRuntimeManager().registerModuleFactoriesProvider(moduleFactoriesProvider);
    }

    return runtime;
}

int runValdiStandalone(const StandaloneArguments& arguments) {
    auto runtime = createValdiStandaloneRuntime(arguments);

    runtime->evalScript(arguments.scriptPath, arguments.jsArguments);

    return runtime->getMainQueue()->runIndefinitely();
}

} // namespace Valdi
