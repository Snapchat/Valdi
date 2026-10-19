//
//  ResourceManager.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "valdi/runtime/Resources/Bundle.hpp"
#include "valdi/runtime/Resources/ModuleArchiveCache.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Holder.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

namespace snap::valdi_core {
class HTTPRequestManager;
}

namespace Valdi {

class ILogger;
class ResourceManager;

class ValdiModuleArchive;
class IResourceManagerListener;
class RemoteModuleManager;
class DispatchQueue;
class IResourceLoader;
class IDiskCache;
class DownloadableModuleManifestWrapper;
class AssetsManager;
class MainThreadManager;
class AssetLoaderManager;
class ValdiRuntimeTweaks;
class ComponentPath;
class Metrics;

enum class ResourceManagerLoadModuleType {
    Sources,
    Assets,
    SourcesAndAssets,
};

/**
 * Memory cache for documents and JS files. Lazily load them using the IResourceLoader the first time they are
 * requested.
 */
class ResourceManager : public SimpleRefCountable {
public:
    ResourceManager(const Shared<IResourceLoader>& resourceLoader,
                    const Ref<IDiskCache>& diskCache,
                    const Ref<AssetLoaderManager>& assetLoaderManager,
                    const Holder<Shared<snap::valdi_core::HTTPRequestManager>>& requestManager,
                    const Ref<DispatchQueue>& workerQueue,
                    MainThreadManager& mainThreadManager,
                    double deviceDensity,
                    bool hotReloaderEnabled,
                    ILogger& logger);
    ~ResourceManager() override;

    bool isBundleLoaded(const StringBox& bundleName);
    bool bundleHasRemoteAssets(const StringBox& bundleName);
    bool bundleHasRemoteSources(const StringBox& bundleName);

    Ref<Bundle> getBundle(const StringBox& bundleName);

    std::vector<StringBox> getAllLoadedBundleNames() const;
    std::vector<std::pair<StringBox, Ref<Bundle>>> getAllInitializedBundles() const;

    void insertImageAssetInBundle(const Ref<Bundle>& bundle, const StringBox& filePath, const BytesView& imageData);
    void insertAssetPackageInBundle(const Ref<Bundle>& bundle, const BytesView& assetPackage);
    void onAssetCatalogChanged(const Ref<Bundle>& bundle);

    void addListener(IResourceManagerListener* listener);
    void removeListener(IResourceManagerListener* listener);

    void removeUnusedResources();

    void loadModuleAsync(const StringBox& bundleName,
                         ResourceManagerLoadModuleType loadType,
                         Function<void(Result<Void>)> onComplete);
    void loadModuleAsync(const Ref<DispatchQueue>& dispatchQueue,
                         const StringBox& bundleName,
                         ResourceManagerLoadModuleType loadType,
                         Function<void(Result<Void>)> onComplete);

    const Ref<IDiskCache>& getDiskCache() const;

    const Ref<AssetsManager>& getAssetsManager() const;

    void setRuntimeTweaks(const Ref<ValdiRuntimeTweaks>& runtimeTweaks);
    Ref<ValdiRuntimeTweaks> getRuntimeTweaks() const;

    void setMetrics(const Ref<Metrics>& metrics);
    const Ref<Metrics>& getMetrics() const;

    void preloadForComponentPath(const ComponentPath& componentPath);

    void warmUpBundles(const std::vector<StringBox>& modulePaths);

    int32_t getApiVersion();

    bool enableAccessibility() const;
    bool enableDeferredGC() const;
    bool isLazyModulePreloadingEnabled() const;
    void setLazyModulePreloadingEnabled(bool lazyModulePreloadingEnabled);
    bool enableTSN() const;
    bool enableTSNForModule(StringBox& moduleName) const;
    void setEnableTSN(bool enableTSN);

    void setInlineAssetsEnabled(bool inlineAssetsEnabled);

    // `buildVersion` identifies the app build; the archive cache starts fresh when it changes.
    void setMmapCacheDirectory(const Path& path, const std::string& buildVersion);

    // Synchronously appends the archives of earlier cache misses to the persistent archive cache.
    void flushPendingArchiveCachePublishes();

    // Called when the JS engine fails to parse a file of `bundleName`, so that a corrupt cached archive is
    // not served again.
    void reportModuleParseFailure(const StringBox& bundleName);

private:
    Shared<IResourceLoader> _resourceLoader;
    Ref<IDiskCache> _diskCache;
    Ref<RemoteModuleManager> _remoteModuleManager;
    Ref<DispatchQueue> _workerQueue;
    Ref<AssetsManager> _assetsManager;
    double _deviceDensity;
    Ref<ValdiRuntimeTweaks> _runtimeTweaks;
    Ref<Metrics> _metrics;
    std::once_flag _imageAssetOverrideDirectoryOnce;
    std::once_flag _sourceMapsBundledOnce;
    bool _sourceMapsBundled = false;
    bool _enableTSN = true;
    bool _inlineAssetsEnabled = true;
    bool _hotReloaderEnabled;
    std::optional<int32_t> _apiVersion;
    std::atomic_bool _lazyModulePreloadingEnabled = true;
    Ref<ModuleArchiveCache> _archiveCache;

    ILogger& _logger;
    FlatMap<StringBox, Ref<Bundle>> _bundleByName;
    std::vector<IResourceManagerListener*> _listeners;
    std::shared_ptr<snap::valdi_core::HTTPRequestManager> _requestManager;
    // Guards the bundle table and settings. Lock order is _mutex -> Bundle mutex (getBundle takes the
    // new Bundle's mutex via BundleInitializer while holding _mutex); never take _mutex while holding
    // a Bundle mutex. Do not hold _mutex across anything that can block for long: the resource
    // loader, runtime tweak reads, disk I/O, or waiting on a Bundle mutex another thread holds across
    // its init. The main thread contends on this mutex from Runtime::createContext.
    mutable Mutex _mutex;
    // Pack entry that served each bundle of this runtime, so that a parse failure bans that entry and not
    // another variant of the module served to another runtime. A leaf lock: taken under a Bundle mutex,
    // never held while taking another lock.
    mutable Mutex _packEntryKeysMutex;
    FlatMap<StringBox, std::string> _packEntryKeyByBundle;
    // Separate from _mutex so the main-thread dedupe in preloadForComponentPath never queues
    // behind a bundle load.
    mutable Mutex _seenComponentPathsMutex;
    FlatSet<StringBox> _seenComponentPaths;

    // The cache and metrics are passed in by the caller (getBundle) rather than read here: this runs
    // while the caller holds the Bundle's mutex, and acquiring _mutex under that lock would invert the
    // cleanup path's _mutex -> Bundle order and can deadlock. `archiveCache` is null when the pack is off.
    [[nodiscard]] Result<Ref<ValdiModuleArchive>> getArchiveForModule(const StringBox& modulePath,
                                                                      Ref<ModuleArchiveCache> archiveCache,
                                                                      const Ref<Metrics>& metrics);
    // Records the pack entry that served `bundleName`, or forgets it when `entryKey` is empty (heap load).
    void rememberPackEntry(const StringBox& bundleName, std::string entryKey);
    void initializeBundle(BundleInitializer& bundleInitializer, Ref<ValdiModuleArchive> moduleArchive);

    BundleInitializer registerBundle(const StringBox& bundleName);

    Ref<Bundle> lockFreeGetBundle(const StringBox& bundleName, std::unique_lock<Mutex>& lock);

    void populateSourceMap(const StringBox& bundleName, Bundle& bundle);
    // Whether this build ships source maps; probed once through the resource loader.
    bool areSourceMapsBundled();

    void loadModuleAsyncInner(const StringBox& bundleName,
                              FlatSet<StringBox>& processedModules,
                              ResourceManagerLoadModuleType loadType,
                              Function<void(Result<Void>)> onComplete);
    void doInsertImageAssetInBundle(const Ref<Bundle>& bundle, const StringBox& filePath, const BytesView& imageData);

    Path getImageAssetsOverrideDirectory();
};

} // namespace Valdi
