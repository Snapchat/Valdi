#include "TerminalTreeFixture.hpp"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

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

TEST(TerminalRoot, numbersFramesInIncreasingOrder) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    std::vector<uint64_t> sequences;
    const auto id = f.root->addFrameObserver(
        TerminalFrameObserver([&sequences](const FrameSnapshot& frame) { sequences.push_back(frame.sequence); }));
    render(f);
    f.root->paintNow();
    f.root->paintNow();
    // The fixture finishes the root when it is destroyed, which paints once more.
    f.root->removeFrameObserver(id);

    ASSERT_EQ(3u, sequences.size());
    ASSERT_EQ(1u, sequences[0]);
    ASSERT_LT(sequences[0], sequences[1]);
    ASSERT_LT(sequences[1], sequences[2]);
    ASSERT_EQ(sequences[2], f.root->getLastFrame().sequence);
    ASSERT_EQ(sequences[2], f.sink->lastFrame().sequence);
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
    f.root->destroy();
    const auto before = f.sink->frameCount();
    ASSERT_TRUE(f.root->isDestroyed());
    f.root->scheduleFrame();
    f.root->paintNow();
    f.mainQueue->flush();
    ASSERT_EQ(before, f.sink->frameCount());
}

TEST(TerminalRoot, aPendingPaintAfterDestroyWritesNothing) {
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

    f.root->scheduleFrame();
    ASSERT_EQ(1u, queued.size());
    f.root->destroy();
    const auto writes = f.sink->writes().size();
    const auto frames = f.sink->frameCount();
    queued[0]();
    ASSERT_EQ(writes, f.sink->writes().size());
    ASSERT_EQ(frames, f.sink->frameCount());
}

TEST(TerminalRoot, observersRunWithoutHoldingTheRootsLocks) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    std::atomic<bool> workerDone{false};
    bool workerFinishedWhileObserving = false;
    std::thread worker;
    const auto observerId = f.root->addFrameObserver(TerminalFrameObserver([&](const FrameSnapshot&) {
        // Same thread: re-entering the root from an observer works.
        f.root->getLastFrame();
        f.root->scheduleFrame();

        // Another thread needs the root's mutex while this observer is still running.
        worker = std::thread([&]() {
            f.root->getLastFrame();
            f.root->scheduleFrame();
            workerDone = true;
        });
        for (int i = 0; i < 300 && !workerDone; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        workerFinishedWhileObserving = workerDone;
    }));
    render(f);
    // destroy() paints a last frame; the observer must not run again once its captures are gone.
    f.root->removeFrameObserver(observerId);
    if (worker.joinable()) {
        worker.join();
    }
    ASSERT_TRUE(workerFinishedWhileObserving);
}

TEST(TerminalRoot, skipsObserversRemovedByAnEarlierObserverOfTheSameFrame) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    int removedCalls = 0;
    int removedId = 0;
    f.root->addFrameObserver(
        TerminalFrameObserver([&](const FrameSnapshot&) { f.root->removeFrameObserver(removedId); }));
    removedId = f.root->addFrameObserver(TerminalFrameObserver([&](const FrameSnapshot&) { removedCalls++; }));
    render(f);
    ASSERT_EQ(0, removedCalls);
}

TEST(TerminalRoot, destroyFinishesTheOutputFirstAndLaterFinishIsANoOp) {
    TerminalTreeFixture f;
    f.sink->setIsTTY(false);
    auto label = addLabel(f, f.rootNode, "first", 1);
    render(f);
    f.set(label, "value", text("second"));
    render(f);
    ASSERT_EQ("", f.sink->output());
    f.root->destroy();
    ASSERT_EQ("second\n", f.sink->output());
    f.root->finish();
    f.root->destroy();
    ASSERT_EQ("second\n", f.sink->output());
}

TEST(TerminalRoot, destroyShowsTheCursorAgainOnAnInteractiveSink) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    render(f);
    ASSERT_NE(std::string(Ansi::kCursorShow), f.sink->writes().back());
    f.root->destroy();
    ASSERT_EQ(std::string(Ansi::kCursorShow), f.sink->writes().back());
}

TEST(TerminalRoot, destroyingTheLastReferenceFinishesTheOutput) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "hi", 1);
    render(f);
    f.root = nullptr;
    ASSERT_EQ(std::string(Ansi::kCursorShow), f.sink->writes().back());
}

TEST(TerminalRoot, keepsTheViewManagerAliveAfterTheRuntimeIsReleased) {
    // Declared first so it outlives the fixture, whose tree still refers to the manager while it is torn down.
    Ref<TerminalViewManager> keptForTeardown;
    TerminalTreeFixture f;
    keptForTeardown = strongRef(&f.terminalRuntime->getViewManager());
    Weak<TerminalViewManager> weakManager = weakRef(keptForTeardown.get());
    keptForTeardown = nullptr;
    ASSERT_NE(nullptr, weakManager.lock());

    // Only the runtime and the root hold the manager; dropping the runtime must not free it.
    f.terminalRuntime = nullptr;
    keptForTeardown = weakManager.lock();
    ASSERT_NE(nullptr, keptForTeardown);
}

TEST(TerminalRoot, dropsFrameRequestsWhenThereIsNoMainThreadManagerOrDispatcher) {
    TerminalTreeFixture f;
    auto tree = makeShared<ViewNodeTree>(
        f.tree->getContext(), f.viewManagerContext, &f.terminalRuntime->getViewManager(), nullptr, nullptr, false);
    auto sink = makeShared<MemorySink>();
    auto root = makeShared<TerminalRoot>(tree, sink, TerminalRootOptions());
    root->scheduleFrame();
    root->scheduleFrame();
    ASSERT_EQ(0u, sink->frameCount());
    ASSERT_EQ(0u, sink->writes().size());
    root->destroy();
}

TEST(TerminalRoot, aDispatcherThatDropsTheFrameDoesNotBlockLaterRequests) {
    int dispatches = 0;
    TerminalRootOptions options;
    options.dispatcher = [&dispatches](DispatchFunction /*dropped*/) { dispatches++; };
    TerminalTreeFixture f(options);
    const int baseline = dispatches;
    f.root->scheduleFrame();
    ASSERT_EQ(baseline + 1, dispatches);
    f.root->scheduleFrame();
    ASSERT_EQ(baseline + 2, dispatches);
}

TEST(TerminalRoot, borderAttributeAcceptsAWidthWithoutAColor) {
    TerminalTreeFixture f;
    f.sink->setColorLevel(ColorLevel::TrueColor);
    auto box = f.createNode("SCValdiView");
    f.set(box, "width", Value(4.0));
    f.set(box, "height", Value(3.0));
    f.set(box, "border", Value(1.0));
    f.append(f.rootNode, box);
    render(f);
    ASSERT_EQ((Lines{"┌──┐", "│  │", "└──┘"}), f.sink->lastPlainLines());
    ASSERT_EQ(f.sink->lastFrame().plainLines[0], f.sink->lastFrame().ansiLines[0]);
}

TEST(TerminalRoot, borderAttributeAcceptsTheShorthandString) {
    TerminalTreeFixture f;
    f.sink->setColorLevel(ColorLevel::TrueColor);
    auto box = f.createNode("SCValdiView");
    f.set(box, "width", Value(4.0));
    f.set(box, "height", Value(3.0));
    f.set(box, "border", text("1 solid red"));
    f.append(f.rootNode, box);
    render(f);
    ASSERT_EQ((Lines{"┌──┐", "│  │", "└──┘"}), f.sink->lastPlainLines());
    ASSERT_NE(std::string::npos, f.sink->lastFrame().ansiLines[0].find("38;2;255;0;0"));
}

TEST(TerminalRoot, labelPaddingInsetsTheTextAndCountsTowardTheHeight) {
    TerminalTreeFixture f;
    auto label = addLabel(f, f.rootNode, "hi", 1);
    f.set(label, "padding", Value(1.0));
    render(f);
    ASSERT_EQ(3, f.sink->lastFrame().height);
    ASSERT_EQ((Lines{"", " hi", ""}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, labelsWrapAtTheWidthInsideTheirPadding) {
    TerminalTreeFixture f;
    f.root->setSize(7, 10);
    auto label = addLabel(f, f.rootNode, "ab cd ef", 0);
    f.set(label, "padding", Value(1.0));
    render(f);
    ASSERT_EQ((Lines{"", " ab cd", " ef", ""}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, clippedSingleLineLabelsCutAtTheWidthWithoutAnEllipsis) {
    TerminalTreeFixture f;
    f.root->setSize(8, 10);
    auto label = f.createNode("SCValdiLabel");
    f.set(label, "value", text("hello wonderful world"));
    f.set(label, "textOverflow", text("clip"));
    f.append(f.rootNode, label);
    render(f);
    ASSERT_EQ((Lines{"hello wo"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, writesQueuedLinesAboveTheLiveFrameOnce) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "live", 1);
    render(f);

    f.root->writeAbove({"log 1", "log 2"});
    f.mainQueue->flush();
    f.root->paintNow();

    const auto output = f.sink->output();
    ASSERT_NE(std::string::npos, output.find("log 1\nlog 2\n"));
    ASSERT_EQ(output.find("log 1"), output.rfind("log 1"));
    ASSERT_EQ((Lines{"live"}), f.sink->lastPlainLines());
}

TEST(TerminalRoot, clearErasesTheLiveFrameFromTheSink) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "live", 1);
    render(f);
    const auto before = f.sink->output().size();
    f.root->clear();
    ASSERT_GT(f.sink->output().size(), before);
}

TEST(TerminalRoot, clearAndWriteAboveAreNoOpsAfterFinish) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "live", 1);
    render(f);
    f.root->finish();
    const auto before = f.sink->output();

    f.root->clear();
    f.root->writeAbove({"late"});
    f.mainQueue->flush();
    f.root->paintNow();

    ASSERT_EQ(before, f.sink->output());
}

TEST(TerminalRoot, clearAndWriteAboveAreNoOpsAfterDestroy) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "live", 1);
    render(f);
    f.root->destroy();
    const auto before = f.sink->output();

    f.root->clear();
    f.root->writeAbove({"late"});
    f.mainQueue->flush();

    ASSERT_EQ(before, f.sink->output());
}

TEST(TerminalRoot, writeAboveQueuedBeforeFinishIsFlushedByTheFinalFrame) {
    TerminalTreeFixture f;
    addLabel(f, f.rootNode, "live", 1);
    render(f);
    f.root->writeAbove({"last words"});
    f.root->finish();
    ASSERT_NE(std::string::npos, f.sink->output().find("last words\n"));
}

} // namespace ValdiTest
