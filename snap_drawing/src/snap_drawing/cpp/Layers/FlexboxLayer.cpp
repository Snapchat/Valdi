//
//  FlexboxLayer.cpp
//  snap_drawing-macos
//
//  Created by Simon Corsin on 3/10/23.
//

#include "snap_drawing/cpp/Layers/FlexboxLayer.hpp"
#include <yoga/node/Node.h>
#include <yoga/style/StyleLength.h>
#include <yoga/style/StyleSizeLength.h>

namespace snap::drawing {

static YGConfigRef makeYogaConfig() {
    auto* config = YGConfigNew();
    YGConfigSetExperimentalFeatureEnabled(config, YGExperimentalFeatureWebFlexBasis, true);
    return config;
}

static YGConfigRef getYogaConfig() {
    static auto* kYogaConfig = makeYogaConfig();

    return kYogaConfig;
}

struct FlexboxNode : public Valdi::SimpleRefCountable {
    YGNodeRef yogaNode;

    FlexboxNode() : yogaNode(YGNodeNewWithConfig(getYogaConfig())) {}

    ~FlexboxNode() override {
        YGNodeFree(yogaNode);
    }

    void calculateLayout(Size size, bool isRightToLeft) const {
        auto ownerWidth =
            (size.width == std::numeric_limits<Scalar>::max() || std::isnan(size.width)) ? YGUndefined : size.width;
        auto ownerHeight =
            (size.height == std::numeric_limits<Scalar>::max() || std::isnan(size.height)) ? YGUndefined : size.height;

        YGNodeCalculateLayout(yogaNode, ownerWidth, ownerHeight, isRightToLeft ? YGDirectionRTL : YGDirectionLTR);
    }

    void setLayoutDirty() const {
        if (facebook::yoga::resolveRef(yogaNode)->hasMeasureFunc()) {
            YGNodeMarkDirty(yogaNode);
        }
    }

    Rect getFrame() const {
        return Rect::makeXYWH(sanitizeYogaValue(YGNodeLayoutGetLeft(yogaNode)),
                              sanitizeYogaValue(YGNodeLayoutGetTop(yogaNode)),
                              sanitizeYogaValue(YGNodeLayoutGetWidth(yogaNode)),
                              sanitizeYogaValue(YGNodeLayoutGetHeight(yogaNode)));
    }

private:
    static inline float sanitizeYogaValue(float yogaValue) {
        if (std::isnan(yogaValue)) {
            return 0.0;
        }

        return yogaValue;
    }
};

static facebook::yoga::FloatOptional toFloatOptional(const std::optional<Scalar>& value) {
    return value ? facebook::yoga::FloatOptional(value.value()) : facebook::yoga::FloatOptional();
}

static facebook::yoga::StyleLength toStyleLength(FlexValue value) {
    switch (value.value.unit) {
        case YGUnitAuto:
            return facebook::yoga::StyleLength::ofAuto();
        case YGUnitPercent:
            return facebook::yoga::StyleLength::percent(value.value.value);
        case YGUnitPoint:
            return facebook::yoga::StyleLength::points(value.value.value);
        case YGUnitUndefined:
        case YGUnitMaxContent:
        case YGUnitFitContent:
        case YGUnitStretch:
            return facebook::yoga::StyleLength::undefined();
    }
}

static facebook::yoga::StyleSizeLength toStyleSizeLength(FlexValue value) {
    switch (value.value.unit) {
        case YGUnitAuto:
            return facebook::yoga::StyleSizeLength::ofAuto();
        case YGUnitPercent:
            return facebook::yoga::StyleSizeLength::percent(value.value.value);
        case YGUnitPoint:
            return facebook::yoga::StyleSizeLength::points(value.value.value);
        case YGUnitMaxContent:
            return facebook::yoga::StyleSizeLength::ofMaxContent();
        case YGUnitFitContent:
            return facebook::yoga::StyleSizeLength::ofFitContent();
        case YGUnitStretch:
            return facebook::yoga::StyleSizeLength::ofStretch();
        case YGUnitUndefined:
            return facebook::yoga::StyleSizeLength::undefined();
    }
}

FlexboxAttributes::FlexboxAttributes(facebook::yoga::Style* style) : _style(style) {}

FlexboxAttributes& FlexboxAttributes::setDirection(YGDirection value) {
    _style->setDirection(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setFlexDirection(YGFlexDirection value) {
    _style->setFlexDirection(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setJustifyContent(YGJustify value) {
    _style->setJustifyContent(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setAlignItems(YGAlign value) {
    _style->setAlignItems(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setAlignContent(YGAlign value) {
    _style->setAlignContent(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setAlignSelf(YGAlign value) {
    _style->setAlignSelf(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setPadding(YGEdge edge, FlexValue value) {
    _style->setPadding(facebook::yoga::scopedEnum(edge), toStyleLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setMargin(YGEdge edge, FlexValue value) {
    _style->setMargin(facebook::yoga::scopedEnum(edge), toStyleLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setBorder(YGEdge edge, FlexValue value) {
    _style->setBorder(facebook::yoga::scopedEnum(edge), toStyleLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setPosition(YGEdge edge, FlexValue value) {
    _style->setPosition(facebook::yoga::scopedEnum(edge), toStyleLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setPositionType(YGPositionType value) {
    _style->setPositionType(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setFlexWrap(YGWrap value) {
    _style->setFlexWrap(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setOverflow(YGOverflow value) {
    _style->setOverflow(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setDisplay(YGDisplay value) {
    _style->setDisplay(facebook::yoga::scopedEnum(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setFlex(std::optional<Scalar> value) {
    _style->setFlex(toFloatOptional(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setFlexGrow(std::optional<Scalar> value) {
    _style->setFlexGrow(toFloatOptional(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setFlexShrink(std::optional<Scalar> value) {
    _style->setFlexShrink(toFloatOptional(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setFlexBasis(FlexValue value) {
    _style->setFlexBasis(toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setWidth(FlexValue value) {
    _style->setDimension(facebook::yoga::Dimension::Width, toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setHeight(FlexValue value) {
    _style->setDimension(facebook::yoga::Dimension::Height, toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setMinWidth(FlexValue value) {
    _style->setMinDimension(facebook::yoga::Dimension::Width, toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setMinHeight(FlexValue value) {
    _style->setMinDimension(facebook::yoga::Dimension::Height, toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setMaxWidth(FlexValue value) {
    _style->setMaxDimension(facebook::yoga::Dimension::Width, toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setMaxHeight(FlexValue value) {
    _style->setMaxDimension(facebook::yoga::Dimension::Height, toStyleSizeLength(value));
    return *this;
}

FlexboxAttributes& FlexboxAttributes::setAspectRatio(std::optional<Scalar> value) {
    _style->setAspectRatio(toFloatOptional(value));
    return *this;
}

void onYogaNodeDirty(YGNodeConstRef node) {
    auto* layer = reinterpret_cast<snap::drawing::Layer*>(facebook::yoga::resolveRef(node)->getContext());
    if (layer == nullptr) {
        return;
    }

    layer->setNeedsLayout();
}

YGSize onYogaMeasure(
    YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
    auto* layer = reinterpret_cast<snap::drawing::Layer*>(facebook::yoga::resolveRef(node)->getContext());
    if (layer == nullptr) {
        return {.width = 0, .height = 0};
    }

    auto outputSize = layer->sizeThatFits(
        Size::make(widthMode == YGMeasureModeUndefined ? std::numeric_limits<Scalar>::max() : width,
                   heightMode == YGMeasureModeUndefined ? std::numeric_limits<Scalar>::max() : height));

    return {.width = outputSize.width, .height = outputSize.height};
}

static Ref<FlexboxNode> createAndAssociateFlexboxNode(Layer* layer, bool isOwner) {
    auto flexboxNode = Valdi::makeShared<FlexboxNode>();
    layer->setAttachedData(flexboxNode);

    auto* yogaNode = facebook::yoga::resolveRef(flexboxNode->yogaNode);
    yogaNode->setContext(static_cast<snap::drawing::Layer*>(layer));
    yogaNode->setDirtiedFunc(&onYogaNodeDirty);

    if (!isOwner) {
        yogaNode->setMeasureFunc(&onYogaMeasure);
    }

    return flexboxNode;
}

static Ref<FlexboxNode> getFlexboxNode(const Layer* layer) {
    return Valdi::castOrNull<FlexboxNode>(layer->getAttachedData());
}

static Ref<FlexboxNode> mustGetFlexboxNode(const Layer* layer) {
    auto node = getFlexboxNode(layer);
    SC_ASSERT_NOTNULL(node);
    return node;
}

static Ref<FlexboxNode> getOrCreateFlexboxNode(Layer* layer) {
    auto flexboxNode = getFlexboxNode(layer);
    if (flexboxNode == nullptr) {
        flexboxNode = createAndAssociateFlexboxNode(layer, false);
    }

    return flexboxNode;
}

FlexboxLayer::FlexboxLayer(const Ref<Resources>& resources) : Layer(resources) {}
FlexboxLayer::~FlexboxLayer() = default;

void FlexboxLayer::onInitialize() {
    createAndAssociateFlexboxNode(this, true);
}

Size FlexboxLayer::sizeThatFits(Size maxSize) {
    auto node = getOrCreateFlexboxNode(this);

    node->calculateLayout(maxSize, isRightToLeft());
    return node->getFrame().size();
}

void FlexboxLayer::onBoundsChanged() {
    Layer::onBoundsChanged();

    setNeedsLayout();
}

void FlexboxLayer::onLayout() {
    Layer::onLayout();

    auto node = mustGetFlexboxNode(this);
    if (YGNodeGetOwner(node->yogaNode) == nullptr) {
        // we are the root node, calculate the layout starting from us
        node->calculateLayout(getFrame().size(), isRightToLeft());
    }

    auto childrenSize = getChildrenSize();
    for (size_t i = 0; i < childrenSize; i++) {
        auto child = getChild(i);
        auto childNode = mustGetFlexboxNode(child.get());

        child->setFrame(childNode->getFrame());
    }
}

FlexboxAttributes FlexboxLayer::updateLayoutAttributes() {
    return updateLayoutAttributesForLayer(this);
}

FlexboxAttributes FlexboxLayer::updateLayoutAttributesForLayer(const Ref<Layer>& layer) {
    return updateLayoutAttributesForLayer(layer.get());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
FlexboxAttributes FlexboxLayer::updateLayoutAttributesForLayer(Layer* layer) {
    auto flexboxNode = mustGetFlexboxNode(layer);

    flexboxNode->setLayoutDirty();

    return FlexboxAttributes(&facebook::yoga::resolveRef(flexboxNode->yogaNode)->style());
}

void FlexboxLayer::requestLayout(ILayer* layer) {
    Layer::requestLayout(layer);

    auto* childLayer = dynamic_cast<Layer*>(layer);
    if (childLayer != nullptr && childLayer != this && childLayer->getParent().get() == this) {
        mustGetFlexboxNode(childLayer)->setLayoutDirty();
    }
}

void FlexboxLayer::onChildRemoved(Layer* childLayer) {
    auto ownerNode = mustGetFlexboxNode(this);
    auto childNode = mustGetFlexboxNode(childLayer);

    YGNodeRemoveChild(ownerNode->yogaNode, childNode->yogaNode);
}

void FlexboxLayer::onChildInserted(Layer* childLayer, size_t index) {
    auto ownerNode = mustGetFlexboxNode(this);
    auto childNode = getOrCreateFlexboxNode(childLayer);

    auto* previousParent = YGNodeGetParent(childNode->yogaNode);
    if (previousParent != nullptr) {
        YGNodeRemoveChild(previousParent, childNode->yogaNode);
    }
    YGNodeInsertChild(ownerNode->yogaNode, childNode->yogaNode, static_cast<uint32_t>(index));
}

} // namespace snap::drawing
