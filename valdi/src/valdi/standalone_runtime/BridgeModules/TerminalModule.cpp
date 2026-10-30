//
//  TerminalModule.cpp
//  valdi-standalone_runtime
//

#include "valdi/standalone_runtime/BridgeModules/TerminalModule.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#if !defined(_WIN32)
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace Valdi {

namespace {

#if !defined(_WIN32)

// The terminal must be restored even when the process exits through process.exit() or an
// uncaught error, so the saved attributes live outside the module instance.
Valdi::Mutex gTerminalStateMutex;
bool gHasSavedTermios = false;
bool gRawModeEnabled = false;
bool gAtExitRegistered = false;
struct termios gSavedTermios;

void restoreTerminalLocked() {
    if (gRawModeEnabled && gHasSavedTermios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &gSavedTermios);
    }
    gRawModeEnabled = false;
}

void restoreTerminalAtExit() {
    std::lock_guard<Valdi::Mutex> guard(gTerminalStateMutex);
    restoreTerminalLocked();
}

bool enableRawMode() {
    std::lock_guard<Valdi::Mutex> guard(gTerminalStateMutex);
    if (isatty(STDIN_FILENO) == 0) {
        return false;
    }
    if (!gHasSavedTermios) {
        if (tcgetattr(STDIN_FILENO, &gSavedTermios) != 0) {
            return false;
        }
        gHasSavedTermios = true;
    }
    if (!gAtExitRegistered) {
        gAtExitRegistered = true;
        std::atexit(restoreTerminalAtExit);
    }

    struct termios raw = gSavedTermios;
    // Same subset as Node's tty.setRawMode(): no echo, no line buffering, no signal keys and
    // no input translation, but output post-processing stays on so "\n" still moves to column 0.
    raw.c_iflag &= ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }
    gRawModeEnabled = true;
    return true;
}

void disableRawMode() {
    std::lock_guard<Valdi::Mutex> guard(gTerminalStateMutex);
    restoreTerminalLocked();
}

bool queryWindowSize(int& columns, int& rows) {
    const int fds[] = {STDOUT_FILENO, STDERR_FILENO, STDIN_FILENO};
    for (int fd : fds) {
        struct winsize size{};
        if (ioctl(fd, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && size.ws_row > 0) {
            columns = size.ws_col;
            rows = size.ws_row;
            return true;
        }
    }
    return false;
}

void writeAll(int fd, std::string_view data) {
    while (!data.empty()) {
        auto written = ::write(fd, data.data(), data.size());
        if (written < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return;
        }
        data.remove_prefix(static_cast<size_t>(written));
    }
}

// Returns the length of the longest prefix made of complete UTF-8 sequences, so a multi-byte
// character split across two read() calls is delivered whole.
size_t completeUtf8Length(const std::string& bytes) {
    size_t length = bytes.size();
    size_t i = length;
    size_t continuationCount = 0;
    while (i > 0 && continuationCount < 3) {
        unsigned char c = static_cast<unsigned char>(bytes[i - 1]);
        if ((c & 0xC0) != 0x80) {
            size_t expected = 1;
            if ((c & 0xE0) == 0xC0) {
                expected = 2;
            } else if ((c & 0xF0) == 0xE0) {
                expected = 3;
            } else if ((c & 0xF8) == 0xF0) {
                expected = 4;
            }
            size_t available = length - (i - 1);
            return available < expected ? i - 1 : length;
        }
        continuationCount++;
        i--;
    }
    return length;
}

#endif

} // namespace

#define BIND_METHOD(module, __method__)                                                                                \
    module.setMapValue(                                                                                                \
        STRINGIFY(__method__),                                                                                         \
        Valdi::Value(Valdi::ValueFunctionWithCallable::makeFromMethod(this, &TerminalModule::__method__)))

TerminalModule::TerminalModule() = default;

TerminalModule::~TerminalModule() {
    shutdown();
}

Valdi::StringBox TerminalModule::getModulePath() {
    return STRING_LITERAL("valdi_standalone/src/TerminalBridge");
}

Valdi::Value TerminalModule::loadModule() {
    Valdi::Value module;
    BIND_METHOD(module, isTTY);
    BIND_METHOD(module, getSize);
    BIND_METHOD(module, setRawMode);
    BIND_METHOD(module, write);
    BIND_METHOD(module, observeInput);
    BIND_METHOD(module, observeResize);
    return module;
}

void TerminalModule::shutdown() {
    stopThread();
#if !defined(_WIN32)
    disableRawMode();
#endif
    std::lock_guard<Valdi::Mutex> guard(_mutex);
    _inputObservers.clear();
    _resizeObservers.clear();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Valdi::Value TerminalModule::isTTY(const Valdi::ValueFunctionCallContext& callContext) {
#if defined(_WIN32)
    return Valdi::Value(false);
#else
    auto fd = callContext.getParametersSize() > 0 ? callContext.getParameter(0).toInt() : STDOUT_FILENO;
    return Valdi::Value(isatty(fd) != 0);
#endif
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Valdi::Value TerminalModule::getSize(const Valdi::ValueFunctionCallContext& /*callContext*/) {
#if defined(_WIN32)
    return Valdi::Value();
#else
    int columns = 0;
    int rows = 0;
    if (!queryWindowSize(columns, rows)) {
        return Valdi::Value();
    }
    auto out = Valdi::ValueArray::make(2);
    out->emplace(0, Valdi::Value(static_cast<int32_t>(columns)));
    out->emplace(1, Valdi::Value(static_cast<int32_t>(rows)));
    return Valdi::Value(out);
#endif
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Valdi::Value TerminalModule::setRawMode(const Valdi::ValueFunctionCallContext& callContext) {
#if defined(_WIN32)
    return Valdi::Value(false);
#else
    auto enabled = callContext.getParametersSize() > 0 && callContext.getParameter(0).toBool();
    if (enabled) {
        return Valdi::Value(enableRawMode());
    }
    disableRawMode();
    return Valdi::Value(true);
#endif
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Valdi::Value TerminalModule::write(const Valdi::ValueFunctionCallContext& callContext) {
#if !defined(_WIN32)
    if (callContext.getParametersSize() < 2) {
        return Valdi::Value();
    }
    auto fd = callContext.getParameter(0).toInt();
    auto text = callContext.getParameter(1).toStringBox();
    writeAll(fd, text.toStringView());
#endif
    return Valdi::Value();
}

Valdi::Value TerminalModule::observeInput(const Valdi::ValueFunctionCallContext& callContext) {
    return addObserver(callContext, &TerminalModule::_inputObservers);
}

Valdi::Value TerminalModule::observeResize(const Valdi::ValueFunctionCallContext& callContext) {
    return addObserver(callContext, &TerminalModule::_resizeObservers);
}

Valdi::Value TerminalModule::addObserver(const Valdi::ValueFunctionCallContext& callContext,
                                         Observers TerminalModule::* observers) {
    auto observer = callContext.getParameterAsFunction(0);
    if (!callContext.getExceptionTracker()) {
        return Valdi::Value();
    }

    uint64_t id = 0;
    {
        std::lock_guard<Valdi::Mutex> guard(_mutex);
        id = ++_nextObserverId;
        (this->*observers).emplace(id, observer);
    }
    ensureThreadStarted();

    Valdi::Value out;
    out.setMapValue(
        "cancel",
        Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
            [weakSelf = weakRef(this), id, observers](const Valdi::ValueFunctionCallContext&) -> Valdi::Value {
                auto self = weakSelf.lock();
                if (self != nullptr) {
                    std::lock_guard<Valdi::Mutex> guard(self->_mutex);
                    ((*self).*observers).erase(id);
                }
                return Valdi::Value();
            })));
    return out;
}

void TerminalModule::ensureThreadStarted() {
#if !defined(_WIN32)
    std::lock_guard<Valdi::Mutex> guard(_mutex);
    if (_threadStarted) {
        return;
    }
    if (pipe(_wakePipe) != 0) {
        return;
    }
    _stopRequested = false;
    _threadStarted = true;
    _thread = std::thread([this]() { runLoop(); });
#endif
}

void TerminalModule::stopThread() {
#if !defined(_WIN32)
    {
        std::lock_guard<Valdi::Mutex> guard(_mutex);
        if (!_threadStarted) {
            return;
        }
        _stopRequested = true;
        const char wake = 1;
        writeAll(_wakePipe[1], std::string_view(&wake, 1));
    }
    if (_thread.joinable()) {
        _thread.join();
    }
    std::lock_guard<Valdi::Mutex> guard(_mutex);
    close(_wakePipe[0]);
    close(_wakePipe[1]);
    _wakePipe[0] = -1;
    _wakePipe[1] = -1;
    _threadStarted = false;
#endif
}

void TerminalModule::runLoop() {
#if !defined(_WIN32)
    while (!_stopRequested) {
        bool pollStdin = false;
        {
            std::lock_guard<Valdi::Mutex> guard(_mutex);
            pollStdin = !_inputObservers.empty() && !_stdinClosed;
        }

        struct pollfd fds[2];
        fds[0].fd = _wakePipe[0];
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        nfds_t count = 1;
        if (pollStdin) {
            fds[1].fd = STDIN_FILENO;
            fds[1].events = POLLIN;
            fds[1].revents = 0;
            count = 2;
        }

        // The timeout doubles as the resize poll interval; it avoids a SIGWINCH handler that
        // would have to coexist with the host's own signal handling.
        auto result = poll(fds, count, 100);
        if (_stopRequested) {
            break;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (result > 0) {
            if ((fds[0].revents & POLLIN) != 0) {
                char drain[16];
                while (read(_wakePipe[0], drain, sizeof(drain)) > 0) {
                }
            }
            if (count == 2 && (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
                readStdin();
            }
        }
        checkResize();
    }
#endif
}

void TerminalModule::readStdin() {
#if !defined(_WIN32)
    char buffer[4096];
    auto readCount = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (readCount > 0) {
        _pendingInput.append(buffer, static_cast<size_t>(readCount));
        dispatchInput();
    } else if (readCount == 0 || (errno != EINTR && errno != EAGAIN)) {
        std::lock_guard<Valdi::Mutex> guard(_mutex);
        _stdinClosed = true;
    }
#endif
}

void TerminalModule::dispatchInput() {
#if !defined(_WIN32)
    auto completeLength = completeUtf8Length(_pendingInput);
    if (completeLength == 0) {
        return;
    }
    auto text = Valdi::StringBox::fromString(std::string_view(_pendingInput.data(), completeLength));
    _pendingInput.erase(0, completeLength);

    std::vector<Valdi::Ref<Valdi::ValueFunction>> observers;
    {
        std::lock_guard<Valdi::Mutex> guard(_mutex);
        for (const auto& it : _inputObservers) {
            observers.push_back(it.second);
        }
    }
    for (const auto& observer : observers) {
        (*observer)(Valdi::ValueFunctionFlagsNeverCallSync, {Valdi::Value(text)});
    }
#endif
}

void TerminalModule::checkResize() {
#if !defined(_WIN32)
    int columns = 0;
    int rows = 0;
    if (!queryWindowSize(columns, rows)) {
        return;
    }
    bool first = _lastColumns < 0;
    if (columns == _lastColumns && rows == _lastRows) {
        return;
    }
    _lastColumns = columns;
    _lastRows = rows;
    if (first) {
        // The initial size is read synchronously through getSize(); only changes are reported.
        return;
    }

    std::vector<Valdi::Ref<Valdi::ValueFunction>> observers;
    {
        std::lock_guard<Valdi::Mutex> guard(_mutex);
        for (const auto& it : _resizeObservers) {
            observers.push_back(it.second);
        }
    }
    for (const auto& observer : observers) {
        (*observer)(Valdi::ValueFunctionFlagsNeverCallSync,
                    {Valdi::Value(static_cast<int32_t>(columns)), Valdi::Value(static_cast<int32_t>(rows))});
    }
#endif
}

} // namespace Valdi
