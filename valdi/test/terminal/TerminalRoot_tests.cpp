#include "TerminalTreeFixture.hpp"

#include <gtest/gtest.h>

using namespace Valdi;
using namespace Valdi::Terminal;

namespace ValdiTest {

using Lines = std::vector<std::string>;

namespace {

Value text(const char* value) {
    return Value(StringCache::getGlobal().makeString(std::string_view(value)));
}

Ref<ViewNode> addLabel(TerminalTreeFixture& f, const Ref<ViewNode>& parent, const char* value, int numberOfLines = 3) {
    auto label = f.createNode("SCValdiLabel");
    f.set(label, "value", text(value));
    f.set(label, "numberOfLines", Value(static_cast<int32_t>(numberOfLines)));
    f.append(parent, label);
    return label;
}

void render(TerminalTreeFixture& f) {
    f.layout();
    f.root->paintNow();
}

} // namespace

TEST(TerminalRoot, wrapsLabelsToTheTerminalWidthAndReflowsOnResize) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hello world");
    render(f);
    ASSERT_EQ((Lines{"hello world"}), f.sink->lastPlainLines());

    f.root->setSize(5, 10);
    render(f);
    ASSERT_EQ((Lines{"hello", "world"}), f.sink->lastPlainLines());
    ASSERT_EQ(5, f.sink->lastFrame().columns);
}

TEST(TerminalRoot, laysOutRowsOfLabels) {
    TerminalTreeFixture f;
    f.set(f.rootNode, "flexDirection", text("row"));
    addLabel(f, f.rootNode, "left");
    auto spacer = f.createNode("SCValdiView");
    f.set(spacer, "width", Value(3.0));
    f.append(f.rootNode, spacer);
    addLabel(f, f.rootNode, "right");
    render(f);
    ASSERT_EQ((Lines{"left   right"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, singleLineLabelsTruncateWithAnEllipsisByDefault) {
    TerminalTreeFixture f;
    f.root->setSize(10, 10);
    auto label = f.createNode("SCValdiLabel");
    f.set(label, "value", text("hello wonderful world"));
    f.append(f.rootNode, label);
    render(f);
    ASSERT_EQ((Lines{"hello won…"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, clipsChildrenOfViewsWithHiddenOverflow) {
    TerminalTreeFixture f;
    auto box = f.createNode("SCValdiView");
    f.set(box, "width", Value(5.0));
    f.set(box, "height", Value(1.0));
    f.set(box, "overflow", text("hidden"));
    auto label = f.createNode("SCValdiLabel");
    f.set(label, "value", text("abcdefghij"));
    f.set(label, "width", Value(10.0));
    f.append(box, label);
    f.append(f.rootNode, box);
    render(f);
    ASSERT_EQ((Lines{"abcde"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, appliesLabelAndViewStylesFromAttributes) {
    TerminalTreeFixture f;
    f.sink->setColorLevel(ColorLevel::Basic16);
    auto box = f.createNode("SCValdiView");
    f.set(box, "backgroundColor", Value(static_cast<int64_t>(Color(0, 0, 255, 255).value)));
    auto label = addLabel(f, box, "hey", 1);
    f.set(label, "color", Value(static_cast<int64_t>(Color(255, 0, 0, 255).value)));
    f.set(label, "font", text("AvenirNext-Bold 16"));
    f.append(f.rootNode, box);
    render(f);

    const auto frame = f.sink->lastFrame();
    ASSERT_EQ(1u, frame.ansiLines.size());
    ASSERT_EQ("\x1b[1;91;104mhey\x1b[0m\x1b[104m" + std::string(17, ' ') + "\x1b[0m", frame.ansiLines[0]);
    ASSERT_EQ((Lines{"hey"}), frame.plainLines);
    ASSERT_EQ(frame.ansiLines[0], frame.ansi);
}

TEST(TerminalRoot, honorsTextAlignAndNumberOfLinesAttributes) {
    TerminalTreeFixture f;
    f.root->setSize(7, 10);
    auto label = addLabel(f, f.rootNode, "one two three", 2);
    f.set(label, "textAlign", text("right"));
    render(f);
    ASSERT_EQ((Lines{"one two", "  three"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, fitTerminalFillsTheTerminalHeight) {
    TerminalRootOptions options;
    options.fitTerminal = true;
    TerminalTreeFixture f(options);
    f.sink->setSize(20, 4);
    f.root->setSize(20, 4);
    addLabel(f, f.rootNode, "hi");
    render(f);
    ASSERT_EQ(4, f.sink->lastFrame().height);
    ASSERT_EQ((Lines{"hi", "", "", ""}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, contentHeightFramesAreAsTallAsTheirContent) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi");
    render(f);
    ASSERT_EQ(1, f.sink->lastFrame().height);
}

TEST(TerminalRoot, rendersStaticChildrenOnceAboveTheLiveFrame) {
    TerminalTreeFixture f;
    auto statics = f.createNode("ValdiTerminalStatic");
    f.set(statics, "position", text("absolute"));
    f.set(statics, "width", Value(20.0));
    addLabel(f, statics, "one", 1);
    addLabel(f, statics, "two", 1);
    f.append(f.rootNode, statics);
    addLabel(f, f.rootNode, "live", 1);

    render(f);
    ASSERT_EQ((Lines{"live"}), f.sink->lastPlainLines());
    const auto firstWrites = f.sink->writes().size();
    ASSERT_NE(std::string::npos, f.sink->output().find("one\ntwo\nlive\n"));

    addLabel(f, statics, "three", 1);
    render(f);
    ASSERT_EQ(firstWrites + 1, f.sink->writes().size());
    const auto output = f.sink->writes().back();
    ASSERT_NE(std::string::npos, output.find("three\nlive\n"));
    ASSERT_EQ(std::string::npos, output.find("one"));
    ASSERT_EQ(std::string::npos, output.find("two"));

    render(f);
    ASSERT_EQ(firstWrites + 1, f.sink->writes().size());
}

TEST(TerminalRoot, scrollsChildrenByTheContentOffset) {
    TerminalTreeFixture f;
    auto scroll = f.createNode("SCValdiScrollView");
    f.set(scroll, "width", Value(5.0));
    f.set(scroll, "height", Value(2.0));
    for (const char* row : {"row0", "row1", "row2", "row3"}) {
        addLabel(f, scroll, row, 1);
    }
    f.append(f.rootNode, scroll);
    render(f);
    ASSERT_EQ((Lines{"row0", "row1"}), f.sink->lastPlainLines());

    f.set(scroll, "contentOffsetY", Value(1.0));
    render(f);
    ASSERT_EQ((Lines{"row1", "row2"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, ignoresAttributesTheBackendDoesNotKnow) {
    TerminalTreeFixture f;
    auto label = addLabel(f, f.rootNode, "hi", 1);
    f.set(label, "someFutureAttribute", Value(1.0));
    f.set(label, "letterSpacing", Value(2.0));
    f.set(label, "opacity", Value(0.5));
    render(f);
    ASSERT_EQ((Lines{"hi"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, laysOutForTheSinkSizeWhenAttached) {
    TerminalRootOptions options;
    options.columns = 5;
    TerminalTreeFixture f(options);
    addLabel(f, f.rootNode, "hello world");
    render(f);
    ASSERT_EQ((Lines{"hello", "world"}), f.sink->lastPlainLines());
    ASSERT_EQ(5, f.sink->lastFrame().columns);
}

TEST(TerminalRoot, coalescesRepaintRequestsIntoOnePaintPerTurn) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    render(f);
    f.mainQueue->flush();
    const auto before = f.sink->frameCount();

    f.root->scheduleFrame();
    f.root->scheduleFrame();
    f.root->scheduleFrame();
    ASSERT_EQ(before, f.sink->frameCount());
    f.mainQueue->flush();
    ASSERT_EQ(before + 1, f.sink->frameCount());

    f.root->scheduleFrame();
    f.mainQueue->flush();
    ASSERT_EQ(before + 2, f.sink->frameCount());
}

TEST(TerminalRoot, viewMutationsScheduleARepaint) {
    TerminalTreeFixture f;
    auto label = addLabel(f, f.rootNode, "before", 1);
    render(f);
    f.mainQueue->flush();
    const auto before = f.sink->frameCount();

    f.set(label, "value", text("after"));
    f.mainQueue->flush();
    ASSERT_GT(f.sink->frameCount(), before);
    f.layout();
    f.root->paintNow();
    ASSERT_EQ((Lines{"after"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, usesAnInjectedDispatcher) {
    std::vector<DispatchFunction> queued;
    TerminalRootOptions options;
    options.dispatcher = [&queued](DispatchFunction function) { queued.push_back(std::move(function)); };
    TerminalTreeFixture f(options);
    addLabel(f, f.rootNode, "hi", 1);
    render(f);
    for (auto& function : queued) {
        function();
    }
    queued.clear();
    const auto before = f.sink->frameCount();

    f.root->scheduleFrame();
    f.root->scheduleFrame();
    ASSERT_EQ(1u, queued.size());
    ASSERT_EQ(before, f.sink->frameCount());
    queued[0]();
    ASSERT_EQ(before + 1, f.sink->frameCount());
}

TEST(TerminalRoot, notifiesFrameObserversUntilTheyAreRemoved) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    std::vector<Lines> seen;
    const auto id = f.root->addFrameObserver(
        TerminalFrameObserver([&seen](const FrameSnapshot& frame) { seen.push_back(frame.plainLines); }));
    render(f);
    ASSERT_EQ(1u, seen.size());
    ASSERT_EQ((Lines{"hi"}), seen[0]);

    f.root->removeFrameObserver(id);
    f.root->paintNow();
    ASSERT_EQ(1u, seen.size());
    ASSERT_EQ((Lines{"hi"}), f.root->getLastFrame().plainLines);
}

TEST(TerminalRoot, writesOnlyTheFinalFrameToANonInteractiveSink) {
    TerminalTreeFixture f;
    f.sink->setIsTTY(false);
    auto label = addLabel(f, f.rootNode, "first", 1);
    render(f);
    f.set(label, "value", text("second"));
    render(f);
    ASSERT_EQ("", f.sink->output());
    f.root->finish();
    ASSERT_EQ("second\n", f.sink->output());
}

TEST(TerminalRoot, ignoresRepaintRequestsAfterDestroy) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    render(f);
    const auto before = f.sink->frameCount();
    f.root->destroy();
    ASSERT_TRUE(f.root->isDestroyed());
    f.root->scheduleFrame();
    f.root->paintNow();
    f.mainQueue->flush();
    ASSERT_EQ(before, f.sink->frameCount());
}

} // namespace ValdiTest
