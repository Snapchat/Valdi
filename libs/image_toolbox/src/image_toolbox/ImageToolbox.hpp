#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <optional>

namespace snap::imagetoolbox {

struct ImageInfo {
    uint32_t width;
    uint32_t height;
};

Valdi::Result<ImageInfo> getImageInfo(const Valdi::StringBox& imageFilePath);
Valdi::Result<Valdi::BytesView> outputImageInfo(const Valdi::StringBox& imageFilePath);

Valdi::Result<Valdi::BytesView> outputImageInfo(const Valdi::StringBox& imageFilePath);
Valdi::Result<Valdi::BytesView> convertImage(const Valdi::StringBox& inputImageFilePath,
                                             const Valdi::StringBox& outputImageFilePath,
                                             const std::optional<int>& outputWidth,
                                             const std::optional<int>& outputHeight,
                                             double qualityRatio,
                                             bool roundOutput);

} // namespace snap::imagetoolbox
