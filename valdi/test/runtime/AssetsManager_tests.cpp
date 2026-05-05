
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/StandaloneLoadedAsset.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"

#include "valdi/runtime/Context/ContextEntry.hpp"
#include "valdi/runtime/Resources/AssetCatalog.hpp"
#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#include "valdi/runtime/Resources/AssetLoaderManager.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi/runtime/Resources/Bundle.hpp"
#include "valdi/runtime/Resources/Remote/DownloadableModuleManifestWrapper.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleManager.hpp"
#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"

#include "valdi/runtime/Utils/BytesUtils.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "MockAssetLoader.hpp"
#include "MockAssetLoaderFactory.hpp"
#include "MockDownloader.hpp"
#include "RequestManagerMock.hpp"
#include "valdi_core/AssetLoadObserver.hpp"
#include "valdi_core/Platform.hpp"
#include "valdi_core/cpp/Utils/ContainerUtils.hpp"
#include "valdi_test_utils.hpp"

#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

class SyncAssetLoadObserver : public SharedPtrRefCountable, public snap::valdi_core::AssetLoadObserver {
public:
    SyncAssetLoadObserver() {}

    void onLoad(const Shared<snap::valdi_core::Asset>& asset,
                const Valdi::Value& loadedAsset,
                const std::optional<Valdi::StringBox>& error) override {
        SC_ASSERT(_allowMultipleResults || _results.empty());

        if (error) {
            _results.emplace_back(Error(error.value()));
        } else {
            _results.emplace_back(loadedAsset);
        }
    }

    Result<Value> load(const Ref<MainQueue>& mainQueue,
                       const Ref<Asset>& asset,
                       int32_t preferredWidth = 0,
                       int32_t preferredHeight = 0,
                       Value filter = Value(),
                       snap::valdi_core::AssetOutputType outputType = snap::valdi_core::AssetOutputType::Dummy) {
        asset->addLoadObserver(strongRef(this), outputType, preferredWidth, preferredHeight, filter);

        mainQueue->runUntilTrue([&]() { return !_results.empty(); });

        if (_results.empty()) {
            return Error("Timed out");
        }

        return _results[_results.size() - 1];
    }

    void unload(const Ref<Asset>& asset) {
        asset->removeLoadObserver(strongRef(this));
    }

    void setAllowMultipleResults(bool allowMultipleResults) {
        _allowMultipleResults = allowMultipleResults;
    }

    std::vector<Result<Value>> getResults() const {
        return _results;
    }

    static Result<Value> loadSync(
        const Ref<MainQueue>& mainQueue,
        const Ref<Asset>& asset,
        int32_t preferredWidth,
        int32_t preferredHeight,
        Value filter = Value(),
        snap::valdi_core::AssetOutputType outputType = snap::valdi_core::AssetOutputType::Dummy) {
        auto observer = makeShared<SyncAssetLoadObserver>();

        return observer->load(mainQueue, asset, preferredWidth, preferredHeight, filter, outputType);
    }

private:
    std::vector<Result<Value>> _results;
    bool _allowMultipleResults = false;
};

static Ref<Bundle> makeInitializedBundle(const StringBox& moduleName, bool hasRemoteAssets) {
    auto bundle = makeShared<Bundle>(moduleName, ConsoleLogger::getLogger());
    auto initializer = bundle->prepareForInit();
    initializer.initWithLocalArchive(makeShared<ValdiModuleArchive>(), hasRemoteAssets);
    return bundle;
}

struct AssetsManagerWrapper : public AssetsManagerListener {
    Ref<MainQueue> mainQueue;
    Ref<DispatchQueue> workerQueue;
    Ref<InMemoryDiskCache> diskCache;
    Shared<StandaloneResourceLoader> resourceLoader;
    Ref<RemoteModuleManager> remoteModuleManager;
    Ref<MainThreadManager> mainThreadManager;
    Ref<AssetsManager> assetsManager;
    Ref<MockAssetLoader> assetLoader;
    Shared<RequestManagerMock> requestManager;
    Ref<AssetLoaderManager> assetLoaderManager;

    std::deque<Function<void(const Ref<ManagedAsset>&)>> callbacks;
    Function<void()> onPerformedUpdatesCallback;
    size_t totalCallbacks = 0;
    std::vector<Ref<Asset>> assets;
    Ref<AsyncGroup> asyncGroup;

    AssetsManagerWrapper() {
        mainQueue = makeShared<MainQueue>();
        auto& logger = ConsoleLogger::getLogger();
        workerQueue = DispatchQueue::create(STRING_LITERAL("Worker Thread"), ThreadQoSClassHigh);
        diskCache = makeShared<InMemoryDiskCache>();
        resourceLoader = makeShared<StandaloneResourceLoader>();

        Holder<Shared<snap::valdi_core::HTTPRequestManager>> requestManagerHolder;
        remoteModuleManager =
            makeShared<RemoteModuleManager>(diskCache, requestManagerHolder, workerQueue, logger, 1.0);
        mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
        mainThreadManager->markCurrentThreadIsMainThread();

        assetLoaderManager = makeShared<AssetLoaderManager>();
        assetLoader = makeShared<MockAssetLoader>();
        assetLoaderManager->registerAssetLoader(assetLoader);

        assetsManager = makeShared<AssetsManager>(
            resourceLoader, remoteModuleManager, assetLoaderManager, workerQueue, *mainThreadManager, logger);
        assetsManager->setListener(this);

        remoteModuleManager->setDecompressionDisabled(true);

        requestManager = Valdi::makeShared<RequestManagerMock>(logger);
        requestManagerHolder.set(requestManager);
    }

    ~AssetsManagerWrapper() {
        if (asyncGroup != nullptr) {
            resumeWorkerQueue();
        }

        workerQueue->fullTeardown();
        assetsManager->setListener(nullptr);
    }

    void onManagedAssetUpdated(const Ref<ManagedAsset>& managedAsset) override {
        if (totalCallbacks == 0 && callbacks.empty()) {
            // If no callbacks are registered we skip the callback tests
            return;
        }

        ASSERT_FALSE(callbacks.empty()) << fmt::format("No matching callback for callback number {}",
                                                       totalCallbacks + 1);

        auto callback = std::move(callbacks[0]);
        callbacks.pop_front();
        totalCallbacks++;

        callback(managedAsset);
    }

    void onPerformedUpdates() override {
        if (onPerformedUpdatesCallback) {
            onPerformedUpdatesCallback();
        }
    }

    Result<Value> loadAssetSync(const AssetKey& assetKey,
                                int32_t preferredWidth = 0,
                                int32_t preferredHeight = 0,
                                Value filter = Value()) {
        auto asset = assetsManager->getAsset(assetKey);
        assets.emplace_back(asset);
        return SyncAssetLoadObserver::loadSync(mainQueue, asset, preferredWidth, preferredHeight, filter);
    }

    bool allCallbacksCalled() const {
        return callbacks.empty();
    }

    void tearDown() {
        ASSERT_TRUE(allCallbacksCalled());
        assetsManager->setListener(nullptr);
        assets.clear();
    }

    void flushQueues() {
        mainQueue->flush();
        workerQueue->sync([]() {});
        mainQueue->flush();
    }

    void pauseWorkerQueue() {
        SC_ASSERT(this->asyncGroup == nullptr);
        auto asyncGroup = makeShared<AsyncGroup>();
        asyncGroup->enter();
        this->asyncGroup = asyncGroup;

        workerQueue->async([asyncGroup]() { asyncGroup->blockingWait(); });
    }

    void resumeWorkerQueue() {
        SC_ASSERT(asyncGroup != nullptr);
        asyncGroup->leave();
        asyncGroup = nullptr;
    }

    void mockRemoteModule(const StringBox& moduleName, const std::vector<AssetSpecs>& assetSpecs) {
        ValdiArchiveBuilder moduleBuilder;
        for (const auto& specs : assetSpecs) {
            moduleBuilder.addEntry(ValdiArchiveEntry(specs.getName(), specs.getName()));
        }

        auto moduleBytes = moduleBuilder.build();

        auto url = STRING_LITERAL("http://snap.com/valdi/").append(moduleName);
        requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

        auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
        auto* asset = manifest1->pb.add_assets();
        asset->mutable_artifact()->set_url(url.slowToString());
        auto sha256Digest = BytesUtils::sha256(moduleBytes->toBytesView());
        asset->mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());

        remoteModuleManager->registerManifest(moduleName, manifest1);
    }

    AssetKey createAndRegisterLocalAsset() {
        auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
        return createAndRegisterLocalAsset(assetToLoad);
    }

    AssetKey createAndRegisterLocalAsset(const Ref<StandaloneLoadedAsset>& assetToLoad) {
        return createAndRegisterLocalAsset(Result<Ref<LoadedAsset>>(assetToLoad));
    }

    AssetKey createAndRegisterLocalAsset(const Result<Ref<Valdi::LoadedAsset>>& assetToLoad) {
        auto moduleName = STRING_LITERAL("module");
        auto filePath = STRING_LITERAL("local");

        auto url = STRING_LITERAL("asset://module/local");

        assetLoader->setAssetResponse(url, assetToLoad);
        resourceLoader->setLocalAssetURL(moduleName, filePath, url);
        auto bundle = makeInitializedBundle(moduleName, false);
        return AssetKey(bundle, filePath);
    }

    AssetKey createAndRegisterRemoteAsset() {
        auto moduleName = STRING_LITERAL("module");
        auto filePath = STRING_LITERAL("remote");

        auto bundle = makeInitializedBundle(moduleName, true);

        std::vector<AssetSpecs> assetSpecs;
        assetSpecs.emplace_back(AssetSpecs(filePath, 42, 42, AssetSpecsType::IMAGE));
        mockRemoteModule(moduleName, assetSpecs);

        auto assetCatalog = makeShared<AssetCatalog>(assetSpecs);
        bundle->setAssetCatalog(STRING_LITERAL("res"), assetCatalog);

        return AssetKey(bundle, filePath);
    }
};

TEST(AssetsManager, canResolveAndLoadLocalAsset) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterLocalAsset(assetToLoad);

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    wrapper.tearDown();
}

TEST(AssetsManager, canResolveAndLoadRemoteAsset) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();
    wrapper.assetLoader->setAssetResponse(STRING_LITERAL("file:///resources-module.dir/remote"),
                                          Ref<Valdi::LoadedAsset>(assetToLoad));

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    wrapper.tearDown();
}

TEST(AssetsManager, propagatesContextOnLoad) {
    AssetsManagerWrapper wrapper;
    auto context = makeShared<Context>(1, nullptr);
    SharedAtomic<Ref<Context>> contextAtLoad;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();
    wrapper.assetLoader->setAssetResponseCallback(STRING_LITERAL("file:///resources-module.dir/remote"), [=]() {
        contextAtLoad.set(Context::currentRef());
        return Result(Ref<Valdi::LoadedAsset>(assetToLoad));
    });

    ContextEntry entry(context);
    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(context, contextAtLoad.get());

    wrapper.tearDown();
}

TEST(AssetsManager, doesntResolveAgainOnceResolved) {
    AssetsManagerWrapper wrapper;

    auto assetKey = wrapper.createAndRegisterLocalAsset();

    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) { ASSERT_EQ(AssetStateReady, asset->getState()); });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_TRUE(result) << result.description();

    ASSERT_TRUE(wrapper.allCallbacksCalled());

    // Replace local resource identifier, the AssetsManager should keep the cached 42.0 value
    wrapper.resourceLoader->setLocalAssetURL(
        assetKey.getBundle()->getName(), assetKey.getPath(), STRING_LITERAL("invalid://this_is_dangling"));

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());

        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());

        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result2 = wrapper.loadAssetSync(assetKey);
    ASSERT_TRUE(result2) << result2.description();

    wrapper.tearDown();
}

TEST(AssetsManager, canLoadAssetAsynchronously) {
    AssetsManagerWrapper wrapper;
    // Turning the AssetLoader async
    auto taskQueue = makeShared<TaskQueue>();
    wrapper.assetLoader->setWorkerQueue(taskQueue);

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterLocalAsset(assetToLoad);

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.workerQueue->async([=]() { ASSERT_TRUE(taskQueue->runNextTask()); });
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);
    wrapper.tearDown();
}

TEST(AssetsManager, canLoadAssetFromURL) {
    AssetsManagerWrapper wrapper;
    auto taskQueue = makeShared<TaskQueue>();
    wrapper.assetLoader->setWorkerQueue(taskQueue);

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.workerQueue->async([=]() { ASSERT_TRUE(taskQueue->runNextTask()); });
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(AssetKey(url));

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);
    wrapper.tearDown();
}

TEST(AssetsManager, doesntLoadAssetAgainWhenReceivingNewConsumersWithSameSpecs) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    wrapper.callbacks.emplace_back([](const auto& asset) {});

    auto result = wrapper.loadAssetSync(AssetKey(url));

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);
    ASSERT_TRUE(wrapper.allCallbacksCalled());

    auto assetToLoad2 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    // Replace the asset to load, the second consumer should NOT get it
    wrapper.assetLoader->setAssetResponse(url, assetToLoad2);

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer1->getState());
        ASSERT_EQ(AssetConsumerStateLoaded, consumer2->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_FALSE(consumer2->notified());
    });

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer2->getState());
        ASSERT_TRUE(consumer2->notified());
    });

    auto result2 = wrapper.loadAssetSync(AssetKey(url));

    ASSERT_TRUE(result2) << result2.description();

    auto loadedAsset2 = result2.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset2 != nullptr) << result2.description();

    ASSERT_NE(assetToLoad2, loadedAsset2);
    ASSERT_EQ(assetToLoad, loadedAsset2);
    wrapper.tearDown();
}

TEST(AssetsManager, loadAssetAgainWhenReceivingNewConsumersWithDifferentSpecs) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    wrapper.callbacks.emplace_back([](const auto& asset) {});

    auto result = wrapper.loadAssetSync(AssetKey(url));

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);
    ASSERT_TRUE(wrapper.allCallbacksCalled());

    auto assetToLoad2 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    // Replace the asset to load, the second consumer should get it
    wrapper.assetLoader->setAssetResponse(url, assetToLoad2);

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer1->getState());
        ASSERT_EQ(AssetConsumerStateLoading, consumer2->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_FALSE(consumer2->notified());
    });

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer2->getState());
        ASSERT_TRUE(consumer2->notified());
    });

    auto result2 = wrapper.loadAssetSync(AssetKey(url), 42, 42);

    ASSERT_TRUE(result2) << result2.description();

    auto loadedAsset2 = result2.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset2 != nullptr) << result2.description();

    ASSERT_EQ(assetToLoad2, loadedAsset2);

    auto assetToLoad3 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    wrapper.assetLoader->setAssetResponse(url, assetToLoad3);

    wrapper.callbacks.emplace_back([&](const auto& asset) {

    });
    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    // Load with a different filter

    auto result3 = wrapper.loadAssetSync(AssetKey(url), 42, 42, Value(42.0));

    ASSERT_TRUE(result3) << result3.description();

    auto loadedAsset3 = result3.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset3 != nullptr) << result3.description();

    ASSERT_EQ(assetToLoad3, loadedAsset3);

    wrapper.tearDown();
}

TEST(AssetsManager, canRemoveConsumers) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    auto observer1 = makeShared<SyncAssetLoadObserver>();
    auto observer2 = makeShared<SyncAssetLoadObserver>();

    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(observer1, consumer1->getObserver());
        ASSERT_EQ(observer2, consumer2->getObserver());
    });

    auto asset = wrapper.assetsManager->getAsset(AssetKey(url));

    auto result = observer1->load(wrapper.mainQueue, asset);
    auto result2 = observer2->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result) << result.description();
    ASSERT_TRUE(result2) << result2.description();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer2 = asset->getConsumer(0);

        ASSERT_EQ(observer2, consumer2->getObserver());
    });

    asset->removeLoadObserver(strongRef(observer1.get()));

    wrapper.tearDown();
}

TEST(AssetsManager, unloadAssetOnRemoveConsumer) {
    AssetsManagerWrapper wrapper;

    auto bytes = makeShared<ByteBuffer>("Hello World")->toBytesView();

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(bytes, 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    auto observer1 = makeShared<SyncAssetLoadObserver>();

    auto asset = wrapper.assetsManager->getAsset(AssetKey(url));

    // 2 refs in the test
    ASSERT_EQ(static_cast<long>(2), assetToLoad.use_count());

    auto result = observer1->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result) << result.description();

    wrapper.workerQueue->sync([]() {});

    // 2 refs in the test, 1 in the asset consumer
    // 1 in the AssetLoaderRequestHandler, 1 in the returned result
    // 1 in the _results array inside the SyncAssetLoadObserver
    ASSERT_EQ(static_cast<long>(6), assetToLoad.use_count());

    ASSERT_EQ(bytes, assetToLoad->getBytes());
    ASSERT_TRUE(assetToLoad->getBytes().size() > 0);

    asset->removeLoadObserver(strongRef(observer1.get()));
    wrapper.workerQueue->sync([]() {});

    result = Result<Value>();
    observer1 = nullptr;

    ASSERT_EQ(static_cast<long>(2), assetToLoad.use_count());

    wrapper.tearDown();
}

TEST(AssetsManager, failsConsumerOnResolveFail) {
    AssetsManagerWrapper wrapper;

    auto moduleName = STRING_LITERAL("module");
    auto filePath = STRING_LITERAL("local");

    wrapper.callbacks.emplace_back(
        [](const auto& asset) { ASSERT_EQ(AssetStateResolvingLocation, asset->getState()); });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedPermanently, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedPermanently, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto bundle = makeInitializedBundle(moduleName, false);
    auto result = wrapper.loadAssetSync(AssetKey(bundle, filePath));

    ASSERT_FALSE(result) << result.description();

    wrapper.tearDown();
}

TEST(AssetsManager, failsConsumerOnLoadFail) {
    AssetsManagerWrapper wrapper;

    auto assetKey = wrapper.createAndRegisterLocalAsset(Error("Failed to load"));

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_FALSE(result) << result.description();

    wrapper.tearDown();
}

TEST(AssetsManager, failsConsumerOnRequestPayloadFail) {
    AssetsManagerWrapper wrapper;

    auto assetKey = wrapper.createAndRegisterLocalAsset(Error("Failed to load"));

    wrapper.assetLoader->setRequestPayloadError(Error("Invalid payload"));

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_FALSE(result) << result.description();
    ASSERT_TRUE(result.error().toStringBox().contains("Invalid payload"));

    wrapper.tearDown();
}

TEST(AssetsManager, retriesResolveOnNetworkFailure) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();

    wrapper.assetLoader->setAssetResponse(STRING_LITERAL("file:///resources-module.dir/remote"),
                                          Ref<Valdi::LoadedAsset>(assetToLoad));

    // Make the request manager fail
    wrapper.requestManager->setDisabled(true);

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());
        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedRetryable, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedRetryable, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_FALSE(result) << result.description();

    // Re-enable the request manager
    wrapper.requestManager->setDisabled(false);

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateFailed, consumer1->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_EQ(AssetConsumerStateInitial, consumer2->getState());
        ASSERT_FALSE(consumer2->notified());

        wrapper.resumeWorkerQueue();
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateFailed, consumer1->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_EQ(AssetConsumerStateLoading, consumer2->getState());
        ASSERT_FALSE(consumer2->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateFailed, consumer1->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_EQ(AssetConsumerStateLoaded, consumer2->getState());
        ASSERT_TRUE(consumer2->notified());
    });

    auto result2 = wrapper.loadAssetSync(assetKey);

    ASSERT_TRUE(result2) << result2.description();

    auto loadedAsset2 = result2.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset2 != nullptr) << result2.description();

    ASSERT_EQ(assetToLoad, loadedAsset2);

    wrapper.tearDown();
}

TEST(AssetsManager, doesntRetryResolveOnPermanentFailure) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto moduleName = STRING_LITERAL("module");
    auto filePath = STRING_LITERAL("remote");

    auto bundle = makeInitializedBundle(moduleName, true);
    auto assetKey = AssetKey(bundle, filePath);

    std::vector<AssetSpecs> assetSpecs;
    assetSpecs.emplace_back(AssetSpecs(STRING_LITERAL("blabla"), 42, 42, AssetSpecsType::IMAGE));
    wrapper.mockRemoteModule(moduleName, assetSpecs);

    auto assetCatalog = makeShared<AssetCatalog>(assetSpecs);
    bundle->setAssetCatalog(STRING_LITERAL("res"), assetCatalog);
    wrapper.assetLoader->setAssetResponse(STRING_LITERAL("file:///resources-module.dir/remote"),
                                          Ref<Valdi::LoadedAsset>(assetToLoad));

    wrapper.pauseWorkerQueue();

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateInitial, consumer->getState());
        ASSERT_FALSE(consumer->notified());

        wrapper.resumeWorkerQueue();
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedPermanently, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_FALSE(consumer->notified());
    });
    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedPermanently, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateFailed, consumer->getState());
        ASSERT_TRUE(consumer->notified());
    });

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_FALSE(result) << result.description();

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedPermanently, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateFailed, consumer1->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_EQ(AssetConsumerStateFailed, consumer2->getState());
        ASSERT_FALSE(consumer2->notified());
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateFailedPermanently, asset->getState());
        ASSERT_TRUE(asset->getObservable() != nullptr);
        ASSERT_EQ(static_cast<size_t>(2), asset->getConsumersSize());

        auto consumer1 = asset->getConsumer(0);
        auto consumer2 = asset->getConsumer(1);

        ASSERT_EQ(AssetConsumerStateFailed, consumer1->getState());
        ASSERT_TRUE(consumer1->notified());
        ASSERT_EQ(AssetConsumerStateFailed, consumer2->getState());
        ASSERT_TRUE(consumer2->notified());
    });

    auto result2 = wrapper.loadAssetSync(assetKey);

    ASSERT_FALSE(result2) << result2.description();

    wrapper.tearDown();
}

TEST(AssetsManager, keepLocalAssetWhenObservablesAreDestroyed) {
    AssetsManagerWrapper wrapper;

    auto assetKey = wrapper.createAndRegisterLocalAsset();

    wrapper.callbacks.emplace_back([](const auto& asset) {

    });
    wrapper.callbacks.emplace_back([](const auto& asset) {

    });
    wrapper.callbacks.emplace_back([](const auto& asset) {});

    auto observer = makeShared<SyncAssetLoadObserver>();

    auto asset = wrapper.assetsManager->getAsset(assetKey);

    auto result = observer->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result) << result.description();

    wrapper.callbacks.emplace_back([](const auto& asset) { ASSERT_TRUE(asset->getObservable() != nullptr); });

    asset->removeLoadObserver(strongRef(observer.get()));

    Weak<Asset> weakAsset = asset;

    wrapper.callbacks.emplace_back([](const auto& asset) { ASSERT_FALSE(asset->getObservable() != nullptr); });

    auto assetLocation = wrapper.assetsManager->getResolvedAssetLocation(assetKey);
    ASSERT_TRUE(assetLocation.has_value());

    asset = nullptr;

    assetLocation = wrapper.assetsManager->getResolvedAssetLocation(assetKey);

    // We should still be able to resolve the location, as the asset should have stayed in the AssetsManager
    ASSERT_TRUE(assetLocation.has_value());
    wrapper.tearDown();
}

TEST(AssetsManager, removesUrlAssetWhenObservablesAreDestroyed) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    wrapper.callbacks.emplace_back([](const auto& asset) {});

    auto assetKey = AssetKey(url);

    auto observer = makeShared<SyncAssetLoadObserver>();

    auto asset = wrapper.assetsManager->getAsset(assetKey);

    auto result = observer->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result) << result.description();

    wrapper.callbacks.emplace_back([](const auto& asset) { ASSERT_TRUE(asset->getObservable() != nullptr); });

    asset->removeLoadObserver(strongRef(observer.get()));

    Weak<Asset> weakAsset = asset;

    wrapper.callbacks.emplace_back([](const auto& asset) { ASSERT_FALSE(asset->getObservable() != nullptr); });

    auto assetLocation = wrapper.assetsManager->getResolvedAssetLocation(assetKey);
    ASSERT_TRUE(assetLocation.has_value());

    asset = nullptr;

    assetLocation = wrapper.assetsManager->getResolvedAssetLocation(assetKey);

    // We should NOT be able to resolve the location anymore, as the asset should have been removed from the
    // AssetsManager
    ASSERT_FALSE(assetLocation.has_value());

    wrapper.tearDown();
}

TEST(AssetsManager, cancelLoadRequestOnUnobserve) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    auto observer1 = makeShared<SyncAssetLoadObserver>();

    auto asset = wrapper.assetsManager->getAsset(AssetKey(url));

    auto result = observer1->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result) << result.description();

    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getCurrentLoadRequestsCount());

    wrapper.callbacks.emplace_back([&](const auto& asset) {});

    asset->removeLoadObserver(strongRef(observer1.get()));
    wrapper.workerQueue->sync([]() {});

    ASSERT_EQ(static_cast<size_t>(0), wrapper.assetLoader->getCurrentLoadRequestsCount());

    wrapper.tearDown();
}

TEST(AssetsManager, loaderCanCallLoadCallbackMultipleTimes) {
    AssetsManagerWrapper wrapper;

    auto url = STRING_LITERAL("asset://module/local");

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto moduleName = STRING_LITERAL("module");
    auto filePath = STRING_LITERAL("local");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);
    wrapper.resourceLoader->setLocalAssetURL(moduleName, filePath, url);

    auto observer = makeShared<SyncAssetLoadObserver>();
    observer->setAllowMultipleResults(true);

    auto asset = wrapper.assetsManager->getAsset(AssetKey(url));

    observer->load(wrapper.mainQueue, asset);

    auto results1 = observer->getResults();
    ASSERT_EQ(static_cast<size_t>(1), results1.size());
    ASSERT_TRUE(results1[0]) << results1[0].description();

    auto loadedAsset1 = results1[0].value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset1 != nullptr) << results1[0].description();

    ASSERT_EQ(assetToLoad, loadedAsset1);
    ASSERT_TRUE(wrapper.allCallbacksCalled());

    auto assetToLoad2 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);

    wrapper.callbacks.emplace_back([](const auto& asset) {});

    wrapper.assetLoader->updateAllAssets(assetToLoad2);

    auto results2 = observer->getResults();
    ASSERT_EQ(static_cast<size_t>(2), results2.size());
    ASSERT_TRUE(results2[1]) << results2[1].description();

    auto loadedAsset2 = results2[1].value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset2 != nullptr) << results2[1].description();

    ASSERT_EQ(assetToLoad2, loadedAsset2);

    wrapper.tearDown();
}

TEST(AssetsManager, failsLoadOnNullAsset) {
    AssetsManagerWrapper wrapper;

    auto assetKey = wrapper.createAndRegisterLocalAsset(Ref<LoadedAsset>());

    auto result = wrapper.loadAssetSync(assetKey);

    ASSERT_FALSE(result) << result.description();

    wrapper.tearDown();
}

TEST(AssetsManager, cachesRequestPayload) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    ASSERT_EQ(static_cast<size_t>(0), wrapper.assetLoader->getRequestPayloadCallCount());

    auto result = wrapper.loadAssetSync(AssetKey(url));

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getRequestPayloadCallCount());

    // Load a second time

    result = wrapper.loadAssetSync(AssetKey(url));

    ASSERT_TRUE(result) << result.description();

    loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    // requestPayload should have been called only once
    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getRequestPayloadCallCount());

    wrapper.tearDown();
}

TEST(AssetsManager, canPauseUpdates) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    bool updatesPaused = true;
    wrapper.assetsManager->beginPauseUpdates();

    wrapper.callbacks.emplace_back([&](const auto& asset) { ASSERT_FALSE(updatesPaused); });

    auto asset = wrapper.assetsManager->getAsset(AssetKey(url));

    auto observer1 = makeShared<SyncAssetLoadObserver>();
    auto observer2 = makeShared<SyncAssetLoadObserver>();
    auto observer3 = makeShared<SyncAssetLoadObserver>();

    asset->addLoadObserver(observer1.toShared(), snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());
    asset->addLoadObserver(observer2.toShared(), snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());
    asset->addLoadObserver(observer3.toShared(), snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());

    wrapper.flushQueues();

    ASSERT_TRUE(observer1->getResults().empty());
    ASSERT_TRUE(observer2->getResults().empty());
    ASSERT_TRUE(observer3->getResults().empty());

    updatesPaused = false;
    wrapper.callbacks.clear();
    wrapper.assetsManager->endPauseUpdates();

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), observer1->getResults().size());
    ASSERT_EQ(static_cast<size_t>(1), observer2->getResults().size());
    ASSERT_EQ(static_cast<size_t>(1), observer3->getResults().size());
    // There should have been only one load call
    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getCurrentLoadRequestsCount());
    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getLoadAssetCount());

    wrapper.tearDown();
}

TEST(AssetsManager, canLoadWhileUpdatesArePaused) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad1 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url1 = STRING_LITERAL("https://snapchat.com/image1.png");

    wrapper.assetLoader->setAssetResponse(url1, assetToLoad1);

    auto assetToLoad2 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url2 = STRING_LITERAL("https://snapchat.com/image2.png");

    wrapper.assetLoader->setAssetResponse(url2, assetToLoad2);

    auto asset1 = wrapper.assetsManager->getAsset(AssetKey(url1));
    auto asset2 = wrapper.assetsManager->getAsset(AssetKey(url2));

    auto taskQueue = makeShared<TaskQueue>();
    wrapper.assetLoader->setWorkerQueue(taskQueue);

    auto observer1 = makeShared<SyncAssetLoadObserver>();
    asset1->addLoadObserver(observer1.toShared(), snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());

    wrapper.workerQueue->sync([=]() {});

    wrapper.assetsManager->beginPauseUpdates();

    wrapper.onPerformedUpdatesCallback = [&]() {
        // Force complete the load response of the first asset
        taskQueue->flush();
    };

    auto observer2 = makeShared<SyncAssetLoadObserver>();

    asset2->addLoadObserver(observer2.toShared(), snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());

    ASSERT_TRUE(observer1->getResults().empty());
    ASSERT_TRUE(observer2->getResults().empty());

    wrapper.assetsManager->endPauseUpdates();
    wrapper.onPerformedUpdatesCallback = Function<void()>();

    wrapper.flushQueues();
    taskQueue->flush();
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), observer1->getResults().size());
    ASSERT_EQ(static_cast<size_t>(1), observer2->getResults().size());
    ASSERT_EQ(static_cast<size_t>(2), wrapper.assetLoader->getCurrentLoadRequestsCount());
    ASSERT_EQ(static_cast<size_t>(2), wrapper.assetLoader->getLoadAssetCount());

    wrapper.tearDown();
}

TEST(AssetsManager, canPreventUnnecessaryLoadCancelation) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto url = STRING_LITERAL("https://snapchat.com/image.png");

    wrapper.assetLoader->setAssetResponse(url, assetToLoad);

    auto asset = wrapper.assetsManager->getAsset(AssetKey(url));

    auto observer1 = makeShared<SyncAssetLoadObserver>().toShared();
    auto observer2 = makeShared<SyncAssetLoadObserver>().toShared();
    observer1->setAllowMultipleResults(true);
    observer2->setAllowMultipleResults(true);

    asset->addLoadObserver(observer1, snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getCurrentLoadRequestsCount());
    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getLoadAssetCount());
    ASSERT_EQ(static_cast<size_t>(1), observer1->getResults().size());

    asset->removeLoadObserver(observer1);
    asset->addLoadObserver(observer2, snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());
    wrapper.flushQueues();

    // Without pausing updates, we should have a new load request since the removeLoadObserver
    // was done before the addLoadObserver
    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getCurrentLoadRequestsCount());
    ASSERT_EQ(static_cast<size_t>(2), wrapper.assetLoader->getLoadAssetCount());
    ASSERT_EQ(static_cast<size_t>(1), observer1->getResults().size());
    ASSERT_EQ(static_cast<size_t>(1), observer2->getResults().size());

    wrapper.assetsManager->beginPauseUpdates();

    // We now do the same thing inside a beginPauseUpdates: we remove an observer
    // then add a new one.
    asset->removeLoadObserver(observer2);
    asset->addLoadObserver(observer1, snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());

    wrapper.assetsManager->endPauseUpdates();

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), wrapper.assetLoader->getCurrentLoadRequestsCount());
    // We should not have received a new load request
    ASSERT_EQ(static_cast<size_t>(2), wrapper.assetLoader->getLoadAssetCount());
    // Observer1 should have received its new asset
    ASSERT_EQ(static_cast<size_t>(2), observer1->getResults().size());
    ASSERT_EQ(static_cast<size_t>(1), observer2->getResults().size());

    wrapper.tearDown();
}

TEST(AssetsManager, canOverrideResolvedAssetLocationAfterResolving) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();
    wrapper.assetLoader->setAssetResponse(STRING_LITERAL("file:///resources-module.dir/remote"),
                                          Ref<Valdi::LoadedAsset>(assetToLoad));

    wrapper.callbacks.emplace_back([&](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) {});
    wrapper.callbacks.emplace_back([](const auto& asset) {});

    auto asset = wrapper.assetsManager->getAsset(assetKey);

    auto observer = makeShared<SyncAssetLoadObserver>();
    auto result = observer->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result);

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    wrapper.callbacks.emplace_back([](const auto& asset) {});

    observer->unload(asset);

    observer = nullptr;

    auto previousLocation = wrapper.assetsManager->getResolvedAssetLocation(assetKey);

    ASSERT_TRUE(previousLocation);

    AssetLocation expectedLocation(STRING_LITERAL("file://some_location"), false);

    wrapper.assetsManager->setResolvedAssetLocation(assetKey, expectedLocation);

    auto newLocation = wrapper.assetsManager->getResolvedAssetLocation(assetKey);

    ASSERT_TRUE(newLocation);

    ASSERT_NE(previousLocation.value(), newLocation.value());
    ASSERT_EQ(expectedLocation, newLocation.value());

    wrapper.tearDown();
}

TEST(AssetsManager, canOverrideResolvedAssetLocatioDuringResolving) {
    AssetsManagerWrapper wrapper;

    auto updatedUrl = STRING_LITERAL("file://some_location");

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();
    wrapper.assetLoader->setAssetResponse(updatedUrl, Ref<Valdi::LoadedAsset>(assetToLoad));

    wrapper.pauseWorkerQueue();

    AssetLocation expectedLocation(updatedUrl, false);

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateResolvingLocation, asset->getState());
        // Cancel during resolving location
        wrapper.assetsManager->setResolvedAssetLocation(assetKey, expectedLocation);
        // Resume
        wrapper.resumeWorkerQueue();
    });

    wrapper.callbacks.emplace_back([](const auto& asset) { ASSERT_EQ(AssetStateReady, asset->getState()); });

    wrapper.callbacks.emplace_back([](const auto& asset) {});

    auto asset = wrapper.assetsManager->getAsset(assetKey);

    auto observer = makeShared<SyncAssetLoadObserver>();
    auto result = observer->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result);

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    wrapper.tearDown();
}

TEST(AssetsManager, canOverrideResolvedAssetLocatioDuringLoading) {
    AssetsManagerWrapper wrapper;

    auto updatedUrl = STRING_LITERAL("file://some_location");
    AssetLocation expectedLocation(updatedUrl, false);

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();
    wrapper.assetLoader->setAssetResponse(updatedUrl, Ref<Valdi::LoadedAsset>(assetToLoad));

    wrapper.callbacks.emplace_back(
        [&](const auto& asset) { ASSERT_EQ(AssetStateResolvingLocation, asset->getState()); });

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());

        wrapper.assetsManager->setResolvedAssetLocation(assetKey, expectedLocation);
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
    });

    auto asset = wrapper.assetsManager->getAsset(assetKey);

    auto observer = makeShared<SyncAssetLoadObserver>();
    auto result = observer->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result);

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    wrapper.tearDown();
}

TEST(AssetsManager, canOverrideResolvedAssetLocatioAfterLoading) {
    AssetsManagerWrapper wrapper;

    auto assetToLoad = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    auto assetKey = wrapper.createAndRegisterRemoteAsset();
    wrapper.assetLoader->setAssetResponse(STRING_LITERAL("file:///resources-module.dir/remote"),
                                          Ref<Valdi::LoadedAsset>(assetToLoad));

    wrapper.callbacks.emplace_back(
        [&](const auto& asset) { ASSERT_EQ(AssetStateResolvingLocation, asset->getState()); });

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
    });

    auto asset = wrapper.assetsManager->getAsset(assetKey);

    auto observer = makeShared<SyncAssetLoadObserver>();
    observer->setAllowMultipleResults(true);
    auto result = observer->load(wrapper.mainQueue, asset);

    ASSERT_TRUE(result);

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    ASSERT_EQ(assetToLoad, loadedAsset);

    auto updatedUrl = STRING_LITERAL("file://some_location");
    AssetLocation expectedLocation(updatedUrl, false);

    auto assetToLoad2 = makeShared<StandaloneLoadedAsset>(BytesView(), 0, 0);
    wrapper.assetLoader->setAssetResponse(updatedUrl, Ref<Valdi::LoadedAsset>(assetToLoad2));

    wrapper.callbacks.emplace_back([&](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoading, consumer->getState());
    });

    wrapper.callbacks.emplace_back([](const auto& asset) {
        ASSERT_EQ(AssetStateReady, asset->getState());
        ASSERT_EQ(static_cast<size_t>(1), asset->getConsumersSize());

        auto consumer = asset->getConsumer(0);

        ASSERT_EQ(AssetConsumerStateLoaded, consumer->getState());
    });

    wrapper.assetsManager->setResolvedAssetLocation(assetKey, expectedLocation);

    wrapper.mainQueue->runUntilTrue([&]() { return observer->getResults().size() == 2; });
    ASSERT_EQ(static_cast<size_t>(2), observer->getResults().size());

    auto secondResult = observer->getResults()[1];
    ASSERT_TRUE(secondResult);

    auto loadedAsset2 = secondResult.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset2 != nullptr) << secondResult.description();

    ASSERT_EQ(assetToLoad2, loadedAsset2);

    wrapper.tearDown();
}

TEST(AssetsManager, canLoadBytesAssetWithBytesOutput) {
    AssetsManagerWrapper wrapper;

    auto downloader = makeShared<MockDownloader>(nullptr);
    wrapper.assetLoaderManager->registerDownloaderForScheme(STRING_LITERAL("my-bytes"), downloader);

    auto bytes = makeShared<ByteBuffer>("This is the image content");

    auto asset = wrapper.assetsManager->createAssetWithBytes(bytes->toBytesView());
    auto result = SyncAssetLoadObserver::loadSync(
        wrapper.mainQueue, asset, 0, 0, Value(), snap::valdi_core::AssetOutputType::Bytes);

    ASSERT_TRUE(result) << result.description();

    auto typedArray = result.value().getTypedRef<ValueTypedArray>();

    ASSERT_TRUE(typedArray != nullptr) << result.description();

    ASSERT_EQ(typedArray->getBuffer(), bytes->toBytesView());

    wrapper.tearDown();
}

TEST(AssetsManager, canLoadBytesAssetWithNonBytesOutput) {
    AssetsManagerWrapper wrapper;

    auto downloader = makeShared<MockDownloader>(nullptr);
    wrapper.assetLoaderManager->registerDownloaderForScheme(STRING_LITERAL("my-bytes"), downloader);
    wrapper.assetLoaderManager->registerAssetLoaderFactory(
        makeShared<MockAssetLoaderFactory>(snap::valdi_core::AssetOutputType::Dummy));

    auto bytes = makeShared<ByteBuffer>("This is the image content");

    auto asset = wrapper.assetsManager->createAssetWithBytes(bytes->toBytesView());
    auto result = SyncAssetLoadObserver::loadSync(
        wrapper.mainQueue, asset, 0, 0, Value(), snap::valdi_core::AssetOutputType::Dummy);

    ASSERT_TRUE(result) << result.description();

    auto loadedAsset = result.value().getTypedRef<LoadedAsset>();

    ASSERT_TRUE(loadedAsset != nullptr) << result.description();

    auto bytesContentResult = loadedAsset->getBytesContent();

    ASSERT_TRUE(bytesContentResult) << result.description();
    ASSERT_EQ(bytesContentResult.value(), bytes->toBytesView());

    wrapper.tearDown();
}

} // namespace ValdiTest
