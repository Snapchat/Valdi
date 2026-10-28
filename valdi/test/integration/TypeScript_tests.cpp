#include "JSBridgeTestFixture.hpp"
#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "valdi_test_utils.hpp"
#include "gtest/gtest.h"
#include <iostream>

using namespace Valdi;
using namespace snap::valdi;

namespace ValdiTest {

class TypeScriptTestFixture : public JSBridgeTestFixture {
protected:
    void runTest(std::string_view filePath) {
        auto success = runTypeScriptTests(filePath, getJsBridge(), nullptr);
        ASSERT_TRUE(success);
    }
};

TEST_P(TypeScriptTestFixture, canRunProtobufTests) {
    runTest("valdi_protobuf/test/Test.spec");
}

TEST_P(TypeScriptTestFixture, canRunPersistentStoreTests) {
    runTest("persistence/test/PersistentStoreTest");
}

TEST_P(TypeScriptTestFixture, canRunModuleLoaderUnloadTests) {
    runTest("valdi_core/test/ModuleLoaderUnloadTest");
}

INSTANTIATE_TEST_SUITE_P(TypeScriptTests,
                         TypeScriptTestFixture,
                         ::testing::Values(JavaScriptEngineTestCase::QuickJS),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
