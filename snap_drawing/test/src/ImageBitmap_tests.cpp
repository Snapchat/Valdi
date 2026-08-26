#include <gtest/gtest.h>

#include "TestBitmap.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"

using namespace Valdi;

namespace snap::drawing {

TEST(ImageBitmap, testBitmapFullConversion) {
    auto bitmap = createTestBitmap();
    auto imageResult = Image::makeFromBitmap(bitmap, true);
    ASSERT_FALSE(imageResult.failure()) << imageResult.error().getMessage();

    auto image = imageResult.value();
    auto returnedBitmap = image->getBitmap();
    auto returnedInfo = returnedBitmap->getInfo();
    ASSERT_TRUE(returnedInfo == bitmap->getInfo());

    const auto pixelDataSize = returnedInfo.height * returnedInfo.rowBytes;
    auto bitmapData = static_cast<uint8_t*>(bitmap->lockBytes());
    auto returnedBitmapData = static_cast<uint8_t*>(returnedBitmap->lockBytes());

    ASSERT_EQ(BytesView(nullptr, bitmapData, pixelDataSize), BytesView(nullptr, returnedBitmapData, pixelDataSize));
}

TEST(ImageBitmap, testConvertedBitmapResize) {
    auto bitmap = createTestBitmap();
    auto imageResult = Image::makeFromBitmap(bitmap, true);
    ASSERT_FALSE(imageResult.failure()) << imageResult.error().getMessage();

    auto resizedBitmap = imageResult.value()->toConvertedBitmap(
        Valdi::BitmapInfo(1, 1, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypeUnpremul, 4));
    ASSERT_NE(resizedBitmap, nullptr);

    auto resizedInfo = resizedBitmap->getInfo();
    ASSERT_EQ(resizedInfo.width, 1);
    ASSERT_EQ(resizedInfo.height, 1);
    ASSERT_EQ(resizedInfo.colorType, ColorTypeRGBA8888);
    ASSERT_EQ(resizedInfo.rowBytes, 4u);
}
} // namespace snap::drawing
