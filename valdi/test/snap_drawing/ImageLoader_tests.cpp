#include "BlockingAssetCompletionHandler.hpp"
#include "MockAssetLoader.hpp"
#include "MockDownloader.hpp"
#include "TestAsyncUtils.hpp"
#include "TestBitmap.hpp"
#include "TestDataUtils.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoader.hpp"
#include "valdi/snap_drawing/ImageLoading/ImageLoaderFactory.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_test_utils.hpp"

#include <gtest/gtest.h>

using namespace Valdi;
using namespace snap::drawing;

namespace ValdiTest {

class ImageLoaderTests : public ::testing::Test {
protected:
    void SetUp() override {
        _queue = DispatchQueue::create(STRING_LITERAL("com.snap.valdi.ImageLoader"), ThreadQoSClassNormal);

        auto cachesInMemoryDiskCache = makeShared<InMemoryDiskCache>();
        _downloader = makeShared<MockDownloader>(_queue);
        _imageLoader = createImageLoader(_queue, ConsoleLogger::getLogger(), 0);
        _assetLoader = _imageLoader->createAssetLoader({}, _downloader);

        auto img = Image::makeFromBitmap(createTestBitmap(), true).value();
        _imageData = img->toPNG().value();
        _downloader->setDataResponse(_url, _imageData);

        auto zeroBitmap = loadResourceFromDisk("testdata/0x0.bmp");
        ASSERT_FALSE(zeroBitmap.failure())
            << "Failed to load testdata/0x0.bmp (ensure the file is in test data/runfiles)";
        _downloader->setDataResponse(_zeroDimensionErrorUrl, zeroBitmap.value());

        _downloader->setDataResponse(_dataErrorUrl, BytesView());
        _downloader->setErrorResponse(_downloadErrorUrl, Error(_errMsg));

        _width = img->width();
        _height = img->height();

        _filter = makeShared<ImageFilter>();
    }

    Result<Valdi::Ref<Image>> loadImage(const StringBox& url,
                                        int32_t width,
                                        int32_t height,
                                        const Valdi::Value filter = Valdi::Value()) {
        auto payload = _assetLoader->requestPayloadFromURL(url);

        auto completionHandler = makeShared<BlockingAssetCompletionHandler>().toShared();

        _assetLoader->loadAsset(payload.value(), width, height, filter, completionHandler);

        auto retvalResult = completionHandler->getResult();
        if (!retvalResult) {
            return retvalResult.error();
        } else {
            auto retval = retvalResult.moveValue();
            return castOrNull<Image>(retval);
        }
    }

    void TearDown() override {
        _queue->fullTeardown();
    }

    int getWidth() {
        return _width;
    }

    int getHeight() {
        return _height;
    }

    Valdi::Ref<MockDownloader> _downloader;
    Valdi::Ref<ImageLoader> _imageLoader;
    Valdi::Ref<AssetLoader> _assetLoader;
    StringBox _zeroDimensionErrorUrl = STRING_LITERAL("asset://module/local-zero-dimension-error");
    StringBox _downloadErrorUrl = STRING_LITERAL("asset://module/local-download-error");
    StringBox _dataErrorUrl = STRING_LITERAL("asset://module/local-data-error");
    StringBox _url = STRING_LITERAL("asset://module/local");
    StringBox _errMsg = STRING_LITERAL("Error URL Result");
    BytesView _imageData;
    Valdi::Ref<ImageFilter> _filter;

private:
    int _width;
    int _height;

    Valdi::Ref<DispatchQueue> _queue;
};

TEST_F(ImageLoaderTests, returnsErrorIfDownloaderFails) {
    auto result = loadImage(_downloadErrorUrl, getWidth(), getHeight());
    ASSERT_TRUE(result.failure());
    ASSERT_TRUE(result.error().toString() == _errMsg.toStringView());
}

TEST_F(ImageLoaderTests, returnsErrorIfDataCannotBeConvertedToImage) {
    auto result = loadImage(_dataErrorUrl, getWidth(), getHeight());
    ASSERT_TRUE(result.failure());
    ASSERT_TRUE(result.error().toString().length() != 0);
}

// NOTE(rjaber): Zero Dimension bitmaps are possible, but not currently supported by snap drawing
//               They seems to fail in the Image::make(data) call. Leaving here in case
//               this changes in the future
TEST_F(ImageLoaderTests, returnsForZeroDimensionBitmap) {
    auto result = loadImage(_zeroDimensionErrorUrl, getWidth(), getHeight());
    ASSERT_TRUE(result.failure());
    ASSERT_TRUE(result.error().toString().length() != 0);
}

TEST_F(ImageLoaderTests, resizeOnWidth) {
    int desiredWidth = getWidth() / 2;
    int expectedWidth = desiredWidth;
    int expectedHeight = getHeight() / 2;

    auto result = loadImage(_url, getWidth() / 2, 0);
    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == expectedWidth);
    ASSERT_TRUE(image->height() == expectedHeight);
}

TEST_F(ImageLoaderTests, resizeOnHeight) {
    int desiredHeight = getHeight() / 2;
    int expectedWidth = getWidth() / 2;
    int expectedHeight = desiredHeight;

    auto result = loadImage(_url, 0, getHeight() / 2);
    ASSERT_FALSE(result.failure());
    auto image = result.moveValue();

    ASSERT_TRUE(image->width() == expectedWidth);
    ASSERT_TRUE(image->height() == expectedHeight);
}

TEST_F(ImageLoaderTests, doubleLoadingOriginalSizeAvoidsDownload) {
    auto result1 = loadImage(_url, getWidth(), getHeight());
    ASSERT_EQ(_downloader->getDownloadRequests(), 1ull);
    auto result2 = loadImage(_url, getWidth(), getHeight());
    ASSERT_EQ(_downloader->getDownloadRequests(), 1ull);
}

TEST_F(ImageLoaderTests, loadingResizedAfterOriginalAvoidsDownload) {
    auto result1 = loadImage(_url, getWidth(), getHeight());
    ASSERT_EQ(_downloader->getDownloadRequests(), 1ull);
    auto result2 = loadImage(_url, getWidth() / 2, getHeight() / 2);
    ASSERT_EQ(_downloader->getDownloadRequests(), 1ull);
}

TEST_F(ImageLoaderTests, loadingDoubleResizedAvoidsDownload) {
    int width = getWidth() / 2;
    int height = getHeight() / 2;

    auto result1 = loadImage(_url, width, height);
    ASSERT_EQ(_downloader->getDownloadRequests(), 1ull);
    auto result2 = loadImage(_url, width, height);
    ASSERT_EQ(_downloader->getDownloadRequests(), 1ull);
}

TEST_F(ImageLoaderTests, getFilterInImageIsTheSameWithNoResize) {
    auto result = loadImage(_url, getWidth(), getHeight(), Valdi::Value(_filter));
    ASSERT_TRUE(result);
    auto img = result.moveValue();
    ASSERT_EQ(img->getFilter().get(), _filter.get());
}

TEST_F(ImageLoaderTests, filterIsCopiesAndBlurRadiusIsScaledOnResize) {
    const float blurRadius = 2;
    const float expectedBlurRadius = 1;
    const float matrix[ImageFilter::kColorMatrixSize] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                                                         10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

    _filter->setBlurRadius(blurRadius);
    _filter->concatColorMatrix(matrix);

    auto result = loadImage(_url, getWidth() / 2, getHeight() / 2, Valdi::Value(_filter));
    ASSERT_TRUE(result);
    auto img = result.moveValue();
    ASSERT_NE(img->getFilter().get(), _filter.get());
    ASSERT_EQ(img->getFilter()->getBlurRadius(), expectedBlurRadius);

    auto returnMatrix = img->getFilter()->getColorMatrix();
    auto originalMatrix = _filter->getColorMatrix();
    ASSERT_NE(returnMatrix, originalMatrix);
    for (int i = 0; i < static_cast<int>(ImageFilter::kColorMatrixSize); i++) {
        ASSERT_EQ(returnMatrix[i], originalMatrix[i]);
    }
}

TEST_F(ImageLoaderTests, getReturnsANewImageWithoutFilter) {
    auto result = loadImage(_url, getWidth(), getHeight(), Valdi::Value(_filter));
    ASSERT_TRUE(result);
    auto result1 = loadImage(_url, getWidth(), getHeight());
    ASSERT_TRUE(result1);
    auto img = result1.moveValue();
    ASSERT_EQ(img->getFilter(), nullptr);
}

} // namespace ValdiTest
