//
//  LinuxRuntime.hpp
//  valdi-linux
//

#pragma once

#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace ValdiLinux {

// Creates and returns a ValdiStandaloneRuntime configured for Linux
// (PlatformTypeLinux, SnapDrawing rendering backend).
Valdi::Ref<Valdi::ValdiStandaloneRuntime> createLinuxRuntime(bool enableDebuggerService,
                                                              bool disableHotReloader);

} // namespace ValdiLinux
