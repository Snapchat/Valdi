#include "valdi/terminal/Host/TerminalHost.hpp"

#include "valdi/terminal/Modules/TerminalBackendModule.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace Valdi::Terminal {

namespace {

// Everything the signal handlers touch is a plain static or a lock-free atomic: handlers may
// interrupt any thread, including one that holds a mutex.
struct termios gSavedTermios;
std::atomic<bool> gHasSavedTermios{false};
std::atomic<bool> gRawActive{false};
// Set while a tty root may have left the cursor hidden or colors on.
std::atomic<bool> gOutputArmed{false};
std::atomic<bool> gRestoreWritten{false};
int gWakeFd[2] = {-1, -1};

constexpr char kWakeResize = 'w';
constexpr char kWakeUpdate = 'u';

constexpr char kRestoreSequence[] = "\x1b[0m\x1b[?25h\n";
// The newline only separates the shell prompt from the frame the first time.
constexpr char kRestoreSequenceNoNewline[] = "\x1b[0m\x1b[?25h";

constexpr int kExitCodeInterrupted = 130;
constexpr int kExitGraceMs = 1000;

static_assert(std::atomic<bool>::is_always_lock_free, "signal handlers need lock-free atomics");

void writeFully(int fd, const char* data, size_t size) {
    while (size > 0) {
        const auto written = ::write(fd, data, size);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        data += written;
        size -= static_cast<size_t>(written);
    }
}

// Async-signal-safe: only atomics, tcsetattr and write.
void restoreTerminalSignalSafe() {
    if (gRawActive.exchange(false) && gHasSavedTermios.load()) {
        tcsetattr(STDIN_FILENO, TCSANOW, &gSavedTermios);
    }
    if (gOutputArmed.load()) {
        if (!gRestoreWritten.exchange(true)) {
            writeFully(STDOUT_FILENO, kRestoreSequence, sizeof(kRestoreSequence) - 1);
        } else {
            writeFully(STDOUT_FILENO, kRestoreSequenceNoNewline, sizeof(kRestoreSequenceNoNewline) - 1);
        }
    }
}

void restoreAtExit() {
    restoreTerminalSignalSafe();
}

void wake(char reason) {
    if (gWakeFd[1] >= 0) {
        const int savedErrno = errno;
        // A full pipe already has a wake pending.
        const auto result = ::write(gWakeFd[1], &reason, 1);
        (void)result;
        errno = savedErrno;
    }
}

void resizeSignalHandler(int /*signal*/) {
    wake(kWakeResize);
}

constexpr int kTerminatingSignals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
constexpr int kCrashSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};

struct sigaction gPreviousCrashActions[NSIG];

void terminatingSignalHandler(int signalNumber) {
    restoreTerminalSignalSafe();
    // SA_RESETHAND already reset the disposition and SA_NODEFER left the signal unblocked, so the
    // default action ends the process with the status the shell expects.
    raise(signalNumber);
}

void crashSignalHandler(int signalNumber, siginfo_t* info, void* context) {
    restoreTerminalSignalSafe();
    const struct sigaction& previous = gPreviousCrashActions[signalNumber];
    if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
        previous.sa_sigaction(signalNumber, info, context);
        return;
    }
    if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
        previous.sa_handler(signalNumber);
        return;
    }
    struct sigaction reset;
    memset(&reset, 0, sizeof(reset));
    reset.sa_handler = SIG_DFL;
    sigemptyset(&reset.sa_mask);
    sigaction(signalNumber, &reset, nullptr);
    raise(signalNumber);
}

void ensureWakePipe() {
    if (gWakeFd[0] >= 0) {
        return;
    }
    if (pipe(gWakeFd) != 0) {
        gWakeFd[0] = -1;
        gWakeFd[1] = -1;
        return;
    }
    for (int fd : gWakeFd) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    }
}

bool enableRawModeLocked() {
    if (isatty(STDIN_FILENO) == 0) {
        return false;
    }
    if (!gHasSavedTermios.load()) {
        if (tcgetattr(STDIN_FILENO, &gSavedTermios) != 0) {
            return false;
        }
        gHasSavedTermios = true;
    }
    struct termios raw = gSavedTermios;
    // The subset of Node's tty.setRawMode(): no echo, line buffering, signal keys or input
    // translation. Output post-processing stays on so "\n" still moves to column 0.
    raw.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }
    gRawActive = true;
    return true;
}

void disableRawMode() {
    if (gRawActive.exchange(false) && gHasSavedTermios.load()) {
        tcsetattr(STDIN_FILENO, TCSANOW, &gSavedTermios);
    }
}

} // namespace

TerminalHost::TerminalHost() = default;

TerminalHost& TerminalHost::shared() {
    static TerminalHost* const instance = new TerminalHost();
    return *instance;
}

void TerminalHost::installSignalHandlers() {
    std::lock_guard<Mutex> guard(_mutex);
    if (_signalHandlersInstalled) {
        return;
    }
    _signalHandlersInstalled = true;

    ensureWakePipe();

    std::atexit(restoreAtExit);

    for (int signalNumber : kTerminatingSignals) {
        struct sigaction current;
        if (sigaction(signalNumber, nullptr, &current) == 0 && current.sa_handler == SIG_IGN) {
            // Respect a parent that ignores the signal, like nohup does for SIGHUP.
            continue;
        }
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = terminatingSignalHandler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESETHAND | SA_NODEFER;
        sigaction(signalNumber, &action, nullptr);
    }

    for (int signalNumber : kCrashSignals) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_sigaction = crashSignalHandler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER | SA_RESETHAND;
        if (sigaction(signalNumber, &action, &gPreviousCrashActions[signalNumber]) != 0) {
            memset(&gPreviousCrashActions[signalNumber], 0, sizeof(struct sigaction));
        }
    }

    struct sigaction resize;
    memset(&resize, 0, sizeof(resize));
    resize.sa_handler = resizeSignalHandler;
    sigemptyset(&resize.sa_mask);
    resize.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &resize, nullptr);
}

void TerminalHost::setExitHandler(Function<void(int)> exitHandler) {
    std::lock_guard<Mutex> guard(_mutex);
    _exitHandler = std::move(exitHandler);
}

void TerminalHost::setClient(Weak<TerminalBackendModule> client) {
    std::lock_guard<Mutex> guard(_mutex);
    _client = std::move(client);
}

void TerminalHost::rootAttached() {
    std::lock_guard<Mutex> guard(_mutex);
    _attachedRoots++;
    if (_attachedRoots == 1) {
        gRestoreWritten = false;
        gOutputArmed = isatty(STDOUT_FILENO) != 0;
        queryWindowSize(_lastColumns, _lastRows);
    }
    startReaderIfNeeded();
}

void TerminalHost::rootDetached() {
    std::lock_guard<Mutex> guard(_mutex);
    if (_attachedRoots == 0) {
        return;
    }
    _attachedRoots--;
    if (_attachedRoots == 0) {
        // The roots already showed the cursor when they finished; only the modes are left.
        gOutputArmed = false;
        _rawModeUsers = 0;
        disableRawMode();
        _readerWantsInput = false;
        wake(kWakeUpdate);
    }
}

bool TerminalHost::acquireRawMode() {
    std::lock_guard<Mutex> guard(_mutex);
    if (_rawModeUsers == 0 && !enableRawModeLocked()) {
        return false;
    }
    _rawModeUsers++;
    _readerWantsInput = true;
    startReaderIfNeeded();
    wake(kWakeUpdate);
    return true;
}

void TerminalHost::releaseRawMode() {
    std::lock_guard<Mutex> guard(_mutex);
    if (_rawModeUsers == 0) {
        return;
    }
    _rawModeUsers--;
    if (_rawModeUsers == 0) {
        disableRawMode();
        _readerWantsInput = false;
        wake(kWakeUpdate);
    }
}

void TerminalHost::restoreTerminal() {
    restoreTerminalSignalSafe();
}

bool TerminalHost::isTTY(int fd) {
    return isatty(fd) != 0;
}

bool TerminalHost::queryWindowSize(int& columns, int& rows) {
    const int fds[] = {STDOUT_FILENO, STDERR_FILENO, STDIN_FILENO};
    for (int fd : fds) {
        struct winsize size;
        memset(&size, 0, sizeof(size));
        if (ioctl(fd, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && size.ws_row > 0) {
            columns = size.ws_col;
            rows = size.ws_row;
            return true;
        }
    }
    return false;
}

size_t TerminalHost::completeUtf8Length(const std::string& bytes) {
    const size_t length = bytes.size();
    size_t index = length;
    size_t continuationCount = 0;
    while (index > 0 && continuationCount < 3) {
        const auto byte = static_cast<unsigned char>(bytes[index - 1]);
        if ((byte & 0xC0) != 0x80) {
            size_t expected = 1;
            if ((byte & 0xE0) == 0xC0) {
                expected = 2;
            } else if ((byte & 0xF0) == 0xE0) {
                expected = 3;
            } else if ((byte & 0xF8) == 0xF0) {
                expected = 4;
            }
            const size_t available = length - (index - 1);
            return available < expected ? index - 1 : length;
        }
        continuationCount++;
        index--;
    }
    return length;
}

void TerminalHost::startReaderIfNeeded() {
    ensureWakePipe();
    if (_readerStarted || gWakeFd[0] < 0) {
        return;
    }
    _readerStarted = true;
    // Detached on purpose: the host is never destroyed, and the thread only parks when no root is attached.
    std::thread([this]() { runReader(); }).detach();
}

void TerminalHost::runReader() {
    while (true) {
        struct pollfd fds[2];
        fds[0].fd = gWakeFd[0];
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        nfds_t count = 1;
        if (_readerWantsInput && !_stdinClosed) {
            fds[1].fd = STDIN_FILENO;
            fds[1].events = POLLIN;
            fds[1].revents = 0;
            count = 2;
        }

        const int result = poll(fds, count, -1);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }

        bool resized = false;
        if ((fds[0].revents & POLLIN) != 0) {
            drainWakePipe(resized);
        }
        if (resized) {
            handleResize();
        }
        if (count == 2 && (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            readStdin();
        }
    }
}

void TerminalHost::drainWakePipe(bool& resized) {
    char buffer[16];
    while (true) {
        const auto readCount = read(gWakeFd[0], buffer, sizeof(buffer));
        if (readCount <= 0) {
            return;
        }
        for (ssize_t i = 0; i < readCount; i++) {
            if (buffer[i] == kWakeResize) {
                resized = true;
            }
        }
    }
}

void TerminalHost::readStdin() {
    char buffer[4096];
    const auto readCount = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (readCount > 0) {
        _pendingInput.append(buffer, static_cast<size_t>(readCount));
        const auto completeLength = completeUtf8Length(_pendingInput);
        if (completeLength == 0) {
            return;
        }
        std::string text = _pendingInput.substr(0, completeLength);
        _pendingInput.erase(0, completeLength);
        handleInput(text);
    } else if (readCount == 0 || (errno != EINTR && errno != EAGAIN)) {
        _stdinClosed = true;
    }
}

void TerminalHost::handleInput(const std::string& text) {
    Ref<TerminalBackendModule> client;
    {
        std::lock_guard<Mutex> guard(_mutex);
        client = _client.lock();
    }
    if (client == nullptr) {
        return;
    }
    // Raw mode turned ISIG off, so Ctrl+C arrives as data.
    if (text.find('\x03') != std::string::npos && client->hostExitsOnCtrlC()) {
        requestExit(kExitCodeInterrupted);
        return;
    }
    client->handleHostInput(StringBox::fromString(text));
}

void TerminalHost::handleResize() {
    int columns = 0;
    int rows = 0;
    if (!queryWindowSize(columns, rows)) {
        return;
    }
    Ref<TerminalBackendModule> client;
    {
        std::lock_guard<Mutex> guard(_mutex);
        if (columns == _lastColumns && rows == _lastRows) {
            return;
        }
        _lastColumns = columns;
        _lastRows = rows;
        client = _client.lock();
    }
    if (client != nullptr) {
        client->handleHostResize(columns, rows);
    }
}

void TerminalHost::requestExit(int code) {
    if (_exiting.exchange(true)) {
        return;
    }
    restoreTerminalSignalSafe();

    Function<void(int)> exitHandler;
    {
        std::lock_guard<Mutex> guard(_mutex);
        exitHandler = _exitHandler;
    }
    if (!exitHandler) {
        _exit(code);
    }

    // The main queue may never get to exit when JS is stuck; the user's Ctrl+C must still end the process.
    std::thread([code]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(kExitGraceMs));
        restoreTerminalSignalSafe();
        _exit(code);
    }).detach();
    exitHandler(code);
}

} // namespace Valdi::Terminal
