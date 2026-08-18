#include "valdi/compiler_toolbox/CollapseWebPaths.hpp"

#include "valdi/compiler_toolbox/RewriteWebRequires.hpp"
#include "valdi/compiler_toolbox/WebPackageUtils.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/FlatSet.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/TextParser.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Valdi {

namespace {

struct ManifestEntry {
    std::string source;
    std::string destination;
};

static bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

static Result<std::vector<ManifestEntry>> loadManifest(const Path& manifestPath) {
    auto manifestData = DiskUtils::load(manifestPath);
    if (!manifestData) {
        return manifestData.moveError();
    }

    std::vector<ManifestEntry> entries;
    TextParser parser(manifestData.value().asStringView());
    while (!parser.isAtEnd()) {
        if (parser.tryParse('\n')) {
            continue;
        }

        auto source = parser.readUntilCharacterAndParse('\t');
        if (!source) {
            return parser.getError().rethrow(fmt::format("Invalid manifest entry in {}", manifestPath.toString()));
        }
        auto destination = parser.readUntilCharacter('\n');
        if (!destination.empty() && destination.back() == '\r') {
            destination.remove_suffix(1);
        }
        entries.push_back(ManifestEntry{std::string(source.value()), std::string(destination)});
    }
    return entries;
}

static Result<Void> copyPath(const Path& source, const Path& destination) {
    auto sourceStat = DiskUtils::stat(source);
    if (sourceStat.isDir()) {
        auto ensureResult = ensureDirectory(destination);
        if (!ensureResult) {
            return ensureResult.moveError();
        }
        for (const auto& child : DiskUtils::listDirectory(source)) {
            auto copyResult = copyPath(child, destination.appending(child.getLastComponent()));
            if (!copyResult) {
                return copyResult.moveError();
            }
        }
        return Void();
    }
    if (!sourceStat.isFile()) {
        return Error(fmt::format("Manifest input does not exist: {}", source.toString()));
    }

    auto ensureResult = ensureDirectory(destination.removingLastComponent());
    if (!ensureResult) {
        return ensureResult.moveError();
    }
    return DiskUtils::copy(source, destination);
}

static Result<Void> resetOutputDirectory(const Path& outputDirectory) {
    if (DiskUtils::stat(outputDirectory).exists() && !DiskUtils::remove(outputDirectory)) {
        return Error(fmt::format("Unable to remove output directory at {}", outputDirectory.toString()));
    }
    return ensureDirectory(outputDirectory);
}

static Result<Void> copyManifest(const Path& outputDirectory, const Path& manifestPath) {
    auto entriesResult = loadManifest(manifestPath);
    if (!entriesResult) {
        return entriesResult.moveError();
    }

    for (const auto& entry : entriesResult.value()) {
        auto destination = outputDirectory.appending(entry.destination);
        destination.normalize();
        auto copyResult = copyPath(Path(entry.source), destination);
        if (!copyResult) {
            return copyResult.moveError();
        }
    }
    return Void();
}

static bool isModuleNameCharacter(char character) {
    auto unsignedCharacter = static_cast<unsigned char>(character);
    return std::isalnum(unsignedCharacter) != 0 || character == '_' || character == '.' || character == '-';
}

static void rewriteDeclarationImportsForPrefix(std::string& content,
                                               std::string_view prefix,
                                               char quote,
                                               const std::string& packageName) {
    size_t offset = 0;
    while ((offset = content.find(prefix, offset)) != std::string::npos) {
        auto pathStart = offset + prefix.size();
        auto pathEnd = content.find(quote, pathStart);
        if (pathEnd == std::string::npos) {
            return;
        }

        auto importPath = std::string_view(content).substr(pathStart, pathEnd - pathStart);
        auto marker = importPath.find("/src/");
        auto validModuleName = marker != std::string_view::npos && marker > 0 && marker + 5 < importPath.size();
        for (size_t i = 0; validModuleName && i < marker; i++) {
            validModuleName = isModuleNameCharacter(importPath[i]);
        }
        if (!validModuleName) {
            offset = pathEnd + 1;
            continue;
        }

        auto rewrittenPath = fmt::format("{}/src/{}", packageName, importPath);
        content.replace(pathStart, importPath.size(), rewrittenPath);
        offset = pathStart + rewrittenPath.size() + 1;
    }
}

static void rewriteDeclarationImports(std::string& content, const std::string& packageName) {
    rewriteDeclarationImportsForPrefix(content, "from '", '\'', packageName);
    rewriteDeclarationImportsForPrefix(content, "from \"", '"', packageName);
    rewriteDeclarationImportsForPrefix(content, "import '", '\'', packageName);
    rewriteDeclarationImportsForPrefix(content, "import \"", '"', packageName);
}

static Result<Void> copySourceDeclarations(const Path& outputDirectory, const Path& manifestPath) {
    auto entriesResult = loadManifest(manifestPath);
    if (!entriesResult) {
        return entriesResult.moveError();
    }

    for (const auto& entry : entriesResult.value()) {
        Path source(entry.source);
        if (!DiskUtils::isFile(source)) {
            continue;
        }

        auto sourceWithLeadingSlash = fmt::format("/{}", entry.source);
        auto moduleMarker = fmt::format("/{}/", entry.destination);
        auto marker = sourceWithLeadingSlash.find(moduleMarker);
        if (marker == std::string::npos) {
            continue;
        }

        auto relativePath = sourceWithLeadingSlash.substr(marker + moduleMarker.size());
        auto destination = outputDirectory.appending("src").appending(entry.destination).appending(relativePath);
        auto copyResult = copyPath(source, destination);
        if (!copyResult) {
            return copyResult.moveError();
        }
    }
    return Void();
}

static Result<Void> rewriteDeclarations(const Path& outputDirectory, const std::string& packageName) {
    auto files = collectPackageFiles(outputDirectory);
    for (const auto& file : files) {
        if (!hasSuffix(file.relativePath, ".d.ts")) {
            continue;
        }

        auto fileData = DiskUtils::load(file.path);
        if (!fileData) {
            return fileData.moveError();
        }
        std::string content(fileData.value().asStringView());
        rewriteDeclarationImports(content, packageName);

        if (startsWith(file.relativePath, "native/")) {
            auto moduleStart = std::string_view("native/").size();
            auto moduleEnd = file.relativePath.find('/', moduleStart);
            if (moduleEnd != std::string::npos) {
                auto moduleName = file.relativePath.substr(moduleStart, moduleEnd - moduleStart);
                auto replacement = fmt::format("{}/src/{}/src/", packageName, moduleName);
                stringReplace(content, fmt::format("{}/src/../{}/src/", packageName, moduleName), replacement);
                stringReplace(content, fmt::format("{}/src/../src/", packageName), replacement);
            }
        }

        auto storeResult = DiskUtils::store(file.path, content);
        if (!storeResult) {
            return storeResult.moveError();
        }
    }
    return Void();
}

static Result<Void> generateContentRegistry(const Path& sourceDirectory,
                                            const std::string& outputName,
                                            const std::string& globalName,
                                            const std::string& contentPattern) {
    auto files = collectPackageFiles(sourceDirectory);
    std::string content = fmt::format("var __r = (globalThis.{0} = globalThis.{0} || {{}});\n", globalName);
    for (const auto& file : files) {
        if (file.path.getFileExtension() != "js") {
            continue;
        }
        auto fileData = DiskUtils::load(file.path);
        if (!fileData) {
            return fileData.moveError();
        }
        if (fileData.value().asStringView().find(contentPattern) == std::string_view::npos) {
            continue;
        }

        auto pathWithoutExtension = file.relativePath.substr(0, file.relativePath.size() - 3);
        content.append(fmt::format("__r['{0}'] = function() {{ return require('./{0}'); }};\n", pathWithoutExtension));
    }
    return DiskUtils::store(sourceDirectory.appending(outputName), content);
}

static bool isImageExtension(std::string_view extension) {
    static const FlatSet<std::string_view> extensions = {
        "png",
        "jpg",
        "jpeg",
        "svg",
        "webp",
        "gif",
    };
    return extensions.find(extension) != extensions.end();
}

static bool isImageScaleVariant(std::string_view stem) {
    auto at = stem.find('@');
    if (at != std::string_view::npos && !stem.empty() && stem.back() == 'x') {
        return true;
    }
    for (const auto suffix : {"_xxxhdpi", "_xxhdpi", "_xhdpi", "_hdpi", "_mdpi", "_ldpi"}) {
        if (hasSuffix(stem, suffix)) {
            return true;
        }
    }
    return false;
}

static std::string camelCaseImageStem(std::string_view stem) {
    std::string result;
    bool capitalizeNext = false;
    for (const auto character : stem) {
        if (character == '-' || character == '_') {
            capitalizeNext = true;
        } else if (capitalizeNext) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            capitalizeNext = false;
        } else {
            result.push_back(character);
        }
    }
    return result;
}

static Result<Void> generateImageRegistry(const Path& sourceDirectory, const FlatSet<std::string>& noInlineModules) {
    std::vector<Path> moduleDirectories;
    for (const auto& child : DiskUtils::listDirectory(sourceDirectory)) {
        if (DiskUtils::isDirectory(child)) {
            moduleDirectories.push_back(child);
        }
    }
    std::sort(moduleDirectories.begin(), moduleDirectories.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.getLastComponent() < rhs.getLastComponent();
    });

    std::string content = "var __r = (globalThis.__valdiImageRegistry = globalThis.__valdiImageRegistry || {});\n";
    for (const auto& moduleDirectory : moduleDirectories) {
        auto resourcesDirectory = moduleDirectory.appending("res");
        if (!DiskUtils::isDirectory(resourcesDirectory)) {
            continue;
        }

        std::vector<Path> images;
        for (const auto& resource : DiskUtils::listDirectory(resourcesDirectory)) {
            if (DiskUtils::isFile(resource) && isImageExtension(resource.getFileExtension())) {
                images.push_back(resource);
            }
        }
        std::sort(images.begin(), images.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.getLastComponent() < rhs.getLastComponent();
        });

        auto moduleName = std::string(moduleDirectory.getLastComponent());
        auto noInlineImages = noInlineModules.find(moduleName) != noInlineModules.end();
        std::string entries;
        for (const auto& image : images) {
            auto filename = std::string(image.getLastComponent());
            auto stem = std::string(Path::removeFileExtensionFromComponent(filename));
            if (isImageScaleVariant(stem)) {
                continue;
            }
            auto key = camelCaseImageStem(stem);
            if (key.empty()) {
                continue;
            }
            std::string resourceQuery = image.getFileExtension() == "svg" ? "?url" : "";
            if (noInlineImages) {
                resourceQuery.append(resourceQuery.empty() ? "?no-inline" : "&no-inline");
            }
            entries.append(
                fmt::format("  '{}': require('./{}/res/{}{}'),\n", key, moduleName, filename, resourceQuery));
        }
        if (!entries.empty()) {
            content.append(fmt::format("__r['{}/res'] = {{\n{}}};\n", moduleName, entries));
        }
    }
    return DiskUtils::store(sourceDirectory.appending("_image_registry.js"), content);
}

static Result<Void> generateRegistries(const Path& sourceDirectory, const FlatSet<std::string>& noInlineModules) {
    auto navigationResult = generateContentRegistry(
        sourceDirectory, "_navigation_registry.js", "__valdiNavigationPages", "NavigationPage)(module)");
    if (!navigationResult) {
        return navigationResult.moveError();
    }
    auto workerResult =
        generateContentRegistry(sourceDirectory, "_worker_registry.js", "__valdiWorkerModules", "workerService");
    if (!workerResult) {
        return workerResult.moveError();
    }
    return generateImageRegistry(sourceDirectory, noInlineModules);
}

static Result<Void> generateWebWorkerFactories(const Path& sourceDirectory, const Path& manifestPath) {
    auto entriesResult = loadManifest(manifestPath);
    if (!entriesResult) {
        return entriesResult.moveError();
    }

    auto entries = entriesResult.moveValue();
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) { return lhs.source < rhs.source; });

    std::string content = "/**\n"
                          " * AUTO-GENERATED - Do not edit. Browser worker factories for Valdi.\n"
                          " */\n"
                          "import ValdiWebWorker from \"./web_renderer/src/ValdiWebWorker.js\";\n"
                          "\n"
                          "const { registerValdiWebWorker } = ValdiWebWorker;\n\n";
    for (const auto& entry : entries) {
        auto workerUrl = fmt::format("./{}.js", entry.destination);
        content.append(fmt::format("registerValdiWebWorker(\n"
                                   "  {},\n"
                                   "  () =>\n"
                                   "    new Worker(new URL({}, import.meta.url), {{\n"
                                   "      type: \"module\",\n"
                                   "    }}),\n"
                                   ");\n\n",
                                   quoteJavaScriptString(entry.source, '"'),
                                   quoteJavaScriptString(workerUrl, '"')));
    }
    auto moduleResult = DiskUtils::store(sourceDirectory.appending("_web_worker_factories.mjs"), content);
    if (!moduleResult) {
        return moduleResult.moveError();
    }
    return DiskUtils::store(sourceDirectory.appending("_web_worker_factories.js"),
                            "require(\"./_web_worker_factories.mjs\");\n");
}

static Result<Void> generateStringsPreloads(const Path& outputDirectory, const Path& manifestPath) {
    auto entriesResult = loadManifest(manifestPath);
    if (!entriesResult) {
        return entriesResult.moveError();
    }

    for (const auto& entry : entriesResult.value()) {
        auto moduleDirectory = outputDirectory.appending("src").appending(entry.source);
        auto moduleSourceDirectory = moduleDirectory.appending("src");
        auto stringsDirectory = moduleDirectory.appending(entry.destination);
        auto stringsFile = moduleSourceDirectory.appending("Strings.js");
        if (!DiskUtils::isFile(stringsFile) || !DiskUtils::isDirectory(stringsDirectory)) {
            continue;
        }

        auto jsonFiles = collectPackageFiles(stringsDirectory);
        std::string content = "// Auto-generated: pre-loads locale JSON for bundler resolution\n";
        content.append("(globalThis.__valdiPreloadedStrings = globalThis.__valdiPreloadedStrings || {})\n");
        content.append(fmt::format("  ['{}'] = {{\n", entry.source));
        for (const auto& jsonFile : jsonFiles) {
            if (jsonFile.path.getFileExtension() != "json") {
                continue;
            }
            auto relativePath = fmt::format("{}/{}", entry.destination, jsonFile.relativePath);
            content.append(fmt::format("  '{0}': () => require('../{0}'),\n", relativePath));
        }
        content.append("};\n");

        auto preloadResult = DiskUtils::store(moduleSourceDirectory.appending("_strings_preload.js"), content);
        if (!preloadResult) {
            return preloadResult.moveError();
        }

        auto stringsData = DiskUtils::load(stringsFile);
        if (!stringsData) {
            return stringsData.moveError();
        }
        std::string stringsContent(stringsData.value().asStringView());
        stringsContent.append("\nrequire('./_strings_preload');\n");
        auto storeResult = DiskUtils::store(stringsFile, stringsContent);
        if (!storeResult) {
            return storeResult.moveError();
        }
    }
    return Void();
}

} // namespace

Result<Void> collapsePaths(const StringBox& outputDirectory, const StringBox& manifestPath) {
    Path outputPath(outputDirectory);
    auto resetResult = resetOutputDirectory(outputPath);
    if (!resetResult) {
        return resetResult.moveError();
    }
    return copyManifest(outputPath, Path(manifestPath));
}

Result<Void> collapseWebPaths(const StringBox& outputDirectory,
                              const StringBox& manifestPath,
                              const StringBox& packageName,
                              const StringBox& stringsManifestPath,
                              const StringBox& declarationsManifestPath,
                              const StringBox& webWorkersManifestPath,
                              const StringBox& imagePolicyManifestPath) {
    Path outputPath(outputDirectory);
    auto collapseResult = collapsePaths(outputDirectory, manifestPath);
    if (!collapseResult) {
        return collapseResult.moveError();
    }

    auto declarationsResult = copySourceDeclarations(outputPath, Path(declarationsManifestPath));
    if (!declarationsResult) {
        return declarationsResult.moveError();
    }
    declarationsResult = rewriteDeclarations(outputPath, packageName.slowToString());
    if (!declarationsResult) {
        return declarationsResult.moveError();
    }

    auto sourceDirectory = outputPath.appending("src");
    if (DiskUtils::isDirectory(sourceDirectory)) {
        auto imagePolicies = loadManifest(Path(imagePolicyManifestPath));
        if (!imagePolicies) {
            return imagePolicies.moveError();
        }
        FlatSet<std::string> noInlineModules;
        for (const auto& imagePolicy : imagePolicies.value()) {
            noInlineModules.insert(imagePolicy.source);
        }

        auto registryResult = generateRegistries(sourceDirectory, noInlineModules);
        if (!registryResult) {
            return registryResult.moveError();
        }
        auto workerFactoriesResult = generateWebWorkerFactories(sourceDirectory, Path(webWorkersManifestPath));
        if (!workerFactoriesResult) {
            return workerFactoriesResult.moveError();
        }
        auto prepareResult = prepareWebPackage(outputDirectory);
        if (!prepareResult) {
            return prepareResult.moveError();
        }
    }

    return generateStringsPreloads(outputPath, Path(stringsManifestPath));
}

} // namespace Valdi
