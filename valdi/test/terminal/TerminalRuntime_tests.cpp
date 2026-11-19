#include "TerminalTestUtils.hpp"
#include "TerminalTreeFixture.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <yoga/Yoga.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

using Lines = std::vector<std::string>;

static std::string padded(const std::string& text, size_t width) {
    return text + std::string(width - text.size(), ' ');
}

TEST(TerminalRuntime, createsAWholePointViewManagerContextOnDemand) {
    TerminalTreeFixture f;
    ASSERT_NE(nullptr, f.viewManagerContext);
    ASSERT_EQ(&f.terminalRuntime->getViewManager(), &f.viewManagerContext->getViewManager());
    ASSERT_EQ(f.viewManagerContext.get(), f.terminalRuntime->getViewManagerContext().get());
    ASSERT_EQ(RenderingBackendTypeTerminal, f.viewManagerContext->getViewManager().getRenderingBackendType());
    ASSERT_EQ(1.0f, YGConfigGetPointScaleFactor(f.viewManagerContext->getAttributesManager().getYogaConfig()));
}

TEST(TerminalRuntime, splitsAnUnevenFlexRowIntoWholeCellsWithoutGapsOrOverlap) {
    TerminalTreeFixture f;
    f.set(f.rootNode, "flexDirection", Value(StringCache::getGlobal().makeString(std::string_view("row"))));
    std::vector<Ref<ViewNode>> columns;
    for (int i = 0; i < 3; i++) {
        auto column = f.createNode("SCValdiView");
        f.set(column, "flexGrow", Value(1.0));
        f.set(column, "height", Value(2.0));
        f.set(column, "borderWidth", Value(1.0));
        // A leaf gets a measure function and Yoga's text rounding, so the column gets a child to stay a container.
        f.append(column, f.createNode("SCValdiView"));
        f.append(f.rootNode, column);
        columns.push_back(column);
    }

    f.root->setSize(10, 4);
    f.layout();
    f.root->paintNow();

    std::string top;
    float x = 0;
    for (const auto& column : columns) {
        const auto& frame = column->getViewFrame();
        ASSERT_EQ(x, frame.x);
        ASSERT_EQ(std::floor(frame.width), frame.width);
        ASSERT_GE(frame.width, 3.0f);
        ASSERT_LE(frame.width, 4.0f);
        x += frame.width;
        std::string horizontal;
        for (int i = 0; i < static_cast<int>(frame.width) - 2; i++) {
            horizontal += "─";
        }
        top += "┌" + horizontal + "┐";
    }
    ASSERT_EQ(10.0f, x);
    ASSERT_EQ(top, f.sink->lastPlainLines().at(0));
}

TEST(TerminalRuntime, splitsAnUnevenFlexRowOfLeafViewsIntoWholeCellsWithoutOverlap) {
    TerminalTreeFixture f;
    f.set(f.rootNode, "flexDirection", Value(StringCache::getGlobal().makeString(std::string_view("row"))));
    std::vector<Ref<ViewNode>> columns;
    for (int i = 0; i < 3; i++) {
        auto column = f.createNode("SCValdiView");
        f.set(column, "flexGrow", Value(1.0));
        f.append(f.rootNode, column);
        columns.push_back(column);
    }
    f.root->setSize(10, 4);
    f.layout();

    float x = 0;
    for (const auto& column : columns) {
        ASSERT_EQ(x, column->getViewFrame().x);
        x += column->getViewFrame().width;
    }
    ASSERT_EQ(10.0f, x);
}

TEST(TerminalRuntime, rendersABorderedBoxWithALabelEndToEnd) {
    TerminalTreeFixture f;
    auto box = f.createNode("SCValdiView");
    f.set(box, "borderWidth", Value(1.0));
    f.set(box, "padding", Value(1.0));
    auto label = f.createNode("SCValdiLabel");
    f.set(label, "value", Value(STRING_LITERAL("hello")));
    f.append(f.rootNode, box);
    f.append(box, label);

    f.root->setSize(20, 10);
    f.layout();
    f.root->paintNow();

    const auto width = 20u;
    ASSERT_EQ((Lines{"┌──────────────────┐", "│" + padded("hello", width - 2) + "│", "└──────────────────┘"}),
              f.sink->lastPlainLines());
}

} // namespace ValdiTest
