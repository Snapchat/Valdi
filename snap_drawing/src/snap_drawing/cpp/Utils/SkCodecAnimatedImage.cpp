//
//  SkCodecAnimatedImage.cpp
//  snap_drawing
//
//  Created by Ben Dodson on 06/19/24.
//

#include "snap_drawing/cpp/Utils/SkCodecAnimatedImage.hpp"
#include "include/core/SkCanvas.h"
#include "snap_drawing/cpp/Drawing/DrawingContext.hpp"
#include "snap_drawing/cpp/Drawing/Surface/DrawableSurfaceCanvas.hpp"

namespace snap::drawing {

SkCodecAnimatedImage::~SkCodecAnimatedImage() = default;

const Duration& SkCodecAnimatedImage::getDuration() const {
    return _duration;
}

Duration SkCodecAnimatedImage::getCurrentTime() const {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    return _currentTime;
}

const Size& SkCodecAnimatedImage::getSize() const {
    return _size;
}

double SkCodecAnimatedImage::getFrameRate() const {
    return _frameRate;
}

SkCodecAnimatedImage::SkCodecAnimatedImage(std::unique_ptr<SkCodec> codec)
    : _size(Size(codec->dimensions().width(), codec->dimensions().height())) {
    SkImageInfo imageInfo = codec->getInfo().makeColorType(kN32_SkColorType);
    std::vector<SkCodec::FrameInfo> frameInfos = codec->getFrameInfo();
    const auto totalFrames = frameInfos.size();
    long totalAnimationDurationMs = 0u;
    for (size_t i = 0; i < totalFrames; i++) {
        totalAnimationDurationMs += frameInfos[i].fDuration;
    }
    _numberOfFrames = totalFrames;
    _duration = Duration::fromMilliseconds(totalAnimationDurationMs);
    _frameRate = _frameRate = codec->getFrameCount() / _duration.seconds();
    _player = std::make_unique<SkAnimCodecPlayer>(std::move(codec));
}

Valdi::Value SkCodecAnimatedImage::getMetadata() const {
    return Valdi::Value()
        .setMapValue("type", Valdi::Value(Valdi::StringBox::fromCString("skcodec")))
        .setMapValue("width", Valdi::Value(static_cast<int32_t>(_size.width)))
        .setMapValue("height", Valdi::Value(static_cast<int32_t>(_size.height)))
        .setMapValue("numberOfFrames", Valdi::Value(static_cast<int32_t>(_numberOfFrames)))
        .setMapValue("durationMs", Valdi::Value(static_cast<int32_t>(_duration.milliseconds())));
}

void SkCodecAnimatedImage::doDraw(SkCanvas* canvas,
                                  const Rect& drawBounds,
                                  const Duration& time,
                                  FittingSizeMode fittingSizeMode) {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    _currentTime = std::clamp(time, Duration(), _duration);

    _player->seek(_currentTime.milliseconds());
    const auto& image = _player->getFrame();
    const SkRect srcR = SkRect::MakeWH(_size.width, _size.height);
    canvas->drawImageRect(image,
                          srcR,
                          drawBounds.getSkValue(),
                          SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone),
                          nullptr,
                          SkCanvas::kStrict_SrcRectConstraint);
}

Valdi::Result<Ref<SkCodecAnimatedImage>> SkCodecAnimatedImage::make(std::unique_ptr<SkCodec> codec) {
    return Valdi::makeShared<SkCodecAnimatedImage>(std::move(codec));
}

VALDI_CLASS_IMPL(SkCodecAnimatedImage)

} // namespace snap::drawing
