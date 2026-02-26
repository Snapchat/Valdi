//
//  StandaloneViewTransaction.cpp
//  valdi
//
//  Created by Simon Corsin on 7/26/24.
//

#include "valdi/snap_drawing/SnapDrawingViewTransaction.hpp"
#include "valdi/snap_drawing/SnapDrawingLayerHolder.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"

#include <fmt/format.h>

#include "snap_drawing/cpp/Utils/Image.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"

#include "valdi/snap_drawing/Layers/BridgeLayer.hpp"

#include "snap_drawing/cpp/Animations/Animation.hpp"
#include "snap_drawing/cpp/Animations/SpringAnimation.hpp"
#include "snap_drawing/cpp/Animations/ValueInterpolators.hpp"
#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "valdi/snap_drawing/Animations/ValdiAnimator.hpp"

#include "snap_drawing/cpp/Layers/Interfaces/ILoadedAssetLayer.hpp"
#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "snap_drawing/cpp/Layers/ScrollLayer.hpp"
#include "valdi/runtime/Views/Measure.hpp"

using namespace Valdi;

namespace snap::drawing {

/// Returns the Layer for a View that is a SnapDrawingLayerHolder. Returns null if the view is
/// not a holder (e.g. a bridged NSView on macOS), so callers must null-check to avoid crashes.
static Valdi::Ref<Layer> toLayer(const Ref<View>& view) {
    return valdiViewToLayer(view);
}

SnapDrawingViewTransaction::SnapDrawingViewTransaction() = default;
SnapDrawingViewTransaction::~SnapDrawingViewTransaction() = default;

void SnapDrawingViewTransaction::flush(bool sync) {}

void SnapDrawingViewTransaction::willUpdateRootView(const Ref<View>& view) {}

void SnapDrawingViewTransaction::didUpdateRootView(const Ref<View>& view, bool layoutDidBecomeDirty) {}

void SnapDrawingViewTransaction::moveViewToTree(const Ref<View>& view, ViewNodeTree* viewNodeTree, ViewNode* viewNode) {
    auto layer = toLayer(view);
    if (layer == nullptr) {
        return;
    }
    setValdiViewNodeToLayer(*layer, viewNode);
}

void SnapDrawingViewTransaction::insertChildView(const Ref<View>& view,
                                                 const Ref<View>& childView,
                                                 int index,
                                                 const Ref<Animator>& animator) {
    auto parent = toLayer(view);
    auto child = toLayer(childView);
    if (parent == nullptr || child == nullptr) {
        return;
    }

    auto scrollView = Valdi::castOrNull<ScrollLayer>(parent);

    if (scrollView != nullptr) {
        scrollView->getContentLayer()->insertChild(child, static_cast<size_t>(index));
        auto viewNode = snap::drawing::valdiViewNodeFromLayer(*scrollView);
        if (viewNode != nullptr) {
            scrollView->setHorizontal(viewNode->isHorizontal());
        }
    } else {
        parent->insertChild(child, static_cast<size_t>(index));
    }
}

void SnapDrawingViewTransaction::removeViewFromParent(const Ref<View>& view,
                                                      const Ref<Animator>& animator,
                                                      bool shouldClearViewNode) {
    auto layer = toLayer(view);
    if (layer != nullptr) {
        layer->removeFromParent();
    }
}

void SnapDrawingViewTransaction::invalidateViewLayout(const Ref<View>& view) {
    auto layer = toLayer(view);
    if (layer != nullptr) {
        layer->setNeedsLayout();
    }
}

static Rect makeViewFrame(const Valdi::Frame& frame, Scalar displayScale) {
    auto x = Valdi::roundToPixelGrid(frame.x, displayScale);
    auto y = Valdi::roundToPixelGrid(frame.y, displayScale);
    auto width = Valdi::roundToPixelGrid(frame.width, displayScale);
    auto height = Valdi::roundToPixelGrid(frame.height, displayScale);

    return Rect::makeXYWH(x, y, width, height);
}

void SnapDrawingViewTransaction::setViewFrame(const Ref<View>& view,
                                              const Frame& newFrame,
                                              bool isRightToLeft,
                                              const Ref<Animator>& animator) {
    auto layer = toLayer(view);
    if (layer == nullptr) {
        return;
    }
    auto frame = makeViewFrame(newFrame, layer->getResources()->getDisplayScale());

    auto typedAnimator =
        Valdi::castOrNull<ValdiAnimator>(animator != nullptr ? animator->getNativeAnimator() : nullptr);
    if (typedAnimator != nullptr) {
        auto fromFrame = layer->getFrame();

        Ref<IAnimation> animation;
        if (typedAnimator->isSpringAnimation()) {
            animation = Valdi::makeShared<SpringAnimation>(typedAnimator->getStiffness(),
                                                           typedAnimator->getDamping(),
                                                           MIN_VISIBLE_CHANGE_PIXEL,
                                                           [frame, fromFrame](Layer& view, double ratio) {
                                                               view.setFrame(interpolateValue(fromFrame, frame, ratio));
                                                           });
        } else {
            animation = Valdi::makeShared<Animation>(typedAnimator->getDuration(),
                                                     typedAnimator->getInterpolationFunction(),
                                                     [frame, fromFrame](Layer& view, double ratio) {
                                                         view.setFrame(interpolateValue(fromFrame, frame, ratio));
                                                     });
        }

        typedAnimator->appendAnimation(layer, animation, STRING_LITERAL("frame"));
    } else {
        layer->setFrame(frame);
    }

    layer->setRightToLeft(isRightToLeft);
}

void SnapDrawingViewTransaction::setViewScrollSpecs(const Ref<View>& view,
                                                    const Valdi::Point& directionDependentContentOffset,
                                                    const Valdi::Size& contentSize,
                                                    bool animated) {
    auto layer = toLayer(view);
    if (layer == nullptr) {
        return;
    }
    auto scrollView = Valdi::castOrNull<ScrollLayer>(layer);

    if (scrollView != nullptr) {
        scrollView->setContentSize(Size::make(contentSize.width, contentSize.height));
        scrollView->setContentOffset(
            Point::make(directionDependentContentOffset.x, directionDependentContentOffset.y), Vector(0, 0), animated);
    }
}

void SnapDrawingViewTransaction::setViewLoadedAsset(const Ref<View>& view,
                                                    const Ref<LoadedAsset>& loadedAsset,
                                                    bool shouldDrawFlipped) {
    auto layer = toLayer(view);
    if (layer == nullptr) {
        return;
    }
    auto loadedAssetLayer = dynamic_cast<ILoadedAssetLayer*>(layer.get());
    if (loadedAssetLayer != nullptr) {
        loadedAssetLayer->onLoadedAssetChanged(loadedAsset, shouldDrawFlipped);
    }
}

void SnapDrawingViewTransaction::layoutView(const Ref<View>& view) {}

void SnapDrawingViewTransaction::cancelAllViewAnimations(const Ref<View>& view) {}

void SnapDrawingViewTransaction::willEnqueueViewToPool(const Ref<View>& view, Valdi::Function<void(View&)> onEnqueue) {
    auto snapDrawingView = toLayer(view);
    if (snapDrawingView == nullptr) {
        return;
    }
    snapDrawingView->setAttachedData(nullptr);

    snapDrawingView->prepareForReuse();
    snapDrawingView->removeAllAnimations();

    onEnqueue(*view);
}

void SnapDrawingViewTransaction::snapshotView(const Ref<View>& view,
                                              Valdi::Function<void(Valdi::Result<Valdi::BytesView>)> cb) {
    auto layer = toLayer(view);
    if (layer == nullptr) {
        cb(Valdi::Error(STRING_LITERAL("View is not a SnapDrawing layer")));
        return;
    }

    auto bridgeLayer = Valdi::castOrNull<BridgeLayer>(layer);
    if (bridgeLayer != nullptr) {
        auto viewNode = snap::drawing::valdiViewNodeFromLayer(*bridgeLayer);
        auto innerView = bridgeLayer->getBridgedView();
        if (viewNode != nullptr && innerView != nullptr) {
            innerView->getViewTransaction(viewNode->getViewNodeTree())
                .snapshotView(innerView->getView(), std::move(cb));
        } else {
            cb(Valdi::Error(STRING_LITERAL("BridgeLayer does not have associated View")));
        }

        return;
    }

    auto root = dynamic_cast<LayerRoot*>(layer->getRoot());
    auto scale = (root != nullptr) ? root->getScale() : 1.0f;

    auto viewWidth = layer->getFrame().width() * scale;
    auto viewHeight = layer->getFrame().height() * scale;

    if (viewWidth < 1 || viewHeight < 1) {
        cb(Valdi::Error(STRING_FORMAT("View is too small: {}x{}", viewWidth, viewHeight)));
        return;
    }

    auto iViewWidth = static_cast<uint32_t>(viewWidth);
    auto iViewHeight = static_cast<uint32_t>(viewHeight);

    BitmapGraphicsContext graphicsContext;
    auto surface = graphicsContext.createBitmapSurface(
        Valdi::BitmapInfo(iViewWidth, iViewHeight, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypePremul, 0));

    auto canvasResult = surface->prepareCanvas();

    if (!canvasResult) {
        cb(canvasResult.error());
        return;
    }

    auto canvas = canvasResult.moveValue();

    drawLayerInCanvas(layer, canvas);

    auto snapshot = canvas.snapshot();
    if (!snapshot) {
        cb(snapshot.moveError());
        return;
    }

    cb(snapshot.value()->toPNG());
}

void SnapDrawingViewTransaction::flushAnimator(const Ref<Animator>& animator, const Valdi::Value& completionCallback) {
    auto typedAnimator = Valdi::castOrNull<ValdiAnimator>(animator->getNativeAnimator());
    typedAnimator->flushAnimations(completionCallback);
}

void SnapDrawingViewTransaction::cancelAnimator(const Ref<Animator>& animator) {
    auto typedAnimator = Valdi::castOrNull<ValdiAnimator>(animator->getNativeAnimator());
    typedAnimator->cancel();
}

void SnapDrawingViewTransaction::executeInTransactionThread(DispatchFunction executeFn) {
    executeFn();
}

} // namespace snap::drawing
