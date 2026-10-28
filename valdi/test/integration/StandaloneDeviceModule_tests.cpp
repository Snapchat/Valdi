#include "JSBridgeTestFixture.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/standalone_runtime/BridgeModules/DeviceModule.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <atomic>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

namespace ValdiTest {
using namespace Valdi;

static Value observe(const Ref<DeviceModule>& device, ValueFunctionCallable callback) {
    auto result =
        device->loadModule()
            .getMapValue("observeDarkMode")
            .getFunctionRef()
            ->call(ValueFunctionFlagsNone, {Value(makeShared<ValueFunctionWithCallable>(std::move(callback)))});
    EXPECT_TRUE(result.success());
    return result.value();
}

static void cancel(const Value& subscription) {
    EXPECT_TRUE(subscription.getMapValue("cancel").getFunctionRef()->call(ValueFunctionFlagsNone, {}).success());
}

TEST(StandaloneDevice, InitialAppearanceAndChanges) {
    auto device = makeShared<DeviceModule>();
    std::vector<bool> values;
    auto subscription = observe(device, [&](const auto& call) {
        values.push_back(call.getParameterAsBool(0));
        return Value();
    });
    EXPECT_EQ(values, std::vector<bool>({false}));
    device->setDarkMode(true);
    device->setDarkMode(true);
    device->setDarkMode(false);
    EXPECT_EQ(values, std::vector<bool>({false, true, false}));
    cancel(subscription);
    cancel(subscription);
    device->setDarkMode(true);
    EXPECT_EQ(values.size(), 3u);
}

TEST(StandaloneDevice, SeededAppearanceAndIndependentSubscriptions) {
    auto device = makeShared<DeviceModule>();
    device->setDarkMode(true);
    std::vector<bool> first, second;
    auto a = observe(device, [&](const auto& call) {
        first.push_back(call.getParameterAsBool(0));
        return Value();
    });
    auto b = observe(device, [&](const auto& call) {
        second.push_back(call.getParameterAsBool(0));
        return Value();
    });
    cancel(a);
    device->setDarkMode(false);
    EXPECT_EQ(first, std::vector<bool>({true}));
    EXPECT_EQ(second, std::vector<bool>({true, false}));
    cancel(b);
}

TEST(StandaloneDevice, CancellationReleasesObserverAndOutlivesModule) {
    Value subscription;
    std::weak_ptr<int> weak;
    {
        auto device = makeShared<DeviceModule>();
        auto payload = std::make_shared<int>(0);
        weak = payload;
        subscription = observe(device, [payload](const auto&) { return Value(); });
        payload.reset();
        EXPECT_FALSE(weak.expired());
        cancel(subscription);
        EXPECT_TRUE(weak.expired());
    }
    cancel(subscription);
}

TEST(StandaloneDevice, NativeObserverCanCancelAndPublishDuringNotification) {
    auto device = makeShared<DeviceModule>();
    Value first;
    std::vector<bool> values;
    first = observe(device, [&](const auto& call) {
        if (call.getParameterAsBool(0)) {
            cancel(first);
            device->setDarkMode(false);
        }
        return Value();
    });
    auto second = observe(device, [&](const auto& call) {
        values.push_back(call.getParameterAsBool(0));
        return Value();
    });
    device->setDarkMode(true);
    EXPECT_EQ(values, std::vector<bool>({false, false})); // No stale outer true after nested false.
    cancel(second);
}

TEST(StandaloneDevice, HostThreadCanPublish) {
    auto device = makeShared<DeviceModule>();
    std::vector<bool> values;
    auto subscription = observe(device, [&](const auto& call) {
        values.push_back(call.getParameterAsBool(0));
        return Value();
    });
    std::thread host([&] {
        for (int i = 0; i < 100; ++i)
            device->setDarkMode(i % 2 == 0);
    });
    host.join();
    EXPECT_EQ(values.size(), 101u);
    cancel(subscription);
}

TEST(StandaloneDevice, InvalidObserverIsRejected) {
    auto device = makeShared<DeviceModule>();
    auto observer = device->loadModule().getMapValue("observeDarkMode").getFunctionRef();
    EXPECT_FALSE(observer->call(ValueFunctionFlagsNone, {}).success());
    EXPECT_FALSE(observer->call(ValueFunctionFlagsNone, {Value(true)}).success());
}

// These tests use the actual JS callback wrapper and standalone runtime. Native
// ValueFunctionWithCallable callbacks above do not exercise JS scheduling or VM teardown.
class StandaloneDeviceRuntimeFixture : public JSBridgeTestFixture {
protected:
    Ref<ValdiStandaloneRuntime> createRuntime(bool darkMode = false) {
        StandaloneArguments arguments;
        arguments.jsBridge = getJsBridge();
        arguments.logLevel = LogTypeError;
        auto runtime = createValdiStandaloneRuntime(arguments);
        runtime->setDarkMode(darkMode);
        return runtime;
    }

    Result<Value> evaluate(ValdiStandaloneRuntime& runtime, const char* source) {
        return runtime.getRuntime().getJavaScriptRuntime()->evaluateScript(
            makeShared<ByteBuffer>(source)->toBytesView(), STRING_LITERAL("standalone_device_test.js"));
    }
};

TEST_P(StandaloneDeviceRuntimeFixture, InitialJavaScriptObserverRunsBeforeRegistrationReturns) {
    for (bool darkMode : {false, true}) {
        auto runtime = createRuntime(darkMode);
        auto result = evaluate(*runtime,
                               "var bridge = global.require('valdi_core/src/DeviceBridge');"
                               "var events = ['before'];"
                               "var subscription = bridge.observeDarkMode(function (dark) {"
                               "  events.push(dark ? 'dark' : 'light');"
                               "});"
                               "events.push('after');"
                               "subscription.cancel();"
                               "return events.join(',');");
        ASSERT_TRUE(result) << result.description();
        EXPECT_EQ(result.value().toString(), darkMode ? "before,dark,after" : "before,light,after");
    }
}

TEST_P(StandaloneDeviceRuntimeFixture, HostUpdatesReachJavaScriptUntilCancellation) {
    auto runtime = createRuntime();
    auto initial = evaluate(*runtime,
                            "global.appearanceValues = [];"
                            "global.appearanceSubscription = global.require('valdi_core/src/DeviceBridge')"
                            "  .observeDarkMode(function (dark) { global.appearanceValues.push(dark); });"
                            "return JSON.stringify(global.appearanceValues);");
    ASSERT_TRUE(initial) << initial.description();
    EXPECT_EQ(initial.value().toString(), "[false]");

    runtime->setDarkMode(true);
    runtime->setDarkMode(true);
    auto changed = evaluate(*runtime, "return JSON.stringify(global.appearanceValues);");
    ASSERT_TRUE(changed) << changed.description();
    EXPECT_EQ(changed.value().toString(), "[false,true]");

    auto cancelled = evaluate(*runtime, "global.appearanceSubscription.cancel(); return 'cancelled';");
    ASSERT_TRUE(cancelled) << cancelled.description();
    runtime->setDarkMode(false);
    auto afterCancel = evaluate(*runtime, "return JSON.stringify(global.appearanceValues);");
    ASSERT_TRUE(afterCancel) << afterCancel.description();
    EXPECT_EQ(afterCancel.value().toString(), "[false,true]");
}

TEST_P(StandaloneDeviceRuntimeFixture, RuntimeTeardownReleasesUncancelledJavaScriptObserver) {
    for (int cycle = 0; cycle < 8; ++cycle) {
        std::weak_ptr<int> observerLifetime;
        std::atomic<int> callbacks = 0;
        // These outlive runtime even if an assertion returns early.
        auto runtime = createRuntime();
        auto* js = runtime->getRuntime().getJavaScriptRuntime();
        {
            auto lifetime = std::make_shared<int>(cycle);
            observerLifetime = lifetime;
            auto callback = makeShared<ValueFunctionWithCallable>([lifetime, &callbacks](const auto&) {
                ++callbacks;
                return Value();
            });
            js->setValueToGlobalObject(STRING_LITERAL("observerLifetime"), Value(callback));
        }
        {
            auto subscribed = evaluate(*runtime,
                                       "(function (keepAlive) {"
                                       "  global.require('valdi_core/src/DeviceBridge').observeDarkMode(function () {"
                                       "    keepAlive();"
                                       "  });"
                                       "})(global.observerLifetime);"
                                       "delete global.observerLifetime;"
                                       "return 'subscribed';");
            ASSERT_TRUE(subscribed) << subscribed.description();
        }
        // Only the uncancelled JS closure should keep the native payload alive.
        js->performGc();
        js->getJsDispatchQueue()->sync([]() {});
        EXPECT_FALSE(observerLifetime.expired());
        EXPECT_EQ(callbacks.load(), 1);

        // Also exercise teardown with a host notification queued. No subscription
        // or JS Value escapes the runtime's scope, and the observer never cancels.
        runtime->setDarkMode(true);
        js = nullptr;
        runtime = nullptr;
        EXPECT_TRUE(observerLifetime.expired()) << "observer retained after runtime teardown, cycle " << cycle;
    }
}

INSTANTIATE_TEST_SUITE_P(StandaloneDeviceRuntimeTests,
                         StandaloneDeviceRuntimeFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());
} // namespace ValdiTest
