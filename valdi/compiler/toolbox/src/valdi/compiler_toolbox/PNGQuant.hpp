#pragma once

#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

Result<Void> optimizePNG(const StringBox& inputPath);

} // namespace Valdi
