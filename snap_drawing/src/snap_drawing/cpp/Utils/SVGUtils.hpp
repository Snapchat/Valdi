#pragma once

#include "include/core/SkRefCnt.h"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include <utility>

class SkSVGDOM;

namespace snap::drawing {

bool isSVG(const Valdi::BytesView& data);

Valdi::Result<sk_sp<SkSVGDOM>> makeSVGDOM(const Valdi::BytesView& data);

Valdi::Result<std::pair<int, int>> getSVGSize(const Valdi::BytesView& data);

Valdi::Result<Valdi::Ref<Valdi::IBitmap>> rasterizeSVG(const Valdi::BytesView& data,
                                                       const Valdi::Ref<Valdi::IBitmapFactory>& bitmapFactory,
                                                       int preferredWidth = 0,
                                                       int preferredHeight = 0);

} // namespace snap::drawing
