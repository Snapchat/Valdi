#pragma once

#include <algorithm>

namespace Valdi::Terminal {

/** An integer rectangle in terminal cells. */
struct IntRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr IntRect() = default;
    constexpr IntRect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}

    constexpr bool contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    constexpr bool isEmpty() const {
        return width <= 0 || height <= 0;
    }

    constexpr bool operator==(const IntRect& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }

    constexpr bool operator!=(const IntRect& other) const {
        return !(*this == other);
    }
};

constexpr IntRect intersect(const IntRect& a, const IntRect& b) {
    const int x = std::max(a.x, b.x);
    const int y = std::max(a.y, b.y);
    const int right = std::min(a.x + a.width, b.x + b.width);
    const int bottom = std::min(a.y + a.height, b.y + b.height);
    return IntRect(x, y, std::max(0, right - x), std::max(0, bottom - y));
}

} // namespace Valdi::Terminal
