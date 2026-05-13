//
//  ResourceManager.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Resources/AssetDensityResolver.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleManager.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModulePrefetchTask.hpp"
#include "valdi/runtime/Resources/Remote/RemoteModuleResources.hpp"
#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi_core/cpp/Resources/ValdiArchive.hpp"

#include "valdi_core/cpp/Constants.hpp"

#include "valdi/runtime/Interfaces/IDiskCache.hpp"
#include "valdi/runtime/Interfaces/IResourceLoader.hpp"

#include "valdi/runtime/Resources/Remote/DownloadableModuleManifestWrapper.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"

#include "valdi/runtime/Utils/BytesUtils.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"

#include "utils/time/StopWatch.hpp"
#include "valdi_core/cpp/Context/ComponentPath.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Parser.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

#include <chrono>
#include <fmt/format.h>
#include <fmt/ostream.h>

namespace Valdi {

ResourceManager::ResourceManager(const Shared<IResourceLoader>& resourceLoader,
                                 const Ref<IDiskCache>& diskCache,
                                 const Ref<AssetLoaderManager>& assetLoaderManager,
                                 const Holder<Shared<snap::valdi_core::HTTPRequestManager>>& requestManager,
                                 const Ref<DispatchQueue>& workerQueue,
                                 MainThreadManager& mainThreadManager,
                                 double deviceDensity,
                                 bool hotReloaderEnabled,
                                 ILogger& logger)
    : _resourceLoader(resourceLoader),
      _diskCache(diskCache),
      _workerQueue(workerQueue),
      _deviceDensity(deviceDensity),
      _hotReloaderEnabled(hotReloaderEnabled),
      _logger(logger) {
    _remoteModuleManager =
        makeShared<RemoteModuleManager>(diskCache, requestManager, _workerQueue, _logger, _deviceDensity);
    _assetsManager = makeShared<AssetsManager>(
        resourceLoader, _remoteModuleManager, assetLoaderManager, workerQueue, mainThreadManager, logger);
}

ResourceManager::~ResourceManager() = default;

static StringBox resolveModuleArchiveFilePath(const StringBox& modulePath) {
    return modulePath.append(".valdimodule");
}

static StringBox resolveSourceMapFilePath(const StringBox& modulePath) {
    return modulePath.append(".map.json");
}

Result<Ref<ValdiModuleArchive>> ResourceManager::getArchiveForModule(const StringBox& modulePath) {
    auto bundleFilePath = resolveModuleArchiveFilePath(modulePath);

    auto bundleContent = _resourceLoader->loadModuleContent(bundleFilePath);
    if (!bundleContent) {
        // Fallback on .valdimodule for backward compatibility
        auto bundleContentAlternative = _resourceLoader->loadModuleContent(modulePath.append(".valdimodule"));
        if (!bundleContentAlternative) {
            return bundleContent.error().rethrow(STRING_FORMAT("Unable to load module '{}'", modulePath));
        } else {
            bundleContent = std::move(bundleContentAlternative);
        }
    }

    const auto& data = bundleContent.value();

    auto result = ValdiModuleArchive::decompress(data.data(), data.size());
    if (!result) {
        return result.moveError();
    }
    return Valdi::makeShared<ValdiModuleArchive>(result.moveValue());
}

BundleInitializer ResourceManager::registerBundle(const StringBox& bundleName) {
    auto bundle = makeShared<Bundle>(bundleName, _logger);
    _bundleByName[bundleName] = bundle;

    return bundle->prepareForInit();
}

void ResourceManager::initializeBundle(BundleInitializer& bundleInitializer, Ref<ValdiModuleArchive> moduleArchive) {
    static auto kDownloadManifestName = STRING_LITERAL("download_manifest");

    const auto& bundle = bundleInitializer.getBundle();
    const auto& bundleName = bundle->getName();

    if (moduleArchive->containsEntry(kDownloadManifestName)) {
        // This is a downlodable module
        auto entry = moduleArchive->getEntry(kDownloadManifestName);
        SC_ASSERT(entry.has_value(), "Unable to retrieve module entry");
        auto manifest = makeShared<DownloadableModuleManifestWrapper>();

        if (!manifest->pb.ParseFromArray(entry->data, static_cast<int>(entry->size))) {
            VALDI_ERROR(_logger, "Invalid download manifest in module '{}'", bundleName);
            initializeBundle(bundleInitializer, makeShared<ValdiModuleArchive>());
            return;
        }

        _remoteModuleManager->registerManifest(bundleName, manifest);

        auto hasRemoteAssets = manifest->pb.assets_size() > 0;

        if (manifest->pb.has_artifact()) {
            // This is a remote module
            bundleInitializer.initWithRemoteArchive(hasRemoteAssets);
        } else {
            bundleInitializer.initWithLocalArchive(std::move(moduleArchive), hasRemoteAssets);
        }
    } else {
        // This is a regular module
        bundleInitializer.initWithLocalArchive(std::move(moduleArchive), false);
    }
}

void ResourceManager::doInsertImageAssetInBundle(const Ref<Bundle>& bundle,
                                                 const StringBox& filePath,
                                                 const BytesView& imageData) {
    auto extension = filePath.substring(filePath.lastIndexOf('.').value() + 1);
    auto assetFilename = ResourcesBundleResultTransformer::sanitizeAssetFilename(filePath);
    if (!assetFilename) {
        VALDI_ERROR(_logger, "Unable to insert image in bundle '{}': {}", bundle->getName(), assetFilename.error());
        return;
    }

    auto sha256 = BytesUtils::sha256String(imageData);

    auto directory = getImageAssetsOverrideDirectory();

    auto outputPath = directory.appending(fmt::format("{}.{}", sha256, extension));

    if (!_diskCache->exists(outputPath)) {
        auto storeResult = _diskCache->store(outputPath, imageData);
        if (!storeResult) {
            VALDI_ERROR(_logger, "Unable to insert image in bundle '{}': {}", bundle->getName(), storeResult.error());
            return;
        }
    }

    auto absoluteUrl = _diskCache->getAbsoluteURL(outputPath);
    _assetsManager->setResolvedAssetLocation(AssetKey(bundle, assetFilename.value()),
                                             AssetLocation(absoluteUrl, false));
}

void ResourceManager::insertImageAssetInBundle(const Ref<Bundle>& bundle,
                                               const StringBox& filePath,
                                               const BytesView& imageData) {
    _workerQueue->async([self = strongSmallRef(this), bundle, filePath, imageData]() {
        self->doInsertImageAssetInBundle(bundle, filePath, imageData);
    });
}

void ResourceManager::insertAssetPackageInBundle(const Ref<Bundle>& bundle, const BytesView& assetPackageData) {
    _workerQueue->async([self = strongSmallRef(this), bundle, assetPackageData]() {
        auto result = ValdiModuleArchive::decompress(assetPackageData.data(), assetPackageData.size());
        if (!result) {
            VALDI_ERROR(self->_logger,
                        "Failed to decompress asset bundle in bundle '{}': {}",
                        bundle->getName(),
                        result.error());
            return;
        }

        const auto& assetPackage = result.value();

        AssetDensityResolver resolver;

        for (const auto& entryPath : assetPackage.getAllEntryPaths()) {
            resolver.appendDensity(Value(entryPath).toDouble());
        }

        auto bestIndex = resolver.select(self->_deviceDensity);
        if (!bestIndex) {
            return;
        }

        auto assetsEntry = assetPackage.getEntryForIndex(bestIndex.value());

        ValdiArchive archive(assetsEntry.data, assetsEntry.data + assetsEntry.size);
        auto allFiles = archive.getEntries();
        if (!allFiles) {
            VALDI_ERROR(self->_logger,
                        "Failed to extract archive of asset bundle '{}' at index '{}': {}",
                        bundle->getName(),
                        bestIndex.value(),
                        allFiles.error());
            return;
        }

        for (const auto& asset : allFiles.value()) {
            auto bytes = BytesView(result.value().getDecompressedContent().getSource(), asset.data, asset.dataLength);
            self->doInsertImageAssetInBundle(bundle, asset.filePath, bytes);
        }
    });
}

void ResourceManager::onAssetCatalogChanged(const Ref<Bundle>& bundle) {
    _assetsManager->onAssetCatalogChanged(bundle);
}

Path ResourceManager::getImageAssetsOverrideDirectory() {
    Path path("hotreloaded_assets");
    std::lock_guard<Mutex> guard(_mutex);
    if (!_didSetupImageAssetOverrideDirectory) {
        _didSetupImageAssetOverrideDirectory = true;
        _diskCache->remove(path);
    }

    return path;
}

bool ResourceManager::isBundleLoaded(const StringBox& bundleName) {
    std::lock_guard<Mutex> guard(_mutex);
    const auto& it = _bundleByName.find(bundleName);
    if (it == _bundleByName.end()) {
        return false;
    }
    return !it->second->hasRemoteArchiveNeedingLoad();
}

bool ResourceManager::bundleHasRemoteSources(const StringBox& bundleName) {
    return getBundle(bundleName)->hasRemoteArchive();
}

bool ResourceManager::bundleHasRemoteAssets(const StringBox& bundleName) {
    return getBundle(bundleName)->hasRemoteAssets();
}

Ref<Bundle> ResourceManager::getBundle(const StringBox& bundleName) {
    std::unique_lock<Mutex> lock(_mutex);
    const auto& it = _bundleByName.find(bundleName);
    if (it != _bundleByName.end()) {
        return it->second;
    }

    VALDI_TRACE_META("Valdi.loadBundle", bundleName);
    snap::utils::time::StopWatch sw;
    sw.start();

    auto bundleInitializer = registerBundle(bundleName);
    auto inlineAssetsEnabled = _inlineAssetsEnabled;

    // We now have a lock on the Bundle itself. Release our lock so that
    // other threads can query the ResourceManager on other bundles.
    lock.unlock();

    auto assetPackageKey = STRING_LITERAL("res.assetpackage");
    auto hasAssetPackage = false;

    auto archiveResult = getArchiveForModule(bundleName);
    if (!archiveResult) {
        if (!_hotReloaderEnabled) {
            VALDI_ERROR(_logger, "Failed to load archive of Module '{}': {}", bundleName, archiveResult.error());
        }
        initializeBundle(bundleInitializer, makeShared<ValdiModuleArchive>());
    } else {
        hasAssetPackage = inlineAssetsEnabled && archiveResult.value()->containsEntry(assetPackageKey);
        initializeBundle(bundleInitializer, archiveResult.value());
    }

    populateSourceMap(bundleName, *bundleInitializer.getBundle());

    if (hasAssetPackage) {
        insertAssetPackageInBundle(bundleInitializer.getBundle(),
                                   bundleInitializer.getBundle()->getEntry(assetPackageKey).value());
    }

    if (Valdi::traceLoadModules) {
        VALDI_INFO(_logger, "Loaded bundle '{}' in {}", bundleName, sw.elapsed());
    }

    return bundleInitializer.getBundle();
}

/**
 * Returns the list of all loaded bundle names.
 * Returns:
 * - A vector of all loaded bundle names.
 */
std::vector<StringBox> ResourceManager::getAllLoadedBundleNames() const {
    std::lock_guard<Mutex> guard(_mutex);
    std::vector<StringBox> result;
    result.reserve(_bundleByName.size());
    for (const auto& it : _bundleByName) {
        result.push_back(it.first);
    }
    return result;
}

std::vector<std::pair<StringBox, Ref<Bundle>>> ResourceManager::getAllInitializedBundles() const {
    std::vector<std::pair<StringBox, Ref<Bundle>>> result;
    result.reserve(_bundleByName.size());
    std::lock_guard<Mutex> guard(_mutex);
    for (auto it = _bundleByName.begin(); it != _bundleByName.end(); ++it) {
        if (it->second != nullptr && it->second->initialized()) {
            result.push_back(*it);
        }
    }
    return result;
}

void ResourceManager::populateSourceMap(const StringBox& bundleName, Bundle& bundle) {
    auto sourceMapFilePath = resolveSourceMapFilePath(bundleName);
    auto sourceMapContent = _resourceLoader->loadModuleContent(sourceMapFilePath);
    if (!sourceMapContent) {
        return;
    }
    auto sourceMapIndexJson = jsonToValue(sourceMapContent.value().data(), sourceMapContent.value().size());
    if (!sourceMapIndexJson || !sourceMapIndexJson.value().isMap()) {
        VALDI_WARN(_logger, "Invalid source map index file");
        return;
    }

    auto expectedPrefix = bundleName.append("/");
    auto expectedSuffix = STRING_LITERAL(".js");

    for (const auto& it : *sourceMapIndexJson.value().getMap()) {
        if (!it.second.isString() || !it.first.hasPrefix(expectedPrefix) || !it.first.hasSuffix(expectedSuffix)) {
            VALDI_WARN(_logger, "Invalid source map entry for {}", it.first);
            continue;
        }

        auto fileKey = it.first.substring(expectedPrefix.length(), it.first.length() - expectedSuffix.length());

        // Inject the source map content into the bundle
        auto jsFile = bundle.getJs(fileKey);

        if (jsFile) {
            bundle.setJs(fileKey, JavaScriptFile(jsFile.value().content, it.second.toStringBox()));
        }
    }
}

void ResourceManager::preloadForComponentPath(const ComponentPath& componentPath) {
    auto componentPathString = StringCache::getGlobal().makeString(componentPath.toString());
    {
        std::lock_guard<Mutex> lock(_mutex);
        if (_seenComponentPaths.contains(componentPathString)) {
            return;
        }
        _seenComponentPaths.insert(componentPathString);
    }

    // Attempt to perform some load operations while other operations are ongoing.
    _workerQueue->async([self = strongSmallRef(this), componentPath = componentPath, componentPathString]() {
        VALDI_TRACE_META("Valdi.preloadForComponentPath", componentPathString);
        auto bundle = self->getBundle(componentPath.getResourceId().bundleName);
        if (!bundle->hasModuleLoadStrategy()) {
            return;
        }

        auto moduleLoadStrategy = bundle->getModuleLoadStrategy();
        SC_ASSERT(moduleLoadStrategy.success(), moduleLoadStrategy.description());

        if (!moduleLoadStrategy) {
            return;
        }
        const auto* strategy = moduleLoadStrategy.value()->getStrategyForComponentPath(componentPathString);
        if (strategy == nullptr) {
            return;
        }

        // Load each modules
        for (const auto& moduleName : strategy->valdiModules) {
            self->getBundle(moduleName);
        }
    });
}

void ResourceManager::warmUpBundles(const std::vector<StringBox>& modulePaths) {
    FlatSet<StringBox> seen;
    std::vector<StringBox> bundles;
    for (const auto& path : modulePaths) {
        auto sv = path.toStringView();
        auto slash = sv.find('/');
        if (slash != std::string_view::npos) {
            auto bundleName = StringCache::getGlobal().makeString(sv.substr(0, slash));
            if (seen.insert(bundleName).second) {
                bundles.push_back(bundleName);
            }
        }
    }

    _workerQueue->async([self = strongSmallRef(this), bundles = std::move(bundles)]() {
        VALDI_TRACE("Valdi.warmUpBundles");
        for (const auto& bundleName : bundles) {
            self->getBundle(bundleName);
        }
    });
}

void ResourceManager::loadModuleAsync(const StringBox& bundleName,
                                      ResourceManagerLoadModuleType loadType,
                                      Function<void(Result<Void>)> onComplete) {
    loadModuleAsync(_workerQueue, bundleName, loadType, std::move(onComplete));
}

void ResourceManager::loadModuleAsync(const Ref<DispatchQueue>& dispatchQueue,
                                      const StringBox& bundleName,
                                      ResourceManagerLoadModuleType loadType,
                                      Function<void(Result<Void>)> onComplete) {
    dispatchQueue->async([self = strongSmallRef(this), bundleName, loadType, completion = std::move(onComplete)]() {
        FlatSet<StringBox> processedModules;
        self->loadModuleAsyncInner(bundleName, processedModules, loadType, completion);
    });
}

void ResourceManager::loadModuleAsyncInner(const StringBox& bundleName,
                                           FlatSet<StringBox>& processedModules,
                                           ResourceManagerLoadModuleType loadType,
                                           Function<void(Result<Void>)> onComplete) {
    if (processedModules.find(bundleName) != processedModules.end()) {
        onComplete(Void());
        return;
    }
    processedModules.insert(bundleName);

    auto bundle = getBundle(bundleName);
    if (!bundle->hasRemoteArchive()) {
        onComplete(Void());
        return;
    }

    auto manifest = _remoteModuleManager->getRegisteredManifest(bundleName);
    if (manifest == nullptr) {
        onComplete(Void());
        return;
    }

    auto task = Valdi::makeShared<RemoteModulePrefetchTask>();

    for (const auto& dependency : manifest->pb.dependencies()) {
        task->enter();
        loadModuleAsyncInner(StringCache::getGlobal().makeString(dependency),
                             processedModules,
                             loadType,
                             [=](auto result) { task->leave(result); });
    }

    auto includeSources = loadType == ResourceManagerLoadModuleType::Sources ||
                          loadType == ResourceManagerLoadModuleType::SourcesAndAssets;
    auto includeAssets = loadType == ResourceManagerLoadModuleType::Assets ||
                         loadType == ResourceManagerLoadModuleType::SourcesAndAssets;

    if (includeSources) {
        task->enter();
        _remoteModuleManager->loadModule(bundleName, [=](auto result) {
            if (result) {
                bundle->setLoadedArchiveIfNeeded(result.value());
            }
            task->leave(result);
        });
    }

    if (includeAssets) {
        task->enter();
        _remoteModuleManager->loadResources(bundleName, [=](auto result) { task->leave(result); });
    }

    task->notify(std::move(onComplete));
}

void ResourceManager::addListener(Valdi::IResourceManagerListener* listener) {
    std::lock_guard<Mutex> guard(_mutex);
    _listeners.push_back(listener);
}

void ResourceManager::removeListener(Valdi::IResourceManagerListener* listener) {
    std::lock_guard<Mutex> guard(_mutex);
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

const Ref<AssetsManager>& ResourceManager::getAssetsManager() const {
    return _assetsManager;
}

const Ref<IDiskCache>& ResourceManager::getDiskCache() const {
    return _diskCache;
}

bool ResourceManager::enableAccessibility() const {
    if (_runtimeTweaks == nullptr) {
        return false;
    }
    return _runtimeTweaks->enableAccessibility();
}

bool ResourceManager::enableDeferredGC() const {
    std::lock_guard<Mutex> guard(_mutex);
    if (_runtimeTweaks == nullptr) {
        return false;
    }
    return _runtimeTweaks->enableDeferredGC();
}

void ResourceManager::setRuntimeTweaks(const Ref<ValdiRuntimeTweaks>& runtimeTweaks) {
    std::lock_guard<Mutex> guard(_mutex);
    _runtimeTweaks = runtimeTweaks;
}

Ref<ValdiRuntimeTweaks> ResourceManager::getRuntimeTweaks() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _runtimeTweaks;
}

void ResourceManager::setMetrics(const Ref<Metrics>& metrics) {
    _metrics = metrics;
}

const Ref<Metrics>& ResourceManager::getMetrics() const {
    return _metrics;
}

void ResourceManager::removeUnusedResources() {
    std::lock_guard<Mutex> guard(_mutex);

    auto it = _bundleByName.begin();
    while (it != _bundleByName.end()) {
        if (it->second.use_count() == 1) {
            it = _bundleByName.erase(it);
        } else {
            it->second->unloadUnusedResources();
            ++it;
        }
    }
}

bool ResourceManager::isLazyModulePreloadingEnabled() const {
    return _lazyModulePreloadingEnabled;
}

void ResourceManager::setLazyModulePreloadingEnabled(bool lazyModulePreloadingEnabled) {
    _lazyModulePreloadingEnabled = lazyModulePreloadingEnabled;
}

bool ResourceManager::enableTSN() const {
    std::lock_guard<Mutex> guard(_mutex);
    if (_runtimeTweaks == nullptr) {
        return _enableTSN;
    }
    return _runtimeTweaks->enableTSN();
}

bool ResourceManager::enableTSNForModule(StringBox& moduleName) const {
    std::lock_guard<Mutex> guard(_mutex);
    if (_runtimeTweaks == nullptr) {
        return _enableTSN;
    }

    return _runtimeTweaks->enableTSNForModule(moduleName);
}

void ResourceManager::setEnableTSN(bool enableTSN) {
    std::lock_guard<Mutex> guard(_mutex);
    _enableTSN = enableTSN;
}

void ResourceManager::setInlineAssetsEnabled(bool inlineAssetsEnabled) {
    std::lock_guard<Mutex> guard(_mutex);
    _inlineAssetsEnabled = inlineAssetsEnabled;
}

} // namespace Valdi
