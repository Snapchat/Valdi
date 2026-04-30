//
//  ScrollLayerClass.cpp
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/12/22.
//

#include "ScrollLayerClass.hpp"
#include "snap_drawing/cpp/Layers/Scroll/AndroidScroller.hpp"
#include "snap_drawing/cpp/Layers/Scroll/IOSScroller.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"

namespace snap::drawing {

class ValdiScrollPerfLogger : public ScrollPerfLogger {
public:
    ValdiScrollPerfLogger(const Valdi::Ref<Valdi::ValueFunction>& resumeFn,
                          const Valdi::Ref<Valdi::ValueFunction>& pauseFn)
        : _resumeFn(resumeFn), _pauseFn(pauseFn) {}
    ~ValdiScrollPerfLogger() override = default;

    void resume() override {
        (*_resumeFn)();
    }

    void pause(bool cancelLogging) override {
        (*_pauseFn)({Valdi::Value(cancelLogging)});
    }

private:
    Valdi::Ref<Valdi::ValueFunction> _resumeFn;
    Valdi::Ref<Valdi::ValueFunction> _pauseFn;
};

class ValdiScrollLayerListener : public ScrollLayerListener {
    std::optional<Point> onScroll(const ScrollLayer& scrollLayer, const Point& point, const Vector& velocity) override {
        auto viewNode = valdiViewNodeFromLayer(scrollLayer);
        if (viewNode == nullptr) {
            return std::nullopt;
        }

        auto contentOffset = Valdi::Point(point.x, point.y);
        auto pointVelocity = Valdi::Point(velocity.dx, velocity.dy);
        auto result = viewNode->onScroll(contentOffset, contentOffset, pointVelocity);
        if (!result) {
            return std::nullopt;
        }

        return {Point::make(result.value().x, result.value().y)};
    }

    void onScrollEnd(const ScrollLayer& scrollLayer, const Point& point) override {
        auto viewNode = valdiViewNodeFromLayer(scrollLayer);
        if (viewNode == nullptr) {
            return;
        }

        auto contentOffset = Valdi::Point(point.x, point.y);
        viewNode->onScrollEnd(contentOffset, contentOffset);
    }

    void onDragStart(const ScrollLayer& scrollLayer, const Point& point, const Vector& velocity) override {
        auto viewNode = valdiViewNodeFromLayer(scrollLayer);
        if (viewNode == nullptr) {
            return;
        }
        auto contentOffset = Valdi::Point(point.x, point.y);
        auto pointVelocity = Valdi::Point(velocity.dx, velocity.dy);

        viewNode->onDragStart(contentOffset, contentOffset, pointVelocity);
    }

    std::optional<Point> onDragEnding(const ScrollLayer& scrollLayer,
                                      const Point& point,
                                      const Vector& velocity) override {
        auto viewNode = valdiViewNodeFromLayer(scrollLayer);
        if (viewNode == nullptr) {
            return std::nullopt;
        }

        auto contentOffset = Valdi::Point(point.x, point.y);
        auto pointVelocity = Valdi::Point(velocity.dx, velocity.dy);

        auto result = viewNode->onDragEnding(contentOffset, contentOffset, pointVelocity);
        if (!result) {
            return std::nullopt;
        }

        return {Point::make(result.value().x, result.value().y)};
    }
};

ScrollLayerClass::ScrollLayerClass(const Ref<Resources>& resources,
                                   bool useAndroidStyleScroller,
                                   const Ref<LayerClass>& parentClass)
    : ILayerClass(resources, "SCValdiScrollView", "com.snap.valdi.views.ValdiScrollView", parentClass, false),
      _scrollLayerListener(Valdi::makeShared<ValdiScrollLayerListener>()),
      _useAndroidStyleScroller(useAndroidStyleScroller) {}

Valdi::Ref<Layer> ScrollLayerClass::instantiate() {
    auto scrollLayer = snap::drawing::makeLayer<snap::drawing::ScrollLayer>(getResources());
    scrollLayer->setListener(_scrollLayerListener);

    if (_useAndroidStyleScroller) {
        auto scroller = Valdi::makeShared<AndroidScroller>(getResources());
        scrollLayer->setScroller(scroller);
    } else {
        auto scroller = Valdi::makeShared<IOSScroller>();
        scrollLayer->setScroller(scroller);
    }

    return scrollLayer;
}

void ScrollLayerClass::bindAttributes(Valdi::AttributesBindingContext& binder) {
    binder.bindScrollAttributes();

    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, showsVerticalScrollIndicator, false);
    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, showsHorizontalScrollIndicator, false);

    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, bouncesVerticalWithSmallContent, false);
    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, bouncesHorizontalWithSmallContent, false);
    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, bounces, false);

    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, pagingEnabled, false);

    BIND_UNTYPED_ATTRIBUTE(ScrollLayer, scrollPerfLoggerBridge, false);

    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, cancelsTouchesOnScroll, false);
    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, dismissKeyboardOnDrag, false);

    BIND_DOUBLE_ATTRIBUTE(ScrollLayer, fadingEdgeLength, true);
    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, fadingEdgeStart, false);
    BIND_BOOLEAN_ATTRIBUTE(ScrollLayer, fadingEdgeEnd, false);

    REGISTER_PREPROCESSOR(scrollPerfLoggerBridge, false);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> ScrollLayerClass::apply_showsVerticalScrollIndicator(ScrollLayer& /*view*/,
                                                                                bool /*value*/,
                                                                                const AttributeContext& /*context*/) {
    // TODO(simon): Implement scroll bar?
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void ScrollLayerClass::reset_showsVerticalScrollIndicator(ScrollLayer& view, const AttributeContext& /*context*/) {}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> ScrollLayerClass::apply_showsHorizontalScrollIndicator(ScrollLayer& /*view*/,
                                                                                  bool /*value*/,
                                                                                  const AttributeContext& /*context*/) {
    // TODO(simon): Implement scroll bar?
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void ScrollLayerClass::reset_showsHorizontalScrollIndicator(ScrollLayer& view, const AttributeContext& /*context*/) {}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> ScrollLayerClass::apply_bouncesVerticalWithSmallContent(
    ScrollLayer& view, bool value, const AttributeContext& /*context*/) {
    view.setBouncesVerticalWithSmallContent(value);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void ScrollLayerClass::reset_bouncesVerticalWithSmallContent(ScrollLayer& view, const AttributeContext& /*context*/) {
    view.setBouncesVerticalWithSmallContent(false);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> ScrollLayerClass::apply_bouncesHorizontalWithSmallContent(
    ScrollLayer& view, bool value, const AttributeContext& /*context*/) {
    view.setBouncesHorizontalWithSmallContent(value);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void ScrollLayerClass::reset_bouncesHorizontalWithSmallContent(ScrollLayer& view, const AttributeContext& /*context*/) {
    view.setBouncesHorizontalWithSmallContent(false);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> ScrollLayerClass::apply_bounces(ScrollLayer& view,
                                                           bool value,
                                                           const AttributeContext& /*context*/) {
    view.setBounces(value);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void ScrollLayerClass::reset_bounces(ScrollLayer& view, const AttributeContext& /*context*/) {
    view.setBounces(true);
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
Valdi::Result<Valdi::Void> ScrollLayerClass::apply_pagingEnabled(ScrollLayer& view,
                                                                 bool value,
                                                                 const AttributeContext& /*context*/) {
    view.setPagingEnabled(value);
    return Valdi::Void();
}

// NOLINTNEXTLINE(readability-identifier-naming, readability-convert-member-functions-to-static)
void ScrollLayerClass::reset_pagingEnabled(ScrollLayer& view, const AttributeContext& /*context*/) {
    view.setPagingEnabled(false);
}

Valdi::Result<Valdi::Value> ScrollLayerClass::preprocess_scrollPerfLoggerBridge(const Valdi::Value& value) {
    auto resumeFn = value.getMapValue("resume").getFunctionRef();
    auto pauseFn = value.getMapValue("pause").getFunctionRef();

    if (resumeFn == nullptr || pauseFn == nullptr) {
        return Valdi::Error("Invalid scrollPerfLoggerBridge");
    }

    return Valdi::Value(Valdi::makeShared<ValdiScrollPerfLogger>(resumeFn, pauseFn));
}

IMPLEMENT_UNTYPED_ATTRIBUTE(
    ScrollLayer,
    scrollPerfLoggerBridge,
    {
        auto scrollPerfLogger = Valdi::castOrNull<ScrollPerfLogger>(value.getValdiObject());
        view.setScrollPerfLogger(scrollPerfLogger);
        return Valdi::Void();
    },
    { view.setScrollPerfLogger(nullptr); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    ScrollLayer,
    cancelsTouchesOnScroll,
    {
        view.setCancelsOtherGesturesOnScroll(value);
        return Valdi::Void();
    },
    { view.setCancelsOtherGesturesOnScroll(true); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    ScrollLayer,
    dismissKeyboardOnDrag,
    {
        view.setDismissKeyboardOnDrag(value);
        return Valdi::Void();
    },
    { view.setDismissKeyboardOnDrag(false); })

IMPLEMENT_DOUBLE_ATTRIBUTE(
    ScrollLayer,
    fadingEdgeLength,
    {
        view.setFadingEdgeLength(value);
        return Valdi::Void();
    },
    { view.setFadingEdgeLength(0); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    ScrollLayer,
    fadingEdgeStart,
    {
        view.setFadingEdgeStart(value);
        return Valdi::Void();
    },
    { view.setFadingEdgeStart(true); })

IMPLEMENT_BOOLEAN_ATTRIBUTE(
    ScrollLayer,
    fadingEdgeEnd,
    {
        view.setFadingEdgeEnd(value);
        return Valdi::Void();
    },
    { view.setFadingEdgeEnd(true); })

} // namespace snap::drawing
