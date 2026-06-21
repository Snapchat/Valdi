//
//  Paint.cpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/28/22.
//

#include "snap_drawing/cpp/Drawing/Paint.hpp"
#include "snap_drawing/cpp/Drawing/MaskFilter.hpp"
#include "snap_drawing/cpp/Drawing/Shader.hpp"

#include "include/core/SkColorFilter.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathEffect.h"
#include "include/effects/Sk1DPathEffect.h"
#include "include/effects/SkDashPathEffect.h"
#include <utility>

namespace snap::drawing {

Paint::Paint() = default;

Paint::Paint(const Paint& other) : Paint() {
    *this = other;
}

Paint::~Paint() = default;

Paint& Paint::operator=(const Paint& other) {
    if (&other != this) {
        getSkValue() = other.getSkValue();
    }

    return *this;
}

void Paint::setColor(Color color) {
    getSkValue().setColor(color.getSkValue());
}

Color Paint::getColor() const {
    return fromSkValue<Color>(getSkValue().getColor());
}

void Paint::setAlpha(float alpha) {
    getSkValue().setAlphaf(alpha);
}

void Paint::setBlendColorFilter(Color color) {
    if (color == Color::transparent()) {
        getSkValue().setColorFilter(nullptr);
    } else {
        getSkValue().setColorFilter(SkColorFilters::Blend(color.getSkValue(), SkBlendMode::kSrcATop));
    }
}

void Paint::setStroke(bool stroke) {
    getSkValue().setStroke(stroke);
}

void Paint::setStrokeWidth(Scalar strokeWidth) {
    getSkValue().setStrokeWidth(strokeWidth);
}

Scalar Paint::getStrokeWidth() const {
    return getSkValue().getStrokeWidth();
}

void Paint::setStrokeJoin(PaintStrokeJoin strokeJoin) {
    getSkValue().setStrokeJoin(static_cast<SkPaint::Join>(strokeJoin));
}

void Paint::setStrokeCap(PaintStrokeCap strokeCap) {
    getSkValue().setStrokeCap(static_cast<SkPaint::Cap>(strokeCap));
}

void Paint::setBlendMode(SkBlendMode blendMode) {
    getSkValue().setBlendMode(blendMode);
}

void Paint::setAntiAlias(bool antialias) {
    getSkValue().setAntiAlias(antialias);
}

void Paint::setShader(const Shader& shader) {
    getSkValue().setShader(shader.getSkValue());
}

void Paint::setMaskFilter(const MaskFilter& maskFilter) {
    getSkValue().setMaskFilter(maskFilter.getSkValue());
}

void Paint::setStrokeDashPattern(Scalar onLength, Scalar offLength) {
    SkScalar intervals[] = {static_cast<SkScalar>(onLength), static_cast<SkScalar>(offLength)};
    getSkValue().setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
}

void Paint::setStrokeDotPattern(Scalar radius, Scalar advance) {
    SkPath dot;
    dot.addCircle(0, 0, static_cast<SkScalar>(radius));
    getSkValue().setPathEffect(
        SkPath1DPathEffect::Make(dot, static_cast<SkScalar>(advance), 0, SkPath1DPathEffect::kTranslate_Style));
}

} // namespace snap::drawing
