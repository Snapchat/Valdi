#pragma once

#include "valdi/terminal/Paint/CellGrid.hpp"
#include "valdi/terminal/Paint/Geometry.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"

#include <vector>

namespace Valdi {
class TerminalView;
}

namespace Valdi::Terminal {

/** Where a view is drawn: its frame in absolute cell coordinates and the region it may draw into. */
struct PaintArea {
    IntRect rect;
    IntRect clip;
};

/** Cells of the children of a Static view that have not been written above the live frame yet. */
struct StaticSlice {
    CellGrid grid;
    /** First row of `grid` that belongs to the slice. */
    int firstRow = 0;
};

/**
 * Walks a TerminalView tree and paints it into a CellGrid. Each view paints itself through its
 * view class; the painter provides the traversal, clipping and the shared box and border drawing.
 */
class Painter {
public:
    explicit Painter(CellGrid& target);

    CellGrid& target();

    /** Paints `root` with its frame relative to the grid origin. */
    void paintRoot(TerminalView& root);

    /** Paints a view whose parent is at (originX, originY), restricted to `clip`. */
    void paintView(TerminalView& view, int originX, int originY, const IntRect& clip);

    /**
     * Paints the children of `view`. `rect` is the view's absolute rectangle; the children are
     * translated by -(scrollX, scrollY) and clipped to `rect` when `clipToRect` is set.
     */
    void paintChildren(TerminalView& view,
                       const IntRect& rect,
                       const IntRect& clip,
                       bool clipToRect,
                       int scrollX = 0,
                       int scrollY = 0);

    /** Draws the background and border of a view. */
    void paintBox(TerminalView& view, const PaintArea& area);

    /** Whether this painter renders the content of a Static view rather than the live frame. */
    bool isPaintingStatic() const;

    /** Called by Static views met while painting the live frame, in document order. */
    void recordStatic(TerminalView& view);
    const std::vector<TerminalView*>& staticViews() const;

    /** Renders children [firstChild, n) of a Static view at their laid out frames. */
    static StaticSlice paintStaticSlice(TerminalView& staticView, size_t firstChild);

    /** Rounds a frame in the parent's coordinate space to whole cells, in absolute coordinates. */
    static IntRect roundRect(const Valdi::Frame& frame, int originX, int originY);

private:
    CellGrid& _target;
    bool _paintingStatic = false;
    std::vector<TerminalView*> _staticViews;
};

} // namespace Valdi::Terminal
