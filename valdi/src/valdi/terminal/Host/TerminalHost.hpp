#pragma once

#include "valdi_core/cpp/Context/PlatformType.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <atomic>
#include <string>

namespace Valdi {

class TerminalBackendModule;

namespace Terminal {

/** The platform a CLI application built for this machine runs on, reported to the JS runtime. */
constexpr PlatformType hostPlatformType() {
#if defined(__APPLE__)
    return PlatformTypeMacOS;
#else
    return PlatformTypeLinux;
#endif
}

/**
 * Process-wide ownership of the controlling terminal for `tty` roots: raw mode, the stdin reader,
 * window size changes, Ctrl+C and restoring the terminal however the process ends.
 *
 * Only applications that opt in (`valdi_cli_application(terminal = True)`) hand a host to the
 * terminal module; the module test runners register the module without one and never touch
 * terminal modes or signal handlers. The instance is intentionally never destroyed so its reader
 * thread and the signal handlers can outlive static destruction.
 *
 * Input and resize are delivered to the module (TerminalBackendModule), which forwards them to the
 * observers of every live `tty` root and resizes those roots on the main thread.
 */
class TerminalHost {
public:
    /** The process-wide host. */
    static TerminalHost& shared();

    /**
     * Installs async-signal-safe restore handlers for SIGINT, SIGTERM, SIGHUP, SIGQUIT and SIGPIPE, which restore
     * the terminal and re-raise the signal, and for the crash signals, which restore the terminal and then run the
     * previously installed handler so the stack trace of the crash handler still prints. Also installs the SIGWINCH
     * handler and registers the exit restore.
     * Call it after SignalHandler::install(); calling it again does nothing.
     */
    void installSignalHandlers();

    /**
     * Sets the function that asks the application to exit with a code, normally StandaloneMainQueue::exit.
     * Used for Ctrl+C. The host falls back to `_exit(130)` when the application has not exited about a second
     * later, or at once on a second Ctrl+C, so a hung JS thread cannot trap the user in the terminal.
     */
    void setExitHandler(Function<void(int)> exitHandler);

    /** Registers the module that receives input and resize events. Only one module at a time. */
    void setClient(Weak<TerminalBackendModule> client);

    /** A `tty` root was created. The first one arms the terminal restore and starts the reader thread. */
    void rootAttached();

    /** A `tty` root went away. The last one restores the terminal modes. */
    void rootDetached();

    /**
     * Puts the terminal in raw mode and starts delivering keyboard input. Calls are counted: raw mode
     * ends with the matching last releaseRawMode(). Returns false when stdin is not a terminal.
     */
    bool acquireRawMode();
    void releaseRawMode();

    /** Restores the terminal modes and shows the cursor. Safe to call from any thread. */
    void restoreTerminal();

    static bool isTTY(int fd);

    /** Whether raw mode may be entered: stdin and stdout must both be terminals. */
    static bool rawModeAllowed(bool stdinIsTTY, bool stdoutIsTTY);

    /**
     * Whether the signal path or Ctrl+C already restored the terminal and wrote the trailing newline, in which case
     * a root finishing afterwards must not paint again or repeat it.
     */
    static bool isRestoreWritten();

    /** The size of the controlling terminal, or false when there is none. */
    static bool queryWindowSize(int& columns, int& rows);

    /** The length of the longest prefix of `bytes` made of complete UTF-8 sequences, so a split character is delivered
     * whole. */
    static size_t completeUtf8Length(const std::string& bytes);

private:
    TerminalHost();

    Mutex _mutex;
    Weak<TerminalBackendModule> _client;
    Function<void(int)> _exitHandler;
    int _attachedRoots = 0;
    int _rawModeUsers = 0;
    bool _readerStarted = false;
    bool _signalHandlersInstalled = false;
    std::atomic<bool> _exiting{false};
    std::atomic<bool> _stdinClosed{false};
    std::atomic<bool> _readerWantsInput{false};
    std::string _pendingInput;
    int _lastColumns = 0;
    int _lastRows = 0;

    void startReaderIfNeeded();
    void runReader();
    void drainWakePipe(bool& resized);
    void readStdin();
    void handleInput(const std::string& text);
    void handleResize();
    void requestExit(int code);
};

} // namespace Terminal
} // namespace Valdi
