//
//  Bitmap.cpp
//  snap_drawing
//
//  Created by Ramzy Jaber on 3/28/22.
//

#include "snap_drawing/cpp/Utils/Bitmap.hpp"
#include "snap_drawing/cpp/Utils/BitmapUtils.hpp"

namespace snap::drawing {

Bitmap::Bitmap(SkBitmap&& bitmap) : _bitmap(std::move(bitmap)) {}

Valdi::BitmapInfo Bitmap::getInfo() const {
    const auto& pixmap = _bitmap.pixmap();
    auto info = toBitmapInfo(pixmap.info());
    // The pixels may have been allocated with a stride wider than the minimum.
    info.rowBytes = pixmap.rowBytes();
    return info;
}

void* Bitmap::lockBytes() {
    const auto& pixmap = _bitmap.pixmap();
    return pixmap.writable_addr();
}

void Bitmap::unlockBytes() {}

void Bitmap::dispose() {
    _bitmap = SkBitmap();
}

Valdi::Result<Ref<Bitmap>> Bitmap::make(const Valdi::BitmapInfo& info) {
    SkBitmap skBitmap;
    auto imageInfo = toSkiaImageInfo(info);
    if (!tryAllocBitmapPixels(skBitmap, imageInfo, std::max(info.rowBytes, imageInfo.minRowBytes()))) {
        return Valdi::Error("Failed to allocate pixels for bitmap");
    }

    return Valdi::makeShared<Bitmap>(std::move(skBitmap));
}

} // namespace snap::drawing
