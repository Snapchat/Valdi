#include "valdi/terminal/Paint/Painter.hpp"

#include "valdi/terminal/Classes/ITerminalViewClass.hpp"
#include "valdi/terminal/Paint/Borders.hpp"
#include "valdi/terminal/TerminalView.hpp"

#include <algorithm>
#include <cmath>

namespace Valdi::Terminal {

static int roundCell(double value) {
    return static_cast<int>(std::floor(value + 0.5));
}

Painter::Painter(CellGrid& target) : _target(target) {}

CellGrid& Painter::target() {
    return _target;
}

IntRect Painter::roundRect(const Valdi::Frame& frame, int originX, int originY) {
    const int x = roundCell(originX + static_cast<double>(frame.x));
    const int y = roundCell(originY + static_cast<double>(frame.y));
    const int right = roundCell(originX + static_cast<double>(frame.x) + frame.width);
    const int bottom = roundCell(originY + static_cast<double>(frame.y) + frame.height);
    return IntRect(x, y, std::max(0, right - x), std::max(0, bottom - y));
}

void Painter::paintRoot(TerminalView& root) {
    paintView(root, 0, 0, _target.bounds());
}

void Painter::paintView(TerminalView& view, int originX, int originY, const IntRect& clip) {
    const auto rect = roundRect(view.getFrame(), originX, originY);
    view.getViewClass()->paint(view, *this, PaintArea{rect, clip});
}

void Painter::paintChildren(
    TerminalView& view, const IntRect& rect, const IntRect& clip, bool clipToRect, int scrollX, int scrollY) {
    const auto childClip = clipToRect ? intersect(rect, clip) : clip;
    // Painting a child can detach views from the tree, so iterate a copy of the children.
    const auto children = view.getChildren();
    for (const auto& child : children) {
        paintView(*child, rect.x - scrollX, rect.y - scrollY, childClip);
    }
}

static Style sideStyle(const TerminalViewStyle& style, TerminalBorderSide side) {
    const auto& color =
        style.sideColors[static_cast<size_t>(side)] ? style.sideColors[static_cast<size_t>(side)] : style.borderColor;
    Style out;
    if (color) {
        out.fg = color;
    }
    out.dim = style.borderDim;
    return out;
}

static std::string repeat(std::string_view text, int count) {
    std::string out;
    for (int i = 0; i < count; i++) {
        out += text;
    }
    return out;
}

static void paintBorder(CellGrid& grid, const IntRect& rect, const TerminalViewStyle& style, const IntRect& clip) {
    if (!style.hasBorder() || rect.width == 0 || rect.height == 0) {
        return;
    }
    const auto* characters = resolveBorderStyle(style.borderStyle);
    if (characters == nullptr) {
        characters = resolveBorderStyle("single");
    }
    const bool top = style.sideEnabled[static_cast<size_t>(TerminalBorderSide::Top)];
    const bool bottom = style.sideEnabled[static_cast<size_t>(TerminalBorderSide::Bottom)];
    const bool left = style.sideEnabled[static_cast<size_t>(TerminalBorderSide::Left)];
    // A single column has no room for a separate right edge; the left edge is drawn alone.
    const bool right = style.sideEnabled[static_cast<size_t>(TerminalBorderSide::Right)] && rect.width > 1;
    const int x0 = rect.x;
    const int x1 = rect.x + rect.width - 1;
    const int y0 = rect.y;
    const int y1 = rect.y + rect.height - 1;
    const int innerWidth = std::max(0, rect.width - (left ? 1 : 0) - (right ? 1 : 0));

    if (top) {
        const auto line = std::string(left ? characters->topLeft : std::string_view()) +
                          repeat(characters->top, innerWidth) +
                          std::string(right ? characters->topRight : std::string_view());
        grid.writeText(x0, y0, line, sideStyle(style, TerminalBorderSide::Top), clip);
    }
    if (bottom && rect.height > 1) {
        const auto line = std::string(left ? characters->bottomLeft : std::string_view()) +
                          repeat(characters->bottom, innerWidth) +
                          std::string(right ? characters->bottomRight : std::string_view());
        grid.writeText(x0, y1, line, sideStyle(style, TerminalBorderSide::Bottom), clip);
    }
    const int firstRow = top ? y0 + 1 : y0;
    const int lastRow = bottom ? y1 - 1 : y1;
    if (left) {
        const auto sideLook = sideStyle(style, TerminalBorderSide::Left);
        for (int y = firstRow; y <= lastRow; y++) {
            grid.writeText(x0, y, characters->left, sideLook, clip);
        }
    }
    if (right) {
        const auto sideLook = sideStyle(style, TerminalBorderSide::Right);
        for (int y = firstRow; y <= lastRow; y++) {
            grid.writeText(x1, y, characters->right, sideLook, clip);
        }
    }
}

void Painter::paintBox(TerminalView& view, const PaintArea& area) {
    const auto& style = view.style();
    if (style.backgroundColor) {
        _target.fillBackground(area.rect, *style.backgroundColor, area.clip);
    }
    paintBorder(_target, area.rect, style, area.clip);
}

bool Painter::isPaintingStatic() const {
    return _paintingStatic;
}

void Painter::recordStatic(TerminalView& view) {
    _staticViews.push_back(&view);
}

const std::vector<TerminalView*>& Painter::staticViews() const {
    return _staticViews;
}

StaticSlice Painter::paintStaticSlice(TerminalView& staticView, size_t firstChild) {
    const auto children = staticView.getChildren();
    int top = 0;
    int bottom = 0;
    bool any = false;
    for (size_t i = firstChild; i < children.size(); i++) {
        const auto rect = roundRect(children[i]->getFrame(), 0, 0);
        top = any ? std::min(top, rect.y) : rect.y;
        bottom = any ? std::max(bottom, rect.y + rect.height) : rect.y + rect.height;
        any = true;
    }
    const int width = roundRect(staticView.getFrame(), 0, 0).width;
    if (!any) {
        return StaticSlice{CellGrid(width, 0), 0};
    }
    top = std::max(0, top);
    StaticSlice slice{CellGrid(width, std::max(0, bottom)), top};
    Painter painter(slice.grid);
    painter._paintingStatic = true;
    for (size_t i = firstChild; i < children.size(); i++) {
        painter.paintView(*children[i], 0, 0, slice.grid.bounds());
    }
    return slice;
}

} // namespace Valdi::Terminal
