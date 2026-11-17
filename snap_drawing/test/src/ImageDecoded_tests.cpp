#include <gtest/gtest.h>

#include "TestBitmap.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"

#include "include/core/SkImage.h"

namespace snap::drawing {

class ImageDecoded : public ::testing::Test {
protected:
    void SetUp() override {
        auto image = Image::makeFromBitmap(createTestBitmap(), true);
        ASSERT_TRUE(image.success());
        auto encoded = image.value()->encode(EncodedImageFormatPNG, 1.0);
        ASSERT_TRUE(encoded.success());
        _encoded = encoded.value();
    }

    Valdi::BytesView _encoded;
};

TEST_F(ImageDecoded, makeLeavesDecodingToTheFirstDraw) {
    auto image = Image::make(_encoded);
    ASSERT_TRUE(image.success());
    EXPECT_TRUE(image.value()->getSkValue()->isLazyGenerated());
}

TEST_F(ImageDecoded, decodedHoldsDecodedPixels) {
    auto image = Image::make(_encoded);
    ASSERT_TRUE(image.success());
    auto decoded = image.value()->decoded();
    EXPECT_FALSE(decoded->getSkValue()->isLazyGenerated());
    EXPECT_EQ(decoded->width(), image.value()->width());
    EXPECT_EQ(decoded->height(), image.value()->height());
}

TEST_F(ImageDecoded, decodedKeepsTheFilter) {
    auto image = Image::make(_encoded);
    ASSERT_TRUE(image.success());
    auto filtered = image.value()->withFilter(Valdi::makeShared<Valdi::ImageFilter>());
    EXPECT_EQ(filtered->decoded()->getFilter(), filtered->getFilter());
}

} // namespace snap::drawing
