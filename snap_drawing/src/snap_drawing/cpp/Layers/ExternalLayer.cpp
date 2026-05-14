//
//  ExternalLayer.cpp
//  snap_drawing-macos
//
//  Created by Simon Corsin on 2/1/22.
//

#include "snap_drawing/cpp/Layers/ExternalLayer.hpp"
#include "snap_drawing/cpp/Drawing/Surface/ExternalSurfacePresenterState.hpp"
#include "snap_drawing/cpp/Layers/Interfaces/ILayerRoot.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"

namespace snap::drawing {

ExternalLayer::ExternalLayer(const Ref<Resources>& resources) : Layer(resources) {}
ExternalLayer::~ExternalLayer() = default;

void ExternalLayer::setExternalSurface(const Ref<ExternalSurface>& externalSurface) {
    if (_externalSurface != externalSurface) {
        _externalSurface = externalSurface;
        setNeedsDisplay();
    }
}

const Ref<ExternalSurface>& ExternalLayer::getExternalSurface() const {
    return _externalSurface;
}

bool ExternalLayer::shouldRasterizeExternalSurface() const {
    auto* root = getRoot();
    if (root == nullptr) {
        return true;
    }
    return root->shouldRasterizeExternalSurface();
}

Valdi::Result<Ref<Image>> ExternalLayer::rasterExternalSurface(int width, int height) const {
    VALDI_TRACE("SnapDrawing.rasterExternalSurface");
    auto bitmapFactory = _externalSurface->getRasterBitmapFactory();
    if (bitmapFactory == nullptr) {
        return Valdi::Error("No bitmap factory");
    }
    // When rasterizing to off-screen buffer, use 1:1 scale. This is because
    // unlike the display, the off-screen rasterization buffer is not high
    // density.
    auto displayScale = shouldRasterizeExternalSurface() ? 1 : getResources()->getDisplayScale();
    auto widthInPixels = static_cast<int>(width * displayScale);
    auto heightInPixels = static_cast<int>(height * displayScale);
    auto bitmap = bitmapFactory->createBitmap(widthInPixels, heightInPixels);
    if (!bitmap) {
        return bitmap.moveError();
    }

    auto rasterIntoResult = _externalSurface->rasterInto(
        bitmap.value(), Rect::makeXYWH(0, 0, width, height), Matrix(), displayScale, displayScale);
    if (!rasterIntoResult) {
        return rasterIntoResult.error().rethrow("Failed to rasterize external surface");
    }

    return Image::makeFromBitmap(bitmap.value(), false);
}

void ExternalLayer::onDraw(DrawingContext& drawingContext) {
    if (_externalSurface != nullptr) {
        auto frameSize = getFrame().size();
        _externalSurface->setRelativeSize(frameSize);

        if (shouldRasterizeExternalSurface()) {
            auto imageResult = rasterExternalSurface(frameSize.width, frameSize.height);

            if (imageResult) {
                auto image = imageResult.moveValue();
                auto imageRect =
                    Rect::makeLTRB(0, 0, static_cast<Scalar>(image->width()), static_cast<Scalar>(image->height()));
                auto bounds = Rect::makeXYWH(0, 0, frameSize.width, frameSize.height);
                drawingContext.drawImage(*image, imageRect, bounds, nullptr);
            } else {
                VALDI_ERROR(getLogger(), "Failed to draw ExternalLayer: {}", imageResult.error());
            }
        } else {
            drawingContext.drawExternalSurface(_externalSurface);
        }
    }
}

} // namespace snap::drawing
