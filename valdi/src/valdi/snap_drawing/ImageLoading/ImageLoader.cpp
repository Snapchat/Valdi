//
//  ImageLoader.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/7/20.
//

#include "valdi/snap_drawing/ImageLoading/ImageLoader.hpp"

#include "snap_drawing/cpp/Utils/Image.hpp"
#include "valdi/snap_drawing/ImageLoading/AssetSourceDescription.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoaderBridge.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoaderTask.hpp"

#include "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#include "valdi_core/cpp/Attributes/ImageFilter.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "valdi_core/cpp/Constants.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace snap::drawing {

class ImageLoaderTaskCancelable : public snap::valdi_core::Cancelable {
public:
    explicit ImageLoaderTaskCancelable(const Valdi::Ref<ImageLoaderTask>& task) : _task(task.toWeak()) {}
    ~ImageLoaderTaskCancelable() override = default;

    void cancel() override {
        if (auto task = _task.lock()) {
            task->cancel();
        }
    }

private:
    Valdi::Weak<ImageLoaderTask> _task;
};

ImageLoader::ImageLoader(const Valdi::Ref<Valdi::DispatchQueue>& queue, Valdi::ILogger& logger, size_t maxSize)
    : _queue(queue), _logger(logger), _cache(logger, maxSize), _reclamationInterval(0) {}

ImageLoader::~ImageLoader() = default;

Valdi::Ref<Valdi::AssetLoader> ImageLoader::createAssetLoader(const std::vector<Valdi::StringBox>& urlSchemes,
                                                              const Ref<Valdi::IRemoteDownloader>& downloader) {
    return Valdi::makeShared<ImageLoaderBridge>(Valdi::strongSmallRef(this), downloader, urlSchemes);
}

snap::valdi_core::AssetOutputType ImageLoader::getOutputType() const {
    return snap::valdi_core::AssetOutputType::ImageSnapDrawing;
}

static Valdi::Ref<Valdi::LoadedAsset> toFilteredImage(const CachedImage& cacheItem,
                                                      const Valdi::Value& associatedData) {
    auto typedFilter = associatedData.getTypedRef<Valdi::ImageFilter>();
    auto image = cacheItem.image;
    if (typedFilter != nullptr) {
        const auto scaling = cacheItem.scale;
        if (scaling > 1) {
            auto newBlurValue = typedFilter->getBlurRadius() / scaling;
            typedFilter = typedFilter->withBlurRadius(newBlurValue);
        }
        image = image->withFilter(typedFilter);
    }
    return image;
}

Valdi::Shared<snap::valdi_core::Cancelable> ImageLoader::loadAsset(
    const Valdi::StringBox& url,
    int32_t preferredWidth,
    int32_t preferredHeight,
    const Valdi::Value& associatedData,
    const Valdi::Ref<Valdi::IRemoteDownloader>& downloader,
    const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    auto task = Valdi::makeShared<ImageLoaderTask>(
        Valdi::weakRef(this), url, preferredWidth, preferredHeight, associatedData, downloader, completion);

    _queue->async([task] {
        if (auto strongThis = task->getImageLoader().lock()) {
            strongThis->loadImage(task);
        }
    });

    return Valdi::makeShared<ImageLoaderTaskCancelable>(task);
}

void ImageLoader::loadImage(const Ref<ImageLoaderTask>& task) {
    if (task->wasCanceled()) {
        return;
    }

    auto imgResult =
        _cache.getResizedCachedImage(task->getUrl(), task->getPreferredWidth(), task->getPreferredHeight());

    if (!imgResult) {
        auto cancelable = task->getRemoteDownloader()->downloadItem(
            task->getUrl(), [task](const Valdi::Result<Valdi::BytesView>& result) {
                if (auto strongThis = task->getImageLoader().lock()) {
                    strongThis->handleByteViewLoadResult(task, result);
                }
            });
        task->setCurrentCancelable(cancelable);
    } else {
        handleImageLoadResult(task, imgResult.value());
    }
}

void ImageLoader::handleImageLoadResult(const Ref<ImageLoaderTask>& task, const Valdi::Result<CachedImage>& result) {
    if (result) {
        task->notifyCompletion(toFilteredImage(result.value(), task->getFilter()));
    } else {
        task->notifyCompletion(result.error());
    }
}

void ImageLoader::handleByteViewLoadResult(const Ref<ImageLoaderTask>& task,
                                           const Valdi::Result<Valdi::BytesView>& result) {
    if (result.failure()) {
        handleImageLoadResult(task, result.error());
    } else {
        _queue->async([task, bytes = result.value()]() {
            if (auto strongThis = task->getImageLoader().lock()) {
                strongThis->loadImageFromBytes(task, bytes);
            }
        });
    }
}

void ImageLoader::loadImageFromBytes(const Ref<ImageLoaderTask>& task, const Valdi::BytesView& bytes) {
    if (task->wasCanceled()) {
        return;
    }

    auto imgResult =
        _cache.getResizedCachedImage(task->getUrl(), task->getPreferredWidth(), task->getPreferredHeight());

    if (imgResult) {
        handleImageLoadResult(task, imgResult);
        return;
    }

    auto result = Image::make(bytes);
    if (!result) {
        // flatten() rather than getMessage(): the SVG rasterization path rethrows with a cause, and
        // getMessage() returns only the outermost message, which would drop why the bitmap failed.
        const auto message = std::string(result.error().flatten().getMessage().toStringView()) + " [" +
                             describeAssetSource(task->getUrl()) + "]";
        handleImageLoadResult(task, Valdi::Error(std::string_view(message)));
        return;
    }

    imgResult = _cache.setCachedItemAndGetResizedImage(
        task->getUrl(), result.value(), task->getPreferredWidth(), task->getPreferredHeight());

    handleImageLoadResult(task, imgResult);
}

void ImageLoader::setReclamationInterval(size_t expirationTime) {
    std::unique_lock<Valdi::Mutex> guard(_mutex);
    auto previousExpirationTime = _reclamationInterval;
    _reclamationInterval = expirationTime;
    _cache.setMaxAge(_reclamationInterval);
    if (previousExpirationTime == 0) {
        guard.unlock();
        scheduleReclamation();
    }
}

void ImageLoader::scheduleReclamation() {
    if (VALDI_LIKELY(_reclamationInterval)) {
        _queue->asyncAfter(
            [weakThis = weakRef(this)]() {
                if (auto strongThis = weakThis.lock()) {
                    strongThis->_cache.invalidateCachedItems(ImageCache::EvictionPolicy::Time);
                    strongThis->scheduleReclamation();
                }
            },
            std::chrono::seconds(_reclamationInterval));
    }
}

} // namespace snap::drawing
