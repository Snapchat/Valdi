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

TEST(ImageBitmap, testConvertedBitmapReportsPaddedRowBytes) {
    auto bitmap = createTestBitmap();
    auto imageResult = Image::makeFromBitmap(bitmap, true);
    ASSERT_FALSE(imageResult.failure()) << imageResult.error().getMessage();

    auto convertedBitmap = imageResult.value()->toConvertedBitmap(
        Valdi::BitmapInfo(2, 2, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypeUnpremul, 16));
    ASSERT_NE(convertedBitmap, nullptr);
    ASSERT_EQ(convertedBitmap->getInfo().rowBytes, 16u);

    auto roundTripResult = Image::makeFromBitmap(convertedBitmap, true);
    ASSERT_FALSE(roundTripResult.failure()) << roundTripResult.error().getMessage();
    auto roundTripBitmap = roundTripResult.value()->getBitmap();
    ASSERT_NE(roundTripBitmap, nullptr);
    ASSERT_EQ(roundTripBitmap->getInfo().rowBytes, 8u);

    auto converted = static_cast<const uint8_t*>(convertedBitmap->lockBytes());
    auto roundTrip = static_cast<const uint8_t*>(roundTripBitmap->lockBytes());
    for (size_t row = 0; row < 2; row++) {
        ASSERT_EQ(BytesView(nullptr, converted + row * 16, 8), BytesView(nullptr, roundTrip + row * 8, 8))
            << "row " << row;
    }
}
} // namespace snap::drawing
