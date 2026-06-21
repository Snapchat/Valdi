//
//  Paint.hpp
//  snap_drawing
//
//  Created by Simon Corsin on 1/28/22.
//

#pragma once

#include "snap_drawing/cpp/Utils/Color.hpp"
#include "snap_drawing/cpp/Utils/SkiaBridge.hpp"

#include "include/core/SkPaint.h"

namespace snap::drawing {

enum PaintStrokeCap {
    PaintStrokeCapButt = SkPaint::Cap::kButt_Cap,
    PaintStrokeCapRound = SkPaint::Cap::kRound_Cap,
    PaintStrokeCapSquare = SkPaint::Cap::kSquare_Cap,
};

enum PaintStrokeJoin {
    PaintStrokeJoinMiter = SkPaint::Join::kMiter_Join,
    PaintStrokeJoinRound = SkPaint::Join::kRound_Join,
    PaintStrokeJoinBevel = SkPaint::Join::kBevel_Join,
};

class Shader;
class MaskFilter;

class Paint : public WrappedSkValue<SkPaint> {
public:
    Paint();
    Paint(const Paint& other);

    ~Paint();

    Paint& operator=(const Paint& other);

    Color getColor() const;
    void setColor(Color color);
    void setAlpha(float alpha);
    void setBlendColorFilter(Color color);
    void setStroke(bool stroke);

    void setStrokeWidth(Scalar strokeWidth);
    Scalar getStrokeWidth() const;

    void setStrokeCap(PaintStrokeCap strokeCap);
    void setStrokeJoin(PaintStrokeJoin strokeJoin);
    void setBlendMode(SkBlendMode blendMode);
    void setAntiAlias(bool antialias);

    void setShader(const Shader& shader);
    void setMaskFilter(const MaskFilter& maskFilter);
    void setStrokeDashPattern(Scalar onLength, Scalar offLength);
    void setStrokeDotPattern(Scalar radius, Scalar advance);
};

} // namespace snap::drawing
