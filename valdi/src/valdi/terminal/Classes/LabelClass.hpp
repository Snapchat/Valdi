#pragma once

#include "valdi/terminal/Classes/ITerminalViewClass.hpp"

namespace Valdi {

/**
 * A text view. Measures in terminal cells: width is the display width of the widest line and
 * height is the number of lines after wrapping.
 */
class LabelClass : public ITerminalViewClass {
public:
    explicit LabelClass(const Ref<ITerminalViewClass>& parentClass);
    ~LabelClass() override;

    void bindAttributes(AttributesBindingContext& binder) override;

    Size onMeasure(const Ref<ValueMap>& attributes,
                   float width,
                   MeasureMode widthMode,
                   float height,
                   MeasureMode heightMode,
                   bool isRightToLeft) override;

    void paint(TerminalView& view, Terminal::Painter& painter, const Terminal::PaintArea& area) override;

    /** Converts a `value` attribute, either a plain string or attributed text, into styled runs. */
    static std::vector<Terminal::TextRun> parseRuns(const Value& value);

    /** Bold, dim and italic flags derived from a font name such as "AvenirNext-Bold 16". */
    static Terminal::Style styleFromFontName(std::string_view fontName);
};

} // namespace Valdi
