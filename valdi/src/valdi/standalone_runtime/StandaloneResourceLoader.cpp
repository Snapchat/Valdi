//
//  StandaloneResourceLoader.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/14/19.
//

#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi/runtime/Resources/ZStdUtils.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

namespace Valdi {

StandaloneResourceLoader::StandaloneResourceLoader() {
    for (const auto& resource : StaticResourceRegistry::sharedInstance().getResources()) {
        auto resourcePath = StringCache::getGlobal().makeString(std::string_view(resource.path));
        _staticResourceByModule.try_emplace(resourcePath, resource);
    }
}

StandaloneResourceLoader::~StandaloneResourceLoader() = default;

Result<BytesView> StandaloneResourceLoader::loadModuleContent(const StringBox& module) {
    std::unique_lock<Mutex> lock(_mutex);
    // Explicitly provided module files take precedence over the copies embedded in the binary, so that
    // callers like test runners load the build they asked for (e.g. one instrumented for code coverage).
    // If the file can't be read, the embedded copy is used as a backup.
    const auto& modulePathIt = _pathByModule.find(module);
    if (modulePathIt != _pathByModule.end()) {
        auto loadResult = DiskUtils::load(modulePathIt->second);
        if (loadResult || _staticResourceByModule.find(module) == _staticResourceByModule.end()) {
            return loadResult;
        }
    }

    // Search in statically registered modules second
    const auto& moduleContentIt = _staticResourceByModule.find(module);
    if (moduleContentIt != _staticResourceByModule.end()) {
        const auto& resource = moduleContentIt->second;
        if (resource.requiresDecompression) {
            auto decompressed = ZStdUtils::decompress(resource.bytes, resource.length);
            if (!decompressed) {
                return decompressed.moveError();
            }
            return decompressed.value()->toBytesView();
        } else {
            return BytesView(nullptr, resource.bytes, resource.length);
        }
    }

    // Otherwise look in module search directories

    for (const auto& searchPath : _moduleSearchDirectories) {
        auto directory = Path(searchPath.toStringView());

        auto loadResult = searchForModule(directory, module);
        if (loadResult) {
            return loadResult;
        }
    }

    return Error(
        STRING_FORMAT("Could not find module in search paths '{}'", StringBox::join(_moduleSearchDirectories, ", ")));
}

void StandaloneResourceLoader::addModuleSearchDirectory(const StringBox& searchDirectory) {
    auto searchPath = DiskUtils::absolutePathFromString(searchDirectory.toStringView());

    std::unique_lock<Mutex> lock(_mutex);
    if (DiskUtils::isFile(searchPath)) {
        auto moduleName = StringCache::getGlobal().makeString(searchPath.getLastComponent());
        _pathByModule[moduleName] = std::move(searchPath);
    } else {
        _moduleSearchDirectories.emplace_back(StringCache::getGlobal().makeString(searchPath.toString()));
    }
}

Result<BytesView> StandaloneResourceLoader::searchForModule(const Path& directory, const StringBox& module) {
    auto file = directory.appending(module.toStringView());
    if (DiskUtils::isFile(file)) {
        return DiskUtils::load(file);
    }

    if (DiskUtils::isDirectory(directory)) {
        auto subdirectories = DiskUtils::listDirectory(directory);
        for (const auto& subDirectory : subdirectories) {
            auto loadResult = searchForModule(subDirectory, module);
            if (loadResult) {
                return loadResult;
            }
        }
    }

    return Error("Could not find module");
}

StringBox StandaloneResourceLoader::resolveLocalAssetURL(const StringBox& moduleName, const StringBox& resourcePath) {
    std::unique_lock<Mutex> lock(_mutex);
    const auto& it = _localAssetURLs.find(ResourceId(moduleName, resourcePath));
    if (it == _localAssetURLs.end()) {
        return StringBox();
    }

    return it->second;
}

void StandaloneResourceLoader::setDiskCache(const Ref<InMemoryDiskCache>& diskCache) {
    std::lock_guard<Mutex> lock(_mutex);
    _diskCache = diskCache;
}

void StandaloneResourceLoader::setLocalAssetURL(const StringBox& moduleName,
                                                const StringBox& resourcePath,
                                                const StringBox& url) {
    std::unique_lock<Mutex> lock(_mutex);
    _localAssetURLs[ResourceId(moduleName, resourcePath)] = url;
}

} // namespace Valdi
