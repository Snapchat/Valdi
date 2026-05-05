//
//  DiskCache_tests.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 10/1/19.
//

#include "valdi/runtime/Resources/DiskCacheImpl.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "gtest/gtest.h"
#include <stdlib.h>

using namespace Valdi;

namespace ValdiTest {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        char directoryLocation[] = "/tmp/.valdi_test.XXXXXX";
        if (mkdtemp(directoryLocation) == nullptr) {
            throw Exception(STRING_FORMAT("Failed to create temporary directory: {}", strerror(errno)));
        } else {
            _rootDirectory = STRING_LITERAL(directoryLocation);
        }
    }

    ~TemporaryDirectory() {
        if (!DiskUtils::remove(Path(_rootDirectory.toStringView()))) {
            std::cout << "Failed to delete temporary directory: " << strerror(errno) << std::endl;
        }
    }

    const StringBox& get() {
        return _rootDirectory;
    }

private:
    StringBox _rootDirectory;
};

BytesView createContent(const char* filename) {
    auto str = STRING_LITERAL(filename);
    return BytesView(str.getInternedString(), reinterpret_cast<const Byte*>(str.getCStr()), str.length());
}

TEST(DiskCache, canStoreAndRetrieve) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto result = diskCache.store(Path("hello"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto loadResult = diskCache.load(Path("hello"));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();

    ASSERT_EQ(createContent("world"), loadResult.value());
}

TEST(DiskCache, failOnInvalidFile) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto loadResult = diskCache.load(Path("hello"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();

    auto storeResult = diskCache.store(Path("hello"), createContent("world"));
    ASSERT_TRUE(storeResult.success()) << storeResult.description();

    // Should now load
    auto secondLoadResult = diskCache.load(Path("hello"));
    ASSERT_TRUE(secondLoadResult.success()) << secondLoadResult.description();
}

TEST(DiskCache, canStoreInDirectory) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto result = diskCache.store(Path("hello/this/is/nice"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto loadResult = diskCache.load(Path("hello"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();
    loadResult = diskCache.load(Path("hello/this"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();
    loadResult = diskCache.load(Path("hello/this/is"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();

    loadResult = diskCache.load(Path("hello/this/is/nice"));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();

    ASSERT_EQ(createContent("world"), loadResult.value());
}

TEST(DiskCache, canDeleteFile) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto result = diskCache.store(Path("hello"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto loadResult = diskCache.load(Path("hello"));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();

    auto removeSuccess = diskCache.remove(Path("hello"));
    ASSERT_TRUE(removeSuccess);

    // Should now fail
    loadResult = diskCache.load(Path("hello"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();
}

TEST(DiskCache, canDeleteDirectory) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto result = diskCache.store(Path("hello/this/is/nice"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto result2 = diskCache.store(Path("hello/cool"), createContent("nice"));
    ASSERT_TRUE(result2.success()) << result2.description();

    auto removeSuccess = diskCache.remove(Path("hello/this"));
    ASSERT_TRUE(removeSuccess);

    auto loadResult = diskCache.load(Path("hello/this/is/nice"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();
    loadResult = diskCache.load(Path("hello/cool"));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();
}

TEST(DiskCache, detectOutOfBoundsWrite) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto result = diskCache.store(Path("../hello"), createContent("world"));
    ASSERT_FALSE(result.success()) << result.description();
}

TEST(DiskCache, canLoadFromAbsolutePath) {
    TemporaryDirectory directory;
    DiskCacheImpl diskCache(directory.get());

    auto result = diskCache.store(Path("hello"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto absoluteURL = directory.get().append("/hello");

    // Make sure we have an absolute URL
    ASSERT_TRUE(absoluteURL.hasPrefix("/tmp/.valdi_test."));
    ASSERT_TRUE(absoluteURL.hasSuffix("/hello"));

    auto loadResult = diskCache.load(Path(absoluteURL));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();
}

TEST(DiskCache, canCreateScopedInstance) {
    TemporaryDirectory directory;
    DiskCacheImpl parentDiskCache(directory.get());

    auto scoped = parentDiskCache.scopedCache(Path("dir"), false);

    auto result = scoped->store(Path("hello"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto loadResult = scoped->load(Path("hello"));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();

    auto loadFromParent = parentDiskCache.load(Path("hello"));

    ASSERT_FALSE(loadFromParent.success()) << loadFromParent.description();

    loadFromParent = parentDiskCache.load(Path("dir/hello"));

    ASSERT_TRUE(loadFromParent.success()) << loadFromParent.description();
}

TEST(DiskCache, scopedInstanceRespectsIsolation) {
    TemporaryDirectory directory;

    DiskCacheImpl parentDiskCache(directory.get());

    auto result = parentDiskCache.store(Path("hello"), createContent("world"));
    ASSERT_TRUE(result.success()) << result.description();

    auto scopedWithNoReadRights = parentDiskCache.scopedCache(Path("dir"), false);
    auto scopedWithReadRights = parentDiskCache.scopedCache(Path("dir"), true);

    auto loadResult = scopedWithNoReadRights->load(Path("../hello"));
    ASSERT_FALSE(loadResult.success()) << loadResult.description();

    loadResult = scopedWithReadRights->load(Path("../hello"));
    ASSERT_TRUE(loadResult.success()) << loadResult.description();

    // Neither of them should be allowed to write

    result = scopedWithNoReadRights->store(Path("../hello"), createContent("world"));
    ASSERT_FALSE(result.success()) << result.description();

    result = scopedWithReadRights->store(Path("../hello"), createContent("world"));
    ASSERT_FALSE(result.success()) << result.description();
}

} // namespace ValdiTest
