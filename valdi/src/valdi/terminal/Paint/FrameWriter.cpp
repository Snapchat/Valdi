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

    std::string text;
    const int rows = _sink->rows();
    if (rows > 0 && static_cast<int>(frame.size()) >= rows) {
        // Taller than the viewport: erasing by line count would scroll, so clear and repaint.
        text += Ansi::kClearTerminal;
        text += staticOutput;
        text += output + "\n";
    } else if (staticOutput.empty() && _previousOutput && !frame.empty() && frame.size() == _previousFrame.size()) {
        text += Ansi::cursorUp(static_cast<int>(frame.size()));
        text += Ansi::kCursorLeft;
        for (size_t i = 0; i < frame.size(); i++) {
            if (frame[i] != _previousFrame[i]) {
                text += Ansi::kEraseLine;
                text += frame[i];
            }
            text += '\n';
        }
    } else {
        text += Ansi::eraseLines(_previousLineCount);
        text += staticOutput;
        text += output + "\n";
    }
    _sink->write(text);
    _previousOutput = output;
    _previousFrame = frame;
    _previousLineCount = static_cast<int>(frame.size()) + 1;
}

void FrameWriter::clear() {
    if (_finished || !_sink->isTTY() || _options.debug) {
        return;
    }
    _sink->write(Ansi::eraseLines(_previousLineCount));
    _previousLineCount = 0;
    _previousOutput.reset();
    _previousFrame.clear();
}

void FrameWriter::finish() {
    if (_finished) {
        return;
    }
    _finished = true;
    if (!_sink->isTTY() && !_options.debug && !_lastFrame.empty()) {
        _sink->write(join(_lastFrame) + "\n");
    }
    if (_cursorHidden) {
        _sink->write(Ansi::kCursorShow);
        _cursorHidden = false;
    }
}

} // namespace Valdi::Terminal
