#include "valdi_modules/valdi_cli/valdi_cli.hpp"

#include "valdi_core/cpp/Threading/Thread.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <optional>
#include <poll.h>
#include <pthread.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <crt_externs.h>
#else
extern char** environ;
#endif

#if defined(__APPLE__) && TARGET_OS_OSX
#define VALDI_CLI_HAS_SPAWN_CHDIR 1
#elif defined(__GLIBC__) && !defined(__ANDROID__)
#if __GLIBC_PREREQ(2, 29)
#define VALDI_CLI_HAS_SPAWN_CHDIR 1
#endif
#endif

namespace snap::valdi_modules::valdi_cli {
namespace {

constexpr int kChildExitPollTimeoutMs = 100;
constexpr auto kChildTerminateGracePeriod = std::chrono::milliseconds(500);
constexpr auto kChildTerminatePollInterval = std::chrono::milliseconds(10);

char** getEnvironment() {
#if defined(__APPLE__)
    // Dylibs and frameworks can't link `environ` directly on Apple platforms.
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

Valdi::Exception makeErrnoException(const Valdi::StringBox& operation, int errorCode) {
    return Valdi::Exception(STRING_FORMAT("{} failed: {}", operation, strerror(errorCode)));
}

std::vector<char*> makeArgv(const Valdi::StringBox& command, std::vector<Valdi::StringBox>& args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(command.getCStr()));
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.getCStr()));
    }
    argv.push_back(nullptr);
    return argv;
}

int statusCode(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
}

void setCloseOnExec(int fd) {
    const int flags = fcntl(fd, F_GETFD);
    if (flags >= 0) {
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
}

void clearCloseOnExec(int fd) {
    const int flags = fcntl(fd, F_GETFD);
    if (flags >= 0) {
        fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
    }
}

void setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void writeWakeByte(int fd) {
    if (fd < 0) {
        return;
    }

    char byte = 1;
    const ssize_t result = write(fd, &byte, sizeof(byte));
    (void)result;
}

void closeIfOpen(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

int addDup2(posix_spawn_file_actions_t& actions, int fd, int targetFd) {
    if (fd < 0) {
        return 0;
    }
    if (fd == targetFd) {
        // dup2 onto itself doesn't portably clear FD_CLOEXEC, so the child would lose this fd.
        clearCloseOnExec(fd);
        return 0;
    }
    return posix_spawn_file_actions_adddup2(&actions, fd, targetFd);
}

int addClose(posix_spawn_file_actions_t& actions, int fd, int targetFd) {
    if (fd < 0 || fd == targetFd) {
        return 0;
    }
    return posix_spawn_file_actions_addclose(&actions, fd);
}

bool hasCwd(const std::optional<Valdi::StringBox>& cwd) {
    return cwd.has_value() && !cwd->isEmpty();
}

#if defined(VALDI_CLI_HAS_SPAWN_CHDIR)
int addCwd(posix_spawn_file_actions_t& actions, const std::optional<Valdi::StringBox>& cwd) {
    if (!hasCwd(cwd)) {
        return 0;
    }
#if defined(__APPLE__)
    return posix_spawn_file_actions_addchdir(&actions, cwd->getCStr());
#else
    return posix_spawn_file_actions_addchdir_np(&actions, cwd->getCStr());
#endif
}
#endif

pid_t spawnProcess(const Valdi::StringBox& command,
                   std::vector<Valdi::StringBox> args,
                   const std::optional<Valdi::StringBox>& cwd,
                   int stdinFd,
                   int stdoutFd,
                   int stderrFd) {
#if !defined(VALDI_CLI_HAS_SPAWN_CHDIR)
    if (hasCwd(cwd)) {
        // No posix_spawn chdir action on this libc (glibc < 2.29 sysroots, Android), so a shell
        // changes directory and then execs the command in place.
        args.insert(args.begin(),
                    {STRING_LITERAL("-c"),
                     STRING_LITERAL("cd -- \"$1\" || exit 127; shift; exec \"$@\""),
                     STRING_LITERAL("sh"),
                     *cwd,
                     command});
        return spawnProcess(STRING_LITERAL("sh"), std::move(args), std::nullopt, stdinFd, stdoutFd, stderrFd);
    }
#endif

    posix_spawn_file_actions_t actions;
    int result = posix_spawn_file_actions_init(&actions);
    if (result != 0) {
        throw makeErrnoException(STRING_LITERAL("posix_spawn_file_actions_init"), result);
    }

#if defined(VALDI_CLI_HAS_SPAWN_CHDIR)
    result = addCwd(actions, cwd);
#endif
    if (result == 0) {
        result = addDup2(actions, stdinFd, STDIN_FILENO);
    }
    if (result == 0) {
        result = addDup2(actions, stdoutFd, STDOUT_FILENO);
    }
    if (result == 0) {
        result = addDup2(actions, stderrFd, STDERR_FILENO);
    }
    if (result == 0) {
        result = addClose(actions, stdinFd, STDIN_FILENO);
    }
    if (result == 0) {
        result = addClose(actions, stdoutFd, STDOUT_FILENO);
    }
    if (result == 0) {
        result = addClose(actions, stderrFd, STDERR_FILENO);
    }

    // Children otherwise inherit the host's signal mask and ignored signals; a host that ignores
    // SIGTERM would make kill() a no-op.
    posix_spawnattr_t attributes;
    bool hasAttributes = false;
    if (result == 0) {
        result = posix_spawnattr_init(&attributes);
        hasAttributes = result == 0;
    }
    if (result == 0) {
        sigset_t defaultSignals;
        sigfillset(&defaultSignals);
        sigdelset(&defaultSignals, SIGKILL);
        sigdelset(&defaultSignals, SIGSTOP);
        sigset_t emptyMask;
        sigemptyset(&emptyMask);
        result = posix_spawnattr_setsigdefault(&attributes, &defaultSignals);
        if (result == 0) {
            result = posix_spawnattr_setsigmask(&attributes, &emptyMask);
        }
        if (result == 0) {
            result = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
        }
    }

    pid_t pid = -1;
    if (result == 0) {
        auto argv = makeArgv(command, args);
        result = posix_spawnp(&pid, command.getCStr(), &actions, &attributes, argv.data(), getEnvironment());
    }

    if (hasAttributes) {
        posix_spawnattr_destroy(&attributes);
    }
    posix_spawn_file_actions_destroy(&actions);
    if (result != 0) {
        throw makeErrnoException(STRING_FORMAT("posix_spawnp({})", command), result);
    }
    return pid;
}

} // namespace

class ChildProcessNativeModuleImpl : public ChildProcessNativeModule {
public:
    ChildProcessNativeModuleImpl() {
        if (pipe(_wakePipe) != 0) {
            throw makeErrnoException(STRING_LITERAL("pipe(wake)"), errno);
        }
        setCloseOnExec(_wakePipe[0]);
        setCloseOnExec(_wakePipe[1]);
        setNonBlocking(_wakePipe[0]);
        setNonBlocking(_wakePipe[1]);
        _ioThread =
            Valdi::Thread::create(STRING_LITERAL("Valdi ChildProcess IO"), Valdi::ThreadQoSClassNormal, [this]() {
                ioLoop();
            }).moveValue();
    }

    ~ChildProcessNativeModuleImpl() override {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            _tearingDown = true;
        }
        wake();
        if (_ioThread != nullptr) {
            _ioThread->join();
            _ioThread = nullptr;
        }
        cleanupProcesses();
        closeIfOpen(_wakePipe[0]);
        closeIfOpen(_wakePipe[1]);
    }

    double spawn(Valdi::StringBox command,
                 std::vector<Valdi::StringBox> args,
                 std::optional<Valdi::StringBox> cwd,
                 Valdi::Ref<NativeChildProcessPipe> stdoutPipe,
                 Valdi::Ref<NativeChildProcessPipe> stderrPipe,
                 Valdi::Ref<NativeChildProcessExitListener> exitListener,
                 std::optional<bool> inheritOutput) final {
        int stdinPipe[2];
        if (pipe(stdinPipe) != 0) {
            throw makeErrnoException(STRING_LITERAL("pipe(stdin)"), errno);
        }
        // Every pipe end is close-on-exec so a concurrent spawn can't inherit it and hold the pipe open;
        // the dup2 file actions clear the flag on the child's stdio.
        setCloseOnExec(stdinPipe[0]);
        setCloseOnExec(stdinPipe[1]);
        setNonBlocking(stdinPipe[1]);
#if defined(F_SETNOSIGPIPE)
        fcntl(stdinPipe[1], F_SETNOSIGPIPE, 1);
#endif

        int stdoutPipeFd[2] = {-1, -1};
        int stderrPipeFd[2] = {-1, -1};
        if (!inheritOutput.value_or(false)) {
            if (pipe(stdoutPipeFd) != 0) {
                close(stdinPipe[0]);
                close(stdinPipe[1]);
                throw makeErrnoException(STRING_LITERAL("pipe(stdout)"), errno);
            }
            if (pipe(stderrPipeFd) != 0) {
                close(stdinPipe[0]);
                close(stdinPipe[1]);
                close(stdoutPipeFd[0]);
                close(stdoutPipeFd[1]);
                throw makeErrnoException(STRING_LITERAL("pipe(stderr)"), errno);
            }
            setCloseOnExec(stdoutPipeFd[0]);
            setCloseOnExec(stdoutPipeFd[1]);
            setCloseOnExec(stderrPipeFd[0]);
            setCloseOnExec(stderrPipeFd[1]);
            setNonBlocking(stdoutPipeFd[0]);
            setNonBlocking(stderrPipeFd[0]);
        }

        pid_t pid = -1;
        try {
            pid = spawnProcess(command, std::move(args), cwd, stdinPipe[0], stdoutPipeFd[1], stderrPipeFd[1]);
        } catch (...) {
            close(stdinPipe[0]);
            close(stdinPipe[1]);
            closeIfOpen(stdoutPipeFd[0]);
            closeIfOpen(stdoutPipeFd[1]);
            closeIfOpen(stderrPipeFd[0]);
            closeIfOpen(stderrPipeFd[1]);
            throw;
        }
        close(stdinPipe[0]);
        closeIfOpen(stdoutPipeFd[1]);
        closeIfOpen(stderrPipeFd[1]);
        ProcessState state;
        state.stdinFd = stdinPipe[1];
        state.stdoutFd = stdoutPipeFd[0];
        state.stderrFd = stderrPipeFd[0];
        state.stdoutPipe = stdoutPipe;
        state.stderrPipe = stderrPipe;
        state.exitListener = exitListener;

        {
            std::lock_guard<std::mutex> guard(_mutex);
            _processes.insert_or_assign(pid, std::move(state));
        }
        wake();

        return static_cast<double>(pid);
    }

    void sendToStdin(double pidValue, Valdi::BytesView data) final {
        const auto pid = static_cast<pid_t>(pidValue);
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _processes.find(pid);
            if (it == _processes.end()) {
                return;
            }
            it->second.pendingStdin.push_back(data);
        }
        wake();
    }

    void closeStdin(double pidValue) final {
        const auto pid = static_cast<pid_t>(pidValue);
        {
            std::lock_guard<std::mutex> guard(_mutex);
            auto it = _processes.find(pid);
            if (it == _processes.end()) {
                return;
            }
            auto& state = it->second;
            if (state.pendingStdin.empty()) {
                closeIfOpen(state.stdinFd);
            } else {
                state.closeStdinWhenDrained = true;
            }
        }
        wake();
    }

    void kill(double pidValue) final {
        const auto pid = static_cast<pid_t>(pidValue);
        {
            // Reaping happens under this mutex, so a tracked pid is still our (possibly zombie) child and
            // can't have been reused by an unrelated process.
            std::lock_guard<std::mutex> guard(_mutex);
            if (pid <= 0 || _processes.find(pid) == _processes.end()) {
                return;
            }
            ::kill(pid, SIGTERM);
        }
        wake();
    }

private:
    struct ProcessState {
        int stdinFd = -1;
        int stdoutFd = -1;
        int stderrFd = -1;
        Valdi::Ref<NativeChildProcessPipe> stdoutPipe;
        Valdi::Ref<NativeChildProcessPipe> stderrPipe;
        Valdi::Ref<NativeChildProcessExitListener> exitListener;
        std::vector<Valdi::BytesView> pendingStdin;
        bool closeStdinWhenDrained = false;
    };

    enum class PollEntryKind {
        Stdout,
        Stderr,
        Stdin,
    };

    struct PollEntry {
        pid_t pid;
        PollEntryKind kind;
    };

    struct OutputCallback {
        Valdi::Ref<NativeChildProcessPipe> pipe;
        Valdi::Ref<Valdi::ByteBuffer> bytes;
    };

    struct ExitCallback {
        Valdi::Ref<NativeChildProcessExitListener> listener;
        int exitStatus;
    };

    void wake() {
        writeWakeByte(_wakePipe[1]);
        _condition.notify_one();
    }

    void ioLoop() {
        // A child that closes its stdin between poll() and write() raises SIGPIPE, which would otherwise
        // terminate the host. On Linux it is delivered to the writing thread, so blocking it here is
        // enough; macOS also sets F_SETNOSIGPIPE on the stdin pipe. write() still reports EPIPE.
        sigset_t pipeSignals;
        sigemptyset(&pipeSignals);
        sigaddset(&pipeSignals, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &pipeSignals, nullptr);

        while (true) {
            std::vector<pollfd> pollFds;
            std::vector<PollEntry> pollEntries;
            {
                std::unique_lock<std::mutex> guard(_mutex);
                _condition.wait(guard, [this]() { return _tearingDown || !_processes.empty(); });
                if (_tearingDown) {
                    return;
                }
                buildPollFds(pollFds, pollEntries);
            }

            const int pollResult = poll(pollFds.data(), static_cast<nfds_t>(pollFds.size()), kChildExitPollTimeoutMs);
            if (pollResult < 0 && errno == EINTR) {
                continue;
            }

            std::vector<OutputCallback> outputCallbacks;
            std::vector<ExitCallback> exitCallbacks;
            {
                std::lock_guard<std::mutex> guard(_mutex);
                if (_tearingDown) {
                    return;
                }
                if (pollResult > 0) {
                    drainWakePipeIfNeeded(pollFds);
                    handlePollEvents(pollFds, pollEntries, outputCallbacks);
                }
                reapExitedProcesses(outputCallbacks, exitCallbacks);
            }

            emitOutputCallbacks(outputCallbacks);
            emitExitCallbacks(exitCallbacks);
        }
    }

    void buildPollFds(std::vector<pollfd>& pollFds, std::vector<PollEntry>& pollEntries) {
        pollFds.push_back({_wakePipe[0], POLLIN, 0});

        for (auto& entry : _processes) {
            const pid_t pid = entry.first;
            auto& state = entry.second;
            if (state.stdoutFd >= 0) {
                pollFds.push_back({state.stdoutFd, POLLIN, 0});
                pollEntries.push_back({pid, PollEntryKind::Stdout});
            }
            if (state.stderrFd >= 0) {
                pollFds.push_back({state.stderrFd, POLLIN, 0});
                pollEntries.push_back({pid, PollEntryKind::Stderr});
            }
            if (state.stdinFd >= 0 && !state.pendingStdin.empty()) {
                pollFds.push_back({state.stdinFd, POLLOUT, 0});
                pollEntries.push_back({pid, PollEntryKind::Stdin});
            }
        }
    }

    void drainWakePipeIfNeeded(const std::vector<pollfd>& pollFds) {
        if (pollFds.empty() || !(pollFds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
            return;
        }

        char buffer[64];
        while (read(_wakePipe[0], buffer, sizeof(buffer)) > 0) {
        }
    }

    void handlePollEvents(const std::vector<pollfd>& pollFds,
                          const std::vector<PollEntry>& pollEntries,
                          std::vector<OutputCallback>& outputCallbacks) {
        for (size_t i = 0; i < pollEntries.size(); i++) {
            const auto& pollFd = pollFds[i + 1];
            if (pollFd.revents == 0) {
                continue;
            }

            const auto& pollEntry = pollEntries[i];
            auto processIt = _processes.find(pollEntry.pid);
            if (processIt == _processes.end()) {
                continue;
            }

            auto& state = processIt->second;
            switch (pollEntry.kind) {
                case PollEntryKind::Stdout:
                    if (pollFd.revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
                        drainOutput(state.stdoutFd, state.stdoutPipe, outputCallbacks);
                    }
                    break;
                case PollEntryKind::Stderr:
                    if (pollFd.revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
                        drainOutput(state.stderrFd, state.stderrPipe, outputCallbacks);
                    }
                    break;
                case PollEntryKind::Stdin:
                    if (pollFd.revents & POLLOUT) {
                        flushStdin(state);
                    }
                    if (pollFd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
                        closeIfOpen(state.stdinFd);
                        state.pendingStdin.clear();
                    }
                    break;
            }
        }
    }

    void reapExitedProcesses(std::vector<OutputCallback>& outputCallbacks, std::vector<ExitCallback>& exitCallbacks) {
        for (auto it = _processes.begin(); it != _processes.end();) {
            const pid_t pid = it->first;
            auto& state = it->second;

            int status = 0;
            const pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == 0 || (result < 0 && errno == EINTR)) {
                ++it;
                continue;
            }

            drainOutput(state.stdoutFd, state.stdoutPipe, outputCallbacks);
            drainOutput(state.stderrFd, state.stderrPipe, outputCallbacks);
            closeIfOpen(state.stdinFd);
            closeIfOpen(state.stdoutFd);
            closeIfOpen(state.stderrFd);
            state.pendingStdin.clear();

            const int exitStatus = result < 0 ? 127 : statusCode(status);
            if (state.exitListener != nullptr) {
                exitCallbacks.push_back({state.exitListener, exitStatus});
            }
            it = _processes.erase(it);
        }
    }

    void drainOutput(int& fd,
                     const Valdi::Ref<NativeChildProcessPipe>& pipe,
                     std::vector<OutputCallback>& outputCallbacks) {
        if (fd < 0) {
            return;
        }

        auto bytes = Valdi::makeShared<Valdi::ByteBuffer>();
        while (true) {
            char buffer[4096];
            const ssize_t count = read(fd, buffer, sizeof(buffer));
            if (count > 0) {
                bytes->append(buffer, buffer + count);
            } else if (count == 0) {
                enqueueOutputCallback(pipe, bytes, outputCallbacks);
                closeIfOpen(fd);
                return;
            } else if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                enqueueOutputCallback(pipe, bytes, outputCallbacks);
                return;
            } else {
                enqueueOutputCallback(pipe, bytes, outputCallbacks);
                closeIfOpen(fd);
                return;
            }
        }
    }

    void enqueueOutputCallback(const Valdi::Ref<NativeChildProcessPipe>& pipe,
                               const Valdi::Ref<Valdi::ByteBuffer>& bytes,
                               std::vector<OutputCallback>& outputCallbacks) {
        if (!bytes->empty() && pipe != nullptr) {
            outputCallbacks.push_back({pipe, bytes});
        }
    }

    void emitOutputCallbacks(std::vector<OutputCallback>& outputCallbacks) {
        for (auto& callback : outputCallbacks) {
            callback.pipe->onData(callback.bytes->toBytesView());
        }
    }

    void emitExitCallbacks(std::vector<ExitCallback>& exitCallbacks) {
        for (auto& callback : exitCallbacks) {
            callback.listener->onExit(callback.exitStatus);
        }
    }

    void flushStdin(ProcessState& state) {
        if (state.stdinFd < 0 || state.pendingStdin.empty()) {
            return;
        }

        while (!state.pendingStdin.empty()) {
            auto& data = state.pendingStdin.front();
            if (data.empty()) {
                state.pendingStdin.erase(state.pendingStdin.begin());
                continue;
            }

            const ssize_t written = write(state.stdinFd, data.data(), data.size());
            if (written > 0) {
                if (static_cast<size_t>(written) >= data.size()) {
                    state.pendingStdin.erase(state.pendingStdin.begin());
                } else {
                    data = data.subrange(static_cast<size_t>(written), data.size() - static_cast<size_t>(written));
                }
            } else if (written < 0 && errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                closeIfOpen(state.stdinFd);
                state.pendingStdin.clear();
                return;
            }
        }

        if (state.closeStdinWhenDrained) {
            closeIfOpen(state.stdinFd);
        }
    }

    void cleanupProcesses() {
        std::lock_guard<std::mutex> guard(_mutex);
        std::vector<pid_t> running;
        for (auto& entry : _processes) {
            const pid_t pid = entry.first;
            auto& state = entry.second;
            closeIfOpen(state.stdinFd);
            if (pid > 0) {
                ::kill(pid, SIGTERM);
                running.push_back(pid);
            }
        }

        const auto deadline = std::chrono::steady_clock::now() + kChildTerminateGracePeriod;
        while (!running.empty()) {
            for (auto it = running.begin(); it != running.end();) {
                int status = 0;
                const pid_t waitResult = waitpid(*it, &status, WNOHANG);
                if (waitResult == 0 || (waitResult < 0 && errno == EINTR)) {
                    ++it;
                } else {
                    it = running.erase(it);
                }
            }
            if (running.empty() || std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            std::this_thread::sleep_for(kChildTerminatePollInterval);
        }

        for (const pid_t pid : running) {
            ::kill(pid, SIGKILL);
            int status = 0;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
        }

        for (auto& entry : _processes) {
            auto& state = entry.second;
            closeIfOpen(state.stdoutFd);
            closeIfOpen(state.stderrFd);
            state.pendingStdin.clear();
        }
        _processes.clear();
    }

    Valdi::Ref<Valdi::Thread> _ioThread;
    std::mutex _mutex;
    std::condition_variable _condition;
    bool _tearingDown = false;
    int _wakePipe[2] = {-1, -1};
    Valdi::FlatMap<pid_t, ProcessState> _processes;
};

class ChildProcessNativeModuleFactoryImpl : public ChildProcessNativeModuleFactory {
public:
    ChildProcessNativeModuleFactoryImpl() = default;

    Valdi::Ref<ChildProcessNativeModule> onLoadModule() final {
        return Valdi::makeShared<ChildProcessNativeModuleImpl>();
    }
};

auto registerChildProcessNativeModule =
    Valdi::RegisterModuleFactory::registerTyped<ChildProcessNativeModuleFactoryImpl>();

} // namespace snap::valdi_modules::valdi_cli
