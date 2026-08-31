//
//  ConsoleLogger.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "utils/platform/BuildOptions.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"

#include <iomanip>
#include <iostream>

namespace Valdi {

#if defined(__ANDROID__)
#include <android/log.h>

ConsoleLogger& ConsoleLogger::getLogger() {
    static auto* kLogger = new ConsoleLogger();
    return *kLogger;
}

ConsoleLogger& ConsoleLogger::getErrorLogger() {
    return getLogger();
}
#else

ConsoleLogger& ConsoleLogger::getLogger() {
    static auto* kLogger = new ConsoleLogger(std::cout);
    return *kLogger;
}

ConsoleLogger& ConsoleLogger::getErrorLogger() {
    static auto* kLogger = new ConsoleLogger(std::cerr);
    return *kLogger;
}

#endif

#if defined(SC_IOS)
constexpr bool isDebug = snap::kIsDevBuild;

ConsoleLogger::ConsoleLogger(std::ostream& stream) : _valdiLogger(os_log_create("com.snap.valdi", "Valdi")) {}
#elif !defined(__ANDROID__)
ConsoleLogger::ConsoleLogger(std::ostream& stream) : _stream(stream) {
    _stopwatch.start();
}
#endif

void ConsoleLogger::log(Valdi::LogType type, std::string message) {
    std::lock_guard<Mutex> guard(_mutex);

#if defined(__ANDROID__)
    android_LogPriority logType = ANDROID_LOG_DEFAULT;
    switch (type) {
        case Valdi::LogTypeDebug:
            logType = ANDROID_LOG_DEBUG;
            break;
        case Valdi::LogTypeInfo:
            logType = ANDROID_LOG_INFO;
            break;
        case Valdi::LogTypeWarn:
            logType = ANDROID_LOG_WARN;
            break;
        case Valdi::LogTypeError:
            logType = ANDROID_LOG_ERROR;
            break;
        case Valdi::LogTypeCount:
            return;
    }

    __android_log_print(logType, "Valdi", "%s", message.c_str());
#elif defined(SC_IOS)
    os_log_type_t logType = OS_LOG_TYPE_DEFAULT;

    switch (type) {
        case Valdi::LogTypeDebug:
            logType = OS_LOG_TYPE_DEBUG;
            break;
        case Valdi::LogTypeInfo:
            logType = OS_LOG_TYPE_INFO;
            break;
        case Valdi::LogTypeWarn:
            logType = OS_LOG_TYPE_DEFAULT; // NOTE(rjaber): oslog doesn't contain an equivalent log level
            break;
        case Valdi::LogTypeError:
            logType = OS_LOG_TYPE_ERROR;
            break;
        case Valdi::LogTypeCount:
            return;
    }
    if constexpr (isDebug) {
        os_log_with_type(_valdiLogger, logType, "%{public}s", message.c_str());
    } else {
        os_log_with_type(_valdiLogger, logType, "%s", message.c_str());
    }
#else
    const char* logType;

    switch (type) {
        case Valdi::LogTypeDebug:
            logType = "DEBUG";
            break;
        case Valdi::LogTypeInfo:
            logType = "INFO";
            break;
        case Valdi::LogTypeWarn:
            logType = "WARN";
            break;
        case Valdi::LogTypeError:
            logType = "ERROR";
            break;
        case Valdi::LogTypeCount:
            return;
    }
    auto ms = _stopwatch.elapsedMs() % 1000;
    _stream << "[" << _stopwatch.elapsedSeconds() << "." << std::setfill('0') << std::setw(3) << ms << ": " << logType
            << "] " << message << std::endl;
#endif
}

void ConsoleLogger::writeDirect(std::string_view message) {
    std::lock_guard<Mutex> guard(_mutex);
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, "Valdi", "%.*s", static_cast<int>(message.size()), message.data());
#elif defined(SC_IOS)
    if constexpr (isDebug) {
        os_log_info(_valdiLogger, "%{public}.*s", (int)message.size(), message.data());
    } else {
        os_log_info(_valdiLogger, "%.*s", (int)message.size(), message.data());
    }
#else
    auto t = message.size();
    (void)t;
    _stream << message << std::flush;
#endif
}

bool ConsoleLogger::isEnabledForType(Valdi::LogType type) {
    return type >= _minLogType;
}

void ConsoleLogger::setMinLogType(Valdi::LogType minLogType) {
    _minLogType = minLogType;
}

} // namespace Valdi
