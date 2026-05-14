#include "ViewNodeTestsUtils.hpp"
#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

// flexShrink: children shrink proportionally when container is too small
TEST(YogaLayout, flexShrinkDistributesSpaceProportionally) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(200.0));
    utils.setViewNodeAttribute(child1, "flexShrink", Value(1.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(200.0));
    utils.setViewNodeAttribute(child2, "flexShrink", Value(3.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionLTR);

    // 200px overflow, shrink ratio 1:3 => child1 shrinks 50, child2 shrinks 150
    ASSERT_EQ(Frame(0, 0, 150, 100), child1->getCalculatedFrame());
    ASSERT_EQ(Frame(150, 0, 50, 100), child2->getCalculatedFrame());
}

TEST(YogaLayout, flexShrinkZeroPreventsShinking) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(80.0));
    utils.setViewNodeAttribute(child1, "flexShrink", Value(0.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(80.0));
    utils.setViewNodeAttribute(child2, "flexShrink", Value(1.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    ASSERT_EQ(80.0f, child1->getCalculatedFrame().width);
    ASSERT_EQ(20.0f, child2->getCalculatedFrame().width);
}

// flexBasis: overrides width/height as the initial main-axis size
TEST(YogaLayout, flexBasisOverridesWidthInRow) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "flexBasis", Value(200.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 100), LayoutDirectionLTR);

    ASSERT_EQ(200.0f, child->getCalculatedFrame().width);
}

TEST(YogaLayout, flexBasisOverridesHeightInColumn) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(300.0));

    utils.setViewNodeAttribute(child, "height", Value(50.0));
    utils.setViewNodeAttribute(child, "flexBasis", Value(200.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 300), LayoutDirectionLTR);

    ASSERT_EQ(200.0f, child->getCalculatedFrame().height);
}

// alignContent: controls how wrapped lines distribute in the cross axis
TEST(YogaLayout, alignContentCenter) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap")));
    utils.setViewNodeAttribute(root, "alignContent", Value(STRING_LITERAL("center")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(60.0));
    utils.setViewNodeAttribute(child1, "height", Value(40.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(60.0));
    utils.setViewNodeAttribute(child2, "height", Value(40.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // Two wrapped lines of 40px each = 80px total. Centered in 200px => 60px offset
    ASSERT_EQ(60.0f, child1->getCalculatedFrame().y);
    ASSERT_EQ(100.0f, child2->getCalculatedFrame().y);
}

TEST(YogaLayout, alignContentSpaceBetween) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap")));
    utils.setViewNodeAttribute(root, "alignContent", Value(STRING_LITERAL("space-between")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(60.0));
    utils.setViewNodeAttribute(child1, "height", Value(40.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(60.0));
    utils.setViewNodeAttribute(child2, "height", Value(40.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // space-between: first line at top, last line at bottom
    ASSERT_EQ(0.0f, child1->getCalculatedFrame().y);
    ASSERT_EQ(160.0f, child2->getCalculatedFrame().y);
}

// alignItems: controls cross-axis alignment of children
TEST(YogaLayout, alignItemsCenter) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "alignItems", Value(STRING_LITERAL("center")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // 50px child centered in 200px cross axis => y=75
    ASSERT_EQ(75.0f, child->getCalculatedFrame().y);
}

TEST(YogaLayout, alignItemsFlexEnd) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "alignItems", Value(STRING_LITERAL("flex-end")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    ASSERT_EQ(150.0f, child->getCalculatedFrame().y);
}

// alignSelf: per-child override of alignItems
TEST(YogaLayout, alignSelfOverridesAlignItems) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "alignItems", Value(STRING_LITERAL("flex-start")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(50.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    utils.setViewNodeAttribute(child2, "alignSelf", Value(STRING_LITERAL("flex-end")));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // child1 follows alignItems (flex-start) => y=0
    ASSERT_EQ(0.0f, child1->getCalculatedFrame().y);
    // child2 overrides with alignSelf (flex-end) => y=150
    ASSERT_EQ(150.0f, child2->getCalculatedFrame().y);
}

// justifyContent: controls main-axis distribution
TEST(YogaLayout, justifyContentCenter) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "justifyContent", Value(STRING_LITERAL("center")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionLTR);

    ASSERT_EQ(75.0f, child->getCalculatedFrame().x);
}

TEST(YogaLayout, justifyContentSpaceBetween) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "justifyContent", Value(STRING_LITERAL("space-between")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(50.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child3, "width", Value(50.0));
    utils.setViewNodeAttribute(child3, "height", Value(50.0));
    child3->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 100), LayoutDirectionLTR);

    // 150px free space, 2 gaps => 75px each
    ASSERT_EQ(0.0f, child1->getCalculatedFrame().x);
    ASSERT_EQ(125.0f, child2->getCalculatedFrame().x);
    ASSERT_EQ(250.0f, child3->getCalculatedFrame().x);
}

TEST(YogaLayout, justifyContentSpaceAround) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "justifyContent", Value(STRING_LITERAL("space-around")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(50.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionLTR);

    // 100px free, 2 children => each gets 25px margin on each side
    ASSERT_EQ(25.0f, child1->getCalculatedFrame().x);
    ASSERT_EQ(125.0f, child2->getCalculatedFrame().x);
}

// display: none hides elements and removes them from layout
TEST(YogaLayout, displayNoneRemovesFromLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(100.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    utils.setViewNodeAttribute(child1, "display", Value(STRING_LITERAL("none")));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(100.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // child1 is display:none => child2 should be at top
    ASSERT_EQ(0.0f, child2->getCalculatedFrame().y);
    ASSERT_EQ(50.0f, child2->getCalculatedFrame().height);
}

// minWidth / minHeight: floor constraints
TEST(YogaLayout, minWidthPreventsShinkingBelowMinimum) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    utils.setViewNodeAttribute(child, "minWidth", Value(80.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    // flexGrow would give 100, minWidth is 80 => should get 100 (min doesn't cap upward)
    ASSERT_EQ(100.0f, child->getCalculatedFrame().width);
}

TEST(YogaLayout, minWidthEnforcedWhenShrinking) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(80.0));
    utils.setViewNodeAttribute(child1, "flexShrink", Value(1.0));
    utils.setViewNodeAttribute(child1, "minWidth", Value(60.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(80.0));
    utils.setViewNodeAttribute(child2, "flexShrink", Value(1.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);

    // child1 can't shrink below 60, so child2 absorbs the remaining overflow
    ASSERT_TRUE(child1->getCalculatedFrame().width >= 60.0f);
}

TEST(YogaLayout, minHeightEnforced) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "width", Value(100.0));
    utils.setViewNodeAttribute(child, "height", Value(10.0));
    utils.setViewNodeAttribute(child, "minHeight", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    ASSERT_EQ(50.0f, child->getCalculatedFrame().height);
}

// borderWidth: affects layout (takes space like padding but on the outside of the content box)
TEST(YogaLayout, borderWidthAffectsChildPosition) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));
    utils.setViewNodeAttribute(root, "borderLeftWidth", Value(10.0));
    utils.setViewNodeAttribute(root, "borderTopWidth", Value(20.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // Border pushes child inward
    ASSERT_EQ(10.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(20.0f, child->getCalculatedFrame().y);
}

TEST(YogaLayout, uniformBorderWidthAffectsChildPosition) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));
    utils.setViewNodeAttribute(root, "borderWidth", Value(15.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    ASSERT_EQ(15.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(15.0f, child->getCalculatedFrame().y);
}

// aspectRatio: enforces width/height proportional relationship
TEST(YogaLayout, aspectRatioSetsWidthFromHeight) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "height", Value(100.0));
    utils.setViewNodeAttribute(child, "aspectRatio", Value(2.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // aspectRatio 2.0 with height 100 => width = 200
    ASSERT_EQ(200.0f, child->getCalculatedFrame().width);
    ASSERT_EQ(100.0f, child->getCalculatedFrame().height);
}

TEST(YogaLayout, aspectRatioSetsHeightFromWidth) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(300.0));

    utils.setViewNodeAttribute(child, "width", Value(100.0));
    utils.setViewNodeAttribute(child, "aspectRatio", Value(0.5));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 300), LayoutDirectionLTR);

    // aspectRatio 0.5 with width 100 => height = 200
    ASSERT_EQ(100.0f, child->getCalculatedFrame().width);
    ASSERT_EQ(200.0f, child->getCalculatedFrame().height);
}

// flexWrap: children wrap to next line when container overflows
TEST(YogaLayout, flexWrapCreatesMultipleLines) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(60.0));
    utils.setViewNodeAttribute(child1, "height", Value(40.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(60.0));
    utils.setViewNodeAttribute(child2, "height", Value(40.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child3, "width", Value(60.0));
    utils.setViewNodeAttribute(child3, "height", Value(40.0));
    child3->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // 100px wide, 60px children => 1 per row, 3 rows
    ASSERT_EQ(0.0f, child1->getCalculatedFrame().y);
    ASSERT_EQ(40.0f, child2->getCalculatedFrame().y);
    ASSERT_EQ(80.0f, child3->getCalculatedFrame().y);
}

TEST(YogaLayout, flexWrapReverseReversesLineOrder) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap-reverse")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(60.0));
    utils.setViewNodeAttribute(child1, "height", Value(40.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(60.0));
    utils.setViewNodeAttribute(child2, "height", Value(40.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // wrap-reverse: lines stack from bottom to top
    ASSERT_TRUE(child1->getCalculatedFrame().y > child2->getCalculatedFrame().y);
}

// Combined: flexGrow + flexShrink + flexBasis interaction
TEST(YogaLayout, flexGrowDistributesRemainingSpace) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "flexBasis", Value(50.0));
    utils.setViewNodeAttribute(child1, "flexGrow", Value(1.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "flexBasis", Value(50.0));
    utils.setViewNodeAttribute(child2, "flexGrow", Value(3.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 100), LayoutDirectionLTR);

    // 200px free space, ratio 1:3 => child1=50+50=100, child2=50+150=200
    ASSERT_EQ(100.0f, child1->getCalculatedFrame().width);
    ASSERT_EQ(200.0f, child2->getCalculatedFrame().width);
}

// Percentage-based sizing
TEST(YogaLayout, percentageWidth) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child, "width", Value(STRING_LITERAL("50%")));
    utils.setViewNodeAttribute(child, "height", Value(STRING_LITERAL("100%")));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionLTR);

    ASSERT_EQ(100.0f, child->getCalculatedFrame().width);
    ASSERT_EQ(100.0f, child->getCalculatedFrame().height);
}

// RTL direction
TEST(YogaLayout, rtlDirectionReversesRowLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(50.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionRTL);

    // RTL: child1 starts from right edge
    ASSERT_EQ(150.0f, child1->getCalculatedFrame().x);
    ASSERT_EQ(100.0f, child2->getCalculatedFrame().x);
}

// Absolute positioning
TEST(YogaLayout, absolutePositionIgnoresFlexLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "position", Value(STRING_LITERAL("absolute")));
    utils.setViewNodeAttribute(child1, "left", Value(10.0));
    utils.setViewNodeAttribute(child1, "top", Value(10.0));
    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(100.0));
    utils.setViewNodeAttribute(child2, "height", Value(100.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    ASSERT_EQ(Frame(10, 10, 50, 50), child1->getCalculatedFrame());
    // child2 should be at origin since child1 is absolute (removed from flow)
    ASSERT_EQ(0.0f, child2->getCalculatedFrame().x);
    ASSERT_EQ(0.0f, child2->getCalculatedFrame().y);
}

// Nested flex containers
TEST(YogaLayout, nestedFlexContainers) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto row = utils.createLayout();
    auto cell1 = utils.createLayout();
    auto cell2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(300.0));

    utils.setViewNodeAttribute(row, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(row, "height", Value(100.0));

    utils.setViewNodeAttribute(cell1, "flexGrow", Value(1.0));
    cell1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(cell2, "flexGrow", Value(2.0));
    cell2->getAttributesApplier().flush(utils.getViewTransactionScope());

    row->appendChild(utils.getViewTransactionScope(), cell1);
    row->appendChild(utils.getViewTransactionScope(), cell2);
    root->appendChild(utils.getViewTransactionScope(), row);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 300), LayoutDirectionLTR);

    ASSERT_EQ(Frame(0, 0, 300, 100), row->getCalculatedFrame());
    ASSERT_EQ(100.0f, cell1->getCalculatedFrame().width);
    ASSERT_EQ(200.0f, cell2->getCalculatedFrame().width);
}

// maxWidth / maxHeight cap growth
TEST(YogaLayout, maxWidthCapsFlexGrow) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    utils.setViewNodeAttribute(child, "maxWidth", Value(150.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 100), LayoutDirectionLTR);

    ASSERT_EQ(150.0f, child->getCalculatedFrame().width);
}

TEST(YogaLayout, maxHeightCapsFlexGrow) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(300.0));

    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    utils.setViewNodeAttribute(child, "maxHeight", Value(150.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 300), LayoutDirectionLTR);

    ASSERT_EQ(150.0f, child->getCalculatedFrame().height);
}

// Padding interaction with children
TEST(YogaLayout, paddingReducesAvailableSpaceForChildren) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));
    utils.setViewNodeAttribute(root, "paddingLeft", Value(20.0));
    utils.setViewNodeAttribute(root, "paddingTop", Value(30.0));
    utils.setViewNodeAttribute(root, "paddingRight", Value(20.0));
    root->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    ASSERT_EQ(20.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(30.0f, child->getCalculatedFrame().y);
    ASSERT_EQ(160.0f, child->getCalculatedFrame().width);
}

// Combined wrap + alignContent + justifyContent
TEST(YogaLayout, wrapWithJustifyContentAndAlignContent) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap")));
    utils.setViewNodeAttribute(root, "justifyContent", Value(STRING_LITERAL("center")));
    utils.setViewNodeAttribute(root, "alignContent", Value(STRING_LITERAL("flex-end")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(40.0));
    utils.setViewNodeAttribute(child1, "height", Value(30.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(40.0));
    utils.setViewNodeAttribute(child2, "height", Value(30.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child3, "width", Value(40.0));
    utils.setViewNodeAttribute(child3, "height", Value(30.0));
    child3->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // Row 1: child1 + child2 (80px <= 100px, fit), row 2: child3
    // Total height: 60px. alignContent flex-end => offset = 200 - 60 = 140
    ASSERT_EQ(140.0f, child1->getCalculatedFrame().y);
    ASSERT_EQ(140.0f, child2->getCalculatedFrame().y);
    ASSERT_EQ(170.0f, child3->getCalculatedFrame().y);

    // justifyContent center within each row
    // Row 1: 80px content, 20px free => 10px offset
    ASSERT_EQ(10.0f, child1->getCalculatedFrame().x);
    // Row 2: 40px content, 60px free => 30px offset
    ASSERT_EQ(30.0f, child3->getCalculatedFrame().x);
}

// row-reverse: children laid out right-to-left
TEST(YogaLayout, rowReverse) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row-reverse")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(50.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionLTR);

    // row-reverse: child1 starts from right edge
    ASSERT_EQ(150.0f, child1->getCalculatedFrame().x);
    ASSERT_EQ(100.0f, child2->getCalculatedFrame().x);
}

// column-reverse: children laid out bottom-to-top
TEST(YogaLayout, columnReverse) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column-reverse")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(100.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(100.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // column-reverse: child1 at bottom, child2 above
    ASSERT_EQ(150.0f, child1->getCalculatedFrame().y);
    ASSERT_EQ(100.0f, child2->getCalculatedFrame().y);
}

// row-reverse with wrap: lines still stack top-to-bottom, items reversed within each line
TEST(YogaLayout, rowReverseWithWrap) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();
    auto child3 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row-reverse")));
    utils.setViewNodeAttribute(root, "flexWrap", Value(STRING_LITERAL("wrap")));
    utils.setViewNodeAttribute(root, "width", Value(100.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child1, "width", Value(60.0));
    utils.setViewNodeAttribute(child1, "height", Value(40.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(60.0));
    utils.setViewNodeAttribute(child2, "height", Value(40.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child3, "width", Value(60.0));
    utils.setViewNodeAttribute(child3, "height", Value(40.0));
    child3->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);
    root->appendChild(utils.getViewTransactionScope(), child3);

    root->performLayout(utils.getViewTransactionScope(), Size(100, 200), LayoutDirectionLTR);

    // row-reverse: child1 starts from right (x=40), wraps to new lines
    ASSERT_EQ(40.0f, child1->getCalculatedFrame().x);
    ASSERT_EQ(0.0f, child1->getCalculatedFrame().y);
    // child2 wraps to second line
    ASSERT_EQ(40.0f, child2->getCalculatedFrame().x);
    ASSERT_EQ(40.0f, child2->getCalculatedFrame().y);
}

// right/bottom absolute position edges
TEST(YogaLayout, absolutePositionRightBottom) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "position", Value(STRING_LITERAL("absolute")));
    utils.setViewNodeAttribute(child, "right", Value(10.0));
    utils.setViewNodeAttribute(child, "bottom", Value(20.0));
    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // right=10 => x = 200 - 50 - 10 = 140
    // bottom=20 => y = 200 - 50 - 20 = 130
    ASSERT_EQ(140.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(130.0f, child->getCalculatedFrame().y);
}

// Absolute position with both left+right (stretches width)
TEST(YogaLayout, absolutePositionLeftRightStretches) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "position", Value(STRING_LITERAL("absolute")));
    utils.setViewNodeAttribute(child, "left", Value(20.0));
    utils.setViewNodeAttribute(child, "right", Value(30.0));
    utils.setViewNodeAttribute(child, "top", Value(10.0));
    utils.setViewNodeAttribute(child, "bottom", Value(10.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // left=20, right=30 => width = 200 - 20 - 30 = 150
    // top=10, bottom=10 => height = 200 - 10 - 10 = 180
    ASSERT_EQ(Frame(20, 10, 150, 180), child->getCalculatedFrame());
}

// Percentage-based margin
TEST(YogaLayout, percentageMargin) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("column")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "width", Value(100.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    utils.setViewNodeAttribute(child, "marginLeft", Value(STRING_LITERAL("10%")));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // 10% of 200 = 20
    ASSERT_EQ(20.0f, child->getCalculatedFrame().x);
}

// Percentage-based padding
TEST(YogaLayout, percentagePadding) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));
    utils.setViewNodeAttribute(root, "paddingLeft", Value(STRING_LITERAL("10%")));
    utils.setViewNodeAttribute(root, "paddingTop", Value(STRING_LITERAL("5%")));
    root->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // paddingLeft 10% of 200 = 20, paddingTop 5% of 200 = 10
    ASSERT_EQ(20.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(10.0f, child->getCalculatedFrame().y);
}

// Percentage-based absolute position
TEST(YogaLayout, percentageAbsolutePosition) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(400.0));
    utils.setViewNodeAttribute(root, "height", Value(400.0));

    utils.setViewNodeAttribute(child, "position", Value(STRING_LITERAL("absolute")));
    utils.setViewNodeAttribute(child, "left", Value(STRING_LITERAL("25%")));
    utils.setViewNodeAttribute(child, "top", Value(STRING_LITERAL("10%")));
    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(400, 400), LayoutDirectionLTR);

    // left 25% of 400 = 100, top 10% of 400 = 40
    ASSERT_EQ(100.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(40.0f, child->getCalculatedFrame().y);
}

// Negative absolute position (offscreen)
TEST(YogaLayout, negativeAbsolutePosition) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));

    utils.setViewNodeAttribute(child, "position", Value(STRING_LITERAL("absolute")));
    utils.setViewNodeAttribute(child, "left", Value(-20.0));
    utils.setViewNodeAttribute(child, "top", Value(-10.0));
    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    ASSERT_EQ(-20.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(-10.0f, child->getCalculatedFrame().y);
}

// flexBasis with percentage
TEST(YogaLayout, flexBasisPercentage) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "flexBasis", Value(STRING_LITERAL("33.33%")));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "flexBasis", Value(STRING_LITERAL("66.67%")));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 100), LayoutDirectionLTR);

    // ~1/3 and ~2/3 of 300
    ASSERT_NEAR(100.0f, child1->getCalculatedFrame().width, 1.0f);
    ASSERT_NEAR(200.0f, child2->getCalculatedFrame().width, 1.0f);
}

// borderRightWidth and borderBottomWidth
TEST(YogaLayout, allBorderEdgesAffectLayout) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(200.0));
    utils.setViewNodeAttribute(root, "borderLeftWidth", Value(10.0));
    utils.setViewNodeAttribute(root, "borderTopWidth", Value(20.0));
    utils.setViewNodeAttribute(root, "borderRightWidth", Value(30.0));
    utils.setViewNodeAttribute(root, "borderBottomWidth", Value(40.0));

    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 200), LayoutDirectionLTR);

    // Child offset by borders, size reduced by borders on all sides
    ASSERT_EQ(10.0f, child->getCalculatedFrame().x);
    ASSERT_EQ(20.0f, child->getCalculatedFrame().y);
    // width: 200 - 10 - 30 = 160, height: 200 - 20 - 40 = 140
    ASSERT_EQ(160.0f, child->getCalculatedFrame().width);
    ASSERT_EQ(140.0f, child->getCalculatedFrame().height);
}

// direction attribute on individual nodes
TEST(YogaLayout, directionAttributeRTL) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child1 = utils.createLayout();
    auto child2 = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "direction", Value(STRING_LITERAL("rtl")));
    utils.setViewNodeAttribute(root, "width", Value(200.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child1, "width", Value(50.0));
    utils.setViewNodeAttribute(child1, "height", Value(50.0));
    child1->getAttributesApplier().flush(utils.getViewTransactionScope());

    utils.setViewNodeAttribute(child2, "width", Value(50.0));
    utils.setViewNodeAttribute(child2, "height", Value(50.0));
    child2->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child1);
    root->appendChild(utils.getViewTransactionScope(), child2);

    root->performLayout(utils.getViewTransactionScope(), Size(200, 100), LayoutDirectionLTR);

    // direction=rtl on the node: child1 should be on the right
    ASSERT_TRUE(child1->getCalculatedFrame().x > child2->getCalculatedFrame().x);
}

// aspectRatio combined with flexGrow
TEST(YogaLayout, aspectRatioWithFlexGrow) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "flexDirection", Value(STRING_LITERAL("row")));
    utils.setViewNodeAttribute(root, "width", Value(300.0));
    utils.setViewNodeAttribute(root, "height", Value(100.0));

    utils.setViewNodeAttribute(child, "flexGrow", Value(1.0));
    utils.setViewNodeAttribute(child, "aspectRatio", Value(2.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(300, 100), LayoutDirectionLTR);

    // flexGrow=1 in row gives width=300, aspectRatio=2 => height=150
    // but container height is 100 — aspect ratio should still be respected
    float w = child->getCalculatedFrame().width;
    float h = child->getCalculatedFrame().height;
    ASSERT_NEAR(2.0f, w / h, 0.01f);
}

// Zero-size container edge case
TEST(YogaLayout, zeroSizeContainer) {
    ViewNodeTestsDependencies utils;

    auto root = utils.createLayout();
    auto child = utils.createLayout();

    utils.setViewNodeAttribute(root, "width", Value(0.0));
    utils.setViewNodeAttribute(root, "height", Value(0.0));

    utils.setViewNodeAttribute(child, "width", Value(50.0));
    utils.setViewNodeAttribute(child, "height", Value(50.0));
    child->getAttributesApplier().flush(utils.getViewTransactionScope());

    root->appendChild(utils.getViewTransactionScope(), child);

    root->performLayout(utils.getViewTransactionScope(), Size(0, 0), LayoutDirectionLTR);

    // Child should still have its explicit size
    ASSERT_EQ(50.0f, child->getCalculatedFrame().width);
    ASSERT_EQ(50.0f, child->getCalculatedFrame().height);
}

} // namespace ValdiTest
