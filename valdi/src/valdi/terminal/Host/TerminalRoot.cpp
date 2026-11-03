#include "valdi/terminal/Host/TerminalRoot.hpp"

#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/terminal/Paint/CellGrid.hpp"
#include "valdi/terminal/Paint/Painter.hpp"
#include "valdi/terminal/TerminalView.hpp"

#include <algorithm>
#include <limits>

namespace Valdi {

static constexpr int kFallbackColumns = 80;
static constexpr int kFallbackRows = 24;

TerminalRoot::TerminalRoot(const Ref<ViewNodeTree>& tree,
                           const Ref<Terminal::ITerminalSink>& sink,
                           TerminalRootOptions options)
    : _tree(tree),
      _sink(sink),
      _options(std::move(options)),
      _writer(sink, Terminal::FrameWriterOptions{_options.debug}) {}

TerminalRoot::~TerminalRoot() = default;

const Ref<ViewNodeTree>& TerminalRoot::getTree() const {
    return _tree;
}

int TerminalRoot::currentColumns() const {
    if (_options.columns) {
        return std::max(1, *_options.columns);
    }
    const int columns = _sink->columns();
    return columns > 0 ? columns : kFallbackColumns;
}

int TerminalRoot::currentRows() const {
    if (_options.rows) {
        return std::max(1, *_options.rows);
    }
    const int rows = _sink->rows();
    return rows > 0 ? rows : kFallbackRows;
}

void TerminalRoot::attach() {
    auto lock = _tree->lock();
    _tree->setRootViewWithDefaultViewClass();
    _tree->setRetainsLayoutSpecsOnInvalidateLayout(true);

    auto rootView = TerminalView::unwrap(_tree->getRootView());
    if (rootView != nullptr) {
        rootView->setRoot(strongRef(this));
    }
    applyLayout(currentColumns(), currentRows());
}

void TerminalRoot::applyLayout(int columns, int rows) {
    const auto key = std::make_tuple(columns, rows, _options.fitTerminal);
    if (_appliedLayout && *_appliedLayout == key) {
        return;
    }
    _appliedLayout = key;
    // Unspecified height lets content decide how tall the frame is.
    const float height = _options.fitTerminal ? static_cast<float>(rows) : std::numeric_limits<float>::quiet_NaN();
    _tree->setLayoutSpecs(Size(static_cast<float>(columns), height), LayoutDirectionLTR);
}

void TerminalRoot::setSize(int columns, int rows) {
    if (_destroyed) {
        return;
    }
    applyLayout(std::max(1, columns), std::max(1, rows));
    scheduleFrame();
}

void TerminalRoot::dispatchOnPaintThread(DispatchFunction function) {
    if (_options.dispatcher) {
        _options.dispatcher(std::move(function));
        return;
    }
    const auto& mainThreadManager = _tree->getMainThreadManager();
    if (mainThreadManager != nullptr) {
        mainThreadManager->dispatch(_tree->getContext(), std::move(function));
        return;
    }
    function();
}

void TerminalRoot::scheduleFrame() {
    if (_destroyed || _frameScheduled.exchange(true)) {
        return;
    }
    auto weakSelf = weakRef(this);
    dispatchOnPaintThread(DispatchFunction([weakSelf]() {
        auto self = weakSelf.lock();
        if (self == nullptr) {
            return;
        }
        self->_frameScheduled = false;
        self->paintNow();
    }));
}

void TerminalRoot::paintNow() {
    if (_destroyed) {
        return;
    }
    auto treeLock = _tree->lock();
    std::lock_guard<Mutex> guard(_mutex);
    if (_painting) {
        return;
    }
    _painting = true;

    auto rootView = TerminalView::unwrap(_tree->getRootView());
    if (rootView == nullptr) {
        _painting = false;
        return;
    }

    if (!_options.columns && !_options.rows) {
        applyLayout(currentColumns(), currentRows());
    }

    const int columns = currentColumns();
    const auto rootRect = Terminal::Painter::roundRect(rootView->getFrame(), 0, 0);
    const int height = _options.fitTerminal ? currentRows() : std::max(0, rootRect.y + rootRect.height);

    Terminal::CellGrid grid(columns, height);
    Terminal::Painter painter(grid);
    painter.paintRoot(*rootView);

    const auto level = _sink->colorLevel();
    std::vector<std::string> staticLines;
    for (auto* staticView : painter.staticViews()) {
        const size_t childCount = staticView->getChildren().size();
        const size_t emitted = std::min(staticView->getEmittedChildCount(), childCount);
        if (emitted < childCount) {
            auto slice = Terminal::Painter::paintStaticSlice(*staticView, emitted);
            const auto lines = slice.grid.toLines(level);
            for (size_t row = static_cast<size_t>(slice.firstRow); row < lines.size(); row++) {
                staticLines.push_back(lines[row]);
            }
        }
        staticView->setEmittedChildCount(childCount);
    }

    Terminal::FrameSnapshot frame;
    frame.ansiLines = grid.toLines(level);
    frame.plainLines = grid.toPlainLines();
    frame.columns = columns;
    frame.height = height;
    for (size_t i = 0; i < frame.ansiLines.size(); i++) {
        if (i > 0) {
            frame.ansi += '\n';
        }
        frame.ansi += frame.ansiLines[i];
    }

    _writer.write(frame.ansiLines, staticLines);
    _lastFrame = frame;
    _sink->didRenderFrame(frame);
    _sink->flush();

    // Observers may call back into the root, so run them without holding the paint state.
    auto observers = _observers;
    _painting = false;
    for (auto& observer : observers) {
        observer.second(frame);
    }
}

int TerminalRoot::addFrameObserver(TerminalFrameObserver observer) {
    std::lock_guard<Mutex> guard(_mutex);
    const int id = _nextObserverId++;
    _observers.emplace_back(id, std::move(observer));
    return id;
}

void TerminalRoot::removeFrameObserver(int id) {
    std::lock_guard<Mutex> guard(_mutex);
    _observers.erase(
        std::remove_if(_observers.begin(), _observers.end(), [id](const auto& o) { return o.first == id; }),
        _observers.end());
}

Terminal::FrameSnapshot TerminalRoot::getLastFrame() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _lastFrame;
}

void TerminalRoot::finish() {
    if (_destroyed) {
        return;
    }
    paintNow();
    std::lock_guard<Mutex> guard(_mutex);
    _writer.finish();
    _sink->flush();
}

void TerminalRoot::destroy() {
    if (_destroyed.exchange(true)) {
        return;
    }
    auto treeLock = _tree->lock();
    auto rootView = TerminalView::unwrap(_tree->getRootView());
    if (rootView != nullptr) {
        rootView->setRoot(nullptr);
    }
    std::lock_guard<Mutex> guard(_mutex);
    _observers.clear();
}

bool TerminalRoot::isDestroyed() const {
    return _destroyed;
}

} // namespace Valdi
