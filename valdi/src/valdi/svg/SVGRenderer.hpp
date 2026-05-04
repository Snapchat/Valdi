#pragma once

#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

namespace Valdi {

class SVGRenderer {
public:
    static bool isSVG(const BytesView& data);

    static Result<Ref<IBitmap>> rasterizeSVG(const BytesView& svgData,
                                             const Ref<IBitmapFactory>& bitmapFactory,
                                             int32_t preferredWidth,
                                             int32_t preferredHeight);
};

} // namespace Valdi
