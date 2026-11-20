//
//  ImageLoader.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/7/20.
//

#pragma once

#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "valdi/runtime/Interfaces/IRemoteDownloader.hpp"
#include "valdi/runtime/Resources/AssetLoaderFactory.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageCache.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

namespace snap::drawing {

class Image;

class ImageLoaderTask;

class ImageLoader : public Valdi::AssetLoaderFactory {
public:
    /**
     With decodesOnLoad set, images are decoded on the loader's queue as they load, instead of on their first draw.
     */
    ImageLoader(const Valdi::Ref<Valdi::DispatchQueue>& queue,
                Valdi::ILogger& logger,
                size_t maxSize,
                bool decodesOnLoad = false);
    ~ImageLoader() override;

    snap::valdi_core::AssetOutputType getOutputType() const override;

    Valdi::Ref<Valdi::AssetLoader> createAssetLoader(const std::vector<Valdi::StringBox>& urlSchemes,
                                                     const Ref<Valdi::IRemoteDownloader>& downloader) override;

    void setReclamationInterval(size_t expirationTime);

    Valdi::Shared<snap::valdi_core::Cancelable> loadAsset(const Valdi::StringBox& url,
                                                          int32_t preferredWidth,
                                                          int32_t preferredHeight,
                                                          const Valdi::Value& associatedData,
                                                          const Valdi::Ref<Valdi::IRemoteDownloader>& downloader,
                                                          const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion);

private:
    mutable Valdi::Mutex _mutex;
    Valdi::Ref<Valdi::DispatchQueue> _queue;
    [[maybe_unused]] Valdi::ILogger& _logger;
    ImageCache _cache;
    const bool _decodesOnLoad;

    size_t _reclamationInterval;

    void loadImage(const Ref<ImageLoaderTask>& task);

    void handleByteViewLoadResult(const Ref<ImageLoaderTask>& task, const Valdi::Result<Valdi::BytesView>& result);

    void handleImageLoadResult(const Ref<ImageLoaderTask>& task, const Valdi::Result<CachedImage>& result);

    void loadImageFromBytes(const Ref<ImageLoaderTask>& task, const Valdi::BytesView& bytes);

    void scheduleReclamation();
};

} // namespace snap::drawing
