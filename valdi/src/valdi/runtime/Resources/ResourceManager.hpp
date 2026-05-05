//
//  ResourceManager.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/25/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include <mutex>
#include <string>

#include "valdi/runtime/Resources/Bundle.hpp"
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

    bool enableAccessibility() const;
    bool enableDeferredGC() const;
    bool isLazyModulePreloadingEnabled() const;
    void setLazyModulePreloadingEnabled(bool lazyModulePreloadingEnabled);
    bool enableTSN() const;
    bool enableTSNForModule(StringBox& moduleName) const;
    void setEnableTSN(bool enableTSN);

    void setInlineAssetsEnabled(bool inlineAssetsEnabled);

private:
    Shared<IResourceLoader> _resourceLoader;
    Ref<IDiskCache> _diskCache;
    Ref<RemoteModuleManager> _remoteModuleManager;
    Ref<DispatchQueue> _workerQueue;
    Ref<AssetsManager> _assetsManager;
    double _deviceDensity;
    Ref<ValdiRuntimeTweaks> _runtimeTweaks;
    Ref<Metrics> _metrics;
    bool _didSetupImageAssetOverrideDirectory = false;
    bool _enableTSN = true;
    bool _inlineAssetsEnabled = true;
    bool _hotReloaderEnabled;
    std::atomic_bool _lazyModulePreloadingEnabled = true;

    ILogger& _logger;
    FlatMap<StringBox, Ref<Bundle>> _bundleByName;
    FlatSet<StringBox> _seenComponentPaths;
    std::vector<IResourceManagerListener*> _listeners;
    std::shared_ptr<snap::valdi_core::HTTPRequestManager> _requestManager;
    mutable Mutex _mutex;

    [[nodiscard]] Result<Ref<ValdiModuleArchive>> getArchiveForModule(const StringBox& modulePath);
    void initializeBundle(BundleInitializer& bundleInitializer, Ref<ValdiModuleArchive> moduleArchive);

    BundleInitializer registerBundle(const StringBox& bundleName);

    Ref<Bundle> lockFreeGetBundle(const StringBox& bundleName, std::unique_lock<Mutex>& lock);

    void populateSourceMap(const StringBox& bundleName, Bundle& bundle);

    void loadModuleAsyncInner(const StringBox& bundleName,
                              FlatSet<StringBox>& processedModules,
                              ResourceManagerLoadModuleType loadType,
                              Function<void(Result<Void>)> onComplete);
    void doInsertImageAssetInBundle(const Ref<Bundle>& bundle, const StringBox& filePath, const BytesView& imageData);

    Path getImageAssetsOverrideDirectory();
};

} // namespace Valdi
