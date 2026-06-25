//
//  LinuxRuntime_tests.cpp
//  valdi-linux
//
//  Unit tests for the Linux Valdi runtime: platform type reporting and
//  StandaloneViewManager construction with PlatformTypeLinux.
//

#include "valdi/standalone_runtime/StandaloneViewManager.hpp"
#include "valdi_core/cpp/Context/PlatformType.hpp"
#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

TEST(LinuxRuntime, StandaloneViewManagerReportsLinuxPlatformType) {
    StandaloneViewManager viewManager(PlatformTypeLinux);
    EXPECT_EQ(viewManager.getPlatformType(), PlatformTypeLinux);
}

TEST(LinuxRuntime, StandaloneViewManagerDefaultIsIOS) {
    StandaloneViewManager viewManager;
    EXPECT_EQ(viewManager.getPlatformType(), PlatformTypeIOS);
}

TEST(LinuxRuntime, PlatformTypeLinuxValue) {
    // PlatformTypeLinux must be 4 — JavaScriptRuntime returns this value to JS
    // and the TypeScript side checks `getCurrentPlatform() === 5` is never 4.
    EXPECT_EQ(static_cast<int>(PlatformTypeLinux), 4);
}

TEST(LinuxRuntime, PlatformTypeOrdering) {
    EXPECT_EQ(static_cast<int>(PlatformTypeAndroid), 0);
    EXPECT_EQ(static_cast<int>(PlatformTypeIOS), 1);
    EXPECT_EQ(static_cast<int>(PlatformTypeMacOS), 2);
    EXPECT_EQ(static_cast<int>(PlatformTypeWeb), 3);
    EXPECT_EQ(static_cast<int>(PlatformTypeLinux), 4);
}

} // namespace ValdiTest
