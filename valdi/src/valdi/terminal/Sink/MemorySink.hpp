#pragma once

#include "valdi/terminal/Sink/ITerminalSink.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"

namespace Valdi::Terminal {

/**
 * An in-memory sink that records everything written to it. Used by tests and headless hosts. Thread
 * safe: the paint thread writes while the host reads and resizes, so accessors return copies.
 */
class MemorySink : public ITerminalSink {
public:
    MemorySink();
    ~MemorySink() override;

    void write(std::string_view data) override;
    int columns() const override;
    int rows() const override;
    bool isTTY() const override;
    ColorLevel colorLevel() const override;
    void didRenderFrame(const FrameSnapshot& frame) override;

    void setSize(int columns, int rows);
    void setIsTTY(bool isTTY);
    void setColorLevel(ColorLevel colorLevel);

    /** Each call to write(), in order. */
    std::vector<std::string> writes() const;

    /** Everything written so far, concatenated. */
    std::string output() const;

    /** The last frame reported through didRenderFrame(). */
    FrameSnapshot lastFrame() const;

    /** Plain text rows of the last frame. */
    std::vector<std::string> lastPlainLines() const;

    size_t frameCount() const;

    void clearRecording();

private:
    mutable Mutex _mutex;
    std::vector<std::string> _writes;
    FrameSnapshot _lastFrame;
    size_t _frameCount = 0;
    int _columns = 80;
    int _rows = 24;
    bool _isTTY = true;
    ColorLevel _colorLevel = ColorLevel::None;
};

} // namespace Valdi::Terminal
