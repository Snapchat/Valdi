#include "ViewNodeTestsUtils.hpp"
#include "gtest/gtest.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

std::unique_ptr<ViewNodeChildrenIndexer> makeIndexer(ViewNode* viewNode) {
    auto indexer = std::make_unique<ViewNodeChildrenIndexer>(viewNode);
    indexer->setHorizontal(viewNode->isHorizontal());
    return indexer;
}

bool contains(const ReusableArray<ViewNode*>& nodes, ViewNode* node) {
    return std::find(nodes->begin(), nodes->end(), node) != nodes->end();
}

Frame visualBounds(ViewNode* node) {
    return node->getCalculatedFrame().withOffset(node->getDirectionDependentTranslationX(), node->getTranslationY());
}

std::vector<ViewNode*> sortedNodes(const ReusableArray<ViewNode*>& nodes) {
    std::vector<ViewNode*> sorted(nodes->begin(), nodes->end());
    std::sort(sorted.begin(), sorted.end(), std::less<>());
    return sorted;
}

void expectConservativeCandidates(const ChildrenVisibilityResult& result,
                                  const std::vector<Ref<ViewNode>>& children,
                                  const Frame& viewport) {
    auto visible = sortedNodes(result.visibleChildren);
    auto invisible = sortedNodes(result.invisibleChildren);
    EXPECT_EQ(visible.end(), std::adjacent_find(visible.begin(), visible.end()));
    EXPECT_EQ(invisible.end(), std::adjacent_find(invisible.begin(), invisible.end()));
    std::vector<ViewNode*> inBoth;
    std::set_intersection(
        visible.begin(), visible.end(), invisible.begin(), invisible.end(), std::back_inserter(inBoth), std::less<>());
    EXPECT_TRUE(inBoth.empty());
    std::vector<ViewNode*> known;
    for (const auto& child : children) {
        known.emplace_back(child.get());
    }
    std::sort(known.begin(), known.end(), std::less<>());
    for (const auto* nodes : {&visible, &invisible}) {
        for (auto* node : *nodes) {
            EXPECT_TRUE(std::binary_search(known.begin(), known.end(), node, std::less<>()));
        }
    }
    for (size_t i = 0; i < children.size(); i++) {
        if (visualBounds(children[i].get()).intersects(viewport)) {
            EXPECT_TRUE(std::binary_search(visible.begin(), visible.end(), children[i].get(), std::less<>())) << i;
        }
    }
}

void expectFrameVisibility(ViewNode* parent, const std::string& context) {
    // Expectations come from frames and translations only, so scaled children and custom viewport flags need
    // explicit checks instead.
    size_t index = 0;
    for (auto* child : *parent) {
        auto expected =
            parent->isVisibleInViewport() && visualBounds(child).intersects(parent->getCalculatedViewport());
        EXPECT_EQ(expected, child->isVisibleInViewport()) << context << ", child " << index;
        expectFrameVisibility(child, context + "/" + std::to_string(index));
        index++;
    }
}

} // namespace

TEST(ViewNodeChildrenIndexer, findsVisibleChildrenWhenWrapFitsOnOneLine) {
    for (bool horizontal : {false, true}) {
        SCOPED_TRACE(horizontal);
        ViewNodeTestsDependencies utils;
        auto root = utils.createLayout();
        utils.setViewNodeAttribute(root, "flexDirection", Value(horizontal ? "row" : "column"));
        utils.setViewNodeAttribute(root, "flexWrap", Value("wrap"));
        std::vector<Ref<ViewNode>> children;
        for (size_t i = 0; i < 200; i++) {
            auto child = utils.createLayout();
            utils.setViewNodeAttribute(child, "width", Value(10.0));
            utils.setViewNodeAttribute(child, "height", Value(10.0));
            root->appendChild(utils.getViewTransactionScope(), child);
            children.emplace_back(std::move(child));
        }
        root->performLayout(
            utils.getViewTransactionScope(), horizontal ? Size(2000, 10) : Size(10, 2000), LayoutDirectionLTR);

        auto indexer = makeIndexer(root.get());
        auto viewport = horizontal ? Frame(100, 0, 20, 10) : Frame(0, 100, 10, 20);
        auto result = indexer->findChildrenVisibility(viewport);
        expectConservativeCandidates(result, children, viewport);
        EXPECT_LE(result.visibleChildren->size(), 2u);
    }
}

TEST(ViewNodeChildrenIndexer, includesTranslatedAndZeroSizeChildren) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        utils.setViewNodeFrame(child, 0, static_cast<double>(i) * 10, 1000, i == 10 ? 0 : 10);
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    utils.setViewNodeAttribute(children[50], "translationY", Value(-400.0));
    root->performLayout(utils.getViewTransactionScope(), Size(1000, 1000), LayoutDirectionLTR);

    auto indexer = makeIndexer(root.get());
    auto viewport = Frame(0, 95, 1000, 20);
    auto result = indexer->findChildrenVisibility(viewport);
    expectConservativeCandidates(result, children, viewport);
    EXPECT_TRUE(contains(result.visibleChildren, children[10].get()));
    EXPECT_TRUE(contains(result.visibleChildren, children[50].get()));
}

TEST(ViewNodeChildrenIndexer, honorsScaleAndCustomViewportFlagsInWrappedGrid) {
    for (bool horizontal : {false, true}) {
        SCOPED_TRACE(horizontal);
        ViewNodeTestsDependencies utils;
        auto root = utils.createLayout();
        utils.setViewNodeAttribute(root, "flexDirection", Value(horizontal ? "row" : "column"));
        utils.setViewNodeAttribute(root, "flexWrap", Value("wrap"));
        std::vector<Ref<ViewNode>> children;
        for (size_t i = 0; i < 200; i++) {
            auto child = utils.createView();
            utils.setViewNodeAttribute(child, "width", Value(horizontal ? 50.0 : 10.0));
            utils.setViewNodeAttribute(child, "height", Value(horizontal ? 10.0 : 50.0));
            root->appendChild(utils.getViewTransactionScope(), child);
            children.emplace_back(std::move(child));
        }
        root->performLayout(
            utils.getViewTransactionScope(), horizontal ? Size(100, 1000) : Size(1000, 100), LayoutDirectionLTR);
        utils.getTree().setViewport({horizontal ? Frame(0, 100, 100, 20) : Frame(100, 0, 20, 100)});
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        ASSERT_FALSE(children[18]->isVisibleInViewport());
        ASSERT_FALSE(children[0]->isVisibleInViewport());

        for (float scale : {3.0f, -3.0f}) {
            if (horizontal) {
                children[18]->setScaleY(scale);
            } else {
                children[18]->setScaleX(scale);
            }
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            EXPECT_TRUE(children[18]->isVisibleInViewport());
        }

        children[0]->setIgnoreParentViewport(true);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        EXPECT_TRUE(children[0]->isVisibleInViewport());
        children[0]->setIgnoreParentViewport(false);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        EXPECT_FALSE(children[0]->isVisibleInViewport());

        auto grandchild = utils.createView();
        utils.setViewNodeFrame(grandchild, 100, 100, 20, 20);
        children[0]->appendChild(utils.getViewTransactionScope(), grandchild);
        root->performLayout(
            utils.getViewTransactionScope(), horizontal ? Size(100, 1000) : Size(1000, 100), LayoutDirectionLTR);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        ASSERT_FALSE(children[0]->isVisibleInViewport());
        children[0]->setExtendViewportWithChildren(true);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        EXPECT_TRUE(children[0]->isVisibleInViewport());

        utils.setViewNodeFrame(grandchild, 0, 0, 10, 10);
        root->performLayout(
            utils.getViewTransactionScope(), horizontal ? Size(100, 1000) : Size(1000, 100), LayoutDirectionLTR);
        utils.getTree().setViewport({horizontal ? Frame(0, 110, 100, 20) : Frame(110, 0, 20, 100)});
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        EXPECT_FALSE(children[0]->isVisibleInViewport());
    }
}

TEST(ViewNodeChildrenIndexer, limitsCandidatesForListOfNarrowChildren) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    double y = 0;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        auto height = i % 2 == 0 ? 10.0 : 30.0;
        utils.setViewNodeFrame(child, 0, y, 1, height);
        y += height;
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    root->performLayout(utils.getViewTransactionScope(), Size(400, 2000), LayoutDirectionLTR);

    auto indexer = makeIndexer(root.get());
    auto viewport = Frame(0, 1000, 400, 20);
    auto result = indexer->findChildrenVisibility(viewport);
    expectConservativeCandidates(result, children, viewport);
    EXPECT_LE(result.visibleChildren->size(), 3u);
}

TEST(ViewNodeChildrenIndexer, limitsCandidatesForNarrowViewportOverStackedRows) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        utils.setViewNodeFrame(child, 0, static_cast<double>(i) * 100, 200, 100);
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    root->performLayout(utils.getViewTransactionScope(), Size(10000, 10000), LayoutDirectionLTR);

    auto indexer = makeIndexer(root.get());
    auto viewport = Frame(0, 0, 100, 1000);
    auto result = indexer->findChildrenVisibility(viewport);
    expectConservativeCandidates(result, children, viewport);
    EXPECT_LE(result.visibleChildren->size(), 10u);
}

TEST(ViewNodeChildrenIndexer, cullsScrollViewChildrenAlongEitherScrollAxis) {
    for (bool horizontal : {false, true}) {
        SCOPED_TRACE(horizontal);
        ViewNodeTestsDependencies utils;
        auto root = utils.createScroll();
        utils.setViewNodeAttribute(root, "flexDirection", Value(horizontal ? "row" : "column"));
        std::vector<Ref<ViewNode>> children;
        for (size_t i = 0; i < 200; i++) {
            auto child = utils.createView();
            utils.setViewNodeAttribute(child, "width", Value(horizontal ? 10.0 : 100.0));
            utils.setViewNodeAttribute(child, "height", Value(horizontal ? 100.0 : 10.0));
            utils.setViewNodeAttribute(child, "flexShrink", Value(0.0));
            root->appendChild(utils.getViewTransactionScope(), child);
            children.emplace_back(std::move(child));
        }
        root->performLayout(utils.getViewTransactionScope(), Size(100, 100), LayoutDirectionLTR);
        ASSERT_EQ(2000.0f, horizontal ? root->getChildrenSpaceWidth() : root->getChildrenSpaceHeight());

        utils.getTree().setViewport({Frame(0, 0, 100, 100)});
        root->setScrollContentOffset(utils.getViewTransactionScope(), horizontal ? Point(100, 0) : Point(0, 100));
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        ASSERT_NE(nullptr, root->getChildrenIndexer());
        for (size_t i = 0; i < children.size(); i++) {
            EXPECT_EQ(i >= 10 && i < 20, children[i]->isVisibleInViewport()) << i;
        }
    }
}

TEST(ViewNodeChildrenIndexer, updatesVisibilityAfterChildrenReflowWithoutManualInvalidation) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        utils.setViewNodeFrame(child, 0, static_cast<double>(i) * 10, 1000, 10);
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    root->performLayout(utils.getViewTransactionScope(), Size(1000, 1000), LayoutDirectionLTR);
    utils.getTree().setViewport({Frame(100, 100, 20, 20)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    ASSERT_TRUE(children[10]->isVisibleInViewport());

    for (size_t i = 0; i < children.size(); i++) {
        utils.setViewNodeFrame(children[i], static_cast<double>(i) * 10, 0, 10, 1000);
    }
    root->performLayout(utils.getViewTransactionScope(), Size(1000, 1000), LayoutDirectionLTR);
    utils.getTree().setViewport({Frame(200, 100, 20, 20)});
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    for (size_t i = 0; i < children.size(); i++) {
        EXPECT_EQ(i == 20 || i == 21, children[i]->isVisibleInViewport()) << i;
    }

    root->performLayout(utils.getViewTransactionScope(), Size(1000, 2000), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    expectFrameVisibility(root.get(), "container grown");

    utils.getTree().setViewport({Frame(0, 300, 1000, 200)});
    for (double rowHeight : {30.0, 10.0}) {
        double y = 0;
        for (size_t i = 0; i < children.size(); i++) {
            auto height = i < 20 ? rowHeight : 10.0;
            utils.setViewNodeFrame(children[i], 0, y, 1000, height);
            y += height;
        }
        root->performLayout(utils.getViewTransactionScope(), Size(1000, 2000), LayoutDirectionLTR);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        expectFrameVisibility(root.get(), "first rows " + std::to_string(rowHeight) + " tall");
        size_t firstVisible = rowHeight == 30.0 ? 10 : 30;
        size_t lastVisible = rowHeight == 30.0 ? 16 : 49;
        for (size_t i = 0; i < children.size(); i++) {
            EXPECT_EQ(i >= firstVisible && i <= lastVisible, children[i]->isVisibleInViewport())
                << rowHeight << " " << i;
        }
    }

    root->performLayout(utils.getViewTransactionScope(), Size(1000, 1000), LayoutDirectionLTR);
    root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    expectFrameVisibility(root.get(), "container shrunk");
}

TEST(ViewNodeChildrenIndexer, keepsVisibilityExactThroughViewportShapeChangesAndJumps) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        utils.setViewNodeFrame(child, static_cast<double>(i) * 100, static_cast<double>(i) * 100, 100, 100);
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    root->performLayout(utils.getViewTransactionScope(), Size(10000, 10000), LayoutDirectionLTR);
    for (size_t step = 0; step < 60; step++) {
        bool tall = step >= 10 && step < 35;
        float position = step % 10 == 0 ? 5000.0f : static_cast<float>(step) * 10;
        auto viewport = tall ? Frame(position, 0, 100, 10000) : Frame(0, position, 10000, 100);
        utils.getTree().setViewport({viewport});
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        for (const auto& child : children) {
            EXPECT_EQ(child->getCalculatedFrame().intersects(viewport), child->isVisibleInViewport()) << step;
        }
    }
}

TEST(ViewNodeChildrenIndexer, limitsCandidatesWhenHiddenListBecomesVisible) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    double y = 0;
    for (size_t i = 0; i < 200; i++) {
        auto child = utils.createView();
        auto height = i % 2 == 0 ? 10.0 : 30.0;
        utils.setViewNodeFrame(child, 0, y, 400, height);
        y += height;
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    root->performLayout(utils.getViewTransactionScope(), Size(400, 4000), LayoutDirectionLTR);

    auto indexer = makeIndexer(root.get());
    indexer->findChildrenVisibility(Frame());
    auto viewport = Frame(0, 1000, 400, 20);
    auto result = indexer->findChildrenVisibility(viewport);
    expectConservativeCandidates(result, children, viewport);
    EXPECT_LE(result.visibleChildren->size(), 3u);
}

TEST(ViewNodeChildrenIndexer, wrappedGridsInScrollKeepTileVisibility) {
    for (size_t columns : {2, 3, 4}) {
        SCOPED_TRACE(columns);
        ViewNodeTestsDependencies utils;
        auto scroll = utils.createScroll();
        auto header = utils.createView();
        utils.setViewNodeAttribute(header, "width", Value(390.0));
        utils.setViewNodeAttribute(header, "height", Value(150.0));
        utils.setViewNodeAttribute(header, "flexShrink", Value(0.0));
        scroll->appendChild(utils.getViewTransactionScope(), header);
        auto grid = utils.createLayout();
        utils.setViewNodeAttribute(grid, "flexDirection", Value("row"));
        utils.setViewNodeAttribute(grid, "flexWrap", Value("wrap"));
        utils.setViewNodeAttribute(grid, "width", Value(390.0));
        utils.setViewNodeAttribute(grid, "flexShrink", Value(0.0));
        scroll->appendChild(utils.getViewTransactionScope(), grid);

        std::vector<Ref<ViewNode>> tiles;
        std::vector<Ref<ViewNode>> dividers;
        auto rowCount = [&]() { return (tiles.size() + columns - 1) / columns; };
        auto addPage = [&]() {
            for (size_t i = 0; i < 15; i++) {
                auto tile = utils.createView();
                utils.setViewNodeAttribute(tile, "width", Value(390.0 / static_cast<double>(columns)));
                utils.setViewNodeAttribute(tile, "height", Value(120.0));
                grid->appendChild(utils.getViewTransactionScope(), tile);
                if (columns == 2 && tiles.size() % 2 == 0) {
                    auto divider = utils.createView();
                    grid->appendChild(utils.getViewTransactionScope(), divider);
                    dividers.emplace_back(std::move(divider));
                }
                tiles.emplace_back(std::move(tile));
            }
            for (const auto& divider : dividers) {
                utils.setViewNodeFrame(divider, 195, 0, 1, static_cast<double>(rowCount()) * 120);
            }
            scroll->performLayout(utils.getViewTransactionScope(), Size(390, 600), LayoutDirectionLTR);
        };
        auto scrollTo = [&](float offset) {
            scroll->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, offset));
            scroll->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            expectFrameVisibility(scroll.get(), "offset " + std::to_string(offset));
        };

        utils.getTree().setViewport({Frame(0, 0, 390, 600)});
        addPage();
        addPage();
        for (float offset : {0.0f, 250.0f, 500.0f, 150.0f, 0.0f}) {
            scrollTo(offset);
        }
        EXPECT_TRUE(tiles[0]->isVisibleInViewport());
        EXPECT_FALSE(tiles[4 * columns]->isVisibleInViewport());

        scrollTo(450);
        addPage();
        scroll->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        expectFrameVisibility(scroll.get(), "page appended while scrolled");
        scrollTo(grid->getCalculatedFrame().getBottom() - 600.0f);
        EXPECT_FALSE(tiles[0]->isVisibleInViewport());
        EXPECT_TRUE(tiles.back()->isVisibleInViewport());
    }
}

TEST(ViewNodeChildrenIndexer, nestedCarouselsInVerticalScroll) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    auto outer = utils.createScroll();
    utils.setViewNodeFrame(outer, 0, 60, 390, 300);
    root->appendChild(utils.getViewTransactionScope(), outer);
    std::vector<Ref<ViewNode>> rows;
    std::vector<std::vector<Ref<ViewNode>>> cards(20);
    for (size_t r = 0; r < 20; r++) {
        auto row = utils.createScroll();
        utils.setViewNodeAttribute(row, "flexDirection", Value("row"));
        utils.setViewNodeAttribute(row, "flexShrink", Value(0.0));
        utils.setViewNodeMarginAndSize(row, 20, 300, 100);
        for (size_t c = 0; c < 20; c++) {
            auto card = utils.createView();
            utils.setViewNodeAttribute(card, "width", Value(100.0));
            utils.setViewNodeAttribute(card, "height", Value(100.0));
            utils.setViewNodeAttribute(card, "flexShrink", Value(0.0));
            row->appendChild(utils.getViewTransactionScope(), card);
            cards[r].emplace_back(std::move(card));
        }
        outer->appendChild(utils.getViewTransactionScope(), row);
        rows.emplace_back(std::move(row));
    }
    utils.getTree().setViewport({Frame(0, 0, 390, 500)});
    root->performLayout(utils.getViewTransactionScope(), Size(390, 500), LayoutDirectionLTR);
    ASSERT_EQ(Frame(20, 160, 300, 100), rows[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(300, 0, 100, 100), cards[1][3]->getCalculatedFrame());

    auto scrollOuter = [&](float offset) {
        outer->setScrollContentOffset(utils.getViewTransactionScope(), Point(0, offset));
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        expectFrameVisibility(root.get(), "outer offset " + std::to_string(offset));
    };
    auto scrollRow = [&](size_t r, float offset) {
        rows[r]->setScrollContentOffset(utils.getViewTransactionScope(), Point(offset, 0));
    };

    scrollRow(1, 250);
    scrollOuter(50);
    EXPECT_TRUE(rows[0]->isVisibleInViewport());
    EXPECT_TRUE(cards[0][2]->isVisibleInViewport());
    // Inside the outer scroll's viewport, but past the right edge of its own row.
    EXPECT_FALSE(cards[0][3]->isVisibleInViewport());
    for (size_t c = 0; c < 20; c++) {
        EXPECT_EQ(c >= 2 && c <= 5, cards[1][c]->isVisibleInViewport()) << c;
    }
    EXPECT_TRUE(rows[2]->isVisibleInViewport());
    EXPECT_FALSE(rows[3]->isVisibleInViewport());

    for (float offset = 100; offset <= 900; offset += 50) {
        if (offset == 300) {
            scrollRow(4, 1700);
        }
        scrollOuter(offset);
    }
    for (float offset = 800; offset >= 0; offset -= 100) {
        scrollOuter(offset);
    }

    scrollOuter(50);
    ASSERT_TRUE(cards[1][2]->isVisibleInViewport());
    scrollOuter(900);
    EXPECT_FALSE(rows[1]->isVisibleInViewport());
    scrollRow(1, 1200);
    scrollOuter(50);
    for (size_t c = 0; c < 20; c++) {
        EXPECT_EQ(c >= 12 && c <= 14, cards[1][c]->isVisibleInViewport()) << c;
    }
}

TEST(ViewNodeChildrenIndexer, transformedChildrenTrackVisualBounds) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        utils.setViewNodeFrame(child, 0, static_cast<double>(i) * 100, 400, 100);
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    auto update = [&](const Frame& viewport) {
        utils.getTree().setViewport({viewport});
        root->performLayout(utils.getViewTransactionScope(), Size(400, 10000), LayoutDirectionLTR);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
    };
    auto viewport = Frame(0, 1000, 400, 300);
    update(viewport);
    expectFrameVisibility(root.get(), "initial");
    ASSERT_FALSE(children[50]->isVisibleInViewport());

    utils.setViewNodeAttribute(children[50], "translationY", Value(-3950.0));
    update(viewport);
    expectFrameVisibility(root.get(), "translated into view");
    EXPECT_TRUE(children[50]->isVisibleInViewport());
    update(Frame(0, 5000, 400, 300));
    expectFrameVisibility(root.get(), "untranslated position in view");
    EXPECT_FALSE(children[50]->isVisibleInViewport());
    EXPECT_TRUE(children[51]->isVisibleInViewport());
    update(viewport);
    expectFrameVisibility(root.get(), "back to the translated position");
    EXPECT_TRUE(children[50]->isVisibleInViewport());
    utils.setViewNodeAttribute(children[50], "translationY", Value(0.0));
    update(viewport);
    expectFrameVisibility(root.get(), "translated out of view");
    EXPECT_FALSE(children[50]->isVisibleInViewport());

    utils.setViewNodeAttribute(children[12], "translationX", Value(500.0));
    update(viewport);
    expectFrameVisibility(root.get(), "translated sideways out of view");
    EXPECT_FALSE(children[12]->isVisibleInViewport());
    utils.setViewNodeAttribute(children[12], "translationX", Value(0.0));
    update(viewport);
    expectFrameVisibility(root.get(), "translated sideways back");
    EXPECT_TRUE(children[12]->isVisibleInViewport());

    auto shortViewport = Frame(0, 1000, 400, 140);
    update(shortViewport);
    expectFrameVisibility(root.get(), "short viewport");
    EXPECT_TRUE(children[11]->isVisibleInViewport());
    children[11]->setScaleY(0.1f);
    update(shortViewport);
    EXPECT_FALSE(children[11]->isVisibleInViewport());
    EXPECT_TRUE(children[10]->isVisibleInViewport());
    children[11]->setScaleY(1.0f);
    update(shortViewport);
    EXPECT_TRUE(children[11]->isVisibleInViewport());
}

TEST(ViewNodeChildrenIndexer, oversizedChildrenStayVisibleWhileOverlapping) {
    auto build = [](ViewNodeTestsDependencies& utils, std::vector<Ref<ViewNode>>& children) {
        auto root = utils.createLayout();
        auto add = [&](double y, double height) {
            auto child = utils.createView();
            utils.setViewNodeFrame(child, 0, y, 400, height);
            root->appendChild(utils.getViewTransactionScope(), child);
            children.emplace_back(std::move(child));
        };
        for (size_t i = 0; i < 98; i++) {
            add(static_cast<double>(i) * 100, 100);
        }
        add(1000, 2000);
        add(0, 9000);
        root->performLayout(utils.getViewTransactionScope(), Size(400, 10000), LayoutDirectionLTR);
        return root;
    };
    struct Step {
        Frame viewport;
        bool hero;
        bool background;
    };
    const std::vector<Step> steps = {
        {Frame(0, 9500, 400, 300), false, false},
        {Frame(0, 800, 400, 200), false, true},
        {Frame(0, 900, 400, 200), true, true},
        {Frame(0, 2000, 400, 200), true, true},
        {Frame(0, 2900, 400, 200), true, true},
        {Frame(0, 3000, 400, 200), false, true},
        {Frame(0, 2950, 400, 200), true, true},
        {Frame(0, 1500, 400, 200), true, true},
        {Frame(0, 9500, 400, 300), false, false},
        {Frame(0, 1000, 400, 200), true, true},
        {Frame(0, 0, 400, 300), false, true},
        {Frame(0, 8900, 400, 200), false, true},
        {Frame(0, 9000, 400, 200), false, false},
    };

    {
        ViewNodeTestsDependencies utils;
        std::vector<Ref<ViewNode>> children;
        auto root = build(utils, children);
        for (size_t i = 0; i < steps.size(); i++) {
            SCOPED_TRACE(i);
            utils.getTree().setViewport({steps[i].viewport});
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            EXPECT_EQ(steps[i].hero, children[98]->isVisibleInViewport());
            EXPECT_EQ(steps[i].background, children[99]->isVisibleInViewport());
            expectFrameVisibility(root.get(), "step " + std::to_string(i));
        }
    }

    ViewNodeTestsDependencies utils;
    std::vector<Ref<ViewNode>> children;
    auto root = build(utils, children);
    auto indexer = makeIndexer(root.get());
    for (size_t i = 0; i < steps.size(); i++) {
        SCOPED_TRACE(i);
        expectConservativeCandidates(indexer->findChildrenVisibility(steps[i].viewport), children, steps[i].viewport);
    }
}

TEST(ViewNodeChildrenIndexer, childrenChangesAroundIndexingThreshold) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> rows;
    auto makeRow = [&]() {
        auto row = utils.createView();
        utils.setViewNodeAttribute(row, "width", Value(400.0));
        utils.setViewNodeAttribute(row, "height", Value(100.0));
        utils.setViewNodeAttribute(row, "flexShrink", Value(0.0));
        rows.emplace_back(row);
        return row;
    };
    auto update = [&](const std::string& context) {
        root->performLayout(utils.getViewTransactionScope(), Size(400, 2000), LayoutDirectionLTR);
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        expectFrameVisibility(root.get(), context);
    };
    utils.getTree().setViewport({Frame(0, 0, 400, 250)});

    for (size_t i = 0; i < kMaxChildrenBeforeIndexing; i++) {
        root->appendChild(utils.getViewTransactionScope(), makeRow());
    }
    update("10 children");
    EXPECT_EQ(nullptr, root->getChildrenIndexer());
    EXPECT_TRUE(root->getChildAt(2)->isVisibleInViewport());
    EXPECT_FALSE(root->getChildAt(3)->isVisibleInViewport());

    root->appendChild(utils.getViewTransactionScope(), makeRow());
    update("11 children");
    ASSERT_NE(nullptr, root->getChildrenIndexer());

    root->getChildAt(10)->removeFromParent(utils.getViewTransactionScope());
    update("back to 10 children");

    while (root->getChildCount() > 0) {
        root->getChildAt(0)->removeFromParent(utils.getViewTransactionScope());
    }
    update("no children");

    for (size_t i = 0; i <= kMaxChildrenBeforeIndexing; i++) {
        root->appendChild(utils.getViewTransactionScope(), makeRow());
    }
    update("11 children again");
    EXPECT_TRUE(root->getChildAt(0)->isVisibleInViewport());

    utils.getTree().setViewport({Frame(0, 600, 400, 250)});
    update("scrolled");
    auto* shiftedIn = root->getChildAt(5);
    auto* pushedOut = root->getChildAt(8);
    ASSERT_FALSE(shiftedIn->isVisibleInViewport());
    ASSERT_TRUE(pushedOut->isVisibleInViewport());
    root->insertChildAt(utils.getViewTransactionScope(), makeRow(), 0);
    update("inserted at the front while scrolled");
    EXPECT_TRUE(shiftedIn->isVisibleInViewport());
    EXPECT_FALSE(pushedOut->isVisibleInViewport());

    auto* removed = root->getChildAt(7);
    ASSERT_TRUE(removed->isVisibleInViewport());
    removed->removeFromParent(utils.getViewTransactionScope());
    update("removed a visible child");

    ASSERT_EQ(shiftedIn, root->getChildAt(6));
    ASSERT_TRUE(shiftedIn->isVisibleInViewport());
    shiftedIn->removeFromParent(utils.getViewTransactionScope());
    auto replacement = makeRow();
    root->insertChildAt(utils.getViewTransactionScope(), replacement, 6);
    update("replaced a visible child");
    EXPECT_TRUE(replacement->isVisibleInViewport());

    ViewNodeTestsDependencies directUtils;
    auto directRoot = directUtils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 12; i++) {
        auto child = directUtils.createView();
        directUtils.setViewNodeFrame(child, 0, static_cast<double>(i) * 100, 400, 100);
        directRoot->appendChild(directUtils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    directRoot->performLayout(directUtils.getViewTransactionScope(), Size(400, 1200), LayoutDirectionLTR);
    auto indexer = makeIndexer(directRoot.get());
    auto viewport = Frame(0, 450, 400, 200);
    expectConservativeCandidates(indexer->findChildrenVisibility(viewport), children, viewport);
    auto removedChild = children[5];
    removedChild->removeFromParent(directUtils.getViewTransactionScope());
    children.erase(children.begin() + 5);
    directRoot->performLayout(directUtils.getViewTransactionScope(), Size(400, 1200), LayoutDirectionLTR);
    indexer->setNeedsUpdate();
    for (auto nextViewport : {viewport, Frame(0, 0, 400, 1200)}) {
        auto result = indexer->findChildrenVisibility(nextViewport);
        expectConservativeCandidates(result, children, nextViewport);
        EXPECT_FALSE(contains(result.visibleChildren, removedChild.get()));
        EXPECT_FALSE(contains(result.invisibleChildren, removedChild.get()));
    }
}

TEST(ViewNodeChildrenIndexer, keepsVisibilityExactScrollingFromSparseToWideContent) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    auto add = [&](double x, double y) {
        auto child = utils.createView();
        utils.setViewNodeFrame(child, x, y, 100, 100);
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    };
    for (size_t i = 0; i < 6; i++) {
        add(0, static_cast<double>(i) * 200);
    }
    for (size_t i = 0; i < 90; i++) {
        add(static_cast<double>(i) * 100, 1600);
    }
    root->performLayout(utils.getViewTransactionScope(), Size(9000, 10000), LayoutDirectionLTR);

    std::vector<float> positions;
    for (int step = 0; step <= 42; step++) {
        positions.push_back(static_cast<float>(step) * 40);
    }
    for (int step = 42; step >= 0; step -= 3) {
        positions.push_back(static_cast<float>(step) * 40);
    }
    for (float position : {1600.0f, 0.0f, 1200.0f, 400.0f, 1680.0f}) {
        positions.push_back(position);
    }
    for (auto position : positions) {
        utils.getTree().setViewport({Frame(0, position, 390, 844)});
        root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
        expectFrameVisibility(root.get(), "position " + std::to_string(position));
    }
    for (size_t i = 6; i < children.size(); i++) {
        EXPECT_EQ(i < 10, children[i]->isVisibleInViewport()) << i;
    }
}

TEST(ViewNodeChildrenIndexer, rowsUseDirectionDependentTranslation) {
    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    utils.setViewNodeAttribute(root, "flexDirection", Value("row"));
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 20; i++) {
        auto child = utils.createView();
        utils.setViewNodeAttribute(child, "width", Value(50.0));
        utils.setViewNodeAttribute(child, "height", Value(50.0));
        utils.setViewNodeAttribute(child, "flexShrink", Value(0.0));
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    utils.setViewNodeAttribute(children[10], "translationX", Value(200.0));

    for (auto direction : {LayoutDirectionLTR, LayoutDirectionRTL, LayoutDirectionLTR}) {
        SCOPED_TRACE(direction);
        root->performLayout(utils.getViewTransactionScope(), Size(1000, 50), direction);
        auto frameX = children[10]->getCalculatedFrame().x;
        auto shift = direction == LayoutDirectionRTL ? -200.0f : 200.0f;
        auto check = [&](float x, bool visible) {
            utils.getTree().setViewport({Frame(x, 0, 50, 50)});
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            EXPECT_EQ(visible, children[10]->isVisibleInViewport()) << x;
            expectFrameVisibility(root.get(), "viewport x " + std::to_string(x));
        };
        check(frameX + shift, true);
        check(frameX - shift, false);
        check(frameX, false);
    }
}

TEST(ViewNodeChildrenIndexer, zeroHeightChildrenOnBucketEdges) {
    for (bool dividerFirst : {true, false}) {
        SCOPED_TRACE(dividerFirst);
        ViewNodeTestsDependencies utils;
        auto root = utils.createLayout();
        std::vector<Ref<ViewNode>> dividers;
        std::vector<Ref<ViewNode>> rows;
        for (size_t i = 0; i < 10; i++) {
            auto divider = utils.createView();
            utils.setViewNodeFrame(divider, 0, static_cast<double>(i) * 100, 400, 0);
            auto row = utils.createView();
            utils.setViewNodeFrame(row, 0, static_cast<double>(i) * 100, 400, 100);
            root->appendChild(utils.getViewTransactionScope(), dividerFirst ? divider : row);
            root->appendChild(utils.getViewTransactionScope(), dividerFirst ? row : divider);
            dividers.emplace_back(std::move(divider));
            rows.emplace_back(std::move(row));
        }
        root->performLayout(utils.getViewTransactionScope(), Size(400, 1000), LayoutDirectionLTR);

        auto check = [&](const Frame& viewport, const std::vector<size_t>& visibleDividers) {
            utils.getTree().setViewport({viewport});
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            expectFrameVisibility(root.get(), "viewport y " + std::to_string(viewport.y));
            for (size_t i = 0; i < dividers.size(); i++) {
                auto expected = std::find(visibleDividers.begin(), visibleDividers.end(), i) != visibleDividers.end();
                EXPECT_EQ(expected, dividers[i]->isVisibleInViewport()) << viewport.y << " divider " << i;
            }
        };
        check(Frame(0, 150, 400, 200), {2, 3});
        check(Frame(0, 250, 400, 50), {});
        check(Frame(0, 299, 400, 2), {3});
        check(Frame(0, 300, 400, 50), {});
        check(Frame(0, 200, 400, 200), {3});
        check(Frame(0, 0, 400, 1000), {1, 2, 3, 4, 5, 6, 7, 8, 9});
    }
}

TEST(ViewNodeChildrenIndexer, stackedChildrenShareVisibility) {
    auto build = [](ViewNodeTestsDependencies& utils,
                    std::vector<Ref<ViewNode>>& children,
                    std::vector<Ref<ViewNode>>& stacked) {
        auto root = utils.createLayout();
        for (size_t i = 0; i < 15; i++) {
            auto row = utils.createView();
            utils.setViewNodeFrame(row, 0, static_cast<double>(i) * 200, 400, 200);
            root->appendChild(utils.getViewTransactionScope(), row);
            children.emplace_back(std::move(row));
            if (i != 4) {
                continue;
            }
            for (size_t j = 0; j < 12; j++) {
                auto view = utils.createView();
                utils.setViewNodeFrame(view, 0, 1000, 400, 200);
                root->appendChild(utils.getViewTransactionScope(), view);
                if (j % 4 == 0) {
                    view->setZIndex(utils.getViewTransactionScope(), static_cast<int>(j) + 1);
                }
                children.emplace_back(view);
                stacked.emplace_back(std::move(view));
            }
        }
        root->performLayout(utils.getViewTransactionScope(), Size(400, 3000), LayoutDirectionLTR);
        return root;
    };
    struct Step {
        Frame viewport;
        bool stackVisible;
    };
    const std::vector<Step> steps = {
        {Frame(0, 0, 400, 300), false},
        {Frame(0, 900, 400, 300), true},
        {Frame(0, 1300, 400, 300), false},
        {Frame(0, 1100, 400, 300), true},
    };

    {
        ViewNodeTestsDependencies utils;
        std::vector<Ref<ViewNode>> children;
        std::vector<Ref<ViewNode>> stacked;
        auto root = build(utils, children, stacked);
        auto update = [&](const Frame& viewport, const std::string& context) {
            utils.getTree().setViewport({viewport});
            root->performLayout(utils.getViewTransactionScope(), Size(400, 3000), LayoutDirectionLTR);
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            expectFrameVisibility(root.get(), context);
        };
        for (size_t i = 0; i < steps.size(); i++) {
            update(steps[i].viewport, "step " + std::to_string(i));
            for (size_t j = 0; j < stacked.size(); j++) {
                EXPECT_EQ(steps[i].stackVisible, stacked[j]->isVisibleInViewport()) << i << " stacked " << j;
            }
        }

        utils.setViewNodeAttribute(stacked[7], "translationY", Value(1500.0));
        update(Frame(0, 1100, 400, 300), "one stacked child translated away");
        for (size_t j = 0; j < stacked.size(); j++) {
            EXPECT_EQ(j != 7, stacked[j]->isVisibleInViewport()) << j;
        }
        utils.setViewNodeAttribute(stacked[7], "translationY", Value(0.0));
        update(Frame(0, 1100, 400, 300), "translated back");
        for (const auto& view : stacked) {
            EXPECT_TRUE(view->isVisibleInViewport());
        }
    }

    ViewNodeTestsDependencies utils;
    std::vector<Ref<ViewNode>> children;
    std::vector<Ref<ViewNode>> stacked;
    auto root = build(utils, children, stacked);
    auto indexer = makeIndexer(root.get());
    for (size_t i = 0; i < steps.size(); i++) {
        SCOPED_TRACE(i);
        expectConservativeCandidates(indexer->findChildrenVisibility(steps[i].viewport), children, steps[i].viewport);
    }
}

TEST(ViewNodeChildrenIndexer, orderedReversedAndWrappedLayoutsMatchFrames) {
    const std::vector<std::function<Frame(size_t)>> shapes = {
        [](size_t i) { return Frame(0, static_cast<float>(i) * 10, 1000, 10); },
        [](size_t i) { return Frame(0, static_cast<float>(59 - i) * 10, 1000, 10); },
        [](size_t i) { return Frame(0, static_cast<float>(i) * 10, 1000, i == 10 || i == 11 ? 0 : 10); },
        [](size_t i) {
            if (i >= 10 && i < 16) {
                return Frame(0, 100, 1000, 0);
            }
            return Frame(0, static_cast<float>(i < 10 ? i : i - 6) * 10, 1000, 10);
        },
        [](size_t i) { return Frame(static_cast<float>(i) * 16, static_cast<float>(i) * 2, 16, 2); },
        [](size_t i) { return Frame(static_cast<float>(i % 2) * 500, static_cast<float>(i / 2) * 10, 500, 10); },
        [](size_t i) { return Frame(static_cast<float>(i) * 10, static_cast<float>(i) * 10, 10, 10); },
    };
    for (size_t shape = 0; shape < shapes.size(); shape++) {
        SCOPED_TRACE(shape);
        ViewNodeTestsDependencies utils;
        auto root = utils.createLayout();
        std::vector<Ref<ViewNode>> children;
        for (size_t i = 0; i < 60; i++) {
            auto child = utils.createView();
            auto frame = shapes[shape](i);
            utils.setViewNodeFrame(child, frame.x, frame.y, frame.width, frame.height);
            root->appendChild(utils.getViewTransactionScope(), child);
            children.emplace_back(std::move(child));
        }
        root->performLayout(utils.getViewTransactionScope(), Size(1000, 1000), LayoutDirectionLTR);
        auto indexer = makeIndexer(root.get());
        for (float position : {0.0f, 100.0f, 300.0f, 590.0f, 100.0f}) {
            for (auto viewport : {Frame(0, position, 1000, 20), Frame(position, 0, 20, 1000)}) {
                expectConservativeCandidates(indexer->findChildrenVisibility(viewport), children, viewport);
            }
        }
    }

    struct FlexLayout {
        const char* direction;
        const char* wrap;
        LayoutDirection layoutDirection;
    };
    for (auto layout : {FlexLayout{"row", "wrap", LayoutDirectionLTR},
                        FlexLayout{"row", "wrap", LayoutDirectionRTL},
                        FlexLayout{"row-reverse", "wrap-reverse", LayoutDirectionLTR},
                        FlexLayout{"column", "wrap", LayoutDirectionRTL},
                        FlexLayout{"column-reverse", "wrap", LayoutDirectionLTR}}) {
        SCOPED_TRACE(layout.direction);
        SCOPED_TRACE(layout.wrap);
        SCOPED_TRACE(layout.layoutDirection);
        bool horizontal = layout.direction[0] == 'r';
        ViewNodeTestsDependencies utils;
        auto root = utils.createLayout();
        utils.setViewNodeAttribute(root, "flexDirection", Value(layout.direction));
        utils.setViewNodeAttribute(root, "flexWrap", Value(layout.wrap));
        std::vector<Ref<ViewNode>> children;
        for (size_t i = 0; i < 200; i++) {
            auto child = utils.createView();
            utils.setViewNodeAttribute(child, "width", Value(horizontal ? 50.0 : 10.0));
            utils.setViewNodeAttribute(child, "height", Value(horizontal ? 10.0 : 50.0));
            root->appendChild(utils.getViewTransactionScope(), child);
            children.emplace_back(std::move(child));
        }
        root->performLayout(
            utils.getViewTransactionScope(), horizontal ? Size(100, 1000) : Size(1000, 100), layout.layoutDirection);
        for (const auto& child : children) {
            ASSERT_GE(child->getCalculatedFrame().x, 0);
            ASSERT_GE(child->getCalculatedFrame().y, 0);
        }
        for (float position : {0.0f, 500.0f, 980.0f, 250.0f}) {
            utils.getTree().setViewport({horizontal ? Frame(0, position, 100, 20) : Frame(position, 0, 20, 100)});
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            expectFrameVisibility(root.get(), "position " + std::to_string(position));
        }
    }

    ViewNodeTestsDependencies utils;
    auto root = utils.createLayout();
    std::vector<Ref<ViewNode>> children;
    for (size_t i = 0; i < 100; i++) {
        auto child = utils.createView();
        utils.setViewNodeAttribute(child, "width", Value(50.0));
        utils.setViewNodeAttribute(child, "height", Value(10.0));
        utils.setViewNodeAttribute(child, "flexShrink", Value(0.0));
        root->appendChild(utils.getViewTransactionScope(), child);
        children.emplace_back(std::move(child));
    }
    auto relayout = [&](const char* direction, const char* wrap, LayoutDirection layoutDirection) {
        utils.setViewNodeAttribute(root, "flexDirection", Value(direction));
        utils.setViewNodeAttribute(root, "flexWrap", Value(wrap));
        root->performLayout(utils.getViewTransactionScope(), Size(100, 1000), layoutDirection);
        for (auto viewport : {Frame(0, 200, 100, 100), Frame(0, 600, 100, 100)}) {
            utils.getTree().setViewport({viewport});
            root->updateVisibilityAndPerformUpdates(utils.getViewTransactionScope());
            expectFrameVisibility(root.get(),
                                  std::string(direction) + " " + wrap + " at " + std::to_string(viewport.y));
        }
    };
    relayout("row", "wrap", LayoutDirectionLTR);
    relayout("row", "wrap-reverse", LayoutDirectionLTR);
    relayout("row", "wrap", LayoutDirectionRTL);
    relayout("column", "wrap", LayoutDirectionLTR);
    relayout("column", "wrap", LayoutDirectionRTL);
}

} // namespace ValdiTest
