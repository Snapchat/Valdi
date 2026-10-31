//
//  TerminalModule.hpp
//  valdi-standalone_runtime
//
//  Native bridge for terminal I/O used by CLI applications (see valdi_tui).
//  Exposed to JS as the `valdi_standalone/src/TerminalBridge` module.
//

#pragma once

#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include <atomic>
#include <map>
#include <string>
#include <thread>

namespace Valdi {

/**
 * Gives JS access to the controlling terminal: tty detection, window size,
 * raw-mode keyboard input, resize notifications and unbuffered writes.
 *
 * Input and resize observers are invoked from a background reader thread with
 * ValueFunctionFlagsNeverCallSync, so they are delivered asynchronously on the JS queue.
 * The reader thread is started lazily by the first observer and stopped by shutdown(),
 * which the standalone runtime calls before tearing down the JS runtime.
 */
class TerminalModule : public Valdi::SharedPtrRefCountable, public snap::valdi_core::ModuleFactory {
public:
    TerminalModule();
    ~TerminalModule() override;

    Valdi::StringBox getModulePath() override;
    Valdi::Value loadModule() override;

    /**
     * Stops the reader thread, restores the terminal attributes and drops all observers.
     * Safe to call multiple times. Must be called before the JS runtime is destroyed.
     */
    void shutdown();

    Valdi::Value isTTY(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value getSize(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value setRawMode(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value write(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value observeInput(const Valdi::ValueFunctionCallContext& callContext);
    Valdi::Value observeResize(const Valdi::ValueFunctionCallContext& callContext);

private:
    using Observers = std::map<uint64_t, Valdi::Ref<Valdi::ValueFunction>>;

    Valdi::Mutex _mutex;
    Observers _inputObservers;
    Observers _resizeObservers;
    uint64_t _nextObserverId = 0;

    std::thread _thread;
    bool _threadStarted = false;
    std::atomic_bool _stopRequested = {false};
    int _wakePipe[2] = {-1, -1};
    bool _stdinClosed = false;
    std::string _pendingInput;
    int _lastColumns = -1;
    int _lastRows = -1;

    Valdi::Value addObserver(const Valdi::ValueFunctionCallContext& callContext, Observers TerminalModule::* observers);
    void ensureThreadStarted();
    void stopThread();
    void runLoop();
    void readStdin();
    void dispatchInput();
    void checkResize();
};

} // namespace Valdi
