//
//  SCValdiAssetLoader.m
//  ios
//
//  Created by Simon Corsin on 5/7/20.
//

#import "valdi/ios/SCValdiAssetLoader.h"

#import "valdi_core/SCValdiBaseAssetLoader.h"
#import "valdi_core/SCValdiImageLoader.h"
#import "valdi_core/SCValdiVideoLoader.h"
#import "valdi_core/SCValdiAssetWithImage+CPP.h"
#import "valdi_core/SCValdiAssetWithVideo+CPP.h"

#import "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#import "valdi/runtime/Resources/BytesAsset.hpp"
#import "valdi/runtime/Interfaces/IRemoteDownloader.hpp"

#import "valdi_core/cpp/Threading/DispatchQueue.hpp"
#import "valdi_core/cpp/Resources/LoadedAsset.hpp"
#import "valdi_core/SCNValdiCoreCancelable+Private.h"
#import "valdi_core/cpp/Attributes/ImageFilter.hpp"
#import "valdi/ios/Utils/SCValdiImageFilter.h"

#include "snap_drawing/cpp/Utils/Image.hpp"

namespace ValdiIOS {

static std::vector<Valdi::StringBox> getSupportedSchemes(NSArray<NSString *> *schemes) {
    std::vector<Valdi::StringBox> supportedSchemes;

    for (NSString *scheme in schemes) {
        supportedSchemes.emplace_back(InternedStringFromNSString(scheme));
    }

    return supportedSchemes;
}

AssetLoaderIOS::AssetLoaderIOS(id<SCValdiBaseAssetLoader> assetLoader): Valdi::AssetLoader(ValdiIOS::getSupportedSchemes([assetLoader supportedURLSchemes])), _assetLoader(assetLoader) {}

AssetLoaderIOS::~AssetLoaderIOS() = default;

id<SCValdiBaseAssetLoader> AssetLoaderIOS::getAssetLoader() const {
    return _assetLoader.getValue();
}

Valdi::Result<Valdi::Value> AssetLoaderIOS::requestPayloadFromURL(const Valdi::StringBox& url) {
    NSURL *nsURL = NSURLFromString(url);
    NSError *error = nil;

    id requestPayload = [getAssetLoader() requestPayloadWithURL:nsURL error:&error];
    if (error) {
        return Valdi::Error(ValdiIOS::InternedStringFromNSString(error.localizedDescription));
    } else {
        return Valdi::Value(ValdiIOS::ValdiObjectFromNSObject(requestPayload));
    }
}

BaseImageLoaderIOS::BaseImageLoaderIOS(id<SCValdiImageLoader> imageLoader): AssetLoaderIOS(imageLoader) {}

BaseImageLoaderIOS::~BaseImageLoaderIOS() = default;

id<SCValdiImageLoader> BaseImageLoaderIOS::getImageLoader() const {
    if ([getAssetLoader() conformsToProtocol:@protocol(SCValdiImageLoader)]) {
        return (id<SCValdiImageLoader>) getAssetLoader();
    }
    return nullptr;
}

ImageAssetLoaderIOS::ImageAssetLoaderIOS(id<SCValdiImageLoader> imageLoader,
                                 const Valdi::Ref<Valdi::DispatchQueue> &workerQueue): BaseImageLoaderIOS(imageLoader), _workerQueue(workerQueue) {}

ImageAssetLoaderIOS::~ImageAssetLoaderIOS() = default;

snap::valdi_core::AssetOutputType ImageAssetLoaderIOS::getOutputType() const {
    return snap::valdi_core::AssetOutputType::ImageIOS;
}

static void handleImageDidFinishLoading(SCValdiImage *image,
                                        NSError *error,
                                        const Valdi::Ref<Valdi::AssetLoaderCompletion> &completion) {
    Valdi::Result<Valdi::Ref<Valdi::LoadedAsset>> result;
    if (image) {
        result = Valdi::Ref<Valdi::LoadedAsset>(Valdi::makeShared<ValdiIOS::IOSLoadedAsset>(image));
    } else {
        result = Valdi::Error(ValdiIOS::InternedStringFromNSString(error.localizedDescription));
    }

    if (completion != nullptr) {
        completion->onLoadComplete(result);
    }
}

static void handleImageLoadResult(SCValdiImage *image,
                                  NSError *error,
                                  const Valdi::Ref<Valdi::ImageFilter> &imageFilter,
                                  const Valdi::Ref<Valdi::DispatchQueue> &workerQueue,
                                  const Valdi::Ref<Valdi::AssetLoaderCompletion> &completion) {
    if (imageFilter == nullptr || !image) {
        handleImageDidFinishLoading(image, error, completion);
    } else {
        workerQueue->async([=]() {
            SCValdiImage *processedImage = ValdiIOS::postprocessImage(image, imageFilter);
            handleImageDidFinishLoading(processedImage, nil, completion);
        });
    }
}

static Valdi::Result<NSData *> rasterizeSVGToPNGData(const Valdi::BytesView& bytes,
                                                     int32_t preferredWidth,
                                                     int32_t preferredHeight) {
    auto image = snap::drawing::Image::makeFromSVG(bytes, preferredWidth, preferredHeight);
    if (!image) {
        return image.moveError();
    }

    auto png = image.value()->toPNG();
    if (!png) {
        return png.moveError();
    }

    return ValdiIOS::NSDataFromBuffer(png.value());
}

Valdi::Shared<snap::valdi_core::Cancelable> ImageAssetLoaderIOS::loadAsset(const Valdi::Value& requestPayload,
                                                                            int32_t preferredWidth,
                                                                            int32_t preferredHeight,
                                                                            const Valdi::Value &attachedData,
                                                                            const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    SCValdiAssetRequestParameters parameters = {
        .preferredWidth = (NSInteger)preferredWidth,
        .preferredHeight = (NSInteger)preferredHeight
    };

    auto completionCaptured = completion;
    auto filterCaptured = attachedData.getTypedRef<Valdi::ImageFilter>();

    id<SCValdiCancelable> cancelable = [getImageLoader() loadImageWithRequestPayload:ValdiIOS::NSObjectFromValdiObject(requestPayload.getValdiObject(), NO)
                                                                             parameters:parameters
                                                                             completion:^(SCValdiImage * _Nullable image, NSError * _Nullable error) {
        handleImageLoadResult(image, error, filterCaptured, _workerQueue, completionCaptured);
    }];

    if (!cancelable) {
        return nullptr;
    }

    return djinni_generated_client::valdi_core::Cancelable::toCpp(cancelable);
}

VideoAssetLoaderIOS::VideoAssetLoaderIOS(id<SCValdiVideoLoader> videoLoader,
                                         const Valdi::Ref<Valdi::DispatchQueue> &workerQueue): AssetLoaderIOS(videoLoader),  _workerQueue(workerQueue), _videoLoader(videoLoader){}

VideoAssetLoaderIOS::~VideoAssetLoaderIOS() = default;

snap::valdi_core::AssetOutputType VideoAssetLoaderIOS::getOutputType() const {
    return snap::valdi_core::AssetOutputType::VideoIOS;
}

/**
 * Loaded video assets are stateful and one loaded asset can only be used per one video element.
 */
bool VideoAssetLoaderIOS::canReuseLoadedAssets() const {
    return false;
}

static void handleVideoDidFinishLoading(id<SCValdiVideoPlayer> player,
                                        NSError *error,
                                        const Valdi::Weak<Valdi::AssetLoaderCompletion> &completionCaptured) {
    Valdi::Result<Valdi::Ref<Valdi::LoadedAsset>> result;
    if (player) {
        result = Valdi::Ref<Valdi::LoadedAsset>(Valdi::makeShared<ValdiIOS::IOSLoadedVideoAsset>(player));
    } else {
        result = Valdi::Error(ValdiIOS::InternedStringFromNSString(error.localizedDescription));
    }

    auto completion = completionCaptured.lock();
    if (completion != nullptr) {
        completion->onLoadComplete(result);
    }
}

id<SCValdiVideoLoader> VideoAssetLoaderIOS::getVideoLoader() const {
    return _videoLoader.getValue();
}

Valdi::Shared<snap::valdi_core::Cancelable> VideoAssetLoaderIOS::loadAsset(const Valdi::Value& requestPayload,
                                                                            int32_t preferredWidth,
                                                                            int32_t preferredHeight,
                                                                            const Valdi::Value &associatedData,
                                                                            const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    SCValdiAssetRequestParameters parameters = {
        .preferredWidth = (NSInteger)preferredWidth,
        .preferredHeight = (NSInteger)preferredHeight
    };

    auto completionCaptured = completion;

    id<SCValdiCancelable> cancelable = [_videoLoader.getValue() loadVideoWithRequestPayload:ValdiIOS::NSObjectFromValdiObject(requestPayload.getValdiObject(), NO)
                                                                             parameters:parameters
                                                                             completion:^(id<SCValdiVideoPlayer> player, NSError * _Nullable error) {
        handleVideoDidFinishLoading(player, error, completionCaptured);
    }];

    if (!cancelable) {
        return nullptr;
    }

    return djinni_generated_client::valdi_core::Cancelable::toCpp(cancelable);
}


BytesAssetLoaderIOS::BytesAssetLoaderIOS(id<SCValdiImageLoader> imageLoader): BaseImageLoaderIOS(imageLoader) {}

BytesAssetLoaderIOS::~BytesAssetLoaderIOS() = default;

snap::valdi_core::AssetOutputType BytesAssetLoaderIOS::getOutputType() const {
    return snap::valdi_core::AssetOutputType::Bytes;
}

Valdi::Shared<snap::valdi_core::Cancelable> BytesAssetLoaderIOS::loadAsset(const Valdi::Value& requestPayload,
                                                                            int32_t preferredWidth,
                                                                            int32_t preferredHeight,
                                                                            const Valdi::Value &associatedData,
                                                                            const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
    auto completionCaptured = completion;

    id<SCValdiCancelable> cancelable = [getImageLoader() loadBytesWithRequestPayload:ValdiIOS::NSObjectFromValdiObject(requestPayload.getValdiObject(), NO)
                                                                             completion:^(NSData * _Nullable data, NSError * _Nullable error) {
        if (error) {
            completionCaptured->onLoadComplete(Valdi::Error(ValdiIOS::InternedStringFromNSString(error.localizedDescription)));
        } else {
            auto bytesView = ValdiIOS::BufferFromNSData(data);
            Valdi::Ref<Valdi::LoadedAsset> asset = Valdi::makeShared<Valdi::BytesAsset>(bytesView);

            completionCaptured->onLoadComplete(asset);
        }
    }];

    if (!cancelable) {
        return nullptr;
    }

    return djinni_generated_client::valdi_core::Cancelable::toCpp(cancelable);
}

class ImageLoaderIOSWithDownloader: public Valdi::AssetLoader {
public:
    ImageLoaderIOSWithDownloader(const std::vector<Valdi::StringBox>& urlSchemes,
                                 const Valdi::Ref<Valdi::DispatchQueue>& workerQueue,
                                 const Valdi::Ref<Valdi::IRemoteDownloader>& downloader): Valdi::AssetLoader(urlSchemes), _workerQueue(workerQueue), _downloader(downloader) {}

    ~ImageLoaderIOSWithDownloader() override = default;

    snap::valdi_core::AssetOutputType getOutputType() const final {
        return snap::valdi_core::AssetOutputType::ImageIOS;
    }

    Valdi::Result<Valdi::Value> requestPayloadFromURL(const Valdi::StringBox& url) final {
        return Valdi::Value(url);
    }

    Valdi::Shared<snap::valdi_core::Cancelable> loadAsset(
        const Valdi::Value& requestPayload,
        int32_t preferredWidth,
        int32_t preferredHeight,
        const Valdi::Value& associatedData,
        const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) final {
        auto imageFilter = associatedData.getTypedRef<Valdi::ImageFilter>();
        return _downloader->downloadItem(requestPayload.toStringBox(),
                                         [weakSelf = Valdi::weakRef(this), imageFilter = std::move(imageFilter), completion, preferredWidth, preferredHeight](const auto& result) {
                                             if (auto strongSelf = weakSelf.lock()) {
                                                 strongSelf->onBytesLoaded(result, preferredWidth, preferredHeight, imageFilter, completion);
                                             }
                                         });
    }

    void onBytesLoaded(const Valdi::Result<Valdi::BytesView>& result,
                       int32_t preferredWidth,
                       int32_t preferredHeight,
                       const Valdi::Ref<Valdi::ImageFilter> &imageFilter,
                       const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) {
        if (!result) {
            completion->onLoadComplete(result.error());
            return;
        }

        auto bytes = result.value();
        if (snap::drawing::Image::isSVG(bytes)) {
            _workerQueue->async([weakSelf = Valdi::weakRef(this), bytes, preferredWidth, preferredHeight, imageFilter, completion]() {
                auto pngData = rasterizeSVGToPNGData(bytes, preferredWidth, preferredHeight);
                if (!pngData) {
                    completion->onLoadComplete(pngData.error());
                    return;
                }

                NSError *error = nil;
                SCValdiImage *image = [SCValdiImage imageWithData:pngData.value() error:&error];
                if (auto strongSelf = weakSelf.lock()) {
                    handleImageLoadResult(image, error, imageFilter, strongSelf->_workerQueue, completion);
                }
            });
            return;
        }

        NSData *data = ValdiIOS::NSDataFromBuffer(bytes);

        NSError *error = nil;
        SCValdiImage *image = [SCValdiImage imageWithData:data error:&error];

        handleImageLoadResult(image, error, imageFilter, _workerQueue, completion);
    }

private:
    Valdi::Ref<Valdi::DispatchQueue> _workerQueue;
    Valdi::Ref<Valdi::IRemoteDownloader> _downloader;
};

ImageAssetLoaderFactoryIOS::ImageAssetLoaderFactoryIOS(const Valdi::Ref<Valdi::DispatchQueue>& workerQueue): _workerQueue(workerQueue) {}
ImageAssetLoaderFactoryIOS::~ImageAssetLoaderFactoryIOS() = default;

snap::valdi_core::AssetOutputType ImageAssetLoaderFactoryIOS::getOutputType() const {
    return snap::valdi_core::AssetOutputType::ImageIOS;
}

Valdi::Ref<Valdi::AssetLoader> ImageAssetLoaderFactoryIOS::createAssetLoader(const std::vector<Valdi::StringBox>& urlSchemes,
                                                                                    const Valdi::Ref<Valdi::IRemoteDownloader>& downloader) {
    return Valdi::makeShared<ImageLoaderIOSWithDownloader>(urlSchemes, _workerQueue, downloader);
}

}
