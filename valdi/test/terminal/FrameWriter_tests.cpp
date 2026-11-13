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
// With a viewport height, a newline on the last visible row scrolls the oldest row into scrollback.
class MiniTerminal {
public:
    explicit MiniTerminal(int viewportHeight = 0) : _viewportHeight(viewportHeight) {}

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
                    _row = std::max(_top, _row - count);
                } else if (command == 'G') {
                    _column = 0;
                } else if (command == 'K') {
                    line(_row).clear();
                } else if (command == 'H') {
                    _row = _top;
                    _column = 0;
                } else if (command == 'J' && params == "2") {
                    for (size_t row = static_cast<size_t>(_top); row < _lines.size(); row++) {
                        _lines[row].clear();
                    }
                } else if (command == 'J' && params == "3") {
                    _lines.erase(_lines.begin(), _lines.begin() + _top);
                    _row -= _top;
                    _top = 0;
                }
            } else if (data[i] == '\n') {
                _row++;
                _column = 0;
                line(_row);
                if (_viewportHeight > 0 && _row >= _top + _viewportHeight) {
                    _top = _row - _viewportHeight + 1;
                }
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

    // Every row written so far including the cursor row, scrollback included.
    std::vector<std::string> allRows() const {
        auto rows = _lines;
        rows.resize(static_cast<size_t>(_row) + 1);
        return rows;
    }

    // The rows the user sees: the viewport, padded with empty rows.
    std::vector<std::string> viewport() const {
        std::vector<std::string> rows;
        for (int row = _top; row < _top + _viewportHeight; row++) {
            rows.push_back(static_cast<size_t>(row) < _lines.size() ? _lines[static_cast<size_t>(row)] : "");
        }
        return rows;
    }

    int cursorRow() const {
        return _row - _top;
    }

private:
    int _viewportHeight;
    std::vector<std::string> _lines;
    int _top = 0;
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

namespace {

// Feeds everything the sink received since the last call to the terminal.
class Screen {
public:
    Screen(const Ref<MemorySink>& sink, int rows) : _sink(sink), _terminal(rows) {}

    MiniTerminal& update() {
        for (; _consumed < _sink->writes().size(); _consumed++) {
            _terminal.feed(_sink->writes()[_consumed]);
        }
        return _terminal;
    }

private:
    Ref<MemorySink> _sink;
    MiniTerminal _terminal;
    size_t _consumed = 0;
};

} // namespace

TEST(TerminalFrameWriter, aFrameExactlyAsTallAsTheViewportKeepsItsFirstRowVisible) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 4);
    FrameWriter writer(sink);
    Screen screen(sink, 4);

    writer.write({"r0", "r1", "r2", "r3"}, {});
    ASSERT_EQ((Lines{"r0", "r1", "r2", "r3"}), screen.update().viewport());

    writer.write({"r0", "X", "r2", "r3"}, {});
    ASSERT_EQ((Lines{"r0", "X", "r2", "r3"}), screen.update().viewport());
    ASSERT_EQ(std::string::npos, sink->writes().back().find(Ansi::kClearTerminal));

    writer.write({"r0", "X", "r2", "Y"}, {});
    ASSERT_EQ((Lines{"r0", "X", "r2", "Y"}), screen.update().viewport());

    writer.write({"a", "b"}, {});
    ASSERT_EQ((Lines{"a", "b", "", ""}), screen.update().viewport());
}

TEST(TerminalFrameWriter, finishMovesTheCursorBelowAFrameThatFilledTheViewport) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 3);
    FrameWriter writer(sink);
    Screen screen(sink, 3);
    writer.write({"a", "b", "c"}, {});
    ASSERT_EQ(2, screen.update().cursorRow());
    writer.finish();
    ASSERT_EQ((Lines{"b", "c", ""}), screen.update().viewport());
}

TEST(TerminalFrameWriter, staticLinesWrittenBeforeATallFrameSurviveIt) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 3);
    FrameWriter writer(sink);
    Screen screen(sink, 3);

    writer.write({"live"}, {"log 1", "log 2"});
    writer.write({"live"}, {"log 3"});
    ASSERT_EQ((Lines{"log 1", "log 2", "log 3", "live"}), screen.update().screen());

    writer.write({"t0", "t1", "t2", "t3", "t4"}, {});
    ASSERT_EQ((Lines{"log 1", "log 2", "log 3", "t0", "t1", "t2", "t3", "t4"}), screen.update().allRows());
    ASSERT_EQ((Lines{"t2", "t3", "t4"}), screen.update().viewport());

    writer.write({"t0", "t1", "t2", "t3", "T4"}, {"log 4"});
    ASSERT_EQ((Lines{"log 1", "log 2", "log 3", "log 4", "t0", "t1", "t2", "t3", "T4"}), screen.update().allRows());
}

TEST(TerminalFrameWriter, shrinkingBackFromATallFrameRedrawsWithTheStaticLinesAbove) {
    auto sink = makeShared<MemorySink>();
    sink->setSize(80, 3);
    FrameWriter writer(sink);
    Screen screen(sink, 3);

    writer.write({"live"}, {"log 1"});
    writer.write({"t0", "t1", "t2", "t3", "t4"}, {});
    writer.write({"live"}, {});
    ASSERT_EQ((Lines{"log 1", "live", ""}), screen.update().viewport());
    ASSERT_EQ((Lines{"log 1", "live"}), screen.update().screen());
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
