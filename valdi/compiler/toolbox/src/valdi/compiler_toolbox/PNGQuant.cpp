#include "valdi/compiler_toolbox/PNGQuant.hpp"

#include "libimagequant.h"
// clang-format off
extern "C" {
#include "rwpng.h"
#include "pngquant_opts.h"
// clang-format on
pngquant_error pngquant_main_internal(struct pngquant_options* options, liq_attr* liq);
}

#include <string>

namespace Valdi {

Result<Void> optimizePNG(const StringBox& inputPath) {
    liq_attr* liq = liq_attr_create();
    if (!liq) {
        return Error("SSE-capable CPU is required for this pngquant build");
    }

    if (LIQ_OK != liq_set_quality(liq, 70, 90)) {
        liq_attr_destroy(liq);
        return Error("Invalid pngquant quality");
    }

    if (LIQ_OK != liq_set_speed(liq, 1)) {
        liq_attr_destroy(liq);
        return Error("Speed should be between 1 (slow) and 11 (fast)");
    }

    std::string inputFilePath = inputPath.slowToString();
    char* files[] = {inputFilePath.data()};
    pngquant_options options = {
        .extension = ".png",
        .files = files,
        .num_files = 1,
        .speed = 1,
        .floyd = 1.f,
        .force = true,
        .min_quality_limit = true,
        .skip_if_larger = true,
        .strip = false,
    };

    auto pngquantResult = pngquant_main_internal(&options, liq);
    liq_attr_destroy(liq);
    if (pngquantResult == SUCCESS || pngquantResult == TOO_LARGE_FILE || pngquantResult == TOO_LOW_QUALITY) {
        return Void();
    }

    return Error("pngquant failed with error code " + std::to_string(pngquantResult));
}

} // namespace Valdi
