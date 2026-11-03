#include "valdi/terminal/Sink/MemorySink.hpp"
#include "valdi/terminal/Sink/TtySink.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <thread>
#include <unistd.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

TEST(TerminalSink, memorySinkRecordsWritesInOrder) {
    MemorySink sink;
    sink.write("a");
    sink.write("bc");
    ASSERT_EQ((std::vector<std::string>{"a", "bc"}), sink.writes());
    ASSERT_EQ("abc", sink.output());
    sink.clearRecording();
    ASSERT_EQ("", sink.output());
}

TEST(TerminalSink, memorySinkSurvivesConcurrentWritesAndReads) {
    MemorySink sink;
    constexpr int kIterations = 2000;
    std::thread writer([&]() {
        for (int i = 0; i < kIterations; i++) {
            sink.write("x");
            FrameSnapshot frame;
            frame.plainLines = {"line"};
            sink.didRenderFrame(frame);
        }
    });
    for (int i = 0; i < kIterations; i++) {
        ASSERT_LE(sink.output().size(), static_cast<size_t>(kIterations));
        ASSERT_LE(sink.lastPlainLines().size(), 1u);
        ASSERT_LE(sink.writes().size(), static_cast<size_t>(kIterations));
        sink.setSize(80 + i % 3, 24);
    }
    writer.join();
    ASSERT_EQ(std::string(kIterations, 'x'), sink.output());
    ASSERT_EQ(static_cast<size_t>(kIterations), sink.frameCount());
}

TEST(TerminalSink, memorySinkReportsItsConfiguration) {
    MemorySink sink;
    ASSERT_EQ(80, sink.columns());
    ASSERT_EQ(24, sink.rows());
    ASSERT_TRUE(sink.isTTY());
    ASSERT_EQ(ColorLevel::None, sink.colorLevel());

    sink.setSize(100, 30);
    sink.setIsTTY(false);
    sink.setColorLevel(ColorLevel::TrueColor);
    ASSERT_EQ(100, sink.columns());
    ASSERT_EQ(30, sink.rows());
    ASSERT_FALSE(sink.isTTY());
    ASSERT_EQ(ColorLevel::TrueColor, sink.colorLevel());
}

TEST(TerminalSink, memorySinkKeepsTheLastFrame) {
    MemorySink sink;
    FrameSnapshot frame;
    frame.plainLines = {"a", "b"};
    frame.columns = 10;
    frame.height = 2;
    sink.didRenderFrame(frame);
    ASSERT_EQ((std::vector<std::string>{"a", "b"}), sink.lastPlainLines());
    ASSERT_EQ(1u, sink.frameCount());
}

TEST(TerminalSink, ttySinkWritesToTheFileDescriptor) {
    int fds[2];
    ASSERT_EQ(0, ::pipe(fds));
    {
        TtySink sink(fds[1]);
        ASSERT_FALSE(sink.isTTY());
        ASSERT_EQ(0, sink.columns());
        ASSERT_EQ(0, sink.rows());
        ASSERT_EQ(ColorLevel::None, sink.colorLevel());
        sink.write("hello\n");
    }
    ::close(fds[1]);

    char buffer[16] = {};
    const auto count = ::read(fds[0], buffer, sizeof(buffer));
    ::close(fds[0]);
    ASSERT_EQ(6, count);
    ASSERT_EQ("hello\n", std::string(buffer, static_cast<size_t>(count)));
}

TEST(TerminalSink, ttySinkWritesEverythingToANonBlockingDescriptorThatFillsUp) {
    int fds[2];
    ASSERT_EQ(0, ::pipe(fds));
    ASSERT_NE(-1, ::fcntl(fds[1], F_SETFL, ::fcntl(fds[1], F_GETFL) | O_NONBLOCK));

    // Far more than a pipe buffer holds, so the writer hits EAGAIN until the reader catches up.
    const std::string payload(4 * 1024 * 1024, 'x');
    std::string received;
    std::thread reader([&]() {
        char buffer[4096];
        while (true) {
            const auto count = ::read(fds[0], buffer, sizeof(buffer));
            if (count <= 0) {
                break;
            }
            received.append(buffer, static_cast<size_t>(count));
            if (received.size() < 65536) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });
    {
        TtySink sink(fds[1]);
        sink.write(payload);
    }
    ::close(fds[1]);
    reader.join();
    ::close(fds[0]);
    ASSERT_EQ(payload.size(), received.size());
    ASSERT_EQ(payload, received);
}

TEST(TerminalSink, ttySinkHonorsAnExplicitColorLevel) {
    int fds[2];
    ASSERT_EQ(0, ::pipe(fds));
    TtySink sink(fds[1], ColorLevel::Ansi256);
    ASSERT_EQ(ColorLevel::Ansi256, sink.colorLevel());
    ::close(fds[0]);
    ::close(fds[1]);
}

} // namespace ValdiTest
