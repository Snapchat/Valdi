//
//  ValdiUtils.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 6/28/20.
//

#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Utils/GeometricPath.hpp"

#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/snap_drawing/Layers/BridgeLayer.hpp"
#include "valdi/snap_drawing/SnapDrawingLayerHolder.hpp"

#include "snap_drawing/cpp/Drawing/DisplayList/DisplayList.hpp"

namespace snap::drawing {

Valdi::Ref<Valdi::View> layerToValdiView(const Valdi::Ref<Layer>& layer, bool makeStrongRef) {
    auto holder = Valdi::makeShared<SnapDrawingLayerHolder>();
    if (makeStrongRef) {
        holder->setStrong(layer);
    } else {
        holder->setWeak(layer);
    }

    return holder;
}

Valdi::Ref<Layer> valdiViewToLayer(const Valdi::Ref<Valdi::View>& view) {
    auto holder = Valdi::castOrNull<SnapDrawingLayerHolder>(view);
    if (holder == nullptr) {
        return nullptr;
    }
    return holder->get();
}

Color snapDrawingColorFromValdiColor(int64_t color) {
    auto valdiColor = Valdi::Color(color);
    return Color::makeARGB(valdiColor.alpha(), valdiColor.red(), valdiColor.green(), valdiColor.blue());
}

Scalar resolveDeltaX(const Layer& layer, Scalar deltaX) {
    if (layer.isRightToLeft()) {
        return deltaX * -1;
    } else {
        return deltaX;
    }
}

Valdi::Ref<ValdiLayerData> getValdiLayerData(const Layer& layer) {
    return Valdi::castOrNull<ValdiLayerData>(layer.getAttachedData());
}

Valdi::Ref<Valdi::ViewNode> valdiViewNodeFromLayer(const Layer& layer) {
    auto* data = dynamic_cast<ValdiLayerData*>(layer.getAttachedData().get());
    if (data == nullptr) {
        return nullptr;
    }

    return Valdi::Ref<Valdi::ViewNode>(data->viewNode.lock());
}

void setValdiViewNodeToLayer(Layer& layer, Valdi::ViewNode* viewNode) {
    auto data = getValdiLayerData(layer);
    if (data == nullptr) {
        data = Valdi::makeShared<ValdiLayerData>();
    }
    if (viewNode == nullptr) {
        data->viewNode.reset();
    } else {
        data->viewNode = Valdi::weakRef(viewNode);
    }
    layer.setAttachedData(data);
}

Valdi::Ref<Valdi::View> bridgeViewFromView(const Valdi::Ref<Valdi::View>& view) {
    auto layerHolder = Valdi::castOrNull<SnapDrawingLayerHolder>(view);
    if (layerHolder == nullptr) {
        return nullptr;
    }
    auto bridgeView = Valdi::castOrNull<BridgeLayer>(layerHolder->get());
    if (bridgeView != nullptr) {
        return bridgeView->getView();
    }
    return nullptr;
}

struct GeometricPathToPathVisitor : public Valdi::GeometricPathVisitor {
    Path& path;
    explicit GeometricPathToPathVisitor(Path& path) : path(path) {}

    void moveTo(double x, double y) override {
        path.moveTo(static_cast<Scalar>(x), static_cast<Scalar>(y));
    }

    void lineTo(double x, double y) override {
        path.lineTo(static_cast<Scalar>(x), static_cast<Scalar>(y));
    }

    void quadTo(double x, double y, double controlX, double controlY) override {
        path.quadTo(static_cast<Scalar>(x),
                    static_cast<Scalar>(y),
                    static_cast<Scalar>(controlX),
                    static_cast<Scalar>(controlY));
    }

    void cubicTo(double x, double y, double controlX1, double controlY1, double controlX2, double controlY2) override {
        path.cubicTo(static_cast<Scalar>(controlX1),
                     static_cast<Scalar>(controlY1),
                     static_cast<Scalar>(controlX2),
                     static_cast<Scalar>(controlY2),
                     static_cast<Scalar>(x),
                     static_cast<Scalar>(y));
    }

    void roundRectTo(double x, double y, double width, double height, double radiusX, double radiusY) override {
        path.addRoundRect(
            Rect::makeXYWH(x, y, width, height), static_cast<Scalar>(radiusX), static_cast<Scalar>(radiusY), true);
    }

    void arcTo(
        double centerX, double centerY, double radiusX, double radiusY, double startAngle, double sweepAngle) override {
        auto oval = Rect::makeLTRB(static_cast<Scalar>(centerX - radiusX),
                                   static_cast<Scalar>(centerY - radiusY),
                                   static_cast<Scalar>(centerX + radiusX),
                                   static_cast<Scalar>(centerY + radiusY));
        path.arcTo(oval, SkRadiansToDegrees(startAngle), SkRadiansToDegrees(sweepAngle));
    }

    void close() override {
        path.close();
    }
};

Path pathFromValdiGeometricPath(const Valdi::BytesView& pathData, double width, double height) {
    Path path;
    GeometricPathToPathVisitor visitor(path);
    Valdi::GeometricPath::visit(pathData.data(), pathData.size(), width, height, visitor);

    return path;
}

void drawLayerInCanvas(const Ref<Layer>& layer, DrawableSurfaceCanvas& canvas) {
    DrawMetrics metrics;
    auto displayList = Valdi::makeShared<DisplayList>(layer->getFrame().size(), TimePoint(0.0));

    // Compensate for the x/y of the view
    // TODO(simon): Fix this
    Matrix matrix;
    matrix.setTranslateX(-layer->getFrame().x());
    matrix.setTranslateY(-layer->getFrame().y());
    displayList->pushContext(matrix, 1.0f, 1, true);

    layer->draw(*displayList, metrics);

    displayList->popContext();

    displayList->draw(canvas, kDisplayListAllPlaneIndexes, /* shouldClearCanvas */ true);
}

} // namespace snap::drawing
