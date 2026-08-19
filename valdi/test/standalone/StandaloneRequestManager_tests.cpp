#include "RequestManagerMock.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include "valdi/test/integration/JSBridgeTestFixture.hpp"

#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

// A CLI app reaches valdi_http through createValdiStandaloneRuntime, which had no way to install a
// request manager, so performRequest always failed with "No RequestManager set".
class StandaloneRequestManagerFixture : public JSBridgeTestFixture {
protected:
    StandaloneArguments makeArguments() {
        StandaloneArguments arguments;
        arguments.jsBridge = getJsBridge();
        arguments.logLevel = LogTypeError;
        return arguments;
    }
};

TEST_P(StandaloneRequestManagerFixture, isNullWhenNotSupplied) {
    auto standaloneRuntime = createValdiStandaloneRuntime(makeArguments());

    ASSERT_EQ(standaloneRuntime->getRuntime().getRequestManager(), nullptr);
}

TEST_P(StandaloneRequestManagerFixture, reachesTheRuntimeWhenSupplied) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(ConsoleLogger::getLogger());

    auto arguments = makeArguments();
    arguments.requestManager = requestManager;

    auto standaloneRuntime = createValdiStandaloneRuntime(arguments);

    ASSERT_EQ(standaloneRuntime->getRuntime().getRequestManager(), requestManager);
}

// The end-to-end shape a CLI app exercises: performRequest resolves instead of throwing.
TEST_P(StandaloneRequestManagerFixture, performRequestSucceedsFromJavaScript) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(ConsoleLogger::getLogger());
    requestManager->addMockedResponse(STRING_LITERAL("http://localhost/"), STRING_LITERAL("GET"), BytesView());

    auto arguments = makeArguments();
    arguments.requestManager = requestManager;

    auto standaloneRuntime = createValdiStandaloneRuntime(arguments);

    std::string js = "var m = global.require('valdi_http/src/NativeHTTPClient');"
                     "try {"
                     "  m.performRequest({ url: 'http://localhost/', method: 'GET', headers: {} }, function () {});"
                     "  return 'ok';"
                     "} catch (e) {"
                     "  return String(e);"
                     "}";

    auto result = standaloneRuntime->getRuntime().getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(js)->toBytesView(), STRING_LITERAL("standalone_request_manager_test.js"));

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ("ok", result.value().toString());
}

INSTANTIATE_TEST_SUITE_P(StandaloneRequestManagerTests,
                         StandaloneRequestManagerFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
