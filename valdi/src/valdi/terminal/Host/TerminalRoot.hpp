#pragma once

#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/terminal/Paint/FrameWriter.hpp"
#include "valdi/terminal/Sink/ITerminalSink.hpp"
#include "valdi/terminal/TerminalViewManager.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Valdi {

struct TerminalRootOptions {
    /**
     * Lay out to the terminal height as well as its width. When false the height is unconstrained
     * and the frame is as tall as its content.
     */
    bool fitTerminal = false;
    /** Overrides for the size the sink reports when the root is attached. */
    std::optional<int> columns;
    std::optional<int> rows;
    /** Append every frame instead of redrawing in place. */
    bool debug = false;
    /**
     * Runs a function on the thread that paints. Defaults to the tree's main thread manager. It
     * must run the function on the thread that applies view transactions (the main thread): a frame
     * is painted from the view tree as it stands, so painting from another thread while a
     * transaction is mutating the tree would show half an update. Dropping the function without
     * running it is allowed; the next scheduleFrame() will dispatch again.
     */
    Function<void(DispatchFunction)> dispatcher;
};

using TerminalFrameObserver = Function<void(const Terminal::FrameSnapshot& frame)>;

/**
 * Renders one ViewNodeTree to a terminal sink. It owns the root view's layout size, coalesces
 * repaint requests into one paint per turn of the painting thread, and writes each frame through
 * a FrameWriter. A later native module can construct one from the ViewNodeTree of a Context.
 */
class TerminalRoot : public SharedPtrRefCountable {
public:
    TerminalRoot(const Ref<ViewNodeTree>& tree, const Ref<Terminal::ITerminalSink>& sink, TerminalRootOptions options);
    ~TerminalRoot() override;

    /**
     * Creates the root view, retains layout specs across invalidations and lays out for the size
     * of the sink. Hosts report later resizes with setSize().
     */
    void attach();

    /** Lays the tree out for a terminal of the given size. Repeated calls with the same size do nothing. */
    void setSize(int columns, int rows);

    /**
     * Like setSize() but applies the layout on the painting thread, for callers on a host thread such as the
     * terminal reader. Applies it inline when there is no painting thread to hop to.
     */
    void setSizeOnPaintThread(int columns, int rows);

    /**
     * Requests a repaint. Safe to call from any thread; requests made before the paint runs
     * coalesce. When the tree has no main thread manager and no dispatcher was provided there is
     * nowhere safe to paint, so the request is dropped and logged once; call paintNow() instead.
     */
    void scheduleFrame();

    /** Paints synchronously and writes the frame to the sink. */
    void paintNow();

    /**
     * Registers a callback that receives every painted frame. Returns an id for removeFrameObserver().
     * Callbacks run on the painting thread with no locks held, so they may call back into the root.
     * The observer list is copied when a frame finishes: an observer removed while earlier observers
     * of the same frame are running is not called, but one that is already running is not interrupted.
     */
    int addFrameObserver(TerminalFrameObserver observer);
    void removeFrameObserver(int id);

    /** The most recently painted frame. */
    Terminal::FrameSnapshot getLastFrame() const;

    /**
     * Queues lines that are written once, above the live frame, with the next paint. Used for output
     * that must not be erased, such as redirected console messages. Dropped once the output is finished.
     */
    void writeAbove(std::vector<std::string> lines);

    /** Erases the live frame from the terminal. The next paint draws it again. Does nothing once finished. */
    void clear();

    /**
     * Paints a final frame and lets the writer leave the output in a consistent state (cursor shown,
     * non-interactive sinks receive the final frame). Later calls and calls after destroy() do nothing.
     */
    void finish();

    /**
     * Finishes the output if finish() was not called, then detaches from the tree; further paint
     * requests are ignored. Also called by the destructor.
     */
    void destroy();

    bool isDestroyed() const;

    const Ref<ViewNodeTree>& getTree() const;

private:
    struct FrameRequest;

    Ref<ViewNodeTree> _tree;
    // The tree's view manager is held by raw reference through the ViewManagerContext, so the root keeps it alive.
    Ref<TerminalViewManager> _viewManager;
    Ref<Terminal::ITerminalSink> _sink;
    TerminalRootOptions _options;
    Terminal::FrameWriter _writer;
    mutable RecursiveMutex _mutex;
    std::atomic<bool> _frameScheduled{false};
    std::atomic<bool> _destroyed{false};
    std::atomic<bool> _finishRequested{false};
    std::atomic<bool> _reportedNoPaintThread{false};
    bool _finished = false;
    bool _painting = false;
    std::optional<std::tuple<int, int, bool>> _appliedLayout;
    Terminal::FrameSnapshot _lastFrame;
    std::vector<std::pair<int, TerminalFrameObserver>> _observers;
    std::vector<std::string> _pendingAbove;
    int _nextObserverId = 1;
    uint64_t _frameSequence = 0;

    int initialColumns() const;
    int initialRows() const;
    void applyLayout(int columns, int rows);
    bool canDispatch() const;
    void dispatchOnPaintThread(DispatchFunction function);
    bool isObserverRegistered(int id) const;
};

} // namespace Valdi
