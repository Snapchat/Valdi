//
//  FlexboxLayer.hpp
//  snap_drawing-macos
//
//  Created by Simon Corsin on 3/10/23.
//

#pragma once

#include "snap_drawing/cpp/Layers/Layer.hpp"

#include <yoga/Yoga.h>

struct YGNode;

namespace facebook::yoga {
class Style;
}

namespace snap::drawing {

struct FlexValue {
    YGValue value;

    constexpr explicit FlexValue(YGValue value) : value(value) {}

    inline static FlexValue point(Scalar value) {
        return FlexValue(YGValue{.value = value, .unit = YGUnitPoint});
    }

    inline static FlexValue percent(Scalar value) {
        return FlexValue(YGValue{.value = value, .unit = YGUnitPercent});
    }

    inline static FlexValue undefined() {
        return FlexValue(YGValue{.value = YGUndefined, .unit = YGUnitUndefined});
    }
};

class FlexboxAttributes {
public:
    FlexboxAttributes(facebook::yoga::Style* style);

    FlexboxAttributes& setDirection(YGDirection value);

    FlexboxAttributes& setFlexDirection(YGFlexDirection value);

    FlexboxAttributes& setJustifyContent(YGJustify value);
    FlexboxAttributes& setAlignItems(YGAlign value);
    FlexboxAttributes& setAlignContent(YGAlign value);
    FlexboxAttributes& setAlignSelf(YGAlign value);

    FlexboxAttributes& setPadding(YGEdge edge, FlexValue value);
    FlexboxAttributes& setMargin(YGEdge edge, FlexValue value);
    FlexboxAttributes& setBorder(YGEdge edge, FlexValue value);
    FlexboxAttributes& setPosition(YGEdge edge, FlexValue value);

    FlexboxAttributes& setPositionType(YGPositionType value);
    FlexboxAttributes& setFlexWrap(YGWrap value);
    FlexboxAttributes& setOverflow(YGOverflow value);
    FlexboxAttributes& setDisplay(YGDisplay value);
    FlexboxAttributes& setFlex(std::optional<Scalar> value);
    FlexboxAttributes& setFlexGrow(std::optional<Scalar> value);
    FlexboxAttributes& setFlexShrink(std::optional<Scalar> value);

    FlexboxAttributes& setFlexBasis(FlexValue value);

    FlexboxAttributes& setWidth(FlexValue value);
    FlexboxAttributes& setHeight(FlexValue value);
    FlexboxAttributes& setMinWidth(FlexValue value);
    FlexboxAttributes& setMinHeight(FlexValue value);

    FlexboxAttributes& setMaxWidth(FlexValue value);
    FlexboxAttributes& setMaxHeight(FlexValue value);

    FlexboxAttributes& setAspectRatio(std::optional<Scalar> value);

private:
    facebook::yoga::Style* _style;
};

/**
 A Layer subclass that can layout its children with the Flexbox layout
 algorithm implemented by Yoga. The FlexboxLayer will associate Flexbox node
 for each children added to the layer. Each children can be a regular Layer
 or a FlexboxLayer as well.
 */
class FlexboxLayer : public Layer {
public:
    explicit FlexboxLayer(const Ref<Resources>& resources);
    ~FlexboxLayer() override;

    void onInitialize() override;

    Size sizeThatFits(Size maxSize) override;

    /**
     Get the flexbox attributes for the Flexbox node of this layer.
     */
    FlexboxAttributes updateLayoutAttributes();

    /**
     Get the flexbox attributes for the Flexbox node of the given layer.
     The layer needs to be a FlexboxLayer or has a FlexboxLayer as its parent.
     */
    FlexboxAttributes updateLayoutAttributesForLayer(const Ref<Layer>& layer);

    /**
     Get the flexbox attributes for the Flexbox node of the given layer.
     The layer needs to be a FlexboxLayer or has a FlexboxLayer as its parent.
     */
    FlexboxAttributes updateLayoutAttributesForLayer(Layer* layer);

    void requestLayout(ILayer* layer) override;

protected:
    void onBoundsChanged() override;
    void onLayout() override;

    void onChildRemoved(Layer* childLayer) override;
    void onChildInserted(Layer* childLayer, size_t index) override;
};

} // namespace snap::drawing
