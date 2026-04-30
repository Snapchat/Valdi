//
//  LottieAnimatedImage.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 7/22/22.
//

#include "snap_drawing/cpp/Utils/LottieAnimatedImage.hpp"
#include "snap_drawing/cpp/Drawing/DrawingContext.hpp"
#include "snap_drawing/cpp/Drawing/Surface/DrawableSurfaceCanvas.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Utils/AnimatedImage.hpp"

namespace skresources {
class DelegatedTypefaceResourceProvider : public ResourceProvider {
public:
    using DelegateFn = std::function<sk_sp<SkTypeface>(const char*, const char*)>;

    static sk_sp<DelegatedTypefaceResourceProvider> Make(DelegateFn delegateFn);

    sk_sp<SkTypeface> loadTypeface(const char* name, const char* url) const override;

private:
    explicit DelegatedTypefaceResourceProvider(DelegateFn delegateFn);

    DelegateFn _delegate;
};

DelegatedTypefaceResourceProvider::DelegatedTypefaceResourceProvider(DelegateFn delegateFn)
    : _delegate(std::move(delegateFn)) {}

sk_sp<DelegatedTypefaceResourceProvider> DelegatedTypefaceResourceProvider::Make(DelegateFn delegateFn) {
    return sk_sp<DelegatedTypefaceResourceProvider>(new DelegatedTypefaceResourceProvider(std::move(delegateFn)));
}

sk_sp<SkTypeface> DelegatedTypefaceResourceProvider::loadTypeface(const char* name, const char* url) const {
    return _delegate(name, url);
}
} // namespace skresources

namespace snap::drawing {

LottieAnimatedImage::~LottieAnimatedImage() = default;

const Duration& LottieAnimatedImage::getDuration() const {
    return _duration;
}

Duration LottieAnimatedImage::getCurrentTime() const {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    return _currentTime;
}

const Size& LottieAnimatedImage::getSize() const {
    return _size;
}

double LottieAnimatedImage::getFrameRate() const {
    return _frameRate;
}

Valdi::Value LottieAnimatedImage::getMetadata() const {
    return Valdi::Value()
        .setMapValue("type", Valdi::Value(Valdi::StringBox::fromCString("lottie")))
        .setMapValue("width", Valdi::Value(static_cast<int32_t>(_size.width)))
        .setMapValue("height", Valdi::Value(static_cast<int32_t>(_size.height)))
        .setMapValue("durationMs", Valdi::Value(static_cast<int32_t>(_duration.milliseconds())));
}

Valdi::Result<Ref<LottieAnimatedImage>> LottieAnimatedImage::make(const Ref<Resources>& resources,
                                                                  const Valdi::Byte* data,
                                                                  size_t length) {
    return make(resources->getFontManager(), data, length);
}

#ifdef SNAP_DRAWING_LOTTIE_ENABLED

LottieAnimatedImage::LottieAnimatedImage(const sk_sp<skottie::Animation>& animation)
    : _animation(animation),
      _duration(Duration::fromSeconds(animation->duration())),
      _size(Size(animation->size().width(), animation->size().height())),
      _frameRate(animation->fps()) {}

void LottieAnimatedImage::doDraw(SkCanvas* canvas,
                                 const Rect& drawBounds,
                                 const Duration& time,
                                 FittingSizeMode fittingSizeMode) {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    _currentTime = std::clamp(time, Duration(), _duration);
    _animation->seekFrameTime(_currentTime.seconds());

    // If fittingSizeMode is fill need to apply transform since
    // Skottie does not render with 'fill' mode by default
    if (fittingSizeMode == FittingSizeModeFill) {
        Scalar scaleX = drawBounds.width() / _size.width;
        Scalar scaleY = drawBounds.height() / _size.height;
        auto fillMatrix = Matrix::makeScaleTranslate(scaleX, scaleY, 0, 0);
        canvas->concat(fillMatrix.getSkValue());

        // nullptr passed as bounds to prevent scaling being applied twice
        _animation->render(canvas, nullptr, 0);
    } else {
        _animation->render(canvas, &drawBounds.getSkValue(), 0);
    }
}

static sk_sp<SkTypeface> loadTypeface(const Ref<IFontManager>& fontManager, const char* name, const char* /*url*/) {
    if (name == nullptr) {
        return nullptr;
    }

    auto typeface =
        fontManager->getTypefaceWithName(Valdi::StringCache::getGlobal().makeString(std::string_view(name)));
    if (typeface) {
        return typeface.value()->getSkValue();
    }

    return nullptr;
}
Valdi::Result<Ref<LottieAnimatedImage>> LottieAnimatedImage::make(const Ref<IFontManager>& fontManager,
                                                                  const Valdi::Byte* data,
                                                                  size_t length) {
    auto resourceProvider = skresources::CachingResourceProvider::Make(
        skresources::DataURIResourceProviderProxy::Make(skresources::DelegatedTypefaceResourceProvider::Make(
            [fontManager](const char* name, const char* url) { return loadTypeface(fontManager, name, url); })));

    auto animation = skottie::Animation::Builder()
                         .setResourceProvider(std::move(resourceProvider))
                         .make(reinterpret_cast<const char*>(data), length);
    if (!animation) {
        return Valdi::Error("Failed to parse animation");
    }

    return Valdi::makeShared<LottieAnimatedImage>(animation);
}

#else

Valdi::Result<Ref<LottieAnimatedImage>> LottieAnimatedImage::make(const Ref<IFontManager>& /*fontManager*/,
                                                                  const Valdi::Byte* /*data*/,
                                                                  size_t /*length*/) {
    return Valdi::Error("Lottie was not enabled in the build");
}

#endif

VALDI_CLASS_IMPL(LottieAnimatedImage)

} // namespace snap::drawing
