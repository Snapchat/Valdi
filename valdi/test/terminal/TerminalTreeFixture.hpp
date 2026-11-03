#pragma once

#include "valdi/runtime/Attributes/AttributesApplier.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/GlobalViewFactories.hpp"
#include "valdi/runtime/Views/ViewTransactionScope.hpp"
#include "valdi/standalone_runtime/InMemoryDiskCache.hpp"
#include "valdi/standalone_runtime/InMemoryKeychain.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/terminal/Host/TerminalRoot.hpp"
#include "valdi/terminal/Sink/MemorySink.hpp"
#include "valdi/terminal/TerminalRuntime.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace ValdiTest {

/**
 * A terminal runtime on a JS-less RuntimeManager with a ViewNodeTree whose updates are driven by
 * hand, the same way the ViewNode tests drive the standalone view manager.
 */
class TerminalTreeFixture {
public:
    explicit TerminalTreeFixture(Valdi::TerminalRootOptions options = Valdi::TerminalRootOptions())
        : mainQueue(Valdi::makeShared<Valdi::StandaloneMainQueue>()),
          runtimeManager(
              Valdi::makeShared<Valdi::RuntimeManager>(mainQueue->createMainThreadDispatcher(),
                                                       nullptr,
                                                       Valdi::makeShared<Valdi::InMemoryDiskCache>(),
                                                       Valdi::makeShared<Valdi::InMemoryKeychain>(),
                                                       nullptr,
                                                       Valdi::PlatformTypeLinux,
                                                       Valdi::ThreadQoSClassNormal,
                                                       Valdi::strongSmallRef(&Valdi::ConsoleLogger::getLogger()),
                                                       /* enableDebuggerService */ false,
                                                       /* disableHotReloader */ false,
                                                       /* isStandalone */ true)),
          terminalRuntime(Valdi::makeShared<Valdi::TerminalRuntime>(*runtimeManager)),
          viewManagerContext(terminalRuntime->getViewManagerContext()),
          sink(Valdi::makeShared<Valdi::Terminal::MemorySink>()) {
        auto& mainThreadManager = runtimeManager->getMainThreadManager();
        mainThreadManager.markCurrentThreadIsMainThread();

        tree = Valdi::makeShared<Valdi::ViewNodeTree>(
            Valdi::makeShared<Valdi::Context>(0, Valdi::strongSmallRef(&Valdi::ConsoleLogger::getLogger())),
            viewManagerContext,
            &terminalRuntime->getViewManager(),
            nullptr,
            &mainThreadManager,
            false);
        lock = tree->lock();
        scope = Valdi::makeShared<Valdi::ViewTransactionScope>(&terminalRuntime->getViewManager(), nullptr, false);
        tree->unsafeSetCurrentViewTransactionScope(scope);
        disableUpdates = tree->beginDisableUpdates();
        tree->unsafeBeginViewTransaction();

        sink->setSize(20, 10);
        root = Valdi::makeShared<Valdi::TerminalRoot>(tree, sink, std::move(options));
        root->attach();

        rootNode = createNode("SCValdiView");
        tree->setRootViewNode(rootNode, false);
    }

    ~TerminalTreeFixture() {
        if (root != nullptr) {
            root->destroy();
        }
    }

    Valdi::Ref<Valdi::ViewNode> createNode(const char* className) {
        auto& attributesManager = viewManagerContext->getAttributesManager();
        auto node =
            Valdi::makeShared<Valdi::ViewNode>(attributesManager.getYogaConfig(),
                                               attributesManager.getAttributeIds(),
                                               attributesManager.getColorPaletteManager()->getActiveColorPalette(),
                                               attributesManager.getLogger());
        node->setViewNodeTree(tree.get());
        node->setViewFactory(*scope,
                             viewManagerContext->getGlobalViewFactories()->getViewFactory(
                                 Valdi::StringCache::getGlobal().makeString(std::string_view(className))));
        // What the node renderer does for every node of a runtime configured with TerminalRuntime::configureRuntime().
        node->setLimitToViewport(Valdi::LimitToViewportDisabled);
        // Nodes are only weakly held by their parents, so the fixture keeps them alive.
        nodes.push_back(node);
        return node;
    }

    void set(const Valdi::Ref<Valdi::ViewNode>& node, const char* attribute, const Valdi::Value& value) {
        auto id =
            viewManagerContext->getAttributesManager().getAttributeIds().getIdForName(std::string_view(attribute));
        node->setAttribute(*scope, id, Valdi::AttributeOwner::getNativeOverridenAttributeOwner(), value, nullptr);
        node->getAttributesApplier().flush(*scope);
    }

    void append(const Valdi::Ref<Valdi::ViewNode>& parent, const Valdi::Ref<Valdi::ViewNode>& child) {
        parent->appendChild(*scope, child);
    }

    /** Recalculates layout and applies the resulting frames to the views. */
    void layout() {
        tree->performUpdates();
    }

    Valdi::Ref<Valdi::StandaloneMainQueue> mainQueue;
    Valdi::Ref<Valdi::RuntimeManager> runtimeManager;
    Valdi::Ref<Valdi::TerminalRuntime> terminalRuntime;
    Valdi::Ref<Valdi::ViewManagerContext> viewManagerContext;
    Valdi::Ref<Valdi::Terminal::MemorySink> sink;
    Valdi::Ref<Valdi::ViewNodeTree> tree;
    Valdi::Ref<Valdi::ViewTransactionScope> scope;
    Valdi::TrackedLock lock;
    Valdi::ViewNodeTreeDisableUpdates disableUpdates;
    Valdi::Ref<Valdi::TerminalRoot> root;
    Valdi::Ref<Valdi::ViewNode> rootNode;
    std::vector<Valdi::Ref<Valdi::ViewNode>> nodes;
};

} // namespace ValdiTest
