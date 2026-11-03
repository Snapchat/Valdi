#include "valdi/terminal/Host/TerminalHost.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

TEST(TerminalHost, hostPlatformTypeIsADesktopPlatform) {
    const auto platform = hostPlatformType();
    EXPECT_TRUE(platform == PlatformTypeMacOS || platform == PlatformTypeLinux);
}

TEST(TerminalHost, completeUtf8LengthKeepsWholeSequences) {
    EXPECT_EQ(0u, TerminalHost::completeUtf8Length(""));
    EXPECT_EQ(3u, TerminalHost::completeUtf8Length("abc"));
    // U+00E9 is two bytes, U+20AC three, U+1F600 four.
    EXPECT_EQ(2u, TerminalHost::completeUtf8Length("\xC3\xA9"));
    EXPECT_EQ(3u, TerminalHost::completeUtf8Length("\xE2\x82\xAC"));
    EXPECT_EQ(4u, TerminalHost::completeUtf8Length("\xF0\x9F\x98\x80"));
}

TEST(TerminalHost, completeUtf8LengthHoldsBackASplitSequence) {
    EXPECT_EQ(1u, TerminalHost::completeUtf8Length("a\xC3"));
    EXPECT_EQ(1u, TerminalHost::completeUtf8Length("a\xE2\x82"));
    EXPECT_EQ(1u, TerminalHost::completeUtf8Length("a\xF0\x9F\x98"));
    EXPECT_EQ(0u, TerminalHost::completeUtf8Length("\xF0"));
}

} // namespace ValdiTest
