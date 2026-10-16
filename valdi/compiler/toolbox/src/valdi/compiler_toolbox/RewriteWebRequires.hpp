#pragma once

#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <cstddef>

namespace Valdi {

Result<size_t> rewriteWebRequires(const StringBox& sourceDirectory);
Result<size_t> prepareWebPackage(const StringBox& outputDirectory);

} // namespace Valdi
