#pragma once

#include "snap_drawing/cpp/Drawing/Raster/BitmapCache.hpp"
#include "snap_drawing/cpp/Drawing/Raster/RasterDamageResolver.hpp"
#include "snap_drawing/cpp/Drawing/Surface/ExternalSurfacePresenterState.hpp"
#include "snap_drawing/cpp/Utils/Aliases.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include <mutex>
#include <vector>

namespace Valdi {

class IBitmap;
class IBitmapFactory;
class ILogger;
struct BitmapInfo;
} // namespace Valdi

namespace snap::drawing {

class DisplayList;
class CompositorPlaneList;
class ExternalSurfaceSnapshot;
class Image;
struct ExternalSurfacePresenterState;
class DrawableSurfaceCanvas;

enum class ExternalSurfaceRasterizationMethod {
    /**
     * The native view will be rasterized using its frame size. If it has a transform (scale, rotation)
     * set on the view or one of its ancestors, the transform will be applied post rasterization.
     * The native view is only redrawn when one of its properties changes. This system results in high
     * draw cache hit rate, but will result in a potentially lower quality image when using transformation.
     */
    FAST = 0,

    /**
     * The native view will be rasterized into the final output buffer with its final transform (scale, rotation)
     * applied. It will be rasterized again every time scale, rotation, translation or frame changes, or if
     * one of its properties changes. This system results in a highly accurate rasterization of the native view,
     * but will result in a potentially lower draw cache hit rate and more expensive rasterization.
     */
    ACCURATE = 1,
};

/**
RasterContext helps with rasterizing a display list into a target bitmap.
If the ExternalSurfaceRasterizationMethod is set to ACCURATE, the RasterContext will perform
composition internally before rasterizing. The given plane list can reference external surfaces
(like embedded iOS or Android views), in which case the RasterContext will take care of rasterizing
them into bitmaps and compositing them into the final bitmap. The raster context holds caches to
avoid re-rasterizing external surfaces for each frame if they don't change.

If "enableDeltaRasterization" is true, all the raster operations will be delta rasterized, with the
RasterContext keeping a bitmap cache of the last raster pass.
 */
class RasterContext : public Valdi::SimpleRefCountable {
public:
    explicit RasterContext(Valdi::ILogger& logger,
                           ExternalSurfaceRasterizationMethod externalSurfaceRasterizationMethod,
                           bool enableDeltaRasterization);
    ~RasterContext() override;

    struct RasterResult {
        size_t renderedPixelsCount = 0;
        std::vector<Rect> damageRects;
    };

    Valdi::Result<RasterResult> raster(const Ref<DisplayList>& displayList,
                                       const Ref<Valdi::IBitmap>& bitmap,
                                       bool shouldClearBitmapBeforeDrawing);

    /**
    Raster the region of the display list that changed since the last draw into the given bitmap.
     */
    Valdi::Result<RasterResult> rasterDelta(const Ref<DisplayList>& displayList, const Ref<Valdi::IBitmap>& bitmap);

private:
    struct CachedRasterizedExternalSurface {
        Ref<Image> image;
        Ref<ExternalSurfaceSnapshot> externalSurfaceSnapshot;
        Rect frame;
        Matrix transform;
        Scalar rasterScaleX = 0;
        Scalar rasterScaleY = 0;
        size_t lastRasterId = 0;
    };

    struct CompositionResult;

    mutable std::recursive_mutex _mutex;
    Valdi::ILogger& _logger;
    ExternalSurfaceRasterizationMethod _externalSurfaceRasterizationMethod;
    std::vector<CachedRasterizedExternalSurface> _cachedRasterizedExternalSurfaces;
    BitmapCache _bitmapCache;
    std::atomic<size_t> _rasterSequence = 0;
    Ref<Valdi::IBitmap> _lastBitmap;
    RasterDamageResolver _rasterDamageResolver;
    bool _deltaRasterizationEnabled;

    CompositionResult performCompositionIfNeeded(const Ref<DisplayList>& displayList) const;

    Valdi::Result<Valdi::Void> doRaster(DrawableSurfaceCanvas& canvas,
                                        const DisplayList& displayList,
                                        const CompositorPlaneList& planeList,
                                        const Valdi::BitmapInfo& bitmapInfo,
                                        bool shouldClearBitmapBeforeDrawing,
                                        size_t rasterId);

    Valdi::Result<RasterResult> doRasterDelta(const CompositionResult& compositionResult,
                                              const Ref<Valdi::IBitmap>& bitmap,
                                              const Valdi::BitmapInfo& bitmapInfo,
                                              const std::vector<Rect>& damageRects,
                                              size_t rasterId);

    Valdi::Result<Valdi::Void> rasterNonDelta(const Ref<Valdi::IBitmap>& bitmap,
                                              const DisplayList& displayList,
                                              const CompositorPlaneList& planeList,
                                              const Valdi::BitmapInfo& bitmapInfo,
                                              bool shouldClearBitmapBeforeDrawing,
                                              size_t rasterId);

    Valdi::Result<Ref<Image>> getOrCreateRasterImageForExternalSurfaceSnapshot(
        ExternalSurfaceSnapshot* externalSurfaceSnapshot,
        const Rect& frame,
        const Matrix& transform,
        const Valdi::BitmapInfo& bitmapInfo,
        Scalar rasterScaleX,
        Scalar rasterScaleY,
        size_t rasterId);

    void removeUnusedCachedRasterizedExternalSurfaces(size_t rasterId);

    bool needsNewBitmap(const Valdi::BitmapInfo& inputBitmapInfo) const;

    std::vector<Rect> computeDamageRects(const Ref<DisplayList>& displayList, const Valdi::BitmapInfo& bitmapInfo);

    Valdi::Result<Valdi::Void> blitDeltaBitmapToOutputBitmap(const Ref<Valdi::IBitmap>& deltaBitmap,
                                                             const Ref<Valdi::IBitmap>& outputBitmap,
                                                             const Valdi::BitmapInfo& bitmapInfo,
                                                             bool fullReplace);
};

} // namespace snap::drawing