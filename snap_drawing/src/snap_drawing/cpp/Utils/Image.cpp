//
//  Image.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/7/20.
//

#include "snap_drawing/cpp/Utils/Image.hpp"
#include "snap_drawing/cpp/Utils/BytesUtils.hpp"

#include "snap_drawing/cpp/Utils/Bitmap.hpp"

#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include "snap_drawing/cpp/Utils/BitmapUtils.hpp"

#include "include/codec/SkEncodedImageFormat.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkStream.h"
#include "include/encode/SkJpegEncoder.h"
#include "include/encode/SkPngEncoder.h"
#include "include/encode/SkWebpEncoder.h"
#include "src/image/SkImage_Base.h"

namespace snap::drawing {

Image::Image(const sk_sp<SkImage>& skImage) : _skImage(skImage) {}

Image::~Image() = default;

void Image::initializeCodecs() {
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        SkCodecs::Register(SkJpegDecoder::Decoder());
        SkCodecs::Register(SkPngDecoder::Decoder());
        SkCodecs::Register(SkWebpDecoder::Decoder());
    });
}

Valdi::Value Image::getMetadata() const {
    return Valdi::Value()
        .setMapValue("type", Valdi::Value(Valdi::StringBox::fromCString("image")))
        .setMapValue("width", Valdi::Value(static_cast<int32_t>(_skImage->width())))
        .setMapValue("height", Valdi::Value(static_cast<int32_t>(_skImage->height())));
}

const sk_sp<SkImage>& Image::getSkValue() const {
    return _skImage;
}

int Image::width() const {
    return _skImage->width();
}

int Image::height() const {
    return _skImage->height();
}

Valdi::Result<Valdi::BytesView> Image::toPNG() const {
    return encode(EncodedImageFormatPNG, 1.0);
}

static bool encodeImage(SkWStream* dst, const SkPixmap& src, EncodedImageFormat format, int quality) {
    switch (format) {
        case EncodedImageFormatJPG: {
            SkJpegEncoder::Options opts;
            opts.fQuality = quality;
            return SkJpegEncoder::Encode(dst, src, opts);
        }
        case EncodedImageFormatPNG: {
            SkPngEncoder::Options opts;
            return SkPngEncoder::Encode(dst, src, opts);
        }
        case EncodedImageFormatWebP: {
            SkWebpEncoder::Options opts;
            if (quality == 100) {
                opts.fCompression = SkWebpEncoder::Compression::kLossless;
                // Note: SkEncodeImage treats 0 quality as the lowest quality
                // (greatest compression) and 100 as the highest quality (least
                // compression). For kLossy, this matches libwebp's
                // interpretation, so it is passed directly to libwebp. But
                // with kLossless, libwebp always creates the highest quality
                // image. In this case, fQuality is reinterpreted as how much
                // effort (time) to put into making a smaller file. This API
                // does not provide a way to specify this value (though it can
                // be specified by using SkWebpEncoder::Encode) so we have to
                // pick one arbitrarily. This value matches that chosen by
                // blink::ImageEncoder::ComputeWebpOptions as well
                // WebPConfigInit.
                opts.fQuality = 75;
            } else {
                opts.fCompression = SkWebpEncoder::Compression::kLossy;
                opts.fQuality = quality;
            }
            return SkWebpEncoder::Encode(dst, src, opts);
        }
    }
}

sk_sp<SkData> Image::encodeSKImageToSKData(SkImage& image, EncodedImageFormat format, int quality) {
    Image::initializeCodecs();
    SkBitmap bm;
    if (!as_IB(&image)->getROPixels(nullptr, &bm)) {
        return nullptr;
    }

    SkPixmap pixmap;
    if (!bm.peekPixels(&pixmap)) {
        return nullptr;
    }

    SkDynamicMemoryWStream stream;
    if (!encodeImage(&stream, pixmap, format, quality)) {
        return nullptr;
    }
    return stream.detachAsData();
}

Valdi::Result<Valdi::BytesView> Image::encode(EncodedImageFormat format, double qualityRatio) const {
    auto quality = static_cast<int>(round(100.0 * qualityRatio));

    auto encoded = Image::encodeSKImageToSKData(*_skImage, format, quality);
    if (encoded == nullptr) {
        return Valdi::Error("Cannot encode image");
    }

    return bytesFromSkData(encoded);
}

Ref<Image> Image::resized(int width, int height) const {
    SkBitmap bitmap;

    SkImageInfo imageInfo = SkImageInfo::Make(width, height, _skImage->colorType(), _skImage->alphaType());
    bitmap.allocPixels(imageInfo, imageInfo.minRowBytes());

    _skImage->scalePixels(bitmap.pixmap(), SkSamplingOptions(SkCubicResampler::Mitchell()));

    bitmap.setImmutable();

    auto skImage = SkImages::RasterFromBitmap(bitmap);
    return Ref<Image>(Valdi::makeShared<Image>(skImage));
}

void Image::draw(const Valdi::BitmapInfo& bitmapInfo, void* bytes) {
    SkPixmap pixmap(toSkiaImageInfo(bitmapInfo), bytes, bitmapInfo.rowBytes);
    _skImage->scalePixels(pixmap, SkSamplingOptions(SkCubicResampler::Mitchell()));
}

Ref<Image> Image::withFilter(const Ref<Valdi::ImageFilter>& filter) {
    auto copiedImage = Valdi::makeShared<Image>(_skImage);
    copiedImage->_filter = filter;
    copiedImage->_sourceImage = Valdi::strongSmallRef(this);
    return copiedImage;
}

const Ref<Valdi::ImageFilter>& Image::getFilter() const {
    return _filter;
}

Valdi::Result<Ref<Image>> Image::make(const Valdi::BytesView& data) {
    Image::initializeCodecs();
    auto skData = skDataFromBytes(data, DataConversionModeNeverCopy);

    auto skImage = SkImages::DeferredFromEncodedData(skData);

    if (skImage == nullptr) {
        return Valdi::Error("Unable to decode image");
    }

    return Ref<Image>(Valdi::makeShared<Image>(skImage));
}

Valdi::Result<Ref<Image>> Image::makeFromPixelsData(const Valdi::BitmapInfo& bitmapInfo,
                                                    const Valdi::BytesView& pixelsData,
                                                    bool shouldCopy) {
    return makeFromPixelsData(
        bitmapInfo,
        skDataFromBytes(pixelsData, shouldCopy ? DataConversionModeAlwaysCopy : DataConversionModeNeverCopy));
}

Valdi::Result<Ref<Image>> Image::makeFromPixelsData(const Valdi::BitmapInfo& bitmapInfo,
                                                    const sk_sp<SkData>& pixelsData) {
    auto imageInfo = snap::drawing::toSkiaImageInfo(bitmapInfo);

    auto skImage = SkImages::RasterFromData(imageInfo, pixelsData, bitmapInfo.rowBytes);

    if (skImage == nullptr) {
        return Valdi::Error("Unable to create image");
    }

    return Ref<Image>(Valdi::makeShared<Image>(skImage));
}

Valdi::Result<Ref<Image>> Image::makeFromBitmap(const Valdi::Ref<Valdi::IBitmap>& bitmap, bool shouldCopy) {
    auto info = bitmap->getInfo();

    auto data = bitmapToData(bitmap, info, shouldCopy);
    if (!data) {
        return data.moveError();
    }

    return makeFromPixelsData(info, data.value());
}

Valdi::Ref<Valdi::IBitmap> Image::getBitmap() {
    auto imageInfo = _skImage->imageInfo();
    if (imageInfo.colorType() == kUnknown_SkColorType) {
        return nullptr;
    }

    SkBitmap bitmap;
    bitmap.allocPixels(_skImage->imageInfo(), imageInfo.minRowBytes());
    auto pixmap = bitmap.pixmap();
    _skImage->readPixels(imageInfo, pixmap.writable_addr(), imageInfo.minRowBytes(), 0, 0);

    return Valdi::makeShared<snap::drawing::Bitmap>(std::move(bitmap));
}

VALDI_CLASS_IMPL(Image)

} // namespace snap::drawing
