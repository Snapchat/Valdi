//
//  Geometry.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/28/22.
//

#include "snap_drawing/cpp/Utils/Geometry.hpp"

namespace snap::drawing {

Scalar Point::length(Scalar dx, Scalar dy) {
    Scalar mag2 = dx * dx + dy * dy;
    return sqrtf(mag2);
}

void Rect::join(const Rect& r) {
    if (r.isEmpty()) {
        return;
    }

    if (this->isEmpty()) {
        *this = r;
    } else {
        left = std::min(left, r.left);
        top = std::min(top, r.top);
        right = std::max(right, r.right);
        bottom = std::max(bottom, r.bottom);
    }
}

Rect Rect::makeFittingSize(Size size, FittingSizeMode mode) const {
    auto selfWidth = width();
    auto selfHeight = height();
    auto halfWidth = selfWidth / 2.0f;
    auto halfHeight = selfHeight / 2.0f;

    switch (mode) {
        case FittingSizeModeFill:
            return *this;
        case FittingSizeModeCenter: {
            return Rect::makeXYWH(
                left + halfWidth - size.width / 2.0f, top + halfHeight - size.height / 2.0f, size.width, size.height);
        }
        case FittingSizeModeCenterScaleFill:
        case FittingSizeModeCenterScaleFit: {
            auto wRatio = selfWidth / size.width;
            auto hRatio = selfHeight / size.height;
            auto ratio = mode == FittingSizeModeCenterScaleFill ? std::max(wRatio, hRatio) : std::min(wRatio, hRatio);

            auto resolvedTargetWidth = size.width * ratio;
            auto resolvedTargetHeight = size.height * ratio;

            return Rect::makeXYWH(left + halfWidth - resolvedTargetWidth / 2.0f,
                                  top + halfHeight - resolvedTargetHeight / 2.0f,
                                  resolvedTargetWidth,
                                  resolvedTargetHeight);
        }
    }
}

bool Rect::intersects(const Rect& other) const {
    // Use Skia's exact floating-point comparison for general intersection checks.
    // This is important for Compositor plane optimization logic.
    return getSkValue().intersects(other.getSkValue());
}

bool Rect::intersectsWithTolerance(const Rect& other) const {
    // Use epsilon-based comparison for damage rect merging.
    // This prevents gaps when rects computed through different transformation paths
    // have tiny numerical differences (e.g., 10.499999 vs 10.500001).
    constexpr Scalar epsilon = 0.0001f; // ~1/10000th of a pixel

    return !(other.right < left - epsilon || right < other.left - epsilon || other.bottom < top - epsilon ||
             bottom < other.top - epsilon);
}

bool Rect::contains(const Point& point) const {
    return !(point.x < left || point.x > right || point.y < top || point.y > bottom);
}

Point Rect::closestPoint(const Point& toPoint) const {
    auto closestX = std::clamp(toPoint.x, left, right);
    auto closestY = std::clamp(toPoint.y, top, bottom);

    return Point::make(closestX, closestY);
}

Rect Rect::intersection(const Rect& other) const {
    auto x = std::max(left, other.left);
    auto y = std::max(top, other.top);
    auto width = std::min(right, other.right) - x;
    auto height = std::min(bottom, other.bottom) - y;

    if (width < 0.0f || height < 0.0f) {
        // Rectangles don't intersect
        return Rect::makeEmpty();
    }

    return Rect::makeXYWH(x, y, width, height);
}

std::ostream& operator<<(std::ostream& os, const Vector& vec) {
    return os << "dx: " << vec.dx << ", dy: " << vec.dy;
}

std::ostream& operator<<(std::ostream& os, const Size& size) {
    return os << "w: " << size.width << ", h: " << size.height;
}

std::ostream& operator<<(std::ostream& os, const Point& point) {
    return os << "x: " << point.x << ", y: " << point.y;
}

std::ostream& operator<<(std::ostream& os, const Rect& rect) {
    return os << "x: " << rect.left << ", y: " << rect.top << ", w: " << rect.width() << ", h: " << rect.height();
}

} // namespace snap::drawing
