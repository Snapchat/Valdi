#pragma once

#include "valdi/terminal/Sink/ITerminalSink.hpp"

#include <atomic>
#include <chrono>
#include <optional>

namespace Valdi::Terminal {

/**
 * A sink that writes to a file descriptor, normally stdout. It queries the terminal size on demand
 * and never changes terminal modes or installs signal handlers. On a non-blocking descriptor that stays
 * full (a terminal stopped with Ctrl+S, a reader that stopped reading) a write gives up after a deadline
 * and drops the rest; a blocking descriptor blocks in write() as usual.
 */
class TtySink : public ITerminalSink {
public:
    /**
     * `writeDeadline` bounds how long one write waits for a full non-blocking descriptor. The root holds its
     * mutex while writing, so this is also how long a stopped terminal can hold up the root.
     */
    explicit TtySink(int fd,
                     std::optional<ColorLevel> colorLevelOverride = std::nullopt,
                     std::chrono::milliseconds writeDeadline = std::chrono::milliseconds(2000));
    ~TtySink() override;

    /** A sink for the process stdout. */
    static Ref<TtySink> makeForStdout();

    void write(std::string_view data) override;
    int columns() const override;
    int rows() const override;
    bool isTTY() const override;
    ColorLevel colorLevel() const override;

private:
    int _fd;
    bool _isTTY;
    ColorLevel _colorLevel;
    std::chrono::milliseconds _writeDeadline;
    std::atomic<bool> _stalled{false};

    bool queryWindowSize(int& columns, int& rows) const;
};

} // namespace Valdi::Terminal
