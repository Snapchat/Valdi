#include "valdi_modules/integration_test_cli/integration_test_cli.hpp"

#include "snap_drawing/cpp/Utils/Image.hpp"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Utils/BitmapWithBuffer.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace snap::valdi_modules::integration_test_cli {
namespace {

constexpr uint8_t kDiffRed = 255;
constexpr uint8_t kDiffGreen = 0;
constexpr uint8_t kDiffBlue = 0;
constexpr uint8_t kDiffAlpha = 255;

class LockedBitmapPixels {
public:
    LockedBitmapPixels(Valdi::Ref<Valdi::IBitmap> bitmap, const char* name) : _bitmap(std::move(bitmap)), _name(name) {
        if (_bitmap == nullptr) {
            throw Valdi::Exception(STRING_FORMAT("{} bitmap is null", _name));
        }

        info = _bitmap->getInfo();
        if (info.width < 0 || info.height < 0) {
            throw Valdi::Exception(
                STRING_FORMAT("{} bitmap has invalid dimensions {}x{}", _name, info.width, info.height));
        }
        if (info.colorType != Valdi::ColorTypeRGBA8888) {
            throw Valdi::Exception(STRING_FORMAT(
                "{} bitmap must be RGBA8888, got color type {}", _name, static_cast<int>(info.colorType)));
        }
        if (info.rowBytes < static_cast<size_t>(info.width) * 4) {
            throw Valdi::Exception(
                STRING_FORMAT("{} bitmap rowBytes {} is smaller than width * 4", _name, info.rowBytes));
        }

        bytes = static_cast<const uint8_t*>(_bitmap->lockBytes());
        if (bytes == nullptr && info.width > 0 && info.height > 0) {
            throw Valdi::Exception(STRING_FORMAT("Failed to lock {} bitmap pixels", _name));
        }
    }

    ~LockedBitmapPixels() {
        if (_bitmap != nullptr && bytes != nullptr) {
            _bitmap->unlockBytes();
        }
    }

    LockedBitmapPixels(const LockedBitmapPixels&) = delete;
    LockedBitmapPixels& operator=(const LockedBitmapPixels&) = delete;

    bool contains(int x, int y) const {
        return x >= 0 && y >= 0 && x < info.width && y < info.height;
    }

    const uint8_t* pixel(int x, int y) const {
        if (!contains(x, y)) {
            return nullptr;
        }
        return bytes + static_cast<size_t>(y) * info.rowBytes + static_cast<size_t>(x) * 4;
    }

    Valdi::BitmapInfo info = Valdi::BitmapInfo(0, 0, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypeUnpremul, 0);
    const uint8_t* bytes = nullptr;

private:
    Valdi::Ref<Valdi::IBitmap> _bitmap;
    const char* _name;
};

uint8_t fadedChannel(uint8_t channel) {
    return static_cast<uint8_t>((static_cast<int>(channel) * 55 + 255 * 45 + 50) / 100);
}

bool pixelsEqual(const uint8_t* beforePixel, const uint8_t* afterPixel, int threshold) {
    for (int channel = 0; channel < 4; channel++) {
        const int beforeValue = beforePixel == nullptr ? 0 : beforePixel[channel];
        const int afterValue = afterPixel == nullptr ? 0 : afterPixel[channel];
        const int delta = beforeValue - afterValue;
        if (delta > threshold || delta < -threshold) {
            return false;
        }
    }
    return true;
}

void writeDiffPixel(uint8_t* outPixel) {
    outPixel[0] = kDiffRed;
    outPixel[1] = kDiffGreen;
    outPixel[2] = kDiffBlue;
    outPixel[3] = kDiffAlpha;
}

void writeFadedPixel(uint8_t* outPixel, const uint8_t* afterPixel) {
    const uint8_t red = afterPixel == nullptr ? 0 : afterPixel[0];
    const uint8_t green = afterPixel == nullptr ? 0 : afterPixel[1];
    const uint8_t blue = afterPixel == nullptr ? 0 : afterPixel[2];
    outPixel[0] = fadedChannel(red);
    outPixel[1] = fadedChannel(green);
    outPixel[2] = fadedChannel(blue);
    outPixel[3] = afterPixel == nullptr ? 0 : afterPixel[3];
}

Valdi::Ref<snap::drawing::Image> imageFromBytes(Valdi::BytesView data, const char* name) {
    auto imageResult = snap::drawing::Image::make(data);
    if (!imageResult) {
        throw Valdi::Exception(imageResult.moveError());
    }
    auto image = imageResult.value();
    if (image == nullptr) {
        throw Valdi::Exception(STRING_FORMAT("Failed to decode {} image", name));
    }
    return image;
}

Valdi::Ref<Valdi::IBitmap> convertImageForComparison(const Valdi::Ref<snap::drawing::Image>& image,
                                                     int width,
                                                     int height,
                                                     const char* name) {
    auto bitmap = image->toConvertedBitmap(Valdi::BitmapInfo(
        width, height, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypeUnpremul, static_cast<size_t>(width) * 4));
    if (bitmap == nullptr) {
        throw Valdi::Exception(STRING_FORMAT("Failed to convert {} image for comparison", name));
    }
    return bitmap;
}

void diffSameSize(const LockedBitmapPixels& before,
                  const LockedBitmapPixels& after,
                  int width,
                  int height,
                  int threshold,
                  uint8_t* outBytes,
                  size_t outRowBytes,
                  double& changedPixels) {
    for (int y = 0; y < height; y++) {
        const uint8_t* beforeRow = before.bytes + static_cast<size_t>(y) * before.info.rowBytes;
        const uint8_t* afterRow = after.bytes + static_cast<size_t>(y) * after.info.rowBytes;
        uint8_t* outRow = outBytes + static_cast<size_t>(y) * outRowBytes;
        for (int x = 0; x < width; x++) {
            const uint8_t* beforePixel = beforeRow + static_cast<size_t>(x) * 4;
            const uint8_t* afterPixel = afterRow + static_cast<size_t>(x) * 4;
            uint8_t* outPixel = outRow + static_cast<size_t>(x) * 4;
            if (!pixelsEqual(beforePixel, afterPixel, threshold)) {
                changedPixels++;
                writeDiffPixel(outPixel);
            } else {
                writeFadedPixel(outPixel, afterPixel);
            }
        }
    }
}

void diffWithBounds(const LockedBitmapPixels& before,
                    const LockedBitmapPixels& after,
                    int width,
                    int height,
                    int threshold,
                    uint8_t* outBytes,
                    size_t outRowBytes,
                    double& changedPixels) {
    for (int y = 0; y < height; y++) {
        uint8_t* outRow = outBytes + static_cast<size_t>(y) * outRowBytes;
        for (int x = 0; x < width; x++) {
            const uint8_t* beforePixel = before.pixel(x, y);
            const uint8_t* afterPixel = after.pixel(x, y);
            uint8_t* outPixel = outRow + static_cast<size_t>(x) * 4;
            if (!pixelsEqual(beforePixel, afterPixel, threshold)) {
                changedPixels++;
                writeDiffPixel(outPixel);
            } else {
                writeFadedPixel(outPixel, afterPixel);
            }
        }
    }
}

NativeImageDiffResult diffComparableBitmaps(const Valdi::Ref<Valdi::IBitmap>& beforeBitmap,
                                            const Valdi::Ref<Valdi::IBitmap>& afterBitmap,
                                            bool dimensionMismatch,
                                            double pixelThreshold) {
    const LockedBitmapPixels before(beforeBitmap, "before");
    const LockedBitmapPixels after(afterBitmap, "after");
    const int width = std::max(before.info.width, after.info.width);
    const int height = std::max(before.info.height, after.info.height);
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    const size_t byteLength = static_cast<size_t>(height) * rowBytes;

    auto outBytes = Valdi::makeShared<Valdi::Bytes>();
    outBytes->resize(byteLength);
    uint8_t* out = outBytes->data();

    double changedPixels = 0;
    const int threshold = static_cast<int>(pixelThreshold);
    if (before.info.width == width && after.info.width == width && before.info.height == height &&
        after.info.height == height) {
        diffSameSize(before, after, width, height, threshold, out, rowBytes, changedPixels);
    } else {
        diffWithBounds(before, after, width, height, threshold, out, rowBytes, changedPixels);
    }

    NativeImageDiffResult result;
    result.setChangedPixels(changedPixels);
    result.setTotalPixels(static_cast<double>(width) * height);
    result.setDimensionMismatch(dimensionMismatch);
    result.setImage(Valdi::Value(Valdi::makeShared<Valdi::BitmapWithBuffer>(
        Valdi::BytesView(outBytes),
        Valdi::BitmapInfo(width, height, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypeUnpremul, rowBytes))));
    return result;
}

bool hasSameAspectRatio(const Valdi::Ref<snap::drawing::Image>& beforeImage,
                        const Valdi::Ref<snap::drawing::Image>& afterImage) {
    return static_cast<int64_t>(beforeImage->width()) * afterImage->height() ==
        static_cast<int64_t>(afterImage->width()) * beforeImage->height();
}

} // namespace

class ImageDiffNativeModuleImpl : public ImageDiffNativeModule {
public:
    NativeImageDiffResult diffEncodedImages(Valdi::BytesView beforeData,
                                            Valdi::BytesView afterData,
                                            double pixelThreshold) {
        auto beforeImage = imageFromBytes(beforeData, "before");
        auto afterImage = imageFromBytes(afterData, "after");
        const bool dimensionMismatch =
            beforeImage->width() != afterImage->width() || beforeImage->height() != afterImage->height();
        int beforeWidth = beforeImage->width();
        int beforeHeight = beforeImage->height();
        int afterWidth = afterImage->width();
        int afterHeight = afterImage->height();
        if (dimensionMismatch && hasSameAspectRatio(beforeImage, afterImage)) {
            beforeWidth = std::min(beforeImage->width(), afterImage->width());
            beforeHeight = std::min(beforeImage->height(), afterImage->height());
            afterWidth = beforeWidth;
            afterHeight = beforeHeight;
        }

        auto beforeBitmap = convertImageForComparison(beforeImage, beforeWidth, beforeHeight, "before");
        auto afterBitmap = convertImageForComparison(afterImage, afterWidth, afterHeight, "after");
        return diffComparableBitmaps(beforeBitmap, afterBitmap, dimensionMismatch, pixelThreshold);
    }
};

class ImageDiffNativeModuleFactoryImpl : public ImageDiffNativeModuleFactory {
public:
    Valdi::Ref<ImageDiffNativeModule> onLoadModule() final {
        return Valdi::makeShared<ImageDiffNativeModuleImpl>();
    }
};

auto registerImageDiffNativeModule = Valdi::RegisterModuleFactory::registerTyped<ImageDiffNativeModuleFactoryImpl>();

} // namespace snap::valdi_modules::integration_test_cli
