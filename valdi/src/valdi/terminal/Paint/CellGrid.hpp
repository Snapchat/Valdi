#pragma once

#include "valdi/terminal/Paint/Ansi.hpp"
#include "valdi/terminal/Paint/Cell.hpp"
#include "valdi/terminal/Paint/Geometry.hpp"
#include "valdi/terminal/Paint/TextLayout.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Valdi::Terminal {

/**
 * A grid of terminal cells that views paint into. Serialized once per frame with toLines(), which
 * emits the minimal SGR changes along each row. All writes are clipped to both the grid and the
 * clip rectangle that is passed in.
 */
class CellGrid {
public:
    CellGrid(int width, int height);

    int width() const;
    int height() const;
    IntRect bounds() const;

    /** Writes clusters left to right starting at (x, y); zero-width clusters attach to the previous cell. */
    int writeClusters(int x, int y, const std::vector<Cluster>& clusters, const Style& style, const IntRect& clip);

    /** Returns the column after the last written cluster. */
    int writeText(int x, int y, std::string_view text, const Style& style, const IntRect& clip);

    int writeSegments(int x, int y, const std::vector<StyledSegment>& segments, const IntRect& clip);

    /** Applies a background color to every cell of `rect`, creating blank cells where needed. */
    void fillBackground(const IntRect& rect, const TermColor& color, const IntRect& clip);

    /** Plain text of a row span, with wide characters counted once and empty cells as spaces. */
    std::string rowText(int y, int x, int width) const;

    /** Serializes each row to a string with ANSI styling. Trailing unstyled blanks are trimmed. */
    std::vector<std::string> toLines(ColorLevel level) const;

    /** Like toLines() without any styling. */
    std::vector<std::string> toPlainLines() const;

private:
    struct Slot {
        bool occupied = false;
        Cell cell;
    };

    int _width;
    int _height;
    std::vector<Slot> _slots;

    Slot& slotAt(int x, int y);
    const Slot& slotAt(int x, int y) const;
    bool inGrid(int x, int y) const;
    bool writable(const IntRect& clip, int x, int y) const;
    void clearCell(int x, int y);
    void putCluster(int x, int y, const Cluster& cluster, const Style& style, const IntRect& clip);
};

} // namespace Valdi::Terminal
