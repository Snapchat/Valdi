#include "snap_drawing/cpp/Utils/SVGAnimatedImage.hpp"

#include "snap_drawing/cpp/Utils/SVGUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include "include/core/SkCanvas.h"

namespace snap::drawing {

SVGAnimatedImage::SVGAnimatedImage(const sk_sp<SkSVGDOM>& dom) : _dom(dom) {
    auto size = _dom->containerSize();
    _size = Size(size.width(), size.height());
    if (_size.width <= 0 || _size.height <= 0) {
        _size = Size(1, 1);
    }
}

SVGAnimatedImage::~SVGAnimatedImage() = default;

Duration SVGAnimatedImage::getCurrentTime() const {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    return _currentTime;
}

const Duration& SVGAnimatedImage::getDuration() const {
    return _duration;
}

const Size& SVGAnimatedImage::getSize() const {
    return _size;
}

double SVGAnimatedImage::getFrameRate() const {
    return 0.0;
}

Valdi::Value SVGAnimatedImage::getMetadata() const {
    return Valdi::Value()
        .setMapValue("type", Valdi::Value(Valdi::StringBox::fromCString("svg")))
        .setMapValue("width", Valdi::Value(static_cast<int32_t>(_size.width)))
        .setMapValue("height", Valdi::Value(static_cast<int32_t>(_size.height)));
}

void SVGAnimatedImage::doDraw(SkCanvas* canvas,
                              const Rect& drawBounds,
                              const Duration& time,
                              FittingSizeMode fittingSizeMode) {
    std::lock_guard<Valdi::Mutex> lock(_mutex);
    _currentTime = time;

    auto svgBounds = drawBounds.makeFittingSize(_size, fittingSizeMode);
    auto saveCount = canvas->save();
    canvas->clipRect(drawBounds.getSkValue());
    canvas->translate(svgBounds.left, svgBounds.top);

    if (_size.width > 0 && _size.height > 0) {
        canvas->scale(svgBounds.width() / _size.width, svgBounds.height() / _size.height);
        _dom->setContainerSize(SkSize::Make(_size.width, _size.height));
    } else {
        _dom->setContainerSize(SkSize::Make(svgBounds.width(), svgBounds.height()));
    }

    _dom->render(canvas);
    canvas->restoreToCount(saveCount);
}

Valdi::Result<Ref<SVGAnimatedImage>> SVGAnimatedImage::make(const Valdi::Byte* data, size_t length) {
    const Valdi::BytesView bytesView(nullptr, data, length);
    auto dom = makeSVGDOM(bytesView);
    if (!dom) {
        return dom.moveError();
    }
    return Valdi::makeShared<SVGAnimatedImage>(dom.value());
}

VALDI_CLASS_IMPL(SVGAnimatedImage)

} // namespace snap::drawing
