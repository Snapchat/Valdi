//
//  ResourceManager_tests.cpp
//  ValdiRuntime
//

#include "valdi/runtime/Interfaces/IResourceLoader.hpp"
#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/Resources/AssetLoaderManager.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Holder.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi_test_utils.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

// Stands in for a platform resource loader whose first module load is slow (e.g. Dynamic Delivery
// archive initialization). Blocks every valdi_api_version load until release() is called.
class BlockingResourceLoader : public IResourceLoader {
public:
    Result<BytesView> loadModuleContent(const StringBox& module) override {
        if (module != STRING_LITERAL("valdi_api_version")) {
            return Error("Not found");
        }
        _loadsStarted++;
        std::unique_lock<std::mutex> lock(_mutex);
        _released.wait(lock, [this]() { return _isReleased; });
        return makeShared<ByteBuffer>(std::string("42"))->toBytesView();
    }

    StringBox resolveLocalAssetURL(const StringBox& /*moduleName*/, const StringBox& /*resourcePath*/) override {
        return StringBox();
    }

    void waitUntilLoadStarted() const {
        while (_loadsStarted.load() == 0) {
            std::this_thread::yield();
        }
    }

    void release() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _isReleased = true;
        }
        _released.notify_all();
    }

private:
    std::atomic<int> _loadsStarted{0};
    std::mutex _mutex;
    std::condition_variable _released;
    bool _isReleased = false;
};

static Ref<ResourceManager> makeTestResourceManager(const Shared<IResourceLoader>& resourceLoader,
                                                    const Ref<DispatchQueue>& workerQueue,
                                                    MainThreadManager& mainThreadManager) {
    Holder<Shared<snap::valdi_core::HTTPRequestManager>> requestManagerHolder;
    return makeShared<ResourceManager>(resourceLoader,
                                       makeShared<InMemoryDiskCache>(),
                                       makeShared<AssetLoaderManager>(),
                                       requestManagerHolder,
                                       workerQueue,
                                       mainThreadManager,
                                       1.0,
                                       false,
                                       ConsoleLogger::getLogger());
}

// Fails every load, except `available` paths, and records which paths were requested.
class RecordingResourceLoader : public IResourceLoader {
public:
    Result<BytesView> loadModuleContent(const StringBox& module) override {
        std::lock_guard<std::mutex> lock(_mutex);
        _requested.emplace_back(module.toStringView());
        if (std::find(available.begin(), available.end(), module.toStringView()) != available.end()) {
            return BytesView(nullptr, reinterpret_cast<const Byte*>("{}"), 2);
        }
        return Error("Not found");
    }

    std::vector<std::string> available;

    StringBox resolveLocalAssetURL(const StringBox& /*moduleName*/, const StringBox& /*resourcePath*/) override {
        return StringBox();
    }

    bool wasRequested(std::string_view path) const {
        std::lock_guard<std::mutex> lock(_mutex);
        return std::find(_requested.begin(), _requested.end(), path) != _requested.end();
    }

private:
    mutable std::mutex _mutex;
    std::vector<std::string> _requested;
};

struct RecordingResourceManagerWrapper {
    Ref<MainQueue> mainQueue = makeShared<MainQueue>();
    Ref<DispatchQueue> workerQueue = DispatchQueue::create(STRING_LITERAL("Worker Thread"), ThreadQoSClassHigh);
    Ref<MainThreadManager> mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    Shared<RecordingResourceLoader> resourceLoader = makeShared<RecordingResourceLoader>();
    Ref<ResourceManager> resourceManager;

    RecordingResourceManagerWrapper() {
        mainThreadManager->markCurrentThreadIsMainThread();
        resourceManager = makeTestResourceManager(resourceLoader, workerQueue, *mainThreadManager);
    }

    ~RecordingResourceManagerWrapper() {
        workerQueue->fullTeardown();
    }
};

struct ResourceManagerWrapper {
    Ref<MainQueue> mainQueue;
    Ref<DispatchQueue> workerQueue;
    Ref<MainThreadManager> mainThreadManager;
    Shared<BlockingResourceLoader> resourceLoader;
    Ref<ResourceManager> resourceManager;

    ResourceManagerWrapper() {
        mainQueue = makeShared<MainQueue>();
        workerQueue = DispatchQueue::create(STRING_LITERAL("Worker Thread"), ThreadQoSClassHigh);
        mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
        mainThreadManager->markCurrentThreadIsMainThread();
        resourceLoader = makeShared<BlockingResourceLoader>();

        resourceManager = makeTestResourceManager(resourceLoader, workerQueue, *mainThreadManager);
    }

    ~ResourceManagerWrapper() {
        resourceLoader->release();
        workerQueue->fullTeardown();
    }
};

// Regression for MUSIC-13144 / SHARING-35215: getApiVersion used to hold the ResourceManager mutex
// across the resource loader, so a slow first load stalled the main thread in preloadForComponentPath.
TEST(ResourceManagerTests, slowApiVersionLoadDoesNotBlockOtherCallers) {
    ResourceManagerWrapper wrapper;

    std::thread apiVersionThread([&]() { wrapper.resourceManager->getApiVersion(); });
    wrapper.resourceLoader->waitUntilLoadStarted();

    auto otherCallers = std::async(std::launch::async, [&]() {
        auto& resourceManager = *wrapper.resourceManager;
        bool loaded = resourceManager.isBundleLoaded(STRING_LITERAL("missing"));
        // The preload's worker task registers "missing" as soon as it gets the lock, so the
        // bundle-name list is not asserted on; these calls only need to return promptly.
        resourceManager.preloadForComponentPath(ComponentPath::parse(STRING_LITERAL("missing/Document")));
        resourceManager.getAllLoadedBundleNames();
        resourceManager.removeUnusedResources();
        return !loaded;
    });

    // Unblock unconditionally before asserting: on a regression the future's destructor would
    // otherwise wait on a task that is itself waiting on the parked apiVersionThread.
    auto status = otherCallers.wait_for(std::chrono::seconds(5));
    wrapper.resourceLoader->release();
    apiVersionThread.join();

    EXPECT_EQ(std::future_status::ready, status) << "ResourceManager callers were blocked behind the API version load";
    EXPECT_TRUE(otherCallers.get());
    EXPECT_EQ(42, wrapper.resourceManager->getApiVersion());
}

TEST(ResourceManagerTests, apiVersionIsLoadedOnceAndCached) {
    ResourceManagerWrapper wrapper;
    wrapper.resourceLoader->release();

    ASSERT_EQ(42, wrapper.resourceManager->getApiVersion());
    ASSERT_EQ(42, wrapper.resourceManager->getApiVersion());
}

class SourceMapTweakProvider : public SharedPtrRefCountable, public ITweakValueProvider {
public:
    explicit SourceMapTweakProvider(bool skipSourceMapLookup) : _skipSourceMapLookup(skipSourceMapLookup) {}

    StringBox getString(const StringBox& /*key*/, const StringBox& fallback) override {
        return fallback;
    }
    bool getBool(const StringBox& key, bool fallback) override {
        return key == STRING_LITERAL("VALDI_SKIP_SOURCE_MAP_LOOKUP") ? _skipSourceMapLookup : fallback;
    }
    float getFloat(const StringBox& /*key*/, float fallback) override {
        return fallback;
    }
    int32_t getInt(const StringBox& /*key*/, int32_t fallback) override {
        return fallback;
    }
    Value getBinary(const StringBox& /*key*/, const Value& fallback) override {
        return fallback;
    }

private:
    bool _skipSourceMapLookup;
};

static void setSkipSourceMapLookup(ResourceManager& resourceManager, bool skip) {
    resourceManager.setRuntimeTweaks(
        makeShared<ValdiRuntimeTweaks>(makeShared<SourceMapTweakProvider>(skip).toShared()));
}

TEST(ResourceManagerTests, looksUpSourceMapByDefault) {
    RecordingResourceManagerWrapper wrapper;

    wrapper.resourceManager->getBundle(STRING_LITERAL("module_a"));

    EXPECT_TRUE(wrapper.resourceLoader->wasRequested("module_a.valdimodule"));
    EXPECT_TRUE(wrapper.resourceLoader->wasRequested("module_a.map.json"));
}

TEST(ResourceManagerTests, looksUpSourceMapWhenTweakOff) {
    RecordingResourceManagerWrapper wrapper;
    setSkipSourceMapLookup(*wrapper.resourceManager, false);

    wrapper.resourceManager->getBundle(STRING_LITERAL("module_a"));

    EXPECT_TRUE(wrapper.resourceLoader->wasRequested("module_a.map.json"));
}

TEST(ResourceManagerTests, skipsSourceMapLookupWhenTweakOnAndNoMapsShip) {
    RecordingResourceManagerWrapper wrapper;
    setSkipSourceMapLookup(*wrapper.resourceManager, true);

    wrapper.resourceManager->getBundle(STRING_LITERAL("module_a"));
    wrapper.resourceManager->getBundle(STRING_LITERAL("module_b"));

    EXPECT_TRUE(wrapper.resourceLoader->wasRequested("module_a.valdimodule"));
    EXPECT_FALSE(wrapper.resourceLoader->wasRequested("module_a.map.json"));
    EXPECT_FALSE(wrapper.resourceLoader->wasRequested("module_b.map.json"));
    EXPECT_TRUE(wrapper.resourceLoader->wasRequested("valdi_core.map.json")) << "probed once";
}

TEST(ResourceManagerTests, keepsSourceMapLookupWhenMapsShip) {
    RecordingResourceManagerWrapper wrapper;
    wrapper.resourceLoader->available.emplace_back("valdi_core.map.json");
    setSkipSourceMapLookup(*wrapper.resourceManager, true);

    wrapper.resourceManager->getBundle(STRING_LITERAL("module_a"));

    EXPECT_TRUE(wrapper.resourceLoader->wasRequested("module_a.map.json"));
}

} // namespace ValdiTest
