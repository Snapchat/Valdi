#pragma once

#include <memory>
#include <string>
#include <string_view>

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

constexpr std::string_view toString(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose:
            return "VERBOSE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::None:
            return "NONE";
    }
    return "?";
}

constexpr std::string_view toString(LogContext context) {
    switch (context) {
        case LogContext::Chat:
            return "Chat";
        case LogContext::ContentManager:
            return "ContentManager";
        case LogContext::General:
            return "General";
        case LogContext::GRPC:
            return "GRPC";
        case LogContext::GRPCTrace:
            return "GRPCTrace";
        case LogContext::Map:
            return "Map";
        case LogContext::Network:
            return "Network";
        case LogContext::Duplex:
            return "Duplex";
        case LogContext::Talk:
            return "Talk";
        case LogContext::Core:
            return "Core";
        case LogContext::CUPS:
            return "CUPS";
        case LogContext::Ad:
            return "Ad";
        case LogContext::TIV:
            return "TIV";
        case LogContext::OnDeviceML:
            return "OnDeviceML";
        case LogContext::DeepLinkResolution:
            return "DeepLinkResolution";
        case LogContext::Notifications:
            return "Notifications";
        case LogContext::S2REvent:
            return "S2REvent";
        case LogContext::Atlas:
            return "Atlas";
        case LogContext::NeoPlayer:
            return "NeoPlayer";
        case LogContext::MediaStrategyCenter:
            return "MediaStrategyCenter";
    }
    return "?";
}

/**
 * @brief Formats milliseconds since epoch into a local time string for logging
 */
std::string millisecondsToString(int64_t milliseconds);

/**
 * @brief Formats seconds since epoch into a local time string for logging
 */
std::string secondsToString(int64_t seconds);
} // namespace snap::utils::logging
