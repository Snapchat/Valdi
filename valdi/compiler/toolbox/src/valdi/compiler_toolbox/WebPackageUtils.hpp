#pragma once

#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Valdi {

struct PackageFile {
    Path path;
    std::string relativePath;
};

bool hasSuffix(std::string_view value, std::string_view suffix);

std::string quoteJavaScriptString(std::string_view value, char quote);

Result<Void> ensureDirectory(const Path& directory);

std::vector<PackageFile> collectPackageFiles(const Path& directory);

} // namespace Valdi
