//
//  RadialGradient.cpp
//  snap_drawing
//
//  Created by Brandon Francis on 3/7/23.
//

#include "snap_drawing/cpp/Drawing/RadialGradient.hpp"
#include "include/effects/SkGradientShader.h"
#include <cmath>

namespace snap::drawing {

RadialGradient::RadialGradient() = default;

RadialGradient::~RadialGradient() = default;

bool RadialGradient::isDirty() const {
    return _dirty;
}

bool RadialGradient::isEmpty() const {
    return _colors.empty();
}

void RadialGradient::setLocations(std::vector<Scalar>&& locations) {
    if (_locations != locations) {
        _locations = std::move(locations);
        _dirty = true;
    }
}

void RadialGradient::setColors(std::vector<Color>&& colors) {
    if (_colors != colors) {
        _colors = std::move(colors);
        _dirty = true;
    }
}

void RadialGradient::updateShader(const Rect& bounds) {
    if (isEmpty()) {
        _shader = nullptr;
    } else {
        auto width = bounds.width();
        auto height = bounds.height();
        auto radius = std::min(width / 2, height / 2);

        const auto* colorsData = reinterpret_cast<const SkColor*>(_colors.data());

        SkMatrix localMatrix;
        if (width != height) {
            localMatrix.postScale(width / height, 1);
        }
        localMatrix.postTranslate(bounds.x() + (width / 2), bounds.y() + (height / 2));

        sk_sp<SkShader> shader;
        if (_colors.size() == _locations.size()) {
            // User provided color distribution

            _shader = SkGradientShader::MakeRadial(Point::make(0, 0).getSkValue(),
                                                   radius,
                                                   colorsData,
                                                   _locations.data(),
                                                   static_cast<int>(_colors.size()),
                                                   SkTileMode::kClamp,
                                                   0,
                                                   &localMatrix);
        } else {
            // Distribute colors evenly
            _shader = SkGradientShader::MakeRadial(Point::make(0, 0).getSkValue(),
                                                   radius,
                                                   colorsData,
                                                   nullptr,
                                                   static_cast<int>(_colors.size()),
                                                   SkTileMode::kClamp,
                                                   0,
                                                   &localMatrix);
        }
    }
}

void RadialGradient::applyToPaint(Paint& paint) const {
    paint.getSkValue().setShader(_shader);
}

void RadialGradient::update(const Rect& bounds) {
    if (_dirty || _lastDrawBounds != bounds) {
        updateShader(bounds);

        _lastDrawBounds = bounds;
        _dirty = false;
    }
}

void RadialGradient::draw(DrawingContext& drawingContext, const BorderRadius& borderRadius) {
    update(drawingContext.drawBounds());

    if (isEmpty()) {
        return;
    }

    Paint paint;
    applyToPaint(paint);
    // As for a solid background: without it, a rounded corner's edge is clipped to whole pixels.
    paint.setAntiAlias(true);

    drawingContext.drawPaint(paint, borderRadius, _lazyPath);
}

} // namespace snap::drawing
