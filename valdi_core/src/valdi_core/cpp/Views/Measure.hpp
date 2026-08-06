//
//  Measure.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 4/26/21.
//

#pragma once

#include <cstdint>
#include <utility>

namespace Valdi {

enum MeasureMode { MeasureModeUnspecified = 0, MeasureModeExactly = 1, MeasureModeAtMost = 2 };

int32_t pointsToPixels(float point, float pointScale);
float pixelsToPoints(int32_t pixels, float pointScale);
float roundToPixelGrid(float point, float pointScale);

float sanitizeMeasurement(float constrainedSize, float measuredSize, float pointScale, MeasureMode measureMode);

std::pair<int32_t, int32_t> unpackIntPair(int64_t compressed);
int64_t packIntPair(int32_t horizontal, int32_t vertical);

int64_t pointsToPackedPixels(float horizontal, float vertical, float pointScale);

class CoordinateResolver {
public:
    explicit CoordinateResolver(float pointScale);

    float getPointScale() const;

    int32_t toPixels(float point) const;
    float toPixelsF(float point) const;
    float toPoints(int32_t pixels) const;

private:
    float _pointScale;
};

} // namespace Valdi
