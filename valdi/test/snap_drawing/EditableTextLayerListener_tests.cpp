#include <gtest/gtest.h>

#include "ViewNodeTestsUtils.hpp"
#include "snap_drawing/cpp/Layers/EditableTextLayer.hpp"
#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "valdi/snap_drawing/Layers/Classes/EditableTextLayerClass.hpp"
#include "valdi/snap_drawing/Utils/ValdiUtils.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <vector>

using namespace Valdi;

namespace snap::drawing {

namespace {

class SessionKeepingHost : public ITextInputHost {
public:
    void beginTextInput(const Ref<TextInputSession>& newSession) override {
        session = newSession;
    }

    void endTextInput(const Ref<TextInputSession>& /*endedSession*/) override {}

    Ref<TextInputSession> session;
};

Ref<ValueFunction> recordInto(std::vector<Value>& events) {
    return makeShared<ValueFunctionWithCallable>([&events](const ValueFunctionCallContext& callContext) {
        events.emplace_back(callContext.getParameter(0));
        return Value::undefined();
    });
}

} // namespace

TEST(EditableTextLayerListener, reportsEditsToTheElementsHandlersAsEditTextEvents) {
    auto resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());
    auto root = makeLayer<LayerRoot>(resources);
    auto content = makeLayer<Layer>(resources);
    root->setContentLayer(content, ContentLayerSizingModeMatchSize);
    auto host = makeShared<SessionKeepingHost>();
    root->setTextInputHost(host);
    auto field = makeLayer<EditableTextLayer>(resources);
    auto listener = makeShared<EditableTextLayerListenerImpl>();
    std::vector<Value> begins;
    std::vector<Value> changes;
    std::vector<Value> returns;
    std::vector<Value> ends;
    listener->onEditBeginFn = recordInto(begins);
    listener->onChangeFn = recordInto(changes);
    listener->onReturnFn = recordInto(returns);
    listener->onEditEndFn = recordInto(ends);
    field->setListener(listener);
    content->addChild(field);

    field->setFocused(true);
    host->session->replaceText(STRING_LITERAL("hé😀"));
    host->session->commit();

    ASSERT_EQ(1u, begins.size());
    ASSERT_EQ(1u, changes.size());
    ASSERT_EQ(1u, returns.size());
    ASSERT_EQ(1u, ends.size());
    const auto& change = changes[0];
    ASSERT_EQ("hé😀", change.getMapValue(STRING_LITERAL("text")).toStringBox().slowToString());
    // JS counts the emoji as two UTF-16 code units.
    ASSERT_EQ(4, change.getMapValue(STRING_LITERAL("selectionStart")).toInt());
    ASSERT_EQ(4, change.getMapValue(STRING_LITERAL("selectionEnd")).toInt());
    ASSERT_EQ(static_cast<int32_t>(EditTextEndReason::ReturnKeyPress), ends[0].getMapValue(STRING_LITERAL("reason")).toInt());
}

TEST(EditableTextLayerListener, remeasuresTheElementAfterAnEdit) {
    ValdiTest::ViewNodeTestsDependencies utils;
    auto rootNode = utils.createRootView();
    auto fieldNode = utils.createView();
    rootNode->appendChild(utils.getViewTransactionScope(), fieldNode);
    rootNode->performLayout(utils.getViewTransactionScope(), Valdi::Size(100, 100), LayoutDirectionLTR);
    ASSERT_FALSE(fieldNode->isFlexLayoutDirty());

    auto resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());
    auto root = makeLayer<LayerRoot>(resources);
    auto content = makeLayer<Layer>(resources);
    root->setContentLayer(content, ContentLayerSizingModeMatchSize);
    auto host = makeShared<SessionKeepingHost>();
    root->setTextInputHost(host);
    auto field = makeLayer<EditableTextLayer>(resources);
    field->setListener(makeShared<EditableTextLayerListenerImpl>());
    setValdiViewNodeToLayer(*field, fieldNode.get());
    content->addChild(field);

    field->setFocused(true);
    host->session->replaceText(STRING_LITERAL("abc"));

    ASSERT_TRUE(fieldNode->isFlexLayoutDirty());
}

} // namespace snap::drawing
