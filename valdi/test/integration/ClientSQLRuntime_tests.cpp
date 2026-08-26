#include "JSBridgeTestFixture.hpp"
#include "RuntimeTestsUtils.hpp"
#include "gtest/gtest.h"
#include "valdi/runtime/Resources/DiskCacheImpl.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <unistd.h>

using namespace Valdi;

namespace ValdiTest {

class ClientSQLRuntimeTemporaryDirectory {
public:
    ClientSQLRuntimeTemporaryDirectory() {
        char path[] = "/tmp/valdi-clientsql-runtime-XXXXXX";
        const auto directory = mkdtemp(path);
        if (directory != nullptr) {
            _path = directory;
        }
    }

    ~ClientSQLRuntimeTemporaryDirectory() {
        if (!_path.empty()) {
            std::error_code error;
            std::filesystem::remove_all(_path, error);
        }
    }

    const std::string& path() const {
        return _path;
    }

private:
    std::string _path;
};

class ClientSQLRuntimeFixture : public JSBridgeTestFixture {
protected:
    void SetUp() override {
        JSBridgeTestFixture::SetUp();
        if (IsSkipped()) {
            return;
        }
        ASSERT_FALSE(directory.path().empty());
        auto diskCache = makeShared<DiskCacheImpl>(StringCache::getGlobal().makeString(directory.path()));
        wrapper = RuntimeWrapper(
            getJsBridge(),
            isWithTSN() ? TSNMode::Enabled : TSNMode::Disabled,
            diskCache);
    }

    void TearDown() override {
        wrapper.teardown();
    }

    ClientSQLRuntimeTemporaryDirectory directory;
    RuntimeWrapper wrapper;
};

TEST_P(ClientSQLRuntimeFixture, resolvesGeneratedDatabaseThroughRealNativeModule) {
    auto loadResult = wrapper.loadModule(
        STRING_LITERAL("client_sql_smoke"),
        ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(loadResult) << loadResult.description();

    const std::string startScript = R"JS(
        global.__clientSQLRuntimeDone = false;
        global.__clientSQLRuntimeResult = undefined;
        global.__clientSQLRuntimeError = undefined;
        global.require('client_sql_smoke/src/ClientSQLSmoke').runClientSQLNativeIntegration(
          function(result, error) {
            global.__clientSQLRuntimeResult = result;
            global.__clientSQLRuntimeError = error;
            global.__clientSQLRuntimeDone = true;
          }
        );
    )JS";
    auto startResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(startScript)->toBytesView(),
        STRING_LITERAL("clientsql_runtime_integration_start.js"));
    ASSERT_TRUE(startResult) << startResult.description();

    bool completed = false;
    for (int attempt = 0; attempt < 500; ++attempt) {
        wrapper.flushQueues();
        const std::string doneScript = "return global.__clientSQLRuntimeDone === true;";
        auto doneResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
            makeShared<ByteBuffer>(doneScript)->toBytesView(),
            STRING_LITERAL("clientsql_runtime_integration_poll.js"));
        ASSERT_TRUE(doneResult) << doneResult.description();
        if (doneResult.value().toBool()) {
            completed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(completed) << "Timed out waiting for generated ClientSQL/native integration";

    const std::string errorScript = "return global.__clientSQLRuntimeError || '';";
    auto errorResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(errorScript)->toBytesView(),
        STRING_LITERAL("clientsql_runtime_integration_error.js"));
    ASSERT_TRUE(errorResult) << errorResult.description();
    EXPECT_EQ(STRING_LITERAL(""), errorResult.value().toStringBox());

    const std::string resultScript = "return global.__clientSQLRuntimeResult || '';";
    auto integrationResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(resultScript)->toBytesView(),
        STRING_LITERAL("clientsql_runtime_integration_result.js"));
    ASSERT_TRUE(integrationResult) << integrationResult.description();
    EXPECT_EQ(STRING_LITERAL("ok"), integrationResult.value().toStringBox());
}

INSTANTIATE_TEST_SUITE_P(ClientSQLRuntimeTests,
                         ClientSQLRuntimeFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
