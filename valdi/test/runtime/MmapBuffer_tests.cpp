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

// Writes `payload` to a new file and returns an open fd to it.
int openFileWithContent(const TemporaryDirectory& dir, const char* name, const std::vector<Byte>& payload) {
    auto pathStr = dir.child(name).toString();
    int fd = open(pathStr.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0 && !payload.empty()) {
        EXPECT_EQ(static_cast<ssize_t>(payload.size()), write(fd, payload.data(), payload.size()));
    }
    return fd;
}

} // namespace

TEST(MmapBuffer, MapReadOnlyMapsTheFileContent) {
    TemporaryDirectory dir;
    std::vector<Byte> payload(64, 0xCD);
    int fd = openFileWithContent(dir, "readable.bin", payload);
    ASSERT_GE(fd, 0);

    auto result = MmapBuffer::mapReadOnly(fd, payload.size(), "readable.bin");
    close(fd);
    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_EQ(payload.size(), result.value()->size());
    ASSERT_EQ(0, std::memcmp(result.value()->data(), payload.data(), payload.size())) << "the mapping outlives the fd";
}

TEST(MmapBuffer, MapReadOnlyRejectsZeroSize) {
    TemporaryDirectory dir;
    int fd = openFileWithContent(dir, "empty.bin", {});
    ASSERT_GE(fd, 0);

    auto result = MmapBuffer::mapReadOnly(fd, 0, "empty.bin");
    close(fd);
    ASSERT_TRUE(result.failure());
}

TEST(MmapBuffer, MapReadOnlyFailsForAnInvalidFd) {
    auto result = MmapBuffer::mapReadOnly(-1, 16, "invalid");
    ASSERT_TRUE(result.failure());
}

} // namespace ValdiTest
