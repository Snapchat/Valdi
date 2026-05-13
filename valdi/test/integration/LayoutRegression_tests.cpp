#include "JSBridgeTestFixture.hpp"
#include "RuntimeTestsUtils.hpp"
#include "valdi_test_utils.hpp"
#include "gtest/gtest.h"

using namespace Valdi;

namespace ValdiTest {

class LayoutRegressionFixture : public JSBridgeTestFixture {
protected:
    void SetUp() override {
        auto* jsBridge = getJsBridge();
        wrapper = RuntimeWrapper(jsBridge, isWithTSN() ? TSNMode::Enabled : TSNMode::Disabled);
        wrapper.runtimeListener->layoutTreeAutomatically = false;
    }

    void TearDown() override {
        wrapper.teardown();
    }

    SharedViewNodeTree createTsxTree(const char* className) {
        auto path = STRING_FORMAT("{}@test/src/{}", className, className);
        auto tree = wrapper.createViewNodeTreeAndContext(path, Value::undefined(), Value::undefined());
        // Layout tests only verify Yoga-calculated frames — view inflation is not needed
        // and triggers a std::bad_alloc on Linux where the standalone view pipeline has
        // never been exercised.
        tree->setViewInflationEnabled(false);
        return tree;
    }

    SharedViewNodeTree createTsxTree(const char* className, const Value& viewModel) {
        auto path = STRING_FORMAT("{}@test/src/{}", className, className);
        auto tree = wrapper.createViewNodeTreeAndContext(path, viewModel, Value::undefined());
        tree->setViewInflationEnabled(false);
        return tree;
    }

    RuntimeWrapper wrapper;
};

// Header / Content / Footer — classic app layout
TEST_P(LayoutRegressionFixture, headerContentFooter) {
    auto tree = createTsxTree("LayoutHeaderContentFooter");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(300.0f, 500.0f), LayoutDirectionLTR);

    auto header = tree->getViewNodeForNodePath(parseNodePath("header"));
    auto content = tree->getViewNodeForNodePath(parseNodePath("content"));
    auto footer = tree->getViewNodeForNodePath(parseNodePath("footer"));

    ASSERT_NE(nullptr, header);
    ASSERT_NE(nullptr, content);
    ASSERT_NE(nullptr, footer);

    ASSERT_EQ(Frame(0, 0, 300, 60), header->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 60, 300, 400), content->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 460, 300, 40), footer->getCalculatedFrame());
}

TEST_P(LayoutRegressionFixture, headerContentFooterResizes) {
    auto tree = createTsxTree("LayoutHeaderContentFooter");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(300.0f, 500.0f), LayoutDirectionLTR);

    auto content = tree->getViewNodeForNodePath(parseNodePath("content"));
    ASSERT_EQ(400.0f, content->getCalculatedFrame().height);

    tree->setLayoutSpecs(Size(300.0f, 200.0f), LayoutDirectionLTR);
    ASSERT_EQ(100.0f, content->getCalculatedFrame().height);
}

// Card grid — wrapped row of fixed-size items
TEST_P(LayoutRegressionFixture, cardGridWrapsCorrectly) {
    auto tree = createTsxTree("LayoutCardGrid");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(200.0f, 400.0f), LayoutDirectionLTR);

    auto card1 = tree->getViewNodeForNodePath(parseNodePath("card1"));
    auto card2 = tree->getViewNodeForNodePath(parseNodePath("card2"));
    auto card3 = tree->getViewNodeForNodePath(parseNodePath("card3"));
    auto card4 = tree->getViewNodeForNodePath(parseNodePath("card4"));

    // 200px wide, 90px cards => 2 per row
    ASSERT_EQ(Frame(0, 0, 90, 80), card1->getCalculatedFrame());
    ASSERT_EQ(Frame(90, 0, 90, 80), card2->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 80, 90, 80), card3->getCalculatedFrame());
    ASSERT_EQ(Frame(90, 80, 90, 80), card4->getCalculatedFrame());
}

TEST_P(LayoutRegressionFixture, cardGridReflowsOnResize) {
    auto tree = createTsxTree("LayoutCardGrid");
    wrapper.waitUntilAllUpdatesCompleted();

    // Wide enough for all 4 in one row
    tree->setLayoutSpecs(Size(400.0f, 400.0f), LayoutDirectionLTR);

    auto card1 = tree->getViewNodeForNodePath(parseNodePath("card1"));
    auto card4 = tree->getViewNodeForNodePath(parseNodePath("card4"));

    ASSERT_EQ(0.0f, card1->getCalculatedFrame().y);
    ASSERT_EQ(0.0f, card4->getCalculatedFrame().y);

    // Narrow: only 1 per row
    tree->setLayoutSpecs(Size(100.0f, 400.0f), LayoutDirectionLTR);

    ASSERT_EQ(0.0f, card1->getCalculatedFrame().y);
    ASSERT_EQ(240.0f, card4->getCalculatedFrame().y);
}

// Sidebar layout — fixed sidebar + flexible main
TEST_P(LayoutRegressionFixture, sidebarLayout) {
    auto tree = createTsxTree("LayoutSidebar");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(400.0f, 300.0f), LayoutDirectionLTR);

    auto sidebar = tree->getViewNodeForNodePath(parseNodePath("sidebar"));
    auto main = tree->getViewNodeForNodePath(parseNodePath("main"));

    ASSERT_EQ(Frame(0, 0, 100, 300), sidebar->getCalculatedFrame());
    ASSERT_EQ(Frame(100, 0, 300, 300), main->getCalculatedFrame());
}

TEST_P(LayoutRegressionFixture, sidebarLayoutRTL) {
    auto tree = createTsxTree("LayoutSidebar");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(400.0f, 300.0f), LayoutDirectionRTL);

    auto sidebar = tree->getViewNodeForNodePath(parseNodePath("sidebar"));
    auto main = tree->getViewNodeForNodePath(parseNodePath("main"));

    // RTL: sidebar on the right
    ASSERT_EQ(300.0f, sidebar->getCalculatedFrame().x);
    ASSERT_EQ(0.0f, main->getCalculatedFrame().x);
    ASSERT_EQ(300.0f, main->getCalculatedFrame().width);
}

// Nested flex containers — column > row + column > items
TEST_P(LayoutRegressionFixture, nestedFlexContainers) {
    auto tree = createTsxTree("LayoutNested");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(300.0f, 400.0f), LayoutDirectionLTR);

    auto topRow = tree->getViewNodeForNodePath(parseNodePath("topRow"));
    auto left = tree->getViewNodeForNodePath(parseNodePath("left"));
    auto right = tree->getViewNodeForNodePath(parseNodePath("right"));
    auto bottomCol = tree->getViewNodeForNodePath(parseNodePath("bottomCol"));
    auto topItem = tree->getViewNodeForNodePath(parseNodePath("topItem"));
    auto bottomItem = tree->getViewNodeForNodePath(parseNodePath("bottomItem"));

    // Top row: fixed 100px height, full width
    ASSERT_EQ(Frame(0, 0, 300, 100), topRow->getCalculatedFrame());

    // Left/right split the row equally
    ASSERT_EQ(150.0f, left->getCalculatedFrame().width);
    ASSERT_EQ(150.0f, right->getCalculatedFrame().width);
    ASSERT_EQ(0.0f, left->getCalculatedFrame().x);
    ASSERT_EQ(150.0f, right->getCalculatedFrame().x);

    // Bottom column: flexGrow=1 gets remaining 300px
    ASSERT_EQ(Frame(0, 100, 300, 300), bottomCol->getCalculatedFrame());

    // Items inside bottom column
    ASSERT_EQ(100.0f, topItem->getCalculatedFrame().height);
    ASSERT_EQ(200.0f, bottomItem->getCalculatedFrame().height);
    ASSERT_EQ(0.0f, topItem->getCalculatedFrame().y);
    ASSERT_EQ(100.0f, bottomItem->getCalculatedFrame().y);
}

// Dynamic relayout — add/remove children via viewModel
TEST_P(LayoutRegressionFixture, dynamicRelayoutAllVisible) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("showItem1")] = Value(true);
    (*viewModel)[STRING_LITERAL("showItem2")] = Value(true);

    auto tree = createTsxTree("LayoutDynamic", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(200.0f, 300.0f), LayoutDirectionLTR);

    auto header = tree->getViewNodeForNodePath(parseNodePath("header"));
    auto item1 = tree->getViewNodeForNodePath(parseNodePath("item1"));
    auto item2 = tree->getViewNodeForNodePath(parseNodePath("item2"));
    auto footer = tree->getViewNodeForNodePath(parseNodePath("footer"));

    ASSERT_EQ(Frame(0, 0, 200, 50), header->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 50, 200, 60), item1->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 110, 200, 60), item2->getCalculatedFrame());
    // footer gets remaining: 300 - 50 - 60 - 60 = 130
    ASSERT_EQ(Frame(0, 170, 200, 130), footer->getCalculatedFrame());
}

TEST_P(LayoutRegressionFixture, dynamicRelayoutHideItem) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("showItem1")] = Value(true);
    (*viewModel)[STRING_LITERAL("showItem2")] = Value(true);

    auto tree = createTsxTree("LayoutDynamic", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(200.0f, 300.0f), LayoutDirectionLTR);

    auto footer = tree->getViewNodeForNodePath(parseNodePath("footer"));
    ASSERT_EQ(130.0f, footer->getCalculatedFrame().height);

    // Hide item1 — footer should grow
    (*viewModel)[STRING_LITERAL("showItem1")] = Value(false);
    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(200.0f, 300.0f), LayoutDirectionLTR);

    auto item2 = tree->getViewNodeForNodePath(parseNodePath("item2"));
    footer = tree->getViewNodeForNodePath(parseNodePath("footer"));

    // item2 moves up to y=50 (right after header)
    ASSERT_EQ(50.0f, item2->getCalculatedFrame().y);
    // footer gets: 300 - 50 - 60 = 190
    ASSERT_EQ(190.0f, footer->getCalculatedFrame().height);
}

// flexGrow + flexShrink + flexBasis + maxWidth interaction
TEST_P(LayoutRegressionFixture, flexPropertyInteraction) {
    auto tree = createTsxTree("LayoutFlexInteraction");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(400.0f, 100.0f), LayoutDirectionLTR);

    auto fixed = tree->getViewNodeForNodePath(parseNodePath("fixed"));
    auto flexible = tree->getViewNodeForNodePath(parseNodePath("flexible"));
    auto capped = tree->getViewNodeForNodePath(parseNodePath("capped"));

    // fixed: width=80, flexShrink=0 => always 80
    ASSERT_EQ(80.0f, fixed->getCalculatedFrame().width);

    // capped: maxWidth=120, so it can't grow beyond 120
    ASSERT_TRUE(capped->getCalculatedFrame().width <= 120.0f);

    // All three should fill the 400px width
    float totalWidth =
        fixed->getCalculatedFrame().width + flexible->getCalculatedFrame().width + capped->getCalculatedFrame().width;
    ASSERT_FLOAT_EQ(400.0f, totalWidth);
}

// row-reverse: children laid out right-to-left through full pipeline
TEST_P(LayoutRegressionFixture, rowReverse) {
    auto tree = createTsxTree("LayoutRowReverse");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(300.0f, 100.0f), LayoutDirectionLTR);

    auto first = tree->getViewNodeForNodePath(parseNodePath("first"));
    auto second = tree->getViewNodeForNodePath(parseNodePath("second"));
    auto third = tree->getViewNodeForNodePath(parseNodePath("third"));

    // row-reverse: first child at right edge, last at left
    ASSERT_EQ(240.0f, first->getCalculatedFrame().x);
    ASSERT_EQ(180.0f, second->getCalculatedFrame().x);
    ASSERT_EQ(120.0f, third->getCalculatedFrame().x);
}

// column-reverse: children laid out bottom-to-top through full pipeline
TEST_P(LayoutRegressionFixture, columnReverse) {
    auto tree = createTsxTree("LayoutColumnReverse");
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(200.0f, 300.0f), LayoutDirectionLTR);

    auto first = tree->getViewNodeForNodePath(parseNodePath("first"));
    auto second = tree->getViewNodeForNodePath(parseNodePath("second"));
    auto third = tree->getViewNodeForNodePath(parseNodePath("third"));

    // column-reverse: first child at bottom, last at top
    ASSERT_EQ(250.0f, first->getCalculatedFrame().y);
    ASSERT_EQ(200.0f, second->getCalculatedFrame().y);
    ASSERT_EQ(150.0f, third->getCalculatedFrame().y);
}

INSTANTIATE_TEST_SUITE_P(LayoutRegressionTests,
                         LayoutRegressionFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::QuickJSWithTSN
#ifdef __APPLE__
                                           ,
                                           JavaScriptEngineTestCase::JSCore
#endif
                                           ),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
