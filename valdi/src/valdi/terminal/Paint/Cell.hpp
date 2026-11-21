#pragma once

#include "valdi/terminal/Paint/Style.hpp"

#include <string>

namespace Valdi::Terminal {

/**
 * One terminal cell. A wide character occupies two cells: the first holds the text and has width 2,
 * the second is a continuation with width 0 and no text.
 */
struct Cell {
    std::string text;
    int width = 1;
    Style style;
};

} // namespace Valdi::Terminal
