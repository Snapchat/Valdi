#pragma once

#include "valdi/terminal/Classes/ITerminalViewClass.hpp"

namespace Valdi {

/**
 * Content that is written once above the live frame and then left in the terminal scrollback,
 * like a log. The live frame skips the subtree; TerminalRoot emits each child exactly once. The
 * view should be absolutely positioned so that it does not take up space in the live frame.
 * Children are append-only: emission tracks how many children were written, so replacing,
 * reordering or removing an already written child is never reflected in the output.
 */
class StaticClass : public ITerminalViewClass {
public:
    explicit StaticClass(const Ref<ITerminalViewClass>& parentClass);
    ~StaticClass() override;

    void paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) override;
};

} // namespace Valdi
