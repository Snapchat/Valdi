//
//  Frame.cpp
//  Valdi
//
//  Created by Simon Corsin on 10/5/18.
//

#include "valdi_core/cpp/Views/Frame.hpp"
#include "valdi_core/cpp/Views/Measure.hpp"

#include "valdi_core/cpp/Utils/StringCache.hpp"

#include <fmt/format.h>

namespace Valdi {

STRING_CONST(keyX, "x")
STRING_CONST(keyY, "y")
STRING_CONST(keyWidth, "width")
STRING_CONST(keyHeight, "height")

Point::Point(const ValueMap& mapRepresentation) {
    auto it = mapRepresentation.find(keyX());
    if (it != mapRepresentation.end()) {
        x = it->second.toFloat();
    }
    it = mapRepresentation.find(keyY());
    if (it != mapRepresentation.end()) {
        y = it->second.toFloat();
    }
}

std::string Point::toString() const {
    return fmt::format("x: {}, y: {}", x, y);
}

Ref<ValueMap> Point::toMap() const {
    auto out = makeShared<ValueMap>();
    (*out)[keyX()] = Value(x);
    (*out)[keyY()] = Value(y);
    return out;
}

Point Point::withOffset(const Point& other) const {
    return Point(x + other.x, y + other.y);
}

Point Point::fromPixels(int32_t width, int32_t height, float pointScale) {
    return Point(pixelsToPoints(width, pointScale), pixelsToPoints(height, pointScale));
}

std::string Size::toString() const {
    return fmt::format("width: {}, height: {}", width, height);
}

Size Size::fromPixels(int32_t width, int32_t height, float pointScale) {
    return Size(pixelsToPoints(width, pointScale), pixelsToPoints(height, pointScale));
}

Size Size::fromPackedPixels(int64_t packed, float pointScale) {
    auto pair = unpackIntPair(packed);
    return fromPixels(pair.first, pair.second, pointScale);
}

Frame::Frame() = default;
Frame::Frame(float x, float y, float width, float height) : x(x), y(y), width(width), height(height) {}

Frame::Frame(const ValueMap& mapRepresentation) {
    auto it = mapRepresentation.find(keyX());
    if (it != mapRepresentation.end()) {
        x = it->second.toFloat();
    }
    it = mapRepresentation.find(keyY());
    if (it != mapRepresentation.end()) {
        y = it->second.toFloat();
    }
    it = mapRepresentation.find(keyWidth());
    if (it != mapRepresentation.end()) {
        width = it->second.toFloat();
    }
    it = mapRepresentation.find(keyHeight());
    if (it != mapRepresentation.end()) {
        height = it->second.toFloat();
    }
}

float Frame::getLeft() const {
    return x;
}

float Frame::getTop() const {
    return y;
}

float Frame::getRight() const {
    return x + width;
}

float Frame::getBottom() const {
    return y + height;
}

Size Frame::size() const {
    return Size(width, height);
}

Point Frame::location() const {
    return Point(x, y);
}

void Frame::setLeft(float left) {
    auto diff = x - left;
    x = left;
    width += diff;
}

void Frame::setTop(float top) {
    auto diff = y - top;
    y = top;
    height += diff;
}

void Frame::setRight(float right) {
    width = right - x;
}

void Frame::setBottom(float bottom) {
    height = bottom - y;
}

bool Frame::intersects(const Valdi::Frame& otherFrame) const {
    if (x >= otherFrame.getRight()) {
        return false;
    }

    if (y >= otherFrame.getBottom()) {
        return false;
    }

    if (getRight() <= otherFrame.x) {
        return false;
    }

    if (getBottom() <= otherFrame.y) {
        return false;
    }

    return true;
}

bool Frame::isEmpty() const {
    return x == 0 && y == 0 && width == 0 && height == 0;
}

Frame Frame::withOffset(float offsetX, float offsetY) const {
    return Frame(x + offsetX, y + offsetY, width, height);
}

Frame Frame::intersection(const Valdi::Frame& otherFrame) const {
    auto x = std::max(this->x, otherFrame.x);
    auto y = std::max(this->y, otherFrame.y);
    auto width = std::min(getRight(), otherFrame.getRight()) - x;
    auto height = std::min(getBottom(), otherFrame.getBottom()) - y;

    if (width < 0.0f || height < 0.0f) {
        // Rectangles don't intersect
        return Frame();
    }

    return Frame(x, y, width, height);
}

bool Frame::contains(const Valdi::Point& point) const {
    auto x = this->x;
    auto y = this->y;
    if (point.x < x || point.x > x + this->width) {
        return false;
    }
    if (point.y < y || point.y > y + this->height) {
        return false;
    }
    return true;
}

bool Frame::operator==(const Valdi::Frame& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
}

bool Frame::operator!=(const Valdi::Frame& other) const {
    return !(*this == other);
}

std::string Frame::toString() const {
    return fmt::format("x: {}, y: {}, width: {}, height: {}", x, y, width, height);
}

Ref<ValueMap> Frame::toMap() const {
    auto out = makeShared<ValueMap>();
    (*out)[keyX()] = Value(x);
    (*out)[keyY()] = Value(y);
    (*out)[keyWidth()] = Value(width);
    (*out)[keyHeight()] = Value(height);

    return out;
}

bool Matrix::isIdentity() const {
    return values[0] == 1.0f && values[1] == 0.0f && values[2] == 0.0f && values[3] == 1.0f && values[4] == 0.0f &&
           values[5] == 0.0f;
}

std::ostream& operator<<(std::ostream& os, const Valdi::Frame& frame) {
    return os << frame.toString();
}

std::ostream& operator<<(std::ostream& os, const Valdi::Point& point) {
    return os << point.toString();
}

std::ostream& operator<<(std::ostream& os, const Valdi::Size& size) {
    return os << size.toString();
}

} // namespace Valdi
