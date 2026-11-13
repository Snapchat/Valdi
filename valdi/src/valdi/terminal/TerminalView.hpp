#pragma once

#include "valdi/runtime/Views/View.hpp"
#include "valdi/terminal/Paint/Geometry.hpp"
#include "valdi/terminal/Paint/Style.hpp"
#include "valdi/terminal/Paint/TextLayout.hpp"
#include "valdi_core/Platform.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Views/Frame.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Valdi {

class ITerminalViewClass;
class TerminalRoot;
class ViewNode;

enum class TerminalBorderSide : size_t { Top = 0, Bottom = 1, Left = 2, Right = 3 };

/** Box decoration of a view. A border is drawn inside the frame and does not affect layout. */
struct TerminalViewStyle {
    std::optional<Terminal::TermColor> backgroundColor;
    std::optional<Terminal::TermColor> borderColor;
    std::array<std::optional<Terminal::TermColor>, 4> sideColors;
    std::array<bool, 4> sideEnabled = {true, true, true, true};
    double borderWidth = 0;
    std::string borderStyle = "single";
    bool borderDim = false;

    bool hasBorder() const;
};

enum class TerminalTextAlign { Left, Center, Right };

/** What determines how a label's text is split into lines. */
struct TerminalLabelParams {
    std::vector<Terminal::TextRun> runs;
    std::optional<Terminal::WrapMode> wrap;
    /** Zero means unlimited. */
    int numberOfLines = 1;
    bool ellipsize = true;

    /**
     * Lays the text out for the given width in cells; nullopt means unconstrained. Without an
     * explicit wrap mode, a single line truncates (with an ellipsis unless `ellipsize` is false, in which
     * case it is clipped) and anything else wraps.
     */
    std::vector<Terminal::StyledLine> layout(std::optional<int> width) const;
};

/** Label content and the cached line layout for the width it was last laid out at. */
class TerminalLabelState {
public:
    TerminalLabelParams params;
    /** Label-wide color, font weight/slant and decoration, overridden by per-run styles. */
    Terminal::Style baseStyle;
    TerminalTextAlign align = TerminalTextAlign::Left;

    /** Lines for `width`, recomputed only when the content or the width changed. */
    const std::vector<Terminal::StyledLine>& lines(int width);

    /** Must be called whenever params or baseStyle change. */
    void invalidate();

private:
    std::vector<Terminal::StyledLine> _lines;
    std::optional<int> _linesWidth;
};

/** Cells between a view's frame and its content box. */
struct TerminalInsets {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

/** A view of the terminal backend: a rectangle of cells with children. Painted by its view class. */
class TerminalView : public View {
public:
    TerminalView(const Ref<ITerminalViewClass>& viewClass, snap::valdi_core::Platform platform);
    ~TerminalView() override;

    snap::valdi_core::Platform getPlatform() const override;

    const Ref<ITerminalViewClass>& getViewClass() const;

    TerminalView* getParent() const;
    const std::vector<Ref<TerminalView>>& getChildren() const;
    void insertChild(const Ref<TerminalView>& child, size_t index);
    void removeFromParent();

    /** Frame relative to the parent, in cells; fractional values are rounded when painting. */
    const Frame& getFrame() const;
    void setFrame(const Frame& frame, bool requestRepaint = true);

    TerminalViewStyle& style();
    const TerminalViewStyle& style() const;

    TerminalLabelState& label();
    const TerminalLabelState& label() const;

    const Point& getScrollOffset() const;
    const Size& getContentSize() const;
    void setScrollSpecs(const Point& offset, const Size& contentSize);

    /**
     * Whether children are clipped to this view's frame. Reads the overflow style of the attached
     * ViewNode when there is one, otherwise the value set with setClipsChildren().
     */
    bool clipsChildren() const;
    void setClipsChildren(bool clipsChildren);

    /**
     * The padding Yoga resolved for the attached ViewNode, rounded to whole cells; zero when there
     * is no node or layout has not run. Views that draw content inset it by this amount.
     */
    TerminalInsets layoutPadding() const;

    /** Associates the ViewNode this view renders; nullptr detaches it. */
    void setViewNode(ViewNode* viewNode);

    /** How many children of a Static view have already been written above the live frame. */
    size_t getEmittedChildCount() const;
    void setEmittedChildCount(size_t count);

    /** Attaches the root that paints the tree this view is the top of. */
    void setRoot(const Ref<TerminalRoot>& root);
    Ref<TerminalRoot> getRoot() const;

    /** Requests a repaint of the tree this view belongs to. */
    void markDirty();

    static Ref<TerminalView> unwrap(const Ref<View>& view);

    VALDI_CLASS_HEADER(TerminalView)

private:
    Ref<ITerminalViewClass> _viewClass;
    snap::valdi_core::Platform _platform;
    TerminalView* _parent = nullptr;
    std::vector<Ref<TerminalView>> _children;
    Frame _frame;
    TerminalViewStyle _style;
    TerminalLabelState _label;
    Point _scrollOffset;
    Size _contentSize;
    bool _clipsChildren = false;
    size_t _emittedChildCount = 0;
    Weak<ViewNode> _viewNode;
    Weak<TerminalRoot> _root;
};

} // namespace Valdi
