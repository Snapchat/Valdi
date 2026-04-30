#include "valdi_test_utils.hpp"
#include "valdi/standalone_runtime/Arguments.hpp"
#include "valdi/standalone_runtime/ValdiStandaloneMain.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Threading/Thread.hpp"

#include <cstdlib>

using namespace Valdi;

namespace ValdiTest {

DummyView::DummyView(const char* className) : DummyView(Valdi::makeShared<StandaloneView>(STRING_LITERAL(className))) {}

DummyView::DummyView(Ref<StandaloneView> impl) : _impl(impl) {}

Valdi::Value DummyView::getAttribute(const char* attribute) {
    return _impl->getAttribute(STRING_LITERAL(attribute));
}

Valdi::Ref<Valdi::LoadedAsset> DummyView::getLoadedAsset() const {
    return _impl->getLoadedAsset();
}

size_t DummyView::getChildrenCount() const {
    return _impl->getChildren().size();
}

DummyView DummyView::getChild(size_t index) {
    return DummyView(_impl->getChild(index));
}

DummyView& DummyView::addChild(DummyView child) {
    _impl->addChild(child._impl);

    return *this;
}

DummyView& DummyView::addAttribute(const char* name, const char* value) {
    _impl->setAttribute(STRING_LITERAL(name), Valdi::Value(STRING_LITERAL(value)));

    return *this;
}

StandaloneView& DummyView::getImpl() const {
    return *_impl;
}

const Ref<StandaloneView>& DummyView::getSharedImpl() const {
    return _impl;
}

void DummyView::setTag(const char* tag) {
    addAttribute("tag", tag);
}

std::string DummyView::getTag() {
    return getAttribute("tag").toString();
}

std::string_view DummyView::getClassName() {
    return _impl->getViewClassName().toStringView();
}

bool DummyView::isRightToLeft() const {
    return _impl->isRightToLeft();
}

DummyView DummyView::clone() const {
    return DummyView(Valdi::makeShared<StandaloneView>(*_impl));
}

std::ostream& operator<<(std::ostream& os, const DummyView& dummyView) {
    return os << "\n" << dummyView.getImpl().toXML();
}

bool operator!=(const DummyView& left, const DummyView& right);

bool operator==(const DummyView& left, const DummyView& right) {
    return left.getImpl() == right.getImpl();
}

bool operator!=(const DummyView& left, const DummyView& right) {
    return !(left == right);
}

DummyView getDummyView(const Valdi::Ref<Valdi::View>& view) {
    return DummyView(StandaloneView::unwrap(view));
}

DummyView getRootView(Valdi::SharedViewNodeTree& viewNodeTree) {
    auto rootView = viewNodeTree->getRootView();
    if (rootView == nullptr) {
        throw Valdi::Exception("No root view node in that context");
    }
    return getDummyView(rootView);
}

Valdi::ViewNodePath parseNodePath(std::string_view nodePath) {
    auto result = Valdi::ViewNodePath::parse(nodePath);
    if (!result) {
        throw Valdi::Exception(result.error().toStringBox());
    }
    return result.value();
}

Valdi::Ref<Valdi::ViewNode> getViewNodeForId(const Valdi::SharedViewNodeTree& viewNodeTree, std::string id) {
    return viewNodeTree->getViewNodeForNodePath(parseNodePath(id));
}

DummyView getViewForId(const Valdi::SharedViewNodeTree& viewNodeTree, std::string id) {
    auto viewNode = getViewNodeForId(viewNodeTree, id);
    if (viewNode == nullptr) {
        return getDummyView(nullptr);
    }

    return getDummyView(viewNode->getViewAndDisablePooling());
}

Valdi::Path resolveTestPath(const std::string& path) {
    char cwdBuffer[PATH_MAX];
    (void)::getcwd(cwdBuffer, PATH_MAX);

    auto basePath = Valdi::Path(cwdBuffer);

#ifndef BUCK_TEST
    basePath.append("src");
#endif
    basePath.append("valdi");
    basePath.append(path);
    basePath.normalize();

    return basePath;
}

Valdi::Path resolveOpenSourceTestPath(const std::string& path) {
    // Bazel sets TEST_SRCDIR to the runfiles root for tests. Data paths are runfiles_root/valdi/<path>.
    const char* testSrcdir = std::getenv("TEST_SRCDIR");
    if (testSrcdir != nullptr) {
        auto basePath = Valdi::Path(testSrcdir);
        basePath.append("valdi");
        basePath.append(path);
        basePath.normalize();
        return basePath;
    }
    const char* runfilesDir = std::getenv("RUNFILES_DIR");
    if (runfilesDir != nullptr) {
        auto basePath = Valdi::Path(runfilesDir);
        basePath.append("valdi");
        basePath.append(path);
        basePath.normalize();
        return basePath;
    }
    char cwdBuffer[PATH_MAX];
    (void)::getcwd(cwdBuffer, PATH_MAX);
    auto basePath = Valdi::Path(cwdBuffer);

    basePath.append("external/+local_repos+valdi/valdi");
    basePath.append(path);
    basePath.normalize();
    return basePath;
}

Valdi::Result<Valdi::BytesView> loadFileFromAbsolutePath(const Valdi::Path& absolutePath) {
    return Valdi::DiskUtils::load(absolutePath);
}

Valdi::Result<Valdi::BytesView> loadResourceFromDisk(const std::string& path) {
    auto result = loadFileFromAbsolutePath(resolveOpenSourceTestPath(path));
    if (result) {
        return result;
    }
    // Fallbacks: Bazel may run tests with cwd = runfiles root without setting TEST_SRCDIR.
    char cwdBuffer[PATH_MAX];
    if (::getcwd(cwdBuffer, PATH_MAX) != nullptr) {
        Valdi::Path runfilesPath(cwdBuffer);
        runfilesPath.append("valdi");
        runfilesPath.append(path);
        runfilesPath.normalize();
        auto fallback = loadFileFromAbsolutePath(runfilesPath);
        if (fallback) {
            return fallback;
        }
        // cwd might be runfiles/valdi (package dir) already
        Valdi::Path pkgPath(cwdBuffer);
        pkgPath.append(path);
        pkgPath.normalize();
        fallback = loadFileFromAbsolutePath(pkgPath);
        if (fallback) {
            return fallback;
        }
    }
    return result;
}

Valdi::FlatMap<Valdi::Path, Valdi::BytesView> readDirectory(const std::string& path) {
    auto absolutePath = resolveOpenSourceTestPath(path);
    Valdi::FlatMap<Valdi::Path, Valdi::BytesView> fileByPath;

    for (const auto& path : DiskUtils::listDirectory(absolutePath)) {
        auto bytes = loadFileFromAbsolutePath(path);
        if (bytes) {
            fileByPath.try_emplace(std::move(path), bytes.moveValue());
        }
    }

    return fileByPath;
}

void MainQueue::runOnce() {
    runOnce("Running a the main loop a single time");
}

void MainQueue::runOnce(const char* desc) {
    auto count = 0;
    runUntilTrue(desc, [&]() {
        count++;
        return count != 1;
    });
}

void MainQueue::flush() {
    while (runNextTask()) {
        numberOfExecutedTasks++;
    }
}

void MainQueue::stop() {
    dispose();
}

void RuntimeListener::onContextCreated(Valdi::Runtime& runtime, const Valdi::SharedContext& context) {}

void RuntimeListener::onContextDestroyed(Valdi::Runtime& runtime, Valdi::Context& context) {}

void RuntimeListener::onContextRendered(Valdi::Runtime& runtime, const Valdi::SharedContext& context) {
    numberOfRenders++;

    setLayoutSpecsToContextIfNeeded(runtime, context);
}

void RuntimeListener::onContextLayoutBecameDirty(Valdi::Runtime& runtime, const Valdi::SharedContext& context) {
    numberOfLayoutBecameDirty++;
}

void RuntimeListener::onAllContextsDestroyed(Valdi::Runtime& runtime) {}

void RuntimeListener::setLayoutSpecsToContextIfNeeded(Valdi::Runtime& runtime, const Valdi::SharedContext& context) {
    if (!layoutTreeAutomatically) {
        return;
    }

    auto tree = runtime.getViewNodeTreeManager().getViewNodeTreeForContextId(context->getContextId());
    if (tree != nullptr && tree->getRootView() != nullptr) {
        auto& rootView = getDummyView(tree->getRootView()).getImpl();

        if (rootView.isLayoutInvalidated()) {
            rootView.setLayoutInvalidated(false);
            tree->setLayoutSpecs(rootView.getFrame().size(), Valdi::LayoutDirectionLTR);
        }
    }
}

std::vector<Valdi::ViewNode*> findViewNodesWithId(const Valdi::SharedViewNode& viewNode, const char* nodeId) {
    auto nodeIdStr = STRING_LITERAL(nodeId);
    std::vector<Valdi::ViewNode*> outNodes;
    forEachViewNode(viewNode, [&](const auto& viewNode) {
        if (viewNode->getDebugId() == nodeIdStr) {
            outNodes.push_back(viewNode);
        }
    });

    return outNodes;
}

void assertHasNoView(Valdi::ViewNode* viewNode) {
    forEachViewNode(viewNode, [&](const auto& node) {
        if (node->hasView()) {
            throw Valdi::Exception(fmt::format("Node HAS a view but SHOULDNT: \n{}", node->toXML()));
        }
    });
}

void assertHasNoView(const Valdi::SharedViewNode& viewNode) {
    assertHasNoView(viewNode.get());
}

void assertAllHasView(Valdi::ViewNode* viewNode) {
    forEachViewNode(viewNode, [&](const auto& node) {
        if (!node->hasView()) {
            throw Valdi::Exception(fmt::format("Node has NOT a view but SHOULD: \n{}", node->toXML()));
        }
    });
}

void assertAllHasView(const Valdi::SharedViewNode& viewNode) {
    assertAllHasView(viewNode.get());
}

int getNumberOfPooledViews(const StringBox& viewClassName, const ViewPoolsStats& stats) {
    const auto& it = stats.find(viewClassName);
    if (it == stats.end()) {
        return 0;
    }
    return static_cast<int>(it->second);
}

void runParallel(size_t threadsCount, int64_t durationSeconds, DispatchFunction run) {
    if (threadsCount > 1) {
        std::vector<Ref<Thread>> threads;
        std::atomic_bool cont = true;

        for (size_t i = 0; i < 8; i++) {
            auto thread = Thread::create(STRING_LITERAL("Worker Thread"), ThreadQoSClassMax, [&]() {
                while (cont) {
                    run();
                }
            });

            SC_ASSERT(thread);

            threads.emplace_back(thread.value());
        }

        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));

        cont = false;

        for (const auto& thread : threads) {
            thread->join();
        }
    } else {
        snap::utils::time::StopWatch sw;
        sw.start();

        while (sw.elapsedSeconds() < durationSeconds) {
            run();
        }
    }
}

bool runTypeScriptTests(std::string_view modulePath,
                        Valdi::IJavaScriptBridge* jsBridge,
                        std::shared_ptr<snap::valdi_core::ModuleFactoriesProvider> moduleFactoriesProvider) {
    Valdi::StandaloneArguments arguments;
    arguments.scriptPath = STRING_LITERAL("valdi_standalone/src/TestsRunner");

    arguments.jsArguments.emplace_back(STRING_LITERAL("./run_test"));
    arguments.jsArguments.emplace_back(STRING_LITERAL("--include"));
    arguments.jsArguments.emplace_back(Valdi::StringCache::getGlobal().makeString(modulePath));

    if (moduleFactoriesProvider != nullptr) {
        arguments.moduleFactoriesProviders.emplace_back(std::move(moduleFactoriesProvider));
    }
    arguments.jsBridge = jsBridge;

    auto result = runValdiStandalone(arguments);

    return result == 0;
}

} // namespace ValdiTest
