//
//  ShapeLayer.cpp
//  snap_drawing-macos
//
//  Created by Simon Corsin on 3/4/22.
//

#include "snap_drawing/cpp/Layers/ShapeLayer.hpp"
#include "snap_drawing/cpp/Utils/PathInterpolator.hpp"

namespace snap::drawing {

ShapeLayer::ShapeLayer(const Ref<Resources>& resources) : Layer(resources) {}
ShapeLayer::~ShapeLayer() = default;

void ShapeLayer::onInitialize() {
    _strokePaint.setAntiAlias(true);
    _strokePaint.setStroke(true);
    _fillPaint.setAntiAlias(true);
    _fillPaint.setStroke(false);

    setStrokeCap(PaintStrokeCapButt);
    setStrokeJoin(PaintStrokeJoinMiter);
    setFillColor(Color::transparent());
    setStrokeColor(Color::transparent());
    setStrokeWidth(1);
}

void ShapeLayer::onDraw(DrawingContext& drawingContext) {
    const auto& path = getActivePath();
    auto fillPaint = _fillPaint;
    if (_fillGradientWrapper.hasGradient()) {
        fillPaint.setColor(Color::black());
        _fillGradientWrapper.update(path.getBounds().value_or(drawingContext.drawBounds()));
        _fillGradientWrapper.applyToPaint(fillPaint);
    }
    drawingContext.drawPaint(fillPaint, path);
    drawingContext.drawPaint(_strokePaint, path);
}

const Path& ShapeLayer::getActivePath() {
    if (_strokeStart == 0.0f && _strokeEnd == 1.0f) {
        return _path;
    }

    if (!_pathInterpolator) {
        _pathInterpolator.emplace(_path);
    }

    return _pathInterpolator.value().interpolate(_strokeStart, _strokeEnd);
}

void ShapeLayer::setPath(Path&& path) {
    _path = std::move(path);
    _pathInterpolator = std::nullopt;
    setNeedsDisplay();
}

Scalar ShapeLayer::getStrokeWidth() const {
    return _strokePaint.getStrokeWidth();
}

void ShapeLayer::setStrokeWidth(Scalar strokeWidth) {
    _strokePaint.setStrokeWidth(strokeWidth);
    setNeedsDisplay();
}

Color ShapeLayer::getStrokeColor() const {
    return _strokePaint.getColor();
}

void ShapeLayer::setStrokeColor(Color strokeColor) {
    _strokePaint.setColor(strokeColor);
    setNeedsDisplay();
}

Color ShapeLayer::getFillColor() const {
    return _fillPaint.getColor();
}

void ShapeLayer::setFillColor(Color fillColor) {
    _fillPaint.setColor(fillColor);
    setNeedsDisplay();
}

void ShapeLayer::setFillLinearGradient(std::vector<Scalar>&& locations,
                                       std::vector<Color>&& colors,
                                       LinearGradientOrientation orientation) {
    if (_fillGradientWrapper.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_fillGradientWrapper.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
            setNeedsDisplay();
        }
        return;
    }

    _fillGradientWrapper.setAsLinear(std::move(locations), std::move(colors), orientation);

    if (_fillGradientWrapper.isDirty()) {
        setNeedsDisplay();
    }
}

void ShapeLayer::setFillRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors) {
    if (_fillGradientWrapper.clearIfNeeded(GradientWrapper::GradientType::LINEAR)) {
        setNeedsDisplay();
    }

    if (colors.empty()) {
        if (_fillGradientWrapper.clearIfNeeded(GradientWrapper::GradientType::RADIAL)) {
            setNeedsDisplay();
        }
        return;
    }

    _fillGradientWrapper.setAsRadial(std::move(locations), std::move(colors));

    if (_fillGradientWrapper.isDirty()) {
        setNeedsDisplay();
    }
}

void ShapeLayer::resetFillGradient() {
    _fillGradientWrapper.clear();
    setNeedsDisplay();
}

void ShapeLayer::setStrokeCap(PaintStrokeCap strokeCap) {
    _strokePaint.setStrokeCap(strokeCap);
    setNeedsDisplay();
}

void ShapeLayer::setStrokeJoin(PaintStrokeJoin strokeJoin) {
    _strokePaint.setStrokeJoin(strokeJoin);
    setNeedsDisplay();
}

Scalar ShapeLayer::getStrokeStart() const {
    return _strokeStart;
}

void ShapeLayer::setStrokeStart(Scalar strokeStart) {
    if (_strokeStart != strokeStart) {
        _strokeStart = strokeStart;
        setNeedsDisplay();
    }
}

Scalar ShapeLayer::getStrokeEnd() const {
    return _strokeEnd;
}

void ShapeLayer::setStrokeEnd(Scalar strokeEnd) {
    if (_strokeEnd != strokeEnd) {
        _strokeEnd = strokeEnd;
        setNeedsDisplay();
    }
}

void ShapeLayer::setMaskFilter(const MaskFilter& maskFilter) {
    _strokePaint.setMaskFilter(maskFilter);
    _fillPaint.setMaskFilter(maskFilter);
}

} // namespace snap::drawing
