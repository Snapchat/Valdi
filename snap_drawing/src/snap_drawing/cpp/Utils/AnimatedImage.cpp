#include "snap_drawing/cpp/Utils/AnimatedImage.hpp"
#include "include/codec/SkCodec.h"
#include "snap_drawing/cpp/Drawing/DrawingContext.hpp"
#include "snap_drawing/cpp/Drawing/Surface/DrawableSurfaceCanvas.hpp"
#include "snap_drawing/cpp/Utils/BytesUtils.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "snap_drawing/cpp/Utils/LottieAnimatedImage.hpp"
#include "snap_drawing/cpp/Utils/SVGAnimatedImage.hpp"
#include "snap_drawing/cpp/Utils/SkCodecAnimatedImage.hpp"
#include "valdi_core/cpp/Utils/JSONReader.hpp"

namespace snap::drawing {

AnimatedImage::AnimatedImage() = default;
AnimatedImage::~AnimatedImage() = default;

void AnimatedImage::draw(SkCanvas* canvas,
                         const Rect& drawBounds,
                         const Duration& time,
                         FittingSizeMode fittingSizeMode) {
    doDraw(canvas, drawBounds, time, fittingSizeMode);
}

void AnimatedImage::drawInCanvas(const DrawableSurfaceCanvas& canvas,
                                 const Rect& drawBounds,
                                 const Duration& time,
                                 FittingSizeMode fittingSizeMode) {
    doDraw(canvas.getSkiaCanvas(), drawBounds, time, fittingSizeMode);
}

Valdi::Result<Ref<AnimatedImage>> AnimatedImage::make(const Ref<IFontManager>& fontManager,
                                                      const Valdi::Byte* data,
                                                      size_t length) {
    Image::initializeCodecs();

    if constexpr (kLottieEnabled) {
        if (isJsonObject(data, length)) {
            return LottieAnimatedImage::make(fontManager, data, length).map<Ref<AnimatedImage>>();
        }
    }

    const Valdi::BytesView bytesView(nullptr, data, length);
    if (Image::isSVG(bytesView)) {
        return SVGAnimatedImage::make(data, length).map<Ref<AnimatedImage>>();
    }

    auto skData = skDataFromBytes(bytesView, DataConversionModeAlwaysCopy);
    auto codec = SkCodec::MakeFromData(skData);
    if (codec == nullptr) {
        return Valdi::Error("Unsupported image format");
    }
    return SkCodecAnimatedImage::make(std::move(codec)).map<Ref<AnimatedImage>>();
}

bool AnimatedImage::isJsonObject(const Valdi::Byte* data, size_t length) {
    // Fuzzy but efficient check for JSON, and if so assume Lottie
    Valdi::JSONReader reader(std::string_view(reinterpret_cast<const char*>(data), length));
    return reader.parseBeginObject();
}

} // namespace snap::drawing
