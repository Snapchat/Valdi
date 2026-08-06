//
//  Frame.hpp
//  Valdi
//
//  Created by Simon Corsin on 10/5/18.
//

#pragma once

#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include <ostream>
#include <string>

namespace Valdi {

struct Size {
    float width = 0;
    float height = 0;

    constexpr Size(float width, float height) : width(width), height(height) {}
    constexpr Size() = default;

    inline bool operator==(const Size& other) const {
        return width == other.width && height == other.height;
    }

    inline bool operator!=(const Size& other) const {
        return !(*this == other);
    }

    inline bool isEmpty() const {
        return width == 0.0f && height == 0.0f;
    }

    std::string toString() const;

    static Size fromPixels(int32_t width, int32_t height, float pointScale);
    static Size fromPackedPixels(int64_t packed, float pointScale);
};

struct Point {
    float x = 0;
    float y = 0;

    constexpr Point(float x, float y) : x(x), y(y) {}
    constexpr Point() = default;
    explicit Point(const ValueMap& mapRepresentation);

    inline bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    inline bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    inline Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }

    inline Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }

    inline Point& operator+=(const Point& other) {
        this->x += other.x;
        this->y += other.y;
        return *this;
    }

    inline Point& operator-=(const Point& other) {
        this->x -= other.x;
        this->y -= other.y;
        return *this;
    }

    Point withOffset(const Point& other) const;

    std::string toString() const;

    Ref<ValueMap> toMap() const;

    static Point fromPixels(int32_t width, int32_t height, float pointScale);
};

struct Frame {
    float x = 0.0;
    float y = 0.0;
    float width = 0.0;
    float height = 0.0;

    Frame();
    Frame(float x, float y, float width, float height);
    explicit Frame(const ValueMap& mapRepresentation);

    float getLeft() const;
    float getTop() const;
    float getRight() const;
    float getBottom() const;

    Size size() const;
    Point location() const;

    void setLeft(float left);
    void setTop(float top);
    void setRight(float right);
    void setBottom(float bottom);

    bool isEmpty() const;

    bool intersects(const Frame& otherFrame) const;

    Frame intersection(const Frame& otherFrame) const;
    Frame withOffset(float offsetX, float offsetY) const;

    bool contains(const Point& point) const;

    std::string toString() const;
    Ref<ValueMap> toMap() const;

    bool operator==(const Frame& other) const;
    bool operator!=(const Frame& other) const;
};

struct Matrix {
    float values[6] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    bool isIdentity() const;
};

std::ostream& operator<<(std::ostream& os, const Valdi::Frame& frame);
std::ostream& operator<<(std::ostream& os, const Valdi::Point& point);
std::ostream& operator<<(std::ostream& os, const Valdi::Size& size);
} // namespace Valdi
