#include <cstring>
#include <gtest/gtest.h>

#include "TestBitmap.hpp"
#include "snap_drawing/cpp/Text/IFontManager.hpp"
#include "snap_drawing/cpp/Utils/AnimatedImage.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "snap_drawing/cpp/Utils/SVGUtils.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"

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

class TestBitmapFactory : public Valdi::IBitmapFactory {
public:
    Valdi::Result<Valdi::Ref<Valdi::IBitmap>> createBitmap(int width, int height) override {
        Valdi::Ref<Valdi::IBitmap> bitmap = Valdi::makeShared<TestBitmap>(width, height);
        return bitmap;
    }
};

// --- isSVG detection ---

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

TEST(SVGImage, doesNotDetectPNGAsSVG) {
    auto png = bytesFromString("\x89PNG\r\n\x1a\n");

    ASSERT_FALSE(Image::isSVG(png));
}

TEST(SVGImage, doesNotDetectEmptyDataAsSVG) {
    auto empty = bytesFromString("");

    ASSERT_FALSE(Image::isSVG(empty));
}

// --- Rasterization (Image::make / makeFromSVG) ---

TEST(SVGImage, rasterizesSVGWithIntrinsicSize) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");

    auto image = Image::make(svg);

    ASSERT_TRUE(image) << image.error().toString();
    EXPECT_EQ(12, image.value()->width());
    EXPECT_EQ(10, image.value()->height());
    expectBitmapIsRed(image.value());
}

TEST(SVGImage, rasterizesSVGWithPreferredSize) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");

    auto image = Image::makeFromSVG(svg, 24, 20);

    ASSERT_TRUE(image) << image.error().toString();
    EXPECT_EQ(24, image.value()->width());
    EXPECT_EQ(20, image.value()->height());
    expectBitmapIsRed(image.value());
}

TEST(SVGImage, failsOnMalformedSVG) {
    auto malformed = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                                     "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>");

    auto image = Image::make(malformed);

    ASSERT_FALSE(image);
}

TEST(SVGImage, failsOnTruncatedSVGTag) {
    auto truncated = bytesFromString("<svg width=\"12\"");

    auto image = Image::make(truncated);

    ASSERT_FALSE(image);
}

// --- AnimatedImage ---

TEST(SVGImage, createsAnimatedImageForSVG) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<circle cx=\"6\" cy=\"5\" r=\"5\" fill=\"#00ff00\"/>"
                               "</svg>");

    auto image = AnimatedImage::make(nullptr, svg.data(), svg.size());

    ASSERT_TRUE(image) << image.error().toString();
    EXPECT_EQ(12, image.value()->getSize().width);
    EXPECT_EQ(10, image.value()->getSize().height);
    EXPECT_EQ(0.0, image.value()->getFrameRate());
}

TEST(SVGImage, animatedImageFailsOnMalformedSVG) {
    auto malformed = bytesFromString("<svg><rect/>");

    auto image = AnimatedImage::make(nullptr, malformed.data(), malformed.size());

    ASSERT_FALSE(image);
}

// --- SVGUtils direct tests ---

TEST(SVGUtils, getSVGSizeReturnsIntrinsicDimensions) {
    auto svg = bytesFromString("<svg width=\"50\" height=\"30\" xmlns=\"http://www.w3.org/2000/svg\"></svg>");

    auto size = getSVGSize(svg);

    ASSERT_TRUE(size) << size.error().toString();
    EXPECT_EQ(50, size.value().first);
    EXPECT_EQ(30, size.value().second);
}

TEST(SVGUtils, getSVGSizeFailsOnMalformedInput) {
    auto malformed = bytesFromString("<svg width=\"50\"");

    auto size = getSVGSize(malformed);

    ASSERT_FALSE(size);
}

TEST(SVGUtils, rasterizeSVGFailsWithNullBitmapFactory) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");

    auto result = rasterizeSVG(svg, nullptr);

    ASSERT_FALSE(result);
}

TEST(SVGUtils, rasterizeSVGSucceedsWithBitmapFactory) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");
    auto factory = Valdi::makeShared<TestBitmapFactory>();

    auto result = rasterizeSVG(svg, factory);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(12, result.value()->getInfo().width);
    EXPECT_EQ(10, result.value()->getInfo().height);
}

TEST(SVGUtils, rasterizeSVGScalesWithPreferredWidth) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");
    auto factory = Valdi::makeShared<TestBitmapFactory>();

    auto result = rasterizeSVG(svg, factory, 24);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(24, result.value()->getInfo().width);
    EXPECT_EQ(20, result.value()->getInfo().height);
}

TEST(SVGUtils, rasterizeSVGScalesWithPreferredHeight) {
    auto svg = bytesFromString("<svg width=\"12\" height=\"10\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"12\" height=\"10\" fill=\"#ff0000\"/>"
                               "</svg>");
    auto factory = Valdi::makeShared<TestBitmapFactory>();

    auto result = rasterizeSVG(svg, factory, 0, 20);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(24, result.value()->getInfo().width);
    EXPECT_EQ(20, result.value()->getInfo().height);
}

TEST(SVGUtils, rasterizeSVGFailsOnMalformedSVG) {
    auto malformed = bytesFromString("<svg width=\"12\"");
    auto factory = Valdi::makeShared<TestBitmapFactory>();

    auto result = rasterizeSVG(malformed, factory);

    ASSERT_FALSE(result);
}

TEST(SVGUtils, rasterizeSVGViewBoxOnlyFailsWithoutPreferredSize) {
    auto svg = bytesFromString("<svg viewBox=\"0 0 100 80\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"100\" height=\"80\" fill=\"#0000ff\"/>"
                               "</svg>");
    auto factory = Valdi::makeShared<TestBitmapFactory>();

    auto result = rasterizeSVG(svg, factory);

    ASSERT_FALSE(result);
}

TEST(SVGUtils, rasterizeSVGViewBoxOnlySucceedsWithPreferredSize) {
    auto svg = bytesFromString("<svg viewBox=\"0 0 100 80\" xmlns=\"http://www.w3.org/2000/svg\">"
                               "<rect width=\"100\" height=\"80\" fill=\"#0000ff\"/>"
                               "</svg>");
    auto factory = Valdi::makeShared<TestBitmapFactory>();

    auto result = rasterizeSVG(svg, factory, 50, 40);

    ASSERT_TRUE(result) << result.error().toString();
    EXPECT_EQ(50, result.value()->getInfo().width);
    EXPECT_EQ(40, result.value()->getInfo().height);
}

} // namespace snap::drawing
