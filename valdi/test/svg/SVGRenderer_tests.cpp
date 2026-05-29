#include <cstring>
#include <gtest/gtest.h>

#include "TestBitmap.hpp"
#include "snap_drawing/cpp/Utils/SVGUtils.hpp"
#include "valdi/svg/SVGRenderer.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"

namespace {

Valdi::BytesView bytesFromString(const char* value) {
    return Valdi::BytesView(nullptr, reinterpret_cast<const Valdi::Byte*>(value), strlen(value));
}

class TrackingBitmapFactory : public Valdi::IBitmapFactory {
public:
    Valdi::Result<Valdi::Ref<Valdi::IBitmap>> createBitmap(int width, int height) override {
        lastBitmap = Valdi::makeShared<snap::drawing::TestBitmap>(width, height);
        return Valdi::Ref<Valdi::IBitmap>(lastBitmap);
    }

    Valdi::Ref<snap::drawing::TestBitmap> lastBitmap;
};

void expectBitmapIsRed(const Valdi::Ref<snap::drawing::TestBitmap>& bitmap) {
    auto info = bitmap->getInfo();
    for (int y = 0; y < info.height; y++) {
        for (int x = 0; x < info.width; x++) {
            EXPECT_EQ(snap::drawing::Color::red(), bitmap->getPixel(x, y)) << "at (" << x << ", " << y << ")";
        }
    }
}

} // namespace

TEST(SVGRenderer, delegatesDetectionToSharedSVGUtils) {
    auto svg = bytesFromString(" \n<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\"></svg>");
    auto xml = bytesFromString("<?xml version=\"1.0\"?><svg></svg>");
    auto json = bytesFromString("{\"v\":\"5.7.4\",\"layers\":[]}");

    EXPECT_TRUE(snap::drawing::isSVG(svg));
    EXPECT_TRUE(Valdi::SVGRenderer::isSVG(svg));
    EXPECT_TRUE(Valdi::SVGRenderer::isSVG(xml));
    EXPECT_FALSE(Valdi::SVGRenderer::isSVG(json));
}

TEST(SVGRenderer, reportsInvalidSVG) {
    auto factory = Valdi::makeShared<TrackingBitmapFactory>();

    auto result = Valdi::SVGRenderer::rasterizeSVG(bytesFromString("<svg"), factory, 0, 0);

    EXPECT_FALSE(result);
}

TEST(SVGRenderer, rasterizesUsingIntrinsicSize) {
    auto factory = Valdi::makeShared<TrackingBitmapFactory>();
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");

    auto result = Valdi::SVGRenderer::rasterizeSVG(svg, factory, 0, 0);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(factory->lastBitmap.get(), result.value().get());
    EXPECT_EQ(12, result.value()->getInfo().width);
    EXPECT_EQ(10, result.value()->getInfo().height);
    expectBitmapIsRed(factory->lastBitmap);
}

TEST(SVGRenderer, rasterizesUsingPreferredSize) {
    auto factory = Valdi::makeShared<TrackingBitmapFactory>();
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");

    auto result = Valdi::SVGRenderer::rasterizeSVG(svg, factory, 24, 20);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(24, result.value()->getInfo().width);
    EXPECT_EQ(20, result.value()->getInfo().height);
    expectBitmapIsRed(factory->lastBitmap);
}

TEST(SVGRenderer, preservesAspectRatioWhenOnePreferredDimensionIsProvided) {
    auto factory = Valdi::makeShared<TrackingBitmapFactory>();
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");

    auto result = Valdi::SVGRenderer::rasterizeSVG(svg, factory, 24, 0);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(24, result.value()->getInfo().width);
    EXPECT_EQ(20, result.value()->getInfo().height);
    expectBitmapIsRed(factory->lastBitmap);
}
