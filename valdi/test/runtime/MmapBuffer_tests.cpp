//
//  MmapBuffer_tests.cpp
//  valdi-pc
//

#include "valdi/runtime/Resources/MmapBuffer.hpp"
#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "gtest/gtest.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        char directoryLocation[] = "/tmp/.valdi_mmap_test.XXXXXX";
        if (mkdtemp(directoryLocation) == nullptr) {
            throw Exception(STRING_FORMAT("Failed to create temporary directory: {}", strerror(errno)));
        }
        _rootDirectory = STRING_LITERAL(directoryLocation);
    }

    ~TemporaryDirectory() {
        DiskUtils::remove(Path(_rootDirectory.toStringView()));
    }

    Path child(const char* name) const {
        return Path(_rootDirectory.toStringView()).appending(std::string_view(name));
    }

private:
    StringBox _rootDirectory;
};

} // namespace

TEST(MmapBuffer, CreateWritableProducesMappingOfRequestedSize) {
    TemporaryDirectory dir;
    constexpr size_t kSize = 4096;

    auto result = MmapBuffer::createWritable(dir.child("writable.bin"), kSize);
    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_EQ(kSize, result.value()->size());
    ASSERT_NE(nullptr, result.value()->data());
}

TEST(MmapBuffer, CreateWritableRejectsZeroSize) {
    TemporaryDirectory dir;
    auto result = MmapBuffer::createWritable(dir.child("zero.bin"), 0);
    ASSERT_TRUE(result.failure());
}

TEST(MmapBuffer, CreateWritableFailsWhenParentDoesNotExist) {
    TemporaryDirectory dir;
    auto result = MmapBuffer::createWritable(dir.child("missing/sub/dir/x.bin"), 64);
    ASSERT_TRUE(result.failure());
}

TEST(MmapBuffer, WritesPersistToDiskAndReopenMatches) {
    TemporaryDirectory dir;
    constexpr size_t kSize = 1024;
    auto path = dir.child("persisted.bin");

    {
        auto result = MmapBuffer::createWritable(path, kSize);
        ASSERT_TRUE(result.success()) << result.description();
        auto& buffer = result.value();
        auto* writable = const_cast<Byte*>(buffer->data());
        for (size_t i = 0; i < kSize; ++i) {
            writable[i] = static_cast<Byte>(i & 0xFF);
        }
        ASSERT_TRUE(buffer->makeReadOnly().success());
    }

    auto reopen = MmapBuffer::openReadOnly(path);
    ASSERT_TRUE(reopen.success()) << reopen.description();
    ASSERT_EQ(kSize, reopen.value()->size());
    for (size_t i = 0; i < kSize; ++i) {
        ASSERT_EQ(static_cast<Byte>(i & 0xFF), reopen.value()->data()[i]) << "mismatch at " << i;
    }
}

TEST(MmapBuffer, CreateWritableFromFdTakesOwnershipOnSuccess) {
    TemporaryDirectory dir;
    auto pathStr = dir.child("fd.bin").toString();
    int fd = open(pathStr.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);

    auto result = MmapBuffer::createWritable(fd, 256, pathStr);
    ASSERT_TRUE(result.success()) << result.description();

    // The fd-based overload must close fd; a second close should fail with EBADF.
    errno = 0;
    ASSERT_EQ(-1, close(fd));
    ASSERT_EQ(EBADF, errno);
}

TEST(MmapBuffer, CreateWritableFromFdClosesFdOnZeroSize) {
    TemporaryDirectory dir;
    auto pathStr = dir.child("fd_zero.bin").toString();
    int fd = open(pathStr.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);

    auto result = MmapBuffer::createWritable(fd, 0, pathStr);
    ASSERT_TRUE(result.failure());

    errno = 0;
    ASSERT_EQ(-1, close(fd));
    ASSERT_EQ(EBADF, errno);
}

TEST(MmapBuffer, OpenReadOnlySucceedsForExistingFile) {
    TemporaryDirectory dir;
    auto pathStr = dir.child("readable.bin").toString();
    std::vector<Byte> payload(64, 0xCD);

    int fd = open(pathStr.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(static_cast<ssize_t>(payload.size()), write(fd, payload.data(), payload.size()));
    close(fd);

    auto result = MmapBuffer::openReadOnly(Path(pathStr));
    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_EQ(payload.size(), result.value()->size());
    ASSERT_EQ(0, std::memcmp(result.value()->data(), payload.data(), payload.size()));
}

TEST(MmapBuffer, OpenReadOnlyFailsForMissingFile) {
    TemporaryDirectory dir;
    auto result = MmapBuffer::openReadOnly(dir.child("does_not_exist.bin"));
    ASSERT_TRUE(result.failure());
}

TEST(MmapBuffer, OpenReadOnlyFailsForEmptyFile) {
    TemporaryDirectory dir;
    auto pathStr = dir.child("empty.bin").toString();
    int fd = open(pathStr.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    close(fd);

    auto result = MmapBuffer::openReadOnly(Path(pathStr));
    ASSERT_TRUE(result.failure());
}

TEST(MmapBuffer, MakeReadOnlyAllowsReadAfterTransition) {
    TemporaryDirectory dir;
    constexpr size_t kSize = 256;
    auto result = MmapBuffer::createWritable(dir.child("ro.bin"), kSize);
    ASSERT_TRUE(result.success()) << result.description();

    auto& buffer = result.value();
    auto* writable = const_cast<Byte*>(buffer->data());
    std::memset(writable, 0x7E, kSize);

    ASSERT_TRUE(buffer->makeReadOnly().success());

    // Reads through the mapping must still work after the transition.
    Byte sum = 0;
    for (size_t i = 0; i < kSize; ++i) {
        sum ^= buffer->data()[i];
    }
    ASSERT_EQ(static_cast<Byte>(0), sum);
}

} // namespace ValdiTest
