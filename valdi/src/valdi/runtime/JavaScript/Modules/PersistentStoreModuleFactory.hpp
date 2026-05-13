//
//  PersistentStoreModuleFactory.hpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/18/19.
//

#pragma once

#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi/runtime/JavaScript/Modules/PersistentStore.hpp"
#include "valdi/runtime/Resources/UserSession.hpp"
#include "valdi/runtime/Utils/SharedAtomic.hpp"
#include "valdi_core/ModuleFactory.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"

namespace snap::valdi {
class Keychain;
}

namespace Valdi {

class ILogger;

class PersistentStoreModuleFactory : public snap::valdi_core::ModuleFactory,
                                     public std::enable_shared_from_this<PersistentStoreModuleFactory> {
public:
    PersistentStoreModuleFactory(const Ref<IDiskCache>& diskCache,
                                 const Ref<DispatchQueue>& dispatchQueue,
                                 const SharedAtomicObject<UserSession>& userSession,
                                 const Shared<snap::valdi::Keychain>& keychain,
                                 ILogger& logger,
                                 bool disableDecryptionByDefault);

    StringBox getModulePath() override;
    Value loadModule() override;

    // Visible for testing
    Valdi::Ref<PersistentStore> getOrCreatePersistentStore(const StringBox& stringPath,
                                                           Ref<UserSession>& userSession,
                                                           uint64_t maxWeight,
                                                           bool disableBatchWrites,
                                                           std::optional<bool> enableEncryption);

private:
    Ref<IDiskCache> _diskCache;
    Ref<DispatchQueue> _dispatchQueue;
    SharedAtomicObject<UserSession> _userSession;
    Shared<snap::valdi::Keychain> _keychain;
    ILogger& _logger;
    FlatMap<StringBox, Weak<PersistentStore>> _existingStores;
    Valdi::Mutex _existingStoreMutex;
    bool _disableEncryptionByDefault;
    bool shouldEncrypt(std::optional<bool> enableEncryption);
    StringBox getEffectiveStorePath(const StringBox& basePath, std::optional<bool> enableEncryption);
};

} // namespace Valdi
