#include "valdi/terminal/Paint/Borders.hpp"

namespace Valdi::Terminal {

static const BorderCharacters kSingle = {"┌", "─", "┐", "│", "┘", "─", "└", "│"};
static const BorderCharacters kDouble = {"╔", "═", "╗", "║", "╝", "═", "╚", "║"};
static const BorderCharacters kRound = {"╭", "─", "╮", "│", "╯", "─", "╰", "│"};
static const BorderCharacters kBold = {"┏", "━", "┓", "┃", "┛", "━", "┗", "┃"};
static const BorderCharacters kSingleDouble = {"╓", "─", "╖", "║", "╜", "─", "╙", "║"};
static const BorderCharacters kDoubleSingle = {"╒", "═", "╕", "│", "╛", "═", "╘", "│"};
static const BorderCharacters kClassic = {"+", "-", "+", "|", "+", "-", "+", "|"};
static const BorderCharacters kArrow = {"↘", "↓", "↙", "←", "↖", "↑", "↗", "→"};

const BorderCharacters* resolveBorderStyle(std::string_view name) {
    if (name == "single") {
        return &kSingle;
    }
    if (name == "double") {
        return &kDouble;
    }
    if (name == "round") {
        return &kRound;
    }
    if (name == "bold") {
        return &kBold;
    }
    if (name == "singleDouble") {
        return &kSingleDouble;
    }
    if (name == "doubleSingle") {
        return &kDoubleSingle;
    }
    if (name == "classic") {
        return &kClassic;
    }
    if (name == "arrow") {
        return &kArrow;
    }
    return nullptr;
}

} // namespace Valdi::Terminal
