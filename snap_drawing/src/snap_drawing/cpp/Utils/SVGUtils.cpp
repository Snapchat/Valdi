#include "snap_drawing/cpp/Utils/SVGUtils.hpp"

#include "snap_drawing/cpp/Text/SkFontMgrSingleton.hpp"
#include "snap_drawing/cpp/Utils/BitmapUtils.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "valdi_core/cpp/Utils/TextParser.hpp"

#include <cmath>
#include <utility>

namespace snap::drawing {

bool isSVG(const Valdi::BytesView& data) {
    if (data.size() == 0 || data.data() == nullptr) {
        return false;
    }
    Valdi::TextParser parser(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
    parser.tryParseWhitespaces();
    return parser.tryParse("<svg") || parser.tryParse("<?xml");
}

Valdi::Result<sk_sp<SkSVGDOM>> makeSVGDOM(const Valdi::BytesView& data) {
    Image::initializeCodecs();
    SkMemoryStream stream(data.data(), data.size(), false);

    auto fontManager = snap_drawing::getSkFontMgrSingleton();
    auto resourceProvider = skresources::DataURIResourceProviderProxy::Make(
        nullptr, skresources::ImageDecodeStrategy::kPreDecode, fontManager);

    auto dom =
        SkSVGDOM::Builder().setFontManager(fontManager).setResourceProvider(std::move(resourceProvider)).make(stream);
    if (!dom) {
        return Valdi::Error("Unable to decode SVG");
    }

    return dom;
}

Valdi::Result<std::pair<int, int>> getSVGSize(const Valdi::BytesView& data) {
    auto dom = makeSVGDOM(data);
    if (!dom) {
        return dom.moveError();
    }

    auto size = dom.value()->containerSize();
    return std::make_pair(static_cast<int>(std::ceil(size.width())), static_cast<int>(std::ceil(size.height())));
}

static int resolveSVGDimension(int preferredDimension, SkScalar intrinsicDimension) {
    if (preferredDimension > 0) {
        return preferredDimension;
    }
    return static_cast<int>(std::ceil(intrinsicDimension));
}

class ScopedBitmapLock {
public:
    explicit ScopedBitmapLock(const Valdi::Ref<Valdi::IBitmap>& bitmap) : _bitmap(bitmap) {
        _bytes = _bitmap->lockBytes();
    }

    ~ScopedBitmapLock() {
        if (_bytes != nullptr) {
            _bitmap->unlockBytes();
        }
    }

    void* bytes() const {
        return _bytes;
    }

private:
    Valdi::Ref<Valdi::IBitmap> _bitmap;
    void* _bytes = nullptr;
};

Valdi::Result<Valdi::Ref<Valdi::IBitmap>> rasterizeSVG(const Valdi::BytesView& data,
                                                       const Valdi::Ref<Valdi::IBitmapFactory>& bitmapFactory,
                                                       int preferredWidth,
                                                       int preferredHeight) {
    if (bitmapFactory == nullptr) {
        return Valdi::Error("SVG rasterization requires a bitmap factory");
    }

    auto dom = makeSVGDOM(data);
    if (!dom) {
        return dom.moveError();
    }

    auto intrinsicSize = dom.value()->containerSize();
    auto intrinsicWidth = intrinsicSize.width();
    auto intrinsicHeight = intrinsicSize.height();

    if (preferredWidth > 0 && preferredHeight <= 0 && intrinsicWidth > 0 && intrinsicHeight > 0) {
        preferredHeight = static_cast<int>(std::ceil(preferredWidth * intrinsicHeight / intrinsicWidth));
    } else if (preferredHeight > 0 && preferredWidth <= 0 && intrinsicWidth > 0 && intrinsicHeight > 0) {
        preferredWidth = static_cast<int>(std::ceil(preferredHeight * intrinsicWidth / intrinsicHeight));
    }

    auto outputWidth = resolveSVGDimension(preferredWidth, intrinsicWidth);
    auto outputHeight = resolveSVGDimension(preferredHeight, intrinsicHeight);
    if (outputWidth <= 0 || outputHeight <= 0) {
        return Valdi::Error("SVG doesn't have a valid size");
    }

    auto bitmap = bitmapFactory->createBitmap(outputWidth, outputHeight);
    if (!bitmap) {
        return bitmap.error().rethrow("Failed to create SVG rasterization bitmap: ");
    }

    auto outputBitmap = bitmap.moveValue();
    auto bitmapInfo = outputBitmap->getInfo();
    if (bitmapInfo.width != outputWidth || bitmapInfo.height != outputHeight) {
        return Valdi::Error("SVG rasterization bitmap factory returned an unexpected bitmap size");
    }

    ScopedBitmapLock bitmapLock(outputBitmap);
    if (bitmapLock.bytes() == nullptr) {
        return Valdi::Error("Failed to lock SVG rasterization bitmap bytes");
    }

    auto surface = SkSurfaces::WrapPixels(toSkiaImageInfo(bitmapInfo), bitmapLock.bytes(), bitmapInfo.rowBytes);
    if (!surface) {
        return Valdi::Error("Unable to create SVG rasterization surface");
    }

    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);

    if (intrinsicWidth > 0 && intrinsicHeight > 0) {
        canvas->scale(static_cast<SkScalar>(outputWidth) / intrinsicWidth,
                      static_cast<SkScalar>(outputHeight) / intrinsicHeight);
        dom.value()->setContainerSize(intrinsicSize);
    } else {
        dom.value()->setContainerSize(SkSize::Make(outputWidth, outputHeight));
    }

    dom.value()->render(canvas);
    surface = nullptr;

    return outputBitmap;
}

} // namespace snap::drawing
