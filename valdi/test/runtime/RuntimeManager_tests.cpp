//
//  RuntimeManager_tests.cpp
//  valdi-pc
//

#include "valdi/runtime/Debugger/DebuggerService.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/InMemoryKeychain.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_test_utils.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

#if SC_LOGGING_COMPILED_IN()
class CapturingLogger : public ILogger {
public:
    void log(LogType type, std::string message) override {
        entries.emplace_back(type, std::move(message));
    }

    std::vector<std::pair<LogType, std::string>> entries;
};
#endif

TEST(RuntimeManager, legacyConstructorRemainsCallable) {
    auto mainQueue = makeShared<MainQueue>();
    auto runtimeManager = makeShared<Valdi::RuntimeManager>(
        mainQueue->createMainThreadDispatcher(),
        nullptr,
        makeShared<InMemoryDiskCache>(),
        makeShared<InMemoryKeychain>(),
        nullptr,
        PlatformTypeLinux,
        ThreadQoSClassNormal,
        strongSmallRef(&ConsoleLogger::getLogger()),
        /* enableDebuggerService */ false,
        /* disableHotReloader */ false,
        /* isStandalone */ true);

    ASSERT_FALSE(runtimeManager->debuggerServiceEnabled());
}

#if SC_LOGGING_COMPILED_IN()
TEST(RuntimeManager, invalidExplicitDebuggerPortWarningIsValueRedacted) {
    if (!kDebuggerServiceEnabled) {
        GTEST_SKIP() << "Debugger service is compile-time disabled";
    }

    auto mainQueue = makeShared<MainQueue>();
    auto logger = makeShared<CapturingLogger>();
    auto runtimeManager = makeShared<Valdi::RuntimeManager>(
        mainQueue->createMainThreadDispatcher(),
        nullptr,
        makeShared<InMemoryDiskCache>(),
        makeShared<InMemoryKeychain>(),
        nullptr,
        PlatformTypeLinux,
        ThreadQoSClassNormal,
        logger,
        /* enableDebuggerService */ true,
        /* disableHotReloader */ false,
        /* isStandalone */ true,
        /* debuggerPort */ 65536);

    ASSERT_TRUE(runtimeManager->debuggerServiceEnabled());
    const auto warning = std::find_if(logger->entries.begin(), logger->entries.end(), [](const auto& entry) {
        return entry.first == LogTypeWarn && entry.second.find("requestedPort") != std::string::npos;
    });
    ASSERT_NE(logger->entries.end(), warning);
    EXPECT_NE(std::string::npos, warning->second.find("<redacted>"));
    EXPECT_EQ(std::string::npos, warning->second.find("65536"));
}
#endif

} // namespace ValdiTest
