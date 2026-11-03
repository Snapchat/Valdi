#include "valdi/terminal/TerminalView.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/terminal/Classes/ITerminalViewClass.hpp"
#include "valdi/terminal/Host/TerminalRoot.hpp"

#include <algorithm>
#include <yoga/Yoga.h>

namespace Valdi {

bool TerminalViewStyle::hasBorder() const {
    return borderWidth > 0;
}

std::vector<Terminal::StyledLine> TerminalLabelParams::layout(std::optional<int> width) const {
    Terminal::LayoutOptions options;
    options.maxWidth = width;
    options.maxLines = std::max(0, numberOfLines);
    options.ellipsize = ellipsize;
    if (wrap) {
        options.mode = *wrap;
    } else if (numberOfLines == 1 && ellipsize) {
        options.mode = Terminal::WrapMode::TruncateEnd;
    }
    return Terminal::layoutText(runs, options);
}

const std::vector<Terminal::StyledLine>& TerminalLabelState::lines(int width) {
    if (_linesWidth && *_linesWidth == width) {
        return _lines;
    }
    TerminalLabelParams styled = params;
    for (auto& run : styled.runs) {
        run.style = Terminal::Style::merge(baseStyle, run.style);
    }
    _lines = styled.layout(width);
    _linesWidth = width;
    return _lines;
}

void TerminalLabelState::invalidate() {
    _linesWidth.reset();
    _lines.clear();
}

TerminalView::TerminalView(const Ref<ITerminalViewClass>& viewClass, snap::valdi_core::Platform platform)
    : _viewClass(viewClass), _platform(platform) {}

TerminalView::~TerminalView() {
    for (const auto& child : _children) {
        child->_parent = nullptr;
    }
}

snap::valdi_core::Platform TerminalView::getPlatform() const {
    return _platform;
}

const Ref<ITerminalViewClass>& TerminalView::getViewClass() const {
    return _viewClass;
}

TerminalView* TerminalView::getParent() const {
    return _parent;
}

const std::vector<Ref<TerminalView>>& TerminalView::getChildren() const {
    return _children;
}

void TerminalView::insertChild(const Ref<TerminalView>& child, size_t index) {
    child->removeFromParent();
    index = std::min(index, _children.size());
    _children.insert(_children.begin() + static_cast<std::ptrdiff_t>(index), child);
    child->_parent = this;
    markDirty();
}

void TerminalView::removeFromParent() {
    auto* parent = _parent;
    if (parent == nullptr) {
        return;
    }
    // Hold ourselves while the parent drops its reference.
    auto self = strongRef(this);
    parent->markDirty();
    _parent = nullptr;
    auto& siblings = parent->_children;
    siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [this](const auto& c) { return c.get() == this; }),
                   siblings.end());
}

const Frame& TerminalView::getFrame() const {
    return _frame;
}

void TerminalView::setFrame(const Frame& frame, bool requestRepaint) {
    _frame = frame;
    if (requestRepaint) {
        markDirty();
    }
}

TerminalViewStyle& TerminalView::style() {
    return _style;
}

const TerminalViewStyle& TerminalView::style() const {
    return _style;
}

TerminalLabelState& TerminalView::label() {
    return _label;
}

const TerminalLabelState& TerminalView::label() const {
    return _label;
}

const Point& TerminalView::getScrollOffset() const {
    return _scrollOffset;
}

const Size& TerminalView::getContentSize() const {
    return _contentSize;
}

void TerminalView::setScrollSpecs(const Point& offset, const Size& contentSize) {
    _scrollOffset = offset;
    _contentSize = contentSize;
    markDirty();
}

bool TerminalView::clipsChildren() const {
    auto node = _viewNode.lock();
    if (node != nullptr) {
        auto* yogaNode = node->getYogaNode();
        if (yogaNode != nullptr) {
            return YGNodeStyleGetOverflow(yogaNode) == YGOverflowHidden;
        }
    }
    return _clipsChildren;
}

void TerminalView::setClipsChildren(bool clipsChildren) {
    _clipsChildren = clipsChildren;
}

void TerminalView::setViewNode(ViewNode* viewNode) {
    _viewNode = viewNode != nullptr ? weakRef(viewNode) : Weak<ViewNode>();
}

size_t TerminalView::getEmittedChildCount() const {
    return _emittedChildCount;
}

void TerminalView::setEmittedChildCount(size_t count) {
    _emittedChildCount = count;
}

void TerminalView::setRoot(const Ref<TerminalRoot>& root) {
    _root = root != nullptr ? weakRef(root.get()) : Weak<TerminalRoot>();
}

Ref<TerminalRoot> TerminalView::getRoot() const {
    const TerminalView* top = this;
    while (top->_parent != nullptr) {
        top = top->_parent;
    }
    return top->_root.lock();
}

void TerminalView::markDirty() {
    auto root = getRoot();
    if (root != nullptr) {
        root->scheduleFrame();
    }
}

Ref<TerminalView> TerminalView::unwrap(const Ref<View>& view) {
    return castOrNull<TerminalView>(view);
}

VALDI_CLASS_IMPL(TerminalView)

} // namespace Valdi
