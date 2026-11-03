#include "TerminalTestUtils.hpp"
#include "TerminalTreeFixture.hpp"

#include <gtest/gtest.h>

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
