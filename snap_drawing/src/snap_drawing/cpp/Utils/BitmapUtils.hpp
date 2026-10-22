//
//  BitmapUtils.hpp
//  snap_drawing-pc
//
//  Created by Simon Corsin on 1/24/22.
//

#pragma once

#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"

namespace snap::drawing {

Valdi::BitmapInfo toBitmapInfo(const SkImageInfo& info);

SkImageInfo toSkiaImageInfo(const Valdi::BitmapInfo& info);
SkColorType toSkiaColorType(Valdi::ColorType colorType);

/**
 * Allocates the bitmap's pixels with `rowBytes` for every row, including the last, so the allocation covers
 * `Valdi::BitmapInfo::size()` (height * rowBytes). SkBitmap::tryAllocPixels() only allocates the minimum
 * width for the last row.
 */
bool tryAllocBitmapPixels(SkBitmap& bitmap, const SkImageInfo& info, size_t rowBytes);

Valdi::Result<sk_sp<SkData>> bitmapToData(const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                          const Valdi::BitmapInfo& info,
                                          bool copy);

} // namespace snap::drawing
