//
//  AndroidViewHolder.cpp
//  valdi-android
//
//  Created by Simon Corsin on 4/26/21.
//

#include "valdi/android/AndroidViewHolder.hpp"
#include "AndroidBitmap.hpp"
#include "valdi/android/DeferredViewOperations.hpp"
#include "valdi/android/NativeBridge.hpp"
#include "valdi/android/ViewManager.hpp"
#include "valdi/android/snap_drawing/AndroidSnapDrawingUtils.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi_core/NativeAnimator.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/jni/JavaCache.hpp"
#include "valdi_core/jni/JavaUtils.hpp"

#include "valdi/android/AndroidBitmap.hpp"
#include "valdi/android/CppPromiseCallbackJNI.hpp"
#include "valdi/android/JavaValueDelegate.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"

#include "snap_drawing/cpp/Drawing/Surface/ExternalSurfacePresenterState.hpp"

namespace ValdiAndroid {

AndroidViewHolder::AndroidViewHolder(const JavaObject& object, ViewManager& viewManager)
    : GlobalRefJavaObjectBase(object, "View"), _viewManager(viewManager) {}
AndroidViewHolder::~AndroidViewHolder() = default;

int32_t AndroidViewHolder::convertPointsToPixels(float points) const {
    Valdi::CoordinateResolver resolver(_viewManager.getPointScale());
    return resolver.toPixels(points);
}

bool AndroidViewHolder::isRecyclable() {
    if (!_isRecyclable) {
        _isRecyclable = {JavaCache::get().getViewRefIsRecyclableMethod().call(toObject())};
    }
    return _isRecyclable.value();
}

snap::valdi_core::Platform AndroidViewHolder::getPlatform() const {
    return snap::valdi_core::Platform::Android;
}

bool AndroidViewHolder::onTouchEvent(long offsetSinceSourceMs,
                                     int touchEventType,
                                     float /*absoluteX*/,
                                     float /*absoluteY*/,
                                     float x,
                                     float y,
                                     float /*dx*/,
                                     float /*dy*/,
                                     bool /*isTouchOwner*/,
                                     const Valdi::Ref<Valdi::RefCountable>& source) {
    auto pixelX = static_cast<float>(convertPointsToPixels(x));
    auto pixelY = static_cast<float>(convertPointsToPixels(y));

    auto originalTouchEvent = javaObjectFromValdiObject(Valdi::castOrNull<Valdi::ValdiObject>(source));
    return JavaEnv::getCache().getViewRefOnTouchEventMethod().call(
        toObject(), offsetSinceSourceMs, touchEventType, pixelX, pixelY, originalTouchEvent);
}

Valdi::ViewHitTestResult AndroidViewHolder::hitTest(float x, float y) {
    auto pixelX = static_cast<float>(convertPointsToPixels(x));
    auto pixelY = static_cast<float>(convertPointsToPixels(y));
    auto result = JavaEnv::getCache().getViewRefCustomizedHitTestMethod().call(toObject(), pixelX, pixelY);

    return static_cast<Valdi::ViewHitTestResult>(result);
}

Valdi::Result<Valdi::Void> AndroidViewHolder::rasterInto(const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                                         const Valdi::Frame& frame,
                                                         const Valdi::Matrix& transform,
                                                         float rasterScaleX,
                                                         float rasterScaleY) {
    auto androidBitmap = Valdi::castOrNull<AndroidBitmap>(bitmap);
    SC_ASSERT_NOTNULL(androidBitmap);
    if (androidBitmap == nullptr) {
        return Valdi::Error("Invalid bitmap");
    }

    Valdi::CoordinateResolver resolver(_viewManager.getPointScale());
    auto left = resolver.toPixels(frame.getLeft());
    auto top = resolver.toPixels(frame.getTop());
    auto right = resolver.toPixels(frame.getRight());
    auto bottom = resolver.toPixels(frame.getBottom());

    auto javaTransform = JavaObject(JavaEnv());
    if (!transform.isIdentity()) {
        auto affineValues = transform.values;
        const float a = affineValues[0];
        const float b = affineValues[1];
        const float c = affineValues[2];
        const float d = affineValues[3];
        const float tx = resolver.toPixelsF(affineValues[4]);
        const float ty = resolver.toPixelsF(affineValues[5]);

        // Android Matrix.setValues expects 9 floats in row-major order:
        // [ MSCALE_X, MSKEW_X, MTRANS_X,
        //   MSKEW_Y, MSCALE_Y, MTRANS_Y,
        //   MPERSP_0, MPERSP_1, MPERSP_2 ]
        // Map from affine [a, b, c, d, tx, ty]
        float matrix9[9] = {a, c, tx, b, d, ty, 0.f, 0.f, 1.f};
        javaTransform = toJavaFloatArray(JavaEnv(), matrix9, 9);
    }

    auto errorMessage = JavaEnv::getCache().getViewRefRasterMethod().call(toObject(),
                                                                          androidBitmap->getJavaBitmap(),
                                                                          left,
                                                                          top,
                                                                          right,
                                                                          bottom,
                                                                          rasterScaleX,
                                                                          rasterScaleY,
                                                                          javaTransform);

    if (errorMessage.isNull()) {
        return Valdi::Void();
    }

    return Valdi::Error(toInternedString(JavaEnv(), errorMessage));
}

VALDI_CLASS_IMPL(AndroidViewHolder)

ViewType fromValdiView(const Valdi::Ref<Valdi::View>& view) {
    auto androidView = Valdi::castOrNull<AndroidViewHolder>(view);
    if (androidView == nullptr) {
        return ViewType(JavaEnv(), nullptr);
    }

    return ViewType(androidView->getEnv(), androidView->get());
}

Valdi::Ref<Valdi::View> toValdiView(const ViewType& viewType, ViewManager& viewManager) {
    return Valdi::makeShared<AndroidViewHolder>(viewType, viewManager);
}

} // namespace ValdiAndroid
