//
//  SnapDrawingLayerRootHost.hpp
//  valdi-android
//
//  Created by Simon Corsin on 7/10/20.
//

#pragma once

#include "snap_drawing/cpp/Drawing/DrawLooper.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/ANativeWindowGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/Surface/SurfacePresenterManager.hpp"
#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Views/View.hpp"

namespace ValdiAndroid {

class ViewManager;

class SnapDrawingLayerRootHost : public Valdi::SimpleRefCountable {
public:
    SnapDrawingLayerRootHost(const Valdi::Ref<snap::drawing::DrawLooper>& drawLooper,
                             const Valdi::Ref<snap::drawing::Resources>& resources,
                             const Valdi::Ref<snap::drawing::ANativeWindowGraphicsContext>& graphicsContext,
                             ViewManager& androidViewManager);

    ~SnapDrawingLayerRootHost() override;

    snap::drawing::Scalar convertUnit(int i) const;

    void setSize(int width, int height);

    /**
     * Sets the contentLayer in the LayerRoot with the given hostView.
     * Returns the SnapDrawing Layer created as the contentLayer.
     */
    Valdi::Ref<Valdi::View> setContentLayerWithHostView(jobject hostViewJava);

    const snap::drawing::Ref<snap::drawing::LayerRoot>& getLayerRoot() const;

    void setSurfacePresenterManager(jobject surfacePresenterManagerJava);

    bool dispatchTouchEvent(snap::drawing::TouchEventType type,
                            int64_t timeNanos,
                            int x,
                            int y,
                            int dx,
                            int dy,
                            int pointerCount,
                            int actionIndex,
                            jintArray pointerLocations,
                            const Valdi::Ref<Valdi::RefCountable>& source);

    void drawInBitmap(snap::drawing::SurfacePresenterId surfacePresenterId, const Valdi::Ref<Valdi::IBitmap>& bitmap);

    void setSurface(snap::drawing::SurfacePresenterId surfacePresenterId, ANativeWindow* surface);
    void setSurfaceNeedsRedraw(snap::drawing::SurfacePresenterId surfacePresenterId);
    void onSurfaceSizeChanged(snap::drawing::SurfacePresenterId surfacePresenterId);

    static Valdi::Result<Valdi::Void> drawLayerInBitmap(const Valdi::Ref<Valdi::View>& layer,
                                                        const Valdi::Ref<Valdi::IBitmap>& bitmap);

    void setDisallowSynchronousDraw(bool disallowSynchronousDraw);

private:
    Valdi::Ref<snap::drawing::DrawLooper> _drawLooper;
    Valdi::Ref<snap::drawing::ANativeWindowGraphicsContext> _graphicsContext;
    ViewManager& _androidViewManager;
    bool _disallowSynchronousDraw = false;

    snap::drawing::Ref<snap::drawing::LayerRoot> _layerRoot;
    bool _addedToLooper = false;

    void removeFromDrawLooper();
    void addToDrawLooper(const Valdi::Ref<snap::drawing::SurfacePresenterManager>& surfacePresenterManager);
};

} // namespace ValdiAndroid
