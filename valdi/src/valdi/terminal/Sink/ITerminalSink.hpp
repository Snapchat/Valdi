#pragma once

#include "valdi/terminal/Paint/Ansi.hpp"

#include "valdi_core/cpp/Utils/Shared.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Valdi::Terminal {

/** The content of a rendered frame, delivered to the sink and to frame observers. */
struct FrameSnapshot {
    /** Live region rows with no styling, trailing blanks trimmed. */
    std::vector<std::string> plainLines;
    /** Live region rows with ANSI styling at the sink's color level. */
    std::vector<std::string> ansiLines;
    /** ansiLines joined with newlines. */
    std::string ansi;
    int columns = 0;
    int height = 0;
};

/**
 * Destination for rendered terminal output. Implementations only move bytes; they do not own
 * input handling, raw mode or signals.
 */
class ITerminalSink : public SimpleRefCountable {
public:
    ~ITerminalSink() override = default;

    virtual void write(std::string_view data) = 0;
    virtual void flush() {}

    /** Width in columns, or 0 when unknown. */
    virtual int columns() const {
        return 0;
    }

    /** Height in rows, or 0 when unknown. */
    virtual int rows() const {
        return 0;
    }

    /** Whether the output is an interactive terminal that supports cursor movement. */
    virtual bool isTTY() const = 0;

    virtual ColorLevel colorLevel() const = 0;

    /** Called after each frame has been written. */
    virtual void didRenderFrame(const FrameSnapshot& /*frame*/) {}
};

} // namespace Valdi::Terminal
