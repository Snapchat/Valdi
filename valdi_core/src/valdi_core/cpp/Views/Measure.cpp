//
//  Measure.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 4/26/21.
//

#include "valdi_core/cpp/Views/Measure.hpp"

#include <algorithm>
#include <cmath>

namespace Valdi {

int32_t pointsToPixels(float point, float pointScale) {
    return static_cast<int32_t>(round(point * pointScale));
}

float pixelsToPoints(int32_t pixels, float pointScale) {
    return static_cast<float>(pixels) / pointScale;
}

float roundToPixelGrid(float point, float pointScale) {
    return roundf(point * pointScale) / pointScale;
}

float sanitizeMeasurement(float constrainedSize, float measuredSize, float pointScale, MeasureMode measureMode) {
    measuredSize = ceilf(measuredSize * pointScale) / pointScale;

    if (measureMode == MeasureModeExactly) {
        return constrainedSize;
    } else if (measureMode == MeasureModeAtMost) {
        return std::min(constrainedSize, measuredSize);
    } else {
        return measuredSize;
    }
}

std::pair<int32_t, int32_t> unpackIntPair(int64_t compressed) {
    return std::make_pair(static_cast<int32_t>(compressed >> 32 & 0xFFFFFFFF),
                          static_cast<int32_t>(compressed & 0xFFFFFFFF));
}

int64_t packIntPair(int32_t horizontal, int32_t vertical) {
    int64_t out;
    *(reinterpret_cast<int32_t*>(&out)) = vertical;
    *(reinterpret_cast<int32_t*>(&out) + 1) = horizontal;
    return out;
}

int64_t pointsToPackedPixels(float horizontal, float vertical, float pointScale) {
    auto horizontalP = pointsToPixels(horizontal, pointScale);
    auto verticalP = pointsToPixels(vertical, pointScale);
    return packIntPair(horizontalP, verticalP);
}

CoordinateResolver::CoordinateResolver(float pointScale) : _pointScale(pointScale) {}

float CoordinateResolver::getPointScale() const {
    return _pointScale;
}

int32_t CoordinateResolver::toPixels(float point) const {
    return pointsToPixels(point, _pointScale);
}

float CoordinateResolver::toPixelsF(float point) const {
    return point * _pointScale;
}

float CoordinateResolver::toPoints(int32_t pixels) const {
    return pixelsToPoints(pixels, _pointScale);
}

} // namespace Valdi
