#pragma once

#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/terminal/TerminalViewManager.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

namespace Valdi {

/**
 * Owns the terminal view manager and the ViewManagerContext contexts render with. The context is
 * created on first use so that a runtime that never renders pays nothing.
 */
class Runtime;

class TerminalRuntime : public SharedPtrRefCountable {
public:
    TerminalRuntime(RuntimeManager& runtimeManager, PlatformType platformType = PlatformTypeIOS);
    ~TerminalRuntime() override;

    /**
     * Prepares a JS Runtime that renders into terminal view managers. Views are only inflated
     * inside the viewport by default, which would drop Static content that lives outside the
     * root frame; painting off-screen content costs nothing in a terminal.
     */
    static void configureRuntime(Runtime& runtime);

    TerminalViewManager& getViewManager();

    /**
     * The ViewManagerContext for terminal rendering. Its Yoga config rounds to whole points, and
     * the view manager reports a point scale of 1, so frames land on whole cells. Returns nullptr
     * once the RuntimeManager is gone.
     */
    Ref<ViewManagerContext> getViewManagerContext();

private:
    Weak<RuntimeManager> _runtimeManager;
    TerminalViewManager _viewManager;
    Mutex _mutex;
    Ref<ViewManagerContext> _viewManagerContext;
};

} // namespace Valdi
