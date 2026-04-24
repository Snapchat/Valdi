#include <cstring>
#include <gtest/gtest.h>

#include "snap_drawing/cpp/Utils/AnimatedImage.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "snap_drawing/cpp/Text/IFontManager.hpp"
#include "TestBitmap.hpp"

namespace snap::drawing {

static Valdi::BytesView bytesFromString(const char* value) {
    return Valdi::BytesView(nullptr, reinterpret_cast<const Valdi::Byte*>(value), strlen(value));
}

static void expectBitmapIsRed(const Ref<Image>& image) {
    TestBitmap bitmap(image->width(), image->height());
    auto info = bitmap.getInfo();
    auto* bytes = bitmap.lockBytes();
    image->draw(info, bytes);
    bitmap.unlockBytes();

    for (int y = 0; y < image->height(); y++) {
        for (int x = 0; x < image->width(); x++) {
            EXPECT_EQ(Color::red(), bitmap.getPixel(x, y)) << "at (" << x << ", " << y << ")";
        }
    }
}

TEST(SVGImage, detectsSVGAfterWhitespace) {
    auto svg = bytesFromString(" \n<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\"></svg>");

    ASSERT_TRUE(Image::isSVG(svg));
}

TEST(SVGImage, detectsXMLPreamble) {
    auto svg = bytesFromString("<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg></svg>");

    ASSERT_TRUE(Image::isSVG(svg));
}

TEST(SVGImage, doesNotDetectLottieAsSVG) {
    auto json = bytesFromString("{\"v\":\"5.7.4\",\"layers\":[]}");

    ASSERT_FALSE(Image::isSVG(json));
}

TEST(SVGImage, rasterizesSVGWithIntrinsicSize) {
    auto svg = bytesFromString(
        "<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
        "</svg>");

    auto image = Image::make(svg);

    ASSERT_TRUE(image) << image.error().toString();
    EXPECT_EQ(12, image.value()->width());
    EXPECT_EQ(10, image.value()->height());
    expectBitmapIsRed(image.value());
}

TEST(SVGImage, rasterizesSVGWithPreferredSize) {
    auto svg = bytesFromString(
        "<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
        "</svg>");

    auto image = Image::makeFromSVG(svg, 24, 20);

    ASSERT_TRUE(image) << image.error().toString();
    EXPECT_EQ(24, image.value()->width());
    EXPECT_EQ(20, image.value()->height());
    expectBitmapIsRed(image.value());
}

TEST(SVGImage, createsAnimatedImageForSVG) {
    auto svg = bytesFromString(
        "<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
        "<circle cx=\"6\" cy=\"5\" r=\"5\" fill=\"#00ff00\"/>"
        "</svg>");

    auto image = AnimatedImage::make(nullptr, svg.data(), svg.size());

    ASSERT_TRUE(image) << image.error().toString();
    EXPECT_EQ(12, image.value()->getSize().width);
    EXPECT_EQ(10, image.value()->getSize().height);
    EXPECT_EQ(0.0, image.value()->getFrameRate());
}

} // namespace snap::drawing
