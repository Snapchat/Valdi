#include "snap_drawing/cpp/Utils/SVGUtils.hpp"

#include "snap_drawing/cpp/Text/SkFontMgrSingleton.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"

#include "include/core/SkStream.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/svg/include/SkSVGDOM.h"

#include <utility>

namespace snap::drawing {

Valdi::Result<sk_sp<SkSVGDOM>> makeSVGDOM(const Valdi::BytesView& data) {
    Image::initializeCodecs();
    SkMemoryStream stream(data.data(), data.size(), false);

    auto fontManager = snap_drawing::getSkFontMgrSingleton();
    auto resourceProvider = skresources::DataURIResourceProviderProxy::Make(
        nullptr, skresources::ImageDecodeStrategy::kPreDecode, fontManager);

    auto dom = SkSVGDOM::Builder()
                   .setFontManager(fontManager)
                   .setResourceProvider(std::move(resourceProvider))
                   .make(stream);
    if (!dom) {
        return Valdi::Error("Unable to decode SVG");
    }

    return dom;
}

} // namespace snap::drawing
