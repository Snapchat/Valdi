#include "valdi/terminal/Classes/ScrollClass.hpp"

#include "valdi/terminal/Paint/Painter.hpp"

#include <cmath>

namespace Valdi {

ScrollClass::ScrollClass(const Ref<ITerminalViewClass>& parentClass)
    : ITerminalViewClass("SCValdiScrollView", "com.snap.valdi.views.ValdiScrollView", parentClass, false) {}

ScrollClass::~ScrollClass() = default;

void ScrollClass::bindAttributes(AttributesBindingContext& binder) {
    binder.bindScrollAttributes();
}

void ScrollClass::paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) {
    painter.paintBox(view, area);
    const auto& offset = view.getScrollOffset();
    painter.paintChildren(view,
                          area.rect,
                          area.clip,
                          true,
                          static_cast<int>(std::floor(offset.x + 0.5f)),
                          static_cast<int>(std::floor(offset.y + 0.5f)));
}

} // namespace Valdi
