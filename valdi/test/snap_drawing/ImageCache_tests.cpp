#include "BlockingAssetCompletionHandler.hpp"
#include "MockAssetLoader.hpp"
#include "MockDownloader.hpp"
#include "TestAsyncUtils.hpp"
#include "TestBitmap.hpp"
#include "TestDataUtils.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageCache.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoaderFactory.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/SimpleAtomicCancelable.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_test_utils.hpp"
#include <gtest/gtest.h>
#include <memory>

using namespace Valdi;
using namespace snap::drawing;

namespace ValdiTest {

class ImageCacheTestsBase {
protected:
    void SetUpBase() {
        auto img = Image::makeFromBitmap(createTestBitmap(), true).value();
        // NOTE(rjaber): We are relying on the fact that image cache will never evict in use images
        _cache = std::make_unique<ImageCache>(ConsoleLogger::getLogger(), ImageCacheItem::imageSize(img));
        _cache->setMaxAge(_maxAge);

        auto zeroBitmap = loadResourceFromDisk("testdata/0x0.bmp");
        ASSERT_FALSE(zeroBitmap.failure())
            << "Failed to load testdata/0x0.bmp (ensure the file is in test data/runfiles)";

        _width = img->width();
        _height = img->height();

        auto cancel = makeShared<SimpleAtomicCancelable>();
        _cache->setCachedItemAndGetResizedImage(_url, img, _width, _height);

        _imgSize = ImageCacheItem::imageSize(img);
    }

    Result<Valdi::Ref<Image>> getImage(const StringBox& url, int32_t width, int32_t height) {
        auto result = _cache->getResizedCachedImage(url, width, height);
        if (result) {
            return result.value().image;
        } else {
            return result.error();
        }
    }

    void TearDownBase() {}

    int getWidth() {
        return _width;
    }

    int getHeight() {
        return _height;
    }

    std::unique_ptr<ImageCache> _cache;
    StringBox _url = STRING_LITERAL("asset://module/local");
    size_t _imgSize;
    uint64_t _maxAge = 0;

private:
    int _width;
    int _height;
};

class ImageCacheTests : public ImageCacheTestsBase, public ::testing::Test {
protected:
    ~ImageCacheTests() override {};

    void SetUp() override {
        SetUpBase();
    }

    void TearDown() override {
        TearDownBase();
    }
};

TEST_F(ImageCacheTests, getUpscaleReturnOriginalSize) {
    auto result = getImage(_url, getWidth() * 2, getHeight() * 2);

    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == getWidth());
    ASSERT_TRUE(image->height() == getHeight());
}

TEST_F(ImageCacheTests, requestingOriginalDimensionReturnOriginalSize) {
    auto result = getImage(_url, getWidth(), getHeight());
    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == getWidth());
    ASSERT_TRUE(image->height() == getHeight());
}

TEST_F(ImageCacheTests, zeroDimensionReturnOriginalSize) {
    auto result = getImage(_url, 0, 0);
    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == getWidth());
    ASSERT_TRUE(image->height() == getHeight());
}

TEST_F(ImageCacheTests, resizeOnWidth) {
    int desiredWidth = getWidth() / 2;
    int expectedWidth = desiredWidth;
    int expectedHeight = getHeight() / 2;

    auto result = getImage(_url, getWidth() / 2, 0);
    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == expectedWidth);
    ASSERT_TRUE(image->height() == expectedHeight);
}

TEST_F(ImageCacheTests, resizeOnHeight) {
    int desiredHeight = getHeight() / 2;
    int expectedWidth = getWidth() / 2;
    int expectedHeight = desiredHeight;

    auto result = getImage(_url, 0, getHeight() / 2);
    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == expectedWidth);
    ASSERT_TRUE(image->height() == expectedHeight);
}

TEST_F(ImageCacheTests, variantsAreTrackerAsExpected) {
    auto result1 = getImage(_url, getWidth(), getHeight());
    ASSERT_EQ(_cache->getVariantCount(_url), 0);
    auto result2 = getImage(_url, getWidth() / 2, getHeight() / 2);
    ASSERT_EQ(_cache->getVariantCount(_url), 1);
    auto result3 = getImage(_url, 1, 1);
    ASSERT_EQ(_cache->getVariantCount(_url), 2);
}

TEST_F(ImageCacheTests, overwritingImageEvictsPastImageFromCache) {
    std::vector<Result<Valdi::Ref<Image>>> results = {
        getImage(_url, getWidth(), getHeight()),
        getImage(_url, getWidth() / 2, getHeight() / 2),
        getImage(_url, 1, 1),
    };
    auto img = Image::makeFromBitmap(createTestBitmap(), true).value();
    auto cancel = makeShared<SimpleAtomicCancelable>();
    _cache->setCachedItemAndGetResizedImage(_url, img, getWidth(), getHeight());

    for (auto& result : results) {
        auto img = result.moveValue();
        ASSERT_EQ(img.use_count(), 1);
    }
}

TEST_F(ImageCacheTests, returnedImagesAreRetainedInternally) {
    std::vector<Result<Valdi::Ref<Image>>> results = {
        getImage(_url, getWidth(), getHeight()),
        getImage(_url, getWidth() / 2, getHeight() / 2),
        getImage(_url, 1, 1),
    };

    for (auto& result : results) {
        auto img = result.moveValue();
        ASSERT_EQ(img.use_count(), 2);
    }
}

TEST_F(ImageCacheTests, testSizeIsTrackerCorrectlyOnAdd) {
    ASSERT_EQ(_cache->getCurrentSize(), _imgSize);
    auto result = getImage(_url, getWidth() / 2, getHeight() / 2);
    ASSERT_TRUE(result);
    auto img = result.moveValue();
    ASSERT_EQ(_cache->getCurrentSize(), _imgSize + ImageCacheItem::imageSize(img));
}

TEST_F(ImageCacheTests, testSizeGetsRemovesOldImageOnAddIfImageExistsInCache) {
    auto imageToKeep = getImage(_url, getWidth(), getHeight());
    std::vector<Result<Valdi::Ref<Image>>> results = {
        getImage(_url, getWidth() / 2, getHeight() / 2),
        getImage(_url, 1, 1),
    };

    ASSERT_EQ(_cache->getVariantCount(_url), 2);
    for (auto& result : results) {
        auto img = result.moveValue();
    }

    auto result = getImage(_url, getWidth() / 2, getHeight() / 2);
    ASSERT_TRUE(result);

    auto img = result.moveValue();
    ASSERT_EQ(_cache->getVariantCount(_url), 1);
    ASSERT_EQ(_cache->getCurrentSize(), _imgSize + ImageCacheItem::imageSize(img));
}

TEST_F(ImageCacheTests, testSizeGetsRemovesOldImageOnAddIfImageDoesntExistsInCache) {
    auto imageToKeep = getImage(_url, getWidth(), getHeight());
    std::vector<Result<Valdi::Ref<Image>>> results = {
        getImage(_url, getWidth() / 2, getHeight() / 2),
        getImage(_url, 1, 1),
    };

    ASSERT_EQ(_cache->getVariantCount(_url), 2);
    for (auto& result : results) {
        auto img = result.moveValue();
    }

    auto result = getImage(_url, getWidth() / 4, getHeight() / 4);
    ASSERT_EQ(_cache->getVariantCount(_url), 1);
    ASSERT_TRUE(result);

    auto img = result.moveValue();
    ASSERT_EQ(_cache->getCurrentSize(), _imgSize + ImageCacheItem::imageSize(img));
}

TEST_F(ImageCacheTests, testSizeGetsRemovesOldImageOnAddNewImageURL) {
    ASSERT_EQ(_cache->getCurrentSize(), _imgSize);
    auto url = STRING_LITERAL("asset://module/local-2");
    auto img = Image::makeFromBitmap(createTestBitmap(), true).value();
    auto retval = _cache->setCachedItemAndGetResizedImage(url, img, 0, 0);
    ASSERT_EQ(_cache->getCurrentSize(), ImageCacheItem::imageSize(img));

    auto result = getImage(_url, 0, 0);
    ASSERT_FALSE(result);
}

TEST_F(ImageCacheTests, overwritingImageEvictsHandledMultipleOriginalImagesGracefully) {
    auto result = getImage(_url, getWidth(), getHeight());

    auto img = Image::makeFromBitmap(createTestBitmap(), true).value();
    _cache->setCachedItemAndGetResizedImage(
        STRING_LITERAL("asset://module/local-other-parent"), img, getWidth(), getHeight());

    _cache->setCachedItemAndGetResizedImage(_url, img, getWidth(), getHeight());

    auto imgUnpacked = result.moveValue();
    ASSERT_EQ(imgUnpacked.use_count(), 1);
}

class ImageCacheEvictionFixture : public ImageCacheTestsBase,
                                  public ::testing::TestWithParam<ImageCache::EvictionPolicy> {
protected:
    ~ImageCacheEvictionFixture() override {};

    void SetUp() override {
        SetUpBase();
        _policy = GetParam();
    }

    void TearDown() override {
        TearDownBase();
    }

    ImageCache::EvictionPolicy _policy;
};

TEST_P(ImageCacheEvictionFixture, originalImageIsStoredAsLongAsVariantExist) {
    auto result1 = getImage(_url, getWidth() / 2, getHeight() / 2);
    _cache->invalidateCachedItems(_policy);
    auto result2 = getImage(_url, getWidth(), getHeight());
    ASSERT_FALSE(!result2);
}

TEST_P(ImageCacheEvictionFixture, originalIsErasedIfVariantIsntHeld) {
    auto result1 = getImage(_url, getWidth() / 2, getHeight() / 2).moveValue();
    ASSERT_FALSE(getImage(_url, getWidth(), getHeight()).failure());
    result1 = nullptr;
    _cache->invalidateCachedItems(_policy);
    ASSERT_TRUE(getImage(_url, getWidth(), getHeight()).failure());
}

TEST_P(ImageCacheEvictionFixture, incrementalDisappearanceOfVariantsDoesntImpactErasure) {
    std::vector<Result<Valdi::Ref<Image>>> results = {
        getImage(_url, getWidth(), getHeight()),
        getImage(_url, getWidth() / 2, getHeight() / 2),
        getImage(_url, 1, 1),
    };

    ASSERT_FALSE(getImage(_url, getWidth(), getHeight()).failure());
    for (auto& result : results) {
        auto img = result.moveValue();
        img = nullptr;
        _cache->invalidateCachedItems(_policy);
    }
    ASSERT_TRUE(getImage(_url, getWidth(), getHeight()).failure());
}

TEST_P(ImageCacheEvictionFixture, testSizeGetsCleanupAndTrackUnusedCorrectly) {
    std::vector<Result<Valdi::Ref<Image>>> results = {
        getImage(_url, getWidth(), getHeight()),
        getImage(_url, getWidth() / 2, getHeight() / 2),
        getImage(_url, 1, 1),
    };

    size_t totalSize = 0;
    for (auto& result : results) {
        auto img = result.moveValue();
        totalSize += ImageCacheItem::imageSize(img);
    }
    ASSERT_EQ(_cache->getCurrentSize(), totalSize);
    _cache->invalidateCachedItems(_policy);
    ASSERT_EQ(_cache->getCurrentSize(), static_cast<size_t>(0));
}

INSTANTIATE_TEST_SUITE_P(ImageCacheEvictionTests,
                         ImageCacheEvictionFixture,
                         ::testing::Values(ImageCache::EvictionPolicy::Time,
                                           ImageCache::EvictionPolicy::Memory,
                                           ImageCache::EvictionPolicy::Both));

} // namespace ValdiTest
