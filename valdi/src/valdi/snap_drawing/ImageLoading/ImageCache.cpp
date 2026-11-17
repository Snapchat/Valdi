//
//  ImageCache.cpp
//  valdi-desktop-apple
//
//  Created by Ramzy Jaber on 5/2/22.
//

#include "valdi/snap_drawing/ImageLoading/ImageCache.hpp"

#include "snap_drawing/cpp/Utils/Image.hpp"

#include "valdi/snap_drawing/ImageLoading/ImageCacheItem.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace snap::drawing {

inline CachedImage::CachedImage(Ref<Image> image, float scale) : image(std::move(image)), scale(scale) {};

struct ScalingResult {
    int width;
    int height;
    float scaling;

    constexpr ScalingResult(int width, int height, float scaling = 1)
        : width(width), height(height), scaling(scaling) {}
};

static ScalingResult findScaledDimensions(int imageWidth, int imageHeight, int targetWidth, int targetHeight) {
    SC_ASSERT(imageWidth != 0 && imageHeight != 0);

    bool targetMatchesOriginal = imageWidth == targetWidth && imageHeight == targetHeight;
    bool targetDimensionsAreZero = targetWidth == 0 && targetHeight == 0;

    if (targetMatchesOriginal || targetDimensionsAreZero) {
        return ScalingResult(imageWidth, imageHeight);
    }

    const bool scaleOnWidth = targetWidth >= targetHeight;

    const int referenceImageSize = scaleOnWidth ? imageWidth : imageHeight;
    const int referenceTargetSize = scaleOnWidth ? targetWidth : targetHeight;

    if (referenceTargetSize > referenceImageSize) {
        return ScalingResult(imageWidth, imageHeight);
    }

    int scalingFactor = 1;
    const int binningFactor = 2;
    while (referenceImageSize / (scalingFactor * binningFactor) >= referenceTargetSize) {
        scalingFactor *= binningFactor;
    }

    const auto scalingFactorFloat = static_cast<double>(scalingFactor);
    const auto ratio = static_cast<double>(imageWidth) / static_cast<double>(imageHeight);
    int newWidth = 0;
    int newHeight = 0;

    if (scaleOnWidth) {
        newWidth = static_cast<int>(round(static_cast<double>(imageWidth) / scalingFactorFloat));
        newHeight = static_cast<int>(round(newWidth / ratio));
    } else {
        newHeight = static_cast<int>(round(static_cast<double>(imageHeight) / scalingFactorFloat));
        newWidth = static_cast<int>(round(newHeight * ratio));
    }

    return ScalingResult(newWidth, newHeight, scalingFactorFloat);
}

ImageCache::ImageCache(Valdi::ILogger& logger, size_t maxSizeInBytes)
    : _maxSizeInBytes(maxSizeInBytes),
      _currentSize(0),
      _maxAgeMicroSeconds(std::chrono::microseconds(0)),
      _logger(logger),
      _cacheListStart(Valdi::makeShared<ImageCacheItem>(STRING_LITERAL("SENTINAL-START"), nullptr, nullptr)),
      _cacheListEnd(Valdi::makeShared<ImageCacheItem>(STRING_LITERAL("SENTINAL-END"), nullptr, nullptr)) {
    _cacheListStart->insertAfter(_cacheListEnd);
}

ImageCache::~ImageCache() = default;

Valdi::Result<CachedImage> ImageCache::getResizedCachedImage(const String& url,
                                                             int preferredWidth,
                                                             int preferredHeight) {
    Valdi::Result<CachedImage> result;
    auto it = _cache.find(url);
    if (it == _cache.end()) {
        result = Valdi::Error("not found");
    } else {
        result = getResizedImage(url, it->second, preferredWidth, preferredHeight);
    }
    return result;
}

bool ImageCache::resizes(int imageWidth, int imageHeight, int preferredWidth, int preferredHeight) {
    const auto scaled = findScaledDimensions(imageWidth, imageHeight, preferredWidth, preferredHeight);
    return scaled.width != imageWidth || scaled.height != imageHeight;
}

size_t ImageCache::getCurrentSize() const {
    return _currentSize;
}

CachedImage ImageCache::getResizedImage(const String& url,
                                        Ref<ImageCacheItem>& cachedItem,
                                        int preferredWidth,
                                        int preferredHeight) {
    Ref<Image> returnImage;
    auto scalingResult =
        findScaledDimensions(cachedItem->getWidth(), cachedItem->getHeight(), preferredWidth, preferredHeight);
    auto newWidth = scalingResult.width;
    auto newHeight = scalingResult.height;
    auto scalingFactor = scalingResult.scaling;

    if ((cachedItem->getWidth() != newWidth || cachedItem->getHeight() != newHeight)) {
        // NOTE(rjaber): This generates a new key, which may not be a valid URL.
        //              case1 (valid)  : https://placecats.com/200/300?foo=bar&com.valdi.dimension=100,150
        //              case2 (invalid): https://placecats.com/200/300&com.valdi.dimension=100,150
        auto new_url = url.append(STRING_FORMAT("&com.valdi.dimensions={},{};", newWidth, newHeight));

        auto it = _cache.find(new_url);
        if (it == _cache.end()) {
            auto resizedCachedItem = cachedItem->getResized(new_url, newWidth, newHeight);
            _cacheListStart->insertAfter(resizedCachedItem);
            returnImage = resizedCachedItem->updateLastAccessAndGetImage();
            _cache[new_url] = resizedCachedItem;
            _currentSize += resizedCachedItem->getImageSizeInBytes();
        } else {
            returnImage = retrieveImage(it->second);
        }
    } else {
        returnImage = retrieveImage(cachedItem);
    }

    if (_currentSize > _maxSizeInBytes) {
        invalidateCachedItems(EvictionPolicy::Memory);
    }

    return CachedImage(returnImage, scalingFactor);
}

void ImageCache::setMaxAge(uint64_t maxAgeSeconds) {
    _maxAgeMicroSeconds = snap::utils::time::Duration<std::chrono::steady_clock>(std::chrono::seconds(maxAgeSeconds));
}

Ref<Image> ImageCache::retrieveImage(Ref<ImageCacheItem>& cachedItem) {
    auto returnImage = cachedItem->updateLastAccessAndGetImage();
    cachedItem->disconnectAndReturnPrevious();
    _cacheListStart->insertAfter(cachedItem);
    return returnImage;
}

Ref<ImageCacheItem> ImageCache::tryRemoveOriginalImage(Ref<ImageCacheItem>& cachedItem,
                                                       snap::utils::time::Duration<std::chrono::steady_clock> pastTime,
                                                       bool enableTimeExit) {
    const auto& url = cachedItem->getUrl();

    if (cachedItem->isUnused() && !cachedItem->hasVariants() && (!enableTimeExit || cachedItem->isExpired(pastTime))) {
        _currentSize -= cachedItem->getImageSizeInBytes();
        _cache.erase(url);
        return cachedItem->disconnectAndReturnPrevious();
    } else {
        return cachedItem->getPrevious();
    }
}

void ImageCache::invalidateCachedItems(EvictionPolicy policy) {
    VALDI_TRACE("Valdi.invalidateCachedItems");
    const bool enableTimeExit = (policy == EvictionPolicy::Time) || (policy == EvictionPolicy::Both);
    const bool enableSizeExit = (policy == EvictionPolicy::Memory) || (policy == EvictionPolicy::Both);

    auto pastTime = ImageCacheItem::getCurrentTime();
    pastTime = (pastTime > _maxAgeMicroSeconds) ? (pastTime - _maxAgeMicroSeconds) :
                                                  snap::utils::time::Duration<std::chrono::steady_clock>();

    auto it = _cacheListEnd->getPrevious();
    while (it != _cacheListStart) {
        if (enableTimeExit && !it->isExpired(pastTime)) {
            break;
        }
        if (enableSizeExit && (_currentSize <= _maxSizeInBytes)) {
            break;
        }

        if (it->isOriginal()) {
            it = tryRemoveOriginalImage(it, pastTime, enableTimeExit);
        } else if (it->isUnused() && (!enableTimeExit || it->isExpired(pastTime))) {
            _currentSize -= it->getImageSizeInBytes();
            _cache.erase(it->getUrl());

            const auto parent = it->getParent();
            auto previous = it->disconnectAndReturnPrevious();
            it = nullptr;

            if (!parent->hasVariants()) {
                auto it_original = _cache.find(parent->getUrl());
                SC_ASSERT(it_original != _cache.end());

                bool parentAndPreviousAreSame = parent == previous;
                auto candidatePrevious = tryRemoveOriginalImage(it_original->second, pastTime, enableTimeExit);
                if (parentAndPreviousAreSame) {
                    previous = candidatePrevious;
                }
            }
            it = previous;
        } else {
            it = it->getPrevious();
        }
    }
}

Valdi::Result<CachedImage> ImageCache::setCachedItemAndGetResizedImage(const String& url,
                                                                       const Ref<Image>& image,
                                                                       int preferredWidth,
                                                                       int preferredHeight) {
    auto cache_itr = _cache.find(url);
    if (VALDI_UNLIKELY(cache_itr != _cache.end())) {
        auto it = _cacheListEnd->getPrevious();
        Ref<ImageCacheItem> parent = cache_itr->second; // NOTE(rjaber): This is meant to extend the life of the parent
        while (it != _cacheListStart) {
            const auto& cacheItem = it;
            const bool shouldErase = cacheItem->getOriginalUrl() == url;
            if (shouldErase) {
                _currentSize -= cacheItem->getImageSizeInBytes();
                _cache.erase(cacheItem->getUrl());
                it = cacheItem->disconnectAndReturnPrevious();
            } else {
                it = cacheItem->getPrevious();
            }
        }
        VALDI_DEBUG(_logger, "Evicting previous instances of image with {}", url);
    }

    auto cacheItem = Valdi::makeShared<ImageCacheItem>(url, nullptr, image);
    _cacheListStart->insertAfter(cacheItem);
    _cache[url] = cacheItem;
    _currentSize += cacheItem->getImageSizeInBytes();

    return getResizedImage(url, cacheItem, preferredWidth, preferredHeight);
}

int ImageCache::getVariantCount(const String& url) const {
    const auto& it = _cache.find(url);
    if (it != _cache.end()) {
        return (it->second)->getVariantCount();
    } else {
        return 0;
    }
}

} // namespace snap::drawing
