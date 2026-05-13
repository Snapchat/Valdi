//
//  SnapDrawingLayerRootHost.cpp
//  valdi-android
//
//  Created by Simon Corsin on 7/10/20.
//

#include "valdi/android/snap_drawing/SnapDrawingLayerRootHost.hpp"
#include "valdi/android/MainThreadDispatcher.hpp"
#include "valdi_core/cpp/Threading/DispatchQueue.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/jni/JavaCache.hpp"

#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi/runtime/Views/Measure.hpp"

#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "valdi/android/AndroidViewHolder.hpp"
#include "valdi/android/ViewManager.hpp"
#include "valdi/android/snap_drawing/AndroidSurfacePresenterManager.hpp"
#include "valdi/snap_drawing/SnapDrawingLayerHolder.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

namespace ValdiAndroid {

class HostLayer : public snap::drawing::Layer {
public:
    HostLayer(const snap::drawing::Ref<snap::drawing::Resources>& resources,
              const Valdi::Ref<Valdi::View>& hostView,
              ViewManager& androidViewManager)
        : snap::drawing::Layer(resources), _hostView(hostView), _androidViewManager(androidViewManager) {}

    ~HostLayer() override = default;

    void requestLayout(snap::drawing::ILayer* layer) override {
        if (_hostView != nullptr) {
            JavaCache::get().getViewRefInvalidateLayoutMethod().call(fromValdiView(_hostView));
        }

        snap::drawing::Layer::requestLayout(layer);
    }

    void requestFocus(snap::drawing::ILayer* layer) override {
        if (_hostView != nullptr) {
            auto javaView = fromValdiView(_hostView);

            JavaCache::get().getViewRefRequestFocusMethod().call(javaView);
        }
    }

    void setAttachedData(const Valdi::Ref<Valdi::RefCountable>& attachedData) override {
        snap::drawing::Layer::setAttachedData(attachedData);

        if (_hostView != nullptr) {
            auto viewNode = valdiViewNodeFromLayer(*this);
            if (viewNode != nullptr) {
                viewNode->getViewNodeTree()
                    ->getCurrentViewTransactionScope()
                    .withViewManager(_androidViewManager)
                    .transaction()
                    .moveViewToTree(_hostView, viewNode->getViewNodeTree(), viewNode.get());
            }
        }
    }

private:
    Valdi::Ref<Valdi::View> _hostView;
    ViewManager& _androidViewManager;
};

SnapDrawingLayerRootHost::SnapDrawingLayerRootHost(
    const Valdi::Ref<snap::drawing::DrawLooper>& drawLooper,
    const Valdi::Ref<snap::drawing::Resources>& resources,
    const Valdi::Ref<snap::drawing::ANativeWindowGraphicsContext>& graphicsContext,
    ViewManager& androidViewManager)
    : _drawLooper(drawLooper),
      _graphicsContext(graphicsContext),
      _androidViewManager(androidViewManager) {
    _layerRoot = snap::drawing::makeLayer<snap::drawing::LayerRoot>(resources);
}

SnapDrawingLayerRootHost::~SnapDrawingLayerRootHost() {
    removeFromDrawLooper();
    _layerRoot->destroy();
    _layerRoot = nullptr;
}

void SnapDrawingLayerRootHost::removeFromDrawLooper() {
    if (_addedToLooper) {
        _addedToLooper = false;
        _drawLooper->removeLayerRoot(_layerRoot);
    }
}

void SnapDrawingLayerRootHost::addToDrawLooper(
    const Valdi::Ref<snap::drawing::SurfacePresenterManager>& surfacePresenterManager) {
    SC_ASSERT(!_addedToLooper);
    _addedToLooper = true;
    _drawLooper->addLayerRoot(_layerRoot, surfacePresenterManager, _disallowSynchronousDraw);
}

Valdi::Ref<Valdi::View> SnapDrawingLayerRootHost::setContentLayerWithHostView(jobject hostViewJava) {
    if (hostViewJava == nullptr) {
        _layerRoot->setContentLayer(nullptr, snap::drawing::ContentLayerSizingModeMatchSize);
        return nullptr;
    } else {
        auto hostView = ValdiAndroid::toValdiView(ValdiAndroid::ViewType(JavaEnv(), hostViewJava), _androidViewManager);

        auto hostLayer = snap::drawing::makeLayer<HostLayer>(_layerRoot->getResources(), hostView, _androidViewManager);
        _layerRoot->setContentLayer(hostLayer, snap::drawing::ContentLayerSizingModeMatchSize);
        return snap::drawing::layerToValdiView(hostLayer, true);
    }
}

const snap::drawing::Ref<snap::drawing::LayerRoot>& SnapDrawingLayerRootHost::getLayerRoot() const {
    return _layerRoot;
}

void SnapDrawingLayerRootHost::setSurfacePresenterManager(jobject surfacePresenterManagerJava) {
    removeFromDrawLooper();
    if (surfacePresenterManagerJava != nullptr) {
        addToDrawLooper(Valdi::makeShared<AndroidSurfacePresenterManager>(
            JavaEnv(), surfacePresenterManagerJava, _androidViewManager));
    }
}

void SnapDrawingLayerRootHost::setSize(int width, int height) {
    _layerRoot->setSize(snap::drawing::Size::make(convertUnit(width), convertUnit(height)),
                        _androidViewManager.getPointScale());
}

void SnapDrawingLayerRootHost::drawInBitmap(snap::drawing::SurfacePresenterId surfacePresenterId,
                                            const Valdi::Ref<Valdi::IBitmap>& bitmap) {
    snap::drawing::BitmapGraphicsContext context;

    auto surface = context.createBitmapSurface(bitmap);

    auto canvas = surface->prepareCanvas();

    if (canvas) {
        _drawLooper->drawPlaneOfLayerRootForPresenterId(*_layerRoot, surfacePresenterId, canvas.value());
    }
}

void SnapDrawingLayerRootHost::setSurface(snap::drawing::SurfacePresenterId surfacePresenterId,
                                          ANativeWindow* surface) {
    snap::drawing::Ref<snap::drawing::DrawableSurface> drawableSurface;
    if (surface != nullptr) {
        drawableSurface = _graphicsContext->createSurfaceForNativeWindow(surface);
    }

    _drawLooper->setDrawableSurfaceOfLayerRootForPresenterId(*_layerRoot, surfacePresenterId, drawableSurface);
}

void SnapDrawingLayerRootHost::setSurfaceNeedsRedraw(snap::drawing::SurfacePresenterId surfacePresenterId) {
    _drawLooper->setPresenterOfLayerRootNeedsRedraw(*_layerRoot, surfacePresenterId);
}

void SnapDrawingLayerRootHost::onSurfaceSizeChanged(snap::drawing::SurfacePresenterId surfacePresenterId) {}

bool SnapDrawingLayerRootHost::dispatchTouchEvent(snap::drawing::TouchEventType type,
                                                  int64_t timeNanos,
                                                  int x,
                                                  int y,
                                                  int dx,
                                                  int dy,
                                                  int pointerCount,
                                                  int actionIndex,
                                                  jintArray pointerLocations,
                                                  const Valdi::Ref<Valdi::RefCountable>& source) {
    auto resolvedX = convertUnit(x);
    auto resolvedY = convertUnit(y);
    auto resolvedDirectionX = convertUnit(dx);
    auto resolvedDirectionY = convertUnit(dy);
    auto time = snap::drawing::TimePoint::fromNanoSeconds(timeNanos);
    auto frameTime = _layerRoot->getFrameTimeForAbsoluteFrameTime(time);

    // we use SmallVector<2> to capture most touches that should have 1 or 2 pointers, and accept the heap allocation
    // costs on complex touch gestures
    snap::drawing::TouchEvent::PointerLocations resolvedPointerLocations;

    JavaEnv::accessEnv([&](JNIEnv& jni) {
        auto length = jni.GetArrayLength(pointerLocations);
        auto* elements = jni.GetIntArrayElements(pointerLocations, NULL);

        SC_ASSERT(pointerCount >= 0 && length == (pointerCount * 2));

        int i = 0;
        while (i < pointerCount * 2) {
            // we save points as a flat array of [x1, y1, x2, y2, ...] where each points x/y coords correspond to index
            // * 2 and index * 2 + 1, respectively
            auto pointX = convertUnit(elements[i++]);
            auto pointY = convertUnit(elements[i++]);
            resolvedPointerLocations.emplace_back(snap::drawing::Point::make(pointX, pointY));
        }

        jni.ReleaseIntArrayElements(pointerLocations, elements, JNI_ABORT);
    });

    return _layerRoot->dispatchTouchEvent(
        snap::drawing::TouchEvent(type,
                                  snap::drawing::Point::make(resolvedX, resolvedY),
                                  snap::drawing::Point::make(resolvedX, resolvedY),
                                  snap::drawing::Vector::make(resolvedDirectionX, resolvedDirectionY),
                                  pointerCount,
                                  actionIndex,
                                  std::move(resolvedPointerLocations),
                                  frameTime,
                                  snap::drawing::Duration(),
                                  source));
}

snap::drawing::Scalar SnapDrawingLayerRootHost::convertUnit(int i) const {
    return Valdi::pixelsToPoints(i, _androidViewManager.getPointScale());
}

Valdi::Result<Valdi::Void> SnapDrawingLayerRootHost::drawLayerInBitmap(const Valdi::Ref<Valdi::View>& layer,
                                                                       const Valdi::Ref<Valdi::IBitmap>& bitmap) {
    auto holder = Valdi::castOrNull<snap::drawing::SnapDrawingLayerHolder>(layer);

    snap::drawing::BitmapGraphicsContext context;

    auto surface = context.createBitmapSurface(bitmap);

    auto canvas = surface->prepareCanvas();
    if (!canvas) {
        return canvas.moveError();
    }

    drawLayerInCanvas(holder->get(), canvas.value());
    surface->flush();
    return Valdi::Void();
}

void SnapDrawingLayerRootHost::setDisallowSynchronousDraw(bool disallowSynchronousDraw) {
    _disallowSynchronousDraw = disallowSynchronousDraw;
}

} // namespace ValdiAndroid
