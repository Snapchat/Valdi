#include "valdi/terminal/Sink/MemorySink.hpp"

namespace Valdi::Terminal {

MemorySink::MemorySink() = default;
MemorySink::~MemorySink() = default;

void MemorySink::write(std::string_view data) {
    _writes.emplace_back(data);
}

int MemorySink::columns() const {
    return _columns;
}

int MemorySink::rows() const {
    return _rows;
}

bool MemorySink::isTTY() const {
    return _isTTY;
}

ColorLevel MemorySink::colorLevel() const {
    return _colorLevel;
}

void MemorySink::didRenderFrame(const FrameSnapshot& frame) {
    _lastFrame = frame;
    _frameCount++;
}

void MemorySink::setSize(int columns, int rows) {
    _columns = columns;
    _rows = rows;
}

void MemorySink::setIsTTY(bool isTTY) {
    _isTTY = isTTY;
}

void MemorySink::setColorLevel(ColorLevel colorLevel) {
    _colorLevel = colorLevel;
}

const std::vector<std::string>& MemorySink::writes() const {
    return _writes;
}

std::string MemorySink::output() const {
    std::string out;
    for (const auto& chunk : _writes) {
        out += chunk;
    }
    return out;
}

const FrameSnapshot& MemorySink::lastFrame() const {
    return _lastFrame;
}

const std::vector<std::string>& MemorySink::lastPlainLines() const {
    return _lastFrame.plainLines;
}

size_t MemorySink::frameCount() const {
    return _frameCount;
}

void MemorySink::clearRecording() {
    _writes.clear();
}

} // namespace Valdi::Terminal
