#pragma once

#include <memory>
#include <string>

namespace snap::utils::logging {

enum class LogLevel { Verbose, Debug, Info, Warn, Error, None };
/**
 * When adding a new context, make sure to update shims.djinni.
 */
enum class LogContext {
    General,
    Chat,
    ContentManager,
    GRPC,
    GRPCTrace,
    Network,
    Duplex,
    Talk,
    Core,
    CUPS,
    Ad,
    TIV,
    Map,
    OnDeviceML,
    DeepLinkResolution,
    Notifications,
    S2REvent,
    Atlas,
    NeoPlayer,
    MediaStrategyCenter
};

/**
 * Callback interface for logger
 */
class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(LogLevel logLevel, LogContext logContext, const std::string& tag, const std::string& message) = 0;
};

/**
 * @brief Sets global logger instance. Keeps reference to the logger instance.
 * Call with empty shared pointer if you want to fall back to console/file logger.
 */
void setExternalLogger(Logger* logger);

/**
 * @brief Logs to log set by setExternalLogger. If logger is not set, logging is redirected to console/file.
 */
void logToExternalLogger(LogLevel logLevel, LogContext logContext, const std::string& tag, const std::string& message);

/**
 * @brief Sets max log level for cases when external logger is not specified
 */
void setInternalLoggerLogLevel(LogLevel maxLogLevel);

/**
 * @brief Formats milliseconds since epoch into a local time string for logging
 */
std::string millisecondsToString(int64_t milliseconds);

/**
 * @brief Formats seconds since epoch into a local time string for logging
 */
std::string secondsToString(int64_t seconds);
} // namespace snap::utils::logging
