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
#include "valdi_core/cpp/Utils/TextParser.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <limits>
#include <string_view>

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

static int32_t parseApiVersion(const BytesView& bytes, ILogger& logger) {
    TextParser parser(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));

    parser.tryParseWhitespaces();
    auto version = parser.parseUInt();
    parser.tryParseWhitespaces();

    if (!version.has_value() || !parser.isAtEnd() || version.value() > std::numeric_limits<int32_t>::max()) {
        VALDI_WARN(logger, "Invalid valdi_api_version resource content. Expected a non-negative integer.");
        return 0;
    }

    return static_cast<int32_t>(version.value());
}

int32_t ResourceManager::getApiVersion() {
    {
        std::lock_guard<Mutex> guard(_mutex);
        if (_apiVersion.has_value()) {
            return _apiVersion.value();
        }
    }

    // Loaded outside _mutex: the resource loader crosses into the platform and on the first call can
    // initialize the Dynamic Delivery archive, which waits on the content manager. Holding _mutex
    // across it blocked the main thread in preloadForComponentPath for 10s+ (MUSIC-13144,
    // SHARING-35215). Concurrent first callers may both reach the loader; the Dynamic Delivery
    // archive init behind it is a std::call_once, so the heavy work still runs once.
    auto content = _resourceLoader->loadModuleContent(STRING_LITERAL("valdi_api_version"));
    int32_t apiVersion = content ? parseApiVersion(content.value(), _logger) : 0;

    std::lock_guard<Mutex> guard(_mutex);
    if (!_apiVersion.has_value()) {
        _apiVersion = apiVersion;
    }
    return _apiVersion.value();
}

Result<Ref<ValdiModuleArchive>> ResourceManager::getArchiveForModule(const StringBox& modulePath,
                                                                     Ref<ModuleArchiveCache> archiveCache,
                                                                     const Ref<Metrics>& metrics) {
    auto bundleFilePath = resolveModuleArchiveFilePath(modulePath);
    // A pack that cannot be used (disabled after a crash loop, locked by another process) costs nothing.
    if (archiveCache != nullptr && !archiveCache->isUsable(metrics, _logger)) {
        archiveCache = nullptr;
    }

    // NOTE: do not acquire _mutex anywhere in this function. It is called from getBundle while the
    // BundleInitializer holds the Bundle's mutex; the cleanup path (removeUnusedResources) takes
    // _mutex and then a Bundle mutex, so taking _mutex here inverts that order and can deadlock.
    // The cache and metrics are snapshotted by getBundle under _mutex and passed in.

    std::optional<ModuleContentIdentity> sourceIdentity;
    if (archiveCache != nullptr) {
        // A hit needs only the loader's identity of the module, not its bytes.
        MetricsStopWatch lookupStopWatch;
        sourceIdentity = _resourceLoader->getModuleIdentity(bundleFilePath);
        if (sourceIdentity) {
            std::string entryKey;
            auto hit = archiveCache->lookup(modulePath, *sourceIdentity, metrics, _logger, &entryKey);
            if (hit) {
                rememberPackEntry(modulePath, std::move(entryKey));
                if (metrics != nullptr) {
                    metrics->emitModuleArchiveMmapSuccess(modulePath);
                    metrics->emitModuleDecompressLatency(modulePath, lookupStopWatch.elapsed());
                }
                return Valdi::makeShared<ValdiModuleArchive>(std::move(*hit));
            }
        }
    }

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

    // The source (e.g. a dynamic-delivery container) may have been replaced between the identity probe and
    // the read. Only key the bytes by the loader's identity if it still holds; otherwise they are hashed.
    if (sourceIdentity) {
        auto identityAfterRead = _resourceLoader->getModuleIdentity(bundleFilePath);
        if (!identityAfterRead || identityAfterRead->kind != sourceIdentity->kind ||
            identityAfterRead->value != sourceIdentity->value) {
            sourceIdentity.reset();
        }
    }

    bool servedFromPack = false;
    std::string entryKey;
    MetricsStopWatch decompressStopWatch;
    Result<ValdiModuleArchive> result = [&]() -> Result<ValdiModuleArchive> {
        if (archiveCache != nullptr) {
            auto cached = archiveCache->load(
                modulePath, sourceIdentity, data.data(), data.size(), metrics, _logger, servedFromPack, &entryKey);
            if (cached) {
                return cached;
            }
        }
        return ValdiModuleArchive::decompress(data.data(), data.size());
    }();
    auto decompressDuration = decompressStopWatch.elapsed();
    rememberPackEntry(modulePath, servedFromPack ? std::move(entryKey) : std::string());

    // Misses are served from the heap like loads with the pack off; the pack appends them in the background.
    if (result && metrics != nullptr) {
        if (servedFromPack) {
            metrics->emitModuleArchiveMmapSuccess(modulePath);
        } else {
            metrics->emitModuleArchiveHeap(modulePath);
        }
        metrics->emitModuleDecompressLatency(modulePath, decompressDuration);
    }

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
    std::call_once(_imageAssetOverrideDirectoryOnce, [&]() { _diskCache->remove(path); });
    return path;
}

bool ResourceManager::isBundleLoaded(const StringBox& bundleName) {
    Ref<Bundle> bundle;
    {
        std::lock_guard<Mutex> guard(_mutex);
        const auto& it = _bundleByName.find(bundleName);
        if (it == _bundleByName.end()) {
            return false;
        }
        bundle = it->second;
    }
    // The Bundle mutex is held for the whole archive decompression while the bundle initializes.
    return !bundle->hasRemoteArchiveNeedingLoad();
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

    // Snapshot the settings while we still hold _mutex. getArchiveForModule runs below while the
    // BundleInitializer holds the Bundle's mutex; it must not take _mutex itself, or it would invert
    // the cleanup path's (_mutex -> Bundle mutex) order and can deadlock.
    auto runtimeTweaks = _runtimeTweaks;
    auto archiveCache = _archiveCache;
    auto metrics = _metrics;

    // We now have a lock on the Bundle itself. Release our lock so that
    // other threads can query the ResourceManager on other bundles.
    lock.unlock();

    // Tweak reads can hit the host's config backend; evaluate them after releasing _mutex. Without tweaks
    // the pack is left alone, since the host may attach a provider that turns it on.
    if (archiveCache != nullptr) {
        if (runtimeTweaks == nullptr) {
            archiveCache = nullptr;
        } else if (!runtimeTweaks->enableMmapModuleArchiveCacheReuse()) {
            archiveCache->scheduleRemoval();
            archiveCache = nullptr;
        }
    }
    // Builds that ship source maps (debug) keep the lookup, so on-device symbolication still works.
    bool lookUpSourceMap = runtimeTweaks == nullptr || !runtimeTweaks->skipSourceMapLookup() || areSourceMapsBundled();

    auto assetPackageKey = STRING_LITERAL("res.assetpackage");
    auto hasAssetPackage = false;

    auto archiveResult = getArchiveForModule(bundleName, archiveCache, metrics);
    if (!archiveResult) {
        if (!_hotReloaderEnabled) {
            VALDI_ERROR(_logger, "Failed to load archive of Module '{}': {}", bundleName, archiveResult.error());
        }
        initializeBundle(bundleInitializer, makeShared<ValdiModuleArchive>());
    } else {
        hasAssetPackage = inlineAssetsEnabled && archiveResult.value()->containsEntry(assetPackageKey);
        initializeBundle(bundleInitializer, archiveResult.value());
    }

    if (lookUpSourceMap) {
        populateSourceMap(bundleName, *bundleInitializer.getBundle());
    }

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
    std::lock_guard<Mutex> guard(_mutex);
    result.reserve(_bundleByName.size());
    for (auto it = _bundleByName.begin(); it != _bundleByName.end(); ++it) {
        if (it->second != nullptr && it->second->initialized()) {
            result.push_back(*it);
        }
    }
    return result;
}

bool ResourceManager::areSourceMapsBundled() {
    // A build ships a source map for every module or for none, and every build has valdi_core.
    std::call_once(_sourceMapsBundledOnce, [this]() {
        _sourceMapsBundled = static_cast<bool>(
            _resourceLoader->loadModuleContent(resolveSourceMapFilePath(STRING_LITERAL("valdi_core"))));
    });
    return _sourceMapsBundled;
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
        std::lock_guard<Mutex> lock(_seenComponentPathsMutex);
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
    auto runtimeTweaks = getRuntimeTweaks();
    return runtimeTweaks != nullptr && runtimeTweaks->enableAccessibility();
}

bool ResourceManager::enableDeferredGC() const {
    auto runtimeTweaks = getRuntimeTweaks();
    return runtimeTweaks != nullptr && runtimeTweaks->enableDeferredGC();
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
    std::vector<Ref<Bundle>> retainedBundles;
    {
        std::lock_guard<Mutex> guard(_mutex);
        auto it = _bundleByName.begin();
        while (it != _bundleByName.end()) {
            if (it->second.use_count() == 1) {
                it = _bundleByName.erase(it);
            } else {
                retainedBundles.push_back(it->second);
                ++it;
            }
        }
    }

    // Each call takes the Bundle mutex, which a concurrent bundle init holds across decompression.
    for (const auto& bundle : retainedBundles) {
        bundle->unloadUnusedResources();
    }
}

bool ResourceManager::isLazyModulePreloadingEnabled() const {
    return _lazyModulePreloadingEnabled;
}

void ResourceManager::setLazyModulePreloadingEnabled(bool lazyModulePreloadingEnabled) {
    _lazyModulePreloadingEnabled = lazyModulePreloadingEnabled;
}

bool ResourceManager::enableTSN() const {
    Ref<ValdiRuntimeTweaks> runtimeTweaks;
    bool enableTSN;
    {
        std::lock_guard<Mutex> guard(_mutex);
        runtimeTweaks = _runtimeTweaks;
        enableTSN = _enableTSN;
    }
    return runtimeTweaks != nullptr ? runtimeTweaks->enableTSN() : enableTSN;
}

bool ResourceManager::enableTSNForModule(StringBox& moduleName) const {
    Ref<ValdiRuntimeTweaks> runtimeTweaks;
    bool enableTSN;
    {
        std::lock_guard<Mutex> guard(_mutex);
        runtimeTweaks = _runtimeTweaks;
        enableTSN = _enableTSN;
    }
    return runtimeTweaks != nullptr ? runtimeTweaks->enableTSNForModule(moduleName) : enableTSN;
}

void ResourceManager::setEnableTSN(bool enableTSN) {
    std::lock_guard<Mutex> guard(_mutex);
    _enableTSN = enableTSN;
}

void ResourceManager::setInlineAssetsEnabled(bool inlineAssetsEnabled) {
    std::lock_guard<Mutex> guard(_mutex);
    _inlineAssetsEnabled = inlineAssetsEnabled;
}

void ResourceManager::flushPendingArchiveCachePublishes() {
    Ref<ModuleArchiveCache> archiveCache;
    {
        std::lock_guard<Mutex> guard(_mutex);
        archiveCache = _archiveCache;
    }
    if (archiveCache != nullptr) {
        archiveCache->flushPendingPublishes();
    }
}

void ResourceManager::reportModuleParseFailure(const StringBox& bundleName) {
    std::string entryKey;
    {
        std::lock_guard<Mutex> guard(_packEntryKeysMutex);
        auto it = _packEntryKeyByBundle.find(bundleName);
        if (it == _packEntryKeyByBundle.end()) {
            return;
        }
        entryKey = std::move(it->second);
        _packEntryKeyByBundle.erase(it);
    }
    Ref<ModuleArchiveCache> archiveCache;
    Ref<Metrics> metrics;
    {
        std::lock_guard<Mutex> guard(_mutex);
        archiveCache = _archiveCache;
        metrics = _metrics;
    }
    if (archiveCache != nullptr) {
        archiveCache->reportParseFailure(bundleName, entryKey, metrics);
    }
}

void ResourceManager::rememberPackEntry(const StringBox& bundleName, std::string entryKey) {
    std::lock_guard<Mutex> guard(_packEntryKeysMutex);
    if (entryKey.empty()) {
        _packEntryKeyByBundle.erase(bundleName);
    } else {
        _packEntryKeyByBundle[bundleName] = std::move(entryKey);
    }
}

void ResourceManager::setMmapCacheDirectory(const Path& path, const std::string& buildVersion) {
    std::lock_guard<Mutex> guard(_mutex);
    // One instance per process and directory: the cache has a single writer.
    _archiveCache = path.empty() ? nullptr : ModuleArchiveCache::getShared(path, buildVersion);
}

} // namespace Valdi
