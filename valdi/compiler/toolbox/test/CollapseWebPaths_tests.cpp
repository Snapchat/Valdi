#include "valdi/compiler_toolbox/CollapseWebPaths.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace Valdi {

namespace {

class CollapseTemporaryDirectory {
public:
    CollapseTemporaryDirectory() : _path(DiskUtils::temporaryFilePath()) {
        EXPECT_TRUE(DiskUtils::makeDirectory(_path, true));
    }

    ~CollapseTemporaryDirectory() {
        DiskUtils::remove(_path);
    }

    const Path& path() const {
        return _path;
    }

    Path write(std::string_view relativePath, std::string_view content) const {
        auto path = _path.appending(relativePath);
        auto parent = path.removingLastComponent();
        if (!DiskUtils::isDirectory(parent)) {
            EXPECT_TRUE(DiskUtils::makeDirectory(parent, true));
        }
        auto result = DiskUtils::store(path, content);
        EXPECT_TRUE(result) << result.description();
        return path;
    }

    std::string read(std::string_view relativePath) const {
        auto result = DiskUtils::load(_path.appending(relativePath));
        EXPECT_TRUE(result) << result.description();
        if (!result) {
            return std::string();
        }
        return std::string(result.value().asStringView());
    }

private:
    Path _path;
};

TEST(CollapsePaths, copiesFilesAndDirectoryTrees) {
    CollapseTemporaryDirectory directory;
    auto file = directory.write("inputs/file.txt", "file\n");
    auto nestedFile = directory.write("inputs/tree/nested/value.txt", "nested\n");
    auto manifest = directory.write(
        "manifest.tsv",
        fmt::format("{}\tdestination/file.txt\n{}\tdestination/tree/nested/value.txt\n", file, nestedFile));
    auto output = directory.path().appending("output");

    auto result = collapsePaths(output.toStringBox(), manifest.toStringBox());

    ASSERT_TRUE(result) << result.description();
    EXPECT_EQ(directory.read("output/destination/file.txt"), "file\n");
    EXPECT_EQ(directory.read("output/destination/tree/nested/value.txt"), "nested\n");
}

TEST(CollapseWebPaths, buildsTheCompleteWebPackage) {
    CollapseTemporaryDirectory directory;
    auto main = directory.write("inputs/app/src/Main.js",
                                "NavigationPage)(module);\n"
                                "module.exports = require('core/src/Core');\n");
    auto worker = directory.write("inputs/app/src/Worker.js", "workerService(module);\n");
    auto strings = directory.write("inputs/app/src/Strings.js", "\"use strict\";\n");
    auto declaration = directory.write("inputs/app/src/Types.d.ts", "import { Core } from 'core/src/Core';\n");
    auto locale = directory.write("inputs/app/strings/en.json", "{}\n");
    auto image = directory.write("inputs/app/res/music_icon.svg", "<svg/>\n");
    auto config = directory.write("inputs/app/res/config.json", "{}\n");
    auto core = directory.write("inputs/core/src/Core.js", "module.exports = {};\n");

    auto manifest = directory.write("manifest.tsv",
                                    fmt::format("{}\tsrc/app/src/Main.js\n"
                                                "{}\tsrc/app/src/Worker.js\n"
                                                "{}\tsrc/app/src/Strings.js\n"
                                                "{}\tsrc/app/strings/en.json\n"
                                                "{}\tsrc/app/res/music_icon.svg\n"
                                                "{}\tsrc/app/res/config.json\n"
                                                "{}\tsrc/core/src/Core.js\n",
                                                main,
                                                worker,
                                                strings,
                                                locale,
                                                image,
                                                config,
                                                core));
    auto stringsManifest = directory.write("strings.tsv", "app\tstrings\n");
    auto declarationsManifest = directory.write("declarations.tsv", fmt::format("{}\tapp\n", declaration));
    auto webWorkersManifest = directory.write("web-workers.tsv",
                                              "zeta/src/Worker\tzeta/src/Entry\n"
                                              "alpha/src/Worker\talpha/src/Entry\n");
    auto imagePolicyManifest = directory.write("image-policy.tsv", "");
    auto output = directory.path().appending("output");

    auto result = collapseWebPaths(output.toStringBox(),
                                   manifest.toStringBox(),
                                   StringCache::getGlobal().makeString(std::string_view("@scope/package")),
                                   stringsManifest.toStringBox(),
                                   declarationsManifest.toStringBox(),
                                   webWorkersManifest.toStringBox(),
                                   imagePolicyManifest.toStringBox());

    ASSERT_TRUE(result) << result.description();
    EXPECT_EQ(directory.read("output/src/app/src/Main.js"),
              "NavigationPage)(module);\n"
              "module.exports = require('../../core/src/Core.js');\n");
    EXPECT_EQ(directory.read("output/src/app/src/Types.d.ts"),
              "import { Core } from '@scope/package/src/core/src/Core';\n");
    EXPECT_EQ(directory.read("output/src/_navigation_registry.js"),
              "var __r = (globalThis.__valdiNavigationPages = globalThis.__valdiNavigationPages || {});\n"
              "__r['app/src/Main'] = function() { return require('./app/src/Main'); };\n");
    EXPECT_EQ(directory.read("output/src/_worker_registry.js"),
              "var __r = (globalThis.__valdiWorkerModules = globalThis.__valdiWorkerModules || {});\n"
              "__r['app/src/Worker'] = function() { return require('./app/src/Worker'); };\n");
    EXPECT_EQ(directory.read("output/src/_web_worker_factories.js"), "require(\"./_web_worker_factories.mjs\");\n");
    EXPECT_EQ(directory.read("output/src/_web_worker_factories.mjs"),
              "/**\n"
              " * AUTO-GENERATED - Do not edit. Browser worker factories for Valdi.\n"
              " */\n"
              "import ValdiWebWorker from \"./web_renderer/src/ValdiWebWorker.js\";\n"
              "\n"
              "const { registerValdiWebWorker } = ValdiWebWorker;\n"
              "\n"
              "registerValdiWebWorker(\n"
              "  \"alpha/src/Worker\",\n"
              "  () =>\n"
              "    new Worker(new URL(\"./alpha/src/Entry.js\", import.meta.url), {\n"
              "      type: \"module\",\n"
              "    }),\n"
              ");\n"
              "\n"
              "registerValdiWebWorker(\n"
              "  \"zeta/src/Worker\",\n"
              "  () =>\n"
              "    new Worker(new URL(\"./zeta/src/Entry.js\", import.meta.url), {\n"
              "      type: \"module\",\n"
              "    }),\n"
              ");\n"
              "\n");
    EXPECT_EQ(directory.read("output/src/_image_registry.js"),
              "var __r = (globalThis.__valdiImageRegistry = globalThis.__valdiImageRegistry || {});\n"
              "__r['app/res'] = {\n"
              "  'musicIcon': require('./app/res/music_icon.svg?url'),\n"
              "};\n");
    EXPECT_EQ(directory.read("output/src/app/src/_strings_preload.js"),
              "// Auto-generated: pre-loads locale JSON for bundler resolution\n"
              "(globalThis.__valdiPreloadedStrings = globalThis.__valdiPreloadedStrings || {})\n"
              "  ['app'] = {\n"
              "  'strings/en.json': () => require('../strings/en.json'),\n"
              "};\n");
    EXPECT_EQ(directory.read("output/src/app/src/Strings.js"),
              "\"use strict\";\n"
              "\nrequire('./_strings_preload');\n");
}

TEST(CollapseWebPaths, appliesImageInliningPolicyPerModule) {
    CollapseTemporaryDirectory directory;
    auto optedInSvg = directory.write("inputs/alpha/res/music_icon.svg", "<svg/>\n");
    auto optedInPng = directory.write("inputs/alpha/res/photo.png", "png\n");
    auto optedInVariant = directory.write("inputs/alpha/res/photo@2x.png", "variant\n");
    auto defaultSvg = directory.write("inputs/zeta/res/plain_icon.svg", "<svg/>\n");
    auto defaultPng = directory.write("inputs/zeta/res/picture.png", "png\n");
    auto manifest = directory.write("manifest.tsv",
                                    fmt::format("{}\tsrc/zeta/res/plain_icon.svg\n"
                                                "{}\tsrc/alpha/res/photo@2x.png\n"
                                                "{}\tsrc/alpha/res/photo.png\n"
                                                "{}\tsrc/zeta/res/picture.png\n"
                                                "{}\tsrc/alpha/res/music_icon.svg\n",
                                                defaultSvg,
                                                optedInVariant,
                                                optedInPng,
                                                defaultPng,
                                                optedInSvg));
    auto stringsManifest = directory.write("strings.tsv", "");
    auto declarationsManifest = directory.write("declarations.tsv", "");
    auto webWorkersManifest = directory.write("web-workers.tsv", "");
    auto imagePolicyManifest = directory.write("image-policy.tsv", "alpha\tno-inline\n");
    auto output = directory.path().appending("output");

    auto result = collapseWebPaths(output.toStringBox(),
                                   manifest.toStringBox(),
                                   StringCache::getGlobal().makeString(std::string_view("@scope/package")),
                                   stringsManifest.toStringBox(),
                                   declarationsManifest.toStringBox(),
                                   webWorkersManifest.toStringBox(),
                                   imagePolicyManifest.toStringBox());

    ASSERT_TRUE(result) << result.description();
    EXPECT_EQ(directory.read("output/src/_image_registry.js"),
              "var __r = (globalThis.__valdiImageRegistry = globalThis.__valdiImageRegistry || {});\n"
              "__r['alpha/res'] = {\n"
              "  'musicIcon': require('./alpha/res/music_icon.svg?url&no-inline'),\n"
              "  'photo': require('./alpha/res/photo.png?no-inline'),\n"
              "};\n"
              "__r['zeta/res'] = {\n"
              "  'picture': require('./zeta/res/picture.png'),\n"
              "  'plainIcon': require('./zeta/res/plain_icon.svg?url'),\n"
              "};\n");
}

} // namespace

} // namespace Valdi
