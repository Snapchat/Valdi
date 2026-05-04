#include "SVGRenderer.hpp"
#include "resvg/c-api/resvg.h"
#include "snap_drawing/cpp/Utils/Bitmap.hpp"
#include "utils/base/NonCopyable.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace snap::imagetoolbox {

static Valdi::Error svgErrorFromResult(int32_t result) {
    switch (static_cast<resvg_error>(result)) {
        case RESVG_OK:
            return Valdi::Error("Everything is ok");
        case RESVG_ERROR_NOT_AN_UTF8_STR:
            return Valdi::Error("Only UTF-8 content are supported");
        case RESVG_ERROR_FILE_OPEN_FAILED:
            return Valdi::Error("Failed to open the provided file");
        case RESVG_ERROR_MALFORMED_GZIP:
            return Valdi::Error("Compressed SVG must use the GZip algorithm");
        case RESVG_ERROR_ELEMENTS_LIMIT_REACHED:
            return Valdi::Error("We do not allow SVG with more than 1_000_000 elements for security reasons");
        case RESVG_ERROR_INVALID_SIZE:
            return Valdi::Error("SVG doesn't have a valid size");
        case RESVG_ERROR_PARSING_FAILED:
            return Valdi::Error("Failed to parse an SVG data");
    }
}

class ReSVGHandle : public Valdi::SimpleRefCountable {
public:
    ReSVGHandle(resvg_render_tree* tree) : _tree(tree) {}

    ~ReSVGHandle() override {
        resvg_tree_destroy(_tree);
    }

    std::pair<int, int> getSize() const {
        auto size = resvg_get_image_size(_tree);
        return std::make_pair(static_cast<int>(size.width), static_cast<int>(size.height));
    }

    resvg_render_tree* tree() const {
        return _tree;
    }

private:
    resvg_render_tree* _tree;
};

class ReSVG {
public:
    ReSVG() {
        static std::once_flag flag;
        std::call_once(flag, []() { resvg_init_log(); });

        _options = resvg_options_create();
    }

    ~ReSVG() {
        resvg_options_destroy(_options);
    }

    Valdi::Result<Valdi::Ref<ReSVGHandle>> parse(const Valdi::Byte* data, size_t length) {
        resvg_render_tree* tree = nullptr;
        auto loadResult = resvg_parse_tree_from_data(reinterpret_cast<const char*>(data), length, _options, &tree);
        if (loadResult != RESVG_OK) {
            return svgErrorFromResult(loadResult);
        }

        return Valdi::makeShared<ReSVGHandle>(tree);
    }

private:
    resvg_options* _options = nullptr;
};

Valdi::Result<std::pair<int, int>> SVGRenderer::getSize(const Valdi::Byte* data, size_t length) {
    ReSVG reSVG;
    auto result = reSVG.parse(data, length);
    if (!result) {
        return result.moveError();
    }

    return result.value()->getSize();
}

Valdi::Result<snap::drawing::Ref<snap::drawing::Image>> SVGRenderer::render(const Valdi::Byte* data,
                                                                            size_t length,
                                                                            int outputWidth,
                                                                            int outputHeight) {
    ReSVG reSVG;
    auto result = reSVG.parse(data, length);
    if (!result) {
        return result.moveError();
    }

    auto svgSize = result.value()->getSize();

    int surfaceWidth;
    if (outputWidth > 0) {
        surfaceWidth = outputWidth;
    } else {
        surfaceWidth = svgSize.first;
    }

    int surfaceHeight;
    if (outputHeight > 0) {
        surfaceHeight = outputHeight;
    } else {
        surfaceHeight = svgSize.second;
    }

    auto outputBitmapResult = snap::drawing::Bitmap::make(Valdi::BitmapInfo(
        surfaceWidth, surfaceHeight, Valdi::ColorType::ColorTypeRGBA8888, Valdi::AlphaType::AlphaTypePremul, 0));
    if (!outputBitmapResult) {
        return outputBitmapResult.error().rethrow("Failed to create output bitmap: ");
    }

    auto outputBitmap = outputBitmapResult.moveValue();

    auto* outputBytes = outputBitmap->lockBytes();

    auto transform = resvg_transform_identity();
    auto scale = std::max(static_cast<float>(surfaceWidth) / static_cast<float>(svgSize.first),
                          static_cast<float>(surfaceHeight) / static_cast<float>(svgSize.second));
    transform.a = scale;
    transform.d = scale;

    resvg_render(result.value()->tree(),
                 transform,
                 static_cast<uint32_t>(surfaceWidth),
                 static_cast<uint32_t>(surfaceHeight),
                 reinterpret_cast<char*>(outputBytes));
    outputBitmap->unlockBytes();

    return snap::drawing::Image::makeFromBitmap(outputBitmap, false);
}

} // namespace snap::imagetoolbox
