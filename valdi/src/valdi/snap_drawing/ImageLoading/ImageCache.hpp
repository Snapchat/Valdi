//
//  ImageCache.hpp
//  valdi-desktop-apple
//
//  Created by Ramzy Jaber on 5/2/22.
//

#pragma once

#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "utils/time/Duration.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageCacheItem.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

namespace snap::drawing {

class Image;

struct CachedImage {
    Ref<Image> image;
    float scale;

    inline CachedImage(Ref<Image> image, float scale);
};

class ImageCache {
public:
    enum class EvictionPolicy { //  Determines when invalidateCachedItems can terminate.
        Time,                   // Termination occurs when the current item is no longer expired.
        Memory,                 // Termination occurs when the current size is less than the max size.
        Both,                   // Termination occurs when either of the above conditions is satisfied.
    };

    explicit ImageCache(Valdi::ILogger& logger, size_t maxSizeInBytes);
    ~ImageCache();

    Valdi::Result<CachedImage> getResizedCachedImage(const String& url, int preferredWidth, int preferredHeight);

    Valdi::Result<CachedImage> setCachedItemAndGetResizedImage(const String& url,
                                                               const Ref<Image>& image,
                                                               int preferredWidth,
                                                               int preferredHeight);

    void invalidateCachedItems(EvictionPolicy policy);

    /**
     Whether an image of the given size is cached resized when it is requested at the preferred size.
     */
    static bool resizes(int imageWidth, int imageHeight, int preferredWidth, int preferredHeight);

    void setMaxAge(uint64_t maxAgeSeconds);
    int getVariantCount(const String& url) const;
    size_t getCurrentSize() const;

private:
    const size_t _maxSizeInBytes;
    size_t _currentSize;
    snap::utils::time::Duration<std::chrono::steady_clock> _maxAgeMicroSeconds;
    [[maybe_unused]] Valdi::ILogger& _logger;

    Ref<ImageCacheItem> _cacheListStart;
    Ref<ImageCacheItem> _cacheListEnd;
    Valdi::FlatMap<String, Ref<ImageCacheItem>> _cache;

    Ref<ImageCacheItem> tryRemoveOriginalImage(Ref<ImageCacheItem>& cachedItem,
                                               snap::utils::time::Duration<std::chrono::steady_clock> pastTime,
                                               bool enableTimeExit);
    Ref<Image> retrieveImage(Ref<ImageCacheItem>& cachedItem);

    CachedImage getResizedImage(const String& url,
                                Ref<ImageCacheItem>& cachedItem,
                                int preferredWidth,
                                int preferredHeight);
};

} // namespace snap::drawing
