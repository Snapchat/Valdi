//
//  LinuxRuntime.cpp
//  valdi-linux
//

#include "valdi/linux/LinuxRuntime.hpp"

#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi_core/cpp/Context/PlatformType.hpp"

namespace ValdiLinux {

Valdi::Ref<Valdi::ValdiStandaloneRuntime> createLinuxRuntime(bool enableDebuggerService,
                                                              bool disableHotReloader) {
    auto mainQueue = Valdi::makeShared<Valdi::StandaloneMainQueue>();
    auto diskCache = Valdi::makeShared<Valdi::InMemoryDiskCache>();
    auto resourceLoader = Valdi::makeShared<Valdi::StandaloneResourceLoader>();

    return Valdi::ValdiStandaloneRuntime::create(enableDebuggerService,
                                                 disableHotReloader,
                                                 /* enableViewPreloader */ false,
                                                 /* registerCustomAttributes */ true,
                                                 /* keepAttributesHistory */ false,
                                                 Valdi::JavaScriptBridge::get(),
                                                 mainQueue,
                                                 diskCache,
                                                 /* runtimeListener */ nullptr,
                                                 resourceLoader,
                                                 /* tweakValueProvider */ nullptr,
                                                 Valdi::PlatformTypeLinux);
}

} // namespace ValdiLinux
