//
//  ResourceManager_tests.cpp
//  ValdiRuntime
//

#include "valdi/runtime/Interfaces/IResourceLoader.hpp"
#include "valdi/runtime/Resources/AssetLoaderManager.hpp"
#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Holder.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi_test_utils.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

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

        Holder<Shared<snap::valdi_core::HTTPRequestManager>> requestManagerHolder;
        resourceManager = makeShared<ResourceManager>(resourceLoader,
                                                      makeShared<InMemoryDiskCache>(),
                                                      makeShared<AssetLoaderManager>(),
                                                      requestManagerHolder,
                                                      workerQueue,
                                                      *mainThreadManager,
                                                      1.0,
                                                      false,
                                                      ConsoleLogger::getLogger());
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

} // namespace ValdiTest
