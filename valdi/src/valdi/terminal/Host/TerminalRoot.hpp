#pragma once

#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/terminal/Paint/FrameWriter.hpp"
#include "valdi/terminal/Sink/ITerminalSink.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

#include <atomic>
#include <optional>
#include <vector>

namespace Valdi {

struct TerminalRootOptions {
    /**
     * Lay out to the terminal height as well as its width. When false the height is unconstrained
     * and the frame is as tall as its content.
     */
    bool fitTerminal = false;
    /** Overrides for the size reported by the sink. */
    std::optional<int> columns;
    std::optional<int> rows;
    /** Append every frame instead of redrawing in place. */
    bool debug = false;
    /** Runs a function on the thread that paints. Defaults to the tree's main thread manager. */
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

    /** Creates the root view, retains layout specs across invalidations and applies the initial layout size. */
    void attach();

    /** Lays the tree out for a terminal of the given size. Repeated calls with the same size do nothing. */
    void setSize(int columns, int rows);

    /** Requests a repaint. Safe to call from any thread; requests made before the paint runs coalesce. */
    void scheduleFrame();

    /** Paints synchronously and writes the frame to the sink. */
    void paintNow();

    /** Registers a callback that receives every painted frame. Returns an id for removeFrameObserver(). */
    int addFrameObserver(TerminalFrameObserver observer);
    void removeFrameObserver(int id);

    /** The most recently painted frame. */
    Terminal::FrameSnapshot getLastFrame() const;

    /** Paints a final frame and lets the writer leave the output in a consistent state. */
    void finish();

    /** Detaches from the tree; further paint requests are ignored. */
    void destroy();

    bool isDestroyed() const;

    const Ref<ViewNodeTree>& getTree() const;

private:
    Ref<ViewNodeTree> _tree;
    Ref<Terminal::ITerminalSink> _sink;
    TerminalRootOptions _options;
    Terminal::FrameWriter _writer;
    mutable Mutex _mutex;
    std::atomic<bool> _frameScheduled{false};
    std::atomic<bool> _destroyed{false};
    bool _painting = false;
    std::optional<std::tuple<int, int, bool>> _appliedLayout;
    Terminal::FrameSnapshot _lastFrame;
    std::vector<std::pair<int, TerminalFrameObserver>> _observers;
    int _nextObserverId = 1;

    int currentColumns() const;
    int currentRows() const;
    void applyLayout(int columns, int rows);
    void dispatchOnPaintThread(DispatchFunction function);
};

} // namespace Valdi
