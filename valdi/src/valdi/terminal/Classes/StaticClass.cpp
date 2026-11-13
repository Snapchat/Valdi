#include "valdi/terminal/Classes/StaticClass.hpp"

#include "valdi/terminal/Paint/Painter.hpp"

namespace Valdi {

StaticClass::StaticClass(const Ref<ITerminalViewClass>& parentClass)
    : ITerminalViewClass("ValdiTerminalStatic", "ValdiTerminalStatic", parentClass, false) {}

StaticClass::~StaticClass() = default;

void StaticClass::paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) {
    if (painter.isPaintingStatic()) {
        // Nested static content belongs to the enclosing static block.
        painter.paintChildren(view, area.rect, area.clip, false);
        return;
    }
    painter.recordStatic(view);
}

} // namespace Valdi
