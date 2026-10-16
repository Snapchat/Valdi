#include "valdi/compiler_toolbox/WebPackageUtils.hpp"

#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"

#include <algorithm>
#include <utility>

namespace Valdi {

namespace {

static std::string appendRelativePath(std::string_view base, std::string_view component) {
    if (base.empty()) {
        return std::string(component);
    }
    return fmt::format("{}/{}", base, component);
}

static void collectPackageFiles(const Path& directory,
                                const std::string& relativeDirectory,
                                std::vector<PackageFile>& files) {
    for (const auto& child : DiskUtils::listDirectory(directory)) {
        auto relativePath = appendRelativePath(relativeDirectory, child.getLastComponent());
        auto stat = DiskUtils::stat(child);
        if (stat.isDir()) {
            collectPackageFiles(child, relativePath, files);
        } else if (stat.isFile()) {
            files.push_back(PackageFile{child, std::move(relativePath)});
        }
    }
}

} // namespace

bool hasSuffix(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

std::string quoteJavaScriptString(std::string_view value, char quote) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back(quote);
    for (const auto character : value) {
        switch (character) {
            case '\\':
                result.append("\\\\");
                break;
            case '\b':
                result.append("\\b");
                break;
            case '\f':
                result.append("\\f");
                break;
            case '\n':
                result.append("\\n");
                break;
            case '\r':
                result.append("\\r");
                break;
            case '\t':
                result.append("\\t");
                break;
            default:
                if (character == quote) {
                    result.push_back('\\');
                    result.push_back(character);
                } else if (static_cast<unsigned char>(character) < 0x20) {
                    result.append(fmt::format("\\u{:04x}", static_cast<unsigned char>(character)));
                } else {
                    result.push_back(character);
                }
                break;
        }
    }
    result.push_back(quote);
    return result;
}

Result<Void> ensureDirectory(const Path& directory) {
    if (DiskUtils::isDirectory(directory)) {
        return Void();
    }
    if (!DiskUtils::makeDirectory(directory, true)) {
        return Error(fmt::format("Unable to create directory at {}", directory.toString()));
    }
    return Void();
}

std::vector<PackageFile> collectPackageFiles(const Path& directory) {
    std::vector<PackageFile> files;
    collectPackageFiles(directory, std::string(), files);
    std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.relativePath < rhs.relativePath;
    });
    return files;
}

} // namespace Valdi
