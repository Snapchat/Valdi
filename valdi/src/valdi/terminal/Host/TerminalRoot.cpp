#include "valdi/terminal/Host/TerminalRoot.hpp"

#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/terminal/Host/TerminalHost.hpp"
#include "valdi/terminal/Paint/Ansi.hpp"
#include "valdi/terminal/Paint/CellGrid.hpp"
#include "valdi/terminal/Paint/Painter.hpp"
#include "valdi/terminal/TerminalView.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/TrackedLock.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>

namespace Valdi {

static constexpr int kFallbackColumns = 80;
static constexpr int kFallbackRows = 24;
// How long finish() waits for the painting thread before finishing without a final paint. Only reached when that
// thread is stuck or no longer runs tasks, for example while the process is exiting.
static constexpr auto kFinishWaitTimeout = std::chrono::milliseconds(2000);

namespace {

struct Completion {
    std::mutex mutex;
    std::condition_variable condition;
    bool done = false;

    void signal() {
        {
            std::lock_guard<std::mutex> guard(mutex);
            done = true;
        }
        condition.notify_all();
    }

    void waitFor(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait_for(lock, timeout, [this]() { return done; });
    }
};

// Signals when the dispatched function that holds it is destroyed, which is also what happens when a torn-down
// queue drops the function without running it.
struct CompletionToken {
    std::shared_ptr<Completion> completion;

    explicit CompletionToken(std::shared_ptr<Completion> completion) : completion(std::move(completion)) {}

    ~CompletionToken() {
        completion->signal();
    }
};

} // namespace

TerminalRoot::TerminalRoot(const Ref<ViewNodeTree>& tree,
                           const Ref<Terminal::ITerminalSink>& sink,
                           TerminalRootOptions options)
    : _tree(tree),
      _sink(sink),
      _options(std::move(options)),
      _writer(sink, Terminal::FrameWriterOptions{_options.debug}) {
    auto* viewManager = dynamic_cast<TerminalViewManager*>(tree->getViewManager());
    if (viewManager != nullptr) {
        _viewManager = strongRef(viewManager);
    }
}

TerminalRoot::~TerminalRoot() {
    destroy();
}

const Ref<ViewNodeTree>& TerminalRoot::getTree() const {
    return _tree;
}

int TerminalRoot::initialColumns() const {
    if (_options.columns) {
        return std::max(1, *_options.columns);
    }
    const int columns = _sink->columns();
    return columns > 0 ? columns : kFallbackColumns;
}

int TerminalRoot::initialRows() const {
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
    applyLayout(initialColumns(), initialRows());
}

void TerminalRoot::applyLayout(int columns, int rows) {
    const auto key = std::make_tuple(columns, rows, _options.fitTerminal);
    {
        // Released before touching the tree: paintNow() takes the tree lock first, then _mutex.
        std::lock_guard<RecursiveMutex> guard(_mutex);
        if (_appliedLayout && *_appliedLayout == key) {
            return;
        }
        _appliedLayout = key;
    }
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

void TerminalRoot::setSizeOnPaintThread(int columns, int rows) {
    if (_destroyed) {
        return;
    }
    if (!canDispatch()) {
        setSize(columns, rows);
        return;
    }
    dispatchOnPaintThread(DispatchFunction([weakSelf = weakRef(this), columns, rows]() {
        auto self = weakSelf.lock();
        if (self != nullptr) {
            self->setSize(columns, rows);
        }
    }));
}

bool TerminalRoot::canDispatch() const {
    return _options.dispatcher || _tree->getMainThreadManager() != nullptr;
}

bool TerminalRoot::isOnPaintThread() const {
    // A dispatcher must run functions on the main thread, so the main thread is the painting thread either way.
    const auto& mainThreadManager = _tree->getMainThreadManager();
    return mainThreadManager != nullptr && mainThreadManager->currentThreadIsMainThread();
}

void TerminalRoot::dispatchOnPaintThread(DispatchFunction function) {
    if (_options.dispatcher) {
        _options.dispatcher(std::move(function));
        return;
    }
    _tree->getMainThreadManager()->dispatch(_tree->getContext(), std::move(function));
}

// Resets the coalescing flag if the dispatched function is destroyed without running, which is what
// happens to tasks that a torn-down MainThreadManager or a dropping dispatcher discards.
struct TerminalRoot::FrameRequest {
    Weak<TerminalRoot> root;
    bool fired = false;

    explicit FrameRequest(Weak<TerminalRoot> root) : root(std::move(root)) {}

    ~FrameRequest() {
        if (fired) {
            return;
        }
        auto self = root.lock();
        if (self != nullptr) {
            self->_frameScheduled = false;
        }
    }
};

void TerminalRoot::scheduleFrame() {
    if (_destroyed) {
        return;
    }
    if (!canDispatch()) {
        // Painting inline could run in the middle of a view transaction, so there is no safe fallback.
        if (!_reportedNoPaintThread.exchange(true)) {
            VALDI_WARN(_tree->getContext()->getLogger(),
                       "TerminalRoot cannot schedule frames: the tree has no MainThreadManager and no dispatcher was "
                       "provided. Call paintNow() to render.");
        }
        return;
    }
    if (_frameScheduled.exchange(true)) {
        return;
    }
    auto request = std::make_shared<FrameRequest>(weakRef(this));
    dispatchOnPaintThread(DispatchFunction([request]() {
        request->fired = true;
        auto self = request->root.lock();
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

    Terminal::FrameSnapshot frame;
    std::vector<std::pair<int, TerminalFrameObserver>> observers;
    {
        auto treeLock = _tree->lock();
        std::unique_lock<RecursiveMutex> guard(_mutex);
        if (_destroyed || _finished || _painting) {
            return;
        }
        _painting = true;
        const struct PaintingScope {
            bool& flag;
            ~PaintingScope() {
                flag = false;
            }
        } paintingScope{_painting};

        auto rootView = TerminalView::unwrap(_tree->getRootView());
        if (rootView == nullptr) {
            return;
        }

        // Valdi leaves the frame of the root view to the host.
        const auto& rootNode = _tree->getRootViewNode();
        if (rootNode != nullptr) {
            rootView->setFrame(rootNode->getCalculatedFrame(), /* requestRepaint */ false);
        }

        const int columns = _appliedLayout ? std::get<0>(*_appliedLayout) : initialColumns();
        const int rows = _appliedLayout ? std::get<1>(*_appliedLayout) : initialRows();
        const auto rootRect = Terminal::Painter::roundRect(rootView->getFrame(), 0, 0);
        const int height = _options.fitTerminal ? rows : std::max(0, rootRect.y + rootRect.height);

        Terminal::CellGrid grid(columns, height);
        Terminal::Painter painter(grid);
        painter.paintRoot(*rootView);

        const auto level = _sink->colorLevel();
        std::vector<std::string> staticLines = std::move(_pendingAbove);
        _pendingAbove.clear();
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

        frame.ansiLines = grid.toLines(level);
        frame.plainLines = grid.toPlainLines();
        frame.columns = columns;
        frame.height = height;
        frame.sequence = ++_frameSequence;
        for (size_t i = 0; i < frame.ansiLines.size(); i++) {
            if (i > 0) {
                frame.ansi += '\n';
            }
            frame.ansi += frame.ansiLines[i];
        }
        observers = _observers;

        // Everything that reads the view tree is done; sink I/O must not hold up the tree.
        treeLock.unlock();

        _writer.write(frame.ansiLines, staticLines);
        _lastFrame = frame;
        _sink->didRenderFrame(frame);
        _sink->flush();
    }

    for (auto& observer : observers) {
        if (isObserverRegistered(observer.first)) {
            observer.second(frame);
        }
    }
}

bool TerminalRoot::isObserverRegistered(int id) const {
    std::lock_guard<RecursiveMutex> guard(_mutex);
    return std::any_of(_observers.begin(), _observers.end(), [id](const auto& o) { return o.first == id; });
}

int TerminalRoot::addFrameObserver(TerminalFrameObserver observer) {
    std::lock_guard<RecursiveMutex> guard(_mutex);
    const int id = _nextObserverId++;
    _observers.emplace_back(id, std::move(observer));
    return id;
}

void TerminalRoot::removeFrameObserver(int id) {
    std::lock_guard<RecursiveMutex> guard(_mutex);
    _observers.erase(
        std::remove_if(_observers.begin(), _observers.end(), [id](const auto& o) { return o.first == id; }),
        _observers.end());
}

Terminal::FrameSnapshot TerminalRoot::getLastFrame() const {
    std::lock_guard<RecursiveMutex> guard(_mutex);
    return _lastFrame;
}

void TerminalRoot::writeAbove(std::vector<std::string> lines) {
    if (_destroyed || lines.empty()) {
        return;
    }
    {
        std::lock_guard<RecursiveMutex> guard(_mutex);
        // A finished output has already been left in its final state, and no later paint would drain these.
        if (_finished) {
            return;
        }
        _pendingAbove.insert(
            _pendingAbove.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
    }
    scheduleFrame();
}

void TerminalRoot::clear() {
    std::lock_guard<RecursiveMutex> guard(_mutex);
    if (_destroyed || _finished) {
        return;
    }
    _writer.clear();
    _sink->flush();
}

void TerminalRoot::redraw() {
    {
        std::lock_guard<RecursiveMutex> guard(_mutex);
        if (_destroyed || _finished) {
            return;
        }
        _writer = Terminal::FrameWriter(_sink, Terminal::FrameWriterOptions{_options.debug});
    }
    scheduleFrame();
}

void TerminalRoot::finish() {
    if (_destroyed || _finishRequested.exchange(true)) {
        return;
    }
    // After a Ctrl+C or signal restore the terminal is already in its final state; painting again would hide the
    // cursor and repeat the newline. Nothing reads the tree, so there is no need to hop.
    if (_sink->isTTY() && Terminal::TerminalHost::isRestoreWritten()) {
        std::lock_guard<RecursiveMutex> guard(_mutex);
        if (!_finished) {
            _writer.abandon();
            _finished = true;
        }
        return;
    }
    if (!canDispatch() || isOnPaintThread()) {
        finishOnPaintThread();
        return;
    }

    // The final paint reads the views, which queued view transactions mutate on the painting thread without the
    // tree lock. Running it there orders it after the transactions queued before this call, such as the last
    // setState before exit(). Callers rely on the output being final when finish() returns, so wait for it.
    auto completion = std::make_shared<Completion>();
    {
        auto token = std::make_shared<CompletionToken>(completion);
        dispatchOnPaintThread(DispatchFunction([weakSelf = weakRef(this), token]() {
            auto self = weakSelf.lock();
            if (self != nullptr) {
                self->finishOnPaintThread();
            }
            token->completion->signal();
        }));
    }
    {
        DropAllTrackedLocks dropAllTrackedLocks;
        completion->waitFor(kFinishWaitTimeout);
    }

    // The painting thread did not get to it: leave the output consistent with the last frame that was painted.
    std::lock_guard<RecursiveMutex> guard(_mutex);
    if (!_finished) {
        _writer.finish();
        _finished = true;
        _sink->flush();
    }
}

void TerminalRoot::finishOnPaintThread() {
    const bool alreadyRestored = _sink->isTTY() && Terminal::TerminalHost::isRestoreWritten();
    if (!alreadyRestored) {
        paintNow();
    }
    std::lock_guard<RecursiveMutex> guard(_mutex);
    // destroy() may have abandoned the output while the paint was queued.
    if (_finished) {
        return;
    }
    if (alreadyRestored) {
        _writer.abandon();
    } else {
        _writer.finish();
    }
    _finished = true;
    _sink->flush();
}

void TerminalRoot::destroy() {
    if (_destroyed.exchange(true)) {
        return;
    }
    _finishRequested = true;
    {
        std::lock_guard<RecursiveMutex> guard(_mutex);
        if (!_finished) {
            // Destroying without finish() abandons the output, as when the first render throws: no final paint and
            // no trailing newline. A live frame that was already drawn is erased so that a half-built frame is not
            // left above the error, and the cursor the first frame hid is shown again. After a signal or Ctrl+C
            // restore the terminal is already final and the frame position is unknown, so nothing is written.
            if (_frameSequence > 0 && _sink->isTTY() && !_options.debug &&
                !Terminal::TerminalHost::isRestoreWritten()) {
                _writer.clear();
                _sink->write(Terminal::Ansi::kCursorShow);
            }
            _writer.abandon();
            _finished = true;
            _sink->flush();
        }
        _observers.clear();
    }

    // The root view's back reference is read on the painting thread while it applies transactions.
    auto detach = [tree = _tree]() {
        auto treeLock = tree->lock();
        auto rootView = TerminalView::unwrap(tree->getRootView());
        if (rootView != nullptr) {
            rootView->setRoot(nullptr);
        }
    };
    if (canDispatch() && !isOnPaintThread()) {
        dispatchOnPaintThread(DispatchFunction(std::move(detach)));
    } else {
        detach();
    }
}

bool TerminalRoot::isDestroyed() const {
    return _destroyed;
}

} // namespace Valdi
