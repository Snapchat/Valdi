//
//  AndroidSurfacePresenterManager.hpp
//  valdi-android
//
//  Created by Simon Corsin on 2/15/22.
//

#pragma once

#include "snap_drawing/cpp/Drawing/Surface/SurfacePresenterManager.hpp"
#include "valdi/runtime/Views/Measure.hpp"
#include "valdi/runtime/Views/View.hpp"
#include "valdi_core/jni/GlobalRefJavaObject.hpp"

#include <unordered_map>

namespace ValdiAndroid {

class ViewManager;

class AndroidSurfacePresenterManager : public snap::drawing::SurfacePresenterManager, public GlobalRefJavaObjectBase {
public:
    AndroidSurfacePresenterManager(JavaEnv env, jobject javaRepr, ViewManager& viewManager);
    ~AndroidSurfacePresenterManager() override;

    snap::drawing::Ref<snap::drawing::DrawableSurface> createPresenterWithDrawableSurface(
        snap::drawing::SurfacePresenterId id, size_t zIndex) override;
    void createPresenterForExternalSurface(snap::drawing::SurfacePresenterId id,
                                           size_t zIndex,
                                           snap::drawing::ExternalSurface& externalSurface) override;
    void setSurfacePresenterZIndex(snap::drawing::SurfacePresenterId id, size_t zIndex) override;
    void setExternalSurfacePresenterState(
        snap::drawing::SurfacePresenterId id,
        const snap::drawing::ExternalSurfacePresenterState& presenterState,
        const snap::drawing::ExternalSurfacePresenterState* previousPresenterState) override;
    void removeSurfacePresenter(snap::drawing::SurfacePresenterId id) override;

    void onDrawableSurfacePresenterUpdated(snap::drawing::SurfacePresenterId presenterId) override;

private:
    ViewManager& _viewManager;
    std::unordered_map<snap::drawing::SurfacePresenterId, float> _lastPointScales;
};

} // namespace ValdiAndroid
