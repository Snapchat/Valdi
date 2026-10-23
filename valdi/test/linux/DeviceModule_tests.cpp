#include "valdi/standalone_runtime/BridgeModules/DeviceModule.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

namespace ValdiTest {
using namespace Valdi;

static Value observe(const Ref<DeviceModule>& device, ValueFunctionCallable callback) {
    auto result = device->loadModule().getMapValue("observeDarkMode").getFunctionRef()->call(
        ValueFunctionFlagsNone, {Value(makeShared<ValueFunctionWithCallable>(std::move(callback)))});
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
    auto a = observe(device, [&](const auto& call) { first.push_back(call.getParameterAsBool(0)); return Value(); });
    auto b = observe(device, [&](const auto& call) { second.push_back(call.getParameterAsBool(0)); return Value(); });
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
    auto second = observe(device, [&](const auto& call) { values.push_back(call.getParameterAsBool(0)); return Value(); });
    device->setDarkMode(true);
    EXPECT_EQ(values, std::vector<bool>({false, false})); // No stale outer true after nested false.
    cancel(second);
}

TEST(StandaloneDevice, HostThreadCanPublishAndObserverCanCancel) {
    auto device = makeShared<DeviceModule>();
    std::vector<bool> values;
    auto subscription = observe(device, [&](const auto& call) { values.push_back(call.getParameterAsBool(0)); return Value(); });
    std::thread host([&] { for (int i = 0; i < 100; ++i) device->setDarkMode(i % 2 == 0); });
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
} // namespace ValdiTest
