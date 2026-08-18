#include "valdi_core/cpp/Utils/PathUtils.hpp"
#include "gtest/gtest.h"
#include <iostream>

using namespace Valdi;

namespace ValdiTest {

TEST(PathUtils, isAbsolutePathAcceptance) {
    ASSERT_EQ(Path("/").isAbsolute(), true);
    ASSERT_EQ(Path("").isAbsolute(), false);
    ASSERT_EQ(Path("/test/dir").isAbsolute(), true);
    ASSERT_EQ(Path("test/dir").isAbsolute(), false);
}

TEST(PathUtils, normalizePathAcceptance) {
    ASSERT_EQ(Path("").normalize().toString(), "");
    ASSERT_EQ(Path("/").normalize().toString(), "/");
    ASSERT_EQ(Path("/test/").normalize().toString(), "/test");
    ASSERT_EQ(Path("test").normalize().toString(), "test");
    ASSERT_EQ(Path("test///dir").normalize().toString(), "test/dir");

    // Single dots should always be ingored
    ASSERT_EQ(Path("./").normalize().toString(), "");
    ASSERT_EQ(Path("/./").normalize().toString(), "/");
    ASSERT_EQ(Path("././test").normalize().toString(), "test");
    ASSERT_EQ(Path("/test/./././dir/.").normalize().toString(), "/test/dir");

    // Double dots
    ASSERT_EQ(Path("/test/../dir").normalize().toString(), "/dir");
    ASSERT_EQ(Path("/test/../../dir").normalize().toString(), "/dir");
    ASSERT_EQ(Path("test/../../dir").normalize().toString(), "../dir");
    ASSERT_EQ(Path("/test/dir/..").normalize().toString(), "/test");
    ASSERT_EQ(Path("test/dir/../..").normalize().toString(), "");
    ASSERT_EQ(Path("/../../test/dir/../..").normalize().toString(), "/");
    ASSERT_EQ(Path("../test/dir/../..").normalize().toString(), "..");
    ASSERT_EQ(Path("../test/dir/..").normalize().toString(), "../test");
}

TEST(PathUtils, removeComponentAcceptance) {
    ASSERT_EQ(Path("").removeLastComponent().toString(), "");
    ASSERT_EQ(Path("/").removeLastComponent().toString(), "");
    ASSERT_EQ(Path("test").removeLastComponent().toString(), "");
    ASSERT_EQ(Path("/test").removeLastComponent().toString(), "/");
    ASSERT_EQ(Path("test/dir").removeLastComponent().toString(), "test");
    ASSERT_EQ(Path("/test/dir").removeLastComponent().toString(), "/test");
}

TEST(PathUtils, appendPathAcceptance) {
    ASSERT_EQ(Path("").append("").toString(), "");
    ASSERT_EQ(Path("/").append("").toString(), "/");
    ASSERT_EQ(Path("").append("/").toString(), "/");
    ASSERT_EQ(Path("/").append("test").toString(), "/test");
    ASSERT_EQ(Path("./").append("/test").normalize().toString(), "test");
    ASSERT_EQ(Path("test").append("dir").toString(), "test/dir");
    ASSERT_EQ(Path("/test").append("dir").toString(), "/test/dir");
    ASSERT_EQ(Path("/test/dir").append("/path").toString(), "/test/dir/path");
    ASSERT_EQ(Path("test/dir").append("/path").toString(), "test/dir/path");
    ASSERT_EQ(Path("test/dir").append("../path").normalize().toString(), "test/path");
}

TEST(PathUtils, relativeToAcceptance) {
    ASSERT_EQ(Path("module/src/File.js").relativeTo(Path("module/src")).toString(), "File.js");
    ASSERT_EQ(Path("module/src").relativeTo(Path("module/src/nested")).toString(), "..");
    ASSERT_EQ(Path("core/src/File.js").relativeTo(Path("module/src")).toString(), "../../core/src/File.js");
    ASSERT_EQ(Path("/root/core/src/File.js").relativeTo(Path("/root/module/src")).toString(), "../../core/src/File.js");
    ASSERT_EQ(Path("module/other/../src/File.js").relativeTo(Path("module/src")).toString(), "File.js");
    ASSERT_EQ(Path("module/src").relativeTo(Path("module/src")).toString(), "");
}

TEST(PathUtils, removeFileExtensionAcceptance) {
    ASSERT_EQ(Path("").removeFileExtension().toString(), "");
    ASSERT_EQ(Path("/").removeFileExtension().toString(), "/");
    ASSERT_EQ(Path("/test").removeFileExtension().toString(), "/test");
    ASSERT_EQ(Path("./test").removeFileExtension().toString(), "./test");

    ASSERT_EQ(Path("/test/").removeFileExtension().toString(), "/test");
    ASSERT_EQ(Path("./test/").removeFileExtension().toString(), "./test");
    ASSERT_EQ(Path("/test.foo").removeFileExtension().toString(), "/test");
    ASSERT_EQ(Path("./test.foo").removeFileExtension().toString(), "./test");
}

TEST(PathUtils, appendFileExtensionAcceptance) {
    ASSERT_EQ(Path("").appendFileExtension("bar").toString(), "");
    ASSERT_EQ(Path("/").appendFileExtension("bar").toString(), "/.bar");
    ASSERT_EQ(Path("/test").appendFileExtension("bar").toString(), "/test.bar");
    ASSERT_EQ(Path("./test").appendFileExtension("bar").toString(), "./test.bar");

    ASSERT_EQ(Path("/test/").appendFileExtension("bar").toString(), "/test.bar");
    ASSERT_EQ(Path("./test/").appendFileExtension("bar").toString(), "./test.bar");
    ASSERT_EQ(Path("/test.foo").appendFileExtension("bar").toString(), "/test.foo.bar");
    ASSERT_EQ(Path("./test.foo").appendFileExtension("bar").toString(), "./test.foo.bar");
}

TEST(PathUtils, startsWithAcceptance) {
    ASSERT_TRUE(Path("").startsWith(Path("")));
    ASSERT_TRUE(Path("/").startsWith(Path("")));
    ASSERT_TRUE(Path("/").startsWith(Path("/")));
    ASSERT_FALSE(Path("/").startsWith(Path("/hello")));
    ASSERT_TRUE(Path("/hello").startsWith(Path("/")));
    ASSERT_FALSE(Path("hello").startsWith(Path("/")));
    ASSERT_FALSE(Path("hello/boy").startsWith(Path("/")));
    ASSERT_FALSE(Path("hello/boy").startsWith(Path("h")));
    ASSERT_TRUE(Path("hello/boy").startsWith(Path("hello")));
    ASSERT_TRUE(Path("hello/boy").startsWith(Path("hello/boy")));
    ASSERT_FALSE(Path("hello/boy").startsWith(Path("hello/boy/nein")));
}

TEST(PathUtils, getFileExtensionAcceptable) {
    ASSERT_EQ("", Path("").getFileExtension());
    ASSERT_EQ("", Path("hello").getFileExtension());
    ASSERT_EQ("txt", Path("hello.txt").getFileExtension());
    ASSERT_EQ("png", Path("nice/bro/hello.png").getFileExtension());
    ASSERT_EQ("jpg", Path("/nice/bro/hello.jpg").getFileExtension());
}

} // namespace ValdiTest
