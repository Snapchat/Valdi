#include "valdi/terminal/Paint/FrameWriter.hpp"
#include "valdi/terminal/Sink/MemorySink.hpp"

#include <gtest/gtest.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

using Lines = std::vector<std::string>;

static std::string str(std::string_view view) {
    return std::string(view);
}

TEST(TerminalFrameWriter, redrawsFramesInPlaceOnATty) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 10);
    FrameWriter writer(sink);
    writer.write({"a", "b"}, {});
    ASSERT_EQ((Lines{str(Ansi::kCursorHide), "a\nb\n"}), sink->writes());
    writer.write({"c"}, {});
    ASSERT_EQ(Ansi::eraseLines(3) + "c\n", sink->writes()[2]);
    writer.finish();
    ASSERT_EQ(str(Ansi::kCursorShow), sink->writes().back());
}

TEST(TerminalFrameWriter, skipsIdenticalFrames) {
    auto sink = makeShared<MemorySink>();
    FrameWriter writer(sink);
    writer.write({"same"}, {});
    writer.write({"same"}, {});
    ASSERT_EQ(2u, sink->writes().size());
}

TEST(TerminalFrameWriter, rewritesOnlyChangedRowsWhenTheHeightIsUnchanged) {
    auto sink = makeShared<MemorySink>();
    FrameWriter writer(sink);
    writer.write({"a", "b", "c"}, {});
    writer.write({"a", "X", "c"}, {});
    ASSERT_EQ(3u, sink->writes().size());
    ASSERT_EQ(Ansi::cursorUp(3) + str(Ansi::kCursorLeft) + "\n" + str(Ansi::kEraseLine) + "X\n" + "\n",
              sink->writes()[2]);
}

TEST(TerminalFrameWriter, writesStaticLinesAboveTheFrameOnce) {
    auto sink = makeShared<MemorySink>();
    FrameWriter writer(sink);
    writer.write({"live"}, {"log 1"});
    ASSERT_EQ("log 1\nlive\n", sink->writes()[1]);
    writer.write({"live"}, {"log 2"});
    ASSERT_EQ(Ansi::eraseLines(2) + "log 2\nlive\n", sink->writes()[2]);
}

TEST(TerminalFrameWriter, clearsTheTerminalForFramesTallerThanTheViewport) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 2);
    FrameWriter writer(sink);
    writer.write({"1", "2", "3"}, {});
    ASSERT_EQ(0u, sink->writes()[1].find(Ansi::kClearTerminal));
}

TEST(TerminalFrameWriter, defersTheLiveFrameUntilFinishWhenTheSinkIsNotATty) {
    auto sink = makeShared<MemorySink>();
    sink->setIsTTY(false);
    FrameWriter writer(sink);
    writer.write({"first"}, {"static"});
    writer.write({"second"}, {});
    ASSERT_EQ("static\n", sink->output());
    writer.finish();
    ASSERT_EQ("static\nsecond\n", sink->output());
}

TEST(TerminalFrameWriter, appendsEveryFrameInDebugMode) {
    auto sink = makeShared<MemorySink>();
    FrameWriterOptions options;
    options.debug = true;
    FrameWriter writer(sink, options);
    writer.write({"a"}, {});
    writer.write({"b"}, {});
    ASSERT_EQ("a\nb\n", sink->output());
}

TEST(TerminalFrameWriter, clearErasesTheLiveFrame) {
    auto sink = makeShared<MemorySink>();
    FrameWriter writer(sink);
    writer.write({"a", "b"}, {});
    writer.clear();
    ASSERT_EQ(Ansi::eraseLines(3), sink->writes().back());
    writer.write({"a", "b"}, {});
    ASSERT_EQ("a\nb\n", sink->writes().back());
}

} // namespace ValdiTest
