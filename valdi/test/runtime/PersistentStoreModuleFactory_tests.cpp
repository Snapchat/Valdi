//
//  PersistentStoreModuleFactory_tests.cpp
//

#include <gtest/gtest.h>

#include "valdi/runtime/JavaScript/Modules/PersistentStore.hpp"
#include "valdi/runtime/JavaScript/Modules/PersistentStoreModuleFactory.hpp"
#include "valdi/runtime/Resources/UserSession.hpp"
#include "valdi/runtime/Utils/SharedAtomic.hpp"

#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/InMemoryKeychain.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

using namespace Valdi;

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

TEST(PersistentStoreModuleFactory, createsNewStoreIfDeallocate) {
    PersistentStoreDependencies dependencies;

    auto userSession = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));
    SharedAtomicObject<UserSession> sharedUserSession;
    sharedUserSession.set(userSession);

    auto storeFactory = Valdi::makeShared<PersistentStoreModuleFactory>(dependencies.diskCache,
                                                                        dependencies.dispatchQueue,
                                                                        sharedUserSession,
                                                                        dependencies.keyChain,
                                                                        dependencies.logger);

    Path rootPath("wut");

    auto storePath = StringCache::getGlobal().makeString(rootPath.appending("stuff").toString());

    auto store = storeFactory->getOrCreatePersistentStore(storePath, userSession, 5, true, true);
    dependencies.dispatchQueue->sync([]() {});
    Weak<PersistentStore> weakStore = store;

    store = nullptr;
    store = storeFactory->getOrCreatePersistentStore(storePath, userSession, 5, true, true);
    dependencies.dispatchQueue->sync([]() {});

    ASSERT_NE(store, nullptr);
    ASSERT_EQ(weakStore.use_count(), 0);
    ASSERT_FALSE(weakStore.lock());
    ASSERT_NE(weakStore.lock(), store);
}

TEST(PersistentStoreModuleFactory, reusesStoreWithSamePath) {
    PersistentStoreDependencies dependencies;

    auto userSession = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));
    SharedAtomicObject<UserSession> sharedUserSession;
    sharedUserSession.set(userSession);

    auto storeFactory = Valdi::makeShared<PersistentStoreModuleFactory>(dependencies.diskCache,
                                                                        dependencies.dispatchQueue,
                                                                        sharedUserSession,
                                                                        dependencies.keyChain,
                                                                        dependencies.logger);

    Path rootPath("wut");

    auto store0Path = StringCache::getGlobal().makeString(rootPath.appending("stuff").toString());
    auto store1Path = StringCache::getGlobal().makeString(rootPath.appending("things").toString());

    auto store0 = storeFactory->getOrCreatePersistentStore(store0Path, userSession, 5, true, true);
    auto store1 = storeFactory->getOrCreatePersistentStore(store1Path, userSession, 5, true, true);
    auto store2 = storeFactory->getOrCreatePersistentStore(store0Path, userSession, 5, true, true);

    ASSERT_NE(store0, store1);
    ASSERT_NE(store1, store2);
    ASSERT_EQ(store0, store2);
}

TEST(PersistentStoreModuleFactory, passesUpdatedConfigurationWhenReusingStore) {
    PersistentStoreDependencies dependencies;

    SharedAtomicObject<UserSession> sharedUserSession;

    auto storeFactory = Valdi::makeShared<PersistentStoreModuleFactory>(dependencies.diskCache,
                                                                        dependencies.dispatchQueue,
                                                                        sharedUserSession,
                                                                        dependencies.keyChain,
                                                                        dependencies.logger);

    auto userSession1 = Valdi::makeShared<UserSession>(STRING_LITERAL("4242"));
    auto userSession2 = Valdi::makeShared<UserSession>(STRING_LITERAL("4343"));

    auto path = STRING_LITERAL("i/am/an/id");

    auto store = storeFactory->getOrCreatePersistentStore(path, userSession1, 5, false, true);

    ASSERT_EQ(userSession1, store->getUserSession());
    ASSERT_EQ(5ul, store->getMaxWeight());
    ASSERT_FALSE(store->batchWritesDisabled());

    auto updatedStore = storeFactory->getOrCreatePersistentStore(path, userSession2, 10, true, true);

    ASSERT_EQ(store, updatedStore);

    ASSERT_EQ(userSession2, store->getUserSession());
    ASSERT_EQ(10ul, store->getMaxWeight());
    ASSERT_TRUE(store->batchWritesDisabled());
}

} // namespace ValdiTest
