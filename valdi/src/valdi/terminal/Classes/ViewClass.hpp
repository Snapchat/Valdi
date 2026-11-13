#pragma once

#include "valdi/terminal/Classes/ITerminalViewClass.hpp"

namespace Valdi {

/**
 * The base class of every terminal view: background color, border and children.
 *
 * Borders are paint-only, as with SnapDrawing: they are drawn over the outermost cells of the
 * frame and do not inset children. A component that wants its content inside the border adds
 * padding itself, as the `Box` component does. `border` accepts `[width, color]` (a "1 solid red"
 * string is turned into that by the attribute preprocessor); `borderWidth`, `borderColor`,
 * `borderStyle` and the per-side flags and colors can be set individually.
 */
class ViewClass : public ITerminalViewClass {
public:
    ViewClass();
    ~ViewClass() override;

    void bindAttributes(AttributesBindingContext& binder) override;
    void paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) override;
};

} // namespace Valdi
