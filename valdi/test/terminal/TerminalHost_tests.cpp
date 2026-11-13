#include "valdi/terminal/Host/HopReleasedRef.hpp"
#include "valdi/terminal/Host/TerminalHost.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

#include <gtest/gtest.h>
#include <string>
#include <thread>

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

TEST(TerminalHost, rawModeNeedsATerminalOnBothStdinAndStdout) {
    EXPECT_TRUE(TerminalHost::rawModeAllowed(true, true));
    EXPECT_FALSE(TerminalHost::rawModeAllowed(true, false));
    EXPECT_FALSE(TerminalHost::rawModeAllowed(false, true));
    EXPECT_FALSE(TerminalHost::rawModeAllowed(false, false));
}

namespace {

class DestructionProbe : public SharedPtrRefCountable {
public:
    explicit DestructionProbe(std::thread::id* destroyedOn) : _destroyedOn(destroyedOn) {}

    ~DestructionProbe() override {
        *_destroyedOn = std::this_thread::get_id();
    }

private:
    std::thread::id* _destroyedOn;
};

std::thread* gReleaseThread = nullptr;

// Stands in for the JS thread: takes the reference to another thread, where it is dropped.
void releaseOnWorkerThread(Ref<DestructionProbe> ref) {
    gReleaseThread = new std::thread([ref = std::move(ref)]() mutable { ref = nullptr; });
}

void releaseInPlace(Ref<DestructionProbe> /*ref*/) {}

} // namespace

TEST(TerminalHost, lastReleaseOfAHopReleasedRefHappensOnTheReleaseThread) {
    std::thread::id destroyedOn;
    auto owner = makeShared<DestructionProbe>(&destroyedOn);
    {
        HopReleasedRef<DestructionProbe> held(owner, &releaseOnWorkerThread);
        // The owner lets go while the dispatching thread still holds its reference.
        owner = nullptr;
        ASSERT_EQ(std::thread::id(), destroyedOn);
    }
    ASSERT_NE(nullptr, gReleaseThread);
    const auto workerId = gReleaseThread->get_id();
    gReleaseThread->join();
    delete gReleaseThread;
    gReleaseThread = nullptr;

    EXPECT_NE(std::thread::id(), destroyedOn);
    EXPECT_EQ(workerId, destroyedOn);
    EXPECT_NE(std::this_thread::get_id(), destroyedOn);
}

TEST(TerminalHost, hopReleasedRefDoesNotReleaseAMovedFromReference) {
    std::thread::id destroyedOn;
    auto owner = makeShared<DestructionProbe>(&destroyedOn);
    {
        HopReleasedRef<DestructionProbe> first(owner, &releaseOnWorkerThread);
        auto second = std::move(first);
        EXPECT_EQ(owner.get(), second.operator->());
    }
    ASSERT_NE(nullptr, gReleaseThread);
    gReleaseThread->join();
    delete gReleaseThread;
    gReleaseThread = nullptr;
    // The owner still holds the probe, so the hop released only the held reference.
    EXPECT_EQ(std::thread::id(), destroyedOn);
    owner = nullptr;
    EXPECT_EQ(std::this_thread::get_id(), destroyedOn);
}

TEST(TerminalHost, hopReleasedRefMayDropInPlaceWhenThereIsNowhereToHop) {
    std::thread::id destroyedOn;
    {
        HopReleasedRef<DestructionProbe> held(makeShared<DestructionProbe>(&destroyedOn), &releaseInPlace);
    }
    EXPECT_EQ(std::this_thread::get_id(), destroyedOn);
}

} // namespace ValdiTest
