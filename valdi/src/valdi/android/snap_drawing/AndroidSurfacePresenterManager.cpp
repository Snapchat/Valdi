//
//  AndroidSurfacePresenterManager.cpp
//  valdi-android
//
//  Created by Simon Corsin on 2/15/22.
//

#include "AndroidSurfacePresenterManager.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/ViewManager.hpp"
#include "valdi/android/snap_drawing/AndroidSnapDrawingUtils.hpp"
#include "valdi/snap_drawing/BridgedView.hpp"
#include "valdi_core/jni/JavaCache.hpp"

namespace ValdiAndroid {

AndroidSurfacePresenterManager::AndroidSurfacePresenterManager(JavaEnv env,
                                                               jobject javaRepr,
                                                               ViewManager& viewManager)
    : GlobalRefJavaObjectBase(env, javaRepr, "SurfacePresenterManager"), _viewManager(viewManager) {}

AndroidSurfacePresenterManager::~AndroidSurfacePresenterManager() = default;

snap::drawing::Ref<snap::drawing::DrawableSurface> AndroidSurfacePresenterManager::createPresenterWithDrawableSurface(
    snap::drawing::SurfacePresenterId id, size_t zIndex) {
    JavaEnv::getCache().getSurfacePresenterManagerCreatePresenterWithDrawableSurfaceMethod().call(
        toObject(), static_cast<int32_t>(id), static_cast<int32_t>(zIndex));

    return nullptr;
}

void AndroidSurfacePresenterManager::createPresenterForExternalSurface(
    snap::drawing::SurfacePresenterId id, size_t zIndex, snap::drawing::ExternalSurface& externalSurface) {
    auto embeddedView = fromValdiView(dynamic_cast<snap::drawing::BridgedView&>(externalSurface).getView());
    JavaEnv::getCache().getSurfacePresenterManagerCreatePresenterForEmbeddedViewMethod().call(
        toObject(), static_cast<int32_t>(id), static_cast<int32_t>(zIndex), embeddedView);
}

void AndroidSurfacePresenterManager::setSurfacePresenterZIndex(snap::drawing::SurfacePresenterId id, size_t zIndex) {
    JavaEnv::getCache().getSurfacePresenterManagerSetPresenterZIndexMethod().call(
        toObject(), static_cast<int32_t>(id), static_cast<int32_t>(zIndex));
}

void AndroidSurfacePresenterManager::removeSurfacePresenter(snap::drawing::SurfacePresenterId id) {
    _lastPointScales.erase(id);
    JavaEnv::getCache().getSurfacePresenterManagerRemovePresenterMethod().call(toObject(), static_cast<int32_t>(id));
}

void AndroidSurfacePresenterManager::setExternalSurfacePresenterState(
    snap::drawing::SurfacePresenterId id,
    const snap::drawing::ExternalSurfacePresenterState& presenterState,
    const snap::drawing::ExternalSurfacePresenterState* previousPresenterState) {
    float currentPointScale = _viewManager.getPointScale();
    auto [it, inserted] = _lastPointScales.try_emplace(id, 0.0f);
    bool pointScaleChanged = it->second != currentPointScale;
    it->second = currentPointScale;

    Valdi::CoordinateResolver resolver(currentPointScale);

    bool forceFullUpdate = previousPresenterState == nullptr || pointScaleChanged;

    JavaObject transform(getEnv());
    bool transformChanged = false;

    if (forceFullUpdate || previousPresenterState->transform != presenterState.transform) {
        transformChanged = true;
        transform = createTransformJavaArray(getEnv(), presenterState.transform, resolver);
    }

    JavaObject clipPath(getEnv());
    bool clipPathChanged = false;

    if (forceFullUpdate || previousPresenterState->clipPath != presenterState.clipPath) {
        clipPathChanged = true;
        if (!presenterState.clipPath.isEmpty()) {
            clipPath = createPathJavaArray(getEnv(), presenterState.clipPath, resolver);
        }
    }

    JavaEnv::getCache().getSurfacePresenterManagerSetEmbeddedViewPresenterStateMethod().call(
        toObject(),
        static_cast<int32_t>(id),
        resolver.toPixels(presenterState.frame.left),
        resolver.toPixels(presenterState.frame.top),
        resolver.toPixels(presenterState.frame.right),
        resolver.toPixels(presenterState.frame.bottom),
        presenterState.opacity,
        transform,
        transformChanged,
        clipPath,
        clipPathChanged);
}

void AndroidSurfacePresenterManager::onDrawableSurfacePresenterUpdated(snap::drawing::SurfacePresenterId presenterId) {
    JavaEnv::getCache().getSurfacePresenterManagerOnDrawableSurfacePresenterUpdatedMethod().call(
        toObject(), static_cast<int32_t>(presenterId));
}

} // namespace ValdiAndroid
