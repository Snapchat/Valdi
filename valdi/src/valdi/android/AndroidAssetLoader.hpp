//
//  AndroidAssetLoader.hpp
//  valdi-android
//
//  Created by Simon Corsin on 8/13/21.
//

#pragma once

#include "valdi/runtime/Resources/AssetLoader.hpp"
#include "valdi/runtime/Resources/AssetLoaderFactory.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/jni/GlobalRefJavaObject.hpp"

namespace ValdiAndroid {

class ResourceLoader;

} // namespace ValdiAndroid

namespace Valdi {
class DispatchQueue;
}

namespace ValdiAndroid {

class AndroidAssetLoaderFactory : public Valdi::AssetLoaderFactory {
public:
    AndroidAssetLoaderFactory(const Valdi::Ref<ResourceLoader>& resourceLoader,
                              const Valdi::Ref<Valdi::DispatchQueue>& workerQueue);
    ~AndroidAssetLoaderFactory() override;

    snap::valdi_core::AssetOutputType getOutputType() const override;
    ;

    Valdi::Ref<Valdi::AssetLoader> createAssetLoader(const std::vector<Valdi::StringBox>& urlSchemes,
                                                     const Valdi::Ref<Valdi::IRemoteDownloader>& downloader) override;

private:
    Valdi::Ref<ResourceLoader> _resourceLoader;
    Valdi::Ref<Valdi::DispatchQueue> _workerQueue;
};

class AndroidAssetLoader : public Valdi::AssetLoader, public ValdiAndroid::GlobalRefJavaObjectBase {
public:
    AndroidAssetLoader(const Valdi::Ref<ResourceLoader>& resourceLoader,
                       JavaEnv env,
                       jobject imageLoader,
                       jobject supportedSchemes,
                       jint supportedOutputType);
    ~AndroidAssetLoader() override;

    bool canReuseLoadedAssets() const override;

    snap::valdi_core::AssetOutputType getOutputType() const override;

    Valdi::Result<Valdi::Value> requestPayloadFromURL(const Valdi::StringBox& url) override;

    Valdi::Shared<snap::valdi_core::Cancelable> loadAsset(
        const Valdi::Value& requestPayload,
        int32_t preferredWidth,
        int32_t preferredHeight,
        const Valdi::Value& attachedData,
        const Valdi::Ref<Valdi::AssetLoaderCompletion>& completion) override;

    static Valdi::Ref<Valdi::LoadedAsset> createLoadedAsset(JavaEnv env, jobject valdiAsset, jboolean isImage);

private:
    Valdi::Ref<ResourceLoader> _resourceLoader;
    jint _supportedOutputType;
};

} // namespace ValdiAndroid
