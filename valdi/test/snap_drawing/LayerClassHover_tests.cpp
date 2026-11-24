#include <gtest/gtest.h>

#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/snap_drawing/Layers/Classes/LayerClass.hpp"
#include "valdi/snap_drawing/Utils/AttributesBinderUtils.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <vector>

namespace snap::drawing {

namespace {

class LayerClassHoverTest : public ::testing::Test {
protected:
    void SetUp() override {
        root->setContentLayer(content, ContentLayerSizingModeMatchSize);
        root->setSize(Size::make(100, 100), 1);
        root->processFrame(TimePoint::fromSeconds(0));
        layer->setFrame(Rect::makeXYWH(0, 0, 30, 30));
        content->addChild(layer);
    }

    void bind(std::vector<Valdi::Value>& events) {
        auto handler = Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
            [&events](const Valdi::ValueFunctionCallContext& callContext) {
                events.emplace_back(callContext.getParameter(0));
                return Valdi::Value::undefined();
            });
        ASSERT_TRUE(layerClass.apply_onHoverChange(*layer, handler, context));
    }

    void hoverAtOrigin() {
        root->dispatchHoverEvent(Point::make(0, 0));
    }

    void processFrame() {
        root->processFrame(TimePoint::fromSeconds(0));
    }

    Valdi::Ref<Valdi::ViewNode> createNode() {
        return Valdi::makeShared<Valdi::ViewNode>(nullptr, attributeIds, nullptr, Valdi::ConsoleLogger::getLogger());
    }

    Valdi::AttributeIds attributeIds;
    Ref<Resources> resources = Valdi::makeShared<Resources>(nullptr, 1.0f, Valdi::ConsoleLogger::getLogger());
    Ref<LayerRoot> root = makeLayer<LayerRoot>(resources);
    Ref<Layer> content = makeLayer<Layer>(resources);
    Ref<Layer> layer = makeLayer<Layer>(resources);
    LayerClass layerClass{resources};
    Valdi::StringBox attributeName = STRING_LITERAL("onHoverChange");
    AttributeContext context{Valdi::Ref<Valdi::Animator>(), attributeName};
};

} // namespace

TEST_F(LayerClassHoverTest, replacementReceivesExitWithoutAnotherEnter) {
    std::vector<Valdi::Value> original;
    std::vector<Valdi::Value> replacement;
    bind(original);
    auto node = createNode();
    setValdiViewNodeToLayer(*layer, node.get());
    hoverAtOrigin();
    ASSERT_EQ(1u, original.size());
    EXPECT_TRUE(original.front().getMapValue(STRING_LITERAL("hovered")).toBool());

    bind(replacement);
    processFrame();
    EXPECT_EQ(1u, original.size());
    EXPECT_TRUE(replacement.empty());

    root->dispatchHoverEvent(std::nullopt);
    ASSERT_EQ(1u, replacement.size());
    EXPECT_FALSE(replacement.front().getMapValue(STRING_LITERAL("hovered")).toBool());
    EXPECT_FALSE(replacement.front().getMapValue(STRING_LITERAL("detached")).toBool());
    EXPECT_EQ(0, replacement.front().getMapValue(STRING_LITERAL("x")).toDouble());
}

TEST_F(LayerClassHoverTest, resetExitsBeforeTheLayerIsRebound) {
    auto oldNode = createNode();
    auto newNode = createNode();
    setValdiViewNodeToLayer(*layer, oldNode.get());
    std::vector<Valdi::Value> original;
    std::vector<Valdi::Value> replacement;
    bind(original);
    hoverAtOrigin();
    ASSERT_EQ(1u, original.size());

    layer->removeFromParent();
    layerClass.reset_onHoverChange(*layer, context);
    ASSERT_EQ(2u, original.size());
    EXPECT_FALSE(original.back().getMapValue(STRING_LITERAL("hovered")).toBool());
    EXPECT_TRUE(original.back().getMapValue(STRING_LITERAL("detached")).toBool());

    layer->setAttachedData(nullptr);
    setValdiViewNodeToLayer(*layer, newNode.get());
    bind(replacement);
    content->addChild(layer);
    processFrame();
    ASSERT_EQ(1u, replacement.size());
    EXPECT_TRUE(replacement.front().getMapValue(STRING_LITERAL("hovered")).toBool());
    EXPECT_EQ(2u, original.size());
}

TEST_F(LayerClassHoverTest, resetSkipsExitWhenEnterHadNoViewNode) {
    std::vector<Valdi::Value> events;
    bind(events);
    hoverAtOrigin();
    layerClass.reset_onHoverChange(*layer, context);
    EXPECT_TRUE(events.empty());
}

} // namespace snap::drawing
