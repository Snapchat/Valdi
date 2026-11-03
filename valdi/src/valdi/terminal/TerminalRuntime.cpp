#include "valdi/terminal/TerminalRuntime.hpp"

#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/Runtime.hpp"

namespace Valdi {

TerminalRuntime::TerminalRuntime(RuntimeManager& runtimeManager, PlatformType platformType)
    : _runtimeManager(weakRef(&runtimeManager)), _viewManager(platformType) {}

TerminalRuntime::~TerminalRuntime() = default;

void TerminalRuntime::configureRuntime(Runtime& runtime) {
    runtime.setLimitToViewportDisabled(true);
}

TerminalViewManager& TerminalRuntime::getViewManager() {
    return _viewManager;
}

Ref<ViewManagerContext> TerminalRuntime::getViewManagerContext() {
    std::lock_guard<Mutex> guard(_mutex);
    if (_viewManagerContext == nullptr) {
        auto runtimeManager = _runtimeManager.lock();
        if (runtimeManager == nullptr) {
            return nullptr;
        }
        _viewManagerContext = runtimeManager->createViewManagerContext(_viewManager,
                                                                       /* enablePreloading */ false,
                                                                       /* mainThreadManagerOverride */ nullptr,
                                                                       Yoga::createConfig(1.0f));
    }
    return _viewManagerContext;
}

} // namespace Valdi
