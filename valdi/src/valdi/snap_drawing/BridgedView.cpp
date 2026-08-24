//
//  BridgedView.cpp
//  valdi-android
//
//  Created by Simon Corsin on 2/15/22.
//

#include "BridgedView.hpp"
#include "snap_drawing/cpp/Drawing/Surface/ExternalSurfacePresenterState.hpp"
#include "snap_drawing/cpp/Utils/Bitmap.hpp"
#include "snap_drawing/cpp/Utils/Image.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Interfaces/IBitmap.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"

#include <chrono>

namespace snap::drawing {

// Generous next to a normal batch (a frame's worth of transactions, single-digit ms), small next
// to a transcode: it bounds the raster stall when a batch owner is wedged, at which point the
// raster proceeds with whatever state has been applied.
static constexpr std::chrono::milliseconds kRasterQuiescenceTimeout{100};

BridgedView::BridgedView(const Valdi::Ref<Valdi::View>& view,
                         Valdi::IViewManager& viewManager,
                         const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager)
    : _view(view), _viewManager(viewManager), _mainThreadManager(mainThreadManager) {}

BridgedView::~BridgedView() = default;

void BridgedView::setMainThreadManager(const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager) {
    std::lock_guard<std::mutex> lockGuard(_mainThreadManagerMutex);
    _mainThreadManager = mainThreadManager;
}

Valdi::Ref<Valdi::MainThreadManager> BridgedView::mainThreadManager() const {
    std::lock_guard<std::mutex> lockGuard(_mainThreadManagerMutex);
    return _mainThreadManager;
}

const Valdi::Ref<Valdi::View>& BridgedView::getView() const {
    return _view;
}

Valdi::IViewManager& BridgedView::getViewManager() const {
    return _viewManager;
}

Valdi::Ref<Valdi::IBitmapFactory> BridgedView::getRasterBitmapFactory() const {
    return _viewManager.getViewRasterBitmapFactory();
}

Valdi::Result<Valdi::Void> BridgedView::rasterInto(const Valdi::Ref<Valdi::IBitmap>& bitmap,
                                                   const Rect& frame,
                                                   const Matrix& transform,
                                                   float rasterScaleX,
                                                   float rasterScaleY) {
    // The platform view is mutated through main-thread transactions that can still be parked in
    // the MainThreadManager queue when this raster runs (an open batch parks them without
    // scheduling a flush, while rasterInto reaches the main thread through a raw dispatch).
    // Snapshotting then captures whatever half-applied state the view happens to hold — e.g. a
    // recycled text view still showing its previous text.
    //
    // Off the main thread, wait for the manager to go quiescent rather than force-flushing: the
    // manager is shared across runtimes, so a mid-batch drain would tear another runtime's frame,
    // not just this snapshot. On timeout just proceed with whatever state has been applied —
    // adding another wait here would hang the raster thread exactly when the main thread stopped
    // draining. On the main thread waiting would block its own endBatch, so pump the queue
    // instead; flushUpToNow caps the pump at tasks dispatched before it, so a task that
    // re-enqueues itself cannot keep the pump alive.
    auto mainThreadManager = this->mainThreadManager();
    if (mainThreadManager != nullptr && !Valdi::MainThreadManager::isPreRasterFenceDisabled()) {
        if (mainThreadManager->currentThreadIsMainThread()) {
            mainThreadManager->flushUpToNow();
        } else {
            mainThreadManager->waitUntilQuiescent(kRasterQuiescenceTimeout);
        }
    }

    if (_view == nullptr) {
        return Valdi::Error("Not view attached");
    }

    auto valdiFrame = Valdi::Frame(frame.x(), frame.y(), frame.width(), frame.height());
    Valdi::Matrix valdiTransform;
    transform.toAffine(&valdiTransform.values[0]);
    return _view->rasterInto(bitmap, valdiFrame, valdiTransform, rasterScaleX, rasterScaleY);
}

Valdi::IViewTransaction& BridgedView::getViewTransaction(Valdi::ViewNodeTree* viewNodeTree) const {
    return viewNodeTree->getCurrentViewTransactionScope().withViewManager(_viewManager).transaction();
}

} // namespace snap::drawing
