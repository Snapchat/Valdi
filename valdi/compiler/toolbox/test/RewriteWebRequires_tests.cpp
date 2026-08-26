#include "valdi/compiler_toolbox/RewriteWebRequires.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace Valdi {

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() : _path(DiskUtils::temporaryFilePath()) {
    EXPECT_TRUE(DiskUtils::makeDirectory(_path, true));
  }

  ~TemporaryDirectory() { DiskUtils::remove(_path); }

  const Path &path() const { return _path; }

  void write(std::string_view relativePath, std::string_view content) const {
    auto path = _path.appending(relativePath);
    auto parent = path.removingLastComponent();
    if (!DiskUtils::isDirectory(parent)) {
      ASSERT_TRUE(DiskUtils::makeDirectory(parent, true));
    }
    auto result = DiskUtils::store(path, content);
    ASSERT_TRUE(result) << result.description();
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

TEST(RewriteWebRequires, rewritesInternalModulesAndVendoredAliases) {
  TemporaryDirectory directory;
  directory.write("valdi_core/src/JSX.js", "module.exports = {};\n");
  directory.write("valdi_core/src/tslib.js", "module.exports = {};\n");
  directory.write("feature/src/Local.js", "module.exports = {};\n");
  directory.write(
      "feature/src/Consumer.js",
      "const jsx = require('valdi_core/src/JSX');\n"
      "const jsxWithExtension = require(\"valdi_core/src/JSX.js\");\n"
      "const helpers = require('tslib');\n"
      "const local = require('./Local');\n"
      "const external = require('external');\n"
      "const scoped = require('@scope/package');\n");

  auto result = rewriteWebRequires(directory.path().toStringBox());
  ASSERT_TRUE(result) << result.description();
  EXPECT_EQ(result.value(), 1);
  EXPECT_EQ(
      directory.read("feature/src/Consumer.js"),
      "const jsx = require('../../valdi_core/src/JSX.js');\n"
      "const jsxWithExtension = require(\"../../valdi_core/src/JSX.js\");\n"
      "const helpers = require('../../valdi_core/src/tslib.js');\n"
      "const local = require('./Local');\n"
      "const external = require('external');\n"
      "const scoped = require('@scope/package');\n");
}

TEST(RewriteWebRequires, rewritesModulesRelativeToThePackageRoot) {
  TemporaryDirectory directory;
  directory.write("Root.js", "module.exports = {};\n");
  directory.write("Consumer.js", "module.exports = require('Root');\n");

  auto result = rewriteWebRequires(directory.path().toStringBox());
  ASSERT_TRUE(result) << result.description();
  EXPECT_EQ(result.value(), 1);
  EXPECT_EQ(directory.read("Consumer.js"),
            "module.exports = require('./Root.js');\n");
}

TEST(RewriteWebRequires, rejectsBareInternalRequiresThatCannotBeReplaced) {
  TemporaryDirectory directory;
  directory.write("Root.js", "module.exports = {};\n");
  directory.write("Consumer.js",
                  "module.exports = require('Root' + suffix);\n");

  auto result = rewriteWebRequires(directory.path().toStringBox());

  ASSERT_FALSE(result);
  EXPECT_NE(result.error().getMessage().toStringView().find(
                "Consumer.js: require(Root)"),
            std::string_view::npos);
}

TEST(RewriteWebRequires, rejectsMissingSourceDirectory) {
  auto missingPath = DiskUtils::temporaryFilePath();
  auto result = rewriteWebRequires(missingPath.toStringBox());

  ASSERT_FALSE(result);
  EXPECT_NE(result.error().getMessage().toStringView().find("does not exist"),
            std::string_view::npos);
}

TEST(PrepareWebPackage, generatesModuleEntryRegistryAndRewritesRequires) {
  TemporaryDirectory directory;
  directory.write("src/app/src/Main.js",
                  "module.exports = require('core/src/Core');\n");
  directory.write("src/app/res/config.json", "{}\n");
  directory.write("src/core/src/Core.js", "module.exports = {};\n");
  directory.write("src/web_renderer/src/Existing.js", "module.exports = {};\n");
  directory.write("native/app/web/Factory.js",
                  "const factory = require('web_renderer/src/Existing');\n"
                  "const external = require('html-to-image');\n");
  directory.write("src/_navigation_registry.js", "navigation registry\n");

  auto result = prepareWebPackage(directory.path().toStringBox());

  ASSERT_TRUE(result) << result.description();
  EXPECT_EQ(result.value(), 2);
  EXPECT_EQ(directory.read("src/app/src/Main.js"),
            "module.exports = require('../../core/src/Core.js');\n");
  EXPECT_EQ(directory.read("src/web_renderer/src/_module_entry_registry.js"),
            "var __r = (globalThis.__valdiModuleEntryRegistry = "
            "globalThis.__valdiModuleEntryRegistry || {});\n"
            "__r['app'] = __r['app'] || {};\n"
            "__r['app']['res/config.json'] = function() { return "
            "require('../../app/res/config.json'); };\n");
  EXPECT_EQ(
      directory.read("native/app/web/Factory.js"),
      "const factory = require('../../../src/web_renderer/src/Existing.js');\n"
      "const external = require('html-to-image');\n");
  EXPECT_FALSE(DiskUtils::isFile(
      directory.path().appending("src/web_renderer/src/_module_registry.js")));

  auto secondResult = prepareWebPackage(directory.path().toStringBox());
  ASSERT_TRUE(secondResult) << secondResult.description();
  EXPECT_EQ(secondResult.value(), 0);
}

} // namespace

} // namespace Valdi
