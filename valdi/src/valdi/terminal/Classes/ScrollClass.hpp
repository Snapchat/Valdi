#pragma once

#include "valdi/terminal/Classes/ITerminalViewClass.hpp"

namespace Valdi {

/** A view whose children are clipped to its frame and shifted by the scroll offset. */
class ScrollClass : public ITerminalViewClass {
public:
    explicit ScrollClass(const Ref<ITerminalViewClass>& parentClass);
    ~ScrollClass() override;

    void bindAttributes(AttributesBindingContext& binder) override;
    void paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) override;
};

} // namespace Valdi
