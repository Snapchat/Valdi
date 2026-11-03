#include "valdi/terminal/Paint/CellGrid.hpp"

#include <algorithm>

namespace Valdi::Terminal {

CellGrid::CellGrid(int width, int height)
    : _width(std::max(0, width)), _height(std::max(0, height)), _slots(static_cast<size_t>(_width) * _height) {}

int CellGrid::width() const {
    return _width;
}

int CellGrid::height() const {
    return _height;
}

IntRect CellGrid::bounds() const {
    return IntRect(0, 0, _width, _height);
}

bool CellGrid::inGrid(int x, int y) const {
    return x >= 0 && x < _width && y >= 0 && y < _height;
}

bool CellGrid::writable(const IntRect& clip, int x, int y) const {
    return inGrid(x, y) && clip.contains(x, y);
}

CellGrid::Slot& CellGrid::slotAt(int x, int y) {
    return _slots[static_cast<size_t>(y) * _width + x];
}

const CellGrid::Slot& CellGrid::slotAt(int x, int y) const {
    return _slots[static_cast<size_t>(y) * _width + x];
}

void CellGrid::clearCell(int x, int y) {
    auto& slot = slotAt(x, y);
    if (!slot.occupied) {
        return;
    }
    // Overwriting half of a wide character leaves the other half as a blank cell.
    if (slot.cell.width == 2 && x + 1 < _width && slotAt(x + 1, y).occupied && slotAt(x + 1, y).cell.width == 0) {
        auto& next = slotAt(x + 1, y);
        next.cell = Cell{" ", 1, slot.cell.style};
    } else if (slot.cell.width == 0 && x > 0 && slotAt(x - 1, y).occupied && slotAt(x - 1, y).cell.width == 2) {
        auto& previous = slotAt(x - 1, y);
        previous.cell = Cell{" ", 1, previous.cell.style};
    }
    slot = Slot();
}

void CellGrid::putCluster(int x, int y, const Cluster& cluster, const Style& style, const IntRect& clip) {
    if (!writable(clip, x, y)) {
        return;
    }
    Style resolved = style;
    const auto& existing = slotAt(x, y);
    if (existing.occupied && existing.cell.style.bg && !style.bg) {
        Style inherited;
        inherited.bg = existing.cell.style.bg;
        resolved = Style::merge(inherited, style);
    }

    if (cluster.width == 2) {
        if (!writable(clip, x + 1, y)) {
            clearCell(x, y);
            auto& slot = slotAt(x, y);
            slot.occupied = true;
            slot.cell = Cell{" ", 1, resolved};
            return;
        }
        clearCell(x, y);
        clearCell(x + 1, y);
        auto& first = slotAt(x, y);
        first.occupied = true;
        first.cell = Cell{cluster.text, 2, resolved};
        auto& second = slotAt(x + 1, y);
        second.occupied = true;
        second.cell = Cell{"", 0, resolved};
        return;
    }
    clearCell(x, y);
    auto& slot = slotAt(x, y);
    slot.occupied = true;
    slot.cell = Cell{cluster.text, 1, resolved};
}

int CellGrid::writeClusters(
    int x, int y, const std::vector<Cluster>& clusters, const Style& style, const IntRect& clip) {
    int cursor = x;
    for (const auto& cluster : clusters) {
        if (cluster.width == 0) {
            if (writable(clip, cursor - 1, y)) {
                auto& slot = slotAt(cursor - 1, y);
                if (slot.occupied && slot.cell.width > 0) {
                    slot.cell.text += cluster.text;
                }
            }
            continue;
        }
        putCluster(cursor, y, cluster, style, clip);
        cursor += cluster.width;
    }
    return cursor;
}

int CellGrid::writeText(int x, int y, std::string_view text, const Style& style, const IntRect& clip) {
    return writeClusters(x, y, clusterize(text), style, clip);
}

int CellGrid::writeSegments(int x, int y, const std::vector<StyledSegment>& segments, const IntRect& clip) {
    int cursor = x;
    for (const auto& segment : segments) {
        cursor = writeText(cursor, y, segment.text, segment.style, clip);
    }
    return cursor;
}

void CellGrid::fillBackground(const IntRect& rect, const TermColor& color, const IntRect& clip) {
    const auto area = intersect(intersect(rect, clip), bounds());
    Style background;
    background.bg = color;
    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            auto& slot = slotAt(x, y);
            if (!slot.occupied) {
                slot.occupied = true;
                slot.cell = Cell{" ", 1, background};
            } else {
                slot.cell.style = Style::merge(background, slot.cell.style);
            }
        }
    }
}

std::string CellGrid::rowText(int y, int x, int width) const {
    std::string out;
    if (y < 0 || y >= _height) {
        return out;
    }
    for (int i = std::max(0, x); i < x + width && i < _width; i++) {
        const auto& slot = slotAt(i, y);
        if (!slot.occupied) {
            out += ' ';
        } else if (slot.cell.width > 0) {
            out += slot.cell.text;
        }
    }
    return out;
}

static bool isBlank(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    });
}

std::vector<std::string> CellGrid::toLines(ColorLevel level) const {
    std::vector<std::string> lines;
    lines.reserve(static_cast<size_t>(_height));
    for (int y = 0; y < _height; y++) {
        int lastContent = -1;
        for (int x = 0; x < _width; x++) {
            const auto& slot = slotAt(x, y);
            if (slot.occupied &&
                (!isBlank(slot.cell.text) || (level != ColorLevel::None && !slot.cell.style.isEmpty()))) {
                lastContent = x;
            }
        }

        std::string line;
        Style currentStyle;
        bool styled = false;
        for (int x = 0; x <= lastContent; x++) {
            const auto& slot = slotAt(x, y);
            if (slot.occupied && slot.cell.width == 0) {
                continue;
            }
            const Style style = slot.occupied ? slot.cell.style : Style();
            if (style != currentStyle) {
                if (styled) {
                    line += Ansi::kReset;
                    styled = false;
                }
                const auto sgr = styleToSgr(style, level);
                if (!sgr.empty()) {
                    line += sgr;
                    styled = true;
                }
                currentStyle = style;
            }
            line += slot.occupied ? slot.cell.text : std::string(" ");
        }
        if (styled) {
            line += Ansi::kReset;
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<std::string> CellGrid::toPlainLines() const {
    return toLines(ColorLevel::None);
}

} // namespace Valdi::Terminal
