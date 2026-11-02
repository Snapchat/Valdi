#include "JSBridgeTestFixture.hpp"
#include "RuntimeTestsUtils.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"
#include "gtest/gtest.h"

#include <future>
#include <thread>

using namespace Valdi;

namespace ValdiTest {

class CallbackAttributionFixture : public JSBridgeTestFixture {
protected:
    void SetUp() override {
        JSBridgeTestFixture::SetUp();
        if (!IsSkipped()) {
            wrapper = RuntimeWrapper(getJsBridge(), isWithTSN() ? TSNMode::Enabled : TSNMode::Disabled);
            runtime = wrapper.runtime->getJavaScriptRuntime();
            runtime->setANRDiagnosticsEnabled(true);
        }
    }

    void TearDown() override {
        wrapper.teardown();
    }

    Result<Value> evaluate(const std::string& script) {
        return runtime->evaluateScript(makeShared<ByteBuffer>(script)->toBytesView(), STRING_LITERAL("attribution.js"));
    }

    std::string invoke(const Ref<ValueFunction>& callback) {
        std::string observed;
        auto report = makeShared<ValueFunctionWithCallable>([&](const auto&) {
            observed = runtime->getANRAttributionInfo();
            return Value::undefined();
        });
        Result<Value> result;
        std::thread caller([&]() { result = callback->call(ValueFunctionFlagsCallSync, {Value(report)}); });
        caller.join();
        EXPECT_TRUE(result) << result.description();
        EXPECT_EQ("", runtime->getANRAttributionInfo());
        return observed;
    }

    RuntimeWrapper wrapper;
    JavaScriptRuntime* runtime = nullptr;
};

TEST_P(CallbackAttributionFixture, retainsIndependentOwnersForDelayedGlobalCallbacks) {
    auto install = evaluate(R"JS(
        return native => {
          const {withGlobalNativeRefs} = global.require('valdi_core/src/NativeReferences');
          for (const label of ['featureA:FeatureA.update', 'featureB:FeatureB.update']) {
            withGlobalNativeRefs(label, () => native.subscribe(report => report()));
          }
        };
    )JS");
    ASSERT_TRUE(install) << install.description();
    std::vector<Ref<ValueFunction>> callbacks;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callbacks.emplace_back(context.getParameter(0).getFunctionRef());
        return Value::undefined();
    });
    auto installed = install.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                            {Value().setMapValue("subscribe", Value(subscribe))});
    ASSERT_TRUE(installed) << installed.description();
    ASSERT_EQ(2U, callbacks.size());

    const std::string labels[] = {"featureA:FeatureA.update", "featureB:FeatureB.update"};
    for (size_t i : {0U, 1U, 0U}) {
        auto* bridged = dynamic_cast<ValueFunctionWithJSValue*>(callbacks[i].get());
        ASSERT_NE(nullptr, bridged);
        EXPECT_EQ(labels[i], bridged->getANRAttribution().slowToString());
        EXPECT_TRUE(bridged->getContext()->getPath().getResourceId().bundleName.isEmpty());
        auto observed = invoke(callbacks[i]);
        EXPECT_NE(std::string::npos, observed.find(labels[i])) << observed;
        EXPECT_EQ(std::string::npos, observed.find(labels[1 - i])) << observed;
    }
}

TEST_P(CallbackAttributionFixture, restoresNestedOwnerAfterExceptionAndPreservesReturnValue) {
    auto installer = evaluate(R"JS(
        return native => runtime.withCallbackAttribution('featureA:FeatureA.update', () => {
          try {
            runtime.withCallbackAttribution('featureB:FeatureB.update', () => {
              native.subscribe(report => report());
              throw new Error('expected');
            });
          } catch (_) {}
          native.subscribe(report => report());
          return 42;
        });
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    std::vector<Ref<ValueFunction>> callbacks;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callbacks.emplace_back(context.getParameter(0).getFunctionRef());
        return Value::undefined();
    });
    auto result = installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                           {Value().setMapValue("subscribe", Value(subscribe))});
    ASSERT_TRUE(result) << result.description();
    EXPECT_EQ(42, result.value().toInt());
    ASSERT_EQ(2U, callbacks.size());
    EXPECT_NE(std::string::npos, invoke(callbacks[0]).find("featureB:FeatureB.update"));
    auto outer = invoke(callbacks[1]);
    EXPECT_NE(std::string::npos, outer.find("featureA:FeatureA.update"));
    EXPECT_EQ(std::string::npos, outer.find("featureB:FeatureB.update"));
}

TEST_P(CallbackAttributionFixture, preservesDiagnosticBreadcrumbsThroughPromiseAndTimerContinuations) {
    auto installer = evaluate(R"JS(
        return native => runtime.withCallbackAttribution('featureA:FeatureA.update', () => {
          native.subscribe(report => {
            Promise.resolve().then(() => {
              runtime.scheduleWorkItem(() => report(), 0);
            });
          });
        });
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    Ref<ValueFunction> callback;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callback = context.getParameter(0).getFunctionRef();
        return Value::undefined();
    });
    auto installed = installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                              {Value().setMapValue("subscribe", Value(subscribe))});
    ASSERT_TRUE(installed) << installed.description();
    ASSERT_NE(nullptr, callback);

    std::promise<std::string> report;
    auto observed = report.get_future();
    auto receiver = makeShared<ValueFunctionWithCallable>([&](const auto&) {
        report.set_value(runtime->getANRAttributionInfo());
        return Value::undefined();
    });
    auto result = callback->call(ValueFunctionFlagsCallSync, {Value(receiver)});
    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(std::future_status::ready, observed.wait_for(std::chrono::seconds(30)));
    auto info = observed.get();
    EXPECT_NE(std::string::npos, info.find("featureA:FeatureA.update")) << info;
    runtime->dispatchSynchronouslyOnJsThread(STRING_LITERAL("test.barrier"), [](auto&) {});
    EXPECT_EQ("", runtime->getANRAttributionInfo());
}

TEST_P(CallbackAttributionFixture, doesNotCaptureGenericDispatchLabelsAsOwners) {
    auto installer = evaluate(R"JS(
        return native => native.subscribe(report => report());
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    auto* bridgedInstaller = dynamic_cast<ValueFunctionWithJSValue*>(installer.value().getFunctionRef().get());
    ASSERT_NE(nullptr, bridgedInstaller);
    EXPECT_TRUE(bridgedInstaller->getANRAttribution().isEmpty());

    Ref<ValueFunction> callback;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callback = context.getParameter(0).getFunctionRef();
        return Value::undefined();
    });
    auto installed = installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                              {Value().setMapValue("subscribe", Value(subscribe))});
    ASSERT_TRUE(installed) << installed.description();
    auto* bridged = dynamic_cast<ValueFunctionWithJSValue*>(callback.get());
    ASSERT_NE(nullptr, bridged);
    EXPECT_TRUE(bridged->getANRAttribution().isEmpty());
    auto observed = invoke(callback);
    EXPECT_NE(std::string::npos, observed.find("subscribe.<p0>")) << observed;
}

TEST_P(CallbackAttributionFixture, explicitProxyOwnerOverridesEnclosingOwner) {
    auto installer = evaluate(R"JS(
        return native => runtime.withCallbackAttribution('featureB:FeatureB.update', () => {
          native.subscribe(runtime.makeANRAttributionProxy('featureA:FeatureA.update', report => report()));
        });
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    Ref<ValueFunction> callback;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callback = context.getParameter(0).getFunctionRef();
        return Value::undefined();
    });
    ASSERT_TRUE(installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                         {Value().setMapValue("subscribe", Value(subscribe))}));
    auto* bridged = dynamic_cast<ValueFunctionWithJSValue*>(callback.get());
    ASSERT_NE(nullptr, bridged);
    EXPECT_EQ("featureA:FeatureA.update", bridged->getANRAttribution().slowToString());
    auto observed = invoke(callback);
    EXPECT_NE(std::string::npos, observed.find("featureA:FeatureA.update")) << observed;
    EXPECT_EQ(std::string::npos, observed.find("featureB:FeatureB.update")) << observed;
}

TEST_P(CallbackAttributionFixture, globalDelegateSurvivesOriginatingContextDestruction) {
    auto installer = evaluate(R"JS(
        return native => {
          const {NativeReferencesContext, withGlobalNativeRefs} = global.require('valdi_core/src/NativeReferences');
          const page = NativeReferencesContext.create();
          page.makeCurrent(() => {
            withGlobalNativeRefs('featureA:FeatureA.delegate', () => {
              native.subscribe({onUpdate(report) { report(); }});
            });
          });
          page.destroy();
        };
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    Ref<ValueFunction> callback;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callback = context.getParameter(0).getMapValue("onUpdate").getFunctionRef();
        return Value::undefined();
    });
    ASSERT_TRUE(installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                         {Value().setMapValue("subscribe", Value(subscribe))}));
    ASSERT_NE(nullptr, callback);
    EXPECT_NE(std::string::npos, invoke(callback).find("featureA:FeatureA.delegate"));
}

TEST_P(CallbackAttributionFixture, restoresOwnerAcrossReentrantCallbacks) {
    auto installer = evaluate(R"JS(
        return native => {
          for (const label of ['featureA:FeatureA.update', 'featureB:FeatureB.update']) {
            runtime.withCallbackAttribution(label, () => native.subscribe(report => {
              report();
              native.exportCallback(() => {});
            }));
          }
        };
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    std::vector<Ref<ValueFunction>> callbacks;
    std::vector<std::string> childOwners;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callbacks.emplace_back(context.getParameter(0).getFunctionRef());
        return Value::undefined();
    });
    auto exportCallback = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        auto* child = dynamic_cast<ValueFunctionWithJSValue*>(context.getParameter(0).getFunctionRef().get());
        EXPECT_NE(nullptr, child);
        if (child != nullptr) {
            childOwners.emplace_back(child->getANRAttribution().slowToString());
        }
        return Value::undefined();
    });
    auto native =
        Value().setMapValue("subscribe", Value(subscribe)).setMapValue("exportCallback", Value(exportCallback));
    ASSERT_TRUE(installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync, {native}));
    ASSERT_EQ(2U, callbacks.size());
    auto noop = makeShared<ValueFunctionWithCallable>([](const auto&) { return Value::undefined(); });
    auto reenter = makeShared<ValueFunctionWithCallable>([&](const auto&) {
        EXPECT_TRUE(callbacks[1]->call(ValueFunctionFlagsCallSync, {Value(noop)}));
        EXPECT_EQ("featureA:FeatureA.update", runtime->getCallbackAttribution().slowToString());
        return Value::undefined();
    });
    ASSERT_TRUE(callbacks[0]->call(ValueFunctionFlagsCallSync, {Value(reenter)}));
    runtime->dispatchSynchronouslyOnJsThread(STRING_LITERAL("test.barrier"), [](auto&) {});
    ASSERT_EQ(2U, childOwners.size());
    EXPECT_EQ("featureB:FeatureB.update", childOwners[0]);
    EXPECT_EQ("featureA:FeatureA.update", childOwners[1]);
}

TEST_P(CallbackAttributionFixture, capturesOwnerForCallbacksExportedByScheduledWork) {
    auto installer = evaluate(R"JS(
        return native => {
          const {withGlobalNativeRefs} = global.require('valdi_core/src/NativeReferences');
          withGlobalNativeRefs('featureA:FeatureA.update', () => {
            runtime.scheduleWorkItem(() => native.subscribe(report => report()), 0);
          });
        };
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    std::promise<Ref<ValueFunction>> exported;
    auto future = exported.get_future();
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        exported.set_value(context.getParameter(0).getFunctionRef());
        return Value::undefined();
    });
    ASSERT_TRUE(installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                         {Value().setMapValue("subscribe", Value(subscribe))}));
    ASSERT_EQ(std::future_status::ready, future.wait_for(std::chrono::seconds(30)));
    auto callback = future.get();
    auto* bridged = dynamic_cast<ValueFunctionWithJSValue*>(callback.get());
    ASSERT_NE(nullptr, bridged);
    EXPECT_EQ("featureA:FeatureA.update", bridged->getANRAttribution().slowToString());
    EXPECT_NE(std::string::npos, invoke(callback).find("featureA:FeatureA.update"));
}

TEST_P(CallbackAttributionFixture, preservesReturnValueAndRejectsEmptyLabel) {
    auto result = evaluate(R"JS(
        const {withGlobalNativeRefs} = global.require('valdi_core/src/NativeReferences');
        if (withGlobalNativeRefs('featureA:FeatureA.update', () => 42) !== 42) {
          throw new Error('Lost return value');
        }
        try {
          withGlobalNativeRefs('', () => { throw new Error('Unexpected callback'); });
          throw new Error('Empty label accepted');
        } catch (error) {
          if (error.message !== 'Global native references require attribution') { throw error; }
        }
        return 42;
    )JS");
    ASSERT_TRUE(result) << result.description();
    EXPECT_EQ(42, result.value().toInt());
}

TEST_P(CallbackAttributionFixture, diagnosticsDisabledSkipsLabelConversionAndPreservesCallbackBehavior) {
    runtime->setANRDiagnosticsEnabled(false);
    auto result = evaluate(R"JS(
        const label = {toString() { throw new Error('Unexpected label conversion'); }};
        if (runtime.withCallbackAttribution(label, () => 42) !== 42) {
          throw new Error('Lost return value');
        }
        const expected = new Error('Expected callback error');
        try {
          runtime.withCallbackAttribution(label, () => { throw expected; });
          throw new Error('Swallowed callback error');
        } catch (error) {
          if (error !== expected) { throw error; }
        }
        try {
          runtime.withCallbackAttribution(label, null);
          throw new Error('Invalid callback accepted');
        } catch (error) {
          if (error.message !== 'Expecting callback function') { throw error; }
        }
        return 42;
    )JS");
    ASSERT_TRUE(result) << result.description();
    EXPECT_EQ(42, result.value().toInt());
}

TEST_P(CallbackAttributionFixture, diagnosticsDisabledPreservesBehaviorWithoutCapturingOwner) {
    runtime->setANRDiagnosticsEnabled(false);
    auto installer = evaluate(R"JS(
        return native => {
          const {withGlobalNativeRefs} = global.require('valdi_core/src/NativeReferences');
          return withGlobalNativeRefs('featureA:FeatureA.update', () => {
            native.subscribe(report => report());
            return 42;
          });
        };
    )JS");
    ASSERT_TRUE(installer) << installer.description();
    Ref<ValueFunction> callback;
    auto subscribe = makeShared<ValueFunctionWithCallable>([&](const auto& context) {
        callback = context.getParameter(0).getFunctionRef();
        return Value::undefined();
    });
    auto installed = installer.value().getFunctionRef()->call(ValueFunctionFlagsCallSync,
                                                              {Value().setMapValue("subscribe", Value(subscribe))});
    ASSERT_TRUE(installed) << installed.description();
    EXPECT_EQ(42, installed.value().toInt());
    auto* bridged = dynamic_cast<ValueFunctionWithJSValue*>(callback.get());
    ASSERT_NE(nullptr, bridged);
    EXPECT_TRUE(bridged->getANRAttribution().isEmpty());
    EXPECT_EQ("", invoke(callback));
}

INSTANTIATE_TEST_SUITE_P(CallbackAttributionTests,
                         CallbackAttributionFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::QuickJSWithTSN,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
