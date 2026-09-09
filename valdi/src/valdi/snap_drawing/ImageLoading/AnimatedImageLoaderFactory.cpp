//
//  AnimatedImageLoaderFactory.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/22/22.
//

#include "valdi/snap_drawing/ImageLoading/AnimatedImageLoaderFactory.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Text/IFontManager.hpp"
#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi/runtime/Resources/AssetLoaderCompletion.hpp"
#include "valdi/snap_drawing/ImageLoading/AssetSourceDescription.hpp"

#include <string>
#include <string_view>

namespace snap::drawing {

class AnimatedImageLoader : public Valdi::AssetLoader {
public:
    AnimatedImageLoader(const Ref<Resources>& resources,
                        std::vector<Valdi::StringBox>&& supportedSchemes,
                        const Valdi::Ref<Valdi::IRemoteDownloader>& downloader)
        : Valdi::AssetLoader(std::move(supportedSchemes)), _resources(resources), _downloader(downloader) {}
    ~AnimatedImageLoader() override = default;

    snap::valdi_core::AssetOutputType getOutputType() const override {
        return snap::valdi_core::AssetOutputType::Lottie;
    }

    Valdi::Result<Valdi::Value> requestPayloadFromURL(const Valdi::StringBox& url) override {
        return Valdi::Value(url);
    }

    Valdi::Shared<snap::valdi_core::Cancelable> loadAsset(
        const Valdi::Value& requestPayload,
        int32_t preferredWidth,
        int32_t preferredHeight,
        const Valdi::Value& associatedData,
        const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) override {
        auto fontManager = associatedData.getTypedRef<IFontManager>();

        auto url = requestPayload.toStringBox();

        return _downloader->downloadItem(
            url, [weakSelf = Valdi::weakRef(this), fontManager, completion, url](const auto& result) {
                if (auto strongSelf = weakSelf.lock()) {
                    strongSelf->onBytesLoaded(result, fontManager, completion, url);
                }
            });
    }

private:
    Ref<Resources> _resources;
    Valdi::Ref<Valdi::IRemoteDownloader> _downloader;

    void onBytesLoaded(const Valdi::Result<Valdi::BytesView>& result,
                       const Valdi::Ref<IFontManager>& fontManager,
                       const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion,
                       const Valdi::StringBox& url) {
        if (!result) {
            completion->onLoadComplete(result.error());
            return;
        }

        auto scene = AnimatedImage::make(fontManager != nullptr ? fontManager : _resources->getFontManager(),
                                         result.value().data(),
                                         result.value().size());
        if (!scene) {
            // flatten() for the same reason as the static loader: getMessage() returns only the
            // outermost message, so a rethrown cause would be dropped. A no-op without a cause.
            const auto message = std::string(scene.error().flatten().getMessage().toStringView()) + " [" +
                                 describeAssetSource(url) + "]";
            completion->onLoadComplete(Valdi::Error(std::string_view(message)));
            return;
        }

        Valdi::Ref<Valdi::LoadedAsset> loadedAsset = scene.moveValue();
        completion->onLoadComplete(loadedAsset);
    }
};

AnimatedImageLoaderFactory::AnimatedImageLoaderFactory(const Ref<Resources>& resources) : _resources(resources) {}
AnimatedImageLoaderFactory::~AnimatedImageLoaderFactory() = default;

snap::valdi_core::AssetOutputType AnimatedImageLoaderFactory::getOutputType() const {
    return snap::valdi_core::AssetOutputType::Lottie;
}

Valdi::Ref<Valdi::AssetLoader> AnimatedImageLoaderFactory::createAssetLoader(
    const std::vector<Valdi::StringBox>& urlSchemes, const Ref<Valdi::IRemoteDownloader>& downloader) {
    return Valdi::makeShared<AnimatedImageLoader>(_resources, std::vector<Valdi::StringBox>(urlSchemes), downloader);
}

} // namespace snap::drawing
