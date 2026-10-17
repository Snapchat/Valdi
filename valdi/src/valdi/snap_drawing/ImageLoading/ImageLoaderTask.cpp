//
//  ImageLoaderTask.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/19/22.
//

#include "valdi/snap_drawing/ImageLoading/ImageLoaderTask.hpp"

namespace snap::drawing {

ImageLoaderTask::ImageLoaderTask(Valdi::Weak<ImageLoader> imageLoader,
                                 const Valdi::StringBox& url,
                                 int32_t preferredWidth,
                                 int32_t preferredHeight,
                                 const Valdi::Value& filter,
                                 const Valdi::Ref<Valdi::IRemoteDownloader>& remoteDownloader,
                                 const Valdi::Weak<Valdi::AssetLoaderCompletion>& completion)
    : _imageLoader(std::move(imageLoader)),
      _url(url),
      _preferredWidth(preferredWidth),
      _preferredHeight(preferredHeight),
      _filter(filter),
      _remoteDownloader(remoteDownloader),
      _completion(completion) {}

ImageLoaderTask::~ImageLoaderTask() = default;

const Valdi::StringBox& ImageLoaderTask::getUrl() const {
    return _url;
}

int32_t ImageLoaderTask::getPreferredWidth() const {
    return _preferredWidth;
}

int32_t ImageLoaderTask::getPreferredHeight() const {
    return _preferredHeight;
}

const Valdi::Value& ImageLoaderTask::getFilter() const {
    return _filter;
}

const Valdi::Ref<Valdi::IRemoteDownloader>& ImageLoaderTask::getRemoteDownloader() const {
    return _remoteDownloader;
}

const Valdi::Weak<ImageLoader>& ImageLoaderTask::getImageLoader() const {
    return _imageLoader;
}

void ImageLoaderTask::setCurrentCancelable(const Valdi::Shared<snap::valdi_core::Cancelable>& currentCancelable) {
    std::unique_lock<Valdi::Mutex> lock(_mutex);
    if (_wasCanceled) {
        lock.unlock();
        if (currentCancelable != nullptr) {
            currentCancelable->cancel();
        }
        return;
    }

    _currentCancelable = currentCancelable;
}

void ImageLoaderTask::notifyCompletion(const Valdi::Result<Valdi::Ref<Valdi::LoadedAsset>>& result) {
    {
        std::lock_guard<Valdi::Mutex> lock(_mutex);
        if (_wasCanceled) {
            return;
        }
        _currentCancelable = nullptr;
    }

    auto completion = _completion.lock();
    if (completion != nullptr) {
        completion->onLoadComplete(result);
    }
}

void ImageLoaderTask::cancel() {
    Valdi::Shared<snap::valdi_core::Cancelable> currentCancelable;
    {
        std::lock_guard<Valdi::Mutex> lock(_mutex);
        if (_wasCanceled) {
            return;
        }
        _wasCanceled = true;
        currentCancelable = std::move(_currentCancelable);
    }

    if (currentCancelable != nullptr) {
        currentCancelable->cancel();
    }
}

bool ImageLoaderTask::wasCanceled() const {
    return _wasCanceled;
}

} // namespace snap::drawing
