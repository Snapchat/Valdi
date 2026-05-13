//
//  SnapDrawingRuntime.hpp
//  valdi-android
//
//  Created by Simon Corsin on 7/23/20.
//

#pragma once

#include "valdi_core/cpp/Utils/Shared.hpp"

#include "snap_drawing/cpp/Utils/Aliases.hpp"

#include "valdi/snap_drawing/SnapDrawingViewManager.hpp"
#include "valdi_core/cpp/Context/PlatformType.hpp"

namespace Valdi {
class IDiskCache;
class IViewManager;
class ILogger;
class DispatchQueue;
class AssetLoaderManager;
} // namespace Valdi

namespace snap::drawing {

class IFrameScheduler;
class FontManager;
class SnapDrawingViewManager;
class ShaderCache;
class DrawLooper;
class Resources;
class GraphicsContext;
struct GesturesConfiguration;

class Runtime : public Valdi::SimpleRefCountable {
public:
    Runtime(const Ref<IFrameScheduler>& frameScheduler,
            const GesturesConfiguration& gesturesConfiguration,
            const Valdi::Ref<Valdi::IDiskCache>& diskCache,
            Valdi::IViewManager* hostViewManager,
            Valdi::ILogger& logger,
            const Valdi::Ref<Valdi::DispatchQueue>& workerQueue,
            uint64_t maxCacheSizeInBytes);
    ~Runtime() override;

    void preload() const;

    virtual void initializeViewManager(Valdi::PlatformType platformType);

    void registerAssetLoaders(Valdi::AssetLoaderManager& assetLoaderManager);

    const Ref<IFrameScheduler>& getFrameScheduler() const;

    const Ref<SnapDrawingViewManager>& getViewManager() const;

    const Ref<FontManager>& getFontManager() const;

    const Ref<Resources>& getResources() const;

    const Ref<DrawLooper>& getDrawLooper() const;

    const Ref<ShaderCache>& getShaderCache() const;

    const Ref<GraphicsContext>& getGraphicsContext() const;

    void setGraphicsContext(const Ref<GraphicsContext>& graphicsContext);

private:
    Valdi::Ref<snap::drawing::IFrameScheduler> _frameScheduler;
    Valdi::Ref<snap::drawing::SnapDrawingViewManager> _snapDrawingViewManager;
    Valdi::Ref<snap::drawing::DrawLooper> _drawLooper;
    Valdi::Ref<snap::drawing::ShaderCache> _shaderCache;
    Valdi::Ref<snap::drawing::FontManager> _fontManager;
    Valdi::Ref<Valdi::DispatchQueue> _workerQueue;
    Valdi::Ref<GraphicsContext> _graphicsContext;
    Valdi::Ref<Resources> _resources;
    Valdi::IViewManager* _hostViewManager;
    uint64_t _maxCacheSizeInBytes;
};

} // namespace snap::drawing
