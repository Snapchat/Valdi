#include "ViewNodeTestsUtils.hpp"
#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

void assertAllFlagsAreUpToDate(ViewNode* viewNode) {
    ASSERT_FALSE(viewNode->isFlexLayoutDirty());
    ASSERT_FALSE(viewNode->calculatedViewportHasChildNeedsUpdate());
    ASSERT_FALSE(viewNode->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(viewNode->cssNeedsUpdate());
    ASSERT_FALSE(viewNode->viewTreeNeedsUpdate());

    for (auto* child : *viewNode) {
        assertAllFlagsAreUpToDate(child);
    }
}

TEST(ViewNode, canInsertChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();
    auto child2 = utils.createLayout();

    root->insertChildAt(utils.getViewTransactionScope(), child, 0);
    root->insertChildAt(utils.getViewTransactionScope(), child2, 1);

    ASSERT_EQ(static_cast<size_t>(2), root->getChildCount());

    ASSERT_EQ(child.get(), root->getChildAt(0));
    ASSERT_EQ(child2.get(), root->getChildAt(1));

    ASSERT_EQ(2, static_cast<int>(YGNodeGetChildCount(root->getYogaNode())));
    ASSERT_EQ(child->getYogaNode(), YGNodeGetChild(root->getYogaNode(), 0));
    ASSERT_EQ(child2->getYogaNode(), YGNodeGetChild(root->getYogaNode(), 1));
}

TEST(ViewNode, canRemoveChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();
    auto child2 = utils.createLayout();

    root->insertChildAt(utils.getViewTransactionScope(), child, 0);
    root->insertChildAt(utils.getViewTransactionScope(), child2, 1);

    ASSERT_EQ(static_cast<size_t>(2), root->getChildCount());

    child->removeFromParent(utils.getViewTransactionScope());

    ASSERT_EQ(static_cast<size_t>(1), root->getChildCount());

    ASSERT_EQ(child2.get(), root->getChildAt(0));

    ASSERT_EQ(1, static_cast<int>(YGNodeGetChildCount(root->getYogaNode())));
    ASSERT_EQ(child2->getYogaNode(), YGNodeGetChild(root->getYogaNode(), 0));
}

TEST(ViewNode, canIterateOverChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), child);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    int i = 0;
    for (auto* viewNode : *root) {
        ASSERT_TRUE(i < 3);

        if (i == 0) {
            ASSERT_EQ(child.get(), viewNode);
        } else if (i == 1) {
            ASSERT_EQ(child2.get(), viewNode);
        } else if (i == 2) {
            ASSERT_EQ(child3.get(), viewNode);
        }
        i++;
    }

    ASSERT_EQ(3, i);
}

TEST(ViewNode, doesntRetainOnInsertChild) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    ASSERT_EQ(1, static_cast<int>(root.use_count()));
    ASSERT_EQ(1, static_cast<int>(child.use_count()));

    root->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_EQ(1, static_cast<int>(root.use_count()));
    ASSERT_EQ(1, static_cast<int>(child.use_count()));
}

TEST(ViewNode, removesItselfFromParentOnDealloc) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_EQ(1, static_cast<int>(root->getChildCount()));

    child = nullptr;

    ASSERT_EQ(0, static_cast<int>(root->getChildCount()));
}

TEST(ViewNode, canCalculateLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();
    auto deeperChild = utils.createLayout();

    utils.setViewNodeAttribute(deeperChild, "marginLeft", Value(12.0));
    utils.setViewNodeAttribute(deeperChild, "marginRight", Value(8.0));
    utils.setViewNodeAttribute(deeperChild, "height", Value(100.0));
    deeperChild->getAttributesApplier().flush(utils.getViewTransactionScope());

    child->appendChild(utils.getViewTransactionScope(), deeperChild);
    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(child->isFlexLayoutDirty());
    ASSERT_FALSE(deeperChild->isFlexLayoutDirty());

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 0, 100, 100), child->getCalculatedFrame());
    ASSERT_EQ(Frame(12, 0, 80, 100), deeperChild->getCalculatedFrame());
}

TEST(ViewNode, updateFlagsWhenChildrenAreChanging) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();

    ASSERT_TRUE(root->viewTreeNeedsUpdate());
    ASSERT_TRUE(root->isFlexLayoutDirty());
    ASSERT_TRUE(root->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(root->calculatedViewportHasChildNeedsUpdate());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->viewTreeNeedsUpdate());
    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(root->calculatedViewportHasChildNeedsUpdate());

    auto child = utils.createLayout();

    root->insertChildAt(utils.getViewTransactionScope(), child, 0);

    ASSERT_TRUE(root->viewTreeNeedsUpdate());
    ASSERT_TRUE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->calculatedViewportNeedsUpdate());
    ASSERT_TRUE(root->calculatedViewportHasChildNeedsUpdate());

    ASSERT_TRUE(child->viewTreeNeedsUpdate());
    ASSERT_TRUE(child->isFlexLayoutDirty());
    ASSERT_TRUE(child->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(child->calculatedViewportHasChildNeedsUpdate());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->viewTreeNeedsUpdate());
    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(root->calculatedViewportHasChildNeedsUpdate());

    ASSERT_FALSE(child->viewTreeNeedsUpdate());
    ASSERT_FALSE(child->isFlexLayoutDirty());
    ASSERT_FALSE(child->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(child->calculatedViewportHasChildNeedsUpdate());

    child->removeFromParent(utils.getViewTransactionScope());

    ASSERT_TRUE(root->viewTreeNeedsUpdate());
    ASSERT_TRUE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(root->calculatedViewportHasChildNeedsUpdate());
}

TEST(ViewNode, createViewsWhenNodesAreVisible) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child = utils.createView();
    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));

    root->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_TRUE(root->hasView());
    ASSERT_TRUE(root->getView() != nullptr);
    ASSERT_FALSE(child->hasView());
    ASSERT_FALSE(child->getView() != nullptr);

    root->updateViewTree(utils.getViewTransactionScope());

    // We should always have a root view
    ASSERT_TRUE(root->hasView());
    ASSERT_TRUE(root->getView() != nullptr);
    ASSERT_FALSE(child->hasView());
    ASSERT_FALSE(child->getView() != nullptr);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_TRUE(root->hasView());
    ASSERT_TRUE(root->getView() != nullptr);
    ASSERT_FALSE(child->hasView());
    ASSERT_FALSE(child->getView() != nullptr);

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->hasView());
    ASSERT_TRUE(root->getView() != nullptr);
    ASSERT_TRUE(child->hasView());
    ASSERT_TRUE(child->getView() != nullptr);
}

TEST(ViewNode, callsInvalidateLayoutWhenTreeChanges) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), child);
    utils.getTree().setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);
    utils.getTree().performUpdates();

    auto rootView = StandaloneView::unwrap(root->getView());

    ASSERT_EQ(1, rootView->getInvalidateLayoutCount());

    child->removeFromParent(utils.getViewTransactionScope());

    ASSERT_EQ(2, rootView->getInvalidateLayoutCount());
    ASSERT_TRUE(root->isFlexLayoutDirty());

    utils.getTree().setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);
    utils.getTree().performUpdates();

    ASSERT_EQ(2, rootView->getInvalidateLayoutCount());
    ASSERT_FALSE(root->isFlexLayoutDirty());

    root->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_EQ(3, rootView->getInvalidateLayoutCount());
    ASSERT_TRUE(root->isFlexLayoutDirty());
}

TEST(ViewNode, callsInvalidateLayoutWhenLayoutAttributeChange) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), child);
    utils.getTree().setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);
    utils.getTree().performUpdates();

    auto rootView = StandaloneView::unwrap(root->getView());

    ASSERT_EQ(1, rootView->getInvalidateLayoutCount());

    utils.setViewNodeAttribute(child, "height", Value(50.0));

    ASSERT_EQ(2, rootView->getInvalidateLayoutCount());
    ASSERT_TRUE(root->isFlexLayoutDirty());
    ASSERT_TRUE(child->isFlexLayoutDirty());
}

TEST(ViewNode, providesLayoutDirtyFlagWhenLayoutAttributeChange) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), child);
    utils.getTree().setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);
    utils.getTree().performUpdates();

    auto rootView = StandaloneView::unwrap(root->getView());

    // This flag is passed only after runUpdates
    ASSERT_EQ(0, rootView->getLayoutDidBecomeDirtyCount());
    // We should have gotten the initial invalidateLayoutCount
    ASSERT_EQ(1, rootView->getInvalidateLayoutCount());

    utils.disableUpdates();
    utils.getTree().unsafeEndViewTransaction();

    ASSERT_EQ(0, rootView->getLayoutDidBecomeDirtyCount());
    ASSERT_EQ(1, rootView->getInvalidateLayoutCount());

    utils.getTree().scheduleExclusiveUpdate([&]() { utils.setViewNodeAttribute(child, "height", Value(50.0)); });

    ASSERT_EQ(1, rootView->getLayoutDidBecomeDirtyCount());
    ASSERT_EQ(2, rootView->getInvalidateLayoutCount());

    // Measure the layout, to simulate the consumer observing the new layout
    utils.getTree().measureLayout(100, MeasureModeExactly, 100, MeasureModeExactly, LayoutDirectionLTR);

    utils.getTree().scheduleExclusiveUpdate([&]() { utils.setViewNodeAttribute(child, "height", Value(75.0)); });

    // Layout dirty flag should have increased, even if the layoutSpecs are still dirty
    ASSERT_EQ(2, rootView->getLayoutDidBecomeDirtyCount());
    // InvalidateLayout should not have been called, as our layoutSpecs was already dirty
    ASSERT_EQ(2, rootView->getInvalidateLayoutCount());

    utils.getTree().scheduleExclusiveUpdate([&]() {
        // Don't change anything
        utils.setViewNodeAttribute(child, "height", Value(75.0));
    });

    // Nothing should have been called
    ASSERT_EQ(2, rootView->getLayoutDidBecomeDirtyCount());
    ASSERT_EQ(2, rootView->getInvalidateLayoutCount());
}

TEST(ViewNode, supportsAllMeasureModes) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child1 = utils.createView();
    auto child2 = utils.createView();
    auto child3 = utils.createView();

    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap")));
    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));

    utils.setViewNodeAttribute(child1, "width", Value(10.0));
    utils.setViewNodeAttribute(child1, "height", Value(5.0));

    utils.setViewNodeAttribute(child2, "width", Value(20.0));
    utils.setViewNodeAttribute(child2, "height", Value(10.0));

    utils.setViewNodeAttribute(child3, "width", Value(20.0));
    utils.setViewNodeAttribute(child3, "height", Value(15.0));

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    auto size = root->measureLayout(30, MeasureModeUnspecified, 30, MeasureModeUnspecified, LayoutDirectionLTR);

    /**
     * Should be laid out as:
     * [child1][child2][child3]
     */
    ASSERT_EQ(50.f, size.width);
    ASSERT_EQ(15.0f, size.height);

    size = root->measureLayout(30, MeasureModeExactly, 200, MeasureModeUnspecified, LayoutDirectionLTR);

    /**
     * Should be laid out as:
     * [child1][child2]
     * [child3]
     */
    ASSERT_EQ(30.f, size.width);
    ASSERT_EQ(25.0f, size.height);

    size = root->measureLayout(30, MeasureModeExactly, 60, MeasureModeExactly, LayoutDirectionLTR);

    /**
     * Should be laid out as:
     * [child1][child2]
     * [child3]
     */
    ASSERT_EQ(30.f, size.width);
    ASSERT_EQ(60.0f, size.height);

    size = root->measureLayout(25, MeasureModeAtMost, 60, MeasureModeUnspecified, LayoutDirectionLTR);

    /**
     * Should be laid out as:
     * [child1]
     * [child2]
     * [child3]
     */
    ASSERT_EQ(25.f, size.width);
    ASSERT_EQ(30.0f, size.height);
}

TEST(ViewNode, canCalculateViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    auto deepChild = utils.createLayout();
    auto deepChild2 = utils.createLayout();
    auto deepChild3 = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), child);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    child->appendChild(utils.getViewTransactionScope(), deepChild);
    child2->appendChild(utils.getViewTransactionScope(), deepChild2);
    child3->appendChild(utils.getViewTransactionScope(), deepChild3);

    utils.setViewNodeFrame(child, 0, 0, 50, 50);
    utils.setViewNodeFrame(child2, 75, 25, 125, 50);
    utils.setViewNodeFrame(child3, 100, 100, 50, 50);

    utils.setViewNodeFrame(deepChild, 10, 10, 20, 20);
    utils.setViewNodeFrame(deepChild2, 10, 25, 30, 50);
    utils.setViewNodeFrame(deepChild3, 0, 0, 50, 50);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());

    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());

    ASSERT_TRUE(deepChild->isVisibleInViewport());
    ASSERT_TRUE(deepChild2->isVisibleInViewport());
    ASSERT_FALSE(deepChild3->isVisibleInViewport());

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getCalculatedViewport());

    ASSERT_EQ(Frame(0, 0, 50, 50), child->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 25, 50), child2->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 0, 0), child3->getCalculatedViewport());

    ASSERT_EQ(Frame(0, 0, 20, 20), deepChild->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 15, 25), deepChild2->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 0, 0), deepChild3->getCalculatedViewport());

    // Move child3 into the visible region

    utils.setViewNodeFrame(child3, 0, 0, 50, 50);
    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());

    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_TRUE(child3->isVisibleInViewport());

    ASSERT_TRUE(deepChild->isVisibleInViewport());
    ASSERT_TRUE(deepChild2->isVisibleInViewport());
    ASSERT_TRUE(deepChild3->isVisibleInViewport());

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getCalculatedViewport());

    ASSERT_EQ(Frame(0, 0, 50, 50), child->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 25, 50), child2->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 50, 50), child3->getCalculatedViewport());

    ASSERT_EQ(Frame(0, 0, 20, 20), deepChild->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 15, 25), deepChild2->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 50, 50), deepChild3->getCalculatedViewport());
}

TEST(ViewNode, canCalculateViewportWithTranslate) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), child);

    utils.setViewNodeFrame(child, 20, 20, 100, 100);
    utils.setViewNodeAttribute(child, "translationX", Value(4));
    utils.setViewNodeAttribute(child, "translationY", Value(3));

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getCalculatedViewport());
    ASSERT_EQ(Frame(0, 0, 76, 77), child->getCalculatedViewport());
}

TEST(ViewNode, canCalculateViewportWithTranslateInRTL) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto child = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), child);

    utils.setViewNodeFrame(child, 20, 20, 100, 100);
    utils.setViewNodeAttribute(child, "translationX", Value(4));
    utils.setViewNodeAttribute(child, "translationY", Value(3));

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionRTL);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getCalculatedViewport());
    ASSERT_EQ(Frame(24, 0, 76, 77), child->getCalculatedViewport());
}

TEST(ViewNode, canUseUserDefinedViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);
    container->appendChild(utils.getViewTransactionScope(), child2);
    container->appendChild(utils.getViewTransactionScope(), child3);

    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(child, 0, 0, 50, 50);
    utils.setViewNodeFrame(child2, 0, 50, 50, 50);
    utils.setViewNodeFrame(child3, 0, 100, 50, 50);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());

    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->calculatedViewportNeedsUpdate());
    ASSERT_FALSE(root->calculatedViewportHasChildNeedsUpdate());

    container->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 50));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    // Setting the userDefinedViewport should immediately update the tree,
    // bypassing
    assertAllFlagsAreUpToDate(root.get());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_TRUE(child3->isVisibleInViewport());

    container->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 150));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    assertAllFlagsAreUpToDate(root.get());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_FALSE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());
}

TEST(ViewNode, canExtendViewportWithChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(child, 100, 100, 50, 50);
    child->setTranslationX(10);
    child->setTranslationY(20);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_EQ(Frame(0, 0, 100, 100), container->getCalculatedViewport());

    // Set the extend viewport flag

    container->setExtendViewportWithChildren(true);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_EQ(Frame(0, 0, 160, 170), container->getCalculatedViewport());
}

TEST(ViewNode, scaleTransformAffectsViewportIntersection) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // child frame (100..150 x 0..50) is outside container (0..100 x 0..100)
    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(child, 100, 0, 50, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());

    // Scale 3x from center: child center x=125, scaled left = 125 - 75 = 50, which overlaps 0..100
    child->setScaleX(3.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_TRUE(child->isVisibleInViewport());

    // Reset scale — should be invisible again
    child->setScaleX(1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, scaleYTransformAffectsViewportIntersection) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // child frame (0..50 x 100..150) is outside container (0..100 x 0..100)
    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(child, 0, 100, 50, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());

    // Scale 3x from center: child center y=125, scaled top = 125 - 75 = 50, which overlaps 0..100
    child->setScaleY(3.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_TRUE(child->isVisibleInViewport());

    child->setScaleY(1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, positiveScaleXOnContainerCorrectlyClipsChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto childIn = utils.createLayout();
    auto childOut = utils.createLayout();

    // Container: 400 wide (2x viewport). Viewport: 200x200.
    // scaleX=2 expands container to visual width 800, centered at x=200.
    // Visible local range: x=100..200 (viewport 0..200 maps to local /2 = 0..100
    // offset into the clip rect, which starts at bounds.x-subtracted 200, giving 100..200).
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    // childIn at local x=150..200: visual x=100..200 — inside the viewport.
    utils.setViewNodeFrame(childIn, 150, 0, 50, 200);
    // childOut at local x=210..260: visual x=220..320 — outside the viewport.
    utils.setViewNodeFrame(childOut, 210, 0, 50, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), childIn);
    container->appendChild(utils.getViewTransactionScope(), childOut);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    // Without scale: clip is local 0..200. childIn visible, childOut not.
    ASSERT_TRUE(childIn->isVisibleInViewport());
    ASSERT_FALSE(childOut->isVisibleInViewport());

    container->setScaleX(2.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    // With scaleX=2: clip rect must be divided by 2 to give local 100..200.
    // childIn (150..200) intersects; childOut (210..260) does not.
    ASSERT_TRUE(childIn->isVisibleInViewport());
    ASSERT_FALSE(childOut->isVisibleInViewport());
}

TEST(ViewNode, positiveScaleYOnContainerCorrectlyClipsChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto childIn = utils.createLayout();
    auto childOut = utils.createLayout();

    // Container: 400 tall (2x viewport). Viewport: 200x200.
    // scaleY=2 expands container to visual height 800, centered at y=200.
    // Visible local range: y=100..200.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(childIn, 0, 150, 200, 50);  // local y=150..200: visual 100..200 ✓
    utils.setViewNodeFrame(childOut, 0, 210, 200, 50); // local y=210..260: visual 220..320 ✗

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), childIn);
    container->appendChild(utils.getViewTransactionScope(), childOut);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(childIn->isVisibleInViewport());
    ASSERT_FALSE(childOut->isVisibleInViewport());

    container->setScaleY(2.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(childIn->isVisibleInViewport());
    ASSERT_FALSE(childOut->isVisibleInViewport());
}

TEST(ViewNode, negativeScaleYOnContainerMovesBottomChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Viewport: 200x200.
    // Container: 200 wide, 400 tall (2x viewport height), at origin.
    // Child: anchored to the bottom of the container (y=350, h=50).
    // Without any transform, the child (y=350..400) is fully below the
    // viewport (0..200) so it should not produce a native view.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(child, 0, 350, 200, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleY=-1 flips the container around its vertical center (y=200).
    // The child at local y=350..400 maps to visual y=0..50 — at the top of
    // the viewport. Both the container and the child must be marked visible
    // so that native views are produced for them.
    container->setScaleY(-1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeScaleXOnContainerMovesRightChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Viewport: 200x200.
    // Container: 400 wide (2x viewport width), 200 tall, at origin.
    // Child: anchored to the right side of the container (x=350, w=50).
    // Without any transform, the child (x=350..400) is fully past the
    // viewport (0..200) so it should not produce a native view.
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    utils.setViewNodeFrame(child, 350, 0, 50, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleX=-1 flips the container around its horizontal center (x=200).
    // The child at local x=350..400 maps to visual x=0..50 — at the left
    // of the viewport. Both the container and the child must be visible.
    container->setScaleX(-1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeScaleXFlipsChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 400 wide, 200 tall. Viewport: 200x200.
    // Child on the left side (x=0..50) — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    utils.setViewNodeFrame(child, 0, 0, 50, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleX=-1: child at local x=0..50 maps to visual x=350..400 — past
    // the viewport edge. It should no longer produce a native view.
    container->setScaleX(-1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeScaleYFlipsChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 200 wide, 400 tall. Viewport: 200x200.
    // Child at the top (y=0..50) — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(child, 0, 0, 200, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleY=-1: child at local y=0..50 maps to visual y=350..400 — below
    // the viewport edge. It should no longer produce a native view.
    container->setScaleY(-1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeScaleXAndScaleYMovesBottomRightChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Viewport: 200x200.
    // Container: 400x400 (2x viewport in both dims), at origin.
    // Child: bottom-right corner (x=350, y=350, 50x50) — fully outside viewport.
    utils.setViewNodeFrame(container, 0, 0, 400, 400);
    utils.setViewNodeFrame(child, 350, 350, 50, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleX=-1, scaleY=-1 flips both axes around the container center (200,200).
    // The child at local (350..400, 350..400) maps to visual (0..50, 0..50) —
    // top-left corner of the viewport.
    container->setScaleX(-1.0f);
    container->setScaleY(-1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeScaleXAndScaleYFlipsTopLeftChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 400x400. Viewport: 200x200.
    // Child at top-left (0,0,50,50) — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 400, 400);
    utils.setViewNodeFrame(child, 0, 0, 50, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleX=-1, scaleY=-1: child at local (0..50, 0..50) maps to visual
    // (350..400, 350..400) — outside the viewport.
    container->setScaleX(-1.0f);
    container->setScaleY(-1.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeHalfScaleXOnContainerMovesRightChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 400 wide (2x viewport), 200 tall. Viewport: 200x200.
    // Child: right side x=300..400 — outside the viewport.
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    utils.setViewNodeFrame(child, 300, 0, 100, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleX=-0.5: flip + shrink. Container visual bounds = x:100..300, half-size.
    // Viewport (0..200) maps to local x:200..400. Child at x=300..400 is inside
    // that range — visual center at x=125, visual frame x=100..150.
    container->setScaleX(-0.5f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeHalfScaleXFlipsLeftChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 400 wide, 200 tall. Viewport: 200x200.
    // Child: left side x=0..100 — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    utils.setViewNodeFrame(child, 0, 0, 100, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleX=-0.5: viewport (0..200) maps to local x:200..400. Child at x=0..100
    // falls outside that range — visual frame is at x=250..300, past the viewport.
    container->setScaleX(-0.5f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeTwoScaleXOnContainerMovesChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 400 wide, 200 tall. Viewport: 200x200.
    // Child: just past the unscaled viewport edge, x=220..270.
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    utils.setViewNodeFrame(child, 220, 0, 50, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleX=-2: flip + double-size. Viewport (0..200) maps to local x:200..300.
    // Child at x=220..270 is inside that range — visual center at x=110,
    // visual frame x=60..160, inside the viewport.
    container->setScaleX(-2.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeTwoScaleXFlipsChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 400 wide, 200 tall. Viewport: 200x200.
    // Child: x=50..100 — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 400, 200);
    utils.setViewNodeFrame(child, 50, 0, 50, 200);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleX=-2: viewport maps to local x:200..300. Child at x=50..100 is outside
    // that range — visual center at x=450, visual frame x=400..500, past the viewport.
    container->setScaleX(-2.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeHalfScaleYOnContainerMovesBottomChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 200 wide, 400 tall (2x viewport). Viewport: 200x200.
    // Child: bottom portion y=300..400 — outside the viewport.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(child, 0, 300, 200, 100);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleY=-0.5: flip + shrink. Container visual bounds = y:100..300, half-size.
    // Viewport (0..200) maps to local y:200..400. Child at y=300..400 is inside
    // that range — visual center at y=125, visual frame y=100..150.
    container->setScaleY(-0.5f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeHalfScaleYFlipsTopChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 200 wide, 400 tall. Viewport: 200x200.
    // Child: top portion y=0..100 — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(child, 0, 0, 200, 100);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleY=-0.5: viewport (0..200) maps to local y:200..400. Child at y=0..100
    // falls outside that range — visual frame is at y=250..300, past the viewport.
    container->setScaleY(-0.5f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeTwoScaleYOnContainerMovesChildIntoViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 200 wide, 400 tall. Viewport: 200x200.
    // Child: just past the unscaled viewport edge, y=220..270.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(child, 0, 220, 200, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());

    // scaleY=-2: flip + double-size. Viewport (0..200) maps to local y:200..300.
    // Child at y=220..270 is inside that range — visual center at y=110,
    // visual frame y=60..160, inside the viewport.
    container->setScaleY(-2.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
}

TEST(ViewNode, negativeTwoScaleYFlipsChildOutOfViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    // Container: 200 wide, 400 tall. Viewport: 200x200.
    // Child: y=50..100 — normally visible.
    utils.setViewNodeFrame(container, 0, 0, 200, 400);
    utils.setViewNodeFrame(child, 0, 50, 200, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(child->isVisibleInViewport());

    // scaleY=-2: viewport maps to local y:200..300. Child at y=50..100 is outside
    // that range — visual center at y=450, visual frame y=400..500, past the viewport.
    container->setScaleY(-2.0f);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(child->isVisibleInViewport());
}

TEST(ViewNode, supportsIgnoreParentViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(child, 75, 75, 50, 50);

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_EQ(Frame(0, 0, 25, 25), child->getCalculatedViewport());

    // Set the ignore parent viewport flag

    child->setIgnoreParentViewport(true);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_EQ(Frame(0, 0, 50, 50), child->getCalculatedViewport());
}

TEST(ViewNode, canCalculateVisibilityUsingChildrenIndexer) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();
    utils.setViewNodeFrame(child, 0, 0, 20, 20);

    root->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_FALSE(root->getChildrenIndexer() != nullptr);

    std::vector<Ref<ViewNode>> children;

    for (size_t i = 0; i < kMaxChildrenBeforeIndexing; i++) {
        auto newChild = utils.createLayout();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i + 1) * 20, 20, 20);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    ASSERT_TRUE(root->isVisibleInViewport());

    for (size_t i = 0; i < 5; i++) {
        ASSERT_TRUE(root->getChildAt(i)->isVisibleInViewport());
    }
    for (size_t i = 5; i < root->getChildCount(); i++) {
        ASSERT_FALSE(root->getChildAt(i)->isVisibleInViewport());
    }
}

TEST(ViewNode, canUseCustomViewport) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto container = utils.createLayout();
    auto child = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();
    auto child4 = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);
    container->appendChild(utils.getViewTransactionScope(), child2);
    container->appendChild(utils.getViewTransactionScope(), child3);
    container->appendChild(utils.getViewTransactionScope(), child4);

    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(child, 0, 0, 50, 50);
    utils.setViewNodeFrame(child2, 50, 0, 50, 50);
    utils.setViewNodeFrame(child3, 50, 50, 50, 50);
    utils.setViewNodeFrame(child4, 0, 50, 50, 50);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_TRUE(child3->isVisibleInViewport());
    ASSERT_TRUE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(0, 0, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_FALSE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());
    ASSERT_FALSE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(25, 0, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());
    ASSERT_FALSE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(50, 0, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());
    ASSERT_FALSE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(50, 25, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_TRUE(child2->isVisibleInViewport());
    ASSERT_TRUE(child3->isVisibleInViewport());
    ASSERT_FALSE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(50, 50, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_FALSE(child2->isVisibleInViewport());
    ASSERT_TRUE(child3->isVisibleInViewport());
    ASSERT_FALSE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(25, 50, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_FALSE(child2->isVisibleInViewport());
    ASSERT_TRUE(child3->isVisibleInViewport());
    ASSERT_TRUE(child4->isVisibleInViewport());

    utils.getTree().setViewport({Frame(0, 50, 50, 50)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->isVisibleInViewport());
    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(child->isVisibleInViewport());
    ASSERT_FALSE(child2->isVisibleInViewport());
    ASSERT_FALSE(child3->isVisibleInViewport());
    ASSERT_TRUE(child4->isVisibleInViewport());
}

TEST(ViewNode, canExtendScrollViewportVertically) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    std::vector<Ref<ViewNode>> children;

    for (size_t i = 0; i < 4; i++) {
        auto newChild = utils.createLayout();

        utils.setViewNodeAttribute(newChild, "width", Value(50.0));
        utils.setViewNodeAttribute(newChild, "height", Value(50.0));
        scrollContainer->appendChild(utils.getViewTransactionScope(), newChild);
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 50));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_FALSE(children[3]->isVisibleInViewport());

    scrollContainer->setViewportExtensionTop(10);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_FALSE(children[3]->isVisibleInViewport());

    scrollContainer->setViewportExtensionBottom(10);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_TRUE(children[3]->isVisibleInViewport());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 40));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_FALSE(children[3]->isVisibleInViewport());

    scrollContainer->setViewportExtensionBottom(20);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_TRUE(children[3]->isVisibleInViewport());
}

TEST(ViewNode, canExtendScrollViewportHorizontally) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "horizontal", Value(true));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    std::vector<Ref<ViewNode>> children;

    for (size_t i = 0; i < 4; i++) {
        auto newChild = utils.createLayout();

        utils.setViewNodeAttribute(newChild, "width", Value(50.0));
        utils.setViewNodeAttribute(newChild, "height", Value(50.0));
        scrollContainer->appendChild(utils.getViewTransactionScope(), newChild);
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(50, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_FALSE(children[3]->isVisibleInViewport());

    scrollContainer->setViewportExtensionLeft(10);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_FALSE(children[3]->isVisibleInViewport());

    scrollContainer->setViewportExtensionRight(10);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_TRUE(children[3]->isVisibleInViewport());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(40, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_FALSE(children[3]->isVisibleInViewport());

    scrollContainer->setViewportExtensionRight(20);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[0]->isVisibleInViewport());
    ASSERT_TRUE(children[1]->isVisibleInViewport());
    ASSERT_TRUE(children[2]->isVisibleInViewport());
    ASSERT_TRUE(children[3]->isVisibleInViewport());
}

TEST(ViewNode, canCalculateVisibilityUsingChildrenIndexerInRTL) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "direction", Value(STRING_LITERAL("rtl")));
    utils.setViewNodeAttribute(scrollContainer, "flexDirection", Value(STRING_LITERAL("row")));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    ASSERT_FALSE(scrollContainer->getChildrenIndexer() != nullptr);

    std::vector<Ref<ViewNode>> children;

    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createLayout();

        utils.setViewNodeAttribute(newChild, "width", Value(20.0));
        utils.setViewNodeAttribute(newChild, "height", Value(20.0));
        scrollContainer->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    // Scroll toward start
    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(-5, 0));

    ASSERT_TRUE(scrollContainer->getChildrenIndexer() != nullptr);
    ASSERT_TRUE(scrollContainer->getChildrenIndexer()->needsUpdate());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(scrollContainer->getChildrenIndexer()->needsUpdate());

    ASSERT_TRUE(scrollContainer->isVisibleInViewport());

    for (size_t i = 0; i < 5; i++) {
        ASSERT_TRUE(scrollContainer->getChildAt(i)->isVisibleInViewport());
    }
    for (size_t i = 5; i < scrollContainer->getChildCount(); i++) {
        ASSERT_FALSE(scrollContainer->getChildAt(i)->isVisibleInViewport());
    }

    // Scroll a bit toward end
    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(40, 0));

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    for (size_t i = 0; i < 2; i++) {
        ASSERT_FALSE(scrollContainer->getChildAt(i)->isVisibleInViewport());
    }
    for (size_t i = 2; i < 7; i++) {
        ASSERT_TRUE(scrollContainer->getChildAt(i)->isVisibleInViewport());
    }
    for (size_t i = 7; i < scrollContainer->getChildCount(); i++) {
        ASSERT_FALSE(scrollContainer->getChildAt(i)->isVisibleInViewport());
    }
}

TEST(ViewNode, canScrollToEquivalentInRtlAndLtr) {
    ViewNodeTestsDependencies utils;

    const char* modeRtl = "rtl";
    const char* modeLtr = "ltr";
    const char* modes[2] = {modeRtl, modeLtr};

    for (int i = 0; i < 2; i++) {
        const char* mode = modes[i];

        auto root = utils.createRootView();
        auto scrollContainer = utils.createScroll();
        utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
        utils.setViewNodeAttribute(scrollContainer, "direction", Value(STRING_LITERAL(mode)));
        utils.setViewNodeAttribute(scrollContainer, "flexDirection", Value(STRING_LITERAL("row")));

        root->appendChild(utils.getViewTransactionScope(), scrollContainer);

        ASSERT_FALSE(scrollContainer->getChildrenIndexer() != nullptr);

        std::vector<Ref<ViewNode>> children;

        for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
            auto newChild = utils.createLayout();

            utils.setViewNodeAttribute(newChild, "width", Value(20.0));
            utils.setViewNodeAttribute(newChild, "height", Value(20.0));
            scrollContainer->appendChild(utils.getViewTransactionScope(), newChild);

            // Put them in an array as we currently don't retain children automatically.
            // The ViewNodeTree is responsible for retaining the nodes.
            children.emplace_back(std::move(newChild));
        }

        root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

        scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(),
                                                Point(10, 0)); // ScrollTo (direction agnostic)

        ASSERT_TRUE(scrollContainer->getChildrenIndexer() != nullptr);
        ASSERT_TRUE(scrollContainer->getChildrenIndexer()->needsUpdate());

        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

        ASSERT_FALSE(scrollContainer->getChildrenIndexer()->needsUpdate());
        ASSERT_TRUE(scrollContainer->isVisibleInViewport());

        for (size_t i = 0; i < 6; i++) {
            ASSERT_TRUE(scrollContainer->getChildAt(i)->isVisibleInViewport());
        }
        for (size_t i = 6; i < scrollContainer->getChildCount(); i++) {
            ASSERT_FALSE(scrollContainer->getChildAt(i)->isVisibleInViewport());
        }
    }
}

TEST(ViewNode, canCheckIfRootViewCanScrollWithNestedScrolls) {
    ViewNodeTestsDependencies utils;

    auto marginRoot = 100;
    auto sizeContent = 100;

    auto root = utils.createRootView();
    auto scrollVertical = utils.createScroll();
    auto scrollHorizontal1 = utils.createScroll();
    auto scrollHorizontal2 = utils.createScroll();

    utils.setViewNodeMarginAndSize(scrollVertical, marginRoot, sizeContent, sizeContent);

    utils.setViewNodeAttribute(scrollHorizontal1, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeMarginAndSize(scrollHorizontal1, 20, 60, 60);

    utils.setViewNodeAttribute(scrollHorizontal2, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeMarginAndSize(scrollHorizontal2, 20, 60, 60);

    scrollVertical->appendChild(utils.getViewTransactionScope(), scrollHorizontal1);
    scrollVertical->appendChild(utils.getViewTransactionScope(), scrollHorizontal2);
    root->appendChild(utils.getViewTransactionScope(), scrollVertical);

    auto count = kMaxChildrenBeforeIndexing + 1;

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < count; i++) {
        auto newChild1 = utils.createLayout();
        utils.setViewNodeMarginAndSize(newChild1, 20, 20, 20);
        scrollHorizontal1->appendChild(utils.getViewTransactionScope(), newChild1);

        auto newChild2 = utils.createLayout();
        utils.setViewNodeMarginAndSize(newChild2, 20, 20, 20);
        scrollHorizontal2->appendChild(utils.getViewTransactionScope(), newChild2);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild1));
        children.emplace_back(std::move(newChild2));
    }

    auto sizeTotal = sizeContent + marginRoot * 2;

    root->performLayout(utils.getViewTransactionScope(), Size(sizeTotal, sizeTotal), LayoutDirectionLTR);

    auto center = sizeContent / 2 + marginRoot;
    auto start = marginRoot + 1;
    auto end = marginRoot + sizeContent - 1;

    Point sZero(0, 0);                  // scroll content offset at origin
    Point sVertMiddle(0, 50);           // scroll content offset so that vertical scrollview is at middle
    Point sVertEnd(0, 100);             // scroll content offset so that vertical scroll is at the end
    Point sHoriEnd(60 * count - 60, 0); // scroll content offset so that horizontal scroll is at the end

    Point locationTop(center, start);     // point at the top of the content
    Point locationCenter(center, center); // point at the middle of the content
    Point locationBottom(center, end);    // point at the bottom of the content

    // When vertical scroll is at the top and horizontal scrolls are at the start
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal1->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal2->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the middle and horizontal scrolls are at the start
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sVertMiddle);
    scrollHorizontal1->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal2->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the end and horizontal scrolls are at the start
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sVertEnd);
    scrollHorizontal1->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal2->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the top and the first horizontal scroll is at the end
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal1->setScrollContentOffset(utils.getViewTransactionScope(), sHoriEnd);
    scrollHorizontal2->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the middle and the first horizontal scroll is at the end
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sVertMiddle);
    scrollHorizontal1->setScrollContentOffset(utils.getViewTransactionScope(), sHoriEnd);
    scrollHorizontal2->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the end and the first horizontal scroll is at the end
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sVertEnd);
    scrollHorizontal1->setScrollContentOffset(utils.getViewTransactionScope(), sHoriEnd);
    scrollHorizontal2->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));
}

TEST(ViewNode, canCheckIfRootViewCanScrollWithAlwaysScrollHorizontal) {
    ViewNodeTestsDependencies utils;

    auto sizeContent = 100;

    auto root = utils.createRootView();
    auto scrollVertical = utils.createScroll();
    auto scrollHorizontal = utils.createScroll();

    utils.setViewNodeMarginAndSize(scrollVertical, 0, sizeContent, sizeContent);

    utils.setViewNodeAttribute(scrollHorizontal, "canAlwaysScrollHorizontal", Value(true));
    utils.setViewNodeAttribute(scrollHorizontal, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeMarginAndSize(scrollHorizontal, 0, sizeContent, sizeContent);

    scrollVertical->appendChild(utils.getViewTransactionScope(), scrollHorizontal);
    root->appendChild(utils.getViewTransactionScope(), scrollVertical);

    auto count = kMaxChildrenBeforeIndexing + 1;

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < count; i++) {
        auto newChild1 = utils.createLayout();
        utils.setViewNodeMarginAndSize(newChild1, 20, 20, 20);
        scrollHorizontal->appendChild(utils.getViewTransactionScope(), newChild1);

        auto newChild2 = utils.createLayout();
        utils.setViewNodeMarginAndSize(newChild2, 20, 20, 20);
        scrollHorizontal->appendChild(utils.getViewTransactionScope(), newChild2);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild1));
        children.emplace_back(std::move(newChild2));
    }

    auto sizeTotal = sizeContent;

    root->performLayout(utils.getViewTransactionScope(), Size(sizeTotal, sizeTotal), LayoutDirectionLTR);

    auto center = sizeContent / 2;
    auto start = 1;
    auto end = sizeContent - 1;

    Point sZero(0, 0);                    // scroll content offset at origin
    Point sVertEnd(0, 100);               // scroll content offset so that vertical scroll is at the end
    Point sHoriEnd(100 * count - 100, 0); // scroll content offset so that horizontal scroll is at the end

    Point locationTop(center, start);     // point at the top of the content
    Point locationCenter(center, center); // point at the middle of the content
    Point locationBottom(center, end);    // point at the bottom of the content

    // When vertical scroll is at the top and horizontal scroll is at the start
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the end and horizontal scroll is at the end
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sVertEnd);
    scrollHorizontal->setScrollContentOffset(utils.getViewTransactionScope(), sHoriEnd);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));
}

TEST(ViewNode, canCheckIfRootViewCanScrollWithAlwaysScrollVertical) {
    ViewNodeTestsDependencies utils;

    auto sizeContent = 100;

    auto root = utils.createRootView();
    auto scrollVertical = utils.createScroll();
    auto scrollHorizontal = utils.createScroll();

    utils.setViewNodeMarginAndSize(scrollVertical, 0, sizeContent, sizeContent);

    utils.setViewNodeAttribute(scrollVertical, "canAlwaysScrollVertical", Value(true));
    utils.setViewNodeAttribute(scrollHorizontal, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeMarginAndSize(scrollHorizontal, 0, sizeContent, sizeContent);

    scrollVertical->appendChild(utils.getViewTransactionScope(), scrollHorizontal);
    root->appendChild(utils.getViewTransactionScope(), scrollVertical);

    auto count = kMaxChildrenBeforeIndexing + 1;

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < count; i++) {
        auto newChild1 = utils.createLayout();
        utils.setViewNodeMarginAndSize(newChild1, 20, 20, 20);
        scrollHorizontal->appendChild(utils.getViewTransactionScope(), newChild1);

        auto newChild2 = utils.createLayout();
        utils.setViewNodeMarginAndSize(newChild2, 20, 20, 20);
        scrollHorizontal->appendChild(utils.getViewTransactionScope(), newChild2);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild1));
        children.emplace_back(std::move(newChild2));
    }

    auto sizeTotal = sizeContent;

    root->performLayout(utils.getViewTransactionScope(), Size(sizeTotal, sizeTotal), LayoutDirectionLTR);

    auto center = sizeContent / 2;
    auto start = 1;
    auto end = sizeContent - 1;

    Point sZero(0, 0);                    // scroll content offset at origin
    Point sVertEnd(0, 100);               // scroll content offset so that vertical scroll is at the end
    Point sHoriEnd(100 * count - 100, 0); // scroll content offset so that horizontal scroll is at the end

    Point locationTop(center, start);     // point at the top of the content
    Point locationCenter(center, center); // point at the middle of the content
    Point locationBottom(center, end);    // point at the bottom of the content

    // When vertical scroll is at the top and horizontal scroll is at the start
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    scrollHorizontal->setScrollContentOffset(utils.getViewTransactionScope(), sZero);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));

    // When vertical scroll is at the end and horizontal scroll is at the end
    scrollVertical->setScrollContentOffset(utils.getViewTransactionScope(), sVertEnd);
    scrollHorizontal->setScrollContentOffset(utils.getViewTransactionScope(), sHoriEnd);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationTop, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationTop, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationCenter, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationCenter, ScrollDirectionRightToLeft));

    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionTopToBottom));
    ASSERT_EQ(true, root->canScroll(locationBottom, ScrollDirectionBottomToTop));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionLeftToRight));
    ASSERT_EQ(false, root->canScroll(locationBottom, ScrollDirectionRightToLeft));
}

TEST(ViewNode, supportsEstimatedSize) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto element1 = utils.createLayout();
    auto element2 = utils.createLayout();
    auto element3 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column")));

    utils.setViewNodeAttribute(element1, "estimatedWidth", Value(100.0));
    utils.setViewNodeAttribute(element1, "estimatedHeight", Value(200.0));
    utils.setViewNodeAttribute(element2, "estimatedWidth", Value(200.0));
    utils.setViewNodeAttribute(element2, "estimatedHeight", Value(300.0));
    utils.setViewNodeAttribute(element3, "estimatedWidth", Value(400.0));
    utils.setViewNodeAttribute(element3, "estimatedHeight", Value(500.0));

    root->appendChild(utils.getViewTransactionScope(), element1);
    root->appendChild(utils.getViewTransactionScope(), element2);
    root->appendChild(utils.getViewTransactionScope(), element3);

    auto size = root->measureLayout(0, MeasureModeUnspecified, 0, MeasureModeUnspecified, LayoutDirectionLTR);

    ASSERT_EQ(400.0f, size.width);
    ASSERT_EQ(1000.0f, size.height);

    // Now we add a child to our last row that is smaller than the estimatedWidth and height

    auto nested = utils.createLayout();

    utils.setViewNodeAttribute(nested, "width", Value(350.0));
    utils.setViewNodeAttribute(nested, "height", Value(450.0));

    element3->appendChild(utils.getViewTransactionScope(), nested);

    size = root->measureLayout(0, MeasureModeUnspecified, 0, MeasureModeUnspecified, LayoutDirectionLTR);

    ASSERT_EQ(350.0f, size.width);
    ASSERT_EQ(950.0f, size.height);
}

TEST(ViewNode, invalidateChildrenIndexerWhenChildrenFrameChanges) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createLayout();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i) * 20, 20, 20);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    utils.setViewNodeFrame(children[5], 0, 5, 20, 20);

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());
}

TEST(ViewNode, invalidateChildrenIndexerWhenParentFrameChanges) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createLayout();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i) * 20, 20, 20);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    root->performLayout(utils.getViewTransactionScope(), Size(90, 90), LayoutDirectionLTR);

    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());
}

TEST(ViewNode, dontInvalidateChildrenIndexerWhenParentFrameDoesntChanges) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createLayout();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i) * 20, 20, 20);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());
}

TEST(ViewNode, invalidateChildrenIndexerWhenNodesAreAddedOrRemoved) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createLayout();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i) * 20, 20, 20);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    children[0]->removeFromParent(utils.getViewTransactionScope());

    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());
    ASSERT_TRUE(root->isFlexLayoutDirty());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    root->insertChildAt(utils.getViewTransactionScope(), children[0], 0);

    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());
}

TEST(ViewNode, invalidateChildrenIndexerWhenTranslateChanges) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createView();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i) * 20, 20, 20);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        // Put them in an array as we currently don't retain children automatically.
        // The ViewNodeTree is responsible for retaining the nodes.
        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    utils.setViewNodeAttribute(children[2], "translationX", Value(20.0));

    ASSERT_TRUE(root->getChildrenIndexer()->needsUpdate());
}

TEST(ViewNode, childrenAreUpdatedWhenTheyConsumeNoSpaceInChildrenIndexer) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    utils.setViewNodeFrame(root, 0, 0, 100, 100);

    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < kMaxChildrenBeforeIndexing + 1; i++) {
        auto newChild = utils.createView();
        utils.setViewNodeFrame(newChild, 0, static_cast<double>(i) * 20, 0, 0);
        root->appendChild(utils.getViewTransactionScope(), newChild);

        children.emplace_back(std::move(newChild));
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->getChildrenIndexer() != nullptr);
    ASSERT_FALSE(root->getChildrenIndexer()->needsUpdate());

    assertAllFlagsAreUpToDate(root.get());

    ASSERT_FALSE(children[5]->isVisibleInViewport());
    ASSERT_FALSE(children[5]->hasView());

    utils.setViewNodeFrame(children[5], 20, 0, 20, 20);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(children[5]->isVisibleInViewport());
    ASSERT_TRUE(children[5]->hasView());

    assertAllFlagsAreUpToDate(root.get());
}

TEST(ViewNode, doesntCreateViewsForLayoutNodes) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto container = utils.createLayout();
    auto marginContainer = utils.createLayout();

    auto view1 = utils.createView();
    auto view2 = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), marginContainer);
    marginContainer->appendChild(utils.getViewTransactionScope(), view1);
    marginContainer->appendChild(utils.getViewTransactionScope(), view2);

    utils.setViewNodeFrame(container, 20, 20, 60, 60);
    utils.setViewNodeFrame(marginContainer, 8, 6, 44, 44);
    utils.setViewNodeFrame(view1, 1, 1, 20, 20);
    utils.setViewNodeFrame(view2, 2, 22, 20, 20);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->getView() != nullptr);
    ASSERT_FALSE(container->getView() != nullptr);
    ASSERT_FALSE(marginContainer->getView() != nullptr);
    ASSERT_TRUE(view1->getView() != nullptr);
    ASSERT_TRUE(view2->getView() != nullptr);

    // The frames provided to platform should be offset
    ASSERT_EQ(Frame(29, 27, 20, 20), StandaloneView::unwrap(view1->getView())->getFrame());
    ASSERT_EQ(Frame(30, 48, 20, 20), StandaloneView::unwrap(view2->getView())->getFrame());
}

TEST(ViewNode, canRemoveViewInChildOfLayoutNode) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto container = utils.createLayout();
    auto subContainer = utils.createLayout();

    auto view = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), subContainer);
    subContainer->appendChild(utils.getViewTransactionScope(), view);

    utils.setViewNodeFrame(container, 0, 0, 20, 20);
    utils.setViewNodeFrame(subContainer, 0, 0, 20, 20);
    utils.setViewNodeFrame(view, 0, 0, 20, 20);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(root->getView() != nullptr);
    ASSERT_FALSE(container->getView() != nullptr);
    ASSERT_FALSE(subContainer->getView() != nullptr);

    ASSERT_TRUE(view->getView() != nullptr);
    ASSERT_FALSE(root->viewTreeNeedsUpdate());

    auto platformRootView = StandaloneView::unwrap(root->getView());
    auto platformView = StandaloneView::unwrap(view->getView());
    ASSERT_TRUE(platformView->getParent() != nullptr);
    ASSERT_EQ(1, static_cast<int>(platformRootView->getChildren().size()));

    container->removeFromParent(utils.getViewTransactionScope());

    ASSERT_FALSE(platformView->getParent() != nullptr);
    ASSERT_EQ(0, static_cast<int>(platformRootView->getChildren().size()));
    ASSERT_TRUE(root->viewTreeNeedsUpdate());

    root->updateViewTree(utils.getViewTransactionScope());
    ASSERT_FALSE(root->viewTreeNeedsUpdate());

    root->appendChild(utils.getViewTransactionScope(), container);

    ASSERT_TRUE(root->viewTreeNeedsUpdate());

    root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_FALSE(root->viewTreeNeedsUpdate());
    ASSERT_TRUE(platformView->getParent() != nullptr);
    ASSERT_EQ(1, static_cast<int>(platformRootView->getChildren().size()));
}

TEST(ViewNode, canHandleLazyLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    ASSERT_EQ(container->getYogaNodeForInsertingChildren(), container->getYogaNode());
    container->setPrefersLazyLayout(utils.getViewTransactionScope(), true);
    ASSERT_NE(container->getYogaNodeForInsertingChildren(), container->getYogaNode());

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_EQ(1, static_cast<int>(container->getChildCount()));
    ASSERT_EQ(0, static_cast<int>(YGNodeGetChildCount(container->getYogaNode())));
    ASSERT_EQ(1, static_cast<int>(YGNodeGetChildCount(container->getYogaNodeForInsertingChildren())));
    ASSERT_EQ(child.get(), container->getChildAt(0));

    utils.setViewNodeFrame(container, 0, 0, 50, 50);
    utils.setViewNodeFrame(child, 16, 16, 16, 16);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_EQ(Frame(0, 0, 50, 50), container->getCalculatedFrame());
    // Lazy layouts are performed after performing visibility updates
    ASSERT_EQ(Frame(0, 0, 0, 0), child->getCalculatedFrame());

    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->isLazyLayoutDirty());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(Frame(0, 0, 50, 50), container->getCalculatedFrame());
    // We should now have a frame
    ASSERT_EQ(Frame(16, 16, 16, 16), child->getCalculatedFrame());
    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->isLazyLayoutDirty());

    // Now updating the child from lazy layout

    utils.setViewNodeFrame(child, 8, 8, 8, 8);

    // The actual yoga node should not be dirty
    ASSERT_FALSE(YGNodeIsDirty(root->getYogaNode()));
    // But we consider the layout to be dirty since we have a dirty lazy layout
    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_TRUE(root->isLazyLayoutDirty());

    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_FALSE(root->isLazyLayoutDirty());

    ASSERT_EQ(Frame(8, 8, 8, 8), child->getCalculatedFrame());
}

// TODO(simon): This test fails because we are not currently able to recover from switching
// from non lazyLayout to lazyLayout after layout attributes have been applied.
TEST(ViewNode, DISABLED_canToggleLazyLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto container = utils.createLayout();
    auto child = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    utils.setViewNodeAttribute(container, "padding", Valdi::Value(10.0));
    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    utils.setViewNodeAttribute(container, "width", Value(STRING_LITERAL("100%")));
    utils.setViewNodeAttribute(container, "height", Value(STRING_LITERAL("100%")));

    container->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(Frame(0, 0, 100, 100), container->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 80, 80), child->getCalculatedFrame());

    container->setPrefersLazyLayout(utils.getViewTransactionScope(), true);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(Frame(0, 0, 100, 100), container->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 80, 80), child->getCalculatedFrame());

    container->setPrefersLazyLayout(utils.getViewTransactionScope(), false);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(Frame(0, 0, 100, 100), container->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 80, 80), child->getCalculatedFrame());
}

TEST(ViewNode, canResolveDirectionUnawareCoordinatesInRTL) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto container = utils.createLayout();

    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child1);
    container->appendChild(utils.getViewTransactionScope(), child2);
    container->appendChild(utils.getViewTransactionScope(), child3);

    utils.setViewNodeAttribute(container, "marginLeft", Value(8.0));
    utils.setViewNodeAttribute(container, "marginRight", Value(3.0));
    utils.setViewNodeAttribute(container, "marginTop", Value(7.0));
    utils.setViewNodeAttribute(container, "height", Value(80.0));
    utils.setViewNodeAttribute(container, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(container, "justifyContent", Value(STRING_LITERAL("space-between")));

    utils.setViewNodeAttribute(child1, "marginLeft", Value(1.0));
    utils.setViewNodeAttribute(child1, "marginTop", Value(4.0));
    utils.setViewNodeAttribute(child1, "width", Value(20.0));
    utils.setViewNodeAttribute(child1, "height", Value(STRING_LITERAL("100%")));

    utils.setViewNodeAttribute(child2, "width", Value(20.0));
    utils.setViewNodeAttribute(child2, "height", Value(STRING_LITERAL("100%")));

    utils.setViewNodeAttribute(child3, "marginRight", Value(3.0));
    utils.setViewNodeAttribute(child3, "width", Value(20.0));
    utils.setViewNodeAttribute(child3, "height", Value(STRING_LITERAL("100%")));

    root->getAttributesApplier().flush(utils.getViewTransactionScope());
    container->getAttributesApplier().flush(utils.getViewTransactionScope());
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());
    child3->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionRTL);

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(8, 7, 89, 80), container->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(1, 4, 20, 80), child1->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(33.5, 0, 20, 80), child2->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(66, 0, 20, 80), child3->getDirectionAgnosticFrame());

    ASSERT_EQ(Point(75.0f, 0), root->convertPointToRelativeDirectionAgnostic(Point(25, 0)));
    ASSERT_EQ(Point(75.0f, 0), root->convertPointToAbsoluteDirectionAgnostic(Point(25, 0)));

    ASSERT_EQ(Point(20.0f, 5), container->convertPointToRelativeDirectionAgnostic(Point(69, 5)));
    ASSERT_EQ(Point(28.0f, 12), container->convertPointToAbsoluteDirectionAgnostic(Point(69, 5)));

    ASSERT_EQ(Point(5.0f, 0), child1->convertPointToRelativeDirectionAgnostic(Point(15, 0)));
    ASSERT_EQ(Point(14.0f, 11.0f), child1->convertPointToAbsoluteDirectionAgnostic(Point(15, 0)));

    ASSERT_EQ(Point(5.0f, 0), child2->convertPointToRelativeDirectionAgnostic(Point(15, 0)));
    ASSERT_EQ(Point(46.5f, 7.0f), child2->convertPointToAbsoluteDirectionAgnostic(Point(15, 0)));

    ASSERT_EQ(Point(5.0f, 0), child3->convertPointToRelativeDirectionAgnostic(Point(15, 0)));
    ASSERT_EQ(Point(79.0f, 7.0f), child3->convertPointToAbsoluteDirectionAgnostic(Point(15, 0)));
}

TEST(ViewNode, canResolveVisualPoints) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto mainContainer = utils.createLayout();
    auto scrollContainer = utils.createScroll();

    utils.setViewNodeFrame(scrollContainer, 2, 0, 120, 100);
    utils.setViewNodeAttribute(scrollContainer, "flexDirection", Value(STRING_LITERAL("column")));
    utils.setViewNodeFrame(mainContainer, 20, 10, 120, 100);

    root->appendChild(utils.getViewTransactionScope(), mainContainer);
    mainContainer->appendChild(utils.getViewTransactionScope(), scrollContainer);

    mainContainer->setTranslationX(5);
    mainContainer->setTranslationY(3);

    std::vector<Ref<ViewNode>> items;
    std::vector<Ref<ViewNode>> nesteds;

    for (size_t i = 0; i < 4; i++) {
        auto item = utils.createLayout();

        utils.setViewNodeAttribute(item, "width", Value(50.0));
        utils.setViewNodeAttribute(item, "height", Value(50.0));

        auto nested = utils.createLayout();
        utils.setViewNodeFrame(nested, 15, 30, 10, 10);

        item->appendChild(utils.getViewTransactionScope(), nested);
        scrollContainer->appendChild(utils.getViewTransactionScope(), item);

        items.emplace_back(item);
        nesteds.emplace_back(nested);
    }

    root->performLayout(utils.getViewTransactionScope(), Size(120, 100), LayoutDirectionLTR);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 40), false);

    const auto& innerChild = nesteds[3];

    auto relativeVisualPoint = innerChild->convertSelfVisualToParentVisual(Point(3, 5));

    ASSERT_EQ(Point(18, 35), relativeVisualPoint);

    auto absoluteVisualPoint = innerChild->convertSelfVisualToRootVisual(Point(3, 5));
    ASSERT_EQ(Point(45, 158), absoluteVisualPoint);

    ASSERT_EQ(Point(2, -40), scrollContainer->convertSelfVisualToParentVisual(Point()));
    ASSERT_EQ(Point(2, 0), scrollContainer->convertSelfVisualToParentVisual(scrollContainer->getBoundsOriginPoint()));
}

TEST(ViewNode, canResolveDirectionUnawareCoordinatesInScrollElement) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 120, 100);
    utils.setViewNodeAttribute(scrollContainer, "flexDirection", Value(STRING_LITERAL("row")));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    std::vector<Ref<ViewNode>> items;

    for (size_t i = 0; i < 4; i++) {
        auto item = utils.createLayout();

        utils.setViewNodeAttribute(item, "width", Value(50.0));
        utils.setViewNodeAttribute(item, "height", Value(50.0));

        scrollContainer->appendChild(utils.getViewTransactionScope(), item);
        items.emplace_back(item);
    }

    root->performLayout(utils.getViewTransactionScope(), Size(120, 100), LayoutDirectionRTL);

    ASSERT_EQ(Frame(0, 0, 50, 50), items[0]->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(50, 0, 50, 50), items[1]->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(100, 0, 50, 50), items[2]->getDirectionAgnosticFrame());
    ASSERT_EQ(Frame(150, 0, 50, 50), items[3]->getDirectionAgnosticFrame());
}

static void assertVisibleInRange(const std::vector<Ref<ViewNode>>& items, size_t minVisible, size_t maxVisible) {
    for (size_t i = 0; i < minVisible; i++) {
        ASSERT_FALSE(items[i]->isVisibleInViewport());
    }

    for (size_t i = minVisible; i <= maxVisible; i++) {
        ASSERT_TRUE(items[i]->isVisibleInViewport());
    }

    for (size_t i = maxVisible + 1; i < items.size(); i++) {
        ASSERT_FALSE(items[i]->isVisibleInViewport());
    }
}

TEST(ViewNode, supportsScrollCircularRatio) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "flexDirection", Value(STRING_LITERAL("row")));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    std::vector<Ref<ViewNode>> items;

    size_t circularRatio = 3;

    for (size_t i = 0; i < circularRatio; i++) {
        for (size_t j = 0; j < 2; j++) {
            auto item = utils.createLayout();
            utils.setViewNodeAttribute(item, "width", Value(50.0));
            utils.setViewNodeAttribute(item, "height", Value(50.0));
            scrollContainer->appendChild(utils.getViewTransactionScope(), item);

            items.emplace_back(item);
        }
    }

    scrollContainer->setScrollCircularRatio(static_cast<int>(3));

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(100, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    assertVisibleInRange(items, 2, 3);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(125, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(125, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    assertVisibleInRange(items, 2, 4);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(150, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(150, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    assertVisibleInRange(items, 3, 4);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(200, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    // Should have teleported back 100
    ASSERT_EQ(Point(100, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    assertVisibleInRange(items, 2, 3);

    // Scroll left

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(50, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(50, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    assertVisibleInRange(items, 1, 2);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(-275, 0));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(125, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    assertVisibleInRange(items, 2, 4);
}

TEST(ViewNode, adjustsContentOffsetWhenContentSizeBecomesTooSmall) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    std::vector<Ref<ViewNode>> items;

    for (size_t i = 0; i < 5; i++) {
        auto item = utils.createLayout();
        utils.setViewNodeAttribute(item, "width", Value(50.0));
        utils.setViewNodeAttribute(item, "height", Value(50.0));
        scrollContainer->appendChild(utils.getViewTransactionScope(), item);

        items.emplace_back(item);
    }

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 100));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(50, scrollContainer->getChildrenSpaceWidth());
    ASSERT_EQ(250, scrollContainer->getChildrenSpaceHeight());
    ASSERT_EQ(Point(0, 100), scrollContainer->getDirectionAgnosticScrollContentOffset());

    // Remove one item
    items[items.size() - 1]->removeFromParent(utils.getViewTransactionScope());
    items.pop_back();

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(50, scrollContainer->getChildrenSpaceWidth());
    ASSERT_EQ(200, scrollContainer->getChildrenSpaceHeight());

    // contentOffset should not have changed
    ASSERT_EQ(Point(0, 100), scrollContainer->getDirectionAgnosticScrollContentOffset());

    // Remove one item
    items[items.size() - 1]->removeFromParent(utils.getViewTransactionScope());
    items.pop_back();

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(50, scrollContainer->getChildrenSpaceWidth());
    ASSERT_EQ(150, scrollContainer->getChildrenSpaceHeight());

    // contentOffset should have been capped to 50 to prevent the new overscroll
    ASSERT_EQ(Point(0, 50), scrollContainer->getDirectionAgnosticScrollContentOffset());

    // We forcefully overscroll by 25 point
    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 75));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(Point(0, 75), scrollContainer->getDirectionAgnosticScrollContentOffset());

    // Remove one item
    items[items.size() - 1]->removeFromParent(utils.getViewTransactionScope());
    items.pop_back();

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(50, scrollContainer->getChildrenSpaceWidth());
    ASSERT_EQ(100, scrollContainer->getChildrenSpaceHeight());

    // overscroll should have been preserved but offset by 50 points
    ASSERT_EQ(Point(0, 25), scrollContainer->getDirectionAgnosticScrollContentOffset());
}

TEST(ViewNode, canScrollOnSameOffsetWithoutTriggeringEventsHorizontal) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "horizontal", Value(true));

    int onScrollCount = 0;
    int onScrollEndCount = 0;

    scrollContainer->setOnScrollCallback(makeShared<ValueFunctionWithCallable>(
        [&onScrollCount](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onScrollCount++;
            return Valdi::Value();
        }));
    scrollContainer->setOnScrollEndCallback(makeShared<ValueFunctionWithCallable>(
        [&onScrollEndCount](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onScrollEndCount++;
            return Valdi::Value();
        }));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(0, onScrollCount); // should not notify as we're already at 0 when initializing the scrollview
    ASSERT_EQ(0, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0), true);
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(0, onScrollCount); // should not notify as we're already at 0 when initializing the scrollview
    ASSERT_EQ(0, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(25, 0));
    ASSERT_EQ(Point(25, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should notify as we actually moved
    ASSERT_EQ(1, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(25, 0), true);
    ASSERT_EQ(Point(25, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should NOT notify as we didn't move (even if we animated)
    ASSERT_EQ(1, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(25, 0), false);
    ASSERT_EQ(Point(25, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should NOT notify as we didn't move (even if we didn't animate)
    ASSERT_EQ(1, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(50, 0));
    ASSERT_EQ(Point(50, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(2, onScrollCount); // We should have moved again
    ASSERT_EQ(2, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(3, onScrollCount); // We should have moved back to start
    ASSERT_EQ(3, onScrollEndCount);
}

TEST(ViewNode, canScrollOnSameOffsetWithoutTriggeringEventsVertical) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "horizontal", Value(false));

    int onScrollCount = 0;
    int onScrollEndCount = 0;

    scrollContainer->setOnScrollCallback(makeShared<ValueFunctionWithCallable>(
        [&onScrollCount](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onScrollCount++;
            return Valdi::Value();
        }));
    scrollContainer->setOnScrollEndCallback(makeShared<ValueFunctionWithCallable>(
        [&onScrollEndCount](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onScrollEndCount++;
            return Valdi::Value();
        }));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(0, onScrollCount); // should not notify as we're already at 0 when initializing the scrollview
    ASSERT_EQ(0, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0), true);
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(0, onScrollCount); // should not notify as we're already at 0 when initializing the scrollview
    ASSERT_EQ(0, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 25));
    ASSERT_EQ(Point(0, 25), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should notify as we actually moved
    ASSERT_EQ(1, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 25), true);
    ASSERT_EQ(Point(0, 25), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should NOT notify as we didn't move (even if we animated)
    ASSERT_EQ(1, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 25), false);
    ASSERT_EQ(Point(0, 25), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should NOT notify as we didn't move (even if we didn't animate)
    ASSERT_EQ(1, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 50));
    ASSERT_EQ(Point(0, 50), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(2, onScrollCount); // We should have moved again
    ASSERT_EQ(2, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(3, onScrollCount); // We should have moved back to start
    ASSERT_EQ(3, onScrollEndCount);
}

TEST(ViewNode, canScrollOnSameOffsetToCancelAnimation) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto scrollContainer = utils.createScroll();
    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "horizontal", Value(false));

    int onScrollCount = 0;
    int onScrollEndCount = 0;

    scrollContainer->setOnScrollCallback(makeShared<ValueFunctionWithCallable>(
        [&onScrollCount](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onScrollCount++;
            return Valdi::Value();
        }));
    scrollContainer->setOnScrollEndCallback(makeShared<ValueFunctionWithCallable>(
        [&onScrollEndCount](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onScrollEndCount++;
            return Valdi::Value();
        }));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(0, onScrollCount); // should not notify as we're already at 0 when initializing the scrollview
    ASSERT_EQ(0, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 25), true);
    ASSERT_EQ(Point(0, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(0, onScrollCount); // animation should have just started so we don't have events yet (next tick only)
    ASSERT_EQ(0, onScrollEndCount);

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 25), false);
    ASSERT_EQ(Point(0, 25), scrollContainer->getDirectionAgnosticScrollContentOffset());
    ASSERT_EQ(1, onScrollCount); // should notify as even tho we moved to same offset we cut the animation
    ASSERT_EQ(1, onScrollEndCount);
}

TEST(ViewNode, handlesLimitToViewportDisabledOnInvisibleParentLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();

    auto container = utils.createLayout();
    auto scroll = utils.createScroll();
    auto scrollChild = utils.createLayout();
    auto view = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), scroll);
    scroll->appendChild(utils.getViewTransactionScope(), scrollChild);
    scrollChild->appendChild(utils.getViewTransactionScope(), view);

    utils.setViewNodeFrame(container, 0, 0, 100, 100);
    utils.setViewNodeFrame(scroll, 0, 0, 100, 100);
    utils.setViewNodeFrame(scrollChild, 50, 50, 10, 10);
    utils.setViewNodeFrame(view, 0, 0, 10, 10);

    view->setLimitToViewport(LimitToViewportDisabled);

    scrollChild->setTranslationX(9999999);
    scroll->setTranslationX(200);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_FALSE(scroll->isVisibleInViewport());
    ASSERT_FALSE(scrollChild->isVisibleInViewport());
    ASSERT_FALSE(view->isVisibleInViewport());

    ASSERT_FALSE(scroll->hasView());
    // the nested view should have a view anyway
    ASSERT_TRUE(view->hasView());
    // The view should not have a parent, since there are no available parents before it
    ASSERT_FALSE(view->isIncludedInViewParent());

    scroll->setTranslationX(0);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_TRUE(container->isVisibleInViewport());
    ASSERT_TRUE(scroll->isVisibleInViewport());
    ASSERT_FALSE(scrollChild->isVisibleInViewport());
    ASSERT_FALSE(view->isVisibleInViewport());

    ASSERT_TRUE(scroll->hasView());
    ASSERT_TRUE(view->hasView());
    ASSERT_TRUE(view->isIncludedInViewParent());
}

TEST(ViewNode, supportsOnMeasureCallback) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto container = utils.createLayout();
    auto child = utils.createView();

    root->appendChild(utils.getViewTransactionScope(), container);
    container->appendChild(utils.getViewTransactionScope(), child);

    ASSERT_FALSE(container->isLazyLayout());

    size_t onMeasureCall = 0;

    container->setOnMeasureCallback(
        utils.getViewTransactionScope(),
        makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext& callContext) -> Valdi::Value {
            onMeasureCall++;
            auto maxWidth = callContext.getParameterAsDouble(0);
            auto maxHeight = callContext.getParameterAsDouble(2);
            return Value(ValueArray::make({Value(maxWidth), Value(std::min(maxHeight, 42.0))}));
        }));

    ASSERT_TRUE(container->isLazyLayout());

    utils.setViewNodeAttribute(container, "padding", Value(static_cast<double>(8)));
    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));

    container->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(static_cast<size_t>(1), onMeasureCall);

    ASSERT_EQ(Frame(0, 0, 100, 100), root->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 0, 100, 42), container->getCalculatedFrame());
    ASSERT_EQ(Frame(8, 8, 84, 26), child->getCalculatedFrame());

    // Passing the same layout specs should not have called the onMeasure

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(static_cast<size_t>(1), onMeasureCall);

    ASSERT_EQ(Frame(0, 0, 100, 42), container->getCalculatedFrame());

    // Changing the onMeasureCallback should cause the layout node to be invalidated

    container->setOnMeasureCallback(
        utils.getViewTransactionScope(),
        makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext& callContext) -> Value {
            onMeasureCall++;
            auto maxWidth = callContext.getParameterAsDouble(0);
            auto maxHeight = callContext.getParameterAsDouble(2);
            return Value(ValueArray::make({Value(maxWidth), Value(std::min(maxHeight, 40.0))}));
        }));

    ASSERT_TRUE(root->isFlexLayoutDirty());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(static_cast<size_t>(2), onMeasureCall);

    ASSERT_EQ(Frame(0, 0, 100, 40), container->getCalculatedFrame());
    ASSERT_EQ(Frame(8, 8, 84, 24), child->getCalculatedFrame());

    // Changing the nested child should not cause a full invalidation
    utils.setViewNodeAttribute(child, "maxHeight", Value(20.0));

    ASSERT_FALSE(root->isFlexLayoutDirty());
    ASSERT_TRUE(root->isLazyLayoutDirty());

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(static_cast<size_t>(2), onMeasureCall);

    ASSERT_EQ(Frame(0, 0, 100, 40), container->getCalculatedFrame());
    ASSERT_EQ(Frame(8, 8, 84, 20), child->getCalculatedFrame());

    // Changing the layout should cause the onMeasure to be called again

    root->performLayout(utils.getViewTransactionScope(), Size(30, 30), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());

    ASSERT_EQ(static_cast<size_t>(3), onMeasureCall);

    ASSERT_EQ(Frame(0, 0, 30, 30), root->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 0, 30, 30), container->getCalculatedFrame());
    ASSERT_EQ(Frame(8, 8, 14, 14), child->getCalculatedFrame());
}

struct UpdateStep {
    Ref<ViewNode> node;
    DummyView expectedView;

    UpdateStep(const Ref<ViewNode>& node, DummyView expectedView) : node(node), expectedView(std::move(expectedView)) {}
};

struct UpdateViewsOnLayoutNodes {
    Ref<ViewNode> root;
    /**/ Ref<ViewNode> containerView;
    /*  */ Ref<ViewNode> container;
    /*    */ Ref<ViewNode> child1Layout;
    /*      */ Ref<ViewNode> child1LayoutView1;
    /*      */ Ref<ViewNode> child1LayoutView2;
    /*    */ Ref<ViewNode> child2Layout;
    /*      */ Ref<ViewNode> child2Layout1;
    /*        */ Ref<ViewNode> child2Layout1View1;
    /*    */ Ref<ViewNode> child3Layout;
    /*      */ Ref<ViewNode> child3LayoutView1;

    /**/ Ref<ViewNode> containerSibling;

    ViewTransactionScope& viewTransactionScope;

    UpdateViewsOnLayoutNodes(ViewNodeTestsDependencies& utils) : viewTransactionScope(utils.getViewTransactionScope()) {
        root = utils.createRootView();
        containerView = utils.createNode("Container");
        container = utils.createLayout();
        child1Layout = utils.createLayout();
        child1LayoutView1 = utils.createNode("Label1");
        child1LayoutView2 = utils.createNode("Label2");
        child2Layout = utils.createLayout();
        child2Layout1 = utils.createLayout();
        child2Layout1View1 = utils.createNode("Nested");
        child3Layout = utils.createLayout();
        child3LayoutView1 = utils.createNode("Label3");

        containerSibling = utils.createNode("Sibling");

        root->appendChild(utils.getViewTransactionScope(), containerView);
        root->appendChild(utils.getViewTransactionScope(), containerSibling);

        containerView->appendChild(utils.getViewTransactionScope(), container);

        container->appendChild(utils.getViewTransactionScope(), child1Layout);
        container->appendChild(utils.getViewTransactionScope(), child2Layout);
        container->appendChild(utils.getViewTransactionScope(), child3Layout);

        child1Layout->appendChild(utils.getViewTransactionScope(), child1LayoutView1);
        child1Layout->appendChild(utils.getViewTransactionScope(), child1LayoutView2);

        child2Layout->appendChild(utils.getViewTransactionScope(), child2Layout1);

        child2Layout1->appendChild(utils.getViewTransactionScope(), child2Layout1View1);

        child3Layout->appendChild(utils.getViewTransactionScope(), child3LayoutView1);
    }

    std::vector<Ref<ViewNode>> allNodes() {
        return {root,
                containerView,
                container,
                child1Layout,
                child1LayoutView1,
                child1LayoutView2,
                child2Layout,
                child2Layout1,
                child2Layout1View1,
                child3Layout,
                child3LayoutView1,
                containerSibling};
    }

    DummyView getRootView() const {
        return getDummyView(root->getView());
    }

    int countViewChildren(ViewNode* viewNode) {
        int viewChildren = 0;
        for (auto* childViewNode : *viewNode) {
            if (childViewNode->isLayout()) {
                viewChildren += countViewChildren(childViewNode);
            } else if (childViewNode->hasView()) {
                viewChildren += 1;
            }
        }
        return viewChildren;
    }

    void checkViewNodeConsistency(ViewNode* viewNode) {
        auto viewChildren = countViewChildren(viewNode);

        ASSERT_EQ(viewChildren, viewNode->getViewChildrenCount());

        for (auto* childViewNode : *viewNode) {
            checkViewNodeConsistency(childViewNode);
        }
    }

    void applyAndCheckUpdateSteps(bool markVisible, std::initializer_list<UpdateStep> updateSteps) {
        for (const auto& step : updateSteps) {
            step.node->setVisibleInViewport(markVisible);
            root->updateViewTree(viewTransactionScope);

            ASSERT_EQ(step.expectedView, getRootView());
            checkViewNodeConsistency(root.get());
        }
    }
};

TEST(ViewNode, canUpdateViewsOnLayoutNodesFromTopToBottom) {
    ViewNodeTestsDependencies utils;
    UpdateViewsOnLayoutNodes nodes(utils);

    // Test construction
    nodes.applyAndCheckUpdateSteps(
        true,
        {
            UpdateStep(nodes.root, DummyView("View")),
            UpdateStep(nodes.containerView, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.container, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1Layout, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1LayoutView1,
                       DummyView("View").addChild(DummyView("Container").addChild(DummyView("Label1")))),
            UpdateStep(nodes.child1LayoutView2,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout1,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout1View1,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested")))),
            UpdateStep(nodes.child3Layout,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested")))),
            UpdateStep(nodes.child3LayoutView1,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested"))
                                                      .addChild(DummyView("Label3")))),
            UpdateStep(nodes.containerSibling,
                       DummyView("View")
                           .addChild(DummyView("Container")
                                         .addChild(DummyView("Label1"))
                                         .addChild(DummyView("Label2"))
                                         .addChild(DummyView("Nested"))
                                         .addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
        });

    // Test destruction
    nodes.applyAndCheckUpdateSteps(
        false,
        {
            UpdateStep(nodes.containerView, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.container, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1Layout, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1LayoutView1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1LayoutView2, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout1View1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child3Layout, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child3LayoutView1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.containerSibling, DummyView("View")),
        });
}

TEST(ViewNode, canUpdateViewsOnLayoutNodesFromBottomToTop) {
    ViewNodeTestsDependencies utils;
    UpdateViewsOnLayoutNodes nodes(utils);

    // Test construction
    nodes.applyAndCheckUpdateSteps(
        true,
        {
            UpdateStep(nodes.root, DummyView("View")),

            UpdateStep(nodes.containerSibling, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child3LayoutView1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child3Layout, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout1View1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1LayoutView2, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1LayoutView1, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1Layout, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.container, DummyView("View").addChild(DummyView("Sibling"))),
            UpdateStep(nodes.containerView,
                       DummyView("View")
                           .addChild(DummyView("Container")
                                         .addChild(DummyView("Label1"))
                                         .addChild(DummyView("Label2"))
                                         .addChild(DummyView("Nested"))
                                         .addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
        });

    // Test destruction
    nodes.applyAndCheckUpdateSteps(
        false,
        {
            UpdateStep(nodes.containerSibling,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested"))
                                                      .addChild(DummyView("Label3")))),
            UpdateStep(nodes.child3LayoutView1,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested")))),
            UpdateStep(nodes.child3Layout,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested")))),
            UpdateStep(nodes.child2Layout1View1,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout1,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child1LayoutView2,
                       DummyView("View").addChild(DummyView("Container").addChild(DummyView("Label1")))),
            UpdateStep(nodes.child1LayoutView1, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1Layout, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.container, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.containerView, DummyView("View")),
        });
}

TEST(ViewNode, canUpdateViewsOnLayoutNodesInRandomOrder) {
    ViewNodeTestsDependencies utils;
    UpdateViewsOnLayoutNodes nodes(utils);

    // Test construction
    nodes.applyAndCheckUpdateSteps(
        true,
        {
            UpdateStep(nodes.root, DummyView("View")),
            UpdateStep(nodes.container, DummyView("View")),
            UpdateStep(nodes.containerView, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1LayoutView2, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1Layout,
                       DummyView("View").addChild(DummyView("Container").addChild(DummyView("Label2")))),
            UpdateStep(nodes.child3Layout,
                       DummyView("View").addChild(DummyView("Container").addChild(DummyView("Label2")))),
            UpdateStep(nodes.child3LayoutView1,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label2")).addChild(DummyView("Label3")))),
            UpdateStep(nodes.child2Layout,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label2")).addChild(DummyView("Label3")))),
            UpdateStep(nodes.child2Layout1View1,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label2")).addChild(DummyView("Label3")))),
            UpdateStep(nodes.child2Layout1,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested"))
                                                      .addChild(DummyView("Label3")))),
            UpdateStep(nodes.containerSibling,
                       DummyView("View")
                           .addChild(DummyView("Container")
                                         .addChild(DummyView("Label2"))
                                         .addChild(DummyView("Nested"))
                                         .addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child1LayoutView1,
                       DummyView("View")
                           .addChild(DummyView("Container")
                                         .addChild(DummyView("Label1"))
                                         .addChild(DummyView("Label2"))
                                         .addChild(DummyView("Nested"))
                                         .addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
        });

    // Test destruction

    // Remove a nested container

    nodes.applyAndCheckUpdateSteps(false,
                                   {
                                       UpdateStep(nodes.child2Layout1,
                                                  DummyView("View")
                                                      .addChild(DummyView("Container")
                                                                    .addChild(DummyView("Label1"))
                                                                    .addChild(DummyView("Label2"))
                                                                    .addChild(DummyView("Label3")))
                                                      .addChild(DummyView("Sibling"))),

                                   });

    // Re-insert it

    nodes.applyAndCheckUpdateSteps(true,
                                   {
                                       UpdateStep(nodes.child2Layout1,
                                                  DummyView("View")
                                                      .addChild(DummyView("Container")
                                                                    .addChild(DummyView("Label1"))
                                                                    .addChild(DummyView("Label2"))
                                                                    .addChild(DummyView("Nested"))
                                                                    .addChild(DummyView("Label3")))
                                                      .addChild(DummyView("Sibling"))),

                                   });

    // Remove some nested views and end with the whole container removed
    nodes.applyAndCheckUpdateSteps(
        false,
        {
            UpdateStep(nodes.child1LayoutView2,
                       DummyView("View")
                           .addChild(DummyView("Container")
                                         .addChild(DummyView("Label1"))
                                         .addChild(DummyView("Nested"))
                                         .addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child2Layout1View1,
                       DummyView("View")
                           .addChild(DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),

            UpdateStep(nodes.child2Layout1View1,
                       DummyView("View")
                           .addChild(DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
            UpdateStep(nodes.child3Layout,
                       DummyView("View")
                           .addChild(DummyView("Container").addChild(DummyView("Label1")))
                           .addChild(DummyView("Sibling"))),

            UpdateStep(nodes.container,
                       DummyView("View").addChild(DummyView("Container")).addChild(DummyView("Sibling"))),
        });

    // Re-insert container
    nodes.applyAndCheckUpdateSteps(true,
                                   {
                                       UpdateStep(nodes.container,
                                                  DummyView("View")
                                                      .addChild(DummyView("Container").addChild(DummyView("Label1")))
                                                      .addChild(DummyView("Sibling"))),
                                   });
}

TEST(ViewNode, canUpdateViewsWithoutVisitingNonDirtyNodes) {
    ViewNodeTestsDependencies utils;
    UpdateViewsOnLayoutNodes nodes(utils);

    nodes.root->setVisibleInViewport(true);

    auto result = nodes.root->updateViewTree(utils.getViewTransactionScope());

    nodes.containerSibling->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_EQ(3, result.visitedNodes);
    ASSERT_EQ(1, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.child1LayoutView1->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_EQ(9, result.visitedNodes);
    ASSERT_EQ(0, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.child1Layout->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());
    ASSERT_EQ(9, result.visitedNodes);
    ASSERT_EQ(0, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.container->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());
    ASSERT_EQ(7, result.visitedNodes);
    ASSERT_EQ(0, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.containerView->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());
    ASSERT_EQ(9, result.visitedNodes);
    ASSERT_EQ(2, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.child2Layout1->setVisibleInViewport(true);
    nodes.child2Layout1View1->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());
    ASSERT_EQ(9, result.visitedNodes);
    ASSERT_EQ(0, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.child2Layout->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());
    ASSERT_EQ(9, result.visitedNodes);
    ASSERT_EQ(1, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.container->setVisibleInViewport(false);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_EQ(11, result.visitedNodes);
    ASSERT_EQ(0, result.createdViews);
    ASSERT_EQ(2, result.destroyedViews);

    nodes.container->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_EQ(11, result.visitedNodes);
    ASSERT_EQ(2, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);

    nodes.child3Layout->setVisibleInViewport(true);
    nodes.child3LayoutView1->setVisibleInViewport(true);
    result = nodes.root->updateViewTree(utils.getViewTransactionScope());

    ASSERT_EQ(8, result.visitedNodes);
    ASSERT_EQ(1, result.createdViews);
    ASSERT_EQ(0, result.destroyedViews);
}

TEST(ViewNode, canRemoveLayoutContainingViews) {
    ViewNodeTestsDependencies utils;
    UpdateViewsOnLayoutNodes nodes(utils);

    nodes.applyAndCheckUpdateSteps(
        true,
        {
            UpdateStep(nodes.root, DummyView("View")),
            UpdateStep(nodes.containerView, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.container, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1Layout, DummyView("View").addChild(DummyView("Container"))),
            UpdateStep(nodes.child1LayoutView1,
                       DummyView("View").addChild(DummyView("Container").addChild(DummyView("Label1")))),
            UpdateStep(nodes.child1LayoutView2,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout1,
                       DummyView("View").addChild(
                           DummyView("Container").addChild(DummyView("Label1")).addChild(DummyView("Label2")))),
            UpdateStep(nodes.child2Layout1View1,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested")))),
            UpdateStep(nodes.child3Layout,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested")))),
            UpdateStep(nodes.child3LayoutView1,
                       DummyView("View").addChild(DummyView("Container")
                                                      .addChild(DummyView("Label1"))
                                                      .addChild(DummyView("Label2"))
                                                      .addChild(DummyView("Nested"))
                                                      .addChild(DummyView("Label3")))),
            UpdateStep(nodes.containerSibling,
                       DummyView("View")
                           .addChild(DummyView("Container")
                                         .addChild(DummyView("Label1"))
                                         .addChild(DummyView("Label2"))
                                         .addChild(DummyView("Nested"))
                                         .addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
        });

    nodes.child1Layout->removeFromParent(utils.getViewTransactionScope());

    nodes.applyAndCheckUpdateSteps(
        true,
        {
            UpdateStep(nodes.child1Layout,
                       DummyView("View")
                           .addChild(DummyView("Container").addChild(DummyView("Nested")).addChild(DummyView("Label3")))
                           .addChild(DummyView("Sibling"))),
        });
}

TEST(ViewNode, canSetStaticScrollContentSize) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createRootView();
    auto scrollContainer = utils.createScroll();

    utils.setViewNodeFrame(scrollContainer, 0, 0, 100, 100);
    utils.setViewNodeAttribute(scrollContainer, "horizontal", Value(true));
    utils.setViewNodeAttribute(scrollContainer, "staticContentWidth", Value(600.0));

    root->appendChild(utils.getViewTransactionScope(), scrollContainer);

    std::vector<Ref<ViewNode>> items;

    int circularRatio = 3;

    for (int i = 0; i < circularRatio; i++) {
        for (int j = 0; j < 2; j++) {
            auto item = utils.createLayout();
            utils.setViewNodeAttribute(item, "width", Value(50.0));
            utils.setViewNodeAttribute(item, "height", Value(50.0));
            scrollContainer->appendChild(utils.getViewTransactionScope(), item);

            items.emplace_back(item);
        }
    }

    scrollContainer->setScrollCircularRatio(circularRatio);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    // Check content size uses the right static values
    ASSERT_EQ(600, scrollContainer->getChildrenSpaceWidth());
    ASSERT_EQ(50, scrollContainer->getChildrenSpaceHeight());

    // Check content offset calculations uses the static content size values
    // See ViewNodeScrollState::applyCircularScroll for calculation logic
    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(200, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(1000, 1000));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(400, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(250, 250));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(250, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(500, 500));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(300, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(25, 25));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(225, 0), scrollContainer->getDirectionAgnosticScrollContentOffset());

    utils.setViewNodeAttribute(scrollContainer, "horizontal", Value(false));
    utils.setViewNodeAttribute(scrollContainer, "staticContentWidth", Value(0.0));
    utils.setViewNodeAttribute(scrollContainer, "staticContentHeight", Value(600.0));

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    // Check content size uses the right static values
    ASSERT_EQ(50, scrollContainer->getChildrenSpaceWidth());
    ASSERT_EQ(600, scrollContainer->getChildrenSpaceHeight());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, 0));
    ASSERT_EQ(Point(0, 200), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(1000, 1000));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(0, 400), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(250, 250));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(0, 250), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(500, 500));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(0, 300), scrollContainer->getDirectionAgnosticScrollContentOffset());

    scrollContainer->setScrollContentOffset(utils.getViewTransactionScope(), Point(25, 25));
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_EQ(Point(0, 225), scrollContainer->getDirectionAgnosticScrollContentOffset());
}

} // namespace ValdiTest
