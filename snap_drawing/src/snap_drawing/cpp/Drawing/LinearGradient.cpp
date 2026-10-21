//
//  LinearGradient.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 7/8/20.
//

#include "snap_drawing/cpp/Drawing/LinearGradient.hpp"
#include "include/effects/SkGradientShader.h"
#include <cmath>

namespace snap::drawing {

LinearGradient::LinearGradient() = default;

LinearGradient::~LinearGradient() = default;

bool LinearGradient::isDirty() const {
    return _dirty;
}

bool LinearGradient::isEmpty() const {
    return _colors.empty();
}

void LinearGradient::setLocations(std::vector<Scalar>&& locations) {
    if (_locations != locations) {
        _locations = std::move(locations);
        _dirty = true;
    }
}

void LinearGradient::setColors(std::vector<Color>&& colors) {
    if (_colors != colors) {
        _colors = std::move(colors);
        _dirty = true;
    }
}

void LinearGradient::setOrientation(LinearGradientOrientation orientation) {
    if (_orientation != orientation) {
        _orientation = orientation;
        _dirty = true;
    }
}

void LinearGradient::updateShader(const Rect& bounds) {
    if (isEmpty()) {
        _shader = nullptr;
    } else {
        auto halfWidth = bounds.x() + (bounds.width() / 2);
        auto halfHeight = bounds.y() + (bounds.height() / 2);

        Point points[2];

        switch (_orientation) {
            case LinearGradientOrientationTopBottom:
                points[0] = Point::make(halfWidth, bounds.top);
                points[1] = Point::make(halfWidth, bounds.bottom);
                break;
            case LinearGradientOrientationTopRightBottomLeft:
                points[0] = Point::make(bounds.right, bounds.top);
                points[1] = Point::make(bounds.left, bounds.bottom);
                break;
            case LinearGradientOrientationRightLeft:
                points[0] = Point::make(bounds.right, halfHeight);
                points[1] = Point::make(bounds.left, halfHeight);
                break;
            case LinearGradientOrientationBottomRightTopLeft:
                points[0] = Point::make(bounds.right, bounds.bottom);
                points[1] = Point::make(bounds.left, bounds.top);
                break;
            case LinearGradientOrientationBottomTop:
                points[0] = Point::make(halfWidth, bounds.bottom);
                points[1] = Point::make(halfWidth, bounds.top);
                break;
            case LinearGradientOrientationBottomLeftTopRight:
                points[0] = Point::make(bounds.left, bounds.bottom);
                points[1] = Point::make(bounds.right, bounds.top);
                break;
            case LinearGradientOrientationLeftRight:
                points[0] = Point::make(bounds.left, halfHeight);
                points[1] = Point::make(bounds.right, halfHeight);
                break;
            case LinearGradientOrientationTopLeftBottomRight:
                points[0] = Point::make(bounds.left, bounds.top);
                points[1] = Point::make(bounds.right, bounds.bottom);
                break;
            default:
                points[0] = Point::make(halfWidth, bounds.top);
                points[1] = Point::make(halfWidth, bounds.bottom);
        }

        const auto* colorsData = reinterpret_cast<const SkColor*>(_colors.data());

        sk_sp<SkShader> shader;
        if (_colors.size() == _locations.size()) {
            // User provided color distribution

            _shader = SkGradientShader::MakeLinear(&points[0].getSkValue(),
                                                   colorsData,
                                                   _locations.data(),
                                                   static_cast<int>(_colors.size()),
                                                   SkTileMode::kClamp);
        } else {
            // Distribute colors evenly
            _shader = SkGradientShader::MakeLinear(
                &points[0].getSkValue(), colorsData, nullptr, static_cast<int>(_colors.size()), SkTileMode::kClamp);
        }
    }
}

void LinearGradient::applyToPaint(Paint& paint) const {
    paint.getSkValue().setShader(_shader);
}

void LinearGradient::update(const Rect& bounds) {
    if (_dirty || _lastDrawBounds != bounds) {
        updateShader(bounds);

        _lastDrawBounds = bounds;
        _dirty = false;
    }
}

void LinearGradient::draw(DrawingContext& drawingContext, const BorderRadius& borderRadius) {
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
