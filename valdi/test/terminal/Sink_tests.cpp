#include "valdi/terminal/Sink/MemorySink.hpp"
#include "valdi/terminal/Sink/TtySink.hpp"

#include <gtest/gtest.h>
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

TEST(TerminalSink, ttySinkHonorsAnExplicitColorLevel) {
    int fds[2];
    ASSERT_EQ(0, ::pipe(fds));
    TtySink sink(fds[1], ColorLevel::Ansi256);
    ASSERT_EQ(ColorLevel::Ansi256, sink.colorLevel());
    ::close(fds[0]);
    ::close(fds[1]);
}

} // namespace ValdiTest
