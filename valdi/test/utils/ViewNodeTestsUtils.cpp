#include "ViewNodeTestsUtils.hpp"

using namespace Valdi;

namespace ValdiTest {

static Ref<MainThreadManager> makeMainThreadManager(const Ref<IMainThreadDispatcher>& mainThreadDispatcher) {
    auto mainThreadManager = makeShared<MainThreadManager>(mainThreadDispatcher);
    mainThreadManager->markCurrentThreadIsMainThread();
    return mainThreadManager;
}

ViewNodeTestsDependencies::ViewNodeTestsDependencies()
    : _mainQueue(makeShared<StandaloneMainQueue>()),
      _mainThreadManager(makeMainThreadManager(_mainQueue->createMainThreadDispatcher())),
      _attributesManager(_viewManager,
                         _attributeIds,
                         makeShared<ColorPaletteManager>(),
                         ConsoleLogger::getLogger(),
                         Yoga::createConfig(0)),
      _viewTransactionScope(makeShared<ViewTransactionScope>(&_viewManager, nullptr, false)) {
    _mainThreadManager->markCurrentThreadIsMainThread();

    _tree = Valdi::makeShared<ViewNodeTree>(makeShared<Context>(0, strongSmallRef(&_attributesManager.getLogger())),
                                            nullptr,
                                            &_viewManager,
                                            nullptr,
                                            _mainThreadManager.get(),
                                            false);
    _lock = _tree->lock();
    _tree->unsafeSetCurrentViewTransactionScope(_viewTransactionScope);
    // We will update the ViewNode tree ourselves
    _disableUpdates = _tree->beginDisableUpdates();
    _tree->unsafeBeginViewTransaction();
    _viewFactories = Valdi::makeShared<GlobalViewFactories>(_attributesManager);

    _mainQueue->runNextTask();
}

ViewNodeTestsDependencies::~ViewNodeTestsDependencies() = default;

void ViewNodeTestsDependencies::disableUpdates() {
    _disableUpdates.endDisableUpdates();
}

ViewNodeTree& ViewNodeTestsDependencies::getTree() const {
    return *_tree;
}

ViewTransactionScope& ViewNodeTestsDependencies::getViewTransactionScope() {
    return *_viewTransactionScope;
}

Ref<ViewNode> ViewNodeTestsDependencies::createNode(const char* viewClassName) {
    auto viewNode = Valdi::makeShared<ViewNode>(_attributesManager.getYogaConfig(),
                                                _attributesManager.getAttributeIds(),
                                                _attributesManager.getColorPaletteManager()->getActiveColorPalette(),
                                                _attributesManager.getLogger());
    viewNode->setViewNodeTree(_tree.get());
    viewNode->setViewFactory(*_viewTransactionScope, _viewFactories->getViewFactory(STRING_LITERAL(viewClassName)));

    return viewNode;
}

Ref<ViewNode> ViewNodeTestsDependencies::createView() {
    return createNode("View");
}

Ref<ViewNode> ViewNodeTestsDependencies::createLayout() {
    return createNode("Layout");
}

Ref<ViewNode> ViewNodeTestsDependencies::createScroll() {
    // Let the scroll view attributes be registered
    _viewManager.setRegisterCustomAttributes(true);
    auto scrollView = createNode("SCValdiScrollView");
    _viewManager.setRegisterCustomAttributes(false);
    return scrollView;
}

Ref<ViewNode> ViewNodeTestsDependencies::createRootView() {
    auto node = createView();

    auto view = node->getViewFactory()->createView(_tree.get(), node.get(), true);
    _tree->setRootView(view);
    _tree->setRootViewNode(node, false);

    return node;
}

StandaloneViewManager& ViewNodeTestsDependencies::getViewManager() {
    return _viewManager;
}

void ViewNodeTestsDependencies::setViewNodeAttribute(const Ref<ViewNode>& viewNode,
                                                     const char* attribute,
                                                     const Value& attributeValue) {
    auto attributeId = _attributesManager.getAttributeIds().getIdForName(std::string_view(attribute));
    viewNode->setAttribute(*_viewTransactionScope,
                           attributeId,
                           AttributeOwner::getNativeOverridenAttributeOwner(),
                           attributeValue,
                           nullptr);
}

void ViewNodeTestsDependencies::setViewNodeFrame(
    const Ref<ViewNode>& viewNode, double x, double y, double width, double height) {
    setViewNodeAttribute(viewNode, "position", Value(STRING_LITERAL("absolute")));
    setViewNodeAttribute(viewNode, "left", Value(x));
    setViewNodeAttribute(viewNode, "width", Value(width));
    setViewNodeAttribute(viewNode, "top", Value(y));
    setViewNodeAttribute(viewNode, "height", Value(height));
}

void ViewNodeTestsDependencies::setViewNodeMarginAndSize(const Ref<ViewNode>& viewNode,
                                                         double margin,
                                                         double width,
                                                         double height) {
    setViewNodeAttribute(viewNode, "margin", Value(margin));
    setViewNodeAttribute(viewNode, "width", Value(width));
    setViewNodeAttribute(viewNode, "height", Value(height));
    viewNode->getAttributesApplier().flush(*_viewTransactionScope);
}

void ViewNodeTestsDependencies::setViewNodeAllMarginsAndSize(const Ref<ViewNode>& viewNode,
                                                             double marginTop,
                                                             double marginRight,
                                                             double marginBottom,
                                                             double marginLeft,
                                                             double width,
                                                             double height) {
    setViewNodeAttribute(viewNode, "marginTop", Value(marginTop));
    setViewNodeAttribute(viewNode, "marginRight", Value(marginRight));
    setViewNodeAttribute(viewNode, "marginBottom", Value(marginBottom));
    setViewNodeAttribute(viewNode, "marginLeft", Value(marginLeft));
    setViewNodeAttribute(viewNode, "width", Value(width));
    setViewNodeAttribute(viewNode, "height", Value(height));
    viewNode->getAttributesApplier().flush(*_viewTransactionScope);
}

} // namespace ValdiTest
