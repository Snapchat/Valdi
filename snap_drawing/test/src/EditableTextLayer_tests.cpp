#include <gtest/gtest.h>

#include "TestGestureUtils.hpp"
#include "snap_drawing/cpp/Layers/EditableTextLayer.hpp"
#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include <string>
#include <vector>

using namespace Valdi;

namespace snap::drawing {

namespace {

class RecordingTextInputHost : public ITextInputHost {
public:
    void beginTextInput(const Ref<TextInputSession>& session) override {
        sessions.emplace_back(session);
        calls.emplace_back("begin:" + session->getInitialText().slowToString());
    }

    void endTextInput(const Ref<TextInputSession>& session) override {
        calls.emplace_back(session == sessions.back() ? "end" : "end:other");
    }

    std::vector<Ref<TextInputSession>> sessions;
    std::vector<std::string> calls;
};

class RecordingListener : public EditableTextLayerListener {
public:
    explicit RecordingListener(std::vector<std::string>& events) : _events(events) {}

    void onEditBegin(EditableTextLayer& layer) override {
        _events.emplace_back("begin:" + layer.getText().slowToString());
    }

    void onChange(EditableTextLayer& layer) override {
        _events.emplace_back("change:" + layer.getText().slowToString());
    }

    void onReturn(EditableTextLayer& layer) override {
        _events.emplace_back("return:" + layer.getText().slowToString());
    }

    void onEditEnd(EditableTextLayer& layer, EditTextEndReason reason) override {
        _events.emplace_back("end" + std::to_string(static_cast<int>(reason)) + ":" + layer.getText().slowToString());
    }

private:
    std::vector<std::string>& _events;
};

} // namespace

class EditableTextLayerTests : public ::testing::Test {
protected:
    void SetUp() override {
        _resources = makeShared<Resources>(nullptr, 1.0f, ConsoleLogger::getLogger());
        _root = makeLayer<LayerRoot>(_resources);
        _content = makeLayer<Layer>(_resources);
        _root->setContentLayer(_content, ContentLayerSizingModeMatchSize);
        _host = makeShared<RecordingTextInputHost>();
        _field = makeLayer<EditableTextLayer>(_resources);
        _field->setListener(makeShared<RecordingListener>(_events));
    }

    void TearDown() override {
        _field = nullptr;
        _root = nullptr;
        _content = nullptr;
        _resources = nullptr;
    }

    static std::string text(const EditableTextLayer& layer) {
        return layer.getText().slowToString();
    }

    void attach() {
        _root->setTextInputHost(_host);
        _content->addChild(_field);
    }

    Ref<Resources> _resources;
    Ref<LayerRoot> _root;
    Ref<Layer> _content;
    Ref<RecordingTextInputHost> _host;
    Ref<EditableTextLayer> _field;
    std::vector<std::string> _events;
};

TEST_F(EditableTextLayerTests, staysDisplayOnlyWithoutTextInputHost) {
    _content->addChild(_field);

    ASSERT_FALSE(_field->setFocused(true));
    ASSERT_TRUE(_events.empty());
}

TEST_F(EditableTextLayerTests, focusOpensSessionWithCurrentText) {
    attach();
    _field->setText(STRING_LITERAL("hi"));

    ASSERT_TRUE(_field->setFocused(true));

    ASSERT_EQ(std::vector<std::string>({"begin:hi"}), _host->calls);
    ASSERT_EQ(std::vector<std::string>({"begin:hi"}), _events);
}

TEST_F(EditableTextLayerTests, focusAskedBeforeAttachWaitsForTheTree) {
    _field->setFocused(true);
    ASSERT_TRUE(_host->calls.empty());

    attach();

    ASSERT_TRUE(_field->isFocused());
    ASSERT_EQ(std::vector<std::string>({"begin:"}), _host->calls);
}

TEST_F(EditableTextLayerTests, editsChangeTextAndFireChange) {
    attach();
    _field->setFocused(true);
    auto session = _host->sessions.back();

    session->replaceText(STRING_LITERAL("hel"));
    session->replaceText(STRING_LITERAL("héllo"));

    ASSERT_EQ("héllo", text(*_field));
    ASSERT_EQ(std::vector<std::string>({"begin:", "change:hel", "change:héllo"}), _events);
}

TEST_F(EditableTextLayerTests, commitReturnsAndEndsTextField) {
    attach();
    _field->setFocused(true);
    auto session = _host->sessions.back();

    session->replaceText(STRING_LITERAL("done"));
    session->commit();

    ASSERT_FALSE(_field->isFocused());
    ASSERT_EQ(std::vector<std::string>({"begin:", "change:done", "return:done", "end1:done"}), _events);
    ASSERT_EQ(std::vector<std::string>({"begin:", "end"}), _host->calls);
}

TEST_F(EditableTextLayerTests, commitKeepsEditingWhenNotClosingOnReturn) {
    attach();
    _field->setClosesWhenReturnKeyPressed(false);
    _field->setFocused(true);

    _host->sessions.back()->commit();

    ASSERT_TRUE(_field->isFocused());
    ASSERT_EQ(std::vector<std::string>({"begin:", "return:"}), _events);
}

TEST_F(EditableTextLayerTests, cancelEndsAsDismissed) {
    attach();
    _field->setFocused(true);

    _host->sessions.back()->cancel();

    ASSERT_FALSE(_field->isFocused());
    ASSERT_EQ(std::vector<std::string>({"begin:", "end2:"}), _events);
}

TEST_F(EditableTextLayerTests, endedSessionIgnoresLateEdits) {
    attach();
    _field->setFocused(true);
    auto first = _host->sessions.back();
    _field->setFocused(false);
    _field->setFocused(true);

    first->replaceText(STRING_LITERAL("late"));
    first->commit();

    ASSERT_TRUE(_field->isFocused());
    ASSERT_EQ("", text(*_field));
    ASSERT_EQ(std::vector<std::string>({"begin:", "end0:", "begin:"}), _events);
}

TEST_F(EditableTextLayerTests, blurEndsSession) {
    attach();
    _field->setFocused(true);

    _field->setFocused(false);

    ASSERT_EQ(std::vector<std::string>({"begin:", "end"}), _host->calls);
    ASSERT_EQ(std::vector<std::string>({"begin:", "end0:"}), _events);
}

TEST_F(EditableTextLayerTests, leavingTheTreeEndsSession) {
    attach();
    _field->setFocused(true);

    _field->removeFromParent();

    ASSERT_FALSE(_field->isFocused());
    ASSERT_EQ(std::vector<std::string>({"begin:", "end"}), _host->calls);
}

TEST_F(EditableTextLayerTests, disablingEndsSessionAndBlocksFocus) {
    attach();
    _field->setFocused(true);

    _field->setEnabled(false);

    ASSERT_FALSE(_field->isFocused());
    ASSERT_FALSE(_field->setFocused(true));
    ASSERT_EQ(std::vector<std::string>({"begin:", "end"}), _host->calls);
}

TEST_F(EditableTextLayerTests, focusAskedWhileDisabledDoesNotFocusOnceEnabled) {
    _field->setEnabled(false);
    _field->setFocused(true);

    _field->setEnabled(true);
    attach();

    ASSERT_FALSE(_field->isFocused());
    ASSERT_TRUE(_host->calls.empty());
}

TEST_F(EditableTextLayerTests, disablingClearsAPendingFocus) {
    _field->setFocused(true);

    _field->setEnabled(false);
    _field->setEnabled(true);
    attach();

    ASSERT_FALSE(_field->isFocused());
    ASSERT_TRUE(_host->calls.empty());
}

TEST_F(EditableTextLayerTests, tapOnTheFocusedFieldDoesNotReachTheParent) {
    auto container = makeContainer(0, 0, 100, 100);
    auto host = makeShared<RecordingTextInputHost>();
    container->root->setTextInputHost(host);
    auto field = makeLayer<EditableTextLayer>(_resources);
    field->setFrame(Rect::makeXYWH(25, 25, 25, 25));
    container->view->addChild(field);
    auto parentTap = addSingleTapGesture(container->view);

    container->dispatchEvent(TouchEventTypeDown, 30, 35);
    container->dispatchEvent(TouchEventTypeUp, 30, 35);
    ASSERT_TRUE(field->isFocused());
    const auto parentTapsAfterFocus = parentTap->counter;

    container->dispatchEvent(TouchEventTypeDown, 30, 35);
    container->dispatchEvent(TouchEventTypeUp, 30, 35);

    ASSERT_TRUE(field->isFocused());
    ASSERT_EQ(parentTapsAfterFocus, parentTap->counter);
}

TEST_F(EditableTextLayerTests, movingToARootWithoutHostDropsTheFocusTap) {
    attach();
    const auto withTap = _field->getGestureRecognizersSize();
    ASSERT_GT(withTap, 0u);

    auto otherRoot = makeLayer<LayerRoot>(_resources);
    auto otherContent = makeLayer<Layer>(_resources);
    otherRoot->setContentLayer(otherContent, ContentLayerSizingModeMatchSize);
    _field->removeFromParent();
    otherContent->addChild(_field);

    ASSERT_EQ(withTap - 1, _field->getGestureRecognizersSize());

    otherRoot->destroy();
    _root->destroy();
}

TEST_F(EditableTextLayerTests, destroyingTheRootReleasesTheHost) {
    attach();
    _field->setFocused(true);
    ASSERT_GT(_host->retainCount(), 1);

    _root->destroy();

    ASSERT_EQ(1, _host->retainCount());
}

} // namespace snap::drawing
