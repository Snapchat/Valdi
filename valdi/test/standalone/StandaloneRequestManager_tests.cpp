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

TEST_P(StandaloneRequestManagerFixture, performRequestReportsSuccessToJavaScript) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(ConsoleLogger::getLogger());
    requestManager->addMockedResponse(STRING_LITERAL("http://localhost/"), STRING_LITERAL("GET"), BytesView());

    auto arguments = makeArguments();
    arguments.requestManager = requestManager;

    auto standaloneRuntime = createValdiStandaloneRuntime(arguments);
    auto* jsRuntime = standaloneRuntime->getRuntime().getJavaScriptRuntime();

    auto evaluate = [&](const std::string& source) {
        return jsRuntime->evaluateScript(makeShared<ByteBuffer>(source)->toBytesView(),
                                         STRING_LITERAL("standalone_request_manager_test.js"));
    };

    auto started = evaluate("var m = global.require('valdi_http/src/NativeHTTPClient');"
                            "global.__outcome = '<callback never invoked>';"
                            "m.performRequest({ url: 'http://localhost/', method: 'GET', headers: {} },"
                            "  function (response, error) {"
                            "    global.__outcome = response ? 'status ' + response.statusCode : 'error: ' + error;"
                            "  });"
                            "return 'started';");
    ASSERT_TRUE(started) << started.description();

    requestManager->getAllPerformedTasks();

    auto outcome = evaluate("return global.__outcome;");
    ASSERT_TRUE(outcome) << outcome.description();
    // The counterpart to the failure test below: nothing else asserts that a successful response
    // actually reaches the callback, so stringifying the wrong parameter would go unnoticed.
    EXPECT_EQ("status 200", outcome.value().toString()) << "a successful response must reach the JavaScript callback";
}

TEST_P(StandaloneRequestManagerFixture, performRequestReportsFailureToJavaScript) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(ConsoleLogger::getLogger());
    // Deliberately no mocked response, so the mock fails the request.

    auto arguments = makeArguments();
    arguments.requestManager = requestManager;

    auto standaloneRuntime = createValdiStandaloneRuntime(arguments);
    auto* jsRuntime = standaloneRuntime->getRuntime().getJavaScriptRuntime();

    auto evaluate = [&](const std::string& source) {
        return jsRuntime->evaluateScript(makeShared<ByteBuffer>(source)->toBytesView(),
                                         STRING_LITERAL("standalone_request_manager_test.js"));
    };

    auto started = evaluate("var m = global.require('valdi_http/src/NativeHTTPClient');"
                            "global.__outcome = '<callback never invoked>';"
                            "m.performRequest({ url: 'http://localhost/missing', method: 'GET', headers: {} },"
                            "  function (response, error) {"
                            "    global.__outcome = response ? 'response' : 'error: ' + error;"
                            "  });"
                            "return 'started';");
    ASSERT_TRUE(started) << started.description();
    ASSERT_EQ("started", started.value().toString());

    // Drains the mock's queue, so the completion — and with it the JavaScript callback — has run.
    requestManager->getAllPerformedTasks();

    auto outcome = evaluate("return global.__outcome;");
    ASSERT_TRUE(outcome) << outcome.description();
    EXPECT_EQ("error: No mocked response for given request", outcome.value().toString())
        << "a failed request must reach the JavaScript callback. Handing the error over as a Value "
           "holding an Error raises it while the arguments are being marshalled instead, so the "
           "callback is never invoked at all and the promise behind it stays pending forever";
}

// The contract callers actually depend on, and the one that hung: a failed request must settle the
// promise HTTPClient hands back, not abandon it.
TEST_P(StandaloneRequestManagerFixture, httpClientRejectsItsPromiseOnFailure) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(ConsoleLogger::getLogger());
    // Deliberately no mocked response, so the mock fails the request.

    auto arguments = makeArguments();
    arguments.requestManager = requestManager;

    auto standaloneRuntime = createValdiStandaloneRuntime(arguments);
    auto* jsRuntime = standaloneRuntime->getRuntime().getJavaScriptRuntime();

    auto evaluate = [&](const std::string& source) {
        return jsRuntime->evaluateScript(makeShared<ByteBuffer>(source)->toBytesView(),
                                         STRING_LITERAL("standalone_request_manager_test.js"));
    };

    auto started = evaluate("var HTTPClient = global.require('valdi_http/src/HTTPClient').HTTPClient;"
                            "global.__outcome = '<promise never settled>';"
                            "new HTTPClient().get('http://localhost/missing').then("
                            "  function () { global.__outcome = 'resolved'; },"
                            "  function (e) { global.__outcome = 'rejected: ' + e; });"
                            "return 'started';");
    ASSERT_TRUE(started) << started.description();

    requestManager->getAllPerformedTasks();

    auto outcome = evaluate("return global.__outcome;");
    ASSERT_TRUE(outcome) << outcome.description();
    EXPECT_EQ("rejected: No mocked response for given request", outcome.value().toString())
        << "the promise was left pending, which is what hangs a CLI: beginKeepAlive holds the runtime "
           "open waiting for a settlement that never comes";
}

INSTANTIATE_TEST_SUITE_P(StandaloneRequestManagerTests,
                         StandaloneRequestManagerFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
