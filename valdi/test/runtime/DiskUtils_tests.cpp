#include "valdi_core/cpp/Utils/DiskUtils.hpp"
#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

TEST(DiskUtilsTests, aFileStatThatDoesNotExistHasNoTypeOrSize) {
    FileStat const missing(false, true, true, 4096);

    EXPECT_FALSE(missing.isDir());
    EXPECT_FALSE(missing.isFile());
    EXPECT_EQ(missing.size(), 0u);
}

TEST(DiskUtilsTests, statOfAMissingPathReportsNeitherFileNorDirectory) {
    auto const missing = DiskUtils::stat(Path("/valdi/no/such/path"));

    EXPECT_FALSE(missing.exists());
    EXPECT_FALSE(missing.isDir());
    EXPECT_FALSE(missing.isFile());
}

} // namespace ValdiTest
