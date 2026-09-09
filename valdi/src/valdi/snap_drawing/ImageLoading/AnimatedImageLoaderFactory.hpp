//
//  AnimatedImageLoaderFactory.hpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/22/22.
//

#pragma once

#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "valdi/runtime/Interfaces/IRemoteDownloader.hpp"
#include "valdi/runtime/Resources/AssetLoaderFactory.hpp"

#include "snap_drawing/cpp/Utils/AnimatedImage.hpp"

namespace snap::drawing {

class Resources;

class AnimatedImageLoaderFactory : public Valdi::AssetLoaderFactory {
public:
    explicit AnimatedImageLoaderFactory(const Ref<Resources>& resources);
    ~AnimatedImageLoaderFactory() override;

    snap::valdi_core::AssetOutputType getOutputType() const override;

    Valdi::Ref<Valdi::AssetLoader> createAssetLoader(const std::vector<Valdi::StringBox>& urlSchemes,
                                                     const Ref<Valdi::IRemoteDownloader>& downloader) override;

private:
    Ref<Resources> _resources;
};

} // namespace snap::drawing
