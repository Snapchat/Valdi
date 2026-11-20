//
//  ImageLoaderFactory.hpp
//  valdi-desktop-apple
//
//  Created by Ramzy Jaber on 3/22/22.
//

#pragma once

#include "snap_drawing/cpp/Utils/Aliases.hpp"

namespace Valdi {

class DispatchQueue;
class ILogger;
class AssetLoaderManager;

} // namespace Valdi

namespace snap::drawing {

class Resources;
class ImageLoader;

Ref<ImageLoader> createImageLoader(const Valdi::Ref<Valdi::DispatchQueue>& queue,
                                   Valdi::ILogger& logger,
                                   uint64_t maxCacheSizeInBytes,
                                   bool decodesImagesOnLoad = false);

void registerAssetLoaders(Valdi::AssetLoaderManager& assetLoaderManager,
                          const Ref<Resources>& resources,
                          const Valdi::Ref<Valdi::DispatchQueue>& queue,
                          Valdi::ILogger& logger,
                          uint64_t maxCacheSizeInBytes,
                          bool decodesImagesOnLoad);

} // namespace snap::drawing
