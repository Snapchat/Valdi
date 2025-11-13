//
//  AssetLoader.cpp
//  valdi
//
//  Created by Simon Corsin on 6/28/21.
//

#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Resources/AssetBytesStore.hpp"
#include "valdi/runtime/Resources/AssetCatalog.hpp"
#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#include "valdi/runtime/Resources/AssetLoaderManager.hpp"
#include "valdi/runtime/Resources/AssetLoaderRequestHandler.hpp"
#include "valdi/runtime/Resources/AssetRequestPayloadCache.hpp"
#include "valdi/runtime/Resources/AssetsManagerTransaction.hpp"
#include "valdi/runtime/Resources/Bundle.hpp"
#include "valdi/runtime/Resources/ObservableAsset.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleManager.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleResources.hpp"
#include "valdi/runtime/Resources/RemoteDownloaderToAssetLoaderAdapter.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

#include "valdi/runtime/Interfaces/IResourceLoader.hpp"

#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"

#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include "valdi_core/AssetLoadObserver.hpp"

#include "utils/debugging/Assert.hpp"

namespace Valdi {

constexpr bool kEnableAssetsLogs = false;

AssetsManager::AssetsManager(const Shared<IResourceLoader>& resourceLoader,
                             const Ref<RemoteModuleManager>& remoteModuleManager,
                             const Ref<AssetLoaderManager>& assetLoaderManager,
                             const Ref<DispatchQueue>& workerQueue,
                             MainThreadManager& mainThreadManager,
                             ILogger& logger)
    : _resourceLoader(resourceLoader),
      _remoteModuleManager(remoteModuleManager),
      _assetLoaderManager(assetLoaderManager),
      _workerQueue(workerQueue),
      _mainThreadManager(mainThreadManager),
      _logger(logger) {}

AssetsManager::~AssetsManager() = default;

Ref<Asset> AssetsManager::getAsset(const AssetKey& assetKey) {
    auto guard = lock();

    return lockFreeGetAsset(assetKey);
}

Ref<Asset> AssetsManager::createAssetWithBytes(const BytesView& bytes) {
    auto guard = lock();
    if (_assetBytesStore == nullptr) {
        _assetBytesStore = makeShared<AssetBytesStore>();
        // This makes it possible to load non bytes asset for URLs that we generate from the AssetBytesStore.
        auto urlScheme = AssetBytesStore::getUrlScheme();
        _assetLoaderManager->registerDownloaderForScheme(urlScheme, _assetBytesStore);
        // This makes it possible to load bytes asset for URLs that we generate from the AssetBytesStore.
        _assetLoaderManager->registerAssetLoader(
            makeShared<RemoteDownloaderToAssetLoaderAdapter>(_assetBytesStore, std::vector<StringBox>({urlScheme})));
    }

    auto assetKey = AssetKey(_assetBytesStore->registerAssetBytes(bytes));
    return lockFreeGetAsset(assetKey);
}

Ref<Asset> AssetsManager::lockFreeGetAsset(const AssetKey& assetKey) {
    auto managedAsset = getOrCreateManagedAsset(assetKey);

    auto observable = managedAsset->getObservable();
    if (observable != nullptr) {
        return observable;
    }
    observable = createObservable(assetKey);
    managedAsset->setObservable(observable);
    return observable;
}

bool AssetsManager::isAssetAlive(const AssetKey& assetKey) const {
    auto guard = lock();
    return getManagedAsset(assetKey) != nullptr;
}

std::optional<AssetLocation> AssetsManager::getResolvedAssetLocation(const AssetKey& assetKey) {
    auto guard = lock();

    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr) {
        return std::nullopt;
    }

    if (managedAsset->getResolvedAssetLocation()) {
        return {managedAsset->getResolvedAssetLocation().value()};
    }

    return std::nullopt;
}

void AssetsManager::setResolvedAssetLocation(const AssetKey& assetKey, const AssetLocation& assetLocation) {
    auto guard = lock();
    auto asset = getOrCreateManagedAsset(assetKey);

    if (asset->getState() == AssetStateReady && asset->getResolvedAssetLocation().value() == assetLocation) {
        // Nothing to do
        return;
    }

    if (asset->getState() == AssetStateReady) {
        for (size_t i = 0; i < asset->getConsumersSize(); i++) {
            const auto& assetConsumer = asset->getConsumer(i);
            assetConsumer->setLoadedAsset(Result<Ref<LoadedAsset>>());
            assetConsumer->setState(AssetConsumerStateInitial);
            assetConsumer->setNotified(false);
            updateConsumerRequestHandler(assetConsumer, nullptr);
        }
    }

    asset->setResolveId(0);
    asset->clearPayloadCache();
    asset->setResolvedAssetLocation(assetLocation);
    asset->setState(AssetStateReady);

    if (asset->hasConsumers()) {
        scheduleAssetUpdate(std::move(guard), assetKey);
    }
}

static Result<Ref<AssetCatalog>> getAssetCatalogForBundle(const Ref<Bundle>& bundle) {
    static auto kAssetCatalogPath = STRING_LITERAL("res");
    return bundle->getAssetCatalog(kAssetCatalogPath);
}

static void updateObservableAssetSize(const Ref<ObservableAsset>& observableAsset,
                                      const StringBox& assetPath,
                                      const Result<Ref<AssetCatalog>>& assetCatalog) {
    int expectedWidth = 0;
    int expectedHeight = 0;
    if (assetCatalog) {
        auto assetSpecs = assetCatalog.value()->getAssetSpecsForName(assetPath);
        if (assetSpecs) {
            expectedWidth = assetSpecs.value().getWidth();
            expectedHeight = assetSpecs.value().getHeight();
        }
    }

    observableAsset->setExpectedSize(expectedWidth, expectedHeight);
}

void AssetsManager::onAssetCatalogChanged(const Ref<Bundle>& bundle) {
    auto assetCatalog = getAssetCatalogForBundle(bundle);
    auto guard = lock();

    for (const auto& it : _assets) {
        if (it.first.getBundle() == bundle) {
            auto observable = it.second->getObservable();
            if (observable != nullptr) {
                updateObservableAssetSize(observable, it.first.getPath(), assetCatalog);
            }
        }
    }
}

bool AssetsManager::isAssetUrl(const StringBox& str) {
    if (str.contains("://")) {
        return true;
    }
    return str.hasPrefix("data:image/");
}

Shared<ObservableAsset> AssetsManager::createObservable(const AssetKey& assetKey) {
    auto asset = makeShared<ObservableAsset>(assetKey, weakRef(this));
    if (!assetKey.isURL()) {
        auto assetCatalog = getAssetCatalogForBundle(assetKey.getBundle());
        updateObservableAssetSize(asset, assetKey.getPath(), assetCatalog);
    }

    return asset.toShared();
}

void AssetsManager::addAssetLoadObserver(const AssetKey& assetKey,
                                         const Shared<snap::valdi_core::AssetLoadObserver>& observer,
                                         const Ref<Context>& context,
                                         snap::valdi_core::AssetOutputType outputType,
                                         int32_t preferredWidth,
                                         int32_t preferredHeight,
                                         const Value& attachedData) {
    auto guard = lock();

    auto managedAsset = getOrCreateManagedAsset(assetKey);

    auto consumer = managedAsset->addConsumer();
    consumer->setContext(context);
    consumer->setObserver(observer);
    consumer->setOutputType(outputType);
    consumer->setPreferredWidth(preferredWidth);
    consumer->setPreferredHeight(preferredHeight);
    consumer->setAttachedData(attachedData);

    if (managedAsset->getState() == AssetStateFailedRetryable) {
        // Retry the resolving now that we have a new consumer
        managedAsset->setState(AssetStateInitial);
    }

    scheduleAssetUpdate(std::move(guard), assetKey);
}

void AssetsManager::removeAssetLoadObserver(const AssetKey& assetKey,
                                            const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& observer) {
    auto guard = lock();

    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr) {
        return;
    }

    for (size_t i = 0; i < managedAsset->getConsumersSize(); i++) {
        const auto& consumer = managedAsset->getConsumer(i);
        if (consumer->getObserver() == observer) {
            consumer->setObserver(nullptr);
            break;
        }
    }

    scheduleAssetUpdate(std::move(guard), assetKey);
}

void AssetsManager::updateAssetLoadObserverPreferredSize(
    const AssetKey& /*assetKey*/,
    const std::shared_ptr<snap::valdi_core::AssetLoadObserver>& /*observer*/,
    int32_t /*preferredWidth*/,
    int32_t /*preferredHeight*/) {
    // TODO(simon): Implement in a later PR
}

void AssetsManager::updateAsset(AssetsManagerTransaction& transaction, const AssetKey& assetKey) {
    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr) {
        return;
    }

    auto state = managedAsset->getState();
    if constexpr (kEnableAssetsLogs) {
        VALDI_INFO(_logger, "Updating asset '{}' with state {}", assetKey, state);
    }

    if (!removeManagedAssetIfNeeded(assetKey, managedAsset)) {
        switch (state) {
            case AssetStateInitial: {
                if (managedAsset->hasConsumers()) {
                    resolveAssetLocation(transaction, assetKey, managedAsset);
                }
            } break;
            case AssetStateResolvingLocation:
                break;
            case AssetStateFailedPermanently:
            case AssetStateFailedRetryable:
            case AssetStateReady:
                updateAssetConsumers(transaction, assetKey, managedAsset);
                break;
        }
    }

    if (_listener != nullptr) {
        _listener->onManagedAssetUpdated(managedAsset);
    }
}

bool AssetsManager::removeManagedAssetIfNeeded(const AssetKey& assetKey, const Ref<ManagedAsset>& managedAsset) {
    if ((!assetKey.isURL() && !_removeUnusedLocalAssets) || managedAsset->hasConsumers() ||
        managedAsset->getObservable() != nullptr) {
        return false;
    }

    // We have a URL ManagedAsset with no consumers and no observables, we can remove it from our index
    // since no one is currently interested in it.
    const auto& it = _assets.find(assetKey);
    if (it != _assets.end()) {
        _assets.erase(it);
    }

    if (_assetBytesStore != nullptr && AssetBytesStore::isAssetBytesUrl(assetKey.getUrl())) {
        _assetBytesStore->unregisterAssetBytes(assetKey.getUrl());
    }

    return true;
}

void AssetsManager::scheduleAssetUpdate(std::unique_lock<std::recursive_mutex> lock, const AssetKey& assetKey) {
    auto* transaction = AssetsManagerTransaction::current();
    if (transaction != nullptr) {
        scheduleAssetUpdate(*transaction, assetKey);
    } else {
        if constexpr (kEnableAssetsLogs) {
            VALDI_INFO(_logger, "Schedule asset update for '{}' outside of transaction", assetKey);
        }

        auto needScheduleUpdates = _pauseUpdatesCount == 0 && _scheduledUpdates.empty();
        _scheduledUpdates.emplace_back(assetKey);

        if (needScheduleUpdates) {
            if (_mainThreadManager.currentThreadIsMainThread()) {
                performUpdates(std::move(lock));
            } else {
                lock.unlock();
                schedulePerformUpdates();
            }
        }
    }
}

void AssetsManager::schedulePerformUpdates() {
    _mainThreadManager.dispatch(nullptr, [self = strongSmallRef(this)]() { self->performUpdates(self->lock()); });
}

void AssetsManager::scheduleAssetUpdate(AssetsManagerTransaction& transaction, const AssetKey& assetKey) {
    if constexpr (kEnableAssetsLogs) {
        VALDI_INFO(_logger, "Schedule asset update for '{}' in transaction", assetKey);
    }

    transaction.enqueueUpdate(assetKey);
}

void AssetsManager::resolveAssetLocation(AssetsManagerTransaction& transaction,
                                         const AssetKey& assetKey,
                                         const Ref<ManagedAsset>& managedAsset) {
    SC_ASSERT(managedAsset->getState() == AssetStateInitial);
    managedAsset->setState(AssetStateResolvingLocation);

    auto resolveId = ++_assetResolveIdSequence;
    managedAsset->setResolveId(resolveId);

    if constexpr (kEnableAssetsLogs) {
        VALDI_INFO(_logger, "Resolving asset location of '{}'", assetKey);
    }

    const auto& bundle = assetKey.getBundle();
    if (bundle != nullptr) {
        if (bundle->hasRemoteAssets()) {
            transaction.releaseLock();
            _remoteModuleManager->loadResources(
                bundle->getName(), [assetKey, weakSelf = weakRef(this), resolveId](auto result) {
                    auto self = strongRef(weakSelf);
                    if (self == nullptr) {
                        return;
                    }

                    self->_workerQueue->async([assetKey, self, result, resolveId]() {
                        self->onLoadingRemoteResourcesCompleted(assetKey, result, resolveId);
                    });
                });
        } else {
            transaction.releaseLock();
            _workerQueue->async([assetKey, self = strongSmallRef(this), resolveId]() {
                self->resolveLocalAssetLocationAndUpdate(assetKey, resolveId);
            });
        }
    } else {
        // For URL assets, the resolved asset location is always a URL
        updateAssetLocation(assetKey, managedAsset, AssetLocation(assetKey.getUrl(), false));

        scheduleAssetUpdate(transaction, assetKey);
    }
}

Result<AssetLocation> AssetsManager::resolveRemoteAssetLocation(const AssetKey& assetKey,
                                                                const Result<Ref<RemoteModuleResources>>& result) {
    if (!result) {
        return result.error();
    }

    auto cacheUrl = result.value()->getResourceCacheUrl(assetKey.getPath());
    if (cacheUrl) {
        return AssetLocation(cacheUrl.value(), false);
    }

    if (_resourceLoader != nullptr) {
        auto url = _resourceLoader->resolveLocalAssetURL(assetKey.getBundle()->getName(), assetKey.getPath());
        if (!url.isEmpty()) {
            return AssetLocation(url, true);
        }
    }

    auto serialized = ValueArray::make(result.value()->getAllUrls().size());

    size_t i = 0;
    for (const auto& it : result.value()->getAllUrls()) {
        serialized->emplace(i++, Value(it.first));
    }

    return Error(STRING_FORMAT("Did not find asset '{}' in remote module '{}', candidates are: {}",
                               assetKey.getPath(),
                               assetKey.getBundle()->getName(),
                               Value(serialized).toString()));
}

Result<AssetLocation> AssetsManager::resolveLocalAssetLocation(const AssetKey& assetKey) {
    if (_resourceLoader != nullptr) {
        auto url = _resourceLoader->resolveLocalAssetURL(assetKey.getBundle()->getName(), assetKey.getPath());
        if (!url.isEmpty()) {
            return AssetLocation(url, true);
        }
    }

    return Error(STRING_FORMAT(
        "Did not find asset '{}' in local module '{}'", assetKey.getPath(), assetKey.getBundle()->getName()));
}

void AssetsManager::resolveLocalAssetLocationAndUpdate(const AssetKey& assetKey, uint64_t resolveId) {
    auto location = resolveLocalAssetLocation(assetKey);

    auto guard = lock();

    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr || managedAsset->getResolveId() != resolveId) {
        return;
    }

    updateAssetLocation(assetKey, managedAsset, location);

    scheduleAssetUpdate(std::move(guard), assetKey);
}

void AssetsManager::updateAssetLocation(const AssetKey& assetKey,
                                        const Ref<ManagedAsset>& managedAsset,
                                        const Result<AssetLocation>& assetLocation) {
    SC_ASSERT(managedAsset->getState() == AssetStateResolvingLocation);
    if constexpr (kEnableAssetsLogs) {
        if (assetLocation) {
            VALDI_INFO(_logger, "Updated asset location of '{}'", assetKey);
        }
    }

    if (!assetLocation) {
        VALDI_WARN(_logger, "Failed to update asset location of '{}': {}", assetKey, assetLocation.error());
    }

    if (assetLocation) {
        managedAsset->setState(AssetStateReady);
        managedAsset->setResolvedAssetLocation({assetLocation.value()});
    } else {
        managedAsset->setState(AssetStateFailedPermanently);
        managedAsset->setResolvedAssetLocation(assetLocation.error());
    }
}

void AssetsManager::onLoadingRemoteResourcesCompleted(const AssetKey& assetKey,
                                                      const Result<Ref<RemoteModuleResources>>& result,
                                                      uint64_t resolveId) {
    auto resolvedAssetLocation = resolveRemoteAssetLocation(assetKey, result);

    auto guard = lock();

    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr) {
        if constexpr (kEnableAssetsLogs) {
            VALDI_DEBUG(_logger, "No ManagedAsset found for asset '{]'", assetKey);
        }
        return;
    }
    if (managedAsset->getResolveId() != resolveId) {
        if constexpr (kEnableAssetsLogs) {
            VALDI_DEBUG(_logger,
                        "ManagedAsset '{}' has different resolveId ({} from expected {})",
                        assetKey,
                        managedAsset->getResolveId(),
                        resolveId);
        }
        return;
    }

    SC_ASSERT(managedAsset->getState() == AssetStateResolvingLocation);

    if (result) {
        updateAssetLocation(assetKey, managedAsset, resolvedAssetLocation);
    } else {
        managedAsset->setState(AssetStateFailedRetryable);
        managedAsset->setResolvedAssetLocation({result.error()});
    }

    scheduleAssetUpdate(std::move(guard), assetKey);
}

static Ref<AssetConsumer> getNextConsumerToUpdate(const Ref<ManagedAsset>& managedAsset, bool& hasMore) {
    Ref<AssetConsumer> consumerToUpdate;

    for (size_t i = 0; i < managedAsset->getConsumersSize(); i++) {
        const auto& consumer = managedAsset->getConsumer(i);

        if (consumer->getObserver() == nullptr) {
            if (consumerToUpdate == nullptr) {
                consumerToUpdate = consumer;
            } else {
                hasMore = true;
                continue;
            }
        }

        if (consumer->notified()) {
            continue;
        }

        switch (consumer->getState()) {
            case AssetConsumerStateInitial:
                [[fallthrough]];
            case AssetConsumerStateFailed:
                [[fallthrough]];
            case AssetConsumerStateLoaded: {
                if (consumerToUpdate == nullptr) {
                    consumerToUpdate = consumer;
                } else {
                    hasMore = true;
                    if (consumerToUpdate->getObserver() == nullptr) {
                        // Take priority over processing of observer removal
                        consumerToUpdate = consumer;
                    }
                    return consumerToUpdate;
                }
            } break;
            case AssetConsumerStateLoading:
                break;
            case AssetConsumerStateRemoved:
                break;
        }
    }

    hasMore = false;

    return consumerToUpdate;
}

void AssetsManager::removeAssetConsumer([[maybe_unused]] AssetsManagerTransaction& transaction,
                                        const Ref<ManagedAsset>& managedAsset,
                                        const Ref<AssetConsumer>& assetConsumer) {
    managedAsset->removeConsumer(assetConsumer);
    assetConsumer->setState(AssetConsumerStateRemoved);

    assetConsumer->setLoadedAsset(Result<Ref<LoadedAsset>>());
    updateConsumerRequestHandler(assetConsumer, nullptr);
}

void AssetsManager::updateAssetConsumers(AssetsManagerTransaction& transaction,
                                         const AssetKey& assetKey,
                                         const Ref<ManagedAsset>& managedAsset) {
    SC_ASSERT(managedAsset->getState() == AssetStateReady || managedAsset->getState() == AssetStateFailedPermanently ||
              managedAsset->getState() == AssetStateFailedRetryable);

    bool hasMore = false;
    auto consumerToUpdate = getNextConsumerToUpdate(managedAsset, hasMore);
    if (consumerToUpdate == nullptr) {
        return;
    }

    if (hasMore) {
        scheduleAssetUpdate(transaction, assetKey);
    }

    doUpdateAssetConsumer(transaction, assetKey, managedAsset, consumerToUpdate);
}

void AssetsManager::doUpdateAssetConsumer(AssetsManagerTransaction& transaction,
                                          const AssetKey& assetKey,
                                          const Ref<ManagedAsset>& managedAsset,
                                          const Ref<AssetConsumer>& consumerToUpdate) {
    if (consumerToUpdate->getObserver() == nullptr) {
        removeAssetConsumer(transaction, managedAsset, consumerToUpdate);
        return;
    }

    switch (consumerToUpdate->getState()) {
        case AssetConsumerStateInitial: {
            if (managedAsset->getState() == AssetStateFailedRetryable ||
                managedAsset->getState() == AssetStateFailedPermanently) {
                consumerToUpdate->setState(AssetConsumerStateFailed);
                consumerToUpdate->setLoadedAsset(managedAsset->getResolvedAssetLocation().error());
                scheduleAssetUpdate(transaction, assetKey);
            } else {
                loadAssetForConsumerAtResolvedLocation(transaction,
                                                       assetKey,
                                                       managedAsset,
                                                       consumerToUpdate,
                                                       managedAsset->getResolvedAssetLocation().value());
            }
        } break;
        case AssetConsumerStateLoading: {
            SC_ASSERT(consumerToUpdate->getObserver() == nullptr);
            removeAssetConsumer(transaction, managedAsset, consumerToUpdate);
        } break;
        case AssetConsumerStateFailed: {
            notifyAssetConsumer(transaction,
                                assetKey,
                                managedAsset,
                                consumerToUpdate,
                                nullptr,
                                consumerToUpdate->getLoadedAsset().error());
        } break;
        case AssetConsumerStateLoaded: {
            notifyAssetConsumer(transaction,
                                assetKey,
                                managedAsset,
                                consumerToUpdate,
                                consumerToUpdate->getLoadedAsset().value(),
                                std::nullopt);
        } break;
        case AssetConsumerStateRemoved:
            break;
    }
}

void AssetsManager::notifyAssetConsumer(AssetsManagerTransaction& transaction,
                                        const AssetKey& assetKey,
                                        const Ref<ManagedAsset>& managedAsset,
                                        const Ref<AssetConsumer>& assetConsumer,
                                        const Ref<LoadedAsset>& loadedAsset,
                                        const std::optional<Error>& error) {
    auto observable = managedAsset->getObservable();
    assetConsumer->setNotified(true);
    transaction.releaseLock();

    std::optional<StringBox> errorStringBox;
    if (error) {
        auto errorString = error.value().toStringBox();
        VALDI_WARN(_logger, "Notifying error for consumer of Asset '{}': {}", assetKey, errorString);
        errorStringBox = {errorString};
    }

    assetConsumer->getObserver()->onLoad(observable, Value(loadedAsset), errorStringBox);

    transaction.acquireLock();
}

void AssetsManager::updateConsumerRequestHandler(const Ref<AssetConsumer>& assetConsumer,
                                                 const Ref<AssetLoaderRequestHandler>& request) {
    auto existingRequest = castOrNull<AssetLoaderRequestHandler>(assetConsumer->getAssetLoaderCompletion());
    assetConsumer->setAssetLoaderCompletion(request);

    if (existingRequest != nullptr) {
        auto consumersCount = existingRequest->decrementConsumersCount();

        if (consumersCount == 0 && !existingRequest->scheduledForCancelation()) {
            existingRequest->setScheduledForCancelation();
            _pendingLoadRequests.emplace_back(existingRequest);

            scheduleFlushLoadRequests();
        }
    }

    if (request != nullptr) {
        request->incrementConsumersCount();

        if (!request->scheduledForLoad()) {
            request->setScheduledForLoad();

            _pendingLoadRequests.emplace_back(request);

            scheduleFlushLoadRequests();
        }
    }
}

static const char* stringFromOutputType(snap::valdi_core::AssetOutputType outputType) {
    return snap::valdi_core::to_string(outputType);
}

void AssetsManager::loadAssetForConsumerAtResolvedLocation(AssetsManagerTransaction& transaction,
                                                           const AssetKey& assetKey,
                                                           const Ref<ManagedAsset>& managedAsset,
                                                           const Ref<AssetConsumer>& assetConsumer,
                                                           const AssetLocation& assetLocation) {
    auto assetLoader =
        _assetLoaderManager->resolveAssetLoader(assetLocation.getScheme(), assetConsumer->getOutputType());

    if (assetLoader == nullptr) {
        assetConsumer->setState(AssetConsumerStateFailed);
        assetConsumer->setLoadedAsset(
            Error(STRING_FORMAT("Cannot resolve AssetLoader for URL scheme '{}' and output type '{}'",
                                assetLocation.getScheme(),
                                stringFromOutputType(assetConsumer->getOutputType()))));
        scheduleAssetUpdate(transaction, assetKey);
        return;
    }

    if constexpr (kEnableAssetsLogs) {
        VALDI_INFO(_logger, "Starting to load asset '{}'", assetKey);
    }

    assetConsumer->setState(AssetConsumerStateLoading);

    auto preferredWidth = assetConsumer->getPreferredWidth();
    auto preferredHeight = assetConsumer->getPreferredHeight();
    auto attachedData = assetConsumer->getAttachedData();

    if (assetLoader->canReuseLoadedAssets()) {
        auto consumersSize = managedAsset->getConsumersSize();
        for (size_t i = 0; i < consumersSize; i++) {
            auto consumer = managedAsset->getConsumer(i);
            auto requestHandler = castOrNull<AssetLoaderRequestHandler>(consumer->getAssetLoaderCompletion());
            if (requestHandler != nullptr && requestHandler->getRequestedWidth() == preferredWidth &&
                requestHandler->getRequestedHeight() == preferredHeight &&
                requestHandler->getAttachedData() == attachedData &&
                consumer->getOutputType() == assetConsumer->getOutputType()) {
                updateConsumerRequestHandler(assetConsumer, requestHandler);

                if (!requestHandler->getLastLoadResult().empty()) {
                    onConsumerLoad(assetConsumer, requestHandler->getLastLoadResult());
                    scheduleAssetUpdate(transaction, assetKey);
                }

                return;
            }
        }
    }

    auto payloadCache = managedAsset->getPayloadCacheForAssetLoader(assetLoader);

    auto requestHandler = makeShared<AssetLoaderRequestHandler>(weakRef(this),
                                                                assetConsumer->getContext(),
                                                                assetKey,
                                                                payloadCache,
                                                                assetLocation.getUrl(),
                                                                preferredWidth,
                                                                preferredHeight,
                                                                attachedData);
    updateConsumerRequestHandler(assetConsumer, requestHandler);
}

void AssetsManager::onObservableDestroyed(const AssetKey& assetKey) {
    if constexpr (kEnableAssetsLogs) {
        VALDI_INFO(_logger, "Observable of '{}' destroyed", assetKey);
    }

    auto guard = lock();
    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr) {
        return;
    }

    scheduleAssetUpdate(std::move(guard), assetKey);
}

void AssetsManager::onLoad(const Ref<AssetLoaderRequestHandler>& request, const Result<Ref<LoadedAsset>>& result) {
    VALDI_TRACE("Valdi.onAssetLoaded")

    const auto& assetKey = request->getAssetKey();
    if constexpr (kEnableAssetsLogs) {
        if (result) {
            VALDI_INFO(_logger, "Asset '{}' finished loaded", assetKey);
        }
    }

    if (!result) {
        VALDI_WARN(_logger, "Asset '{}' finished loaded with error: {}", assetKey, result.error());
    }

    auto guard = lock();

    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr || request->scheduledForCancelation()) {
        return;
    }

    request->setLastLoadResult(result);

    auto consumersSize = managedAsset->getConsumersSize();
    for (size_t i = 0; i < consumersSize; i++) {
        const auto& assetConsumer = managedAsset->getConsumer(i);
        if (assetConsumer->getAssetLoaderCompletion() == request) {
            onConsumerLoad(assetConsumer, result);
        }
    }

    scheduleAssetUpdate(std::move(guard), assetKey);
}

void AssetsManager::onConsumerLoad(const Ref<AssetConsumer>& assetConsumer, const Result<Ref<LoadedAsset>>& result) {
    assetConsumer->setNotified(false);

    if (result) {
        const auto& loadedAsset = result.value();
        if (loadedAsset != nullptr) {
            assetConsumer->setState(AssetConsumerStateLoaded);
            assetConsumer->setLoadedAsset(loadedAsset);
        } else {
            assetConsumer->setState(AssetConsumerStateFailed);
            assetConsumer->setLoadedAsset(Error("AssetLoader provided a null asset"));
        }
    } else {
        assetConsumer->setState(AssetConsumerStateFailed);
        assetConsumer->setLoadedAsset(result.error());
    }
}

void AssetsManager::scheduleFlushLoadRequests() {
    if (!_pendingLoadRequestsScheduled && !_pendingLoadRequests.empty()) {
        _pendingLoadRequestsScheduled = true;
        _workerQueue->async([self = strongSmallRef(this)]() { self->flushLoadRequests(); });
    }
}

void AssetsManager::flushLoadRequests() {
    auto guard = lock();

    while (!_pendingLoadRequests.empty() && _pauseUpdatesCount == 0) {
        auto loadRequest = _pendingLoadRequests.front();
        _pendingLoadRequests.pop_front();

        if (loadRequest->scheduledForCancelation()) {
            auto lastLoadRequestResult = loadRequest->getLastLoadResult();
            // Also clear the last load result ref so that we don't need to wait until the request instance is
            // deallocated
            loadRequest->setLastLoadResult(Result<Ref<LoadedAsset>>());

            guard.unlock();
            loadRequest->cancel();
            lastLoadRequestResult = Result<Ref<LoadedAsset>>();
        } else {
            guard.unlock();
            loadRequest->startLoadIfNeeded();
        }

        guard.lock();
    }

    _pendingLoadRequestsScheduled = false;
}

void AssetsManager::beginPauseUpdates() {
    auto guard = lock();
    _pauseUpdatesCount++;
}

void AssetsManager::flushUpdates() {
    auto guard = lock();

    if (_scheduledUpdates.empty() || !_mainThreadManager.currentThreadIsMainThread()) {
        return;
    }

    performUpdates(std::move(guard));
}

void AssetsManager::endPauseUpdates() {
    auto guard = lock();

    SC_ASSERT(_pauseUpdatesCount > 0);

    bool isMainThread = _mainThreadManager.currentThreadIsMainThread();

    if (_pauseUpdatesCount == 1 && !_scheduledUpdates.empty() && isMainThread) {
        performUpdates(std::move(guard));
        guard = lock();
    }

    _pauseUpdatesCount--;

    if (_pauseUpdatesCount == 0) {
        scheduleFlushLoadRequests();
        if (!_scheduledUpdates.empty()) {
            if (!isMainThread) {
                guard.unlock();
                schedulePerformUpdates();
            } else {
                // We had an incoming update that happened while performing updates.
                // Flush them now
                performUpdates(std::move(guard));
            }
        }
    }
}

void AssetsManager::performUpdates(std::unique_lock<std::recursive_mutex>&& lock) {
    SC_ASSERT(_mainThreadManager.currentThreadIsMainThread());
    VALDI_TRACE("Valdi.performAssetsUpdates")

    AssetsManagerTransaction transaction(std::move(lock));
    AssetsManagerTransaction::setCurrent(&transaction);

    for (const auto& scheduledUpdate : _scheduledUpdates) {
        transaction.enqueueUpdate(scheduledUpdate);
    }
    if constexpr (kEnableAssetsLogs) {
        VALDI_INFO(_logger, "Performing assets updates with {} initial operations", _scheduledUpdates.size());
    }
    _scheduledUpdates.clear();

    [[maybe_unused]] size_t i = 0;
    for (;;) {
        auto update = transaction.dequeueUpdate();
        if (!update) {
            break;
        }

        if constexpr (kEnableAssetsLogs) {
            VALDI_INFO(_logger, "Performing asset update #{}", i);
        }

        transaction.acquireLock();

        updateAsset(transaction, update.value());
        i++;
    }

    AssetsManagerTransaction::setCurrent(nullptr);

    if (_listener != nullptr) {
        transaction.releaseLock();
        _listener->onPerformedUpdates();
    }
}

Ref<ManagedAsset> AssetsManager::getManagedAsset(const AssetKey& assetKey) const {
    const auto& it = _assets.find(assetKey);

    if (it == _assets.end()) {
        return nullptr;
    }

    return it->second;
}

Ref<ManagedAsset> AssetsManager::getOrCreateManagedAsset(const AssetKey& assetKey) {
    auto managedAsset = getManagedAsset(assetKey);
    if (managedAsset == nullptr) {
        managedAsset = makeShared<ManagedAsset>();
        _assets[assetKey] = managedAsset;
    }

    return managedAsset;
}

std::unique_lock<std::recursive_mutex> AssetsManager::lock() const {
    return std::unique_lock<std::recursive_mutex>(_mutex);
}

void AssetsManager::setListener(AssetsManagerListener* listener) {
    _listener = listener;
}

void AssetsManager::setShouldRemoveUnusedLocalAssets(bool removeUnusedLocalAssets) {
    auto guard = lock();
    _removeUnusedLocalAssets = removeUnusedLocalAssets;
}

} // namespace Valdi
