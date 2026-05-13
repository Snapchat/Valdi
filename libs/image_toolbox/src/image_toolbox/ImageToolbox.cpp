#include "ImageToolbox.hpp"
#include "SVGRenderer.hpp"
#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"
#include "snap_drawing/cpp/Drawing/DrawingContext.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/Surface/DrawableSurfaceCanvas.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "snap_drawing/cpp/Utils/Path.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueUtils.hpp"
#include <unistd.h>

namespace snap::imagetoolbox {

using namespace snap::drawing;

template<typename R>
using ProcessImageFn = Valdi::Function<Valdi::Result<R>(Valdi::BytesView)>;

template<typename R>
static Valdi::Result<R> processImage(const Valdi::StringBox& imageFilePath,
                                     ProcessImageFn<R>&& defaultHandler,
                                     ProcessImageFn<R>&& svgHandler) {
    Valdi::Path path(imageFilePath.toStringView());
    auto loadResult = Valdi::DiskUtils::load(path);
    if (!loadResult) {
        return loadResult.moveError();
    }

    auto fileBytes = loadResult.moveValue();

    if (path.getFileExtension() == "svg") {
        return svgHandler(fileBytes);
    } else {
        return defaultHandler(fileBytes);
    }
}

static Valdi::Result<Ref<Image>> loadImage(const Valdi::StringBox& imageFilePath,
                                           const std::optional<int>& widthHint,
                                           const std::optional<int>& heightHint) {
    return processImage<Valdi::Ref<snap::drawing::Image>>(
        imageFilePath,
        [&](auto bytes) { return Image::make(bytes); },
        [&](auto bytes) {
            auto outputWidth = widthHint ? widthHint.value() : 0;
            auto outputHeight = heightHint ? heightHint.value() : 0;

            return SVGRenderer::render(bytes.data(), bytes.size(), outputWidth, outputHeight);
        });
}

static Valdi::Result<Ref<Image>> clipImageToCircle(const Ref<Image>& image) {
    auto width = image->width();
    auto height = image->height();

    DrawingContext drawingContext(width, height);

    Path clipPath;
    clipPath.addOval(Rect::makeXYWH(0, 0, width, height), true);
    drawingContext.clipPath(clipPath);

    auto imageRect = Rect::makeXYWH(0, 0, image->width(), image->height());
    auto targetRect = Rect::makeXYWH(0, 0, width, height);
    drawingContext.drawImage(*image, imageRect, targetRect, nullptr);

    auto displayList = Valdi::makeShared<DisplayList>(Size(width, height), TimePoint(0.0));
    displayList->appendLayerContent(drawingContext.finish(), 1.0);

    BitmapGraphicsContext bitmapContext;
    auto surface = bitmapContext.createBitmapSurface(
        Valdi::BitmapInfo(width, height, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypePremul, 0));

    auto canvas = surface->prepareCanvas();
    if (!canvas) {
        return canvas.moveError();
    }

    displayList->draw(canvas.value(), kDisplayListAllPlaneIndexes, true);

    auto snapshot = canvas.value().snapshot();
    if (!snapshot) {
        return snapshot.moveError();
    }

    return snapshot.moveValue();
}

Valdi::Result<ImageInfo> getImageInfo(const Valdi::StringBox& imageFilePath) {
    auto imageSize = processImage<std::pair<int, int>>(
        imageFilePath,
        [&](auto bytes) -> Valdi::Result<std::pair<int, int>> {
            auto image = Image::make(bytes);
            if (!image) {
                return image.moveError();
            }

            return std::make_pair(image.value()->width(), image.value()->height());
        },
        [&](auto bytes) { return SVGRenderer::getSize(bytes.data(), bytes.size()); });

    if (!imageSize) {
        return imageSize.moveError();
    }

    ImageInfo info;
    info.width = static_cast<uint32_t>(imageSize.value().first);
    info.height = static_cast<uint32_t>(imageSize.value().second);

    return info;
}

Valdi::Result<Valdi::BytesView> outputImageInfo(const Valdi::StringBox& imageFilePath) {
    auto imageInfo = getImageInfo(imageFilePath);
    if (!imageInfo) {
        return imageInfo.moveError();
    }

    Valdi::Value json;
    json.setMapValue("width", Valdi::Value(static_cast<int32_t>(imageInfo.value().width)));
    json.setMapValue("height", Valdi::Value(static_cast<int32_t>(imageInfo.value().height)));

    return Valdi::valueToJson(json)->toBytesView();
}

Valdi::Result<Valdi::BytesView> convertImage(const Valdi::StringBox& inputImageFilePath,
                                             const Valdi::StringBox& outputImageFilePath,
                                             const std::optional<int>& outputWidth,
                                             const std::optional<int>& outputHeight,
                                             double qualityRatio,
                                             bool roundOutput) {
    Valdi::Path outputPath(outputImageFilePath.toStringView());

    snap::drawing::EncodedImageFormat outputFormat;

    auto outputFileExtension = outputPath.getFileExtension();

    if (outputFileExtension == "png") {
        outputFormat = snap::drawing::EncodedImageFormatPNG;
    } else if (outputFileExtension == "webp") {
        outputFormat = snap::drawing::EncodedImageFormatWebP;
    } else if (outputFileExtension == "jpg" || outputFileExtension == "jpeg") {
        outputFormat = snap::drawing::EncodedImageFormatJPG;
    } else {
        return Valdi::Error(STRING_FORMAT(
            "Unsupported file extension '{}' for output file '{}'", outputFileExtension, outputImageFilePath));
    }

    auto imageResult = loadImage(inputImageFilePath, outputWidth, outputHeight);
    if (!imageResult) {
        return imageResult.moveError();
    }
    auto image = imageResult.moveValue();

    int newWidth = image->width();
    int newHeight = image->height();
    auto ratio = static_cast<double>(newWidth) / static_cast<double>(newHeight);

    if (outputWidth && outputHeight) {
        newWidth = outputWidth.value();
        newHeight = outputHeight.value();
    } else if (outputWidth) {
        newWidth = outputWidth.value();
        newHeight = static_cast<int>(round(newWidth / ratio));
    } else if (outputHeight) {
        newHeight = outputHeight.value();
        newWidth = static_cast<int>(round(newHeight * ratio));
    }

    if (newWidth != image->width() || newHeight != image->height()) {
        image = image->resized(newWidth, newHeight);
    }

    if (roundOutput) {
        if (newWidth != newHeight) {
            return Valdi::Error("Round image output requires equal width and height");
        }

        auto clippedImage = clipImageToCircle(image);
        if (!clippedImage) {
            return clippedImage.moveError();
        }

        image = clippedImage.moveValue();
    }

    auto encodeResult = image->encode(outputFormat, qualityRatio);
    if (!encodeResult) {
        return encodeResult.moveError();
    }

    auto storeResult = Valdi::DiskUtils::store(outputPath, encodeResult.value());
    if (!storeResult) {
        return storeResult.moveError();
    }

    return Valdi::BytesView();
}

} // namespace snap::imagetoolbox
