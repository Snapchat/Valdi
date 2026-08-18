#pragma once

#include "utils/logging/Logger.hpp"
#include "utils/platform/BuildOptions.hpp"
#include "utils/platform/TargetPlatform.hpp"

#include <fmt/format.h>

namespace snap {
namespace utils::logging {

// TODO: Move this logic out to build scripts and just plumb in the enum value
constexpr auto kMinAllowedLogLevel = []() constexpr {
    if constexpr (!kLoggingCompiledIn) {
        return LogLevel::None;
    }

    // The on-device Specs lens keeps logging compiled in (see sc_build_flag_logging), but
    // only from Info up: noisier levels stay out to bound log volume and the binary size cost
    // of retained format strings. Dev builds keep everything.
    if constexpr (isSnapOsNdk()) {
        if constexpr (isSystemDebug()) {
            return LogLevel::Verbose;
        }
        return LogLevel::Info;
    }

    if constexpr (isWasm()) {
        if constexpr (kIsDevBuild) {
            // Verbose logs can be very noisy, let's try debug for now.
            return LogLevel::Debug;
        } else {
            // Logs are compiled out in prod currently, so this level is mainly for gold/alpha.
            return LogLevel::Info;
        }
    }

    // TODO: Drive off of build flavors not libc macros!
    if constexpr (isSystemDebug()) {
        return LogLevel::Verbose;
    }

    if constexpr (isDesktop()) {
        // Mac and linux disable noisy debug logs.
        return LogLevel::Info;
    } else {
        return LogLevel::Debug;
    }
}();

template<LogLevel logLevel, typename FmtString, typename... Args>
void log(LogContext context, const std::string& tag, FmtString&& formatStr, Args&&... args) {
    static_assert(logLevel != LogLevel::None, "NONE cannot be used for actual logging");

    if constexpr (logLevel >= kMinAllowedLogLevel) {
        // logging allowed for this level, forwarding into fmtlib
        logToExternalLogger(
            logLevel, context, tag, fmt::format(std::forward<FmtString>(formatStr), std::forward<Args>(args)...));
    }
}
} // namespace utils::logging

///
/// Set of helper functions for every log level, put directly in ::snap namespace to avoid unnecessary typing of full
/// namespace in user code.
///

template<typename FmtString, typename... Args>
void logVerbose(utils::logging::LogContext context, const std::string& tag, FmtString&& formatStr, Args&&... args) {
    utils::logging::log<utils::logging::LogLevel::Verbose>(
        context, tag, std::forward<FmtString>(formatStr), std::forward<Args>(args)...);
}

template<typename FmtString, typename... Args>
void logDebug(utils::logging::LogContext context, const std::string& tag, FmtString&& formatStr, Args&&... args) {
    utils::logging::log<utils::logging::LogLevel::Debug>(
        context, tag, std::forward<FmtString>(formatStr), std::forward<Args>(args)...);
}

template<typename FmtString, typename... Args>
void logInfo(utils::logging::LogContext context, const std::string& tag, FmtString&& formatStr, Args&&... args) {
    utils::logging::log<utils::logging::LogLevel::Info>(
        context, tag, std::forward<FmtString>(formatStr), std::forward<Args>(args)...);
}

template<typename FmtString, typename... Args>
void logWarn(utils::logging::LogContext context, const std::string& tag, FmtString&& formatStr, Args&&... args) {
    utils::logging::log<utils::logging::LogLevel::Warn>(
        context, tag, std::forward<FmtString>(formatStr), std::forward<Args>(args)...);
}

template<typename FmtString, typename... Args>
void logError(utils::logging::LogContext context, const std::string& tag, FmtString&& formatStr, Args&&... args) {
    utils::logging::log<utils::logging::LogLevel::Error>(
        context, tag, std::forward<FmtString>(formatStr), std::forward<Args>(args)...);
}
} // namespace snap

#if SC_LOGGING_COMPILED_IN()
// Used when logging from another data source where file/log line is not useful.
#define SCLOG_GENERIC_NO_FILE(context, level, tag, fmtString, ...)                                                     \
    ::snap::utils::logging::log<level>(context, tag, FMT_STRING(fmtString), ##__VA_ARGS__)
#define SC_STRINGIZE(x) SC_STRINGIZE2(x)
#define SC_STRINGIZE2(x) #x
#define SCLOG_GENERIC(context, level, tag, fmtString, ...)                                                             \
    ::snap::utils::logging::log<level>(                                                                                \
        context, tag, FMT_STRING("[" __FILE_NAME__ ":" SC_STRINGIZE(__LINE__) "] " fmtString), ##__VA_ARGS__)
#else
#define SCLOG_GENERIC_NO_FILE(context, level, tag, fmtString, ...)                                                     \
    {                                                                                                                  \
        (void)tag;                                                                                                     \
    }
#define SCLOG_GENERIC(context, level, tag, fmtString, ...)                                                             \
    {                                                                                                                  \
        (void)tag;                                                                                                     \
    }
#endif

///
/// Set of macros that check formatting string against the supplied arguments and fail to compile if they don't match.
/// NOTE: formatting string must be supplied directly as a string literal. Passing std::string or a pointer to the
/// existing string won't work.
/// For example:
/// SCLOG_I(kTag, "Result of operation is {} for {}", arg1, arg2); // GOOD
///
/// std::string fmtString = "Result of operation is {} for {}";
/// SCLOG_I(kTag, fmtString, arg1, arg2); // BAD, reports some obscure compilation error
/// logVerbose(kTag, fmtString, arg1, arg2); // GOOD, but no compile-time check for the formatting string
///

#define SCLOGV(tag, fmtString, ...)                                                                                    \
    SCLOG_GENERIC(::snap::utils::logging::LogContext::General,                                                         \
                  ::snap::utils::logging::LogLevel::Verbose,                                                           \
                  tag,                                                                                                 \
                  fmtString,                                                                                           \
                  ##__VA_ARGS__)
#define SCLOGD(tag, fmtString, ...)                                                                                    \
    SCLOG_GENERIC(::snap::utils::logging::LogContext::General,                                                         \
                  ::snap::utils::logging::LogLevel::Debug,                                                             \
                  tag,                                                                                                 \
                  fmtString,                                                                                           \
                  ##__VA_ARGS__)
#define SCLOGI(tag, fmtString, ...)                                                                                    \
    SCLOG_GENERIC(::snap::utils::logging::LogContext::General,                                                         \
                  ::snap::utils::logging::LogLevel::Info,                                                              \
                  tag,                                                                                                 \
                  fmtString,                                                                                           \
                  ##__VA_ARGS__)
#define SCLOGW(tag, fmtString, ...)                                                                                    \
    SCLOG_GENERIC(::snap::utils::logging::LogContext::General,                                                         \
                  ::snap::utils::logging::LogLevel::Warn,                                                              \
                  tag,                                                                                                 \
                  fmtString,                                                                                           \
                  ##__VA_ARGS__)
#define SCLOGE(tag, fmtString, ...)                                                                                    \
    SCLOG_GENERIC(::snap::utils::logging::LogContext::General,                                                         \
                  ::snap::utils::logging::LogLevel::Error,                                                             \
                  tag,                                                                                                 \
                  fmtString,                                                                                           \
                  ##__VA_ARGS__)

#define SCLOGCONTEXTV(tag, logContext, fmtString, ...)                                                                 \
    SCLOG_GENERIC(logContext, ::snap::utils::logging::LogLevel::Verbose, tag, fmtString, ##__VA_ARGS__)
#define SCLOGCONTEXTD(tag, logContext, fmtString, ...)                                                                 \
    SCLOG_GENERIC(logContext, ::snap::utils::logging::LogLevel::Debug, tag, fmtString, ##__VA_ARGS__)
#define SCLOGCONTEXTI(tag, logContext, fmtString, ...)                                                                 \
    SCLOG_GENERIC(logContext, ::snap::utils::logging::LogLevel::Info, tag, fmtString, ##__VA_ARGS__)
#define SCLOGCONTEXTW(tag, logContext, fmtString, ...)                                                                 \
    SCLOG_GENERIC(logContext, ::snap::utils::logging::LogLevel::Warn, tag, fmtString, ##__VA_ARGS__)
#define SCLOGCONTEXTE(tag, logContext, fmtString, ...)                                                                 \
    SCLOG_GENERIC(logContext, ::snap::utils::logging::LogLevel::Error, tag, fmtString, ##__VA_ARGS__)
