#pragma once

#include <cstddef>
#include <limits>
#include <optional>

namespace ValdiLinux {

// Validate before multiplying or allocating; also bounds the temporary RGBA
// buffer independently of whatever allocation SDL uses for its window surface.
inline std::optional<size_t> framebufferBytes(int width, int height, size_t limit) {
    if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() / 4) {
        return std::nullopt;
    }
    const auto rowBytes = static_cast<size_t>(width) * 4;
    if (static_cast<size_t>(height) > limit / rowBytes) {
        return std::nullopt;
    }
    return rowBytes * static_cast<size_t>(height);
}

} // namespace ValdiLinux
