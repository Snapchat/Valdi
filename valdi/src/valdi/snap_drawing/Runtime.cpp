//
//  Runtime.cpp
//  valdi-snap_drawing
//
//  Created by Simon Corsin on 7/26/22.
//

#include "valdi/snap_drawing/Runtime.hpp"
#include "valdi/snap_drawing/Graphics/ShaderCache.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoaderFactory.hpp"
#include "valdi/snap_drawing/SnapDrawingViewManager.hpp"

#include "snap_drawing/cpp/Drawing/DrawLooper.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"

namespace snap::drawing {

Runtime::Runtime(const Ref<IFrameScheduler>& frameScheduler,
                 const GesturesConfiguration& gesturesConfiguration,
                 const Valdi::Ref<Valdi::IDiskCache>& diskCache,
                 Valdi::IViewManager* hostViewManager,
                 Valdi::ILogger& logger,
                 const Valdi::Ref<Valdi::DispatchQueue>& workerQueue,
                 uint64_t maxCacheSizeInBytes)
    : _frameScheduler(frameScheduler),
      _workerQueue(workerQueue),
      _hostViewManager(hostViewManager),
      _maxCacheSizeInBytes(maxCacheSizeInBytes) {
    _drawLooper = Valdi::makeShared<snap::drawing::DrawLooper>(_frameScheduler, logger);

    if (diskCache != nullptr) {
        auto shaderPath = Valdi::Path("shaders");
        auto shadersDiskCache = diskCache->scopedCache(shaderPath, false);
        _shaderCache = Valdi::makeShared<snap::drawing::ShaderCache>(shadersDiskCache, workerQueue, logger);
    }

    _fontManager = Valdi::makeShared<snap::drawing::FontManager>(logger);
    _resources = Valdi::makeShared<Resources>(_fontManager,
                                              hostViewManager != nullptr ? hostViewManager->getPointScale() : 1.0f,
                                              gesturesConfiguration,
                                              logger);
}

Runtime::~Runtime() = default;

void Runtime::preload() const {
    // Preload the emoji font at startup to avoid a slow hit in the main thread
    getFontManager()->getEmojiFont(17, 1.0f);
}

void Runtime::initializeViewManager(Valdi::PlatformType platformType) {
    if (_snapDrawingViewManager != nullptr) {
        return;
    }
    _snapDrawingViewManager = Valdi::makeShared<snap::drawing::SnapDrawingViewManager>(_resources, platformType);
    if (_hostViewManager != nullptr) {
        _snapDrawingViewManager->setHostViewManager(_hostViewManager);
    }
}

void Runtime::registerAssetLoaders(Valdi::AssetLoaderManager& assetLoaderManager, bool decodesImagesOnLoad) {
    auto& logger = _resources->getLogger();
    auto queue =
        Valdi::DispatchQueue::create(STRING_LITERAL("com.snap.valdi.ImageLoader"), Valdi::ThreadQoSClassNormal);

    snap::drawing::registerAssetLoaders(
        assetLoaderManager, _resources, queue, logger, _maxCacheSizeInBytes, decodesImagesOnLoad);
}

const Ref<IFrameScheduler>& Runtime::getFrameScheduler() const {
    return _frameScheduler;
}

const Ref<SnapDrawingViewManager>& Runtime::getViewManager() const {
    return _snapDrawingViewManager;
}

const Ref<FontManager>& Runtime::getFontManager() const {
    return _fontManager;
}

const Ref<Resources>& Runtime::getResources() const {
    return _resources;
}

const Ref<DrawLooper>& Runtime::getDrawLooper() const {
    return _drawLooper;
}

const Ref<ShaderCache>& Runtime::getShaderCache() const {
    return _shaderCache;
}

const Ref<GraphicsContext>& Runtime::getGraphicsContext() const {
    return _graphicsContext;
}

void Runtime::setGraphicsContext(const Ref<GraphicsContext>& graphicsContext) {
    if (_graphicsContext != graphicsContext) {
        if (_graphicsContext != nullptr) {
            _drawLooper->removeManagedGraphicsContext(_graphicsContext.get());
        }

        _graphicsContext = graphicsContext;

        if (_graphicsContext != nullptr) {
            _drawLooper->appendManagedGraphicsContext(_graphicsContext);
        }
    }
}

} // namespace snap::drawing
