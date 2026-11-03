#pragma once

#include "valdi/terminal/Sink/ITerminalSink.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Valdi::Terminal {

struct FrameWriterOptions {
    /** Append every frame instead of replacing the previous one. */
    bool debug = false;
};

/**
 * Writes frames to a sink the way log-update does: the previous frame is erased and redrawn in
 * place, static content is emitted once above it, and when the sink is not a terminal the live
 * frame is only written when the writer finishes. Frames of equal height rewrite only the rows
 * that changed.
 *
 * The erase and redraw pattern is the one used by log-update and ansi-escapes (MIT, Sindre Sorhus).
 */
class FrameWriter {
public:
    explicit FrameWriter(const Ref<ITerminalSink>& sink, FrameWriterOptions options = FrameWriterOptions());

    /** The most recent live frame, without trailing newline. */
    const std::vector<std::string>& currentFrame() const;

    void write(const std::vector<std::string>& frame, const std::vector<std::string>& staticLines);

    /** Erases the live frame. */
    void clear();

    /** Leaves the last frame on screen and restores the cursor. */
    void finish();

private:
    Ref<ITerminalSink> _sink;
    FrameWriterOptions _options;
    int _previousLineCount = 0;
    std::optional<std::string> _previousOutput;
    std::vector<std::string> _previousFrame;
    std::vector<std::string> _lastFrame;
    bool _cursorHidden = false;
    bool _finished = false;
};

} // namespace Valdi::Terminal
