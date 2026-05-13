#include "utils/logging/Logger.hpp"
#include "utils/platform/TargetPlatform.hpp"

#include <atomic>
#include <fmt/chrono.h>
#include <fmt/format.h>

// a bunch of headrs only needed on desktop
#if defined(SC_DESKTOP) || defined(EMSCRIPTEN)
#include <cstdio>
#include <fmt/color.h>
#include <fmt/ostream.h>
#include <sys/time.h>
#include <thread>
#endif

namespace snap::utils::logging {
namespace {
// Global variable for logging facility
// Intentionally using a raw pointer and allocating this on the heap to make sure the logger is not destroyed
// on app exit.
// std::atomic is trivallly destructible and hence is not cleaned up on program shutdown and are safe to access
// from other code running during shutdown.
// https://github.com/abseil/abseil-cpp/blob/389ec3f906f018661a5308458d623d01f96d7b23/absl/base/const_init.h#L42
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
std::atomic<Logger*> gExternalLogger;

// Maximum allowed log level when no external logger is specified
LogLevel gMaxInternalLogLevel = LogLevel::Verbose;

} // namespace

void logToExternalLogger(LogLevel logLevel, LogContext logContext, const std::string& tag, const std::string& message) {
    if (auto* logger = gExternalLogger.load()) {
        try {
            logger->log(logLevel, logContext, tag, message);
        } catch (std::range_error&) {
            logger->log(logLevel, logContext, tag, "[INVALID LOG MESSAGE]");
        }
    } else {
        if (logLevel < gMaxInternalLogLevel) {
            return;
        }

#if defined(SC_DESKTOP) || defined(EMSCRIPTEN)

#if defined(EMSCRIPTEN)

        fmt::print("[{}] {} {} {}\n", toString(logLevel), toString(logContext), tag, message);

#else // defined(EMSCRIPTEN)

        timeval curTime;
        gettimeofday(&curTime, nullptr);
        int64_t milliseconds = curTime.tv_sec * 1000 + curTime.tv_usec / 1000;
        auto getStyle = [&] {
            static bool useColor = getenv("TERM") != nullptr;
            if (!useColor) {
                return fmt::text_style();
            }
            switch (logLevel) {
                case LogLevel::Warn:
                    return fmt::fg(fmt::terminal_color::yellow);
                case LogLevel::Error:
                    return fmt::fg(fmt::terminal_color::red);
                default:
                    return fmt::fg(fmt::terminal_color::white);
            }
        };
        fmt::print(getStyle(),
                   "[{}][{:<14}] [{}] {} {} {}",
                   toString(logLevel),
                   std::this_thread::get_id(),
                   millisecondsToString(milliseconds),
                   toString(logContext),
                   tag,
                   message);
        fmt::print("\n");

#endif // defined(EMSCRIPTEN)

#endif // defined(SC_DESKTOP) || defined(EMSCRIPTEN)
    }
}

void setExternalLogger(Logger* logger) {
    gExternalLogger.store(logger);
}

void setInternalLoggerLogLevel(LogLevel maxLogLevel) {
    gMaxInternalLogLevel = maxLogLevel;
}

std::string millisecondsToString(int64_t milliseconds) {
    return fmt::format(
        "{:%Y-%m-%d %H:%M:%S}.{:0>3}", fmt::localtime(static_cast<time_t>(milliseconds / 1000)), milliseconds % 1000);
}

std::string secondsToString(int64_t seconds) {
    return fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::localtime(static_cast<time_t>(seconds)));
}

} // namespace snap::utils::logging
