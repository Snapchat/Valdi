#include "valdi/terminal/Sink/TtySink.hpp"

#include <cerrno>
#include <sys/ioctl.h>
#include <unistd.h>

namespace Valdi::Terminal {

TtySink::TtySink(int fd, std::optional<ColorLevel> colorLevelOverride)
    : _fd(fd), _isTTY(::isatty(fd) != 0), _colorLevel(ColorLevel::None) {
    _colorLevel = colorLevelOverride ? *colorLevelOverride : detectColorLevel(processEnvGetter(), _isTTY);
}

TtySink::~TtySink() = default;

Ref<TtySink> TtySink::makeForStdout() {
    return makeShared<TtySink>(STDOUT_FILENO);
}

void TtySink::write(std::string_view data) {
    while (!data.empty()) {
        const auto written = ::write(_fd, data.data(), data.size());
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        data.remove_prefix(static_cast<size_t>(written));
    }
}

bool TtySink::queryWindowSize(int& columns, int& rows) const {
    if (!_isTTY) {
        return false;
    }
    struct winsize size{};
    if (::ioctl(_fd, TIOCGWINSZ, &size) != 0 || size.ws_col == 0 || size.ws_row == 0) {
        return false;
    }
    columns = size.ws_col;
    rows = size.ws_row;
    return true;
}

int TtySink::columns() const {
    int columns = 0;
    int rows = 0;
    return queryWindowSize(columns, rows) ? columns : 0;
}

int TtySink::rows() const {
    int columns = 0;
    int rows = 0;
    return queryWindowSize(columns, rows) ? rows : 0;
}

bool TtySink::isTTY() const {
    return _isTTY;
}

ColorLevel TtySink::colorLevel() const {
    return _colorLevel;
}

} // namespace Valdi::Terminal
