#include "RasterContext.hpp"
#include "include/core/SkCanvas.h"
#include "snap_drawing/cpp/Drawing/Composition/CompositionState.hpp"
#include "snap_drawing/cpp/Drawing/Composition/Compositor.hpp"
#include "snap_drawing/cpp/Drawing/Composition/CompositorPlaneList.hpp"
#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/Paint.hpp"
#include "snap_drawing/cpp/Drawing/Raster/RasterDamageResolver.hpp"
#include "snap_drawing/cpp/Drawing/Surface/ExternalSurface.hpp"
#include "snap_drawing/cpp/Drawing/Surface/ExternalSurfacePresenterState.hpp"
#include "snap_drawing/cpp/Utils/Bitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Interfaces/ILogger.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include <cstddef>
#include <cstdint>

// These come from Skia SkBlitRow.h .
// We use them for blending rows. They are highly optimized under the hood,
// so it's best to re-use instead of rolling out our own.
class SkBlitRow {
public:
    // NOLINTNEXTLINE(readability-identifier-naming)
    enum Flags32 { kGlobalAlpha_Flag32 = 1 << 0, kSrcPixelAlpha_Flag32 = 1 << 1 };

    // NOLINTNEXTLINE(readability-identifier-naming)
    using Proc32 = void (*)(uint32_t dst[], const SkPMColor src[], int count, U8CPU alpha);

    // NOLINTNEXTLINE(readability-identifier-naming)
    static Proc32 Factory32(unsigned flags32);
};

namespace snap::drawing {

struct RasterContext::CompositionResult {
    CompositorPlaneList planeList;
    Ref<DisplayList> displayList;
};

RasterContext::RasterContext(Valdi::ILogger& logger,
                             ExternalSurfaceRasterizationMethod externalSurfaceRasterizationMethod,
                             bool enableDeltaRasterization)
    : _logger(logger),
      _externalSurfaceRasterizationMethod(externalSurfaceRasterizationMethod),
      _deltaRasterizationEnabled(enableDeltaRasterization) {}
RasterContext::~RasterContext() = default;

RasterContext::CompositionResult RasterContext::performCompositionIfNeeded(const Ref<DisplayList>& displayList) const {
    CompositionResult result;

    if (_externalSurfaceRasterizationMethod == ExternalSurfaceRasterizationMethod::ACCURATE &&
        displayList->hasExternalSurfaces()) {
        Compositor compositor(_logger);
        result.displayList = compositor.performComposition(*displayList, result.planeList);
    } else {
        result.planeList.appendDrawableSurface();
        result.displayList = displayList;
    }

    return result;
}

Valdi::Result<RasterContext::RasterResult> RasterContext::raster(const Ref<DisplayList>& displayList,
                                                                 const Ref<Valdi::IBitmap>& bitmap,
                                                                 bool shouldClearBitmapBeforeDrawing) {
    VALDI_TRACE("SnapDrawing.rasterContext.raster");
    auto rasterId = ++_rasterSequence;

    auto inputBitmapInfo = bitmap->getInfo();
    auto composition = performCompositionIfNeeded(displayList);

    RasterResult output;
    output.renderedPixelsCount = inputBitmapInfo.width * inputBitmapInfo.height;

    if (_deltaRasterizationEnabled) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        output.damageRects = computeDamageRects(displayList, inputBitmapInfo);

        if (needsNewBitmap(inputBitmapInfo)) {
            // Cannot do a delta raster if the input bitmap info has changed
            _lastBitmap = nullptr;

            {
                VALDI_TRACE("SnapDrawing.rasterContext.allocateDeltaBitmap");
                auto newBitmap = Bitmap::make(inputBitmapInfo);
                if (!newBitmap) {
                    return newBitmap.moveError();
                }

                _lastBitmap = newBitmap.value();
            }

            auto result = rasterNonDelta(_lastBitmap,
                                         *composition.displayList,
                                         composition.planeList,
                                         inputBitmapInfo,
                                         shouldClearBitmapBeforeDrawing,
                                         rasterId);
            if (!result) {
                return result.moveError();
            }

            // Calculate rendered pixel count from damage rects for consistency
            output.renderedPixelsCount = 0;
            for (const auto& damageRect : output.damageRects) {
                output.renderedPixelsCount +=
                    static_cast<size_t>(damageRect.width()) * static_cast<size_t>(damageRect.height());
            }
        } else {
            auto result = doRasterDelta(composition, _lastBitmap, inputBitmapInfo, output.damageRects, rasterId);
            if (!result) {
                return result.moveError();
            }

            output.renderedPixelsCount = result.value().renderedPixelsCount;
        }

        auto result =
            blitDeltaBitmapToOutputBitmap(_lastBitmap, bitmap, inputBitmapInfo, shouldClearBitmapBeforeDrawing);
        if (!result) {
            return result.moveError();
        }
    } else {
        auto result = rasterNonDelta(bitmap,
                                     *composition.displayList,
                                     composition.planeList,
                                     inputBitmapInfo,
                                     shouldClearBitmapBeforeDrawing,
                                     rasterId);
        if (!result) {
            return result.moveError();
        }
    }

    removeUnusedCachedRasterizedExternalSurfaces(rasterId);

    return output;
}

Valdi::Result<Valdi::Void> RasterContext::blitDeltaBitmapToOutputBitmap(const Ref<Valdi::IBitmap>& deltaBitmap,
                                                                        const Ref<Valdi::IBitmap>& outputBitmap,
                                                                        const Valdi::BitmapInfo& bitmapInfo,
                                                                        bool fullReplace) {
    if (!fullReplace) {
        if (bitmapInfo.alphaType != Valdi::AlphaType::AlphaTypePremul) {
            return Valdi::Error("Delta rasterization is currently only supported for premultiplied alpha bitmaps");
        }

        if (bitmapInfo.colorType != Valdi::ColorType::ColorTypeRGBA8888 &&
            bitmapInfo.colorType != Valdi::ColorType::ColorTypeBGRA8888) {
            return Valdi::Error("Delta rasterization is currently only supported for RGBA or BGRA bitmaps");
        }
    }

    auto deltaBitmapInfo = deltaBitmap->getInfo();

    // Copy bytes from our bitmap to the input bitmap
    auto inputBytes = deltaBitmap->lockBytes();
    if (inputBytes == nullptr) {
        return Valdi::Error("Failed to lock bytes");
    }

    auto outputBytes = outputBitmap->lockBytes();
    if (outputBytes == nullptr) {
        deltaBitmap->unlockBytes();
        return Valdi::Error("Failed to lock bytes");
    }

    if (fullReplace) {
        if (deltaBitmapInfo.bytesLength() == bitmapInfo.bytesLength()) {
            std::memcpy(outputBytes, inputBytes, bitmapInfo.bytesLength());
        } else {
            auto minRowBytes = std::min(deltaBitmapInfo.rowBytes, bitmapInfo.rowBytes);
            auto* outputPtr = reinterpret_cast<uint8_t*>(outputBytes);
            const auto* inputPtr = reinterpret_cast<const uint8_t*>(inputBytes);
            for (int y = 0; y < bitmapInfo.height; y++) {
                std::memcpy(outputPtr, inputPtr, minRowBytes);

                inputPtr += deltaBitmapInfo.rowBytes;
                outputPtr += bitmapInfo.rowBytes;
            }
        }
    } else {
        auto processProc = SkBlitRow::Factory32(SkBlitRow::kSrcPixelAlpha_Flag32);

        auto* outputPtr = reinterpret_cast<uint8_t*>(outputBytes);
        const auto* inputPtr = reinterpret_cast<const uint8_t*>(inputBytes);

        for (int y = 0; y < bitmapInfo.height; y++) {
            processProc(reinterpret_cast<uint32_t*>(outputPtr),
                        reinterpret_cast<const uint32_t*>(inputPtr),
                        bitmapInfo.width,
                        255);

            inputPtr += deltaBitmapInfo.rowBytes;
            outputPtr += bitmapInfo.rowBytes;
        }
    }

    deltaBitmap->unlockBytes();
    outputBitmap->unlockBytes();
    return Valdi::Void();
}

std::vector<Rect> RasterContext::computeDamageRects(const Ref<DisplayList>& displayList,
                                                    const Valdi::BitmapInfo& bitmapInfo) {
    VALDI_TRACE("SnapDrawing.rasterContext.computeDamageRects");
    _rasterDamageResolver.beginUpdates(bitmapInfo.width, bitmapInfo.height);
    _rasterDamageResolver.addDamageFromDisplayListUpdates(*displayList);

    return _rasterDamageResolver.endUpdates();
}

Valdi::Result<RasterContext::RasterResult> RasterContext::rasterDelta(const Ref<DisplayList>& displayList,
                                                                      const Ref<Valdi::IBitmap>& bitmap) {
    VALDI_TRACE("SnapDrawing.rasterContext.rasterDelta");
    auto inputBitmapInfo = bitmap->getInfo();
    auto rasterId = ++_rasterSequence;

    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto damageRects = computeDamageRects(displayList, inputBitmapInfo);

    auto composition = performCompositionIfNeeded(displayList);
    auto result = doRasterDelta(composition, bitmap, bitmap->getInfo(), damageRects, rasterId);

    removeUnusedCachedRasterizedExternalSurfaces(rasterId);

    if (result) {
        result.value().damageRects = std::move(damageRects);
    }

    return result;
}

Valdi::Result<RasterContext::RasterResult> RasterContext::doRasterDelta(const CompositionResult& compositionResult,
                                                                        const Ref<Valdi::IBitmap>& bitmap,
                                                                        const Valdi::BitmapInfo& bitmapInfo,
                                                                        const std::vector<Rect>& damageRects,
                                                                        size_t rasterId) {
    BitmapGraphicsContext graphicsContext;
    auto surface = graphicsContext.createBitmapSurface(bitmap);

    auto canvas = surface->prepareCanvas();
    if (!canvas) {
        return canvas.moveError();
    }

    RasterResult output;
    output.renderedPixelsCount = 0;

    for (const auto& damageRect : damageRects) {
        VALDI_TRACE("SnapDrawing.rasterContext.rasterDeltaInRect");
        auto* skiaCanvas = canvas.value().getSkiaCanvas();
        auto saveCount = skiaCanvas->save();
        skiaCanvas->clipRect(damageRect.getSkValue());

        auto doRasterResult = doRaster(
            canvas.value(), *compositionResult.displayList, compositionResult.planeList, bitmapInfo, true, rasterId);

        skiaCanvas->restoreToCount(saveCount);

        if (!doRasterResult) {
            return doRasterResult.moveError();
        }

        output.renderedPixelsCount +=
            static_cast<size_t>(damageRect.width()) * static_cast<size_t>(damageRect.height());
    }

    surface->flush();

    return output;
}

Valdi::Result<Valdi::Void> RasterContext::rasterNonDelta(const Ref<Valdi::IBitmap>& bitmap,
                                                         const DisplayList& displayList,
                                                         const CompositorPlaneList& planeList,
                                                         const Valdi::BitmapInfo& bitmapInfo,
                                                         bool shouldClearBitmapBeforeDrawing,
                                                         size_t rasterId) {
    VALDI_TRACE("SnapDrawing.rasterContext.rasterNonDelta");
    BitmapGraphicsContext graphicsContext;
    auto surface = graphicsContext.createBitmapSurface(bitmap);

    auto canvas = surface->prepareCanvas();
    if (!canvas) {
        return canvas.moveError();
    }

    auto doRasterResult =
        doRaster(canvas.value(), displayList, planeList, bitmapInfo, shouldClearBitmapBeforeDrawing, rasterId);

    surface->flush();

    return doRasterResult;
}

Valdi::Result<Valdi::Void> RasterContext::doRaster(DrawableSurfaceCanvas& canvas,
                                                   const DisplayList& displayList,
                                                   const CompositorPlaneList& planeList,
                                                   const Valdi::BitmapInfo& bitmapInfo,
                                                   bool shouldClearBitmapBeforeDrawing,
                                                   size_t rasterId) {
    if (shouldClearBitmapBeforeDrawing) {
        Paint paint;
        paint.setColor(Color::transparent());
        paint.setBlendMode(SkBlendMode::kSrc);
        canvas.getSkiaCanvas()->drawPaint(paint.getSkValue());
    }

    size_t drawablePlaneIndex = 0;
    for (const auto& plane : planeList) {
        switch (plane.getType()) {
            case CompositorPlaneTypeDrawable:
                displayList.draw(canvas, drawablePlaneIndex, false);
                drawablePlaneIndex++;
                break;
            case CompositorPlaneTypeExternal: {
                auto rasterScaleX = bitmapInfo.width / displayList.getSize().width;
                auto rasterScaleY = bitmapInfo.height / displayList.getSize().height;
                const auto& presenterState = *plane.getExternalSurfacePresenterState();

                auto rasterImage = getOrCreateRasterImageForExternalSurfaceSnapshot(plane.getExternalSurfaceSnapshot(),
                                                                                    presenterState.frame,
                                                                                    presenterState.transform,
                                                                                    bitmapInfo,
                                                                                    rasterScaleX,
                                                                                    rasterScaleY,
                                                                                    rasterId);
                if (!rasterImage) {
                    return rasterImage.moveError();
                }

                auto* skiaCanvas = canvas.getSkiaCanvas();
                auto saveCount = skiaCanvas->save();

                if (!presenterState.clipPath.isEmpty()) {
                    auto clipPath = presenterState.clipPath;
                    clipPath.transform(Matrix::makeScale(rasterScaleX, rasterScaleY));
                    skiaCanvas->clipPath(clipPath.getSkValue(), true);
                }

                Paint paint;
                paint.setAntiAlias(true);
                paint.setAlpha(presenterState.opacity);
                skiaCanvas->drawImage(
                    rasterImage.value()->getSkValue(), 0, 0, SkSamplingOptions(), &paint.getSkValue());
                skiaCanvas->restoreToCount(saveCount);
            } break;
        }
    }

    return Valdi::Void();
}

void RasterContext::removeUnusedCachedRasterizedExternalSurfaces(size_t rasterId) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _cachedRasterizedExternalSurfaces.erase(std::remove_if(_cachedRasterizedExternalSurfaces.begin(),
                                                           _cachedRasterizedExternalSurfaces.end(),
                                                           [rasterId](const auto& cachedRasterizedExternalSurface) {
                                                               return cachedRasterizedExternalSurface.lastRasterId <
                                                                      rasterId;
                                                           }),
                                            _cachedRasterizedExternalSurfaces.end());
}

Valdi::Result<Ref<Image>> RasterContext::getOrCreateRasterImageForExternalSurfaceSnapshot(
    ExternalSurfaceSnapshot* externalSurfaceSnapshot,
    const Rect& frame,
    const Matrix& transform,
    const Valdi::BitmapInfo& bitmapInfo,
    Scalar rasterScaleX,
    Scalar rasterScaleY,
    size_t rasterId) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    for (auto& cachedRasterizedExternalSurface : _cachedRasterizedExternalSurfaces) {
        if (cachedRasterizedExternalSurface.externalSurfaceSnapshot.get() == externalSurfaceSnapshot &&
            cachedRasterizedExternalSurface.frame == frame && cachedRasterizedExternalSurface.transform == transform &&
            cachedRasterizedExternalSurface.rasterScaleX == rasterScaleX &&
            cachedRasterizedExternalSurface.rasterScaleY == rasterScaleY) {
            cachedRasterizedExternalSurface.lastRasterId = rasterId;
            return cachedRasterizedExternalSurface.image;
        }
    }

    VALDI_TRACE("SnapDrawing.rasterExternalSurface");
    auto bitmapFactory = externalSurfaceSnapshot->getExternalSurface()->getRasterBitmapFactory();

    if (bitmapFactory == nullptr) {
        return Valdi::Error("Cannot rasterize external surface without a bitmap factory");
    }

    auto bitmap = _bitmapCache.allocateBitmap(bitmapFactory, bitmapInfo.width, bitmapInfo.height);
    if (!bitmap) {
        return bitmap.moveError();
    }

    void* pixels = bitmap.value()->lockBytes();
    if (pixels != nullptr) {
        std::memset(pixels, 0, bitmapInfo.bytesLength());
        bitmap.value()->unlockBytes();
    }

    auto rasterIntoResult = externalSurfaceSnapshot->getExternalSurface()->rasterInto(
        bitmap.value(), frame, transform, rasterScaleX, rasterScaleY);
    if (!rasterIntoResult) {
        return rasterIntoResult.error().rethrow("Failed to rasterize external surface");
    }

    auto image = Image::makeFromBitmap(bitmap.value(), false);
    if (!image) {
        return image.moveError();
    }

    auto& cachedEntry = _cachedRasterizedExternalSurfaces.emplace_back();
    cachedEntry.externalSurfaceSnapshot = externalSurfaceSnapshot;
    cachedEntry.frame = frame;
    cachedEntry.transform = transform;
    cachedEntry.rasterScaleX = rasterScaleX;
    cachedEntry.rasterScaleY = rasterScaleY;
    cachedEntry.lastRasterId = rasterId;
    cachedEntry.image = image.value();

    return image;
}

bool RasterContext::needsNewBitmap(const Valdi::BitmapInfo& inputBitmapInfo) const {
    if (_lastBitmap == nullptr) {
        return true;
    }

    auto lastBitmapInfo = _lastBitmap->getInfo();
    return lastBitmapInfo.width != inputBitmapInfo.width || lastBitmapInfo.height != inputBitmapInfo.height ||
           lastBitmapInfo.colorType != inputBitmapInfo.colorType ||
           lastBitmapInfo.alphaType != inputBitmapInfo.alphaType;
}

} // namespace snap::drawing