//
//  ImageLoaderFactory.cpp
//  valdi-desktop-apple
//
//  Created by Ramzy Jaber on 3/22/22.
//

#include "valdi/snap_drawing/ImageLoading/ImageLoaderFactory.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoader.hpp"

#include "valdi/snap_drawing/ImageLoading/AnimatedImageLoaderFactory.hpp"

#include "valdi/runtime/Resources/AssetLoaderManager.hpp"

namespace snap::drawing {

Valdi::Ref<ImageLoader> createImageLoader(const Valdi::Ref<Valdi::DispatchQueue>& queue,
                                          Valdi::ILogger& logger,
                                          uint64_t maxCacheSizeInBytes,
                                          bool decodesImagesOnLoad) {
    auto imageLoader =
        Valdi::makeShared<snap::drawing::ImageLoader>(queue, logger, maxCacheSizeInBytes, decodesImagesOnLoad);
    imageLoader->setReclamationInterval(120);

    return imageLoader;
}

void registerAssetLoaders(Valdi::AssetLoaderManager& assetLoaderManager,
                          const Ref<Resources>& resources,
                          const Valdi::Ref<Valdi::DispatchQueue>& queue,
                          Valdi::ILogger& logger,
                          uint64_t maxCacheSizeInBytes,
                          bool decodesImagesOnLoad) {
    auto imageLoader = createImageLoader(queue, logger, maxCacheSizeInBytes, decodesImagesOnLoad);

    assetLoaderManager.registerAssetLoaderFactory(imageLoader);
    assetLoaderManager.registerAssetLoaderFactory(Valdi::makeShared<AnimatedImageLoaderFactory>(resources));
}

} // namespace snap::drawing
