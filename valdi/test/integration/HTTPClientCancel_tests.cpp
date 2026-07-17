#include "RequestManagerMock.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"

#include "JSBridgeTestFixture.hpp"
#include "RuntimeTestsUtils.hpp"
#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

class HTTPClientCancelFixture : public JSBridgeTestFixture {
protected:
    void SetUp() override {
        wrapper = RuntimeWrapper(getJsBridge(), TSNMode::Disabled);
    }

    void TearDown() override {
        wrapper.teardown();
    }

    RuntimeWrapper wrapper;
};

// The native performRequest must return a bare cancel function (matching NativeHTTPClient.d.ts), so
// HTTPClient.ts's `() => cancelFn?.()` can call it. Before the fix the C++ factory returns a
// { cancel: fn } object, so `typeof` is "object" and calling it throws "cancel is not a function".
TEST_P(HTTPClientCancelFixture, performRequestCancelHandleIsCallable) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(*wrapper.logger);
    wrapper.runtimeManager->setRequestManager(requestManager);
    // Give the request a clean completion so the mock's async queue doesn't surface a
    // "No mocked response" error if it runs before the synchronous cancel() below.
    requestManager->addMockedResponse(STRING_LITERAL("http://localhost/"), STRING_LITERAL("GET"), BytesView());

    std::string js =
        "var m = global.require('valdi_http/src/NativeHTTPClient');"
        "var cancel = m.performRequest("
        "  { url: 'http://localhost/', method: 'GET', headers: {} }, function () {});"
        "var kind = typeof cancel;"
        "cancel();" // nulls the mocked request's completion before the mock queue can fire it
        "return kind;";

    auto result = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(js)->toBytesView(), STRING_LITERAL("http_cancel_test.js"));

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ("function", result.value().toString());
}

INSTANTIATE_TEST_SUITE_P(HTTPClientCancelTests,
                         HTTPClientCancelFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
