#include <gtest/gtest.h>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "snap_drawing/cpp/Drawing/Mask/IMask.hpp"
#include "snap_drawing/cpp/Layers/Mask/PaintMaskLayer.hpp"
#include "snap_drawing/cpp/Utils/Path.hpp"

namespace snap::drawing {

namespace {

// Erases `maskLayer`'s region out of a 20 x 20 white bitmap and counts the pixels left partly transparent.
int blendedPixelsAfterErasing(PaintMaskLayer& maskLayer) {
    auto mask = maskLayer.createMask(Rect::makeXYWH(0, 0, 20, 20));
    EXPECT_TRUE(mask != nullptr);
    if (mask == nullptr) {
        return -1;
    }

    SkBitmap bitmap;
    bitmap.allocN32Pixels(20, 20);
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    mask->prepare(&canvas);
    canvas.drawColor(SK_ColorWHITE);
    mask->apply(&canvas);

    int blended = 0;
    for (int y = 0; y < 20; y++) {
        for (int x = 0; x < 20; x++) {
            auto alpha = SkColorGetA(bitmap.getColor(x, y));
            if (alpha > 0 && alpha < 255) {
                blended++;
            }
        }
    }
    return blended;
}

} // namespace

TEST(PaintMaskLayerTests, antiAliasesTheEdgeOfAPathRegion) {
    Path path;
    path.addOval(Rect::makeXYWH(2.5f, 2.5f, 15, 15), true);
    auto maskLayer = Valdi::makeShared<PaintMaskLayer>();
    maskLayer->setPath(path);

    // The default DstOut erases the oval out of the white; anti-aliasing blends the pixels its edge crosses.
    EXPECT_GT(blendedPixelsAfterErasing(*maskLayer), 0);
}

TEST(PaintMaskLayerTests, erasesWholePixelsForARectRegion) {
    auto maskLayer = Valdi::makeShared<PaintMaskLayer>();
    maskLayer->setRect(Rect::makeXYWH(2.5f, 2.5f, 15, 15));

    EXPECT_EQ(0, blendedPixelsAfterErasing(*maskLayer));
}

} // namespace snap::drawing
