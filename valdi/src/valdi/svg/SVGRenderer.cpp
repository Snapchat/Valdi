#include "valdi/svg/SVGRenderer.hpp"

#include "snap_drawing/cpp/Utils/SVGUtils.hpp"

namespace Valdi {

bool SVGRenderer::isSVG(const BytesView& data) {
    return snap::drawing::isSVG(data);
}

Result<Ref<IBitmap>> SVGRenderer::rasterizeSVG(const BytesView& svgData,
                                               const Ref<IBitmapFactory>& bitmapFactory,
                                               int32_t preferredWidth,
                                               int32_t preferredHeight) {
    return snap::drawing::rasterizeSVG(svgData, bitmapFactory, preferredWidth, preferredHeight);
}

} // namespace Valdi
