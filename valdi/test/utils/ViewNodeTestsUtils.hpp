
#include "valdi/runtime/Attributes/AttributesApplier.hpp"
#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Context/ViewNodeChildrenIndexer.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/GlobalViewFactories.hpp"
#include "valdi/runtime/Views/ViewTransactionScope.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"
#include "valdi/standalone_runtime/StandaloneViewManager.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi_test_utils.hpp"

#include <yoga/Yoga.h>

using namespace Valdi;

namespace ValdiTest {

class ViewNodeTestsDependencies {
public:
    ViewNodeTestsDependencies();
    ~ViewNodeTestsDependencies();

    ViewNodeTree& getTree() const;

    Ref<ViewNode> createNode(const char* viewClassName);
    Ref<ViewNode> createView();
    Ref<ViewNode> createLayout();
    Ref<ViewNode> createScroll();
    Ref<ViewNode> createRootView();

    void disableUpdates();

    StandaloneViewManager& getViewManager();
    Ref<ViewFactory> getViewFactory(const char* viewClassName);

    void setViewNodeAttribute(const Ref<ViewNode>& viewNode, const char* attribute, const Value& attributeValue);

    void setViewNodeFrame(const Ref<ViewNode>& viewNode, double x, double y, double width, double height);

    void setViewNodeMarginAndSize(const Ref<ViewNode>& viewNode, double margin, double width, double height);

    void setViewNodeAllMarginsAndSize(const Ref<ViewNode>& viewNode,
                                      double marginTop,
                                      double marginRight,
                                      double marginBottom,
                                      double marginLeft,
                                      double width,
                                      double height);

    ViewTransactionScope& getViewTransactionScope();

private:
    Ref<StandaloneMainQueue> _mainQueue;
    StandaloneViewManager _viewManager;
    AttributeIds _attributeIds;
    Ref<MainThreadManager> _mainThreadManager;
    AttributesManager _attributesManager;
    Ref<GlobalViewFactories> _viewFactories;
    Ref<ViewNodeTree> _tree;
    Ref<ViewTransactionScope> _viewTransactionScope;
    TrackedLock _lock;
    ViewNodeTreeDisableUpdates _disableUpdates;
};

} // namespace ValdiTest
