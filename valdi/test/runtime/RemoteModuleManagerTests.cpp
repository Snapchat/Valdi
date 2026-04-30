//
//  RemoteModuleManagerTests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 8/30/19.
//

#include "RequestManagerMock.hpp"
#include "TestAsyncUtils.hpp"
#include "valdi/runtime/Resources/Remote/DownloadableModuleManifestWrapper.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleManager.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleResources.hpp"
#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi/runtime/Utils/BytesUtils.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/valdi.pb.h"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

struct RemoteModuleManagerWrapper {
    Holder<Shared<snap::valdi_core::HTTPRequestManager>> requestManagerHolder;
    Ref<InMemoryDiskCache> disk;
    Ref<DispatchQueue> dispatchQueue;
    Ref<RemoteModuleManager> remoteBundleManager;
    Shared<RequestManagerMock> requestManager;

    RemoteModuleManagerWrapper() : RemoteModuleManagerWrapper(0, Valdi::makeShared<InMemoryDiskCache>()) {}

    RemoteModuleManagerWrapper(double deviceDensity, const Ref<InMemoryDiskCache>& disk) {
        [[maybe_unused]] auto& logger = ConsoleLogger::getLogger();
        this->disk = disk;
        dispatchQueue = DispatchQueue::create(STRING_LITERAL("Valdi Test Thread"), ThreadQoSClassMax);
        remoteBundleManager =
            Valdi::makeShared<RemoteModuleManager>(disk, requestManagerHolder, dispatchQueue, logger, deviceDensity);
        remoteBundleManager->setDecompressionDisabled(true);

        requestManager = Valdi::makeShared<RequestManagerMock>(logger);
        requestManagerHolder.set(requestManager);
    }

    ~RemoteModuleManagerWrapper() {
        dispatchQueue->sync([]() {});
        dispatchQueue->fullTeardown();
    }
};

StringBox bundleEntryToString(const ValdiModuleArchiveEntry& entry) {
    return StringCache::getGlobal().makeString(reinterpret_cast<const char*>(entry.data), entry.size);
}

TEST(RemoteModuleManager, canRegisterManifest) {
    RemoteModuleManagerWrapper wrapper;

    auto module1 = STRING_LITERAL("Module1");
    auto module2 = STRING_LITERAL("Module2");

    ASSERT_TRUE(wrapper.remoteBundleManager->getRegisteredManifest(module1) == nullptr);
    ASSERT_TRUE(wrapper.remoteBundleManager->getRegisteredManifest(module2) == nullptr);

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    wrapper.remoteBundleManager->registerManifest(module1, manifest1);

    ASSERT_EQ(manifest1, wrapper.remoteBundleManager->getRegisteredManifest(module1));
    ASSERT_TRUE(wrapper.remoteBundleManager->getRegisteredManifest(module2) == nullptr);

    auto manifest2 = makeShared<DownloadableModuleManifestWrapper>();
    wrapper.remoteBundleManager->registerManifest(module2, manifest2);

    ASSERT_EQ(manifest1, wrapper.remoteBundleManager->getRegisteredManifest(module1));
    ASSERT_EQ(manifest2, wrapper.remoteBundleManager->getRegisteredManifest(module2));
}

TEST(RemoteModuleManager, loadFailWithoutRegister) {
    RemoteModuleManagerWrapper wrapper;

    ValdiArchiveBuilder moduleBuilder;
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto moduleBytes = moduleBuilder.build();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

    auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto result = resultHolder->waitForResult();

    ASSERT_FALSE(result.success());
    ASSERT_TRUE(wrapper.requestManager->getAllPerformedTasks().empty());
}

TEST(RemoteModuleManager, loadFailWithoutRequestManager) {
    RemoteModuleManagerWrapper wrapper;
    wrapper.requestManagerHolder.set(nullptr);

    ValdiArchiveBuilder moduleBuilder;
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto moduleBytes = moduleBuilder.build();
    auto moduleBytesView = moduleBytes->toBytesView();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    manifest1->pb.mutable_artifact()->set_url(url.slowToString());
    auto sha256Digest = BytesUtils::sha256(moduleBytesView);
    manifest1->pb.mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());
    wrapper.remoteBundleManager->registerManifest(module, manifest1);

    auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto result = resultHolder->waitForResult();

    ASSERT_FALSE(result.success());
    ASSERT_TRUE(wrapper.requestManager->getAllPerformedTasks().empty());
}

TEST(RemoteModuleManager, canLoadModule) {
    RemoteModuleManagerWrapper wrapper;

    ValdiArchiveBuilder moduleBuilder;
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto moduleBytes = moduleBuilder.build();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    manifest1->pb.mutable_artifact()->set_url(url.slowToString());
    auto sha256Digest = BytesUtils::sha256(moduleBytes->toBytesView());
    manifest1->pb.mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());
    wrapper.remoteBundleManager->registerManifest(module, manifest1);

    auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto result = resultHolder->waitForResult();

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.value() != nullptr);

    auto entries = result.value()->getAllEntryPaths();
    ASSERT_EQ(static_cast<size_t>(2), entries.size());

    auto file1 = result.value()->getEntry(STRING_LITERAL("file1"));
    auto file2 = result.value()->getEntry(STRING_LITERAL("file2"));

    ASSERT_TRUE(file1.has_value());
    ASSERT_TRUE(file2.has_value());

    auto content1 = bundleEntryToString(file1.value());
    auto content2 = bundleEntryToString(file2.value());

    ASSERT_EQ(STRING_LITERAL("content1"), content1);
    ASSERT_EQ(STRING_LITERAL("content2"), content2);

    ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());
}

TEST(RemoteModuleManager, canUseModuleFromInMemoryCache) {
    RemoteModuleManagerWrapper wrapper;

    ValdiArchiveBuilder moduleBuilder;
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto moduleBytes = moduleBuilder.build();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    manifest1->pb.mutable_artifact()->set_url(url.slowToString());
    auto sha256Digest = BytesUtils::sha256(moduleBytes->toBytesView());
    manifest1->pb.mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());
    wrapper.remoteBundleManager->registerManifest(module, manifest1);

    auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto result = resultHolder->waitForResult();

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.value() != nullptr);

    ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

    // Try a second load
    wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    result = resultHolder->waitForResult();

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.value() != nullptr);

    // We should not have done a second request
    ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());
}

TEST(RemoteModuleManager, canGroupLoadModuleRequests) {
    RemoteModuleManagerWrapper wrapper;

    ValdiArchiveBuilder moduleBuilder;
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto moduleBytes1 = moduleBuilder.build();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes1->toBytesView());

    ValdiArchiveBuilder module2Builder;
    module2Builder.addEntry(ValdiArchiveEntry(STRING_LITERAL("somefile"), STRING_LITERAL("content1")));
    auto module2Bytes = module2Builder.build();

    auto module2 = STRING_LITERAL("Module2");
    auto url2 = STRING_LITERAL("http://snap.com/valdi/module2");
    wrapper.requestManager->addMockedResponse(url2, STRING_LITERAL("GET"), module2Bytes->toBytesView());

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    manifest1->pb.mutable_artifact()->set_url(url.slowToString());
    auto sha256Digest1 = BytesUtils::sha256(moduleBytes1->toBytesView());
    manifest1->pb.mutable_artifact()->set_sha256digest(sha256Digest1->data(), sha256Digest1->size());
    wrapper.remoteBundleManager->registerManifest(module, manifest1);
    auto manifest2 = makeShared<DownloadableModuleManifestWrapper>();
    manifest2->pb.mutable_artifact()->set_url(url2.slowToString());
    auto sha256Digest2 = BytesUtils::sha256(module2Bytes->toBytesView());
    manifest2->pb.mutable_artifact()->set_sha256digest(sha256Digest2->data(), sha256Digest2->size());
    wrapper.remoteBundleManager->registerManifest(module2, manifest2);

    // 1,2 and 4 are loading module1, 3 is loading module2
    auto resultHolder1 = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder1->makeCompletion());
    auto resultHolder2 = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder2->makeCompletion());
    auto resultHolder3 = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module2, resultHolder3->makeCompletion());
    auto resultHolder4 = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder4->makeCompletion());

    auto result1 = resultHolder1->waitForResult();
    auto result2 = resultHolder2->waitForResult();
    auto result3 = resultHolder3->waitForResult();
    auto result4 = resultHolder4->waitForResult();

    ASSERT_TRUE(result1.success());
    ASSERT_TRUE(result2.success());
    ASSERT_TRUE(result3.success());
    ASSERT_TRUE(result4.success());

    // We should have done only two requests
    ASSERT_EQ(static_cast<size_t>(2), wrapper.requestManager->getAllPerformedTasks().size());

    ASSERT_EQ(*result1.value(), *result2.value());
    ASSERT_EQ(*result2.value(), *result4.value());
    ASSERT_NE(*result2.value(), *result3.value());

    std::vector<StringBox> module1Paths = {STRING_LITERAL("file1"), STRING_LITERAL("file2")};
    std::vector<StringBox> module2Paths = {STRING_LITERAL("somefile")};

    ASSERT_EQ(module1Paths, result1.value()->getAllEntryPaths());
    ASSERT_EQ(module1Paths, result2.value()->getAllEntryPaths());
    ASSERT_EQ(module2Paths, result3.value()->getAllEntryPaths());
    ASSERT_EQ(module1Paths, result4.value()->getAllEntryPaths());
}

TEST(RemoteModuleManager, canStoreAndLoadModuleInDisk) {
    Ref<InMemoryDiskCache> disk;
    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    auto manifest = makeShared<DownloadableModuleManifestWrapper>();
    manifest->pb.mutable_artifact()->set_url(url.slowToString());

    Ref<ValdiModuleArchive> decompressedBundle;

    {
        RemoteModuleManagerWrapper wrapper;
        disk = wrapper.disk;

        ValdiArchiveBuilder moduleBuilder;
        moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
        moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
        auto moduleBytes = moduleBuilder.build();

        auto url = STRING_LITERAL("http://snap.com/valdi/module1");
        wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

        auto sha256Digest = BytesUtils::sha256(moduleBytes->toBytesView());
        manifest->pb.mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());
        wrapper.remoteBundleManager->registerManifest(module, manifest);

        auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
        wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

        auto result = resultHolder->waitForResult();

        ASSERT_TRUE(result.success());
        ASSERT_TRUE(result.value() != nullptr);

        decompressedBundle = result.value();

        ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

        // Flush the work queue so that the remote bundle manager can store the module in the disk cache
        wrapper.dispatchQueue->sync([]() {});

        ASSERT_EQ(static_cast<size_t>(1), wrapper.disk->getAll().size());
    }

    // Now we should be able we query again with our cached disk

    // Re-use our disk, which should have the cache populated
    RemoteModuleManagerWrapper newWrapper(0, disk);

    newWrapper.remoteBundleManager->registerManifest(module, manifest);
    auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
    newWrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto newResult = resultHolder->waitForResult();
    ASSERT_TRUE(newResult.success());
    ASSERT_TRUE(newResult.value() != nullptr);

    // No network requests should have been made
    ASSERT_TRUE(newWrapper.requestManager->getAllPerformedTasks().empty());

    ASSERT_EQ(*decompressedBundle, *newResult.value());

    newWrapper.dispatchQueue->sync([]() {});

    // Nothing should have changed in our disk
    ASSERT_EQ(static_cast<size_t>(1), disk->getAll().size());
}

TEST(RemoteModuleManager, reloadModuleOnOutOfDateCache) {
    RemoteModuleManagerWrapper wrapper;

    ValdiArchiveBuilder moduleBuilder;
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    moduleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto moduleBytes = moduleBuilder.build();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), moduleBytes->toBytesView());

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    manifest1->pb.mutable_artifact()->set_url(url.slowToString());
    auto sha256Digest = BytesUtils::sha256(moduleBytes->toBytesView());
    manifest1->pb.mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());
    wrapper.remoteBundleManager->registerManifest(module, manifest1);

    auto resultHolder = ResultHolder<Ref<ValdiModuleArchive>>::make();
    wrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto result = resultHolder->waitForResult();

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.value() != nullptr);

    auto decompressedBundle = result.value();

    ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

    // Flush the work queue so that the remote bundle manager can store the module in the disk cache
    wrapper.dispatchQueue->sync([]() {});

    ASSERT_EQ(static_cast<size_t>(1), wrapper.disk->getAll().size());

    auto diskContent = wrapper.disk->getAll();

    // Now we should be able we query again with our cached disk

    // Re-use our disk, which should have the cache populated
    RemoteModuleManagerWrapper newWrapper(0, wrapper.disk);

    // We make a new module
    ValdiArchiveBuilder newModuleBuilder;
    newModuleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1.new")));
    newModuleBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2.new")));
    auto newModuleBytes = newModuleBuilder.build();

    auto newUrl = STRING_LITERAL("http://snap.com/valdi/module1.new");
    newWrapper.requestManager->addMockedResponse(newUrl, STRING_LITERAL("GET"), newModuleBytes->toBytesView());

    // We update the url in the module manifest
    manifest1->pb.mutable_artifact()->set_url(newUrl.slowToString());
    auto sha256DigestNew = BytesUtils::sha256(newModuleBytes->toBytesView());
    manifest1->pb.mutable_artifact()->set_sha256digest(sha256DigestNew->data(), sha256DigestNew->size());
    // Register the manifest, which has a different url but the same module path
    newWrapper.remoteBundleManager->registerManifest(module, manifest1);

    newWrapper.remoteBundleManager->loadModule(module, resultHolder->makeCompletion());

    auto newResult = resultHolder->waitForResult();
    ASSERT_TRUE(newResult.success());
    ASSERT_TRUE(newResult.value() != nullptr);

    // We should have downloaded the new module, since the url has changed
    ASSERT_EQ(static_cast<size_t>(1), newWrapper.requestManager->getAllPerformedTasks().size());

    // They should be different, since the module content are different
    ASSERT_NE(*decompressedBundle, *newResult.value());

    newWrapper.dispatchQueue->sync([]() {});

    // We should still have one item on disk
    ASSERT_EQ(static_cast<size_t>(1), wrapper.disk->getAll().size());

    auto newDiskContent = wrapper.disk->getAll();

    // We should have the same key
    ASSERT_EQ(diskContent.begin()->first, newDiskContent.begin()->first);
    // But content should be different
    ASSERT_NE(diskContent.begin()->second, newDiskContent.begin()->second);
}

TEST(RemoteModuleManager, canLoadResources) {
    RemoteModuleManagerWrapper wrapper;

    ValdiArchiveBuilder assetsBuilder;
    assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
    assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
    auto assetsArtifactBytes = assetsBuilder.build();

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1.assets");
    wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), assetsArtifactBytes->toBytesView());

    auto manifest1 = makeShared<DownloadableModuleManifestWrapper>();
    auto assets = manifest1->pb.add_assets();
    assets->mutable_artifact()->set_url(url.slowToString());
    auto bytesView = assetsArtifactBytes->toBytesView();
    auto sha256Digest = BytesUtils::sha256(bytesView);
    assets->mutable_artifact()->set_sha256digest(sha256Digest->data(), sha256Digest->size());
    wrapper.remoteBundleManager->registerManifest(module, manifest1);

    auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
    wrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());
    auto result = resultHolder->waitForResult();

    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.value() != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

    auto resources = result.value();

    auto file1Path = resources->getResourceCacheUrl(STRING_LITERAL("file1"));
    auto file2Path = resources->getResourceCacheUrl(STRING_LITERAL("file2"));

    ASSERT_TRUE(file1Path.has_value());
    ASSERT_TRUE(file2Path.has_value());

    // The stored paths should be reachable in our disk

    auto file1Content = wrapper.disk->loadForAbsoluteURL(file1Path.value());
    auto file2Content = wrapper.disk->loadForAbsoluteURL(file2Path.value());

    ASSERT_TRUE(file1Content.success()) << file1Content.description();
    ASSERT_TRUE(file2Content.success()) << file2Content.description();

    ASSERT_EQ(file1Content.value().asStringView(), std::string_view("content1"));
    ASSERT_EQ(file2Content.value().asStringView(), std::string_view("content2"));
}

TEST(RemoteModuleManager, canLoadResourcesFromDisk) {
    Ref<InMemoryDiskCache> disk;

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1.assets");
    auto manifest = makeShared<DownloadableModuleManifestWrapper>();
    auto artifact = manifest->pb.add_assets()->mutable_artifact();
    artifact->set_url(url.slowToString());

    Ref<RemoteModuleResources> resources;

    // Do an initial load with an isolated module manager to populate the disk cache
    {
        RemoteModuleManagerWrapper wrapper;
        disk = wrapper.disk;

        ValdiArchiveBuilder assetsBuilder;
        assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
        assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
        auto assetsArtifactBytes = assetsBuilder.build();

        wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), assetsArtifactBytes->toBytesView());

        auto sha256Digest = BytesUtils::sha256(assetsArtifactBytes->toBytesView());
        artifact->set_sha256digest(sha256Digest->data(), sha256Digest->size());
        wrapper.remoteBundleManager->registerManifest(module, manifest);

        auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
        wrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

        auto result = resultHolder->waitForResult();

        ASSERT_TRUE(result.success());
        ASSERT_TRUE(result.value() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

        // Flush the work queue so that the remote bundle manager can store the module in the disk cache
        wrapper.dispatchQueue->sync([]() {});

        resources = result.value();
    }

    auto diskBefore = disk->getAll();

    // Now we should be able we query again with our cached disk

    // Re-use our disk, which should have the cache populated
    RemoteModuleManagerWrapper newWrapper(0, disk);

    newWrapper.remoteBundleManager->registerManifest(module, manifest);
    auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
    newWrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

    auto newResult = resultHolder->waitForResult();

    ASSERT_TRUE(newResult.success());
    ASSERT_TRUE(newResult.value() != nullptr);

    // No network requests should have been made
    ASSERT_TRUE(newWrapper.requestManager->getAllPerformedTasks().empty());

    ASSERT_EQ(*resources, *newResult.value());

    newWrapper.dispatchQueue->sync([]() {});

    auto diskAfter = disk->getAll();

    ASSERT_EQ(diskBefore, diskAfter);
}

TEST(RemoteModuleManager, reloadResourcesOnOutOfDateCache) {
    Ref<InMemoryDiskCache> disk;

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1.assets");
    auto manifest = makeShared<DownloadableModuleManifestWrapper>();
    auto artifact = manifest->pb.add_assets()->mutable_artifact();
    artifact->set_url(url.slowToString());

    Ref<RemoteModuleResources> resources;

    // Do an initial load with an isolated module manager to populate the disk cache
    {
        RemoteModuleManagerWrapper wrapper;
        disk = wrapper.disk;

        ValdiArchiveBuilder assetsBuilder;
        assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
        assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
        auto assetsArtifactBytes = assetsBuilder.build();

        wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), assetsArtifactBytes->toBytesView());

        auto sha256Digest = BytesUtils::sha256(assetsArtifactBytes->toBytesView());
        artifact->set_sha256digest(sha256Digest->data(), sha256Digest->size());
        wrapper.remoteBundleManager->registerManifest(module, manifest);

        auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
        wrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

        auto result = resultHolder->waitForResult();

        ASSERT_TRUE(result.success());
        ASSERT_TRUE(result.value() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

        // Flush the work queue so that the remote bundle manager can store the module in the disk cache
        wrapper.dispatchQueue->sync([]() {});

        resources = result.value();
    }

    auto diskBefore = disk->getAll();

    // Now we should be able we query again with our cached disk

    // Re-use our disk, which should have the cache populated
    RemoteModuleManagerWrapper newWrapper(0, disk);

    auto newUrl = STRING_LITERAL("http://snap.com/valdi/module1-new.assets");

    ValdiArchiveBuilder assetsBuilder;
    assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1.new")));
    assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2.new")));
    auto assetsArtifactBytes = assetsBuilder.build();

    newWrapper.requestManager->addMockedResponse(newUrl, STRING_LITERAL("GET"), assetsArtifactBytes->toBytesView());
    auto newArtifact = manifest->pb.mutable_assets(0)->mutable_artifact();
    newArtifact->set_url(newUrl.slowToString());
    auto sha256Digest = BytesUtils::sha256(assetsArtifactBytes->toBytesView());
    newArtifact->set_sha256digest(sha256Digest->data(), sha256Digest->size());
    newWrapper.remoteBundleManager->registerManifest(module, manifest);
    auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
    newWrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

    auto newResult = resultHolder->waitForResult();

    ASSERT_TRUE(newResult.success());
    ASSERT_TRUE(newResult.value() != nullptr);

    // We should have queried the network to get the new data
    ASSERT_EQ(static_cast<size_t>(1), newWrapper.requestManager->getAllPerformedTasks().size());

    // The cache paths should be the same
    ASSERT_EQ(*resources, *newResult.value());

    auto file1Path = resources->getResourceCacheUrl(STRING_LITERAL("file1"));
    auto file2Path = resources->getResourceCacheUrl(STRING_LITERAL("file2"));

    ASSERT_TRUE(file1Path.has_value());
    ASSERT_TRUE(file2Path.has_value());

    // The stored paths should be reachable in our disk

    auto file1Content = disk->loadForAbsoluteURL(file1Path.value());
    auto file2Content = disk->loadForAbsoluteURL(file2Path.value());

    ASSERT_TRUE(file1Content.success()) << file1Content.description();
    ASSERT_TRUE(file2Content.success()) << file2Content.description();

    // The file contents should reflect the updated resources

    ASSERT_EQ(file1Content.value().asStringView(), std::string_view("content1.new"));
    ASSERT_EQ(file2Content.value().asStringView(), std::string_view("content2.new"));

    newWrapper.dispatchQueue->sync([]() {});

    auto diskAfter = disk->getAll();

    ASSERT_NE(diskBefore, diskAfter);
}

void addAssetArtifact(const StringBox& url,
                      const Shared<RequestManagerMock>& requestManager,
                      const Ref<DownloadableModuleManifestWrapper>& manifest,
                      double deviceDensity) {
    auto assets1 = manifest->pb.add_assets();
    assets1->set_device_density(deviceDensity);
    auto assets1Artifact = assets1->mutable_artifact();
    assets1Artifact->set_url(url.slowToString());

    ValdiArchiveBuilder assetsBuilder;
    assetsBuilder.addEntry(
        ValdiArchiveEntry(STRING_FORMAT("density-{}", static_cast<int>(deviceDensity)), StringBox()));
    auto assetsArtifactBytes = assetsBuilder.build();
    auto sha256Digest = BytesUtils::sha256(assetsArtifactBytes->toBytesView());
    // Create a string to retain the memory (otherwise the digest bytes get destructed)
    auto sha256DigestStr = std::string(sha256Digest->toBytesView().asStringView());
    assets1Artifact->set_sha256digest(sha256DigestStr);

    requestManager->addMockedResponse(url, STRING_LITERAL("GET"), assetsArtifactBytes->toBytesView());
}

Result<Ref<RemoteModuleResources>> fetchAsset(const Shared<RequestManagerMock>& requestManager,
                                              const Ref<DownloadableModuleManifestWrapper>& manifest,
                                              const StringBox& module,
                                              double deviceDensity) {
    RemoteModuleManagerWrapper newWrapper(deviceDensity, Valdi::makeShared<InMemoryDiskCache>());

    newWrapper.requestManagerHolder.set(requestManager);
    newWrapper.remoteBundleManager->registerManifest(module, manifest);

    auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();

    newWrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

    return resultHolder->waitForResult();
}

TEST(RemoteBundleManager, canSelectArtifactFromDeviceDensity) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(ConsoleLogger::getLogger());

    auto module = STRING_LITERAL("Module1");

    auto urlLow = STRING_LITERAL("http://snap.com/valdi/module1.low.assets");
    auto urlMedium = STRING_LITERAL("http://snap.com/valdi/module1.medium.assets");
    auto urlHigh = STRING_LITERAL("http://snap.com/valdi/module1.high.assets");
    auto urlHighest = STRING_LITERAL("http://snap.com/valdi/module1.highest.assets");

    auto manifest = makeShared<DownloadableModuleManifestWrapper>();
    addAssetArtifact(urlLow, requestManager, manifest, 1);
    addAssetArtifact(urlMedium, requestManager, manifest, 2);
    addAssetArtifact(urlHigh, requestManager, manifest, 3);
    addAssetArtifact(urlHighest, requestManager, manifest, 4);

    [[maybe_unused]] auto& logger = ConsoleLogger::getLogger();
    for (int index = 0; index < manifest->pb.assets_size(); index++) {
        auto digest = manifest->pb.assets(index).artifact().sha256digest();
        VALDI_ERROR(logger, "Digest for {}: '{}'", index, digest);
    }

    auto resultLow = fetchAsset(requestManager, manifest, module, 0);
    ASSERT_TRUE(resultLow.success());

    // A module with a single file density-1 should have been returned
    ASSERT_TRUE(resultLow.value()->getResourceCacheUrl(STRING_LITERAL("density-1")).has_value());
    ASSERT_FALSE(resultLow.value()->getResourceCacheUrl(STRING_LITERAL("density-2")).has_value());
    ASSERT_FALSE(resultLow.value()->getResourceCacheUrl(STRING_LITERAL("density-3")).has_value());
    ASSERT_FALSE(resultLow.value()->getResourceCacheUrl(STRING_LITERAL("density-4")).has_value());

    auto resultMedium = fetchAsset(requestManager, manifest, module, 2.5);
    ASSERT_TRUE(resultMedium.success());

    // A module with a single file density-2 should have been returned
    ASSERT_FALSE(resultMedium.value()->getResourceCacheUrl(STRING_LITERAL("density-1")).has_value());
    ASSERT_TRUE(resultMedium.value()->getResourceCacheUrl(STRING_LITERAL("density-2")).has_value());
    ASSERT_FALSE(resultMedium.value()->getResourceCacheUrl(STRING_LITERAL("density-3")).has_value());
    ASSERT_FALSE(resultMedium.value()->getResourceCacheUrl(STRING_LITERAL("density-4")).has_value());

    auto resultHigh = fetchAsset(requestManager, manifest, module, 3);
    ASSERT_TRUE(resultHigh.success());

    // A module with a single file density-3 should have been returned
    ASSERT_FALSE(resultHigh.value()->getResourceCacheUrl(STRING_LITERAL("density-1")).has_value());
    ASSERT_FALSE(resultHigh.value()->getResourceCacheUrl(STRING_LITERAL("density-2")).has_value());
    ASSERT_TRUE(resultHigh.value()->getResourceCacheUrl(STRING_LITERAL("density-3")).has_value());
    ASSERT_FALSE(resultHigh.value()->getResourceCacheUrl(STRING_LITERAL("density-4")).has_value());

    auto resultHighest = fetchAsset(requestManager, manifest, module, 10);
    ASSERT_TRUE(resultHighest.success());

    // A module with a single file density-4 should have been returned
    ASSERT_FALSE(resultHighest.value()->getResourceCacheUrl(STRING_LITERAL("density-1")).has_value());
    ASSERT_FALSE(resultHighest.value()->getResourceCacheUrl(STRING_LITERAL("density-2")).has_value());
    ASSERT_FALSE(resultHighest.value()->getResourceCacheUrl(STRING_LITERAL("density-3")).has_value());
    ASSERT_TRUE(resultHighest.value()->getResourceCacheUrl(STRING_LITERAL("density-4")).has_value());
}

TEST(RemoteBundleManager, canRedownloadOnCorruptCache) {
    Ref<InMemoryDiskCache> disk;
    Shared<RequestManagerMock> requestManager;

    auto module = STRING_LITERAL("Module1");
    auto url = STRING_LITERAL("http://snap.com/valdi/module1.assets");
    auto manifest = makeShared<DownloadableModuleManifestWrapper>();
    auto artifact = manifest->pb.add_assets()->mutable_artifact();
    artifact->set_url(url.slowToString());

    Ref<RemoteModuleResources> resources;

    // Do an initial load with an isolated module manager to populate the disk cache
    {
        RemoteModuleManagerWrapper wrapper;
        disk = wrapper.disk;
        requestManager = wrapper.requestManager;

        ValdiArchiveBuilder assetsBuilder;
        assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file1"), STRING_LITERAL("content1")));
        assetsBuilder.addEntry(ValdiArchiveEntry(STRING_LITERAL("file2"), STRING_LITERAL("content2")));
        auto assetsArtifactBytes = assetsBuilder.build();

        wrapper.requestManager->addMockedResponse(url, STRING_LITERAL("GET"), assetsArtifactBytes->toBytesView());

        auto sha256Digest = BytesUtils::sha256(assetsArtifactBytes->toBytesView());
        artifact->set_sha256digest(sha256Digest->data(), sha256Digest->size());
        wrapper.remoteBundleManager->registerManifest(module, manifest);

        auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
        wrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

        auto result = resultHolder->waitForResult();

        ASSERT_TRUE(result.success());
        ASSERT_TRUE(result.value() != nullptr);
        ASSERT_EQ(static_cast<size_t>(1), wrapper.requestManager->getAllPerformedTasks().size());

        // Flush the work queue so that the remote bundle manager can store the module in the disk cache
        wrapper.dispatchQueue->sync([]() {});

        resources = result.value();
    }

    auto diskBefore = disk->getAll();

    // We corrupt our cache by deleting one of the file

    for (const auto& it : diskBefore) {
        if (it.first.hasSuffix("file2")) {
            ASSERT_TRUE(disk->removeForAbsolutePath(Path(it.first)));
        }
    }

    ASSERT_EQ(static_cast<size_t>(1), requestManager->getAllPerformedTasks().size());

    // Now we should be able we query again with our cached disk

    // Re-use our disk, which should have the corrupted cache populated
    RemoteModuleManagerWrapper newWrapper(0, disk);
    newWrapper.requestManagerHolder.set(requestManager);

    newWrapper.remoteBundleManager->registerManifest(module, manifest);
    auto resultHolder = ResultHolder<Ref<RemoteModuleResources>>::make();
    newWrapper.remoteBundleManager->loadResources(module, resultHolder->makeCompletion());

    auto newResult = resultHolder->waitForResult();

    ASSERT_TRUE(newResult.success());
    ASSERT_TRUE(newResult.value() != nullptr);

    // We should have downloaded the resources again
    ASSERT_EQ(static_cast<size_t>(2), requestManager->getAllPerformedTasks().size());

    ASSERT_EQ(*resources, *newResult.value());

    newWrapper.dispatchQueue->sync([]() {});

    auto diskAfter = disk->getAll();

    ASSERT_EQ(diskBefore, diskAfter);
}

} // namespace ValdiTest
