//
//  ShapeLayer.hpp
//  snap_drawing-macos
//
//  Created by Simon Corsin on 3/4/22.
//

#pragma once

#include "snap_drawing/cpp/Drawing/Paint.hpp"
#include "snap_drawing/cpp/Layers/Layer.hpp"
#include "snap_drawing/cpp/Utils/GradientWrapper.hpp"
#include "snap_drawing/cpp/Utils/Path.hpp"
#include "snap_drawing/cpp/Utils/PathInterpolator.hpp"

#include <vector>

namespace snap::drawing {

class MaskFilter;

class ShapeLayer : public Layer {
public:
    explicit ShapeLayer(const Ref<Resources>& resources);
    ~ShapeLayer() override;

    void onInitialize() override;

    void setPath(Path&& path);

    void setStrokeWidth(Scalar strokeWidth);
    Scalar getStrokeWidth() const;

    void setStrokeColor(Color strokeColor);
    Color getStrokeColor() const;

    void setFillColor(Color fillColor);
    Color getFillColor() const;

    void setFillLinearGradient(std::vector<Scalar>&& locations,
                               std::vector<Color>&& colors,
                               LinearGradientOrientation orientation);
    void setFillRadialGradient(std::vector<Scalar>&& locations, std::vector<Color>&& colors);
    void resetFillGradient();

    void setStrokeCap(PaintStrokeCap strokeCap);
    void setStrokeJoin(PaintStrokeJoin strokeJoin);

    void setStrokeStart(Scalar strokeStart);
    Scalar getStrokeStart() const;

    void setStrokeEnd(Scalar strokeEnd);
    Scalar getStrokeEnd() const;

    void setMaskFilter(const MaskFilter& maskFilter);

protected:
    void onDraw(DrawingContext& drawingContext) override;

private:
    Path _path;
    Paint _strokePaint;
    Paint _fillPaint;
    GradientWrapper _fillGradientWrapper;
    Scalar _strokeStart = 0.0f;
    Scalar _strokeEnd = 1.0f;
    std::optional<PathInterpolator> _pathInterpolator;

    const Path& getActivePath();
};

} // namespace snap::drawing
