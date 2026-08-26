#include "valdi/compiler_toolbox/RewriteWebRequires.hpp"

#include "valdi/compiler_toolbox/WebPackageUtils.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Valdi {

namespace {

struct ModuleEntry {
  std::string moduleName;
  std::string entryPath;
  std::string requirePath;
};

static std::unordered_set<std::string> findRequires(std::string_view content) {
  std::unordered_set<std::string> requirePaths;
  for (const auto quote : {'\'', '"'}) {
    auto target = fmt::format("require({}", quote);

    size_t offset = 0;
    while (true) {
      auto start = content.find(target, offset);
      if (start == std::string_view::npos) {
        break;
      }

      start += target.size();
      auto end = content.find(quote, start);
      if (end == std::string_view::npos) {
        break;
      }

      requirePaths.emplace(content.substr(start, end - start));
      offset = end + 1;
    }
  }
  return requirePaths;
}

static std::string resolveInternalKey(const std::string &requirePath) {
  if (requirePath == "tslib") {
    return "valdi_core/src/tslib";
  }
  if (hasSuffix(requirePath, ".js")) {
    return requirePath.substr(0, requirePath.size() - 3);
  }
  return requirePath;
}

static bool
isBareInternalRequire(const std::string &requirePath,
                      const std::unordered_set<std::string> &internalModules) {
  if (requirePath.empty() || requirePath[0] == '.' || requirePath[0] == '@') {
    return false;
  }
  return internalModules.find(resolveInternalKey(requirePath)) !=
         internalModules.end();
}

static std::string makeRelativeRequirePath(std::string_view fromDirectory,
                                           std::string_view targetPath) {
  auto result = Path(targetPath).relativeTo(Path(fromDirectory)).toString();
  if (result.empty() || result[0] != '.') {
    result = fmt::format("./{}", result);
  }
  return result;
}

static std::string directoryOfRelativePath(const std::string &path) {
  auto separator = path.rfind('/');
  if (separator == std::string::npos) {
    return std::string();
  }
  return path.substr(0, separator);
}

static bool isModuleEntry(const PackageFile &file) {
  auto extension = file.path.getFileExtension();
  return extension == "json" || extension == "bin" || extension == "protodecl";
}

static Result<Void>
generateModuleEntryRegistry(const Path &sourceDirectory,
                            const std::vector<PackageFile> &files) {
  const std::string registryRelativePath =
      "web_renderer/src/_module_entry_registry.js";
  auto registryPath = sourceDirectory.appending(registryRelativePath);
  auto ensureResult = ensureDirectory(registryPath.removingLastComponent());
  if (!ensureResult) {
    return ensureResult.moveError();
  }

  std::vector<ModuleEntry> entries;
  for (const auto &file : files) {
    if (!isModuleEntry(file)) {
      continue;
    }

    auto separator = file.relativePath.find('/');
    if (separator == std::string::npos) {
      continue;
    }

    entries.push_back(ModuleEntry{
        file.relativePath.substr(0, separator),
        file.relativePath.substr(separator + 1),
        makeRelativeRequirePath(directoryOfRelativePath(registryRelativePath),
                                file.relativePath),
    });
  }
  std::sort(entries.begin(), entries.end(),
            [](const auto &lhs, const auto &rhs) {
              return std::tie(lhs.moduleName, lhs.entryPath, lhs.requirePath) <
                     std::tie(rhs.moduleName, rhs.entryPath, rhs.requirePath);
            });

  std::string content = "var __r = (globalThis.__valdiModuleEntryRegistry = "
                        "globalThis.__valdiModuleEntryRegistry || {});\n";
  for (const auto &entry : entries) {
    auto moduleName = quoteJavaScriptString(entry.moduleName, '\'');
    content.append(fmt::format("__r[{0}] = __r[{0}] || {{}};\n", moduleName));
    content.append(
        fmt::format("__r[{}][{}] = function() {{ return require({}); }};\n",
                    moduleName, quoteJavaScriptString(entry.entryPath, '\''),
                    quoteJavaScriptString(entry.requirePath, '\'')));
  }
  return DiskUtils::store(registryPath, content);
}

} // namespace

Result<size_t> rewriteWebRequires(const StringBox &sourceDirectory) {
  Path sourcePath(sourceDirectory);
  if (!DiskUtils::isDirectory(sourcePath)) {
    return Error(fmt::format("The web source directory does not exist: {}",
                             sourcePath.toString()));
  }

  auto packageFiles = collectPackageFiles(sourcePath);

  std::vector<PackageFile> files;
  for (auto &file : packageFiles) {
    if (file.path.getFileExtension() == "js") {
      files.emplace_back(std::move(file));
    }
  }
  std::unordered_set<std::string> internalModules;
  internalModules.reserve(files.size());
  for (const auto &file : files) {
    internalModules.emplace(
        file.relativePath.substr(0, file.relativePath.size() - 3));
  }

  size_t rewrittenFileCount = 0;
  std::vector<std::string> validationErrors;
  for (const auto &file : files) {
    auto fileData = DiskUtils::load(file.path);
    if (!fileData) {
      return fileData.moveError();
    }

    std::string content(fileData.value().asStringView());
    std::unordered_map<std::string, std::string> changes;
    for (const auto &requirePath : findRequires(content)) {
      if (!isBareInternalRequire(requirePath, internalModules)) {
        continue;
      }

      auto targetPath = fmt::format("{}.js", resolveInternalKey(requirePath));
      changes.emplace(
          requirePath,
          makeRelativeRequirePath(directoryOfRelativePath(file.relativePath),
                                  targetPath));
    }

    for (const auto &[oldPath, newPath] : changes) {
      for (const auto quote : {'\'', '"'}) {
        auto oldRequire = fmt::format("require({0}{1}{0})", quote, oldPath);
        auto newRequire = fmt::format("require({0}{1}{0})", quote, newPath);
        stringReplace(content, oldRequire, newRequire);
      }
    }

    for (const auto &requirePath : findRequires(content)) {
      if (isBareInternalRequire(requirePath, internalModules)) {
        validationErrors.emplace_back(
            fmt::format("{}: require({})", file.relativePath, requirePath));
      }
    }

    if (!changes.empty()) {
      auto storeResult = DiskUtils::store(file.path, content);
      if (!storeResult) {
        return storeResult.moveError();
      }
      rewrittenFileCount++;
    }
  }

  if (!validationErrors.empty()) {
    std::string message = "Bare internal requires remain after rewrite:\n";
    for (const auto &error : validationErrors) {
      message.append(fmt::format("  {}\n", error));
    }
    return Error(std::move(message));
  }

  return rewrittenFileCount;
}

static Result<size_t> rewriteNativeWebRequires(const Path &packageDirectory) {
  auto packageFiles = collectPackageFiles(packageDirectory);

  std::vector<PackageFile> nativeFiles;
  std::unordered_set<std::string> internalModules;
  for (const auto &file : packageFiles) {
    if (file.path.getFileExtension() != "js") {
      continue;
    }
    if (file.relativePath.rfind("src/", 0) == 0) {
      internalModules.emplace(
          file.relativePath.substr(4, file.relativePath.size() - 7));
    } else if (file.relativePath.rfind("native/", 0) == 0) {
      nativeFiles.push_back(file);
    }
  }

  size_t rewrittenFileCount = 0;
  std::vector<std::string> validationErrors;
  for (const auto &file : nativeFiles) {
    auto fileData = DiskUtils::load(file.path);
    if (!fileData) {
      return fileData.moveError();
    }

    std::string content(fileData.value().asStringView());
    std::unordered_map<std::string, std::string> changes;
    for (const auto &requirePath : findRequires(content)) {
      if (!isBareInternalRequire(requirePath, internalModules)) {
        continue;
      }

      auto targetPath =
          fmt::format("src/{}.js", resolveInternalKey(requirePath));
      changes.emplace(
          requirePath,
          makeRelativeRequirePath(directoryOfRelativePath(file.relativePath),
                                  targetPath));
    }

    for (const auto &[oldPath, newPath] : changes) {
      for (const auto quote : {'\'', '"'}) {
        auto oldRequire = fmt::format("require({0}{1}{0})", quote, oldPath);
        auto newRequire = fmt::format("require({0}{1}{0})", quote, newPath);
        stringReplace(content, oldRequire, newRequire);
      }
    }

    for (const auto &requirePath : findRequires(content)) {
      if (isBareInternalRequire(requirePath, internalModules)) {
        validationErrors.emplace_back(
            fmt::format("{}: require({})", file.relativePath, requirePath));
      }
    }

    if (!changes.empty()) {
      auto storeResult = DiskUtils::store(file.path, content);
      if (!storeResult) {
        return storeResult.moveError();
      }
      rewrittenFileCount++;
    }
  }

  if (!validationErrors.empty()) {
    std::string message =
        "Bare internal requires remain after native web rewrite:\n";
    for (const auto &error : validationErrors) {
      message.append(fmt::format("  {}\n", error));
    }
    return Error(std::move(message));
  }

  return rewrittenFileCount;
}

Result<size_t> prepareWebPackage(const StringBox &outputDirectory) {
  Path outputPath(outputDirectory);
  if (!DiskUtils::isDirectory(outputPath)) {
    return Error(
        fmt::format("The web package output directory does not exist: {}",
                    outputPath.toString()));
  }

  auto sourcePath = outputPath.appending("src");
  if (!DiskUtils::isDirectory(sourcePath)) {
    return Error(
        fmt::format("The web package source directory does not exist: {}",
                    sourcePath.toString()));
  }

  auto files = collectPackageFiles(sourcePath);

  auto moduleEntryResult = generateModuleEntryRegistry(sourcePath, files);
  if (!moduleEntryResult) {
    return moduleEntryResult.moveError();
  }

  auto sourceRewriteResult = rewriteWebRequires(sourcePath.toStringBox());
  if (!sourceRewriteResult) {
    return sourceRewriteResult.moveError();
  }

  auto nativeRewriteResult = rewriteNativeWebRequires(outputPath);
  if (!nativeRewriteResult) {
    return nativeRewriteResult.moveError();
  }

  return sourceRewriteResult.value() + nativeRewriteResult.value();
}

} // namespace Valdi
