//
//  PersistentStoreModuleFactory.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/18/19.
//

#include "valdi/runtime/JavaScript/Modules/PersistentStoreModuleFactory.hpp"
#include "valdi/runtime/JavaScript/Modules/PersistentStore.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include <optional>

namespace Valdi {

constexpr std::string_view kPersistentStoresDirectory = "PersistentStores";

PersistentStoreModuleFactory::PersistentStoreModuleFactory(const Ref<IDiskCache>& diskCache,
                                                           const Ref<DispatchQueue>& dispatchQueue,
                                                           const SharedAtomicObject<UserSession>& userSession,
                                                           const Shared<snap::valdi::Keychain>& keychain,
                                                           ILogger& logger,
                                                           bool disableDecryptionByDefault)
    : _diskCache(diskCache),
      _dispatchQueue(dispatchQueue),
      _userSession(userSession),
      _keychain(keychain),
      _logger(logger),
      _disableEncryptionByDefault(disableDecryptionByDefault) {}

static void bindPersistentStoreMethod(const char* methodName,
                                      const Ref<ValueMap>& persistentStoreObject,
                                      const Ref<PersistentStore>& persistentStore,
                                      Value (*method)(PersistentStore*, const ValueFunctionCallContext&)) {
    (*persistentStoreObject)[STRING_LITERAL(methodName)] = Value(makeShared<ValueFunctionWithCallable>(
        [persistentStore, method](const ValueFunctionCallContext& callContext) -> Value {
            return method(persistentStore.get(), callContext);
        }));
}

StringBox PersistentStoreModuleFactory::getModulePath() {
    return STRING_LITERAL("PersistentStoreNative");
}

Valdi::Ref<PersistentStore> PersistentStoreModuleFactory::getOrCreatePersistentStore(
    const StringBox& stringPath,
    Ref<UserSession>& userSession,
    uint64_t maxWeight,
    bool disableBatchWrites,
    std::optional<bool> enableEncryption) {
    auto effectivePath = getEffectiveStorePath(stringPath, enableEncryption);

    std::lock_guard<Valdi::Mutex> guard(_existingStoreMutex);

    auto result = _existingStores.find(effectivePath);
    if (result != _existingStores.end()) {
        auto store = result->second;
        if (auto spt = store.lock()) {
            spt->setMaxWeight(maxWeight);
            spt->setBatchWritesDisabled(disableBatchWrites);
            spt->setUserSession(userSession);
            return spt;
        }
    }

    auto keychain = shouldEncrypt(enableEncryption) ? _keychain : NULL;

    auto persistentStore = Valdi::makeShared<PersistentStore>(
        effectivePath, _diskCache, userSession, keychain, _dispatchQueue, _logger, maxWeight, disableBatchWrites);
    _existingStores[effectivePath] = persistentStore;
    persistentStore->populate();
    return persistentStore;
}

// Use the COF value only if a non-null bool is provided
bool PersistentStoreModuleFactory::shouldEncrypt(std::optional<bool> enableEncryption) {
    return enableEncryption.value_or(!_disableEncryptionByDefault);
}

StringBox PersistentStoreModuleFactory::getEffectiveStorePath(const StringBox& basePath,
                                                              std::optional<bool> enableEncryption) {
    // We are AB'ing a change to the persistent store encryption behavior.
    // Appending a suffix to the store path will prevent reading old encrypted data.
    if (_disableEncryptionByDefault && !enableEncryption.has_value()) {
        return STRING_FORMAT("{}_V2", basePath);
    }
    return basePath;
}

Value PersistentStoreModuleFactory::loadModule() {
    auto module = makeShared<ValueMap>();

    auto strongThis = shared_from_this();
    (*module)[STRING_LITERAL("newPersistentStore")] =
        Value(makeShared<ValueFunctionWithCallable>([strongThis](const ValueFunctionCallContext& callContext) -> Value {
            auto nameResult = callContext.getParameterAsString(0);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            Path rootPath(kPersistentStoresDirectory);
            auto persistentStorePath = rootPath.appending(nameResult.toStringView());
            if (!persistentStorePath.startsWith(rootPath)) {
                callContext.getExceptionTracker().onError(fmt::format("Invalid filename '{}'", nameResult));
                return Value::undefined();
            }

            auto disableBatchWrites = callContext.getParameterAsBool(1);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto userScoped = callContext.getParameterAsBool(2);
            if (!callContext.getExceptionTracker()) {
                return Value::undefined();
            }

            auto maxWeight = callContext.getParameter(3).toLong();

            std::optional<bool> enableEncryption = callContext.getParameter(6).isNullOrUndefined() ?
                                                       std::nullopt :
                                                       std::optional<bool>{callContext.getParameter(6).toBool()};

            Ref<UserSession> userSession;
            if (userScoped) {
                if (callContext.getParameter(5).isNullOrUndefined()) {
                    userSession = strongThis->_userSession.get();
                } else {
                    auto userId = callContext.getParameterAsString(5);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    userSession = Valdi::makeShared<UserSession>(userId);
                }

                if (userSession == nullptr) {
                    callContext.getExceptionTracker().onError(
                        Error("Cannot create a userScoped store without a user session"));
                    return Value::undefined();
                }
                if (strongThis->_keychain == nullptr) {
                    callContext.getExceptionTracker().onError(
                        Error("Cannot create a userScoped store without a keychain store"));
                    return Value::undefined();
                }
            }

            uint64_t mockedTime = 0;
            if (!callContext.getParameter(4).isNullOrUndefined()) {
                mockedTime = static_cast<uint64_t>(callContext.getParameter(4).toInt());
            }

            auto persistentStore = strongThis->getOrCreatePersistentStore(
                StringCache::getGlobal().makeString(persistentStorePath.toString()),
                userSession,
                static_cast<uint64_t>(maxWeight),
                disableBatchWrites,
                enableEncryption);

            if (mockedTime != 0) {
                persistentStore->setCurrentTimeSeconds(mockedTime);
            }

            auto persistentStoreObject = makeShared<ValueMap>();
            bindPersistentStoreMethod(
                "store",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto key = callContext.getParameterAsString(0);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    BytesView blob;
                    const auto& blobValue = callContext.getParameter(1);
                    if (blobValue.isString()) {
                        auto str = blobValue.toStringBox();
                        blob = BytesView(
                            str.getInternedString(), reinterpret_cast<const Byte*>(str.getCStr()), str.length());
                    } else if (blobValue.isTypedArray()) {
                        blob = blobValue.getTypedArray()->getBuffer();
                    } else {
                        callContext.getExceptionTracker().onError(Error("Blob should be passed as string or bytes"));
                        return Value::undefined();
                    }

                    const auto& ttlSecondsValue = callContext.getParameter(2);
                    auto ttlSeconds = ttlSecondsValue.isNumber() ? ttlSecondsValue.toInt() : 0;

                    const auto& weightValue = callContext.getParameter(3);
                    int64_t weight = weightValue.isNumber() ? weightValue.toLong() : 0;

                    auto callback = callContext.getParameterAsFunction(4);
                    ;
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    persistentStore->store(key,
                                           blob,
                                           static_cast<uint64_t>(ttlSeconds),
                                           static_cast<uint64_t>(weight),
                                           [callback](const Result<Void>& result) {
                                               auto param = Value::undefined();
                                               if (!result) {
                                                   param = Value(result.error().toString());
                                               }

                                               (*callback)(&param, 1);
                                           });

                    return Value::undefined();
                });
            bindPersistentStoreMethod(
                "fetch",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto key = callContext.getParameterAsString(0);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    auto callback = callContext.getParameterAsFunction(1);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    auto asString = callContext.getParameter(2).toBool();

                    persistentStore->fetch(key, [callback, asString](Result<BytesView> result) {
                        std::array<Value, 2> params;
                        if (!result) {
                            params[0] = Value::undefined();
                            params[1] = Value(result.error().toString());
                        } else {
                            if (asString) {
                                params[0] = Value(StringCache::getGlobal().makeString(result.value().asStringView()));
                            } else {
                                auto typedArray =
                                    Valdi::makeShared<ValueTypedArray>(TypedArrayType::ArrayBuffer, result.moveValue());
                                params[0] = Value(typedArray);
                            }
                            params[1] = Value::undefined();
                        }

                        (*callback)(params.data(), params.size());
                    });

                    return Value::undefined();
                });

            bindPersistentStoreMethod(
                "exists",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto key = callContext.getParameterAsString(0);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    auto callback = callContext.getParameterAsFunction(1);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    persistentStore->exists(key, [callback](bool result) { (*callback)({Value(result)}); });

                    return Value::undefined();
                });

            bindPersistentStoreMethod(
                "remove",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto key = callContext.getParameterAsString(0);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    auto callback = callContext.getParameterAsFunction(1);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    persistentStore->remove(key, [callback](const Result<Void>& result) {
                        Value param = Value::undefined();
                        if (!result) {
                            param = Value(result.error().toString());
                        }
                        (*callback)(&param, 1);
                    });

                    return Value::undefined();
                });

            bindPersistentStoreMethod(
                "removeAll",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto callback = callContext.getParameterAsFunction(0);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }
                    persistentStore->removeAll([callback](const Result<Void>& result) {
                        auto param = Value::undefined();
                        if (!result) {
                            param = Value(result.error().toString());
                        }
                        (*callback)(&param, 1);
                    });

                    return Value::undefined();
                });
            bindPersistentStoreMethod(
                "fetchAll",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto callback = callContext.getParameterAsFunction(0);
                    if (!callContext.getExceptionTracker()) {
                        return Value::undefined();
                    }

                    persistentStore->fetchAll(
                        [callback](const std::vector<std::pair<StringBox, KeyValueStoreEntry>>& entries) {
                            auto propertyList = ValueArray::make(entries.size() * 2);

                            size_t i = 0;
                            for (const auto& entry : entries) {
                                propertyList->emplace(i++, Value(entry.first));
                                propertyList->emplace(
                                    i++,
                                    Value(makeShared<ValueTypedArray>(TypedArrayType::ArrayBuffer, entry.second.data)));
                            }

                            (*callback)({Value(propertyList)});
                        });
                    return Value::undefined();
                });

            bindPersistentStoreMethod(
                "setCurrentTime",
                persistentStoreObject,
                persistentStore,
                [](PersistentStore* persistentStore, const ValueFunctionCallContext& callContext) -> Value {
                    auto timeSeconds = callContext.getParameter(0).toInt();
                    persistentStore->setCurrentTimeSeconds(static_cast<uint64_t>(timeSeconds));
                    return Value::undefined();
                });
            return Value(persistentStoreObject);
        }));

    return Value(module);
}
} // namespace Valdi
