//
//  BridgedView.hpp
//  valdi-android
//
//  Created by Simon Corsin on 2/15/22.
//

#pragma once

#include "snap_drawing/cpp/Drawing/Surface/ExternalSurface.hpp"
#include "valdi/runtime/Views/View.hpp"

#include <mutex>

namespace Valdi {
class IViewManager;
class IViewTransaction;
class MainThreadManager;
class ViewNodeTree;
} // namespace Valdi

namespace snap::drawing {

class BridgedView : public ExternalSurface {
public:
    /// `mainThreadManager` may be null when no view node tree is available yet (placeholders,
    /// preloaded views); a null manager skips the pre-raster transaction fence until
    /// setMainThreadManager provides one, which BridgeLayer does when a node adopts the layer.
    BridgedView(const Valdi::Ref<Valdi::View>& view,
                Valdi::IViewManager& viewManager,
                const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager);
    ~BridgedView() override;

    void setMainThreadManager(const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager);

    const Valdi::Ref<Valdi::View>& getView() const;
    Valdi::IViewManager& getViewManager() const;

    Valdi::Ref<Valdi::IBitmapFactory> getRasterBitmapFactory() const override;

    Valdi::Result<Valdi::Void> rasterInto(const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                          const Rect& frame,
                                          const Matrix& transform,
                                          float rasterScaleX,
                                          float rasterScaleY) override;

    Valdi::IViewTransaction& getViewTransaction(Valdi::ViewNodeTree* viewNodeTree) const;

private:
    Valdi::Ref<Valdi::MainThreadManager> mainThreadManager() const;

    Valdi::Ref<Valdi::View> _view;
    Valdi::IViewManager& _viewManager;
    // Set on the layer's thread when a node adopts the layer, read on the raster thread.
    mutable std::mutex _mainThreadManagerMutex;
    Valdi::Ref<Valdi::MainThreadManager> _mainThreadManager;
};

} // namespace snap::drawing
