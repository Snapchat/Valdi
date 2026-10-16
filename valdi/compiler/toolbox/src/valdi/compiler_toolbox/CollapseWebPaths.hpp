#pragma once

#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

Result<Void> collapsePaths(const StringBox& outputDirectory, const StringBox& manifestPath);

Result<Void> collapseWebPaths(const StringBox& outputDirectory,
                              const StringBox& manifestPath,
                              const StringBox& packageName,
                              const StringBox& stringsManifestPath,
                              const StringBox& declarationsManifestPath,
                              const StringBox& webWorkersManifestPath,
                              const StringBox& imagePolicyManifestPath);

} // namespace Valdi
