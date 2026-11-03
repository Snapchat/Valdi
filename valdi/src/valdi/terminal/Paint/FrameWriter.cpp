#include "valdi/terminal/Paint/FrameWriter.hpp"
#include "valdi/terminal/Paint/Ansi.hpp"

namespace Valdi::Terminal {

static std::string join(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

FrameWriter::FrameWriter(const Ref<ITerminalSink>& sink, FrameWriterOptions options) : _sink(sink), _options(options) {}

const std::vector<std::string>& FrameWriter::currentFrame() const {
    return _lastFrame;
}

void FrameWriter::write(const std::vector<std::string>& frame, const std::vector<std::string>& staticLines) {
    if (_finished) {
        return;
    }
    _lastFrame = frame;
    const std::string staticOutput = staticLines.empty() ? std::string() : join(staticLines) + "\n";
    const std::string output = join(frame);

    if (_options.debug) {
        _sink->write(staticOutput + output + "\n");
        return;
    }

    if (!_sink->isTTY()) {
        if (!staticOutput.empty()) {
            _sink->write(staticOutput);
        }
        return;
    }

    if (!_cursorHidden) {
        _sink->write(Ansi::kCursorHide);
        _cursorHidden = true;
    }

    if (staticOutput.empty() && _previousOutput && output == *_previousOutput) {
        return;
    }

    _fullStaticOutput += staticOutput;

    const int rows = _sink->rows();
    const int lineCount = static_cast<int>(frame.size());
    const bool tall = rows > 0 && lineCount > rows;
    // A frame that fills the viewport has no row left below it for the cursor; a trailing newline would
    // scroll the first row away.
    const bool trailingNewline = rows <= 0 || lineCount < rows;

    std::string text;
    if (tall || _previousTall) {
        // Erasing by line count would scroll (tall) or leave the tail of the previous frame on screen
        // (shrinking back), so clear everything and redraw. Static output is replayed because the
        // clear removes scrollback too.
        text += Ansi::kClearTerminal;
        text += _fullStaticOutput;
        text += output;
        if (trailingNewline) {
            text += '\n';
        }
    } else if (staticOutput.empty() && _previousOutput && !frame.empty() && frame.size() == _previousFrame.size() &&
               trailingNewline == _previousTrailingNewline) {
        text += Ansi::cursorUp(trailingNewline ? lineCount : lineCount - 1);
        text += Ansi::kCursorLeft;
        for (size_t i = 0; i < frame.size(); i++) {
            if (frame[i] != _previousFrame[i]) {
                text += Ansi::kEraseLine;
                text += frame[i];
            }
            if (i + 1 < frame.size() || trailingNewline) {
                text += '\n';
            }
        }
    } else {
        text += Ansi::eraseLines(_previousLineCount);
        text += staticOutput;
        text += output;
        if (trailingNewline) {
            text += '\n';
        }
    }
    _sink->write(text);
    _previousOutput = output;
    _previousFrame = frame;
    _previousTall = tall;
    _previousTrailingNewline = trailingNewline;
    _previousLineCount = lineCount + (trailingNewline ? 1 : 0);
}

void FrameWriter::clear() {
    if (_finished || !_sink->isTTY() || _options.debug) {
        return;
    }
    _sink->write(Ansi::eraseLines(_previousLineCount));
    _previousLineCount = 0;
    _previousOutput.reset();
    _previousFrame.clear();
    _previousTall = false;
}

void FrameWriter::finish() {
    if (_finished) {
        return;
    }
    _finished = true;
    if (!_sink->isTTY() && !_options.debug && !_lastFrame.empty()) {
        _sink->write(join(_lastFrame) + "\n");
    }
    if (_sink->isTTY() && !_options.debug && _previousOutput && !_previousTrailingNewline) {
        // The last frame ended without a newline so the cursor would otherwise stay on its final row.
        _sink->write("\n");
    }
    if (_cursorHidden) {
        _sink->write(Ansi::kCursorShow);
        _cursorHidden = false;
    }
}

} // namespace Valdi::Terminal
