//
//  BenchViewTree.hpp
//  valdi
//
//  Shared fixture for the layout/render hot-path microbenchmarks. Stands up a
//  headless ViewNodeTree (no JS engine, no platform integration) via the
//  standalone view manager, and provides a small declarative Element builder
//  that lowers into a RenderRequest for ViewNodeRenderer. This mirrors how the
//  runtime applies a render request produced by the JS side, but without the
//  JS round-trip, so the benchmarks isolate the C++ layout/tree-diff cost.
//
//  This header defines inline helpers only and is included by exactly one
//  translation unit per benchmark binary (layout_benchmark, render_benchmark),
//  so there is no ODR concern across the separate cc_binary targets.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi/runtime/Rendering/RenderRequestEntries.hpp"
#include "valdi/runtime/Rendering/ViewNodeRenderer.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneViewManager.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <utility>
#include <vector>

namespace valdi_bench {

// Owns the standalone view-manager stack a ViewNodeTree needs to render and lay
// out without a platform. The ViewManagerContext supplies the AttributesManager,
// ViewManager and MainThreadManager that ViewNodeRenderer reads.
struct BenchDependencies {
    Valdi::Ref<Valdi::StandaloneMainQueue> mainQueue;
    Valdi::Ref<Valdi::MainThreadManager> mainThreadManager;
    Valdi::StandaloneViewManager viewManager;
    Valdi::AttributeIds attributeIds;
    Valdi::Ref<Valdi::ViewManagerContext> viewManagerContext;

    BenchDependencies() {
        Valdi::ConsoleLogger::getLogger().setMinLogType(Valdi::LogTypeWarn);

        mainQueue = Valdi::makeShared<Valdi::StandaloneMainQueue>();
        mainThreadManager = Valdi::makeShared<Valdi::MainThreadManager>(mainQueue->createMainThreadDispatcher());
        mainThreadManager->markCurrentThreadIsMainThread();

        viewManagerContext =
            Valdi::makeShared<Valdi::ViewManagerContext>(viewManager,
                                                         attributeIds,
                                                         Valdi::makeShared<Valdi::ColorPaletteManager>(),
                                                         Valdi::Yoga::createConfig(0),
                                                         true,
                                                         mainThreadManager,
                                                         Valdi::ConsoleLogger::getLogger());
    }

    Valdi::Ref<Valdi::ViewNodeTree> createTree() {
        // A real Context is required: ViewNodeTree::withLock (reached via
        // measureLayout) enters the context and takes its attribution scope.
        // A fresh one per tree keeps trees independent.
        auto context = Valdi::makeShared<Valdi::Context>(0, Valdi::strongSmallRef(&Valdi::ConsoleLogger::getLogger()));
        return Valdi::makeShared<Valdi::ViewNodeTree>(context,
                                                      viewManagerContext,
                                                      /* viewManager */ &viewManager,
                                                      /* runtime */ nullptr,
                                                      mainThreadManager.get(),
                                                      /* shouldRenderInMainThread */ true);
    }

    void destroyTree(Valdi::Ref<Valdi::ViewNodeTree>& tree) {
        // removeViewNode reaches getCurrentViewTransactionScope, which asserts the
        // tree lock is held; withLock supplies the lock and transaction scope.
        tree->withLock([&]() {
            auto rootViewNode = tree->getRootViewNode();
            if (rootViewNode != nullptr) {
                tree->removeViewNode(rootViewNode->getRawId());
            }
        });
        tree = nullptr;
    }
};

// Declarative element tree, lowered into a RenderRequest below.
struct Element {
    Valdi::StringBox name;
    std::vector<std::pair<Valdi::StringBox, Valdi::Value>> attributes;
    std::vector<Element> children;

    explicit Element(const char* name) : name(STRING_LITERAL(name)) {}

    template<typename T>
    Element& attribute(const char* attrName, const T& value) {
        attributes.emplace_back(STRING_LITERAL(attrName), Valdi::Value(value));
        return *this;
    }

    Element& attribute(const char* attrName, const char* value) {
        return attribute(attrName, STRING_LITERAL(value));
    }

    Element& child(Element&& c) {
        children.emplace_back(std::move(c));
        return *this;
    }

    Element& child(const Element& c) {
        children.emplace_back(c);
        return *this;
    }

    Element& setChildren(const std::vector<Element>& c) {
        children = c;
        return *this;
    }
};

// Applies a RenderRequest to the tree. ViewNodeRenderer expects the tree lock
// held and a current view-transaction scope; ViewNodeTree::withLock provides
// both (and enters the context), so all rendering goes through it.
inline void renderUnderLock(Valdi::ViewNodeTree& tree, Valdi::RenderRequest& request) {
    tree.withLock([&]() {
        Valdi::ViewNodeRenderer renderer(tree, Valdi::ConsoleLogger::getLogger(), false);
        renderer.render(request);
    });
}

struct BenchRenderState {
    Valdi::RawViewNodeId elementId = 0;
    Valdi::AttributeIds& attributeIds;

    explicit BenchRenderState(BenchDependencies& deps) : attributeIds(deps.attributeIds) {}
};

inline Valdi::RawViewNodeId populateRenderRequest(Valdi::RenderRequest& request,
                                                  BenchRenderState& state,
                                                  const Element& element) {
    auto elementId = ++state.elementId;
    auto* createElement = request.appendCreateElement();
    createElement->setElementId(elementId);
    createElement->setViewClassName(element.name);

    for (const auto& attribute : element.attributes) {
        auto* setAttribute = request.appendSetElementAttribute();
        setAttribute->setElementId(elementId);
        setAttribute->setAttributeId(state.attributeIds.getIdForName(attribute.first));
        setAttribute->setAttributeValue(attribute.second);
    }

    std::vector<Valdi::RawViewNodeId> childrenIds;
    for (const auto& child : element.children) {
        childrenIds.emplace_back(populateRenderRequest(request, state, child));
    }

    size_t i = 0;
    for (const auto& childId : childrenIds) {
        auto* moveElementToParent = request.appendMoveElementToParent();
        moveElementToParent->setParentElementId(elementId);
        moveElementToParent->setElementId(childId);
        moveElementToParent->setParentIndex(static_cast<int>(i));
        i++;
    }

    if (elementId == 1) {
        request.appendSetRootElement()->setElementId(elementId);
    }

    return elementId;
}

// A representative non-trivial tree: a header plus 20 horizontally-scrolling
// rows of 100 text cards each (~2000 leaf nodes), so the layout pass exercises
// real flexbox nesting, wrapping text measurement and scroll containers.
inline Element createElementTree() {
    auto header =
        Element("view")
            .attribute("width", "100%")
            .attribute("height", 200)
            .attribute("backgroundColor", "white")
            .child(Element("view")
                       .attribute("margin", 16)
                       .attribute("backgroundColor", "lightgray")
                       .attribute("flexGrow", 1)
                       .attribute("borderRadius", 16)
                       .attribute("borderColor", "yellow")
                       .child(Element("label").attribute("value", "Header").attribute("textAlign", "center")));

    std::vector<Element> cards;
    for (size_t i = 0; i < 100; i++) {
        cards.emplace_back(Element("view")
                               .attribute("width", 40)
                               .attribute("height", "100%")
                               .attribute("background", "white")
                               .child(Element("label")
                                          .attribute("value", "Hello world what is up")
                                          .attribute("lineSpacing", 4)
                                          .attribute("font", "Title")
                                          .attribute("fontSize", 16)
                                          .attribute("color", "black")
                                          .attribute("textAlign", "center")
                                          .attribute("numberOfLines", 0)
                                          .attribute("marginTop", 8)
                                          .attribute("marginBottom", 16)));
    }

    std::vector<Element> cells;
    for (size_t i = 0; i < 20; i++) {
        cells.emplace_back(Element("view")
                               .attribute("flexGrow", 1)
                               .child(Element("scroll").attribute("horizontal", true).setChildren(cards)));
    }

    return Element("view")
        .attribute("width", 100)
        .attribute("height", 100)
        .child(std::move(header))
        .child(Element("scroll").attribute("flexGrow", 1).setChildren(cells));
}

} // namespace valdi_bench
