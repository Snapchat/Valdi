#include "valdi/android/snap_drawing/AndroidSnapDrawingUtils.hpp"
#include "snap_drawing/cpp/Utils/Matrix.hpp"
#include "snap_drawing/cpp/Utils/Path.hpp"
#include "valdi_core/cpp/Views/Measure.hpp"
#include "valdi_core/cpp/Utils/GeometricPath.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

namespace ValdiAndroid {

struct SerializePathVisitor : public snap::drawing::PathVisitor {
    static constexpr float kComponentMove = static_cast<float>(Valdi::GeometricPathComponentTypeMove);
    static constexpr float kComponentLine = static_cast<float>(Valdi::GeometricPathComponentTypeLine);
    static constexpr float kComponentQuad = static_cast<float>(Valdi::GeometricPathComponentTypeQuad);
    static constexpr float kComponentCubic = static_cast<float>(Valdi::GeometricPathComponentTypeCubic);
    static constexpr float kComponentClose = static_cast<float>(Valdi::GeometricPathComponentTypeClose);

    Valdi::SmallVector<float, 16> values;
    Valdi::CoordinateResolver coordinateResolver;

    explicit SerializePathVisitor(const Valdi::CoordinateResolver& coordinateResolver)
        : coordinateResolver(coordinateResolver) {}

    bool shouldConvertConicToQuad() const override {
        return true;
    }

    void appendPoint(const snap::drawing::Point& point) {
        values.emplace_back(coordinateResolver.toPixelsF(point.x));
        values.emplace_back(coordinateResolver.toPixelsF(point.y));
    }

    void move(const snap::drawing::Point& point) override {
        values.emplace_back(kComponentMove);
        appendPoint(point);
    }

    void line(const snap::drawing::Point& from, const snap::drawing::Point& to) override {
        values.emplace_back(kComponentLine);
        appendPoint(to);
    }

    void quad(const snap::drawing::Point& p1, const snap::drawing::Point& p2, const snap::drawing::Point& p3) override {
        values.emplace_back(kComponentQuad);
        appendPoint(p2);
        appendPoint(p3);
    }

    void conic(const snap::drawing::Point& p1,
               const snap::drawing::Point& p2,
               const snap::drawing::Point& p3,
               snap::drawing::Scalar weight) override {}

    void cubic(const snap::drawing::Point& p1,
               const snap::drawing::Point& p2,
               const snap::drawing::Point& p3,
               const snap::drawing::Point& p4) override {
        values.emplace_back(kComponentCubic);
        appendPoint(p2);
        appendPoint(p3);
        appendPoint(p4);
    }

    void close() override {
        values.emplace_back(kComponentClose);
    }
};

JavaObject createTransformJavaArray(JavaEnv env,
                                    const snap::drawing::Matrix& matrix,
                                    const Valdi::CoordinateResolver& coordinateResolver) {
    static_assert(sizeof(snap::drawing::Scalar) == sizeof(float));
    snap::drawing::Scalar values[9];
    matrix.getAll(values);
    values[SkMatrix::kMTransX] = coordinateResolver.toPixelsF(values[SkMatrix::kMTransX]);
    values[SkMatrix::kMTransY] = coordinateResolver.toPixelsF(values[SkMatrix::kMTransY]);
    return toJavaFloatArray(env, values, sizeof(values) / sizeof(snap::drawing::Scalar));
}

JavaObject createPathJavaArray(JavaEnv env,
                               const snap::drawing::Path& path,
                               const Valdi::CoordinateResolver& coordinateResolver) {
    SerializePathVisitor visitor(coordinateResolver);
    path.visit(visitor);
    return toJavaFloatArray(env, visitor.values.data(), visitor.values.size());
}

} // namespace ValdiAndroid