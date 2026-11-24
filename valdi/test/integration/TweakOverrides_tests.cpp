#include "JSBridgeTestFixture.hpp"
#include "RuntimeTestsUtils.hpp"
#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/Interfaces/TweakOverrideProvider.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "gtest/gtest.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

using namespace Valdi;

namespace ValdiTest {

namespace {

constexpr const char* kAccessibilityKey = "VALDI_ENABLE_ACCESSIBILITY_TRAITS";

class HostTweakProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    StringBox getString(const StringBox& key, const StringBox& fallback) override {
        return fallback;
    }
    bool getBool(const StringBox& key, bool fallback) override {
        return key == StringBox::fromCString(kAccessibilityKey) ? true : fallback;
    }
    float getFloat(const StringBox& key, float fallback) override {
        return fallback;
    }
    int32_t getInt(const StringBox& key, int32_t fallback) override {
        return fallback;
    }
    Value getBinary(const StringBox& key, const Value& fallback) override {
        return fallback;
    }
};

class BlockingHostProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    static thread_local bool blockCallsOnThisThread;

    void waitUntilBlocked() {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [&]() { return _blocked; });
    }

    void release() {
        std::lock_guard<std::mutex> lock(_mutex);
        _released = true;
        _condition.notify_all();
    }

    StringBox getString(const StringBox& key, const StringBox& fallback) override {
        return fallback;
    }
    bool getBool(const StringBox& key, bool fallback) override {
        if (blockCallsOnThisThread) {
            std::unique_lock<std::mutex> lock(_mutex);
            _blocked = true;
            _condition.notify_all();
            _condition.wait(lock, [&]() { return _released; });
        }
        return fallback;
    }
    float getFloat(const StringBox& key, float fallback) override {
        return fallback;
    }
    int32_t getInt(const StringBox& key, int32_t fallback) override {
        return fallback;
    }
    Value getBinary(const StringBox& key, const Value& fallback) override {
        return fallback;
    }

private:
    std::mutex _mutex;
    std::condition_variable _condition;
    bool _blocked = false;
    bool _released = false;
};

thread_local bool BlockingHostProvider::blockCallsOnThisThread = false;

class ScopedEnvironment {
public:
    ScopedEnvironment(const char* name, const char* value) : _name(name) {
        setenv(name, value, 1);
    }
    ~ScopedEnvironment() {
        unsetenv(_name);
    }

private:
    const char* _name;
};

} // namespace

class TweakOverridesFixture : public JSBridgeTestFixture {
protected:
    void TearDown() override {
        wrapper.teardown();
    }

    void create(Shared<ITweakValueProvider> provider = nullptr) {
        wrapper = RuntimeWrapper(getJsBridge(), TSNMode::Disabled, false, std::move(provider));
    }

    bool deferredGC() {
        return wrapper.runtime->getResourceManager().enableDeferredGC();
    }

    bool accessibility() {
        return wrapper.runtime->getResourceManager().enableAccessibility();
    }

    RuntimeWrapper wrapper;
};

TEST_P(TweakOverridesFixture, noOverridesLeavesTheDefaults) {
    create();

    ASSERT_FALSE(deferredGC());
    ASSERT_FALSE(accessibility());
}

TEST_P(TweakOverridesFixture, overridesApplyWithoutAHostProvider) {
    create();

    wrapper.runtimeManager->setTweakOverrides(TweakOverrideProvider::parse("VALDI_ENABLE_DEFERRED_GC=1"));

    ASSERT_TRUE(deferredGC());
    ASSERT_FALSE(accessibility());
}

TEST_P(TweakOverridesFixture, overridesWinOverTheHostProviderAndOtherKeysStillReachIt) {
    create(makeShared<HostTweakProvider>().toShared());
    ASSERT_TRUE(accessibility());
    ASSERT_FALSE(deferredGC());

    wrapper.runtimeManager->setTweakOverrides(
        TweakOverrideProvider::parse("VALDI_ENABLE_DEFERRED_GC=true,VALDI_ENABLE_ACCESSIBILITY_TRAITS=0"));

    ASSERT_TRUE(deferredGC());
    ASSERT_FALSE(accessibility());
}

TEST_P(TweakOverridesFixture, clearingOverridesRestoresTheHostProvider) {
    create(makeShared<HostTweakProvider>().toShared());
    wrapper.runtimeManager->setTweakOverrides(TweakOverrideProvider::parse("VALDI_ENABLE_ACCESSIBILITY_TRAITS=0"));
    ASSERT_FALSE(accessibility());

    wrapper.runtimeManager->setTweakOverrides({});

    ASSERT_TRUE(accessibility());
}

TEST_P(TweakOverridesFixture, aHostProviderSetAfterOverridesDoesNotLoseThem) {
    create();
    wrapper.runtimeManager->setTweakOverrides(TweakOverrideProvider::parse("VALDI_ENABLE_DEFERRED_GC=1"));

    wrapper.runtimeManager->setTweakValueProvider(makeShared<HostTweakProvider>().toShared());

    ASSERT_TRUE(deferredGC());
    ASSERT_TRUE(accessibility());
}

// Pauses one apply after it has published its tweaks and before it has pushed them to the runtimes, then runs a
// second update. Without serializing the applies, the paused one resumes last and overwrites the newer result
// with a stale one that has lost the overrides.
TEST_P(TweakOverridesFixture, aPausedApplyDoesNotOverwriteANewerUpdate) {
    create();
    auto blocking = makeShared<BlockingHostProvider>();

    std::thread first([&]() {
        BlockingHostProvider::blockCallsOnThisThread = true;
        wrapper.runtimeManager->setTweakValueProvider(blocking.toShared());
    });
    blocking->waitUntilBlocked();

    std::thread second([&]() {
        wrapper.runtimeManager->setTweakOverrides(TweakOverrideProvider::parse("VALDI_ENABLE_DEFERRED_GC=1"));
    });
    // Without serialization the second update finishes while the first is paused; with it, it waits for the first.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    blocking->release();
    first.join();
    second.join();

    ASSERT_TRUE(deferredGC());
}

TEST_P(TweakOverridesFixture, settingUnchangedOverridesLeavesTheRuntimeTweaksInstanceAlone) {
    create(makeShared<HostTweakProvider>().toShared());
    auto before = wrapper.runtime->getResourceManager().enableAccessibility();

    wrapper.runtimeManager->setTweakOverrides({});
    wrapper.runtimeManager->setTweakOverrides({});

    ASSERT_EQ(before, accessibility());
    ASSERT_TRUE(accessibility());
}

TEST_P(TweakOverridesFixture, theEnvironmentVariableIsReadWhenTheRuntimeManagerIsCreated) {
    ScopedEnvironment environment("VALDI_TWEAKS", "VALDI_ENABLE_DEFERRED_GC=1");

    create();

    ASSERT_TRUE(deferredGC());
}

TEST_P(TweakOverridesFixture, theEnvironmentVariableWinsOverConfiguredOverrides) {
    ScopedEnvironment environment("VALDI_TWEAKS", "VALDI_ENABLE_DEFERRED_GC=0");
    create();

    wrapper.runtimeManager->setTweakOverrides(TweakOverrideProvider::parse("VALDI_ENABLE_DEFERRED_GC=1"));

    ASSERT_FALSE(deferredGC());
}

INSTANTIATE_TEST_SUITE_P(TweakOverrides,
                         TweakOverridesFixture,
                         ::testing::Values(JavaScriptEngineTestCase::QuickJS, JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
