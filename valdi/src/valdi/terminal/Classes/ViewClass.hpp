#pragma once

#include "valdi/terminal/Classes/ITerminalViewClass.hpp"

namespace Valdi {

/** The base class of every terminal view: background color, border and children. */
class ViewClass : public ITerminalViewClass {
public:
    ViewClass();
    ~ViewClass() override;

    void bindAttributes(AttributesBindingContext& binder) override;
    void paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) override;
};

} // namespace Valdi
