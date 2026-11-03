#pragma once

#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/terminal/Host/TerminalHost.hpp"
#include "valdi/terminal/TerminalRuntime.hpp"
#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <vector>

namespace Valdi {

class TerminalRootHandle;

/**
 * The native module `valdi_tui/src/TerminalBackendBridge`. It creates contexts backed by the
 * terminal view manager, hands their frames to JS and carries keyboard input and resize
 * notifications from the host to JS.
 *
 * Input has no reader of its own: hosts (or tests, through feedInput) push text in and the
 * observers registered with observeInput() receive it. Observers belong to one root, so several
 * renders in one process never see each other's input. dispatchInput() and dispatchResize() reach
 * every live root. A module created with a TerminalHost also receives the process terminal's
 * keyboard input, resize events and Ctrl+C for its `tty` roots, and asks the host to put the
 * terminal in raw mode on their behalf; without a host `tty` roots only draw.
 */
class TerminalBackendModule final : public SharedPtrRefCountable, public snap::valdi_core::ModuleFactory {
public:
    explicit TerminalBackendModule(Ref<TerminalRuntime> terminalRuntime, Terminal::TerminalHost* host = nullptr);
    ~TerminalBackendModule() final;

    StringBox getModulePath() final;
    Value loadModule() final;

    /** Delivers raw terminal input to the observers of every live root. */
    void dispatchInput(const StringBox& text);

    /** Reports a host terminal size change to the observers of every live root. */
    void dispatchResize(int columns, int rows);

    /** Delivers input read from the process terminal to the observers of every live `tty` root. */
    void handleHostInput(const StringBox& text);

    /** Lays the live `tty` roots out for the new terminal size, then notifies their resize observers. */
    void handleHostResize(int columns, int rows);

    /** Whether a live `tty` root asked for Ctrl+C to exit the application (the default). */
    bool hostExitsOnCtrlC();

private:
    Ref<TerminalRuntime> _terminalRuntime;
    Terminal::TerminalHost* _host;
    Mutex _mutex;
    std::vector<Weak<TerminalRootHandle>> _handles;

    Value createRoot(const ValueFunctionCallContext& callContext);
    Value destroyRoot(const ValueFunctionCallContext& callContext);
    Value finishRoot(const ValueFunctionCallContext& callContext);
    Value observeFrame(const ValueFunctionCallContext& callContext);
    Value setSize(const ValueFunctionCallContext& callContext);
    Value feedInput(const ValueFunctionCallContext& callContext);
    Value writeAbove(const ValueFunctionCallContext& callContext);
    Value clearRoot(const ValueFunctionCallContext& callContext);
    Value getOutput(const ValueFunctionCallContext& callContext);
    Value getLastFrame(const ValueFunctionCallContext& callContext);
    Value getSize(const ValueFunctionCallContext& callContext);
    Value isTTY(const ValueFunctionCallContext& callContext);
    Value observeInput(const ValueFunctionCallContext& callContext);
    Value observeResize(const ValueFunctionCallContext& callContext);
    Value setRawMode(const ValueFunctionCallContext& callContext);

    Value addObserver(const ValueFunctionCallContext& callContext, bool isInput);
    void registerHandle(const Ref<TerminalRootHandle>& handle);
    std::vector<Ref<TerminalRootHandle>> liveHandles();
    static void dispatchInput(TerminalRootHandle& handle, const StringBox& text);
    static void dispatchResize(TerminalRootHandle& handle, int columns, int rows);
};

} // namespace Valdi
