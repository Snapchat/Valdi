#pragma once

#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#include "valdi/runtime/Views/DefaultMeasureDelegate.hpp"
#include "valdi/terminal/Paint/Geometry.hpp"
#include "valdi/terminal/TerminalView.hpp"
#include "valdi_core/Platform.hpp"

namespace Valdi::Terminal {
class Painter;
struct PaintArea;
} // namespace Valdi::Terminal

namespace Valdi {

/**
 * Describes one kind of view of the terminal backend: its names on each platform, its parent
 * class, its attributes, how it measures and how it paints. Classes only bind attributes they
 * add; inherited attributes come from the parent chain.
 */
class ITerminalViewClass : public DefaultMeasureDelegate {
public:
    ITerminalViewClass(const char* iosClassName,
                       const char* androidClassName,
                       const Ref<ITerminalViewClass>& parentClass,
                       bool canBeMeasured);
    ~ITerminalViewClass() override;

    const char* getIOSClassName() const;
    const char* getAndroidClassName() const;
    const Ref<ITerminalViewClass>& getParent() const;
    bool canBeMeasured() const;

    /** Creates a view of this class. `self` is the registered instance of this class. */
    virtual Ref<TerminalView> instantiate(const Ref<ITerminalViewClass>& self, snap::valdi_core::Platform platform);

    virtual void bindAttributes(AttributesBindingContext& binder);

    /** Measures from the layout attributes. Measurable classes override this. */
    Size onMeasure(const Ref<ValueMap>& attributes,
                   float width,
                   MeasureMode widthMode,
                   float height,
                   MeasureMode heightMode,
                   bool isRightToLeft) override;

    /** Paints `view` and its children into the painter's current target. */
    virtual void paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) = 0;

private:
    const char* _iosClassName;
    const char* _androidClassName;
    Ref<ITerminalViewClass> _parentClass;
    bool _canBeMeasured;
};

} // namespace Valdi
