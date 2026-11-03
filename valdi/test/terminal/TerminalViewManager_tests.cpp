#include "TerminalTestUtils.hpp"

#include "valdi/runtime/Attributes/AttributesBindingContext.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"
#include "valdi/terminal/Host/TerminalRoot.hpp"
#include "valdi/terminal/TerminalViewTransaction.hpp"

#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

TEST(TerminalViewManager, reportsTheTerminalBackend) {
    TerminalViewManager manager;
    ASSERT_EQ(RenderingBackendTypeTerminal, manager.getRenderingBackendType());
    ASSERT_EQ(PlatformTypeIOS, manager.getPlatformType());
    ASSERT_EQ(1.0f, manager.getPointScale());
    ASSERT_EQ(nullptr, manager.getViewRasterBitmapFactory());
}

TEST(TerminalViewManager, usesPlatformSpecificDefaultClassNames) {
    TerminalViewManager ios(PlatformTypeIOS);
    ASSERT_EQ("SCValdiView", ios.getDefaultViewClass());

    TerminalViewManager android(PlatformTypeAndroid);
    ASSERT_EQ("com.snap.valdi.views.ValdiView", android.getDefaultViewClass());
}

TEST(TerminalViewManager, registersClassesUnderBothPlatformNames) {
    TerminalViewManager manager;
    for (const char* name : {"SCValdiView",
                             "com.snap.valdi.views.ValdiView",
                             "SCValdiLabel",
                             "com.snap.valdi.views.ValdiTextView",
                             "SCValdiScrollView",
                             "com.snap.valdi.views.ValdiScrollView",
                             "ValdiTerminalStatic"}) {
        const auto className = StringCache::getGlobal().makeString(std::string_view(name));
        ASSERT_NE(nullptr, manager.getViewClass(className)) << name;
        ASSERT_TRUE(manager.supportsClassNameNatively(className)) << name;
    }
    ASSERT_EQ(nullptr, manager.getViewClass(STRING_LITERAL("UnknownView")));
    ASSERT_FALSE(manager.supportsClassNameNatively(STRING_LITERAL("UnknownView")));
}

TEST(TerminalViewManager, walksTheParentChainForTheClassHierarchy) {
    TerminalViewManager ios(PlatformTypeIOS);
    ASSERT_EQ((std::vector<StringBox>{STRING_LITERAL("SCValdiLabel"), STRING_LITERAL("SCValdiView")}),
              ios.getClassHierarchy(STRING_LITERAL("SCValdiLabel")));
    ASSERT_EQ((std::vector<StringBox>{STRING_LITERAL("SCValdiView")}),
              ios.getClassHierarchy(STRING_LITERAL("SCValdiView")));

    TerminalViewManager android(PlatformTypeAndroid);
    ASSERT_EQ((std::vector<StringBox>{STRING_LITERAL("com.snap.valdi.views.ValdiScrollView"),
                                      STRING_LITERAL("com.snap.valdi.views.ValdiView")}),
              android.getClassHierarchy(STRING_LITERAL("com.snap.valdi.views.ValdiScrollView")));
}

TEST(TerminalViewManager, unknownClassesFallBackToTheBaseClass) {
    TerminalViewManager manager;
    ASSERT_EQ((std::vector<StringBox>{STRING_LITERAL("UnknownView"), STRING_LITERAL("SCValdiView")}),
              manager.getClassHierarchy(STRING_LITERAL("UnknownView")));

    auto factory = manager.createViewFactory(STRING_LITERAL("UnknownView"), nullptr);
    auto view = TerminalView::unwrap(factory->createView(nullptr, nullptr, false));
    ASSERT_NE(nullptr, view);
    ASSERT_STREQ("SCValdiView", view->getViewClass()->getIOSClassName());
}

TEST(TerminalViewManager, viewFactoriesCreateViewsOfTheRequestedClass) {
    TerminalViewManager manager;
    auto factory = manager.createViewFactory(STRING_LITERAL("SCValdiLabel"), nullptr);
    auto view = TerminalView::unwrap(factory->createView(nullptr, nullptr, false));
    ASSERT_NE(nullptr, view);
    ASSERT_STREQ("SCValdiLabel", view->getViewClass()->getIOSClassName());
    ASSERT_EQ(snap::valdi_core::Platform::Ios, view->getPlatform());

    TerminalViewManager android(PlatformTypeAndroid);
    auto androidView =
        TerminalView::unwrap(android.createViewFactory(STRING_LITERAL("com.snap.valdi.views.ValdiTextView"), nullptr)
                                 ->createView(nullptr, nullptr, false));
    ASSERT_EQ(snap::valdi_core::Platform::Android, androidView->getPlatform());
}

TEST(TerminalViewManager, registeredClassesReplaceBuiltInOnes) {
    TerminalViewManager manager;
    auto base = manager.getViewClass(STRING_LITERAL("SCValdiView"));
    auto replacement = makeShared<LabelClass>(base);
    manager.registerClass(replacement);
    ASSERT_EQ(replacement.get(), manager.getViewClass(STRING_LITERAL("SCValdiLabel")).get());
}

TEST(TerminalViewManager, createsDirectTransactionsOnTheMainThread) {
    TerminalViewManager manager;
    auto transaction = manager.createViewTransaction(nullptr, true);
    ASSERT_NE(nullptr, castOrNull<TerminalViewTransaction>(transaction));
}

TEST(TerminalViewTransaction, appliesFramesChildrenAndScrollSpecs) {
    TerminalViewBuilder b;
    auto parent = b.box(0, 0, 10, 4);
    auto child = b.box(0, 0, 0, 0);
    TerminalViewTransaction transaction;

    transaction.insertChildView(parent, child, 0, nullptr);
    ASSERT_EQ(1u, parent->getChildren().size());
    ASSERT_EQ(parent.get(), child->getParent());

    transaction.setViewFrame(child, Frame(1, 2, 3, 4), false, nullptr);
    ASSERT_EQ(Frame(1, 2, 3, 4), child->getFrame());

    transaction.setViewScrollSpecs(parent, Point(0, 5), Size(10, 20), false);
    ASSERT_EQ(Point(0, 5), parent->getScrollOffset());
    ASSERT_EQ(Size(10, 20), parent->getContentSize());

    transaction.removeViewFromParent(child, nullptr, true);
    ASSERT_TRUE(parent->getChildren().empty());
    ASSERT_EQ(nullptr, child->getParent());
}

TEST(TerminalViewTransaction, insertingAtAnIndexReordersChildren) {
    TerminalViewBuilder b;
    auto parent = b.box(0, 0, 10, 4);
    auto first = b.box(0, 0, 1, 1);
    auto second = b.box(0, 0, 1, 1);
    auto inserted = b.box(0, 0, 1, 1);
    TerminalViewTransaction transaction;
    transaction.insertChildView(parent, first, 0, nullptr);
    transaction.insertChildView(parent, second, 1, nullptr);
    transaction.insertChildView(parent, inserted, 1, nullptr);
    ASSERT_EQ(first.get(), parent->getChildren()[0].get());
    ASSERT_EQ(inserted.get(), parent->getChildren()[1].get());
    ASSERT_EQ(second.get(), parent->getChildren()[2].get());

    // Moving a view to a new parent detaches it from the old one.
    auto other = b.box(0, 0, 10, 4);
    transaction.insertChildView(other, inserted, 0, nullptr);
    ASSERT_EQ(2u, parent->getChildren().size());
    ASSERT_EQ(other.get(), inserted->getParent());
}

TEST(TerminalViewTransaction, runsOnNextDrawCallbacksWhenTheRootUpdateEnds) {
    TerminalViewBuilder b;
    auto root = b.box(0, 0, 10, 4);
    TerminalViewTransaction transaction;
    int calls = 0;
    transaction.scheduleOnNextDraw(root, DispatchFunction([&]() { calls++; }));
    ASSERT_EQ(0, calls);
    transaction.didUpdateRootView(root, true);
    ASSERT_EQ(1, calls);
    transaction.didUpdateRootView(root, false);
    ASSERT_EQ(1, calls);
}

TEST(TerminalViewTransaction, executesWorkImmediately) {
    TerminalViewTransaction transaction;
    bool ran = false;
    transaction.executeInTransactionThread(DispatchFunction([&]() { ran = true; }));
    ASSERT_TRUE(ran);
}

} // namespace ValdiTest
