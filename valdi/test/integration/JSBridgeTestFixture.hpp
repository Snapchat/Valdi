#pragma once
#include "valdi/jsbridge/JavaScriptBridge.hpp"
#include "gtest/gtest.h"
#include <ostream>
#include <string>

namespace ValdiTest {

enum class JavaScriptEngineTestCase { JSCore, Hermes, V8, QuickJS, QuickJSWithTSN };

struct PrintJavaScriptEngineType {
    std::string operator()(const testing::TestParamInfo<JavaScriptEngineTestCase>& info) const;
};

class JSBridgeTestFixture : public ::testing::TestWithParam<JavaScriptEngineTestCase> {
public:
    // Skips the test when the parameterized JS engine isn't compiled into this build
    // type (e.g. JSCore is absent on external Linux builds, which use Hermes/QuickJS).
    // Requesting an unavailable engine otherwise aborts in JavaScriptBridge.
    void SetUp() override;

    Valdi::IJavaScriptBridge* getJsBridge() const;

    bool isHermes() const;
    bool isJSCore() const;
    bool isV8() const;
    bool isQuickJS() const;

    bool isWithTSN() const;
};

}; // namespace ValdiTest
