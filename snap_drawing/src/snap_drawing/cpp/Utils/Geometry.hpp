//
//  Geometry.hpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/28/22.
//

#pragma once

#include "snap_drawing/cpp/Utils/Scalar.hpp"
#include "snap_drawing/cpp/Utils/SkiaBridge.hpp"

#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"

#include <ostream>

namespace snap::drawing {

struct Vector {
    Scalar dx = 0;
    Scalar dy = 0;

    constexpr Vector() = default;
    constexpr Vector(Scalar dx, Scalar dy) : dx(dx), dy(dy) {}

    static constexpr Vector make(Scalar dx, Scalar dy) {
        return Vector(dx, dy);
    }

    inline bool operator==(const Vector& other) const {
        return scalarsEqual<2>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    inline bool operator!=(const Vector& other) const {
        return !scalarsEqual<2>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    static constexpr Vector makeEmpty() {
        return make(0, 0);
    }
};

struct Size {
    Scalar width = 0;
    Scalar height = 0;

    constexpr Size() = default;
    constexpr Size(Scalar width, Scalar height) : width(width), height(height) {}

    static constexpr Size make(Scalar width, Scalar height) {
        return Size(width, height);
    }

    inline bool operator==(const Size& other) const {
        return scalarsEqual<2>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    inline bool operator!=(const Size& other) const {
        return !scalarsEqual<2>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    static constexpr Size makeEmpty() {
        return make(0, 0);
    }
};

struct Point : public BridgedSkValue<Point, SkPoint> {
    Scalar x = 0;
    Scalar y = 0;

    constexpr Point() = default;
    constexpr Point(Scalar x, Scalar y) : x(x), y(y) {}

    static constexpr Point make(Scalar x, Scalar y) {
        return Point(x, y);
    }

    static constexpr Point makeEmpty() {
        return make(0, 0);
    }

    constexpr void offset(Scalar dx, Scalar dy) {
        x += dx;
        y += dy;
    }

    constexpr Point makeOffset(Scalar dx, Scalar dy) const {
        return Point::make(x + dx, y + dy);
    }

    /** Returns the Euclidean distance from origin, computed as:

            sqrt(fX * fX + fY * fY)

        .

        @return  straight-line distance to origin
     */
    static Scalar length(Scalar dx, Scalar dy);

    static Scalar distance(const Point& a, const Point& b) {
        return length(a.x - b.x, a.y - b.y);
    }

    inline bool operator==(const Point& other) const {
        return scalarsEqual<2>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    inline bool operator!=(const Point& other) const {
        return !scalarsEqual<2>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }
};

enum FittingSizeMode {
    FittingSizeModeFill,
    FittingSizeModeCenter,
    FittingSizeModeCenterScaleFill,
    FittingSizeModeCenterScaleFit,
};

struct Rect : public BridgedSkValue<Rect, SkRect> {
    Scalar left = 0;
    Scalar top = 0;
    Scalar right = 0;
    Scalar bottom = 0;

    constexpr Rect() = default;
    constexpr Rect(Scalar left, Scalar top, Scalar right, Scalar bottom)
        : left(left), top(top), right(right), bottom(bottom) {}

    constexpr Scalar x() const {
        return left;
    }

    constexpr Scalar y() const {
        return top;
    }

    constexpr Scalar width() const {
        return right - left;
    }

    constexpr Scalar height() const {
        return bottom - top;
    }

    constexpr Size size() const {
        return Size::make(width(), height());
    }

    constexpr Point center() const {
        return Point::make(left + width() / 2.0f, top + height() / 2.0f);
    }

    inline bool operator==(const Rect& other) const {
        return scalarsEqual<4>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    inline bool operator!=(const Rect& other) const {
        return !scalarsEqual<4>(reinterpret_cast<const Scalar*>(this), reinterpret_cast<const Scalar*>(&other));
    }

    static constexpr Rect makeLTRB(Scalar l, Scalar t, Scalar r, Scalar b) {
        return Rect(l, t, r, b);
    }

    static constexpr Rect makeXYWH(Scalar x, Scalar y, Scalar w, Scalar h) {
        return Rect(x, y, x + w, y + h);
    }

    static constexpr Rect makeEmpty() {
        return makeLTRB(0, 0, 0, 0);
    }

    constexpr bool isEmpty() const {
        return !(left < right && top < bottom);
    }

    constexpr Rect makeOffset(Scalar dx, Scalar dy) const {
        return Rect::makeLTRB(left + dx, top + dy, right + dx, bottom + dy);
    }

    constexpr void offsetX(Scalar dx) {
        left += dx;
        right += dx;
    }

    constexpr void offsetY(Scalar dy) {
        top += dy;
        bottom += dy;
    }

    constexpr void offset(Scalar dx, Scalar dy) {
        offsetX(dx);
        offsetY(dy);
    }

    constexpr Rect withInsets(Scalar horizontalInsets, Scalar verticalInsets) const {
        return Rect::makeLTRB(
            left + horizontalInsets, top + verticalInsets, right - horizontalInsets, bottom - verticalInsets);
    }

    /**
     Returns a new rect which will scale and position the given size
     in this rect coordinates using the given mode.
     */
    Rect makeFittingSize(Size size, FittingSizeMode mode) const;

    void join(const Rect& other);

    Rect intersection(const Rect& other) const;

    bool intersects(const Rect& other) const;

    /**
     Returns true if the two rectangles intersect, using a small tolerance for floating-point
     comparison. This is useful for damage rect merging where rects computed through different
     transformation paths may have tiny numerical differences but should still be considered
     as intersecting.
     */
    bool intersectsWithTolerance(const Rect& other) const;

    bool contains(const Point& point) const;

    /**
     Return the closest point to the given point that is within the rectangle bounds.
     */
    Point closestPoint(const Point& toPoint) const;

    /**
     Returns a new rect with outward rounding to ensure all fractional pixels are included.
     This is useful for damage rect calculations where missing sub-pixels can cause artifacts.
     */
    Rect makeOutset() const {
        return Rect::makeLTRB(std::floor(left), std::floor(top), std::ceil(right), std::ceil(bottom));
    }
};

std::ostream& operator<<(std::ostream& os, const Vector& vec);
std::ostream& operator<<(std::ostream& os, const Size& size);
std::ostream& operator<<(std::ostream& os, const Point& point);
std::ostream& operator<<(std::ostream& os, const Rect& rect);

} // namespace snap::drawing
