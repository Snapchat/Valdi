#include "valdi/terminal/Sink/MemorySink.hpp"

#include <mutex>

namespace Valdi::Terminal {

MemorySink::MemorySink() = default;
MemorySink::~MemorySink() = default;

void MemorySink::write(std::string_view data) {
    std::lock_guard<Mutex> guard(_mutex);
    _writes.emplace_back(data);
}

int MemorySink::columns() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _columns;
}

int MemorySink::rows() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _rows;
}

bool MemorySink::isTTY() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _isTTY;
}

ColorLevel MemorySink::colorLevel() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _colorLevel;
}

void MemorySink::didRenderFrame(const FrameSnapshot& frame) {
    std::lock_guard<Mutex> guard(_mutex);
    _lastFrame = frame;
    _frameCount++;
}

void MemorySink::setSize(int columns, int rows) {
    std::lock_guard<Mutex> guard(_mutex);
    _columns = columns;
    _rows = rows;
}

void MemorySink::setIsTTY(bool isTTY) {
    std::lock_guard<Mutex> guard(_mutex);
    _isTTY = isTTY;
}

void MemorySink::setColorLevel(ColorLevel colorLevel) {
    std::lock_guard<Mutex> guard(_mutex);
    _colorLevel = colorLevel;
}

std::vector<std::string> MemorySink::writes() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _writes;
}

std::string MemorySink::output() const {
    std::lock_guard<Mutex> guard(_mutex);
    std::string out;
    for (const auto& chunk : _writes) {
        out += chunk;
    }
    return out;
}

FrameSnapshot MemorySink::lastFrame() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _lastFrame;
}

std::vector<std::string> MemorySink::lastPlainLines() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _lastFrame.plainLines;
}

size_t MemorySink::frameCount() const {
    std::lock_guard<Mutex> guard(_mutex);
    return _frameCount;
}

void MemorySink::clearRecording() {
    std::lock_guard<Mutex> guard(_mutex);
    _writes.clear();
}

} // namespace Valdi::Terminal
