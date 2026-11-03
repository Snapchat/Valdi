#include "valdi/terminal/Paint/FrameWriter.hpp"
#include "valdi/terminal/Sink/MemorySink.hpp"

#include <algorithm>
#include <cctype>
#include <gtest/gtest.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

using Lines = std::vector<std::string>;

static std::string str(std::string_view view) {
    return std::string(view);
}

namespace {

// Interprets the handful of escape sequences the writer emits, to check what a screen would show.
class MiniTerminal {
public:
    void feed(const std::string& data) {
        size_t i = 0;
        while (i < data.size()) {
            if (data[i] == '\x1b' && i + 1 < data.size() && data[i + 1] == '[') {
                i += 2;
                std::string params;
                while (i < data.size() && (std::isdigit(static_cast<unsigned char>(data[i])) || data[i] == '?')) {
                    params += data[i++];
                }
                const char command = data[i++];
                const int count = params.empty() || params[0] == '?' ? 1 : std::stoi(params);
                if (command == 'A') {
                    _row = std::max(0, _row - count);
                } else if (command == 'G') {
                    _column = 0;
                } else if (command == 'K') {
                    line(_row).clear();
                } else if (command == 'J' && params == "2") {
                    _lines.clear();
                    _row = 0;
                    _column = 0;
                }
            } else if (data[i] == '\n') {
                _row++;
                _column = 0;
                line(_row);
                i++;
            } else {
                auto& target = line(_row);
                if (target.size() < static_cast<size_t>(_column) + 1) {
                    target.resize(static_cast<size_t>(_column) + 1, ' ');
                }
                target[static_cast<size_t>(_column++)] = data[i++];
            }
        }
    }

    // Rows above the cursor line, which is where the next write continues.
    std::vector<std::string> screen() const {
        auto rows = _lines;
        rows.resize(static_cast<size_t>(_row));
        return rows;
    }

private:
    std::vector<std::string> _lines;
    int _row = 0;
    int _column = 0;

    std::string& line(int row) {
        if (_lines.size() <= static_cast<size_t>(row)) {
            _lines.resize(static_cast<size_t>(row) + 1);
        }
        return _lines[static_cast<size_t>(row)];
    }
};

} // namespace

TEST(TerminalFrameWriter, theScreenAlwaysShowsTheLatestFrameBelowTheStaticLines) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 20);
    FrameWriter writer(sink);
    MiniTerminal terminal;
    size_t consumed = 0;
    const auto update = [&]() {
        for (; consumed < sink->writes().size(); consumed++) {
            terminal.feed(sink->writes()[consumed]);
        }
    };

    writer.write({"a", "b", "c"}, {});
    update();
    ASSERT_EQ((Lines{"a", "b", "c"}), terminal.screen());

    writer.write({"a", "X", "c"}, {});
    update();
    ASSERT_EQ((Lines{"a", "X", "c"}), terminal.screen());

    writer.write({"a", "X"}, {});
    update();
    ASSERT_EQ((Lines{"a", "X"}), terminal.screen());

    writer.write({"1", "2", "3", "4"}, {"log 1"});
    update();
    ASSERT_EQ((Lines{"log 1", "1", "2", "3", "4"}), terminal.screen());

    writer.write({"1", "two", "3", "4"}, {});
    update();
    ASSERT_EQ((Lines{"log 1", "1", "two", "3", "4"}), terminal.screen());

    writer.write({"only"}, {"log 2", "log 3"});
    update();
    ASSERT_EQ((Lines{"log 1", "log 2", "log 3", "only"}), terminal.screen());

    writer.write({"only", "more"}, {});
    update();
    ASSERT_EQ((Lines{"log 1", "log 2", "log 3", "only", "more"}), terminal.screen());
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
