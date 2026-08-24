#include "JSBridgeTestFixture.hpp"

namespace ValdiTest {

static snap::valdi_core::JavaScriptEngineType toJSEngineType(JavaScriptEngineTestCase testCase) {
    switch (testCase) {
        case JavaScriptEngineTestCase::JSCore:
            return snap::valdi_core::JavaScriptEngineType::JSCore;
        case JavaScriptEngineTestCase::Hermes:
            return snap::valdi_core::JavaScriptEngineType::Hermes;
        case JavaScriptEngineTestCase::V8:
            return snap::valdi_core::JavaScriptEngineType::V8;
        case JavaScriptEngineTestCase::QuickJS:
            return snap::valdi_core::JavaScriptEngineType::QuickJS;
        case JavaScriptEngineTestCase::QuickJSWithTSN:
            return snap::valdi_core::JavaScriptEngineType::QuickJS;
    }
}

static bool isWithTSN(JavaScriptEngineTestCase testCase) {
    switch (testCase) {
        case JavaScriptEngineTestCase::QuickJSWithTSN:
            return true;
        default:
            return false;
    }
}

std::string PrintJavaScriptEngineType::operator()(const testing::TestParamInfo<JavaScriptEngineTestCase>& info) const {
    auto str = std::string(snap::valdi_core::to_string(toJSEngineType(info.param)));

    if (isWithTSN(info.param)) {
        str += "WithTSN";
    }

    return str;
}

bool JSBridgeTestFixture::isHermes() const {
    return GetParam() == JavaScriptEngineTestCase::Hermes;
}

bool JSBridgeTestFixture::isJSCore() const {
    return GetParam() == JavaScriptEngineTestCase::JSCore;
}

bool JSBridgeTestFixture::isV8() const {
    return GetParam() == JavaScriptEngineTestCase::V8;
}

bool JSBridgeTestFixture::isQuickJS() const {
    return GetParam() == JavaScriptEngineTestCase::QuickJS || GetParam() == JavaScriptEngineTestCase::QuickJSWithTSN;
}

bool JSBridgeTestFixture::isWithTSN() const {
    return ValdiTest::isWithTSN(GetParam());
}

void JSBridgeTestFixture::SetUp() {
    auto engineType = toJSEngineType(GetParam());
    if (!Valdi::JavaScriptBridge::isAvailable(engineType)) {
        GTEST_SKIP() << snap::valdi_core::to_string(engineType) << " is not compiled into this build type";
    }
}

Valdi::IJavaScriptBridge* JSBridgeTestFixture::getJsBridge() const {
    return Valdi::JavaScriptBridge::get(toJSEngineType(GetParam()));
}

} // namespace ValdiTest
