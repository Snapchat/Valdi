//
//  DiskStore_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 4/9/20.
//

#include <gtest/gtest.h>

#include "valdi/runtime/JavaScript/Modules/PersistentStore.hpp"
#include "valdi/runtime/Resources/EncryptedDiskCache.hpp"
#include "valdi/runtime/Resources/UserSession.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi/runtime/Utils/SharedAtomic.hpp"

#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/InMemoryKeychain.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

using namespace Valdi;

// NOTE: Most of the tests are already done in PersistentStoreTest.ts
// Those tests are testing more specific logic that are invisible to the
// consumers of the store.

namespace ValdiTest {

struct PersistentStoreDependencies {
    Ref<DispatchQueue> dispatchQueue;
    Ref<InMemoryDiskCache> diskCache;
    Shared<InMemoryKeychain> keyChain;
    ILogger& logger;

    PersistentStoreDependencies()
        : dispatchQueue(DispatchQueue::create(STRING_LITERAL("PersistentStore"), ThreadQoSClassMax)),
          diskCache(Valdi::makeShared<InMemoryDiskCache>()),
          keyChain(Valdi::makeShared<InMemoryKeychain>()),
          logger(ConsoleLogger::getLogger()) {}

    ~PersistentStoreDependencies() {
        dispatchQueue->fullTeardown();
    }
};

TEST(PersistentStore, generatesAKeyWhenCreatedTheFirstTime) {
    PersistentStoreDependencies dependencies;

    auto metadata = dependencies.keyChain->get(EncryptedDiskCache::getKeychainKey());

    ASSERT_TRUE(metadata.empty());

    auto userSession = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));

    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    userSession,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    metadata = dependencies.keyChain->get(EncryptedDiskCache::getKeychainKey());
}

TEST(PersistentStore, encryptsDataWhenKeychainSet) {
    PersistentStoreDependencies dependencies;
    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    nullptr,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    AsyncGroup group;
    group.enter();
    store->store(STRING_LITERAL("item"),
                 makeShared<ByteBuffer>("Hello World")->toBytesView(),
                 0,
                 0,
                 [&](const auto& storeResult) { group.leave(); });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));

    auto items = dependencies.diskCache->getAll();
    ASSERT_EQ(static_cast<size_t>(1), items.size());

    auto archiveData = dependencies.diskCache->load(Path(items.begin()->first));
    ASSERT_TRUE(archiveData) << archiveData.description();
    auto archiveStr = StringCache::getGlobal().makeString(archiveData.value().asStringView());
    ASSERT_FALSE(archiveStr.contains("Hello World"));
}

TEST(PersistentStore, keepsDataInPlainTextWhenKeychainNotSet) {
    PersistentStoreDependencies dependencies;
    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    nullptr,
                                                    nullptr,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    AsyncGroup group;
    group.enter();
    store->store(STRING_LITERAL("item"),
                 makeShared<ByteBuffer>("Hello World")->toBytesView(),
                 0,
                 0,
                 [&](const auto& storeResult) { group.leave(); });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));

    auto items = dependencies.diskCache->getAll();
    ASSERT_EQ(static_cast<size_t>(1), items.size());

    auto archiveData = dependencies.diskCache->load(Path(items.begin()->first));
    ASSERT_TRUE(archiveData) << archiveData.description();
    auto archiveStr = StringCache::getGlobal().makeString(archiveData.value().asStringView());
    ASSERT_TRUE(archiveStr.contains("Hello World"));
}

TEST(PersistentStore, usesGlobalDirectoryWhenUserUserSessionNotSet) {
    PersistentStoreDependencies dependencies;
    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    nullptr,
                                                    nullptr,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    AsyncGroup group;
    group.enter();
    store->store(STRING_LITERAL("item"),
                 makeShared<ByteBuffer>("Hello World")->toBytesView(),
                 0,
                 0,
                 [&](const auto& storeResult) { group.leave(); });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));

    auto archiveData = dependencies.diskCache->load(Path("global/somepath"));
    ASSERT_TRUE(archiveData) << archiveData.description();
}

TEST(PersistentStore, usesUserScopedDirectoryWhenUserSessionSet) {
    PersistentStoreDependencies dependencies;
    auto userSession = makeShared<UserSession>(STRING_LITERAL("4242"));
    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    userSession,
                                                    nullptr,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);

    AsyncGroup group;
    group.enter();
    store->store(STRING_LITERAL("item"),
                 makeShared<ByteBuffer>("Hello World")->toBytesView(),
                 0,
                 0,
                 [&](const auto& storeResult) { group.leave(); });
    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));

    auto archiveData = dependencies.diskCache->load(Path("4242/somepath"));
    ASSERT_TRUE(archiveData) << archiveData.description();
}

TEST(PersistentStore, supportsUpdatingUserSession) {
    PersistentStoreDependencies dependencies;

    auto userSession = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));

    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    userSession,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    auto baseFile = makeShared<Bytes>();
    baseFile->emplace_back(42);

    SharedAtomic<Result<Void>> result;
    AsyncGroup group;

    group.enter();
    store->store(STRING_LITERAL("heyhey"), BytesView(baseFile), 0, 0, [&](const auto& storeResult) {
        result.set(storeResult);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_TRUE(result.get().success());

    // Updating UserSession with another one with the same userId
    store->setUserSession(makeShared<UserSession>(STRING_LITERAL("4242")));

    SharedAtomic<Result<BytesView>> fetchResult;

    group.enter();
    store->fetch(STRING_LITERAL("heyhey"), [&](const auto& result) {
        fetchResult.set(result);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_TRUE(fetchResult.get().success());

    ASSERT_EQ(BytesView(baseFile), fetchResult.get().value());

    // Now updating with a user session that has a different userId
    store->setUserSession(makeShared<UserSession>(STRING_LITERAL("4343")));

    group.enter();
    store->fetch(STRING_LITERAL("heyhey"), [&](const auto& result) {
        fetchResult.set(result);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_FALSE(fetchResult.get().success());
}

TEST(PersistentStore, canStoreAndFetchEncryptedData) {
    PersistentStoreDependencies dependencies;

    auto userSession = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));

    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    userSession,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    auto baseFile = makeShared<Bytes>();
    baseFile->emplace_back(42);

    SharedAtomic<Result<Void>> result;
    AsyncGroup group;

    group.enter();
    store->store(STRING_LITERAL("heyhey"), BytesView(baseFile), 0, 0, [&](const auto& storeResult) {
        result.set(storeResult);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_TRUE(result.get().success());

    // Now fetch
    SharedAtomic<Result<BytesView>> fetchResult;

    group.enter();
    store->fetch(STRING_LITERAL("heyhey"), [&](const auto& result) {
        fetchResult.set(result);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_TRUE(fetchResult.get().success());

    ASSERT_EQ(BytesView(baseFile), fetchResult.get().value());
}

TEST(PersistentStore, canRestoreEncryptedData) {
    PersistentStoreDependencies dependencies;

    auto userSession = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));

    // Create a store and populate it

    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    userSession,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    auto baseFile = makeShared<Bytes>();
    baseFile->emplace_back(42);

    SharedAtomic<Result<Void>> result;
    AsyncGroup group;

    group.enter();
    store->store(STRING_LITERAL("heyhey"), BytesView(baseFile), 0, 0, [&](const auto& storeResult) {
        result.set(storeResult);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_TRUE(result.get().success());

    // Recreate a new store with the user session
    store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                               dependencies.diskCache,
                                               userSession,
                                               dependencies.keyChain,
                                               dependencies.dispatchQueue,
                                               dependencies.logger,
                                               0,
                                               true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    // Now fetch
    SharedAtomic<Result<BytesView>> fetchResult;

    group.enter();
    store->fetch(STRING_LITERAL("heyhey"), [&](const auto& result) {
        fetchResult.set(result);
        group.leave();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    ASSERT_TRUE(fetchResult.get().success());

    ASSERT_EQ(BytesView(baseFile), fetchResult.get().value());
}

TEST(PersistentStore, supportsEvictionByWeight) {
    PersistentStoreDependencies dependencies;
    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    nullptr,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    10,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    store->store(STRING_LITERAL("item1"), BytesView(), 0, 1, [](const auto&) {});
    store->store(STRING_LITERAL("item2"), BytesView(), 0, 1, [](const auto&) {});
    store->store(STRING_LITERAL("item3"), BytesView(), 0, 1, [](const auto&) {});

    SharedAtomic<std::vector<std::pair<StringBox, KeyValueStoreEntry>>> result;
    AsyncGroup group;

    group.enter();
    store->fetchAll([&](const auto& entries) {
        result.set(entries);
        group.leave();
    });

    group.blockingWaitWithTimeout(std::chrono::seconds(5));

    auto entries = result.get();

    ASSERT_EQ(static_cast<size_t>(3), entries.size());
    ASSERT_EQ(STRING_LITERAL("item1"), entries[0].first);
    ASSERT_EQ(STRING_LITERAL("item2"), entries[1].first);
    ASSERT_EQ(STRING_LITERAL("item3"), entries[2].first);

    // Now store an item with a weight which will make the store evict previous items

    store->store(STRING_LITERAL("item4"), BytesView(), 0, 9, [](const auto&) {});

    group.enter();
    store->fetchAll([&](const auto& entries) {
        result.set(entries);
        group.leave();
    });

    group.blockingWaitWithTimeout(std::chrono::seconds(5));

    entries = result.get();

    ASSERT_EQ(static_cast<size_t>(2), entries.size());
    ASSERT_EQ(STRING_LITERAL("item3"), entries[0].first);
    ASSERT_EQ(STRING_LITERAL("item4"), entries[1].first);
}

TEST(PersistentStore, restoredCacheSupportsEviction) {
    PersistentStoreDependencies dependencies;
    auto store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                                    dependencies.diskCache,
                                                    nullptr,
                                                    dependencies.keyChain,
                                                    dependencies.dispatchQueue,
                                                    dependencies.logger,
                                                    0,
                                                    true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    store->store(STRING_LITERAL("item1"), BytesView(), 0, 1, [](const auto&) {});
    store->store(STRING_LITERAL("item2"), BytesView(), 0, 1, [](const auto&) {});
    store->store(STRING_LITERAL("item3"), BytesView(), 0, 1, [](const auto&) {});

    dependencies.dispatchQueue->sync([]() {});

    // Restore the instance
    store = Valdi::makeShared<PersistentStore>(STRING_LITERAL("somepath"),
                                               dependencies.diskCache,
                                               nullptr,
                                               dependencies.keyChain,
                                               dependencies.dispatchQueue,
                                               dependencies.logger,
                                               10,
                                               true);
    store->populate();
    dependencies.dispatchQueue->sync([]() {});

    // Store an item with a weight which will make the store evict previous items
    store->store(STRING_LITERAL("item4"), BytesView(), 0, 9, [](const auto&) {});

    SharedAtomic<std::vector<std::pair<StringBox, KeyValueStoreEntry>>> result;
    AsyncGroup group;

    group.enter();
    store->fetchAll([&](const auto& entries) {
        result.set(entries);
        group.leave();
    });

    group.blockingWaitWithTimeout(std::chrono::seconds(5));

    auto entries = result.get();

    ASSERT_EQ(static_cast<size_t>(2), entries.size());
    ASSERT_EQ(STRING_LITERAL("item3"), entries[0].first);
    ASSERT_EQ(STRING_LITERAL("item4"), entries[1].first);
}

static BytesView makeBytes(std::initializer_list<Byte> data) {
    auto output = makeShared<ByteBuffer>();
    output->set(data);

    return output->toBytesView();
}

// Sanity checks since the updateSequence is used in the tests above
TEST(InMemoryKeychain, increaseSequenceWhenUpdated) {
    PersistentStoreDependencies dependencies;

    ASSERT_EQ(static_cast<uint64_t>(0), dependencies.keyChain->getUpdateSequence());

    dependencies.keyChain->store(STRING_LITERAL("hello"), makeBytes({}));

    ASSERT_EQ(static_cast<uint64_t>(1), dependencies.keyChain->getUpdateSequence());

    dependencies.keyChain->store(STRING_LITERAL("hello"), makeBytes({0, 1, 2, 3}));

    ASSERT_EQ(static_cast<uint64_t>(2), dependencies.keyChain->getUpdateSequence());

    dependencies.keyChain->store(STRING_LITERAL("nice"), makeBytes({42}));

    ASSERT_EQ(static_cast<uint64_t>(3), dependencies.keyChain->getUpdateSequence());

    dependencies.keyChain->get(STRING_LITERAL("badKey"));
    dependencies.keyChain->get(STRING_LITERAL("hello"));

    // Fetch don't mutate the sequence
    ASSERT_EQ(static_cast<uint64_t>(3), dependencies.keyChain->getUpdateSequence());

    dependencies.keyChain->erase(STRING_LITERAL("hello"));

    ASSERT_EQ(static_cast<uint64_t>(4), dependencies.keyChain->getUpdateSequence());
}

} // namespace ValdiTest
