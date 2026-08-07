#include "benchmark_utils.hpp"
#include "valdi_test_utils.hpp"

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Rendering/ViewNodeRenderer.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

#include "valdi/standalone_runtime/StandaloneViewManager.hpp"

#include "valdi/runtime/Rendering/RenderRequest.hpp"

#include "valdi/runtime/Attributes/ViewNodeAttribute.hpp"

#include <benchmark/benchmark.h>

using namespace ValdiTest;
using namespace Valdi;

struct Dependencies {
    Dependencies() : mainQueue(), mainThreadManager(mainQueue), viewManager(), attributeIds() {
        viewManagerContext = makeShared<ViewManagerContext>(viewManager,
                                                            attributeIds,
                                                            makeShared<ColorPaletteManager>(),
                                                            Yoga::createConfig(0),
                                                            true,
                                                            mainThreadManager,
                                                            ConsoleLogger::getLogger());
    }

    ~Dependencies() {}

    Ref<ViewNodeTree> createTree() {
        return makeShared<ViewNodeTree>(nullptr, viewManagerContext, nullptr, mainThreadManager, true);
    }

    void destroyTree(Ref<ViewNodeTree>& tree) {
        auto rootViewNode = tree->getRootViewNode();
        if (rootViewNode != nullptr) {
            tree->removeViewNode(rootViewNode->getRawId());
            rootViewNode = nullptr;
        }
        tree = nullptr;
    }

    StandaloneMainQueue mainQueue;
    MainThreadManager mainThreadManager;
    StandaloneViewManager viewManager;
    AttributeIds attributeIds;
    Ref<ViewManagerContext> viewManagerContext;
    Ref<ViewNodeTree> tree;
};

struct RenderRequestBuilder {
    RenderRequestBuilder& element(const char* type);
};

struct Element {
    StringBox name;
    std::vector<std::pair<StringBox, Value>> attributes;
    std::vector<Element> children;

    Element(const char* name) : name(STRING_LITERAL(name)) {}

    template<typename T>
    Element& attribute(const char* name, const T& value) {
        attributes.emplace_back(std::make_pair(STRING_LITERAL(name), Value(value)));
        return *this;
    }

    Element& attribute(const char* name, const char* value) {
        return attribute(name, STRING_LITERAL(value));
    }

    Element& child(Element&& child) {
        children.emplace_back(std::move(child));
        return *this;
    }

    Element& child(const Element& child) {
        children.emplace_back(child);
        return *this;
    }

    Element& setChildren(const std::vector<Element>& children) {
        this->children = children;
        return *this;
    }
};

struct RenderState {
    RawViewNodeId elementId = 0;
    AttributeIds& attributeIds;

    RenderState(Dependencies& deps) : attributeIds(deps.attributeIds) {}
};

RawViewNodeId populateRenderRequest(RenderRequest& request, RenderState& state, const Element& element) {
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

    std::vector<RawViewNodeId> childrenIds;

    for (const auto& child : element.children) {
        auto childId = populateRenderRequest(request, state, child);
        childrenIds.emplace_back(childId);
    }

    size_t i = 0;
    for (const auto& childId : childrenIds) {
        auto* moveElementToParent = request.appendMoveElementToParent();

        moveElementToParent->setParentElementId(elementId);
        moveElementToParent->setElementId(childId);
        moveElementToParent->setParentIndex(i);
        i++;
    }

    if (elementId == 1) {
        request.appendSetRootElement()->setElementId(elementId);
    }

    return elementId;
}

static Element createElementTree() {
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
        auto card = Element("view")
                        .attribute("width", 40)
                        .attribute("height", "100%")
                        .attribute("background", "white")
                        .child(Element("label")
                                   .attribute("value", STRING_LITERAL("Hello world what is up"))
                                   .attribute("lineSpacing", 4)
                                   .attribute("font", "Title")
                                   .attribute("fontSize", 16)
                                   .attribute("color", "black")
                                   .attribute("textAlign", "center")
                                   .attribute("numberOfLines", 0)
                                   .attribute("marginTop", 8)
                                   .attribute("marginBottom", 16));

        cards.emplace_back(std::move(card));
    }

    std::vector<Element> cells;
    for (size_t i = 0; i < 20; i++) {
        cells.emplace_back(Element("view")
                               .attribute("flexGrow", 1)
                               .child(Element("scroll").attribute("horizontal", true).setChildren(cards)));
    }

    auto root = Element("view")
                    .attribute("width", 100)
                    .attribute("height", 100)
                    .child(header)
                    .child(Element("scroll").attribute("flexGrow", 1).setChildren(cells));
    return root;
}

static void InitialRender(benchmark::State& state) {
    Dependencies deps;

    RenderState renderState(deps);
    auto request = makeShared<RenderRequest>();
    populateRenderRequest(*request, renderState, createElementTree());

    for (auto _ : state) {
        auto tree = deps.createTree();
        ViewNodeRenderer renderer(*tree, ConsoleLogger::getLogger(), false);

        renderer.render(*request);

        state.PauseTiming();
        deps.destroyTree(tree);
        state.ResumeTiming();
    }
}
BENCHMARK(InitialRender);

static void DestroyTree(benchmark::State& state) {
    Dependencies deps;

    RenderState renderState(deps);
    auto request = makeShared<RenderRequest>();
    populateRenderRequest(*request, renderState, createElementTree());

    for (auto _ : state) {
        state.PauseTiming();
        auto tree = deps.createTree();

        ViewNodeRenderer renderer(*tree, ConsoleLogger::getLogger(), false);

        renderer.render(*request);
        state.ResumeTiming();

        deps.destroyTree(tree);
    }
}
BENCHMARK(DestroyTree);

BENCHMARK_MAIN();
