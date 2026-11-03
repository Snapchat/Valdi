#pragma once

#include "valdi/terminal/Sink/ITerminalSink.hpp"

#include <optional>

namespace Valdi::Terminal {

/**
 * A sink that writes to a file descriptor, normally stdout. It queries the terminal size on demand
 * and never changes terminal modes or installs signal handlers.
 */
class TtySink : public ITerminalSink {
public:
    explicit TtySink(int fd, std::optional<ColorLevel> colorLevelOverride = std::nullopt);
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

    bool queryWindowSize(int& columns, int& rows) const;
};

} // namespace Valdi::Terminal
