#include "RequestManagerMock.hpp"
#include "TestAsyncUtils.hpp"
#include "valdi/RuntimeMessageHandler.hpp"
#include "valdi/runtime/Attributes/AttributesApplier.hpp"
#include "valdi/runtime/Context/ContextHandler.hpp"
#include "valdi/runtime/Context/IViewNodesAssetTracker.hpp"
#include "valdi/runtime/Context/ViewNodeAssetHandler.hpp"
#include "valdi/runtime/Context/ViewNodeScrollState.hpp"
#include "valdi/runtime/Interfaces/ITweakValueProvider.hpp"
#include "valdi/runtime/JavaScript/JavaScriptANRDetector.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"
#include "valdi/runtime/JavaScript/WrappedJSValueRef.hpp"
#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi/runtime/Resources/ObservableAsset.hpp"
#include "valdi/runtime/Resources/ValdiModuleArchive.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/ValdiRuntimeTweaks.hpp"
#include "valdi_core/cpp/Attributes/TextAttributeValue.hpp"
#include "valdi_core/cpp/JavaScript/ModuleFactoryRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaTypeResolver.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"

#include "valdi/jsbridge/JavaScriptBridge.hpp"

#include "JSBridgeTestFixture.hpp"
#include "RuntimeTestsUtils.hpp"
#include "TSNTestUtils.hpp"
#include "TestANRDetectorListener.hpp"
#include "utils/base/Function.hpp"
#include "valdi/runtime/Context/AttributionResolver.hpp"
#include "valdi/standalone_runtime/StandaloneLoadedAsset.hpp"
#include "valdi/standalone_runtime/StandaloneNodeRef.hpp"
#include "valdi/standalone_runtime/StandaloneView.hpp"
#include "valdi/standalone_runtime/StandaloneViewManager.hpp"
#include "valdi_core/AssetLoadObserver.hpp"
#include "valdi_core/cpp/JavaScript/JavaScriptPathResolver.hpp"
#include "valdi_core/cpp/Resources/Asset.hpp"
#include "valdi_core/cpp/Resources/LoadedAsset.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include "valdi_core/cpp/Utils/TimePoint.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include "valdi_test_utils.hpp"
#include "gtest/gtest.h"
#include <yoga/YGNode.h>

#include "valdi_modules/test/test.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

class TestTweakValueProvider : public SharedPtrRefCountable,
                               public snap::valdi_core::ModuleFactory,
                               public ITweakValueProvider {
public:
    Value config;

    StringBox getModulePath() override {
        return STRING_LITERAL("Tweak");
    }

    Value loadModule() override {
        return Value();
    }

    StringBox getString(const StringBox& key, const StringBox& fallback) override {
        return config.getMapValue(key).toStringBox();
    }

    bool getBool(const StringBox& key, bool fallback) override {
        return config.getMapValue(key).toBool();
    }

    float getFloat(const StringBox& key, float fallback) override {
        return config.getMapValue(key).toFloat();
    }

    Value getBinary(const StringBox& key, const Value& fallback) override {
        return config.getMapValue(key);
    }
};

class TestValueTypedProxyObject : public ValueTypedProxyObject {
public:
    explicit TestValueTypedProxyObject(const Ref<ValueTypedObject>& typedObject) : ValueTypedProxyObject(typedObject) {}
    std::string_view getType() const final {
        return "Test Proxy";
    }
};

struct TestViewNodesAssetTracker : public IViewNodesAssetTracker {
public:
    std::atomic_int beganRequestCount = 0;
    std::atomic_int endRequestCount = 0;
    std::atomic_int changedCount = 0;

    void onBeganRequestingLoadedAsset(RawViewNodeId viewNodeId, const Ref<Asset>& asset) override {
        beganRequestCount++;
    }

    void onEndRequestingLoadedAsset(RawViewNodeId viewNodeId, const Ref<Asset>& asset) override {
        endRequestCount++;
    }

    void onLoadedAssetChanged(RawViewNodeId viewNodeId,
                              const Ref<Asset>& asset,
                              const std::optional<Valdi::StringBox>& error) override {
        changedCount++;
    }
};

class RuntimeFixture : public JSBridgeTestFixture {
protected:
    void SetUp() override {
        auto* jsBridge = getJsBridge();
        wrapper = RuntimeWrapper(jsBridge, getTSNMode());
    }

    void TearDown() override {
        wrapper.teardown();
    }

    TSNMode getTSNMode() const {
        return isWithTSN() ? TSNMode::Enabled : TSNMode::Disabled;
    }

    RuntimeWrapper wrapper;
};

static Result<Value> getJsModuleWithSchema(
    Runtime* runtime,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    ValueSchemaRegistry* registry,
    std::string_view moduleName,
    const ValueSchema& schema) {
    auto resolvedSchema = schema;
    if (registry != nullptr) {
        ValueSchemaTypeResolver typeResolver(registry);
        auto schemaResult = typeResolver.resolveTypeReferences(schema);
        if (!schemaResult) {
            return schemaResult.moveError();
        }
        resolvedSchema = schemaResult.moveValue();
    }

    Valdi::SimpleExceptionTracker exceptionTracker;
    Valdi::Marshaller marshaller(exceptionTracker);

    marshaller.setCurrentSchema(resolvedSchema);

    auto moduleIndex = runtime->getJavaScriptRuntime()->pushModuleToMarshaller(
        nativeObjectsManager, StringCache::getGlobal().makeString(moduleName), marshaller);
    if (!exceptionTracker) {
        return Error(STRING_FORMAT("Couldn't resolve module '{}'", moduleName));
    }

    return marshaller.get(moduleIndex);
}

static Result<Value> getJsModulePropertyWithSchema(
    Runtime* runtime,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    ValueSchemaRegistry* registry,
    std::string_view moduleName,
    std::string_view propertyName,
    std::string_view propertySchema) {
    auto propertySchemaResult = ValueSchema::parse(propertySchema);
    if (!propertySchemaResult) {
        return propertySchemaResult.moveError();
    }

    auto schema = ValueSchema::cls(
        STRING_LITERAL("JSModule"),
        false,
        {ClassPropertySchema(StringCache::getGlobal().makeString(propertyName), propertySchemaResult.value())});

    auto unmarshalledJsModule = getJsModuleWithSchema(runtime, nativeObjectsManager, registry, moduleName, schema);
    if (!unmarshalledJsModule) {
        return unmarshalledJsModule.moveError();
    }

    return unmarshalledJsModule.value().getTypedObject()->getProperty(0);
}

static Result<Value> getJsModulePropertyWithSchema(Runtime* runtime,
                                                   std::string_view moduleName,
                                                   std::string_view propertyName,
                                                   std::string_view propertySchema) {
    return getJsModulePropertyWithSchema(runtime, nullptr, nullptr, moduleName, propertyName, propertySchema);
}

static Result<Value> getJsModulePropertyAsValue(
    Runtime* runtime,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    std::string_view moduleName,
    std::string_view propertyName) {
    return getJsModulePropertyWithSchema(runtime, nativeObjectsManager, nullptr, moduleName, propertyName, "u");
}

static Result<Ref<ValueFunction>> getJsModulePropertyAsUntypedFunction(
    Runtime* runtime,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    std::string_view moduleName,
    std::string_view propertyName) {
    auto value = getJsModulePropertyAsValue(runtime, nativeObjectsManager, moduleName, propertyName);
    if (!value) {
        return value.moveError();
    }

    SimpleExceptionTracker exceptionTracker;
    auto valueFunction = value.value().checkedTo<Ref<ValueFunction>>(exceptionTracker);
    return exceptionTracker.toResult(std::move(valueFunction));
}

static Result<Value> callFunctionSync(
    RuntimeWrapper& wrapper,
    const std::shared_ptr<snap::valdi_core::JSRuntimeNativeObjectsManager>& nativeObjectsManager,
    std::string_view moduleName,
    std::string_view functionName,
    std::vector<Value> params) {
    auto function =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nativeObjectsManager, moduleName, functionName);
    if (!function) {
        return function.moveError();
    }

    return function.value()->call(ValueFunctionFlagsCallSync, params.data(), params.size());
}

static Result<Value> callFunctionSync(RuntimeWrapper& wrapper,
                                      std::string_view moduleName,
                                      std::string_view functionName,
                                      std::vector<Value> params) {
    return callFunctionSync(wrapper, nullptr, moduleName, functionName, std::move(params));
}

TEST_P(RuntimeFixture, canLoadSimpleViewTree) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, failsWithInvalidDocument) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "InvalidView");

    wrapper.flushQueues();

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));
}

TEST_P(RuntimeFixture, canApplyAttributes) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "StaticAttributes");

    wrapper.waitUntilAllUpdatesCompleted();

    // Should not be equal without the attributes
    ASSERT_NE(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("UIButton"))
                  .addChild(DummyView("UIButton")),
              getRootView(tree));

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("color", "white")
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title").addAttribute("font", "title"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 45.0).addAttribute("value", "Button 2")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, removeAttributesOnDestroy) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "StaticAttributes");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_TRUE(tree->getRootViewNode() != nullptr);
    ASSERT_EQ(static_cast<uint32_t>(3), tree->getRootViewNode()->getChildCount());

    auto rootView = tree->getRootViewNode()->getView();
    auto label = tree->getRootViewNode()->getChildAt(static_cast<uint32_t>(0))->getView();
    auto button1 = tree->getRootViewNode()->getChildAt(static_cast<uint32_t>(1))->getView();
    auto button2 = tree->getRootViewNode()->getChildAt(static_cast<uint32_t>(2))->getView();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("color", "white")
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title").addAttribute("font", "title"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 45.0).addAttribute("value", "Button 2")),
              getDummyView(rootView));

    wrapper.runtime->destroyViewNodeTree(*tree);

    // All attributes should have been removed, and the tree should have been cleared.
    ASSERT_EQ(DummyView("SCValdiView"), getDummyView(rootView));
    ASSERT_EQ(DummyView("SCValdiLabel"), getDummyView(label));
    ASSERT_EQ(DummyView("UIButton"), getDummyView(button1));
    ASSERT_EQ(DummyView("UIButton"), getDummyView(button2));
}

TEST_P(RuntimeFixture, clearViewNodesOnDestroy) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootNode = tree->getRootViewNode();
    ASSERT_TRUE(rootNode != nullptr);

    auto children = rootNode->copyChildren();

    ASSERT_EQ(static_cast<size_t>(2), children.size());

    {
        auto nestedChildren = children[1]->copyChildren();
        children.insert(children.end(), nestedChildren.begin(), nestedChildren.end());
    }

    ASSERT_EQ(static_cast<size_t>(5), children.size());

    ASSERT_TRUE(rootNode->hasView());
    ASSERT_TRUE(rootNode->getViewFactory() != nullptr);
    ASSERT_TRUE(!rootNode->hasParent());
    ASSERT_TRUE(rootNode->getParent() == nullptr);
    ASSERT_EQ(3, rootNode->retainCount());

    for (const auto& child : children) {
        ASSERT_TRUE(child->hasView());
        ASSERT_TRUE(child->getViewFactory() != nullptr);
        ASSERT_TRUE(child->hasParent());
        ASSERT_TRUE(child->getParent() != nullptr);
        ASSERT_EQ(2, child->retainCount());
    }

    wrapper.runtime->destroyViewNodeTree(*tree);

    ASSERT_FALSE(rootNode->hasView());
    ASSERT_FALSE(rootNode->getViewFactory() != nullptr);
    ASSERT_FALSE(rootNode->hasParent());
    ASSERT_FALSE(rootNode->getParent() != nullptr);
    ASSERT_EQ(1, rootNode->retainCount());

    for (const auto& child : children) {
        ASSERT_FALSE(child->hasView());
        ASSERT_FALSE(child->getViewFactory() != nullptr);
        ASSERT_FALSE(child->hasParent());
        ASSERT_FALSE(child->getParent() != nullptr);
        ASSERT_EQ(1, child->retainCount());
    }
}

TEST_P(RuntimeFixture, respectAttributeTyping) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "StaticAttributes");

    wrapper.waitUntilAllUpdatesCompleted();

    auto stringAttribute = getRootView(tree).getChild(0).getAttribute("font");
    auto integerAttribute = getRootView(tree).getChild(1).getAttribute("dummyHeight");
    auto doubleAttribute = getRootView(tree).getChild(2).getAttribute("dummyHeight");

    ASSERT_EQ(stringAttribute.getType(), ValueType::InternedString);
    ASSERT_EQ(integerAttribute.getType(), ValueType::Double);
    ASSERT_EQ(doubleAttribute.getType(), ValueType::Double);
}

TEST_P(RuntimeFixture, canIndex) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "Indexing");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(getViewForId(tree, "rootView").getClassName(), "SCValdiView");
    ASSERT_EQ(getViewForId(tree, "label").getClassName(), "SCValdiLabel");
    ASSERT_EQ(getViewForId(tree, "button1").getClassName(), "UIButton");
    ASSERT_EQ(getViewForId(tree, "button2").getClassName(), "UIButton");
    ASSERT_EQ(getViewForId(tree, "button3").getClassName(), "UIButton");

    ASSERT_EQ(tree->getViewForNodePath(parseNodePath("doesntexist")), nullptr);
}

TEST_P(RuntimeFixture, canGetElementByNodePath) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "NodePath");

    wrapper.waitUntilAllUpdatesCompleted();

    auto outerNodes = tree->getRootViewNode()->copyChildrenWithDebugId(STRING_LITERAL("outer"));
    ASSERT_EQ(static_cast<size_t>(1), outerNodes.size());
    auto containerViewNodes = tree->getRootViewNode()->copyChildrenWithDebugId(STRING_LITERAL("container"));
    ASSERT_EQ(static_cast<size_t>(10), containerViewNodes.size());

    auto uniqueLabel = containerViewNodes[2]->copyChildrenWithDebugId(STRING_LITERAL("uniqueLabel"))[0];
    auto multiLabelContainer = containerViewNodes[6]->copyChildrenWithDebugId(STRING_LITERAL("multiLabel"));
    ASSERT_EQ(static_cast<size_t>(2), multiLabelContainer.size());
    auto multiLabel = multiLabelContainer[1];

    ASSERT_EQ(outerNodes[0], tree->getViewNodeForNodePath(parseNodePath("outer")));
    ASSERT_EQ(uniqueLabel, tree->getViewNodeForNodePath(parseNodePath("container[2].uniqueLabel")));
    ASSERT_EQ(multiLabel, tree->getViewNodeForNodePath(parseNodePath("container[6].multiLabel[1]")));

    // Should fail gracefully on out of bound
    ASSERT_EQ(nullptr, tree->getViewNodeForNodePath(parseNodePath("container[10]")));
    ASSERT_EQ(nullptr, tree->getViewNodeForNodePath(parseNodePath("container[10].uniqueLabel")));
    ASSERT_EQ(nullptr, tree->getViewNodeForNodePath(parseNodePath("container[0].multiLabel[2]")));
}

TEST_P(RuntimeFixture, canApplyCSS) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("containerColor")] = Value(STRING_LITERAL("black"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CSSAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addAttribute("color", "white")
            .addAttribute("alignItems", "center")
            .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title").addAttribute("font", "title"))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("color", "black")
                    .addAttribute("justifyContent", "space-between")
                    .addAttribute("id", "container")
                    .addChild(DummyView("UIButton").addAttribute("dummyHeight", 35).addAttribute("value", "Button 1"))
                    .addChild(DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 2"))
                    .addChild(
                        DummyView("UIButton").addAttribute("dummyHeight", 45.0).addAttribute("value", "Button 3"))),
        getRootView(tree));
}

void checkViewHasSingleHistoryPerAttribute(const DummyView& dummyView) {
    for (const auto& it : *dummyView.getImpl().getAttributesHistory()) {
        ASSERT_EQ(1, static_cast<int>(it.second.size()));
    }
    for (const auto& child : dummyView.getImpl().getChildren()) {
        checkViewHasSingleHistoryPerAttribute(DummyView(child));
    }
}

TEST_P(RuntimeFixture, doesntReapplyCSSAfterMultipleRender) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("containerColor")] = Value(STRING_LITERAL("black"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CSSAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // Each attribute should be applied only once

    const auto& rootView = getRootView(tree);
    checkViewHasSingleHistoryPerAttribute(rootView);
}

TEST_P(RuntimeFixture, canResetCSSValues) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("containerColor")] = Value(STRING_LITERAL("black"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CSSAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("color", "black")
                  .addAttribute("justifyContent", "space-between")
                  .addAttribute("id", "container")
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 35).addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 2"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 45.0).addAttribute("value", "Button 3")),
              getViewForId(tree, "container"));

    (*viewModel)[STRING_LITERAL("containerColor")] = Value(STRING_LITERAL("white"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    const auto& viewToTest = getViewForId(tree, "container");

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("color", "white")
                  .addAttribute("id", "container")
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 35).addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 2"))
                  .addChild(DummyView("UIButton").addAttribute("dummyHeight", 45.0).addAttribute("value", "Button 3")),
              viewToTest);

    // The color attried should have changed twice.
    const auto& it = viewToTest.getImpl().getAttributesHistory()->find(STRING_LITERAL("color"));
    ASSERT_NE(viewToTest.getImpl().getAttributesHistory()->end(), it);

    ASSERT_EQ(2, static_cast<int>(it->second.size()));
}

TEST_P(RuntimeFixture, canApplyDynamicAttributes) {
    auto viewModelParameters = makeShared<ValueMap>();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "DynamicAttributes", Value(viewModelParameters));

    wrapper.waitUntilAllUpdatesCompleted();

    // At this point we should have only the view tree and the static attribute applied

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("title")] = Valdi::Value(std::string("Hello"));
    (*viewModelParameters)[STRING_LITERAL("subtitleHeight")] = Valdi::Value(45.5);
    (*viewModelParameters)[STRING_LITERAL("padding")] = Valdi::Value(10);

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("padding", 10)
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                  .addChild(DummyView("SCValdiLabel").addAttribute("dummyHeight", 45.5)),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRenderChildDocuments) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "ChildDocument");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("color", "red"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    auto childViewModelParameters = makeShared<ValueMap>();
    (*childViewModelParameters)[STRING_LITERAL("title")] = Valdi::Value(std::string("Hello"));
    (*childViewModelParameters)[STRING_LITERAL("subtitleHeight")] = Valdi::Value(45.5);
    (*childViewModelParameters)[STRING_LITERAL("padding")] = Valdi::Value(10);

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("childViewModel")] = Valdi::Value(childViewModelParameters);

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("padding", 10.0)
                    .addChild(DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                    .addChild(DummyView("SCValdiLabel").addAttribute("dummyHeight", 45.5)))
            .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
        getRootView(tree));
}

TEST_P(RuntimeFixture, canConditionallyRender) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "RenderIf");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(false);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(false);

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    // We shouldn't have no views because renderButton2 and 3 are false
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(false);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(true);

    viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 3")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(true);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(true);

    viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 2"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 3")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, applyStylingAfterViewBecomesVisible) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "RenderIf");

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(true);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModelParameters));

    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 2"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 3")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(false);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(false);

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    // We shouldn't have no views because renderButton2 and 3 are false
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(true);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModelParameters));

    wrapper.waitUntilAllUpdatesCompleted();

    // We should have the attributes applied again
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 2"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 3")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRemoveConditionallyRenderedView) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "RenderIf");

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(true);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(true);

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 2"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 3")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(false);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(true);

    viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 3")),
              getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderButton2")] = Valdi::Value(false);
    (*viewModelParameters)[STRING_LITERAL("renderButton3")] = Valdi::Value(false);

    viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRenderForEach) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "ForEach");

    wrapper.waitUntilAllUpdatesCompleted();

    // Should have no views at the beginning
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    auto card1 = makeShared<ValueMap>();
    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 1"));
    (*card1)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 1"));

    auto card2 = makeShared<ValueMap>();
    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 2"));
    (*card2)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 2"));

    ValueArrayBuilder cards;
    cards.emplace(Value(card1));
    cards.emplace(Value(card2));

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 1")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 2")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRemoveViewsInForEach) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "ForEach");

    auto card1 = makeShared<ValueMap>();
    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 1"));
    (*card1)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 1"));

    auto card2 = makeShared<ValueMap>();
    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 2"));
    (*card2)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 2"));

    ValueArrayBuilder cards;
    cards.emplace(Value(card1));
    cards.emplace(Value(card2));

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 1")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 2")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    cards = ValueArrayBuilder();
    cards.emplace(Value(card2));

    (*viewModelParameters)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 2")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    cards = ValueArrayBuilder();

    (*viewModelParameters)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    viewModel = Valdi::Value(viewModelParameters);

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRenderCombinedForEachAndRender) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("cards")] = Value(ValueArray::make(0));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CombinedForEachRenderIf", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // Should have no views at the beginning
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    auto card1 = makeShared<ValueMap>();
    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string(""));
    (*card1)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 1"));

    auto card2 = makeShared<ValueMap>();
    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 2"));
    (*card2)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 2"));

    ValueArrayBuilder cards;
    cards.emplace(Value(card1));
    cards.emplace(Value(card2));

    (*viewModel)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We shouldn't have the first card because title is empty string
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 2")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRemoveCombinedForEachAndRender) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("cards")] = Valdi::Value(ValueArray::make(0));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CombinedForEachRenderIf", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // Should have no views at the beginning
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    auto card1 = makeShared<ValueMap>();
    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 1"));
    (*card1)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 1"));

    auto card2 = makeShared<ValueMap>();
    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 2"));
    (*card2)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 2"));

    ValueArrayBuilder cards;
    cards.emplace(Value(card1));
    cards.emplace(Value(card2));
    (*viewModel)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We shouldn't have the first card because title is empty string
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 1")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Subtitle 2")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
              getRootView(tree));

    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string(""));
    (*card1)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 1"));

    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 2"));
    (*card2)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string(""));

    cards = ValueArrayBuilder();
    cards.emplace(Value(card1));
    cards.emplace(Value(card2));

    (*viewModel)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We shouldn't have the first card because title is empty string
    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView").addChild(DummyView("SCValdiLabel").addAttribute("value", "Title 2")))
            .addChild(DummyView("UIButton").addAttribute("value", "Button 1")),
        getRootView(tree));
}

TEST_P(RuntimeFixture, supportsBackingJSInstance) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "JSInstance");

    // JS adds some attributes and calls render()
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addAttribute("id", "rootView")
            .addAttribute("color", "white")
            .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
            .addChild(
                DummyView("SCValdiView")
                    .addChild(DummyView("UIButton").addAttribute("id", "button1").addAttribute("value", "Button 1"))
                    .addChild(DummyView("UIButton").addAttribute("id", "button2").addAttribute("value", "Button 2"))
                    .addChild(DummyView("UIButton").addAttribute("id", "button3").addAttribute("value", "Button 3"))),
        getRootView(tree));
}

TEST_P(RuntimeFixture, canExecuteCallbackFromJS) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "JSCallback");

    wrapper.waitUntilAllUpdatesCompleted();

    std::atomic_int sequence;
    sequence = 0;

    auto callback = makeShared<ValueFunctionWithCallable>([&](const auto& callContext) -> Value {
        auto paramsDict = callContext.getParameterAsMap(0);
        if (paramsDict == nullptr) {
            return Value::undefined();
        }
        sequence = static_cast<int>(paramsDict->find(STRING_LITERAL("sequence"))->second.toInt());
        return Value::undefined();
    });

    auto valueMap = makeShared<ValueMap>();
    (*valueMap)[STRING_LITERAL("callback")] = Valdi::Value(callback);
    ValueArrayBuilder parameters;
    parameters.emplace(Value(valueMap));

    auto functionName = STRING_LITERAL("increment");
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), functionName, parameters.build());

    // Wait until the runloop has flushed
    wrapper.flushJsQueue();

    ASSERT_EQ(1, sequence);

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), functionName, parameters.build());

    // Wait until the runloop has flushed
    wrapper.flushJsQueue();

    ASSERT_EQ(2, sequence);

    // Also test that optional invocations of natively bridged functions works
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("incrementWithOptionalInvocation"), parameters.build());

    // Wait until the runloop has flushed
    wrapper.flushJsQueue();

    ASSERT_EQ(3, sequence);
}

TEST_P(RuntimeFixture, canCallCallbacksToJS) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "JSCallback");

    wrapper.waitUntilAllUpdatesCompleted();

    auto expected = DummyView("SCValdiView").addAttribute("id", "subView").addAttribute("color", "red");
    ASSERT_NE(expected, getRootView(tree).getChild(1));

    const auto onTap = getRootView(tree).getChild(0).getAttribute("onTap");

    ASSERT_EQ(Valdi::ValueType::Function, onTap.getType());

    (*onTap.getFunction())(Valdi::ValueFunctionFlagsCallSync, {});

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(expected, getRootView(tree).getChild(1));
}

TEST_P(RuntimeFixture, canApplyCompositeAttributes) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("style")] = Value(STRING_LITERAL("dotted"));
    (*viewModel)[STRING_LITERAL("left")] = Value(1.0);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CompositeAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto values = ValueArray::make(5);
    (*values)[0] = Value(1.0);
    (*values)[1] = Value(2.0);
    (*values)[2] = Value(3.0);
    (*values)[3] = Value(4.0);
    (*values)[4] = Value(std::string("dotted"));

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIRectangleView")
                                .addAttribute("id", "container")
                                .addAttribute("rectangle", Valdi::Value(values))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, optionalCompositeAttributeParts) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("left")] = Value(1.0);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CompositeAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto values = ValueArray::make(5);
    (*values)[0] = Value(1.0);
    (*values)[1] = Value(2.0);
    (*values)[2] = Value(3.0);
    (*values)[3] = Value(4.0);
    // Last value should be null because dotted is not here
    (*values)[4] = Value();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIRectangleView")
                                .addAttribute("id", "container")
                                .addAttribute("rectangle", Valdi::Value(values))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, requiredCompositeAttributeParts) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("style")] = Value(STRING_LITERAL("dotted"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CompositeAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We are missing the required attribute 'left', so we should not have the rectangle set
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("UIRectangleView").addAttribute("id", "container")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canPassCompositeAttributesExplicity) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("rectangle")] = Value(STRING_LITERAL("some rectangle"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CompositeAttributesRaw", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView").addChild(DummyView("UIRectangleView").addAttribute("rectangle", "some rectangle")),
        getRootView(tree));
}

TEST_P(RuntimeFixture, canSetupFlexboxTree) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootViewNode = tree->getRootViewNode();
    auto rootYogaNode = rootViewNode->getYogaNode();

    ASSERT_NE(nullptr, rootYogaNode);
    ASSERT_EQ(rootViewNode.get(), Valdi::Yoga::getAttachedViewNode(rootYogaNode));

    ASSERT_EQ(2, static_cast<int>(rootYogaNode->getChildren().size()));

    auto firstChild = rootYogaNode->getChild(0);
    ASSERT_EQ(rootViewNode->copyChildren()[0].get(), Valdi::Yoga::getAttachedViewNode(firstChild));

    ASSERT_EQ(0, static_cast<int>(firstChild->getChildren().size()));

    auto secondChild = rootYogaNode->getChild(1);
    auto secondChildNode = rootViewNode->copyChildren()[1];

    ASSERT_EQ(secondChildNode.get(), Valdi::Yoga::getAttachedViewNode(secondChild));
    ASSERT_EQ(3, static_cast<int>(secondChild->getChildren().size()));

    size_t i = 0;
    for (const auto& child : secondChild->getChildren()) {
        ASSERT_EQ(secondChildNode->copyChildren()[i].get(), Valdi::Yoga::getAttachedViewNode(child));

        ASSERT_EQ(0, static_cast<int>(child->getChildren().size()));
        i++;
    }
}

TEST_P(RuntimeFixture, canSetupFlexboxTreeWithChildDocument) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "RenderIfChildDocument");

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("renderChild")] = Valdi::Value(false);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootViewNode = tree->getRootViewNode();
    auto rootYogaNode = rootViewNode->getYogaNode();

    ASSERT_NE(nullptr, rootYogaNode);
    ASSERT_EQ(rootViewNode.get(), Valdi::Yoga::getAttachedViewNode(rootYogaNode));

    ASSERT_EQ(0, static_cast<int>(rootYogaNode->getChildren().size()));

    // Now we render with the child enabled.

    (*viewModel)[STRING_LITERAL("renderChild")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(1, static_cast<int>(rootYogaNode->getChildren().size()));

    auto containerViewNode = rootViewNode->copyChildren()[0];
    auto containerYogaNode = rootYogaNode->getChild(0);

    ASSERT_EQ(containerViewNode.get(), Valdi::Yoga::getAttachedViewNode(containerYogaNode));

    ASSERT_EQ(1, static_cast<int>(containerYogaNode->getChildren().size()));
    ASSERT_EQ(1, static_cast<int>(containerViewNode->copyChildren().size()));

    // Now we should also have

    auto childContextViewNode = containerViewNode->copyChildren()[0];

    // We should have the full flexbox tree of the child context attached to this flexbox node tree

    auto childRootYogaNode = containerYogaNode->getChild(0);

    ASSERT_EQ(2, static_cast<int>(childRootYogaNode->getChildren().size()));

    auto firstChild = childRootYogaNode->getChild(0);

    ASSERT_EQ(childContextViewNode->copyChildren()[0].get(), Valdi::Yoga::getAttachedViewNode(firstChild));

    ASSERT_EQ(0, static_cast<int>(firstChild->getChildren().size()));

    auto secondChild = childRootYogaNode->getChild(1);
    auto secondChildNode = childContextViewNode->copyChildren()[1];

    ASSERT_EQ(secondChildNode.get(), Valdi::Yoga::getAttachedViewNode(secondChild));
    ASSERT_EQ(3, static_cast<int>(secondChild->getChildren().size()));

    size_t i = 0;
    for (const auto& child : secondChild->getChildren()) {
        ASSERT_EQ(secondChildNode->copyChildren()[i].get(), Valdi::Yoga::getAttachedViewNode(child));

        ASSERT_EQ(0, static_cast<int>(child->getChildren().size()));
        i++;
    }

    // Now removing the child

    (*viewModel)[STRING_LITERAL("renderChild")] = Valdi::Value(false);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    // We should be back to not having children in the root

    ASSERT_EQ(0, static_cast<int>(rootYogaNode->getChildren().size()));

    // The container yoga node should not have a parent anymore
    ASSERT_EQ(nullptr, containerYogaNode->getOwner());
}

TEST_P(RuntimeFixture, handleConflictingAttribute) {
    auto viewModel = makeShared<ValueMap>();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ConflictingAttributeParent", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // Start with nothing set

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));

    // We set 4 different conflicting directives for the color attribute

    (*viewModel)[STRING_LITERAL("childClass")] = Value(STRING_LITERAL("red"));
    (*viewModel)[STRING_LITERAL("childColor")] = Value(STRING_LITERAL("blue"));
    (*viewModel)[STRING_LITERAL("color")] = Value(STRING_LITERAL("white"));
    (*viewModel)[STRING_LITERAL("class")] = Value(STRING_LITERAL("yellow"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // white should have been chosen, as it is set in the template of the parent
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "white")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("color")] = Value::undefined();
    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // blue should have been chosen, as it is set in the template of the child
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "blue")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("childColor")] = Value::undefined();
    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // yellow should have been chosen, as it is set in the css of the parent
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "yellow")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("class")] = Value::undefined();
    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    // red should have been chosen, as it is set in the css of the child
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "red")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("childClass")] = Value::undefined();
    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // we should have nothing now

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));
}

TEST_P(RuntimeFixture, canMaintainHighPriorityAttribute) {
    auto viewModel = makeShared<ValueMap>();

    (*viewModel)[STRING_LITERAL("childClass")] = Value(STRING_LITERAL("red"));
    (*viewModel)[STRING_LITERAL("childColor")] = Value(STRING_LITERAL("blue"));
    (*viewModel)[STRING_LITERAL("color")] = Value(STRING_LITERAL("white"));
    (*viewModel)[STRING_LITERAL("class")] = Value(STRING_LITERAL("yellow"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ConflictingAttributeParent", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // white should have been chosen, as it is set in the template of the parent
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "white")),
              getRootView(tree));

    // We now remove attributes which have a lower priority, they should not impact the rendering as the template
    // attribute should have priority.

    (*viewModel)[STRING_LITERAL("class")] = Value::undefined();

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "white")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("childClass")] = Value::undefined();

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "white")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("childColor")] = Value::undefined();

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "white")),
              getRootView(tree));

    // We now remove our high priority attribute and set a childClass, childClass should take over

    (*viewModel)[STRING_LITERAL("color")] = Value::undefined();
    (*viewModel)[STRING_LITERAL("childClass")] = Value(STRING_LITERAL("red"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("color", "red")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canSetupFlexboxTreeWithRenderIfs) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("renderChild1")] = Value(false);
    (*viewModel)[STRING_LITERAL("renderChild2")] = Value(false);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "FlexBox", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootViewNode = tree->getRootViewNode();
    auto rootYogaNode = rootViewNode->getYogaNode();

    // All ViewNodes should be generated

    ASSERT_EQ(2, static_cast<int>(rootViewNode->copyChildren().size()));

    auto child1ViewNodes = rootViewNode->copyChildrenWithDebugId(STRING_LITERAL("child1"));
    auto child2ViewNodes = rootViewNode->copyChildrenWithDebugId(STRING_LITERAL("child2"));
    auto child3ViewNodes = rootViewNode->copyChildrenWithDebugId(STRING_LITERAL("child3"));
    auto child4ViewNodes = rootViewNode->copyChildrenWithDebugId(STRING_LITERAL("child4"));

    ASSERT_TRUE(child1ViewNodes.size() == 0);
    ASSERT_TRUE(child2ViewNodes.size() == 1);
    ASSERT_TRUE(child3ViewNodes.size() == 0);
    ASSERT_TRUE(child4ViewNodes.size() == 1);

    auto child2ViewNode = child2ViewNodes[0];
    auto child4ViewNode = child4ViewNodes[0];

    ASSERT_EQ(2, static_cast<int>(rootYogaNode->getChildren().size()));

    auto child2YogaNode = rootYogaNode->getChild(0);
    auto child4YogaNode = rootYogaNode->getChild(1);

    ASSERT_EQ(child2YogaNode, child2ViewNode->getYogaNode());
    ASSERT_EQ(child4YogaNode, child4ViewNode->getYogaNode());

    // We then add child3

    (*viewModel)[STRING_LITERAL("renderChild2")] = Value(true);
    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    child3ViewNodes = rootViewNode->copyChildrenWithDebugId(STRING_LITERAL("child3"));

    ASSERT_TRUE(child3ViewNodes.size() == 1);
    auto child3ViewNode = child3ViewNodes[0];

    ASSERT_EQ(3, static_cast<int>(rootYogaNode->getChildren().size()));
    ASSERT_EQ(child2ViewNode->getYogaNode(), rootYogaNode->getChild(0));
    ASSERT_EQ(child3ViewNode->getYogaNode(), rootYogaNode->getChild(1));
    ASSERT_EQ(child4ViewNode->getYogaNode(), rootYogaNode->getChild(2));

    // We now insert the last child, child1

    (*viewModel)[STRING_LITERAL("renderChild1")] = Value(true);
    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    child1ViewNodes = rootViewNode->copyChildrenWithDebugId(STRING_LITERAL("child1"));

    ASSERT_TRUE(child1ViewNodes.size() == 1);
    auto child1ViewNode = child1ViewNodes[0];

    ASSERT_EQ(4, static_cast<int>(rootYogaNode->getChildren().size()));
    ASSERT_EQ(child1ViewNode->getYogaNode(), rootYogaNode->getChild(0));
    ASSERT_EQ(child2ViewNode->getYogaNode(), rootYogaNode->getChild(1));
    ASSERT_EQ(child3ViewNode->getYogaNode(), rootYogaNode->getChild(2));
    ASSERT_EQ(child4ViewNode->getYogaNode(), rootYogaNode->getChild(3));

    // Now removing all the render-if nodes

    (*viewModel)[STRING_LITERAL("renderChild1")] = Value(false);
    (*viewModel)[STRING_LITERAL("renderChild2")] = Value(false);
    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(2, static_cast<int>(rootYogaNode->getChildren().size()));
    ASSERT_EQ(child2ViewNode->getYogaNode(), rootYogaNode->getChild(0));
    ASSERT_EQ(child4ViewNode->getYogaNode(), rootYogaNode->getChild(1));
}

TEST_P(RuntimeFixture, jsCanAttachToMainThread) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "MainThread");

    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(1, wrapper.runtimeListener->numberOfRenders);

    Ref<ValueArray> jsReturnedValues;

    ValueArrayBuilder parameters;
    parameters.emplace(Valdi::Value(makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        jsReturnedValues = callContext.getParametersAsArray();

        return Value::undefined();
    })));
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("getRenders"), parameters.build());

    wrapper.flushJsQueue();

    ASSERT_TRUE(jsReturnedValues != nullptr);
    ASSERT_EQ(1, static_cast<int>(jsReturnedValues->size()));

    const auto& response = (*jsReturnedValues)[0].getMap();
    const auto& renderSyncCallback = response->find(STRING_LITERAL("sync"));
    const auto& renderAsyncCallback = response->find(STRING_LITERAL("async"));

    ASSERT_NE(response->end(), renderSyncCallback);
    ASSERT_NE(response->end(), renderAsyncCallback);

    // When we call the async callback, it should asynchronously dispatch to the JS thread,
    // execute the JS function which then asynchronously render in the main thread.
    // As such, the render effectively happens on the next main thread run loop

    (*renderAsyncCallback->second.getFunction())();

    ASSERT_EQ(1, wrapper.runtimeListener->numberOfRenders);

    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(2, wrapper.runtimeListener->numberOfRenders);

    // When we call the sync callback, it will synchronously dispatch to the JS thread,
    // and every tasks dispatched to the main thread will be executed before returning.

    (*renderSyncCallback->second.getFunction())();

    ASSERT_EQ(3, wrapper.runtimeListener->numberOfRenders);
}

TEST_P(RuntimeFixture, canGCSelfReferencingMainThreadCallback) {
    std::string evalBody = R"""(
        return (receiver) => {
            const obj = {};
            const cb = () => {
                receiver(obj.callback);
            };
            obj.callback = cb;
            runtime.configureCallback(1, cb);

            return obj;
        }
    )""";

    auto evalResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(evalBody)->toBytesView(), STRING_LITERAL("eval.js"));

    ASSERT_TRUE(evalResult) << evalResult.description();

    auto evalFunction = evalResult.value().getFunctionRef();

    ASSERT_TRUE(evalFunction != nullptr);

    auto receiver = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) { return Value::undefined(); });

    Value receivedObject;

    {
        auto param = Value(receiver);

        receivedObject = evalFunction->call(ValueFunctionFlagsCallSync, {param}).value();
    }

    ASSERT_TRUE(receivedObject.getMapValue("callback").isFunction());

    // The receiver should be retained by the engine, because it retains the makeMainThreadCallback which calls into the
    // receiver
    ASSERT_EQ(2, receiver.use_count());

    receivedObject = Value();
    wrapper.runtime->getJavaScriptRuntime()->performGc();
    wrapper.flushQueues();

    ASSERT_EQ(1, receiver.use_count());
}

TEST_P(RuntimeFixture, canHandleDynamicChildDocument) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("childDocumentName")] = Valdi::Value(std::string("test:src/BasicViewTree.valdi"));

    auto parent = wrapper.createViewNodeTreeAndContext("test", "DynamicChildDocument", Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton")))),
              getRootView(parent));

    (*viewModel)[STRING_LITERAL("childDocumentName")] = Valdi::Value(std::string("test:src/CSSAttributes.valdi"));

    wrapper.setViewModel(parent->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("color", "white")
                    .addAttribute("alignItems", "center")
                    .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title").addAttribute("font", "title"))
                    .addChild(
                        DummyView("SCValdiView")
                            .addAttribute("color", "black")
                            .addAttribute("justifyContent", "space-between")
                            .addAttribute("id", "container")
                            .addChild(
                                DummyView("UIButton").addAttribute("dummyHeight", 35).addAttribute("value", "Button 1"))
                            .addChild(
                                DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 2"))
                            .addChild(DummyView("UIButton")
                                          .addAttribute("dummyHeight", 45.0)
                                          .addAttribute("value", "Button 3")))),
        getRootView(parent));
}

TEST_P(RuntimeFixture, canToggleViewInflation) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));

    tree->setViewInflationEnabled(false);

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    tree->setViewInflationEnabled(true);

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, supportsLimitToViewportOnContainer) {
    float width = 100.0f;
    float height = 100.0f;

    ValueArrayBuilder entries;
    for (auto i = 0; i < 10; i++) {
        entries.emplace(Value());
    }

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("entries")] = Valdi::Value(entries.build());

    auto tree = wrapper.createViewNodeTreeAndContext("test", "LimitToViewportContainer", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // We should have only one child view, because only the first item is visible
    ASSERT_EQ(1, static_cast<int>(getRootView(tree).getImpl().getChildren().size()));

    // Also making sure that inside the ViewNode itself, the View are correct.
    ASSERT_TRUE(tree->getViewForNodePath(parseNodePath("container[0]")) != nullptr);
    ASSERT_EQ(tree->getViewForNodePath(parseNodePath("container[1]")), nullptr);

    // The child of container 0 should be there.
    ASSERT_TRUE(tree->getViewForNodePath(parseNodePath("container[0].child")) != nullptr);
    // The child of the other containers should also not be there.
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[1].child")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[2].child")));

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 50));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // We should have two child views, because the first two views are now visible
    ASSERT_EQ(2, static_cast<int>(getRootView(tree).getImpl().getChildren().size()));

    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[0]")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1]")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[0].child")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1].child")));

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 100));
    });
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // Now, only the second View is visible
    ASSERT_EQ(1, static_cast<int>(getRootView(tree).getImpl().getChildren().size()));

    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[0]")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[0].child")));

    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1]")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1].child")));

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, -width));
    });
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // No views visible now
    ASSERT_EQ(0, static_cast<int>(getRootView(tree).getImpl().getChildren().size()));

    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[0]")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[0].child")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[1]")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[1].child")));
}

TEST_P(RuntimeFixture, supportsLimitToViewportOnChildren) {
    float width = 100.0f;
    float height = 100.0f;

    ValueArrayBuilder entries;
    for (auto i = 0; i < 10; i++) {
        entries.emplace(Value());
    }

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("entries")] = Valdi::Value(entries.build());

    auto tree = wrapper.createViewNodeTreeAndContext("test", "LimitToViewportChildren", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // We should have all views directly children of the root
    ASSERT_EQ(10, static_cast<int>(getRootView(tree).getImpl().getChildren().size()));

    // The child of container 0 should be there.
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[0].child")));

    // The child of the other containers should not be there.
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[1].child")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[2].child")));

    // "parent" should all be there
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[0].parent")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1].parent")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[2].parent")));

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 100));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // We should have all views directly children of the root
    ASSERT_EQ(10, static_cast<int>(getRootView(tree).getImpl().getChildren().size()));

    // The child of container 1 should be there.
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1].child")));

    // The child of the other containers should not be there.
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[0].child")));
    ASSERT_EQ(nullptr, tree->getViewForNodePath(parseNodePath("container[2].child")));

    // "parent" should all be there
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[0].parent")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[1].parent")));
    ASSERT_TRUE(nullptr != tree->getViewForNodePath(parseNodePath("container[2].parent")));
}

TEST_P(RuntimeFixture, supportsLimitToViewportOnMultipleComponentLevels) {
    ValueArrayBuilder children;
    children.emplace(std::string("1"));
    children.emplace(std::string("2"));
    children.emplace(std::string("3"));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("children")] = Valdi::Value(children.build());

    auto singleChild =
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addChild(
                        DummyView("SCValdiView")
                            .addAttribute("padding", 10)
                            .addChild(
                                DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                            .addChild(DummyView("SCValdiLabel").addAttribute("dummyHeight", 45.5)))
                    .addChild(DummyView("UIButton").addAttribute("value", "Button 1")));

    auto tree =
        wrapper.createViewNodeTreeAndContext("test", "LimitToViewportChildOfChildParent", Valdi::Value(viewModel));

    float width = 10.0f;
    float height = 10.0f;

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });

    auto nodeChildren = tree->getRootViewNode()->copyChildren();
    ASSERT_EQ(3, static_cast<int>(nodeChildren.size()));

    assertHasNoView(nodeChildren[0]);
    assertHasNoView(nodeChildren[1]);
    assertHasNoView(nodeChildren[2]);

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(DummyView("SCValdiView").addChild(singleChild.addAttribute("customTag", "1")), getRootView(tree));

    assertAllHasView(nodeChildren[0]);
    assertHasNoView(nodeChildren[1]);
    assertHasNoView(nodeChildren[2]);

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height));
    });
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(DummyView("SCValdiView").addChild(singleChild.addAttribute("customTag", "2")), getRootView(tree));

    assertHasNoView(nodeChildren[0]);
    assertAllHasView(nodeChildren[1]);
    assertHasNoView(nodeChildren[2]);

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height * 2));
    });
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(DummyView("SCValdiView").addChild(singleChild.addAttribute("customTag", "3")), getRootView(tree));

    assertHasNoView(nodeChildren[0]);
    assertHasNoView(nodeChildren[1]);
    assertAllHasView(nodeChildren[2]);

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height * 3));
    });
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    assertHasNoView(nodeChildren[0]);
    assertHasNoView(nodeChildren[1]);
    assertHasNoView(nodeChildren[2]);
}

TEST_P(RuntimeFixture, recalculateLayoutOnFrameChange) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "LayoutInvalidation");

    wrapper.waitUntilAllUpdatesCompleted();

    auto frame = tree->getViewNodeForNodePath(parseNodePath("subview"))->getCalculatedFrame();
    ASSERT_EQ(0.0, frame.width);
    ASSERT_EQ(0.0, frame.height);

    tree->setLayoutSpecs(Size(100.0f, 100.0f), LayoutDirectionLTR);

    frame = tree->getViewNodeForNodePath(parseNodePath("subview"))->getCalculatedFrame();
    ASSERT_EQ(50.0, frame.width);
    ASSERT_EQ(50.0, frame.height);

    tree->setLayoutSpecs(Size(400.0f, 400.0f), LayoutDirectionLTR);

    frame = tree->getViewNodeForNodePath(parseNodePath("subview"))->getCalculatedFrame();
    ASSERT_EQ(200.0, frame.width);
    ASSERT_EQ(200.0, frame.height);
}

TEST_P(RuntimeFixture, passesViewModelAtInit) {
    auto viewModelMap = makeShared<ValueMap>();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ViewModelAtInitParent", Valdi::Value(viewModelMap));

    wrapper.waitUntilAllUpdatesCompleted();

    std::promise<bool> promise;
    auto future = promise.get_future();

    auto callback = makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        promise.set_value(callContext.getParameterAsBool(0));
        return Value::undefined();
    });

    ValueArrayBuilder parameters;
    parameters.emplace(callback);

    auto functionName = STRING_LITERAL("hadViewModel");

    // Check that the parent had the view model initially.
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), functionName, parameters.build());

    ASSERT_EQ(true, waitForFuture(future));
}

TEST_P(RuntimeFixture, supportMarginShorthand) {
    wrapper.standaloneRuntime->getViewManager().setRegisterCustomAttributes(false);

    float width = 100.0f;
    float height = 100.0f;

    auto tree = wrapper.createViewNodeTreeAndContext("test", "Margin");

    wrapper.waitUntilAllUpdatesCompleted();

    auto parent = tree->getViewNodeForNodePath(parseNodePath("parent"));
    auto child = tree->getViewNodeForNodePath(parseNodePath("child"));

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(25.0f, 25.0f, 50.0f, 50.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(0.0f, 0.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("margin")] = Value(STRING_LITERAL("2"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(23.0f, 23.0f, 54.0f, 54.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(2.0f, 2.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("margin")] = Value(STRING_LITERAL("2 4"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(21.0f, 23.0f, 58.0f, 54.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(4.0f, 2.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("margin")] = Value(STRING_LITERAL("2 4 6"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(21.0f, 21.0f, 58.0f, 58.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(4.0f, 2.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("margin")] = Value(STRING_LITERAL("2 4 6 8"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(19.0f, 21.0f, 62.0f, 58.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(8.0f, 2.0f, 50.0f, 50.0f), child->getCalculatedFrame());
}

TEST_P(RuntimeFixture, supportMarginShorthandOverride) {
    wrapper.standaloneRuntime->getViewManager().setRegisterCustomAttributes(false);

    float width = 100.0f;
    float height = 100.0f;

    auto tree = wrapper.createViewNodeTreeAndContext("test", "Margin");

    wrapper.waitUntilAllUpdatesCompleted();

    auto parent = tree->getViewNodeForNodePath(parseNodePath("parent"));
    auto child = tree->getViewNodeForNodePath(parseNodePath("child"));

    // Order:
    // margin-top: 2
    // margin-right: 4
    // margin-bottom: 6
    // margin-left: 8

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("margin")] = Value(STRING_LITERAL("2 4 6 8"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(19.0f, 21.0f, 62.0f, 58.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(8.0f, 2.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("marginLeft")] = Value(STRING_LITERAL("10"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(18.0f, 21.0f, 64.0f, 58.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(10.0f, 2.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("marginTop")] = Value(STRING_LITERAL("10"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(18.0f, 17.0f, 64.0f, 66.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(10.0f, 10.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("marginRight")] = Value(STRING_LITERAL("10"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(15.0f, 17.0f, 70.0f, 66.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(10.0f, 10.0f, 50.0f, 50.0f), child->getCalculatedFrame());

    (*viewModel)[STRING_LITERAL("marginBottom")] = Value(STRING_LITERAL("10"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(15.0f, 15.0f, 70.0f, 70.0f), parent->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(10.0f, 10.0f, 50.0f, 50.0f), child->getCalculatedFrame());
}

TEST_P(RuntimeFixture, supportsSlot) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "SlotTest");

    wrapper.waitUntilAllUpdatesCompleted();

    // At the beginning, only the footer should be rendered
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "header-container"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));

    auto viewModel = makeShared<ValueMap>();
    ValueArrayBuilder labels;
    labels.emplace(std::string("Label 1"));
    labels.emplace(std::string("Label 2"));
    labels.emplace(std::string("Label 3"));

    (*viewModel)[STRING_LITERAL("labels")] = Valdi::Value(labels.build());

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We should now have the labels inside the default slot
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "header-container"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 3"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));

    // We now add the header

    (*viewModel)[STRING_LITERAL("renderHeader")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We should have the header in the header container
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "header-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#ff0000")))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 3"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));

    // We make the SlotChild insert a view

    auto slotViewModel = makeShared<ValueMap>();
    (*slotViewModel)[STRING_LITERAL("renderInside")] = Valdi::Value(true);

    (*viewModel)[STRING_LITERAL("slotViewModel")] = Valdi::Value(slotViewModel);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We should have the view inside
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "header-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#ff0000")))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 3"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "inside"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));

    // We try removing a label
    labels.remove(1);

    (*viewModel)[STRING_LITERAL("labels")] = Valdi::Value(labels.build());

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "header-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#ff0000")))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 3"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "inside"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));

    // Remove an entire slot
    (*viewModel)[STRING_LITERAL("renderHeader")] = Valdi::Value(false);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "header-container"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 3"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "inside"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));

    // Removing the view inside the child

    (*slotViewModel)[STRING_LITERAL("renderInside")] = Valdi::Value(false);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "top"))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "header-container"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Label 3"))
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "footer-container")
                                              .addChild(DummyView("SCValdiView").addAttribute("color", "#00ff00")))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canHandleRenderIfInSlot) {
    auto childViewModel = makeShared<ValueMap>();
    (*childViewModel)[STRING_LITERAL("renderSlot")] = Valdi::Value(false);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("childViewModel")] = Valdi::Value(childViewModel);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "SlotRenderIfParent", Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We should not have the container nor the child, since the container has renderIf false
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));

    (*childViewModel)[STRING_LITERAL("renderSlot")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // We should now have the container and the child

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "container")
                                              .addChild(DummyView("SCValdiView"))
                                              .addChild(DummyView("SCValdiView").addAttribute("id", "child"))
                                              .addChild(DummyView("SCValdiView")))),
              getRootView(tree));

    (*childViewModel)[STRING_LITERAL("renderSlot")] = Valdi::Value(false);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // Since we have renderIf switched to false, both the container and the child should be gone
    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));
}

TEST_P(RuntimeFixture, canHotReloadRootComponent) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    auto parentView = DummyView("MyParentView");
    parentView.getImpl().addChild(getDummyView(tree->getRootView()).getSharedImpl());

    ASSERT_EQ(DummyView("MyParentView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton")))),
              parentView);

    auto result = wrapper.hotReload("test/src/StaticAttributes.vue.generated", "test/src/BasicViewTree.vue.generated");
    ASSERT_TRUE(result.success());

    ASSERT_FALSE(tree->getContext()->isDestroyed());
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("MyParentView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("color", "white")
                    .addChild(DummyView("SCValdiLabel").addAttribute("value", "Title").addAttribute("font", "title"))
                    .addChild(DummyView("UIButton").addAttribute("dummyHeight", 40).addAttribute("value", "Button 1"))
                    .addChild(
                        DummyView("UIButton").addAttribute("dummyHeight", 45.0).addAttribute("value", "Button 2"))),
        parentView);
}

TEST_P(RuntimeFixture, canHotReloadChildComponent) {
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("childDocumentName")] = Value(STRING_LITERAL("test/src/JSInstance.valdi"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "DynamicChildDocument", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto parentView = DummyView("MyParentView");
    parentView.getImpl().addChild(getDummyView(tree->getRootView()).getSharedImpl());

    ASSERT_EQ(DummyView("MyParentView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "rootView")
                                              .addAttribute("color", "white")
                                              .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
                                              .addChild(DummyView("SCValdiView")
                                                            .addChild(DummyView("UIButton")
                                                                          .addAttribute("id", "button1")
                                                                          .addAttribute("value", "Button 1"))
                                                            .addChild(DummyView("UIButton")
                                                                          .addAttribute("id", "button2")
                                                                          .addAttribute("value", "Button 2"))
                                                            .addChild(DummyView("UIButton")
                                                                          .addAttribute("id", "button3")
                                                                          .addAttribute("value", "Button 3"))))),
              parentView);

    auto result = wrapper.hotReload("test/src/JSLabel.vue.generated", "test/src/JSInstance.vue.generated");
    ASSERT_TRUE(result.success());

    ASSERT_FALSE(tree->getContext()->isDestroyed());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("MyParentView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "This is a JS label"))),
              parentView);
}

TEST_P(RuntimeFixture, passLatestViewModelWhenHotReloading) {
    auto viewModel = makeShared<ValueMap>();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "DynamicAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(tree));

    (*viewModel)[STRING_LITERAL("title")] = Valdi::Value(std::string("Hello"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(tree));

    ASSERT_EQ(Value(viewModel), tree->getContext()->getViewModel()->toValue());

    auto result =
        wrapper.hotReload("test/src/DynamicAttributes.vue.generated", "test/src/DynamicAttributes.vue.generated");
    ASSERT_TRUE(result.success());

    ASSERT_FALSE(tree->getContext()->isDestroyed());

    auto newTree = wrapper.runtime->getViewNodeTreeManager().getAllRootViewNodeTrees()[0];
    // We should have the same tree
    ASSERT_EQ(tree, newTree);
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value(viewModel), newTree->getContext()->getViewModel()->toValue());

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(newTree));

    // We should have those renders:
    // - Initial render
    // - Updated view model
    // - Destroy tree
    // - Re-render with initial view model
    // - Re-render with updated view model
    ASSERT_EQ(5, wrapper.runtimeListener->numberOfRenders);
}

TEST_P(RuntimeFixture, doesntPassViewModelWhenHotReloadingIfViewModelWasntInitiallyProvided) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "DynamicAttributes", Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(tree));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("title")] = Valdi::Value(std::string("Hello"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto result =
        wrapper.hotReload("test/src/DynamicAttributes.vue.generated", "test/src/DynamicAttributes.vue.generated");
    ASSERT_TRUE(result.success());

    ASSERT_FALSE(tree->getContext()->isDestroyed());

    auto newTree = wrapper.runtime->getViewNodeTreeManager().getAllRootViewNodeTrees()[0];
    // We should have the same tree
    ASSERT_EQ(tree, newTree);

    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(Value(viewModel), newTree->getContext()->getViewModel()->toValue());

    // Should have one empty render

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(newTree));

    // Then one render with the initial view model
    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(newTree));

    wrapper.waitForNextRenderRequest();

    // And another render with the last view model

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(newTree));
}

TEST_P(RuntimeFixture, canHotReloadRootComponentWithMissingDependencies) {
    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("MyComponent@test/src/ComponentWithMissingDependency"), Value(), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    // This component should have failed to load
    ASSERT_FALSE(tree->getRootViewNode() != nullptr);

    // We hot reload the file that is missing
    std::string fileToInject = "module.exports.value = 'I am alive and well';";

    wrapper.hotReload(STRING_LITERAL("test"), STRING_LITERAL("src/NewFile.js"), fileToInject);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_FALSE(tree->getContext()->isDestroyed());

    // We should have a tree this time
    ASSERT_TRUE(tree->getRootViewNode() != nullptr);

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiLabel").addAttribute("value", "I am alive and well")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canHotReloadRootComponentWithoutFile) {
    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("MyComponent@test/src/IDontExist"), Value(), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    // This component should have failed to load
    ASSERT_FALSE(tree->getRootViewNode() != nullptr);

    std::string fileToInject;

    fileToInject = "const Component = require('valdi_core/src/Component').Component;\n";
    fileToInject += "const jsx = require('valdi_core/src/JSX').jsx;\n";
    fileToInject += "const node = jsx.makeNodePrototype('view');\n";
    fileToInject += "class MyComponent extends Component {\n";
    fileToInject += "  onRender() {\n";
    fileToInject += "    jsx.beginRender(node);\n";
    fileToInject += "    jsx.setAttribute('value', 'Hello World');\n";
    fileToInject += "    jsx.endRender();\n";
    fileToInject += "  }\n";
    fileToInject += "}\n";
    fileToInject += "module.exports.MyComponent = MyComponent;\n";

    wrapper.hotReload(STRING_LITERAL("test"), STRING_LITERAL("src/IDontExist.js"), fileToInject);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_FALSE(tree->getContext()->isDestroyed());

    // We should have a tree this time
    ASSERT_TRUE(tree->getRootViewNode() != nullptr);
    ASSERT_EQ(DummyView("SCValdiView").addAttribute("value", "Hello World"), getRootView(tree));
}

TEST_P(RuntimeFixture, canHotReloadCoreFiles) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    auto parentView = DummyView("MyParentView");
    parentView.getImpl().addChild(getDummyView(tree->getRootView()).getSharedImpl());

    ASSERT_EQ(DummyView("MyParentView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton")))),
              parentView);

    std::vector<std::string> coreFilesToCheck = {"valdi_core/src/JSX",
                                                 "valdi_core/src/Renderer",
                                                 "valdi_core/src/RootComponentsManager",
                                                 "valdi_core/src/EntryPointComponent"};

    for (const auto& coreFile : coreFilesToCheck) {
        auto result = wrapper.hotReload(coreFile.c_str(), coreFile.c_str());
        ASSERT_TRUE(result.success());

        wrapper.waitForNextRenderRequest();

        ASSERT_EQ(DummyView("MyParentView")
                      .addChild(DummyView("SCValdiView")
                                    .addChild(DummyView("SCValdiLabel"))
                                    .addChild(DummyView("SCValdiView")
                                                  .addChild(DummyView("UIButton"))
                                                  .addChild(DummyView("UIButton"))
                                                  .addChild(DummyView("UIButton")))),
                  parentView);
    }

    // For valdi_core/src/Valdi, we should have a regular hot reload
    // since RootComponentsManager is not going to be unloaded

    auto result = wrapper.hotReload("valdi_core/src/Valdi", "valdi_core/src/Valdi");
    ASSERT_TRUE(result.success());

    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(DummyView("MyParentView").addChild(DummyView("SCValdiView")), parentView);
    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(DummyView("MyParentView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton")))),
              parentView);
}

TEST_P(RuntimeFixture, hotReloadRemovesDanglingViewNodes) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    auto parentView = DummyView("MyParentView");
    parentView.getImpl().addChild(getDummyView(tree->getRootView()).getSharedImpl());

    ASSERT_EQ(DummyView("MyParentView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton"))
                                              .addChild(DummyView("UIButton")))),
              parentView);

    ASSERT_TRUE(tree->getRootViewNode() != nullptr);
    ASSERT_EQ(static_cast<RawViewNodeId>(1), tree->getRootViewNode()->getRawId());

    // Do a normal hot reload first

    auto result = wrapper.hotReload("test/src/BasicViewTree.vue.generated", "test/src/BasicViewTree.vue.generated");
    ASSERT_TRUE(result.success());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_TRUE(tree->getRootViewNode() != nullptr);
    ASSERT_EQ(static_cast<RawViewNodeId>(7), tree->getRootViewNode()->getRawId());

    // All the previous view nodes should have been removed
    for (RawViewNodeId i = 1; i < 7; i++) {
        ASSERT_TRUE(tree->getViewNode(i) == nullptr);
    }

    // We now reload a core file

    result = wrapper.hotReload("valdi_core/src/RootComponentsManager", "valdi_core/src/RootComponentsManager");
    ASSERT_TRUE(result.success());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_TRUE(tree->getRootViewNode() != nullptr);
    // We should have reset the view node id to 1
    ASSERT_EQ(static_cast<RawViewNodeId>(1), tree->getRootViewNode()->getRawId());

    // All the previous view nodes should have been removed, even if the Renderer was recreated under the hood
    for (RawViewNodeId i = 7; i < 13; i++) {
        ASSERT_TRUE(tree->getViewNode(i) == nullptr);
    }
}

TEST_P(RuntimeFixture, callOnLayout) {
    wrapper.standaloneRuntime->getViewManager().setRegisterCustomAttributes(false);
    wrapper.runtimeListener->layoutTreeAutomatically = false;

    std::promise<Valdi::Value> promise;
    auto future = promise.get_future();

    auto callback = makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        if (callContext.getParametersSize() == 1) {
            promise.set_value(callContext.getParameter(0));
        }
        return Value::undefined();
    });

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("layoutCallback")] = Valdi::Value(callback);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "OnLayout", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    tree->setLayoutSpecs(Size(100.0, 100.0), LayoutDirectionLTR);

    auto frame = waitForFuture(future);

    ASSERT_TRUE(frame.getMap() != nullptr);

    ASSERT_EQ(Valdi::Frame(25.0, 25.0, 50.0, 50.0), Valdi::Frame(*frame.getMap()));
}

TEST_P(RuntimeFixture, doesntSendDownArrayWhenThereIsNoChange) {
    auto card1 = makeShared<ValueMap>();
    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 1"));
    (*card1)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 1"));

    auto card2 = makeShared<ValueMap>();
    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("Title 2"));
    (*card2)[STRING_LITERAL("subtitle")] = Valdi::Value(std::string("Subtitle 2"));

    ValueArrayBuilder cards;
    cards.emplace(card1);
    cards.emplace(card2);

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ForEach", viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    wrapper.setViewModel(tree->getContext(), viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(1, wrapper.runtimeListener->numberOfRenders);
}

Valdi::SharedContext getContext(const std::vector<Valdi::SharedContext>& contexts, const char* documentName) {
    auto strDocumentName = STRING_LITERAL(documentName);
    for (const auto& context : contexts) {
        if (context->getPath().getResourceId().resourcePath == strDocumentName) {
            return context;
        }
    }
    return nullptr;
}

bool hasContext(const std::vector<Valdi::SharedContext>& contexts, const char* documentName) {
    auto ctx = getContext(contexts, documentName);
    return ctx != nullptr;
}

TEST_P(RuntimeFixture, handleComponentInSlot) {
    auto tree =
        wrapper.createViewNodeTreeAndContext("test", "ComponentInsideSlot", Valdi::Value(makeShared<ValueMap>()));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("renderChild")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModelParameters));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView")
                          .addChild(DummyView("SCValdiView")
                                        .addAttribute("id", "container")
                                        .addChild(DummyView("SCValdiView"))
                                        .addChild(DummyView("SCValdiView")
                                                      .addAttribute("id", "rootView")
                                                      .addAttribute("color", "white")
                                                      .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
                                                      .addChild(DummyView("SCValdiView")
                                                                    .addChild(DummyView("UIButton")
                                                                                  .addAttribute("id", "button1")
                                                                                  .addAttribute("value", "Button 1"))
                                                                    .addChild(DummyView("UIButton")
                                                                                  .addAttribute("id", "button2")
                                                                                  .addAttribute("value", "Button 2"))
                                                                    .addChild(DummyView("UIButton")
                                                                                  .addAttribute("id", "button3")
                                                                                  .addAttribute("value", "Button 3"))))
                                        .addChild(DummyView("SCValdiView")))),
        getRootView(tree));

    (*viewModelParameters)[STRING_LITERAL("renderChild")] = Valdi::Value(false);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModelParameters));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));
}

TEST_P(RuntimeFixture, forEachWithComponents) {
    ValueArrayBuilder children;
    children.emplace(true);
    children.emplace(true);
    children.emplace(true);

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("children")] = Valdi::Value(children.build());

    auto tree =
        wrapper.createViewNodeTreeAndContext("test", "ForEachWithComponents", Valdi::Value(viewModelParameters));

    // Wait for the parent and the 3 children to render.
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "rootView")
                    .addAttribute("color", "white")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
                    .addChild(
                        DummyView("SCValdiView")
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button1").addAttribute("value", "Button 1"))
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button2").addAttribute("value", "Button 2"))
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button3").addAttribute("value", "Button 3"))))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "rootView")
                    .addAttribute("color", "white")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
                    .addChild(
                        DummyView("SCValdiView")
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button1").addAttribute("value", "Button 1"))
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button2").addAttribute("value", "Button 2"))
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button3").addAttribute("value", "Button 3"))))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "rootView")
                    .addAttribute("color", "white")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
                    .addChild(
                        DummyView("SCValdiView")
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button1").addAttribute("value", "Button 1"))
                            .addChild(
                                DummyView("UIButton").addAttribute("id", "button2").addAttribute("value", "Button 2"))
                            .addChild(DummyView("UIButton")
                                          .addAttribute("id", "button3")
                                          .addAttribute("value", "Button 3")))),
        getRootView(tree));
}

TEST_P(RuntimeFixture, handlesLimitToViewportOnChildComponent) {
    auto tree =
        wrapper.createViewNodeTreeAndContext("test", "LimitToViewportChildComponentParent", Valdi::Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("id", "slotContainer"))),
        getRootView(tree));

    auto container = tree->getViewNodeForNodePath(parseNodePath("slotContainer"));
    ASSERT_TRUE(container != nullptr);
    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });

    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionLTR);

    auto jsInstanceViewRepresentation =
        DummyView("SCValdiView")
            .addAttribute("id", "rootView")
            .addAttribute("color", "white")
            .addChild(DummyView("SCValdiLabel").addAttribute("id", "label"))
            .addChild(
                DummyView("SCValdiView")
                    .addChild(DummyView("UIButton").addAttribute("id", "button1").addAttribute("value", "Button 1"))
                    .addChild(DummyView("UIButton").addAttribute("id", "button2").addAttribute("value", "Button 2"))
                    .addChild(DummyView("UIButton").addAttribute("id", "button3").addAttribute("value", "Button 3")));

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "slotContainer")
                                              .addChild(jsInstanceViewRepresentation.clone()))),
              getRootView(tree));

    ASSERT_EQ(static_cast<size_t>(4), container->getChildCount());

    ASSERT_TRUE(container->getChildAt(0)->hasView());
    ASSERT_FALSE(container->getChildAt(1)->hasView());
    ASSERT_FALSE(container->getChildAt(2)->hasView());
    ASSERT_FALSE(container->getChildAt(3)->hasView());

    assertHasNoView(container->getChildAt(1));
    assertHasNoView(container->getChildAt(2));
    assertHasNoView(container->getChildAt(3));

    tree->withLock([=]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 5));
    });
    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionLTR);

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView")
                                              .addAttribute("id", "slotContainer")
                                              .addChild(jsInstanceViewRepresentation.clone())
                                              .addChild(jsInstanceViewRepresentation.clone()))),
              getRootView(tree));

    ASSERT_TRUE(container->getChildAt(0)->hasView());
    ASSERT_TRUE(container->getChildAt(1)->hasView());
    ASSERT_FALSE(container->getChildAt(2)->hasView());
    ASSERT_FALSE(container->getChildAt(3)->hasView());

    assertHasNoView(container->getChildAt(2));
    assertHasNoView(container->getChildAt(3));

    tree->withLock([=]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 100));
    });
    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionLTR);

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView").addChild(DummyView("SCValdiView").addAttribute("id", "slotContainer"))),
        getRootView(tree));

    ASSERT_FALSE(container->getChildAt(0)->hasView());
    ASSERT_FALSE(container->getChildAt(1)->hasView());
    ASSERT_FALSE(container->getChildAt(2)->hasView());
    ASSERT_FALSE(container->getChildAt(3)->hasView());

    assertHasNoView(container->getChildAt(0));
    assertHasNoView(container->getChildAt(1));
    assertHasNoView(container->getChildAt(2));
    assertHasNoView(container->getChildAt(3));
}

TEST_P(RuntimeFixture, handlesTranslationsInLimitToViewport) {
    wrapper.runtime->setLimitToViewportDisabled(false);

    auto tree = wrapper.createViewNodeTreeAndContext(STRING_LITERAL("TestComponent@test/src/LimitToViewportTranslate"),
                                                     Value(makeShared<ValueMap>()),
                                                     Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootViewNode = tree->getRootViewNode();
    ASSERT_TRUE(rootViewNode != nullptr && rootViewNode->getChildCount() == 1);
    auto childViewNode = rootViewNode->getChildAt(0);

    tree->setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("SCValdiView")), getRootView(tree));

    ASSERT_EQ(Frame(0, 0, 100, 100), rootViewNode->getCalculatedFrame());
    ASSERT_EQ(Frame(0, 0, 50, 50), childViewNode->getCalculatedFrame());
    ASSERT_EQ(0.0, childViewNode->getTranslationX());
    ASSERT_EQ(0.0, childViewNode->getTranslationY());
    ASSERT_TRUE(childViewNode->isVisibleInViewport());

    auto renderFunc = Function<void(double, double)>([&](double translationX, double translationY) {
        auto viewModel = makeShared<ValueMap>();
        (*viewModel)[STRING_LITERAL("translationX")] = Value(translationX);
        (*viewModel)[STRING_LITERAL("translationY")] = Value(translationY);
        wrapper.setViewModel(tree->getContext(), Value(std::move(viewModel)));
        wrapper.waitUntilAllUpdatesCompleted();
    });

    renderFunc(50, 50);

    ASSERT_EQ(50.0, childViewNode->getTranslationX());
    ASSERT_EQ(50.0, childViewNode->getTranslationY());
    ASSERT_TRUE(childViewNode->isVisibleInViewport());

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView").addAttribute("translationX", 50.0).addAttribute("translationY", 50.0)),
        getRootView(tree));

    renderFunc(100, 100);

    ASSERT_EQ(100.0, childViewNode->getTranslationX());
    ASSERT_EQ(100.0, childViewNode->getTranslationY());
    ASSERT_FALSE(childViewNode->isVisibleInViewport());

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    renderFunc(99, 99);

    ASSERT_EQ(99.0, childViewNode->getTranslationX());
    ASSERT_EQ(99.0, childViewNode->getTranslationY());
    ASSERT_TRUE(childViewNode->isVisibleInViewport());

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView").addAttribute("translationX", 99.0).addAttribute("translationY", 99.0)),
        getRootView(tree));
}

TEST_P(RuntimeFixture, slotCanApplyAttributeDynamicallyToChild) {
    auto tree =
        wrapper.createViewNodeTreeAndContext("test", "SlotApplyAttributeParent", Valdi::Value(makeShared<ValueMap>()));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "myChild").addAttribute("value", "Item 0"))
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "myChild").addAttribute("value", "Item 1"))),
        getRootView(tree));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("valueInSlot")] = Valdi::Value(STRING_LITERAL("Hello"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel")
                                              .addAttribute("id", "myChild")
                                              .addAttribute("value", "Item 0")
                                              .addAttribute("text", "Hello"))
                                .addChild(DummyView("SCValdiLabel")
                                              .addAttribute("id", "myChild")
                                              .addAttribute("value", "Item 1")
                                              .addAttribute("text", "Hello"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, callViewCallbacks) {
    std::atomic_int onCreateCallCount;
    std::atomic_int onDestroyCallCount;
    std::atomic_int onChangeCallCount;
    Atomic<std::vector<Value>> onChangeCallbackResults;

    onCreateCallCount = 0;
    onDestroyCallCount = 0;
    onChangeCallCount = 0;

    Ref<ValueFunction> onCreateCallback(makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        ++onCreateCallCount;
        return Value::undefined();
    }));
    Ref<ValueFunction> onDestroyCallback(makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        ++onDestroyCallCount;
        return Value::undefined();
    }));
    Ref<ValueFunction> onChangeCallback(makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        ++onChangeCallCount;
        onChangeCallbackResults.mutate([&](auto& results) { results.emplace_back(callContext.getParameter(0)); });
        return Value::undefined();
    }));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("shouldRender")] = Valdi::Value(false);
    (*viewModel)[STRING_LITERAL("onViewCreate")] = Valdi::Value(onCreateCallback);
    (*viewModel)[STRING_LITERAL("onViewDestroy")] = Valdi::Value(onDestroyCallback);
    (*viewModel)[STRING_LITERAL("onViewChange")] = Valdi::Value(onChangeCallback);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ViewCallbacks", Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();
    // Wait until the runloop has flushed
    wrapper.flushJsQueue();

    ASSERT_EQ(0, onCreateCallCount);
    ASSERT_EQ(0, onDestroyCallCount);
    ASSERT_EQ(0, onChangeCallCount);

    (*viewModel)[STRING_LITERAL("shouldRender")] = Valdi::Value(true);

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();
    // Wait until the runloop has flushed
    wrapper.flushJsQueue();

    ASSERT_EQ(1, onCreateCallCount);
    ASSERT_EQ(0, onDestroyCallCount);
    ASSERT_EQ(1, onChangeCallCount);

    auto nodeRefs = onChangeCallbackResults.load();

    ASSERT_EQ(static_cast<size_t>(1), nodeRefs.size());

    auto nodeRef = nodeRefs[0].getTypedRef<StandaloneNodeRef>();

    ASSERT_TRUE(nodeRef != nullptr);

    ASSERT_TRUE(nodeRef->getNode() != nullptr);
    ASSERT_TRUE(nodeRef->isInPlatformReference());

    // TODO(simon): Should test onDestroy, but right now it doesn't work with render-if, because a render-if that
    // evaluates to false automatically removes all attributes in JS, including the callbacks
}

TEST_P(RuntimeFixture, supportsNativeNodeReference) {
    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("NativeNodeComponent@test/src/NativeNode"), Value(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    Atomic<Value> receivedNode;

    auto callback = makeShared<ValueFunctionWithCallable>([&](const auto& callContext) {
        receivedNode.set(callContext.getParameter(0));
        return Value();
    });

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("receiveNode"), ValueArray::make({Value(callback)}));

    wrapper.flushQueues();

    auto nodeRef = receivedNode.load().getTypedRef<StandaloneNodeRef>();

    ASSERT_TRUE(nodeRef != nullptr);
    ASSERT_TRUE(nodeRef->getNode() != nullptr);
    ASSERT_FALSE(nodeRef->isInPlatformReference());
}

TEST_P(RuntimeFixture, supportsBidirectionalAttributes) {
    ValueArrayBuilder labels;
    labels.emplace(std::string("Hello 1"));
    labels.emplace(std::string("Hello 2"));
    labels.emplace(std::string("Hello 3"));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("labels")] = Valdi::Value(labels.build());

    auto tree = wrapper.createViewNodeTreeAndContext("test", "BidirectionalAttributes", Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 1")))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 2")))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 3"))),
        getRootView(tree));

    // We manually modify the middle label's value.
    getRootView(tree).getChild(1).getChild(0).addAttribute("value", "Goodbye");

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // Beside us passing a value of "Hello" in the view model, we should still have "Goodbye", because JS
    // and the runtime doesn't know the value changed.
    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 1")))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Goodbye")))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 3"))),
        getRootView(tree));

    auto viewNode = tree->getViewNodeForNodePath(parseNodePath("container[1].label"));

    ASSERT_TRUE(viewNode != nullptr);

    wrapper.runtime->updateAttributeState(*viewNode, STRING_LITERAL("value"), Valdi::Value(std::string("Goodbye")));

    wrapper.setViewModel(tree->getContext(), Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    // This time, the value "Hello" should be placed back because we notified the runtime that we moved the value to
    // "Goodbye"
    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 1")))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 2")))
            .addChild(
                DummyView("SCValdiView")
                    .addAttribute("id", "container")
                    .addChild(DummyView("SCValdiLabel").addAttribute("id", "label").addAttribute("value", "Hello 3"))),
        getRootView(tree));
}

TEST_P(RuntimeFixture, canSetBidirectionAttributeWithoutInitialValue) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));

    auto* node = tree->getRootViewNode()->getChildAt(0);

    auto attributeId = wrapper.runtime->getAttributeIds().getIdForName("value");

    ASSERT_EQ(Value::undefined(), node->getAttributesApplier().getResolvedAttributeValue(attributeId));

    wrapper.runtime->updateAttributeState(*node, STRING_LITERAL("value"), Valdi::Value(std::string("Hello")));

    ASSERT_EQ(Value(STRING_LITERAL("Hello")), node->getAttributesApplier().getResolvedAttributeValue(attributeId));
}

TEST_P(RuntimeFixture, handleRTLOnScrollView) {
    auto viewModel = makeShared<ValueMap>();

    ValueArrayBuilder entries;
    entries.emplace(STRING_LITERAL("item1"));
    entries.emplace(STRING_LITERAL("item2"));
    entries.emplace(STRING_LITERAL("item3"));

    (*viewModel)[STRING_LITERAL("entries")] = Valdi::Value(entries.build());

    auto tree = wrapper.createViewNodeTreeAndContext("test", "RTLScrollView", Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootNode = tree->getRootViewNode();
    auto container = tree->getViewNodeForNodePath(parseNodePath("container"));
    auto entry1 = tree->getViewNodeForNodePath(parseNodePath("entries[0]"));
    auto entry2 = tree->getViewNodeForNodePath(parseNodePath("entries[1]"));
    auto entry3 = tree->getViewNodeForNodePath(parseNodePath("entries[2]"));

    ASSERT_TRUE(rootNode != nullptr && container != nullptr && entry1 != nullptr && entry2 != nullptr &&
                entry3 != nullptr);

    auto directionAttribute = wrapper.runtime->getAttributeIds().getIdForName("direction");

    tree->scheduleExclusiveUpdate([=]() {
        rootNode->setAttribute(tree->getCurrentViewTransactionScope(),
                               directionAttribute,
                               tree.get(),
                               Valdi::Value(std::string("ltr")),
                               nullptr);
        rootNode->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());
    });

    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionLTR);

    // padding left 1, right 2
    // margin left 3, margin right 4

    ASSERT_EQ(Valdi::Frame(4, 0, 10, 10), entry1->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(21, 0, 10, 10), entry2->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(38, 0, 10, 10), entry3->getCalculatedFrame());
    ASSERT_EQ(54, container->getChildrenSpaceWidth());

    tree->scheduleExclusiveUpdate([=]() {
        rootNode->setAttribute(tree->getCurrentViewTransactionScope(),
                               directionAttribute,
                               tree.get(),
                               Valdi::Value(std::string("rtl")),
                               nullptr);
        rootNode->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());
    });

    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionLTR);

    // contentSize should be

    ASSERT_EQ(Valdi::Frame(40, 0, 10, 10), entry1->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(23, 0, 10, 10), entry2->getCalculatedFrame());
    ASSERT_EQ(Valdi::Frame(6, 0, 10, 10), entry3->getCalculatedFrame());
    ASSERT_EQ(54, container->getChildrenSpaceWidth());
}

TEST_P(RuntimeFixture, notifiesRTLStateOnView) {
    auto viewModel = makeShared<ValueMap>();

    ValueArrayBuilder entries;
    entries.emplace(STRING_LITERAL("item1"));

    (*viewModel)[STRING_LITERAL("entries")] = Valdi::Value(entries.build());

    auto tree = wrapper.createViewNodeTreeAndContext("test", "RTLScrollView", Valdi::Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto container = tree->getViewNodeForNodePath(parseNodePath("container"));
    auto entry1 = tree->getViewNodeForNodePath(parseNodePath("entries[0]"));

    ASSERT_TRUE(container != nullptr && entry1 != nullptr);

    tree->scheduleExclusiveUpdate([=]() {
        container->setAttribute(tree->getCurrentViewTransactionScope(),
                                container->getAttributeIds().getIdForName("paddingLeft"),
                                tree.get(),
                                Valdi::Value(0.0),
                                nullptr);
        container->setAttribute(tree->getCurrentViewTransactionScope(),
                                container->getAttributeIds().getIdForName("paddingRight"),
                                tree.get(),
                                Valdi::Value(0.0),
                                nullptr);
        container->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());

        entry1->setAttribute(tree->getCurrentViewTransactionScope(),
                             entry1->getAttributeIds().getIdForName("marginLeft"),
                             tree.get(),
                             Valdi::Value(0.0),
                             nullptr);
        entry1->setAttribute(tree->getCurrentViewTransactionScope(),
                             entry1->getAttributeIds().getIdForName("marginRight"),
                             tree.get(),
                             Valdi::Value(0.0),
                             nullptr);
        entry1->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());
    });

    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionLTR);

    ASSERT_EQ(Valdi::Frame(0, 0, 10, 10), entry1->getCalculatedFrame());
    ASSERT_FALSE(getDummyView(container->getView()).isRightToLeft());
    ASSERT_FALSE(getDummyView(entry1->getView()).isRightToLeft());

    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionRTL);

    ASSERT_EQ(Valdi::Frame(0, 0, 10, 10), entry1->getCalculatedFrame());
    ASSERT_TRUE(getDummyView(container->getView()).isRightToLeft());
    ASSERT_TRUE(getDummyView(entry1->getView()).isRightToLeft());

    tree->scheduleExclusiveUpdate([=]() {
        entry1->setAttribute(tree->getCurrentViewTransactionScope(),
                             entry1->getAttributeIds().getIdForName("direction"),
                             tree.get(),
                             Valdi::Value(STRING_LITERAL("ltr")),
                             nullptr);
        entry1->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());
    });

    tree->setLayoutSpecs(Size(10, 10), LayoutDirectionRTL);

    ASSERT_EQ(Valdi::Frame(0, 0, 10, 10), entry1->getCalculatedFrame());
    ASSERT_TRUE(getDummyView(container->getView()).isRightToLeft());
    ASSERT_FALSE(getDummyView(entry1->getView()).isRightToLeft());
}

TEST_P(RuntimeFixture, handleLayoutNodes) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "LayoutNodes");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("id", "outer")
                                .addChild(DummyView("SCValdiLabel").addAttribute("id", "inner")))
                  .addChild(DummyView("SCValdiView").addAttribute("id", "lonely"))
                  .addChild(DummyView("SCValdiLabel").addAttribute("id", "innerLayout")),
              getRootView(tree));

    tree->setLayoutSpecs(Size(Valdi::kUndefinedFloatValue, Valdi::kUndefinedFloatValue), LayoutDirectionLTR);

    auto rootNode = tree->getRootViewNode();
    auto outerNode = tree->getViewNodeForNodePath(parseNodePath("outer"));
    auto innerNode = tree->getViewNodeForNodePath(parseNodePath("inner"));
    auto lonelyNode = tree->getViewNodeForNodePath(parseNodePath("lonely"));
    auto innerLayoutNode = tree->getViewNodeForNodePath(parseNodePath("innerLayout"));

    ASSERT_TRUE(rootNode != nullptr);
    ASSERT_TRUE(outerNode != nullptr);
    ASSERT_TRUE(innerNode != nullptr);
    ASSERT_TRUE(lonelyNode != nullptr);
    ASSERT_TRUE(innerLayoutNode != nullptr);

    ASSERT_EQ(Valdi::Frame(4, 0, 10, 10), innerNode->getViewFrame());
    ASSERT_EQ(Valdi::Frame(12, 22, 18, 10), outerNode->getViewFrame());
    ASSERT_EQ(Valdi::Frame(10, 34, 5, 5), lonelyNode->getViewFrame());
    ASSERT_EQ(Valdi::Frame(14, 43, 10, 10), innerLayoutNode->getViewFrame());

    ASSERT_EQ(Valdi::Frame(0, 0, 32, 87), rootNode->getViewFrame());
}

TEST_P(RuntimeFixture, handleForEachKeysWithIndex) {
    ValueArrayBuilder items;

    auto item1 = makeShared<ValueMap>();
    auto item2 = makeShared<ValueMap>();
    auto item3 = makeShared<ValueMap>();

    items.emplace(item1);
    items.emplace(item2);
    items.emplace(item3);

    (*item1)[STRING_LITERAL("id")] = Value(std::string("Item 1"));
    (*item2)[STRING_LITERAL("id")] = Value(std::string("Item 2"));
    (*item3)[STRING_LITERAL("id")] = Value(std::string("Item 3"));

    auto viewModel = makeShared<ValueMap>();
    viewModel->insert(std::make_pair(STRING_LITERAL("useKeys"), Value(false)));
    viewModel->insert(std::make_pair(STRING_LITERAL("items"), Value(items.build())));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ForEachKeys", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    // Begin state, all labels should be set to their id

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))),
              getRootView(tree));

    // We shift items around
    (*item1)[STRING_LITERAL("id")] = Value(std::string("Item 3"));
    (*item2)[STRING_LITERAL("id")] = Value(std::string("Item 1"));
    (*item3)[STRING_LITERAL("id")] = Value(std::string("Item 2"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    // The new ids should be updated for each cell

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, handleForEachKeysWithKey) {
    ValueArrayBuilder items;

    auto item1 = makeShared<ValueMap>();
    auto item2 = makeShared<ValueMap>();
    auto item3 = makeShared<ValueMap>();

    items.emplace(Value(item1));
    items.emplace(Value(item2));
    items.emplace(Value(item3));

    (*item1)[STRING_LITERAL("id")] = Value(std::string("Item 1"));
    (*item2)[STRING_LITERAL("id")] = Value(std::string("Item 2"));
    (*item3)[STRING_LITERAL("id")] = Value(std::string("Item 3"));

    auto viewModel = makeShared<ValueMap>();
    viewModel->insert(std::make_pair(STRING_LITERAL("useKeys"), Value(true)));

    (*viewModel)[STRING_LITERAL("items")] = Value(items.build());

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ForEachKeys", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    // Begin state, all labels should be set to their id

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))),
              getRootView(tree));

    // We shift items around
    (*item1)[STRING_LITERAL("id")] = Value(std::string("Item 3"));
    (*item2)[STRING_LITERAL("id")] = Value(std::string("Item 1"));
    (*item3)[STRING_LITERAL("id")] = Value(std::string("Item 2"));

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.waitUntilAllUpdatesCompleted();

    // The individual cells should not have changed, just the ordering should have changed.

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))),
              getRootView(tree));

    // We insert an item at the beginning.

    auto item0 = makeShared<ValueMap>();
    (*item0)[STRING_LITERAL("id")] = Value(std::string("Item 0"));
    items.prepend(Value(item0));

    (*viewModel)[STRING_LITERAL("items")] = Value(items.build());

    wrapper.setViewModel(tree->getContext(), Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 0"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 0")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 3")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 1")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Item 2"))),
              getRootView(tree));

    // Beside our insertions, every view should have only 1 change of attribute, in other words,
    // the views should be re-used automatically, beside having been moved around.
    checkViewHasSingleHistoryPerAttribute(getRootView(tree));
}

TEST_P(RuntimeFixture, supportsNestedSlots) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "NestedSlots");

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiView").addAttribute("id", "first"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("SCValdiView").addAttribute("id", "beforeNestedSlot"))
                                              .addChild(DummyView("SCValdiView").addAttribute("id", "deepInside"))
                                              .addChild(DummyView("SCValdiView").addAttribute("id", "afterNestedSlot")))
                                .addChild(DummyView("SCValdiView").addAttribute("id", "last"))),
              getRootView(tree));

    // Ensure they can be properly deallocated after destruction
    std::vector<Valdi::Weak<ViewNode>> allViewNodes;
    forEachViewNode(tree->getRootViewNode(), [&](ViewNode* viewNode) { allViewNodes.emplace_back(weakRef(viewNode)); });

    wrapper.runtime->destroyContext(tree->getContext());

    auto numberOfUnexpired = 0;
    for (const auto& viewNodePtr : allViewNodes) {
        if (!viewNodePtr.expired()) {
            [[maybe_unused]] auto useCount = viewNodePtr.use_count();
            VALDI_ERROR(*wrapper.logger,
                        "ViewNode {} has not expired, use count {}",
                        viewNodePtr.lock()->getDebugId(),
                        useCount);
            numberOfUnexpired++;
        }
    }
    ASSERT_EQ(0, numberOfUnexpired);
}

Value lazyLayoutViewModel() {
    auto items = ValueArray::make(3);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("items")] = Value(items);

    return Value(viewModel);
}

TEST_P(RuntimeFixture, supportsLazyLayout) {
    wrapper.standaloneRuntime->getViewManager().setRegisterCustomAttributes(false);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("LazyLayout@test/src/LazyLayout"), lazyLayoutViewModel(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    auto innerShadowNodes = findViewNodesWithId(tree->getRootViewNode(), "inner");
    ASSERT_EQ(static_cast<size_t>(3), innerShadowNodes.size());

    float width = 40;
    float height = 40;

    auto scrollViewNode = tree->getRootViewNode()->getChildAt(0);
    tree->withLock(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0)); });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // But only the first cell should have its frame calculated
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[2]->getCalculatedFrame());

    // We now make the second cell visible
    tree->withLock(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height)); });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // The first and second cell should be calculated
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[2]->getCalculatedFrame());

    // Make third cell visible
    tree->withLock([=]() {
        scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height * 2));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // All cells should be calculated
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[2]->getCalculatedFrame());
}

TEST_P(RuntimeFixture, notifyLayoutBecameDirty) {
    wrapper.standaloneRuntime->getViewManager().setRegisterCustomAttributes(false);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("LazyLayout@test/src/LazyLayout"), lazyLayoutViewModel(), Value());

    ASSERT_EQ(0, wrapper.runtimeListener->numberOfLayoutBecameDirty);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(1, wrapper.runtimeListener->numberOfLayoutBecameDirty);

    float width = 40;
    float height = 40;

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(1, wrapper.runtimeListener->numberOfLayoutBecameDirty);

    // Submit a render request that doesn't mutate the layout tree

    auto renderRequest = Valdi::makeShared<RenderRequest>();
    renderRequest->setContextId(tree->getContext()->getContextId());
    auto* setElementAttribute = renderRequest->appendSetElementAttribute();
    setElementAttribute->setElementId(tree->getRootViewNode()->getRawId());

    setElementAttribute->setAttributeId(wrapper.runtime->getAttributeIds().getIdForName("backgroundColor"));
    setElementAttribute->setAttributeValue(Value(STRING_LITERAL("red")));

    wrapper.runtime->processRenderRequest(renderRequest);

    ASSERT_EQ(1, wrapper.runtimeListener->numberOfLayoutBecameDirty);

    // Now submit a render request that does mutate the layout tree

    setElementAttribute->setAttributeId(wrapper.runtime->getAttributeIds().getIdForName("padding"));
    setElementAttribute->setAttributeValue(Value(42.0));

    wrapper.runtime->processRenderRequest(renderRequest);

    ASSERT_EQ(2, wrapper.runtimeListener->numberOfLayoutBecameDirty);

    setElementAttribute->setAttributeId(wrapper.runtime->getAttributeIds().getIdForName("padding"));
    setElementAttribute->setAttributeValue(Value(60.0));

    wrapper.runtime->processRenderRequest(renderRequest);

    ASSERT_EQ(3, wrapper.runtimeListener->numberOfLayoutBecameDirty);
}

TEST_P(RuntimeFixture, canDisableAutoRendering) {
    wrapper.runtime->setAutoRenderDisabled(true);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.flushQueues();

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    ASSERT_TRUE(tree->getContext()->hasPendingRenderRequests());
    wrapper.runtime->setAutoRenderDisabled(false);

    wrapper.flushQueues();

    ASSERT_FALSE(tree->getContext()->hasPendingRenderRequests());

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, disablesAutoRenderingWhileLoadOperationIsEnqueued) {
    auto group = makeShared<AsyncGroup>();

    group->enter();

    wrapper.runtimeManager->enqueueLoadOperation([group]() { group->blockingWait(); });

    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    wrapper.flushQueues();

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    ASSERT_TRUE(tree->getContext()->hasPendingRenderRequests());

    // Now let the load operation complete
    group->leave();
    wrapper.runtimeManager->flushLoadOperations();

    wrapper.flushQueues();
    ASSERT_FALSE(tree->getContext()->hasPendingRenderRequests());

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canRecalculateLazyLayout) {
    wrapper.standaloneRuntime->getViewManager().setRegisterCustomAttributes(false);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("LazyLayout@test/src/LazyLayout"), lazyLayoutViewModel(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    auto innerShadowNodes = findViewNodesWithId(tree->getRootViewNode(), "inner");
    ASSERT_EQ(static_cast<size_t>(3), innerShadowNodes.size());
    float width = 40;
    float height = 40;

    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[2]->getCalculatedFrame());

    auto marginAttribute = wrapper.runtime->getAttributeIds().getIdForName("margin");

    // We then modify the margin of the first cell
    tree->scheduleExclusiveUpdate([&]() {
        innerShadowNodes[0]->setAttribute(
            tree->getCurrentViewTransactionScope(), marginAttribute, tree.get(), Value(5.0), nullptr);
        innerShadowNodes[0]->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // The layout should have been updated.
    ASSERT_EQ(Frame(5, 5, 30, 30), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[2]->getCalculatedFrame());

    // We make the cell non visible.
    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height));
    });

    // Note: this additional calculateLayout() is here to fix a small bug,
    // the calculateLayout() pass first calculates the layout, then figures out the visibility for the nodes.
    // For our test here, this means calling calculateLayout() after we updated a margin will update our cell
    // which was previously visible, even though it became invisible. This should be an issue in real world examples.
    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // We modify the margin again
    tree->scheduleExclusiveUpdate([&]() {
        innerShadowNodes[0]->setAttribute(
            tree->getCurrentViewTransactionScope(), marginAttribute, tree.get(), Value(2.0), nullptr);
        innerShadowNodes[0]->getAttributesApplier().flush(tree->getCurrentViewTransactionScope());
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // The layout should NOT have been updated, because the cell is invisible, even
    // though we invalidated the node
    ASSERT_EQ(Frame(5, 5, 30, 30), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[2]->getCalculatedFrame());

    // We make the cell visible again
    tree->scheduleExclusiveUpdate([&]() {
        tree->getRootViewNode()->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, height - 1));
    });

    tree->setLayoutSpecs(Size(width, height), LayoutDirectionLTR);

    // This time it should be updated.
    ASSERT_EQ(Frame(2, 2, 36, 36), innerShadowNodes[0]->getCalculatedFrame());
    ASSERT_EQ(Frame(10, 10, 20, 20), innerShadowNodes[1]->getCalculatedFrame());
    ASSERT_EQ(Frame(), innerShadowNodes[2]->getCalculatedFrame());
}

TEST_P(RuntimeFixture, canNotifyViewportChanged) {
    SharedAtomic<Value> notifiedViewport, notifiedFrame;
    auto viewportUpdatesCallback =
        makeShared<ValueFunctionWithCallable>([notifiedViewport, notifiedFrame](const auto& parameters) {
            notifiedViewport.set(parameters.getParameter(0));
            notifiedFrame.set(parameters.getParameter(1));
            return Value();
        });

    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("ViewportUpdates@test/src/ViewportUpdates"),
                                             Value().setMapValue("onViewportChanged", Value(viewportUpdatesCallback)),
                                             Value());

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootViewNode = tree->getRootViewNode();
    ASSERT_TRUE(rootViewNode != nullptr);

    ASSERT_EQ(static_cast<size_t>(1), rootViewNode->getChildCount());

    auto scrollViewNode = rootViewNode->getChildAt(0);

    tree->setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);

    tree->scheduleExclusiveUpdate(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0)); });

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedViewport.get());

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedFrame.get());

    tree->scheduleExclusiveUpdate(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(20, 7)); });

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(7.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(93.0)),
              notifiedViewport.get());

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedFrame.get());

    tree->scheduleExclusiveUpdate(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(21, 200)); });

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(0.0))
                  .setMapValue("height", Value(0.0)),
              notifiedViewport.get());

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedFrame.get());

    // Now test horizontal

    tree->scheduleExclusiveUpdate([=]() { scrollViewNode->setHorizontalScroll(true); });
    tree->setLayoutSpecs(Size(100, 100), LayoutDirectionLTR);

    tree->scheduleExclusiveUpdate(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(0, 0)); });

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedViewport.get());

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedFrame.get());

    tree->scheduleExclusiveUpdate(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(20, 7)); });

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(20.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(80.0))
                  .setMapValue("height", Value(100.0)),
              notifiedViewport.get());

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedFrame.get());

    tree->scheduleExclusiveUpdate(
        [=]() { scrollViewNode->setScrollContentOffset(tree->getCurrentViewTransactionScope(), Point(200, 21)); });

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(0.0))
                  .setMapValue("height", Value(0.0)),
              notifiedViewport.get());

    ASSERT_EQ(Value()
                  .setMapValue("x", Value(0.0))
                  .setMapValue("y", Value(0.0))
                  .setMapValue("width", Value(100.0))
                  .setMapValue("height", Value(100.0)),
              notifiedFrame.get());
}

TEST_P(RuntimeFixture, supportsZIndex) {
    auto card1 = makeShared<ValueMap>();
    (*card1)[STRING_LITERAL("title")] = Valdi::Value(std::string("1"));
    (*card1)[STRING_LITERAL("zIndex")] = Valdi::Value(1);

    auto card2 = makeShared<ValueMap>();
    (*card2)[STRING_LITERAL("title")] = Valdi::Value(std::string("2"));
    (*card2)[STRING_LITERAL("zIndex")] = Valdi::Value(2);

    auto card3 = makeShared<ValueMap>();
    (*card3)[STRING_LITERAL("title")] = Valdi::Value(std::string("3"));
    (*card3)[STRING_LITERAL("zIndex")] = Valdi::Value(3);

    ValueArrayBuilder cards;
    cards.emplace(card1);
    cards.emplace(card2);
    cards.emplace(card3);

    auto viewModelParameters = makeShared<ValueMap>();
    (*viewModelParameters)[STRING_LITERAL("cards")] = Valdi::Value(cards.build());

    Valdi::Value viewModel = Valdi::Value(viewModelParameters);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "ForEach", viewModel);

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "1"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "2"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "3"))
                                .addChild(DummyView("SCValdiLabel"))),
              getRootView(tree));

    // Reverse the list
    (*card1)[STRING_LITERAL("zIndex")] = Valdi::Value(3);
    (*card2)[STRING_LITERAL("zIndex")] = Valdi::Value(2);
    (*card3)[STRING_LITERAL("zIndex")] = Valdi::Value(1);

    wrapper.setViewModel(tree->getContext(), viewModel);
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "3"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "2"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "1"))
                                .addChild(DummyView("SCValdiLabel"))),
              getRootView(tree));

    // Put card 1 in middle
    (*card1)[STRING_LITERAL("zIndex")] = Valdi::Value(2);
    (*card2)[STRING_LITERAL("zIndex")] = Valdi::Value(3);

    wrapper.setViewModel(tree->getContext(), viewModel);
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")

                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "3"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "1"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "2"))
                                .addChild(DummyView("SCValdiLabel"))),
              getRootView(tree));

    // Put card 2 at front
    (*card2)[STRING_LITERAL("zIndex")] = Valdi::Value(-100);

    wrapper.setViewModel(tree->getContext(), viewModel);
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "2"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("UIButton").addAttribute("value", "Button 1"))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "3"))
                                .addChild(DummyView("SCValdiLabel")))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("color", "white")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "1"))
                                .addChild(DummyView("SCValdiLabel"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canSwapRootViews) {
    auto rootView1 = Valdi::makeShared<StandaloneView>(STRING_LITERAL("MyRootView1"));
    auto rootView2 = Valdi::makeShared<StandaloneView>(STRING_LITERAL("MyRootView2"));

    auto tree = wrapper.runtime->createViewNodeTreeAndContext(wrapper.standaloneRuntime->getViewManagerContext(),
                                                              STRING_LITERAL("test/src/BasicViewTree.valdi"));

    wrapper.waitUntilAllUpdatesCompleted();
    tree->setLayoutSpecs(Size(200, 200), LayoutDirectionLTR);

    ASSERT_TRUE(tree->getRootView() == nullptr);
    ASSERT_FALSE(tree->getRootViewNode() == nullptr);

    tree->setRootView(rootView1);

    ASSERT_FALSE(tree->getRootView() == nullptr);

    ASSERT_EQ(DummyView("MyRootView1")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getDummyView(rootView1));

    ASSERT_EQ(DummyView("MyRootView2"), getDummyView(rootView2));

    tree->setRootView(rootView2);

    ASSERT_EQ(DummyView("MyRootView1"), getDummyView(rootView1));

    ASSERT_EQ(DummyView("MyRootView2")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getDummyView(rootView2));

    tree->setRootView(nullptr);

    ASSERT_EQ(DummyView("MyRootView1"), getDummyView(rootView1));

    ASSERT_EQ(DummyView("MyRootView2"), getDummyView(rootView2));
}

TEST_P(RuntimeFixture, clearingRootViewClearsTheWholeViewTree) {
    wrapper.standaloneRuntime->getViewManager().setAllowViewPooling(true);

    auto rootView = Valdi::makeShared<StandaloneView>(STRING_LITERAL("MyRootView"));

    auto tree = wrapper.runtime->createViewNodeTreeAndContext(wrapper.standaloneRuntime->getViewManagerContext(),
                                                              STRING_LITERAL("test/src/BasicViewTree.valdi"));

    wrapper.waitUntilAllUpdatesCompleted();
    tree->setLayoutSpecs(Size(200, 200), LayoutDirectionLTR);

    tree->setRootView(rootView);

    auto viewManagerContext = wrapper.standaloneRuntime->getViewManagerContext();

    ASSERT_EQ(DummyView("MyRootView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getDummyView(rootView));

    auto stats = viewManagerContext->getViewPoolsStats();

    ASSERT_EQ(0, getNumberOfPooledViews(STRING_LITERAL("SCValdiLabel"), stats));
    ASSERT_EQ(0, getNumberOfPooledViews(STRING_LITERAL("SCValdiView"), stats));
    ASSERT_EQ(0, getNumberOfPooledViews(STRING_LITERAL("UIButton"), stats));

    // Force layout to be dirty to prevent the update pass

    ASSERT_FALSE(tree->isLayoutSpecsDirty());
    tree->scheduleExclusiveUpdate([=]() { tree->onLayoutDirty(); });
    ASSERT_TRUE(tree->isLayoutSpecsDirty());

    tree->setRootView(nullptr);

    ASSERT_EQ(DummyView("MyRootView"), getDummyView(rootView));

    assertHasNoView(tree->getRootViewNode());

    stats = viewManagerContext->getViewPoolsStats();

    ASSERT_EQ(1, getNumberOfPooledViews(STRING_LITERAL("SCValdiLabel"), stats));
    ASSERT_EQ(1, getNumberOfPooledViews(STRING_LITERAL("SCValdiView"), stats));
    ASSERT_EQ(3, getNumberOfPooledViews(STRING_LITERAL("UIButton"), stats));
}

// class RawComponentHandler : public ContextHandler {
// public:
//    int onCreateCallCount = 0;
//    int onDestroyCallCount = 0;
//    ContextId contextId = ContextIdNull;
//
//    void onCreate(ContextId contextId, Value viewModel, Value componentContext) override {
//        this->contextId = contextId;
//        onCreateCallCount++;
//    }
//
//    void onDestroy(ContextId contextId) override {
//        onDestroyCallCount++;
//    }
//};
//
TEST_P(RuntimeFixture, supportsRawContexts) {
    wrapper.runtime->setLimitToViewportDisabled(true);

    // We now create the component, which should have been registered
    auto context = wrapper.runtime->createContext(
        wrapper.standaloneRuntime->getViewManagerContext(), STRING_LITERAL("valdi.RawComponent"), nullptr, nullptr);
    context->onCreate();

    auto view = DummyView("RootView");
    auto viewNodeTree = wrapper.runtime->createViewNodeTree(context);
    viewNodeTree->setRootView(view.getSharedImpl());

    ASSERT_EQ(DummyView("RootView"), getRootView(viewNodeTree));

    // We now start the render
    auto renderRequest = Valdi::makeShared<RenderRequest>();
    renderRequest->setContextId(context->getContextId());

    auto valueAttribute = wrapper.runtime->getAttributeIds().getIdForName("value");

    auto* createElement = renderRequest->appendCreateElement();
    createElement->setElementId(1);
    createElement->setViewClassName(STRING_LITERAL("UIView"));

    createElement = renderRequest->appendCreateElement();
    createElement->setElementId(2);
    createElement->setViewClassName(STRING_LITERAL("SCValdiLabel"));

    createElement = renderRequest->appendCreateElement();
    createElement->setElementId(3);
    createElement->setViewClassName(STRING_LITERAL("UIView"));

    createElement = renderRequest->appendCreateElement();
    createElement->setElementId(4);
    createElement->setViewClassName(STRING_LITERAL("SCValdiLabel"));

    auto* moveToParent = renderRequest->appendMoveElementToParent();
    moveToParent->setElementId(2);
    moveToParent->setParentElementId(1);

    moveToParent = renderRequest->appendMoveElementToParent();
    moveToParent->setElementId(4);
    moveToParent->setParentElementId(3);

    moveToParent = renderRequest->appendMoveElementToParent();
    moveToParent->setElementId(3);
    moveToParent->setParentElementId(1);
    moveToParent->setParentIndex(1);

    auto* setElementAttribute = renderRequest->appendSetElementAttribute();
    setElementAttribute->setElementId(2);
    setElementAttribute->setAttributeId(valueAttribute);
    setElementAttribute->setAttributeValue(Value(STRING_LITERAL("Im close to root")));

    setElementAttribute = renderRequest->appendSetElementAttribute();
    setElementAttribute->setElementId(4);
    setElementAttribute->setAttributeId(valueAttribute);
    setElementAttribute->setAttributeValue(Value(STRING_LITERAL("Im far from root")));

    auto* setRootElement = renderRequest->appendSetRootElement();
    setRootElement->setElementId(1);

    wrapper.runtime->processRenderRequest(renderRequest);

    ASSERT_EQ(DummyView("RootView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "Im close to root"))
                  .addChild(DummyView("UIView").addChild(
                      DummyView("SCValdiLabel").addAttribute("value", "Im far from root"))),
              view);

    // We do an incremental render

    renderRequest = Valdi::makeShared<RenderRequest>();
    renderRequest->setContextId(context->getContextId());

    auto* destroyElement = renderRequest->appendDestroyElement();
    destroyElement->setElementId(4);

    setElementAttribute = renderRequest->appendSetElementAttribute();
    setElementAttribute->setElementId(2);
    setElementAttribute->setAttributeId(valueAttribute);
    setElementAttribute->setAttributeValue(Value(STRING_LITERAL("Far from root was removed")));

    moveToParent = renderRequest->appendMoveElementToParent();
    moveToParent->setElementId(3);
    moveToParent->setParentElementId(1);
    moveToParent->setParentIndex(0);

    wrapper.runtime->processRenderRequest(renderRequest);

    ASSERT_EQ(DummyView("RootView")
                  .addChild(DummyView("UIView"))
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "Far from root was removed")),
              view);

    wrapper.runtime->destroyContext(context);

    ASSERT_EQ(DummyView("RootView"), view);
}

TEST_P(RuntimeFixture, renderRequestCanRetainAndReleaseItsEntries) {
    auto viewClass = STRING_LITERAL("ThisIsMyViewClass");
    auto attributeValue = makeShared<ValueMap>();

    auto animationsCompletionCallback =
        makeShared<ValueFunctionWithCallable>([](const auto& /*parameters*/) { return Value(); });
    auto layoutCompletionCallback =
        makeShared<ValueFunctionWithCallable>([](const auto& /*parameters*/) { return Value(); });

    ASSERT_EQ(1, viewClass.getInternedString().use_count());
    ASSERT_EQ(1, attributeValue.use_count());
    ASSERT_EQ(1, animationsCompletionCallback.use_count());
    ASSERT_EQ(1, layoutCompletionCallback.use_count());

    auto renderRequest = Valdi::makeShared<RenderRequest>();

    auto* createElement = renderRequest->appendCreateElement();
    createElement->setViewClassName(viewClass);

    renderRequest->appendDestroyElement();
    renderRequest->appendMoveElementToParent();
    renderRequest->appendSetRootElement();

    auto* setElementAttribute = renderRequest->appendSetElementAttribute();
    setElementAttribute->setAttributeValue(Value(attributeValue));

    auto* startAnimations = renderRequest->appendStartAnimations();
    startAnimations->getAnimationOptions().completionCallback = Value(animationsCompletionCallback);

    renderRequest->appendEndAnimations();

    auto* onLayoutComplete = renderRequest->appendOnLayoutComplete();
    onLayoutComplete->setCallback(layoutCompletionCallback);

    ASSERT_EQ(2, viewClass.getInternedString().use_count());
    ASSERT_EQ(2, attributeValue.use_count());
    ASSERT_EQ(2, animationsCompletionCallback.use_count());
    ASSERT_EQ(2, layoutCompletionCallback.use_count());

    renderRequest = nullptr;

    ASSERT_EQ(1, viewClass.getInternedString().use_count());
    ASSERT_EQ(1, attributeValue.use_count());
    ASSERT_EQ(1, animationsCompletionCallback.use_count());
    ASSERT_EQ(1, layoutCompletionCallback.use_count());
}

struct Visitor {
    template<typename T>
    void visit(T& entry) {
        auto* ptr = reinterpret_cast<uint8_t*>(&entry);
        ASSERT_EQ(static_cast<size_t>(0), reinterpret_cast<std::uintptr_t>(ptr) % alignof(T));
    }
};

TEST_P(RuntimeFixture, renderRequestVisitHandlesAlignment) {
    static_assert(sizeof(RenderRequestEntries::EndAnimations) == 1);

    auto renderRequest = Valdi::makeShared<RenderRequest>();

    // Start with a request type that is only one byte long
    renderRequest->appendEndAnimations();

    renderRequest->appendCreateElement();
    renderRequest->appendDestroyElement();
    renderRequest->appendMoveElementToParent();
    renderRequest->appendSetRootElement();
    renderRequest->appendSetElementAttribute();
    renderRequest->appendStartAnimations();
    renderRequest->appendEndAnimations();
    renderRequest->appendOnLayoutComplete();

    Visitor visitor;
    renderRequest->visitEntries(visitor);
}

TEST_P(RuntimeFixture, renderRequestCanSerialize) {
    AttributeIds attributeIds;

    auto callback = makeShared<ValueFunctionWithCallable>([](const auto& marshaller) { return Value(); });

    auto renderRequest = Valdi::makeShared<RenderRequest>();
    renderRequest->setContextId(1);

    auto* createElement = renderRequest->appendCreateElement();
    createElement->setElementId(42);
    createElement->setViewClassName(STRING_LITERAL("MyViewClass"));

    auto* destroyElement = renderRequest->appendDestroyElement();
    destroyElement->setElementId(32);

    auto* moveElementToParent = renderRequest->appendMoveElementToParent();
    moveElementToParent->setElementId(80);
    moveElementToParent->setParentElementId(90);
    moveElementToParent->setParentIndex(13);

    auto* setRootElement = renderRequest->appendSetRootElement();
    setRootElement->setElementId(1000);

    auto* setElementAttribute = renderRequest->appendSetElementAttribute();
    setElementAttribute->setElementId(1);
    setElementAttribute->setAttributeId(attributeIds.getIdForName("width"));
    setElementAttribute->setAttributeValue(Value(static_cast<int64_t>(1337)));
    setElementAttribute->setInjectedFromParent(true);

    auto* startAnimations = renderRequest->appendStartAnimations();
    startAnimations->getAnimationOptions().controlPoints = {0, 1};
    startAnimations->getAnimationOptions().completionCallback = Value(static_cast<int64_t>(42));
    startAnimations->getAnimationOptions().type = snap::valdi_core::AnimationType::EaseIn;
    startAnimations->getAnimationOptions().duration = 9;
    startAnimations->getAnimationOptions().beginFromCurrentState = true;
    startAnimations->getAnimationOptions().crossfade = false;
    startAnimations->getAnimationOptions().stiffness = 20;
    startAnimations->getAnimationOptions().damping = 100;

    renderRequest->appendEndAnimations();

    auto* onLayoutComplete = renderRequest->appendOnLayoutComplete();
    onLayoutComplete->setCallback(callback);

    auto result = renderRequest->serialize(attributeIds);

    auto expectedCreateElement = Value()
                                     .setMapValue("type", Value(STRING_LITERAL("CreateElement")))
                                     .setMapValue("elementId", Value(42))
                                     .setMapValue("viewClass", Value(STRING_LITERAL("MyViewClass")));
    auto expectedDestroyElement =
        Value().setMapValue("type", Value(STRING_LITERAL("DestroyElement"))).setMapValue("elementId", Value(32));
    auto expectedMoveElementToParent = Value()
                                           .setMapValue("type", Value(STRING_LITERAL("MoveElementToParent")))
                                           .setMapValue("elementId", Value(80))
                                           .setMapValue("parentElementId", Value(90))
                                           .setMapValue("parentIndex", Value(13));
    auto expectedSetRootElement =
        Value().setMapValue("type", Value(STRING_LITERAL("SetRootElement"))).setMapValue("elementId", Value(1000));
    auto expectedSetElementAttribute = Value()
                                           .setMapValue("type", Value(STRING_LITERAL("SetElementAttribute")))
                                           .setMapValue("elementId", Value(1))
                                           .setMapValue("attributeName", Value(STRING_LITERAL("width")))
                                           .setMapValue("attributeValue", Value(static_cast<int64_t>(1337)))
                                           .setMapValue("injectedFromParent", Value(true));
    auto expectedStartAnimations =
        Value()
            .setMapValue("type", Value(STRING_LITERAL("StartAnimations")))
            .setMapValue("options",
                         Value()
                             .setMapValue("type", Value(STRING_LITERAL("easeIn")))
                             .setMapValue("controlPoints", Value(ValueArray::make({Value(0.0), Value(1.0)})))
                             .setMapValue("completion", Value(static_cast<int64_t>(42)))
                             .setMapValue("duration", Value(9.0))
                             .setMapValue("beginFromCurrentState", Value(true))
                             .setMapValue("crossfade", Value(false))
                             .setMapValue("stiffness", Value(20))
                             .setMapValue("damping", Value(100)));

    auto expectedEndAnimations = Value().setMapValue("type", Value(STRING_LITERAL("EndAnimations")));
    auto expectedOnLayoutComplete =
        Value().setMapValue("type", Value(STRING_LITERAL("OnLayoutComplete"))).setMapValue("callback", Value(callback));

    auto expectedEntries = ValueArray::make({expectedCreateElement,
                                             expectedDestroyElement,
                                             expectedMoveElementToParent,
                                             expectedSetRootElement,
                                             expectedSetElementAttribute,
                                             expectedStartAnimations,
                                             expectedEndAnimations,
                                             expectedOnLayoutComplete});

    auto expectedResult = Value().setMapValue("contextID", Value(1)).setMapValue("entries", Value(expectedEntries));

    ASSERT_EQ(expectedResult, result);
}

class TestBridgedClass : public ValdiObject {
public:
    VALDI_CLASS_HEADER_IMPL(TestBridgedClass)
};

TEST_P(RuntimeFixture, destroyNativeReferencesOnDestroy) {
    auto bridgedClass = Valdi::makeShared<TestBridgedClass>();

    auto context = makeShared<ValueMap>();
    (*context)[STRING_LITERAL("bridgedClass")] = Value(bridgedClass);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "RetainNativeRefs", Value::undefined(), Value(context));

    Weak<TestBridgedClass> weakBridgedClass = bridgedClass;

    bridgedClass = nullptr;
    context = makeShared<ValueMap>();

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_FALSE(weakBridgedClass.expired());

    wrapper.runtime->destroyContext(tree->getContext());

    wrapper.flushQueues();

    ASSERT_TRUE(weakBridgedClass.expired());
}

TEST_P(RuntimeFixture, destroyNativeFunctionRefOnDestroy) {
    auto fn =
        makeShared<ValueFunctionWithCallable>([](const auto& callContext) -> Value { return Value::undefined(); });

    Value context;
    context.setMapValue("myFunction", Value(fn));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "RetainNativeRefs", Value::undefined(), context);

    wrapper.waitUntilAllUpdatesCompleted();

    context = Value();

    // 1 ref in the test, 1 ref in the Context object, and 1 ref in JS
    ASSERT_EQ(static_cast<long>(3), fn.use_count());

    wrapper.runtime->destroyContext(tree->getContext());
    wrapper.flushQueues();

    // We should only have the local ref remaining
    ASSERT_EQ(static_cast<long>(1), fn.use_count());
}

TEST_P(RuntimeFixture, destroyProxyRefOnDestroy) {
    auto schema = ValueSchema::cls(STRING_LITERAL("MyClass"), true, {});
    auto proxyObject = makeShared<TestValueTypedProxyObject>(ValueTypedObject::make(schema.getClassRef()));

    auto tree = wrapper.createViewNodeTreeAndContext(
        "test", "RetainNativeRefs", Value::undefined(), Value().setMapValue("myProxy", Value(proxyObject)));

    wrapper.waitUntilAllUpdatesCompleted();

    // 1 ref in the test, 1 ref in the Context object, and 1 ref in JS
    ASSERT_EQ(static_cast<long>(3), proxyObject.use_count());

    wrapper.runtime->destroyContext(tree->getContext());
    wrapper.flushQueues();

    // We should only have the local ref remaining
    ASSERT_EQ(static_cast<long>(1), proxyObject.use_count());
}

TEST_P(RuntimeFixture, unwrapsProxyObjectThroughUntypedUnmarshalling) {
    auto schema = ValueSchema::cls(STRING_LITERAL("MyClass"), true, {});
    auto proxyObject = makeShared<TestValueTypedProxyObject>(ValueTypedObject::make(schema.getClassRef()));

    auto objectsManager = wrapper.runtime->getJavaScriptRuntime()->createNativeObjectsManager("");

    auto jsResult = callFunctionSync(wrapper, "test/src/WrapNativeObject", "wrapper", {Value(proxyObject)});

    ASSERT_TRUE(jsResult) << jsResult.description();

    ASSERT_TRUE(jsResult.value().isFunction());

    auto callResult = (*jsResult.value().getFunction())(Valdi::ValueFunctionFlagsCallSync, {});
    ASSERT_TRUE(callResult) << callResult.description();

    auto retValue = callResult.value();

    // We should be able to unwrap our passed object
    ASSERT_EQ(ValueType::ProxyTypedObject, retValue.getType());
    ASSERT_EQ(proxyObject.get(), retValue.getProxyObject());
}

TEST_P(RuntimeFixture, canRetainNativeReferencesOnDestroy) {
    auto bridgedClass = Valdi::makeShared<TestBridgedClass>();

    auto context = makeShared<ValueMap>();
    (*context)[STRING_LITERAL("bridgedClass")] = Value(bridgedClass);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "RetainNativeRefs", Value::undefined(), Value(context));

    Weak<TestBridgedClass> weakBridgedClass = bridgedClass;

    bridgedClass = nullptr;
    context = makeShared<ValueMap>();

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_FALSE(weakBridgedClass.expired());

    SharedAtomic<Value> retainedJsValue;

    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) {
        retainedJsValue.set(callContext.getParameter(0));
        return Value::undefined();
    });

    auto params = ValueArray::make(1);
    params->emplace(0, Value(callback));

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("protectContext"), params);

    wrapper.runtime->destroyContext(tree->getContext());

    wrapper.flushQueues();

    // It should have been retained since protectContext is called.
    ASSERT_FALSE(weakBridgedClass.expired());

    retainedJsValue.set(Value());

    wrapper.runtime->getJavaScriptRuntime()->performGc();
    wrapper.flushQueues();
    wrapper.flushQueues();

    if (isJSCore()) {
        // TODO(simon): object is not GC'd in JSCore, need to investigate and fix
        // After a GC it should be destroyed anyway.
    } else if (isHermes()) {
        // TODO(simon): object is not GC'd in Hermes, need to investigate and fix
        // After a GC it should be destroyed anyway.
    } else {
        ASSERT_TRUE(weakBridgedClass.expired());
    }
}

TEST_P(RuntimeFixture, keepNativeReferencesAliveDuringJsCall) {
    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    SharedAtomic<Value> jsCallbackHolder;

    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        jsCallbackHolder.set(callContext.getParameter(callContext.getParametersSize() - 1));
        return Value::undefined();
    });

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(),
        STRING_LITERAL("makeInterruptibleCallback"),
        ValueArray::make({Value().setMapValue("interruptible", Value(false)).setMapValue("cb", Value(callback))}));

    wrapper.flushQueues();

    auto jsCallback = jsCallbackHolder.get();

    ASSERT_TRUE(jsCallback.isFunction());

    Result<Value> result;

    wrapper.runtime->getJavaScriptRuntime()->dispatchOnJsThreadSync(tree->getContext(), [&](const auto& jsEntry) {
        // Destroy the context while our task is running
        wrapper.runtime->destroyContext(tree->getContext());

        // Attempt to call the function
        result = (*jsCallback.getFunction())(ValueFunctionFlagsPropagatesError, {});
    });

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(42.0), result.value());
}

// Disabling this for now as we no longer respect it
// TEST_P(RuntimeFixture, makeOpaqueReferencesStayAliveAfterComponentIsDestroyed) {
// //     auto tree =
//         wrapper.createViewNodeTreeAndContext("test", "RetainNativeRefs", Value::undefined(), Value::undefined());

//     wrapper.waitUntilAllUpdatesCompleted();

//     SharedAtomic<Value> retainedJsValue;

//     auto callback = makeShared<ValueFunctionWithCallable>([=](auto flags, Valdi::Marshaller& parameters) {
//         retainedJsValue.set(parameters.get(0).value());
//         return false;
//     });

//     auto params = ValueArray::make(1);
//     params->emplace(0, Value(callback));

//     wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
//         tree->getContext(), STRING_LITERAL("makeGlobalCallback"), params);

//     wrapper.runtime->destroyContext(tree->getContext());
//     wrapper.flushQueues();

//     auto cb = retainedJsValue.get();

//     auto jsModule = wrapper.runtime->getJavaScriptRuntime()->getModule(
//         nullptr, STRING_LITERAL("test"), STRING_LITERAL("src/RetainNativeRefs.vue"));
//     ASSERT_TRUE(jsModule != nullptr);

//     auto fn = jsModule->getProperty(STRING_LITERAL("callFn"));

//     ASSERT_EQ(ValueType::FunctionType, fn.getType());

//     Marshaller fnParams;
//     fnParams.push(cb);
//     (*fn.getFunction())(ValueFunctionFlagsCallSync, fnParams);

//     auto returnValue = fnParams.getOrUndefined(-1);

//     ASSERT_EQ(Value(STRING_LITERAL("Nice")), returnValue);
// }

TEST_P(RuntimeFixture, jsArraysStayAliveAfterComponentIsDestroyed) {
    auto tree =
        wrapper.createViewNodeTreeAndContext("test", "RetainNativeRefs", Value::undefined(), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    SharedAtomic<Value> retainedJsValue;

    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) {
        retainedJsValue.set(callContext.getParameter(0));
        return Value::undefined();
    });

    auto params = ValueArray::make(1);
    params->emplace(0, Value(callback));

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("makeBytes"), params);

    wrapper.runtime->destroyContext(tree->getContext());
    wrapper.flushQueues();

    auto cb = retainedJsValue.get();

    ASSERT_EQ(ValueType::TypedArray, cb.getType());

    auto bytes = cb.getTypedArray()->getBuffer();

    ASSERT_EQ(static_cast<size_t>(8), bytes.size());

    auto* data = reinterpret_cast<const uint8_t*>(bytes.data());

    ASSERT_EQ(0, data[0]);
    ASSERT_EQ(2, data[1]);
    ASSERT_EQ(4, data[2]);
    ASSERT_EQ(6, data[3]);
    ASSERT_EQ(8, data[4]);
    ASSERT_EQ(10, data[5]);
    ASSERT_EQ(12, data[6]);
    ASSERT_EQ(14, data[7]);
}

TEST_P(RuntimeFixture, canPreloadViews) {
    wrapper.teardown();
    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), true);

    auto viewClassName = STRING_LITERAL("ValdiView");

    auto viewManagerContext = wrapper.standaloneRuntime->getViewManagerContext();
    viewManagerContext->preloadViews(viewClassName, 10);

    wrapper.mainQueue->runUntilTrue([&]() {
        auto stats = viewManagerContext->getViewPoolsStats();
        return getNumberOfPooledViews(viewClassName, stats) == 10;
    });
    auto stats = viewManagerContext->getViewPoolsStats();

    ASSERT_EQ(10, getNumberOfPooledViews(viewClassName, stats));
}

TEST_P(RuntimeFixture, preloadOnlyAddsToThePoolIfMissing) {
    wrapper.teardown();
    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), true);

    auto viewClassName = STRING_LITERAL("ValdiView");

    auto viewManagerContext = wrapper.standaloneRuntime->getViewManagerContext();

    // Manually populate the views with 2 entries
    auto viewFactory = viewManagerContext->getGlobalViewFactories()->getViewFactory(viewClassName);

    auto view = viewFactory->createView(nullptr, nullptr, false);
    auto view2 = viewFactory->createView(nullptr, nullptr, false);

    viewFactory->enqueueViewToPool(view);
    viewFactory->enqueueViewToPool(view2);

    auto stats = viewManagerContext->getViewPoolsStats();
    ASSERT_EQ(2, getNumberOfPooledViews(viewClassName, stats));

    // Start preloading views

    viewManagerContext->preloadViews(viewClassName, 5);

    wrapper.mainQueue->flush();
    stats = viewManagerContext->getViewPoolsStats();

    // Should have only 5 views (2 existing, 3 additional)
    ASSERT_EQ(5, getNumberOfPooledViews(viewClassName, stats));

    viewManagerContext->preloadViews(viewClassName, 10);
    wrapper.mainQueue->flush();

    stats = viewManagerContext->getViewPoolsStats();
    // Should have only 10 views (5 existing, 5 additional)
    ASSERT_EQ(10, getNumberOfPooledViews(viewClassName, stats));
}

TEST_P(RuntimeFixture, canPreloadMultipleViews) {
    wrapper.teardown();
    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), true);

    auto viewClassName1 = STRING_LITERAL("ValdiView");
    auto viewClassName2 = STRING_LITERAL("ValdiView2");

    auto viewManagerContext = wrapper.standaloneRuntime->getViewManagerContext();
    viewManagerContext->preloadViews(viewClassName1, 10);
    viewManagerContext->preloadViews(viewClassName2, 5);

    wrapper.mainQueue->runUntilTrue([&]() {
        auto stats = viewManagerContext->getViewPoolsStats();
        return getNumberOfPooledViews(viewClassName1, stats) == 10 &&
               getNumberOfPooledViews(viewClassName2, stats) == 5;
    });
    auto stats = viewManagerContext->getViewPoolsStats();

    ASSERT_EQ(10, getNumberOfPooledViews(viewClassName1, stats));
    ASSERT_EQ(5, getNumberOfPooledViews(viewClassName2, stats));
}

TEST_P(RuntimeFixture, canPreloadViewsInBatches) {
    wrapper.teardown();
    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), true);

    // the slowViewName is going to take 10ms to create
    auto viewClassName = STRING_LITERAL("SlowView");
    auto viewManagerContext = wrapper.standaloneRuntime->getViewManagerContext();
    viewManagerContext->preloadViews(viewClassName, 3);

    wrapper.mainQueue->runUntilTrue([&]() {
        auto stats = viewManagerContext->getViewPoolsStats();
        return getNumberOfPooledViews(viewClassName, stats) == 3;
    });

    // We should have 1 initial dispatch for the preload kick off,
    // as well as 3 for preloading each of the view.
    // (Using >= 4 instead of == 4, since the view loader might kick off
    // some internal logic on the main thread)
    ASSERT_TRUE(wrapper.mainQueue->numberOfExecutedTasks >= 4);
}

TEST_P(RuntimeFixture, canPausePreloading) {
    wrapper.teardown();
    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), true);

    auto viewClassName = STRING_LITERAL("ValdiView");

    auto viewManagerContext = wrapper.standaloneRuntime->getViewManagerContext();

    viewManagerContext->setPreloadingPaused(true);

    viewManagerContext->preloadViews(viewClassName, 5);

    wrapper.mainQueue->flush();
    auto stats = viewManagerContext->getViewPoolsStats();

    ASSERT_EQ(0, getNumberOfPooledViews(viewClassName, stats));

    viewManagerContext->setPreloadingPaused(false);

    wrapper.mainQueue->flush();

    stats = viewManagerContext->getViewPoolsStats();

    ASSERT_EQ(5, getNumberOfPooledViews(viewClassName, stats));
}

TEST_P(RuntimeFixture, canResolveJsImportPath) {
    auto bridgeModuleResult = JavaScriptPathResolver::resolveResourceId(STRING_LITERAL("BridgeModule"));
    ASSERT_TRUE(bridgeModuleResult.success());

    ASSERT_EQ(StringBox(), bridgeModuleResult.value().bundleName);
    ASSERT_EQ(STRING_LITERAL("BridgeModule"), bridgeModuleResult.value().resourcePath);

    auto absoluteResult = JavaScriptPathResolver::resolveResourceId(STRING_LITERAL("my_module/MyFile.js"));
    ASSERT_TRUE(absoluteResult.success());

    ASSERT_EQ(STRING_LITERAL("my_module"), absoluteResult.value().bundleName);
    ASSERT_EQ(STRING_LITERAL("MyFile.js"), absoluteResult.value().resourcePath);

    auto relativeLocalResult = JavaScriptPathResolver::resolveResourceId(
        ResourceId(STRING_LITERAL("my_module"), STRING_LITERAL("OtherFile.js")), STRING_LITERAL("./MyFile.js"));
    ASSERT_TRUE(relativeLocalResult.success());

    ASSERT_EQ(STRING_LITERAL("my_module"), relativeLocalResult.value().bundleName);
    ASSERT_EQ(STRING_LITERAL("MyFile.js"), relativeLocalResult.value().resourcePath);

    auto relativeExternalResult = JavaScriptPathResolver::resolveResourceId(
        ResourceId(STRING_LITERAL("my_module"), STRING_LITERAL("OtherFile.js")),
        STRING_LITERAL("../other_module/MyOtherFile.js"));
    ASSERT_TRUE(relativeExternalResult.success());

    ASSERT_EQ(STRING_LITERAL("other_module"), relativeExternalResult.value().bundleName);
    ASSERT_EQ(STRING_LITERAL("MyOtherFile.js"), relativeExternalResult.value().resourcePath);

    auto relativeBridgeModuleResult = JavaScriptPathResolver::resolveResourceId(
        ResourceId(STRING_LITERAL("my_module"), STRING_LITERAL("OtherFile.js")), STRING_LITERAL("../BridgeModule"));
    ASSERT_TRUE(relativeBridgeModuleResult.success());

    ASSERT_EQ(StringBox(), relativeBridgeModuleResult.value().bundleName);
    ASSERT_EQ(STRING_LITERAL("BridgeModule"), relativeBridgeModuleResult.value().resourcePath);
}

class AssetLoadObserverImpl : public snap::valdi_core::AssetLoadObserver {
public:
    AssetLoadObserverImpl() {}

    void onLoad(const Shared<snap::valdi_core::Asset>& asset,
                const Valdi::Value& loadedAsset,
                const std::optional<Valdi::StringBox>& error) override {
        auto loadedAssetRef = loadedAsset.getTypedRef<Valdi::LoadedAsset>();
        if (loadedAssetRef != nullptr) {
            _result = loadedAssetRef;
        } else {
            _result = Error(error.value());
        }
    }

    static Result<Ref<Valdi::LoadedAsset>> loadSync(RuntimeWrapper& runtimeWrapper, const Ref<Asset>& asset) {
        auto observer = makeShared<AssetLoadObserverImpl>();
        asset->addLoadObserver(observer, snap::valdi_core::AssetOutputType::Dummy, 0, 0, Value());

        runtimeWrapper.mainQueue->runUntilTrue([&]() { return !observer->_result.empty(); });

        return observer->_result;
    }

private:
    Result<Ref<Valdi::LoadedAsset>> _result;
};

Ref<Asset> getSrcAssetFromNode(ViewNode& viewNode) {
    auto assetHandler = castOrNull<ViewNodeAssetHandler>(viewNode.getAssetHandler());
    if (assetHandler == nullptr) {
        return nullptr;
    }

    return assetHandler->getAsset();
}

Ref<Asset> getSrcAsset(const Ref<ViewNodeTree>& tree) {
    return getSrcAssetFromNode(*getViewNodeForId(tree, "imageView"));
}

struct RegisteredAsset {
    StringBox url;
    BytesView content;
};

struct RegisteredAssets {
    RegisteredAsset asset1;
    RegisteredAsset asset2;
    RegisteredAsset asset3;
};

static RegisteredAsset registerAsset(RuntimeWrapper& wrapper,
                                     std::string_view module,
                                     std::string_view imageName,
                                     std::string_view fileUrl,
                                     std::string_view assetContent) {
    RegisteredAsset asset;
    asset.url = StringCache::getGlobal().makeString(fileUrl);

    wrapper.resourceLoader->setLocalAssetURL(
        StringCache::getGlobal().makeString(module), StringCache::getGlobal().makeString(imageName), asset.url);

    auto content = makeShared<ByteBuffer>();
    content->append(assetContent);

    wrapper.diskCache->store(Path(URL(asset.url).getPath()), content->toBytesView()).ensureSuccess();

    return asset;
}

static RegisteredAssets registerAssets(RuntimeWrapper& wrapper) {
    RegisteredAssets assets;

    assets.asset1 = registerAsset(wrapper, "test", "image", "file://assets/asset1", "asset1");
    assets.asset2 = registerAsset(wrapper, "test", "otherImage", "file://assets/asset2", "asset2");
    assets.asset3 = registerAsset(wrapper, "test2", "yay", "file://assets/asset3", "asset3");

    return assets;
}

TEST_P(RuntimeFixture, canResolveLocalAssetsInCurrentModule) {
    auto assets = registerAssets(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value::undefined();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("test:image"));
    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);
    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());

    ASSERT_EQ(assets.asset1.url, asset->getResolvedURL());

    // Now resolving an image relative to the current module

    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("otherImage"));
    wrapper.setViewModel(tree->getContext(), Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());

    ASSERT_EQ(assets.asset2.url, asset->getResolvedURL());
}

TEST_P(RuntimeFixture, canResolveLocalAssetsFromOtherModule) {
    auto assets = registerAssets(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("test2:yay"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());

    ASSERT_EQ(assets.asset3.url, asset->getResolvedURL());
}

TEST_P(RuntimeFixture, failResolveOnUnknownLocalAssets) {
    auto assets = registerAssets(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("test:randomImage"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);
    // This should have failed to load
    ASSERT_FALSE(result.success()) << result.description();
    // We should still need resolve.
    ASSERT_TRUE(asset->needResolve());
}

TEST_P(RuntimeFixture, provideSameLocalAssetInstance) {
    auto assets = registerAssets(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("test:image"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto tree2 = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset2 = getSrcAsset(tree2);

    ASSERT_TRUE(asset2 != nullptr);

    ASSERT_EQ(asset, asset2) << "We should have the same instance provided since they refer to the same image";
}

TEST_P(RuntimeFixture, canResolveRemoteURL) {
    auto assets = registerAssets(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("http://snapchat.com/favicon.png"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_FALSE(result.success()) << result.description(); // We don't have a http loader set
    ASSERT_TRUE(asset->isURL());

    ASSERT_EQ(STRING_LITERAL("http://snapchat.com/favicon.png"), asset->getResolvedURL());
}

TEST_P(RuntimeFixture, keepsAssetAliveWhileSrcIsSet) {
    auto assets = registerAssets(wrapper);

    auto url = STRING_LITERAL("http://snapchat.com/favicon.png");
    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(url);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));
    tree->setViewInflationEnabled(false);

    wrapper.waitUntilAllUpdatesCompleted();

    auto assetKey = AssetKey(url);

    const auto& assetsManager = wrapper.runtime->getResourceManager().getAssetsManager();

    ASSERT_TRUE(assetsManager->isAssetAlive(assetKey));

    auto asset = assetsManager->getAsset(assetKey);

    // 1 ref in the test code, 1 in the src attribute
    ASSERT_EQ(static_cast<long>(2), asset.use_count());

    tree->setViewInflationEnabled(true);

    ASSERT_TRUE(assetsManager->isAssetAlive(assetKey));
    // 1 ref in the test code, 1 in the src attribute,
    // 2 in the ImageViewProxy instance
    ASSERT_EQ(static_cast<long>(4), asset.use_count());

    tree->setViewInflationEnabled(false);

    ASSERT_TRUE(assetsManager->isAssetAlive(assetKey));
    // 1 ref in the test code, 1 in the src attribute
    ASSERT_EQ(static_cast<long>(2), asset.use_count());

    // Now unset the src attribute

    wrapper.setViewModel(tree->getContext(), Value().setMapValue("src", Value::undefined()));
    wrapper.waitUntilAllUpdatesCompleted();

    // 1 ref in the test code
    ASSERT_EQ(static_cast<long>(1), asset.use_count());
    asset = nullptr;

    ASSERT_FALSE(assetsManager->isAssetAlive(assetKey));
}

TEST_P(RuntimeFixture, canLoadAssetExplicitlyFromTSSide) {
    auto assets = registerAssets(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(makeShared<ValueMap>()));

    wrapper.waitUntilAllUpdatesCompleted();

    SharedAtomic<Value> assetResult;
    SharedAtomic<Value> errorResult;

    auto params = ValueArray::make(2);
    params->emplace(0, Value(STRING_LITERAL("test:image")));
    params->emplace(1, Value(makeShared<ValueFunctionWithCallable>([assetResult, errorResult](const auto& callContext) {
                        assetResult.set(callContext.getParameter(0));
                        errorResult.set(callContext.getParameter(1));
                        return Value::undefined();
                    })));

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("loadAsset"), params);

    wrapper.flushJsQueue();

    ASSERT_TRUE(wrapper.mainQueue->runNextTask());

    // First flush is so that the AssetsManager can resolve the asset location.
    // It schedules an async task into the worker queue
    wrapper.runtime->getWorkerQueue()->sync([]() {});

    // Flushes the update pass after the asset location has been resolved.
    ASSERT_TRUE(wrapper.mainQueue->runNextTask());
    // The AssetsManager then schedules the load of the underlying asset
    // in to the worker queue. This flushes it.
    wrapper.runtime->getWorkerQueue()->sync([]() {});
    // Flushes the update pass after load has completed.
    ASSERT_TRUE(wrapper.mainQueue->runNextTask());

    // Make sure the JS thread is flushed so that the onLoad callback is called
    wrapper.flushJsQueue();

    auto asset = assetResult.get();

    ASSERT_EQ(ValueType::ValdiObject, asset.getType());

    auto loadedAsset = castOrNull<StandaloneLoadedAsset>(asset.getValdiObject());
    ;
    ASSERT_TRUE(loadedAsset != nullptr);
}

TEST_P(RuntimeFixture, canUnregisterFromLoadedAssetFromTSSide) {
    auto assets = registerAssets(wrapper);

    const auto& assetsManager = wrapper.runtime->getResourceManager().getAssetsManager();
    assetsManager->setShouldRemoveUnusedLocalAssets(true);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(makeShared<ValueMap>()));

    wrapper.waitUntilAllUpdatesCompleted();

    auto loaded = makeShared<std::atomic_bool>(false);

    auto params = ValueArray::make(2);
    params->emplace(0, Value(STRING_LITERAL("test:image")));
    params->emplace(1, Value(makeShared<ValueFunctionWithCallable>([loaded](const auto& callContext) {
                        *loaded = true;
                        return Value::undefined();
                    })));

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("loadAsset"), params);

    wrapper.mainQueue->runUntilTrue([&]() { return loaded->load(); });

    auto assetKey =
        AssetKey(wrapper.runtime->getResourceManager().getBundle(STRING_LITERAL("test")), STRING_LITERAL("image"));

    ASSERT_TRUE(assetsManager->isAssetAlive(assetKey));

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("unloadAsset"), ValueArray::make(0));

    wrapper.flushJsQueue();

    // Flushes the update pass after the asset location has been resolved.
    ASSERT_TRUE(wrapper.mainQueue->runNextTask());

    ASSERT_FALSE(assetsManager->isAssetAlive(assetKey));
}

TEST_P(RuntimeFixture, supportsModulePreloading) {
    // "test/src/DirectionAsset" module imports "valdi_core/src/Asset"
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DirectionalAsset"))));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("valdi_core"), STRING_LITERAL("src/Asset"))));

    wrapper.runtime->getJavaScriptRuntime()->preloadModule(STRING_LITERAL("test/src/DirectionalAsset"), 0);

    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DirectionalAsset"))));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("valdi_core"), STRING_LITERAL("src/Asset"))));

    wrapper.runtime->getJavaScriptRuntime()->preloadModule(STRING_LITERAL("test/src/DirectionalAsset"), 1);

    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DirectionalAsset"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("valdi_core"), STRING_LITERAL("src/Asset"))));
}

TEST_P(RuntimeFixture, supportsDirectionalAsset) {
    auto ltrAssetUrl = STRING_LITERAL("file://ltr.png");
    auto rtlAssetUrl = STRING_LITERAL("file://rtl.png");
    wrapper.diskCache->store(Path(URL(ltrAssetUrl).getPath()), BytesView()).ensureSuccess();
    wrapper.diskCache->store(Path(URL(rtlAssetUrl).getPath()), BytesView()).ensureSuccess();

    auto viewModel = Value().setMapValue("ltrAsset", Value(ltrAssetUrl)).setMapValue("rtlAsset", Value(rtlAssetUrl));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DirectionalAsset@test/src/DirectionalAsset"), viewModel, Value());

    tree->setLayoutSpecs(Size(1.0f, 1.0f), LayoutDirectionLTR);

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootNode = tree->getRootViewNode();
    ASSERT_TRUE(rootNode != nullptr);
    auto asset = getSrcAssetFromNode(*rootNode->getChildAt(0));
    ASSERT_TRUE(asset != nullptr);

    ASSERT_EQ(ltrAssetUrl, asset->getIdentifier());

    tree->setLayoutSpecs(Size(1.0f, 1.0f), LayoutDirectionRTL);

    // TODO(simon): Re-apply for directional attributes should be made automatic on layout direction change
    tree->scheduleExclusiveUpdate([&]() {
        std::vector<AttributeId> attributes = {wrapper.runtime->getAttributeIds().getIdForName("srcOnLoad")};
        tree->getRootViewNode()->reapplyAttributesRecursive(tree->getCurrentViewTransactionScope(), attributes, false);
    });

    asset = getSrcAssetFromNode(*rootNode->getChildAt(0));
    ASSERT_TRUE(asset != nullptr);
    ASSERT_EQ(rtlAssetUrl, asset->getIdentifier());
}

TEST_P(RuntimeFixture, supportsPlatformSpecificAsset) {
    auto defaultAssetUrl = STRING_LITERAL("file://ltr.png");
    auto iOSAssetUrl = STRING_LITERAL("file://rtl.png");
    wrapper.diskCache->store(Path(URL(defaultAssetUrl).getPath()), BytesView()).ensureSuccess();
    wrapper.diskCache->store(Path(URL(iOSAssetUrl).getPath()), BytesView()).ensureSuccess();

    auto viewModel =
        Value().setMapValue("defaultAsset", Value(defaultAssetUrl)).setMapValue("iOSOverrideAsset", Value(iOSAssetUrl));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("PlatformSpecificAsset@test/src/PlatformSpecificAsset"), viewModel, Value());

    tree->setLayoutSpecs(Size(1.0f, 1.0f), LayoutDirectionLTR);

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootNode = tree->getRootViewNode();
    ASSERT_TRUE(rootNode != nullptr);
    auto asset = getSrcAssetFromNode(*rootNode->getChildAt(0));
    ASSERT_TRUE(asset != nullptr);

    ASSERT_EQ(iOSAssetUrl, asset->getIdentifier());
}

TEST_P(RuntimeFixture, canHotReloadAsset) {
    auto assets = registerAssets(wrapper);

    auto resourceData = makeShared<ByteBuffer>("Some asset data")->toBytesView();

    wrapper.runtime->updateResource(resourceData, STRING_LITERAL("random_module"), STRING_LITERAL("random_image.png"));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("random_module:random_image"));

    auto tree = wrapper.createViewNodeTreeAndContext("test", "AssetLoading", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);
    // This should have failed to load
    ASSERT_TRUE(result.success()) << result.description();

    auto assetLocation = asset->getResolvedLocation();
    ASSERT_TRUE(assetLocation);

    ASSERT_EQ(STRING_LITERAL(
                  "file:///hotreloaded_assets/5cd7c9c5b58042a704c2b80a5abfa7a33ce4a94be0e8dc51985dd7373821f966.png"),
              assetLocation.value().getUrl());

    auto loadedAsset = castOrNull<StandaloneLoadedAsset>(result.value());
    ASSERT_TRUE(loadedAsset != nullptr);

    ASSERT_EQ(resourceData, loadedAsset->getBytes());
}

static void registerAssetArchives(RuntimeWrapper& wrapper,
                                  RequestManagerMock& requestManager,
                                  const char* archiveName) {
    auto archive = wrapper.resourceLoader->loadModuleContent(StringBox::fromCString(archiveName));
    if (!archive) {
        throw Exception(archive.moveError().toStringBox());
    }

    auto deserializedArchive = ValdiModuleArchive::deserialize(archive.value());
    if (!deserializedArchive) {
        throw Exception(deserializedArchive.moveError().toStringBox());
    }

    const auto& paths = deserializedArchive.value().getAllEntryPaths();
    for (const auto& path : paths) {
        auto entry = deserializedArchive.value().getEntry(path);
        if (entry) {
            requestManager.addMockedResponse(
                STRING_FORMAT("http://localhost/{}", path),
                STRING_LITERAL("GET"),
                BytesView(archive.value().getSource(), entry.value().data, entry.value().size));
        }
    }
}

Shared<RequestManagerMock> configureWrapperWithRemoteSupport(RuntimeWrapper& wrapper) {
    auto requestManager = Valdi::makeShared<RequestManagerMock>(*wrapper.logger);
    wrapper.runtimeManager->setRequestManager(requestManager);

    registerAssetArchives(wrapper, *requestManager, "remote.valdiarchive");
    registerAssetArchives(wrapper, *requestManager, "remote_assets.valdiarchive");

    return requestManager;
}

TEST_P(RuntimeFixture, canTrackAssets) {
    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("BundledAsset@test/src/BundledAsset"), Value(), Value());

    auto assetTracker = makeShared<TestViewNodesAssetTracker>();
    tree->setAssetTracker(assetTracker);

    wrapper.flushJsQueue();

    ASSERT_EQ(0, assetTracker->beganRequestCount);
    ASSERT_EQ(0, assetTracker->endRequestCount);
    ASSERT_EQ(0, assetTracker->changedCount);

    ASSERT_TRUE(wrapper.mainQueue->runNextTask());

    ASSERT_EQ(1, assetTracker->beganRequestCount);
    ASSERT_EQ(0, assetTracker->endRequestCount);
    ASSERT_EQ(0, assetTracker->changedCount);

    // Flush the worker queue since it's used by the asset manager
    wrapper.runtime->getWorkerQueue()->sync([]() {});
    ASSERT_TRUE(wrapper.mainQueue->runNextTask());

    ASSERT_EQ(1, assetTracker->beganRequestCount);
    ASSERT_EQ(0, assetTracker->endRequestCount);
    ASSERT_EQ(1, assetTracker->changedCount);

    wrapper.runtime->destroyContext(tree->getContext());

    ASSERT_EQ(1, assetTracker->beganRequestCount);
    ASSERT_EQ(1, assetTracker->endRequestCount);
    ASSERT_EQ(1, assetTracker->changedCount);

    wrapper.flushQueues();

    ASSERT_EQ(1, assetTracker->beganRequestCount);
    ASSERT_EQ(1, assetTracker->endRequestCount);
    ASSERT_EQ(1, assetTracker->changedCount);
}

TEST_P(RuntimeFixture, canLoadBundledAsset) {
    configureWrapperWithRemoteSupport(wrapper);

    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("BundledAsset@test/src/BundledAsset"), Value(), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());

    auto loadedAsset = castOrNull<StandaloneLoadedAsset>(result.value());

    ASSERT_TRUE(loadedAsset != nullptr);

    ASSERT_EQ(static_cast<size_t>(1742), loadedAsset->getBytes().size());
}

void loadRemoteBundle(RuntimeWrapper& wrapper) {
    auto result = wrapper.loadModule(STRING_LITERAL("remote"), ResourceManagerLoadModuleType::Sources);
    ASSERT_TRUE(result) << result.description();
}

TEST_P(RuntimeFixture, canInflateRemoteComponent) {
    configureWrapperWithRemoteSupport(wrapper);

    ASSERT_TRUE(wrapper.runtime->getResourceManager().bundleHasRemoteAssets(STRING_LITERAL("remote")));
    ASSERT_TRUE(wrapper.runtime->getResourceManager().bundleHasRemoteSources(STRING_LITERAL("remote")));

    loadRemoteBundle(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("subtitle")] = Value(STRING_LITERAL("Does it work?"));

    auto tree = wrapper.createViewNodeTreeAndContext("remote", "RemoteComponent", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "Hello world remote"))
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "Does it work?")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canInflateRemoteComponentInLocalComponent) {
    configureWrapperWithRemoteSupport(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("local", "RemoteComponentInLocalComponent");
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "This part is local")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "This part is remote"))),
              getRootView(tree));

    loadRemoteBundle(wrapper);
    auto tree2 = wrapper.createViewNodeTreeAndContext("local", "RemoteComponentInLocalComponent");
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(DummyView("SCValdiView")
                          .addChild(DummyView("SCValdiLabel").addAttribute("value", "This part is local")))
            .addChild(DummyView("SCValdiView")
                          .addChild(DummyView("SCValdiLabel").addAttribute("value", "This part is remote"))
                          .addChild(DummyView("SCValdiView")
                                        .addChild(DummyView("SCValdiLabel").addAttribute("value", "Hello world remote"))
                                        .addChild(DummyView("SCValdiLabel")))),
        getRootView(tree2));
}

TEST_P(RuntimeFixture, canInflateLocalComponentInRemoteComponent) {
    configureWrapperWithRemoteSupport(wrapper);
    loadRemoteBundle(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("remote", "LocalComponentInRemoteComponent");
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "This part is remote")))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "This part is local"))
                                .addChild(DummyView("SCValdiView")
                                              .addChild(DummyView("SCValdiLabel"))
                                              .addChild(DummyView("SCValdiView")
                                                            .addChild(DummyView("UIButton"))
                                                            .addChild(DummyView("UIButton"))
                                                            .addChild(DummyView("UIButton"))))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, failsRenderingWhenRemoteBundleIsNotLoaded) {
    configureWrapperWithRemoteSupport(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("remote", "RemoteScriptInRemoteComponent");
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));
}

TEST_P(RuntimeFixture, canUseRemoteScriptInRemoteComponent) {
    configureWrapperWithRemoteSupport(wrapper);
    loadRemoteBundle(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("remote", "RemoteScriptInRemoteComponent");
    wrapper.waitUntilAllUpdatesCompleted();

    // Rendering should have failed because the bundle is not loaded
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "This is remote"))
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "From remote: 1 + 2 = 3")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canUseRemoteScriptInLocalComponent) {
    configureWrapperWithRemoteSupport(wrapper);
    loadRemoteBundle(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("local", "RemoteScriptInLocalComponent");
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "This is local"))
                  .addChild(DummyView("SCValdiLabel").addAttribute("value", "From remote: 42 + 42 = 84")),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canResolveRemoteAssetFromRemoteComponent) {
    configureWrapperWithRemoteSupport(wrapper);
    loadRemoteBundle(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("remote", "RemoteImage");
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());
    ASSERT_TRUE(asset->isURL());

    auto url = asset->getResolvedURL();

    auto bytes = wrapper.diskCache->loadForAbsoluteURL(url);
    ASSERT_FALSE(bytes.empty()) << "Could not resolve asset at URL: " << url.toStringView();
}

TEST_P(RuntimeFixture, canUseRemoteAssetInLocalModule) {
    configureWrapperWithRemoteSupport(wrapper);

    ASSERT_TRUE(wrapper.runtime->getResourceManager().bundleHasRemoteAssets(STRING_LITERAL("remote_assets")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().bundleHasRemoteSources(STRING_LITERAL("remote_assets")));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("remote_assets:remote_image"));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("RemoteAsset@remote_assets/src/RemoteAsset"), Value(viewModel), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());
    ASSERT_TRUE(asset->isURL());

    auto url = asset->getResolvedURL();

    auto bytes = wrapper.diskCache->loadForAbsoluteURL(url);
    ASSERT_FALSE(bytes.empty()) << "Could not resolve asset at URL: " << url.toStringView();
}

TEST_P(RuntimeFixture, failsToResolveOnUnknownRemoteAsset) {
    configureWrapperWithRemoteSupport(wrapper);

    ASSERT_TRUE(wrapper.runtime->getResourceManager().bundleHasRemoteAssets(STRING_LITERAL("remote_assets")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().bundleHasRemoteSources(STRING_LITERAL("remote_assets")));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("remote_assets:local_image"));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("RemoteAsset@remote_assets/src/RemoteAsset"), Value(viewModel), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_FALSE(result.success()) << result.description();
}

TEST_P(RuntimeFixture, moduleWithRemoteAssetsCanFallbackOnLocalAsset) {
    configureWrapperWithRemoteSupport(wrapper);

    auto registeredAsset =
        registerAsset(wrapper, "remote_assets", "local_image", "file://assets/local_image", "content");

    ASSERT_TRUE(wrapper.runtime->getResourceManager().bundleHasRemoteAssets(STRING_LITERAL("remote_assets")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().bundleHasRemoteSources(STRING_LITERAL("remote_assets")));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("remote_assets:local_image"));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("RemoteAsset@remote_assets/src/RemoteAsset"), Value(viewModel), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());
    ASSERT_TRUE(asset->isLocalResource());
    ASSERT_FALSE(asset->isURL());
    ASSERT_EQ(registeredAsset.url, asset->getResolvedURL());
}

TEST_P(RuntimeFixture, canLoadRemoteAsset) {
    configureWrapperWithRemoteSupport(wrapper);

    ASSERT_TRUE(wrapper.runtime->getResourceManager().bundleHasRemoteAssets(STRING_LITERAL("remote_assets")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().bundleHasRemoteSources(STRING_LITERAL("remote_assets")));

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("src")] = Value(STRING_LITERAL("remote_assets:remote_image"));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("RemoteAsset@remote_assets/src/RemoteAsset"), Value(viewModel), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    // We set the disk cache in the resource loader, which will make it respond to loadUrlAsset()

    wrapper.resourceLoader->setDiskCache(wrapper.diskCache);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());

    auto loadedAsset = castOrNull<StandaloneLoadedAsset>(result.value());

    ASSERT_TRUE(loadedAsset != nullptr);

    ASSERT_EQ(static_cast<size_t>(1070), loadedAsset->getBytes().size());
}

TEST_P(RuntimeFixture, canResolveRemoteAssetWithKebabCaseFromRemoteComponent) {
    configureWrapperWithRemoteSupport(wrapper);
    loadRemoteBundle(wrapper);

    auto tree = wrapper.createViewNodeTreeAndContext("remote", "RemoteKebabImage");
    wrapper.waitUntilAllUpdatesCompleted();

    auto asset = getSrcAsset(tree);

    ASSERT_TRUE(asset != nullptr);

    auto result = AssetLoadObserverImpl::loadSync(wrapper, asset);

    ASSERT_TRUE(result.success()) << result.description();
    ASSERT_FALSE(asset->needResolve());
    ASSERT_TRUE(asset->isURL());

    auto url = asset->getResolvedURL();

    auto bytes = wrapper.diskCache->loadForAbsoluteURL(url);
    ASSERT_FALSE(bytes.empty()) << "Could not resolve asset at URL: " << url.toStringView();
}

TEST_P(RuntimeFixture, canLoadRemoteModuleAsynchronously) {
    configureWrapperWithRemoteSupport(wrapper);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("moduleName")] = Value(STRING_LITERAL("remote"));

    auto tree = wrapper.createViewNodeTreeAndContext("local", "RemoteAsyncLoadComponent", Value(viewModel));
    wrapper.waitForNextRenderRequest();

    // At first we are initially not loaded
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "We are not loaded"))),
              getRootView(tree));

    // The component should have asked to load the module asynchronously, wait until the load completes and the next
    // render comes
    wrapper.waitForNextRenderRequest();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Hello world remote"))
                                .addChild(DummyView("SCValdiLabel"))),
              getRootView(tree));

    // We now create the same component again

    auto tree2 = wrapper.createViewNodeTreeAndContext("local", "RemoteAsyncLoadComponent", Value(viewModel));
    wrapper.waitForNextRenderRequest();

    // We should be immediately fully populated without having to wait asynchronously, as the remote
    // module was already loaded.

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "Hello world remote"))
                                .addChild(DummyView("SCValdiLabel"))),
              getRootView(tree2));
}

TEST_P(RuntimeFixture, canHandleRemoteModuleLoadAsyncFailure) {
    auto requestManager = configureWrapperWithRemoteSupport(wrapper);
    requestManager->setDisabled(true);

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("moduleName")] = Value(STRING_LITERAL("remote"));

    auto tree = wrapper.createViewNodeTreeAndContext("local", "RemoteAsyncLoadComponent", Value(viewModel));
    wrapper.waitForNextRenderRequest();

    // At first we are initially not loaded
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("SCValdiLabel").addAttribute("value", "We are not loaded"))),
              getRootView(tree));

    // The component should have asked to load the module asynchronously, wait until the load completes and the next
    // render comes
    wrapper.waitForNextRenderRequest();

    // We should be in error state

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addChild(
                DummyView("SCValdiView").addChild(DummyView("SCValdiLabel").addAttribute("value", "Error occured"))),
        getRootView(tree));
}

TEST_P(RuntimeFixture, canRemoveUnusedResourcesOnTrimMemory) {
    // Disable inline assets for this test as they will prevent the "test" module
    // to be found as unused.
    wrapper.runtime->getResourceManager().setInlineAssetsEnabled(false);

    auto childDocumentViewModel = makeShared<ValueMap>();
    (*childDocumentViewModel)[STRING_LITERAL("childViewModel")] = Value(makeShared<ValueMap>());
    auto tree = wrapper.createViewNodeTreeAndContext("test", "ChildDocument", Value(childDocumentViewModel));
    auto tree2 = wrapper.createViewNodeTreeAndContext("test2", "TestImport", Value(makeShared<ValueMap>()));

    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_TRUE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_TRUE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/ChildDocument.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DynamicAttributes.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test2"), STRING_LITERAL("src/TestImport.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(ResourceId(STRING_LITERAL("Valdi"))));

    wrapper.runtimeManager->applicationIsLowInMemory();

    wrapper.flushQueues();

    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));
    // // Should not have changed because we have components alive
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/ChildDocument.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DynamicAttributes.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test2"), STRING_LITERAL("src/TestImport.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(ResourceId(STRING_LITERAL("Valdi"))));

    wrapper.runtime->destroyContext(tree2->getContext());

    wrapper.runtimeManager->applicationIsLowInMemory();

    wrapper.flushQueues();

    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/ChildDocument.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DynamicAttributes.vue.generated"))));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test2"), STRING_LITERAL("src/TestImport.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(ResourceId(STRING_LITERAL("Valdi"))));

    wrapper.runtime->destroyContext(tree->getContext());

    wrapper.runtimeManager->applicationIsLowInMemory();

    wrapper.flushQueues();

    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/ChildDocument.vue.generated"))));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DynamicAttributes.vue.generated"))));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test2"), STRING_LITERAL("src/TestImport.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(ResourceId(STRING_LITERAL("Valdi"))));

    // We can reload the tree

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("padding")] = Value(42.0);
    (*viewModel)[STRING_LITERAL("title")] = Value(STRING_LITERAL("Hello"));

    auto tree3 = wrapper.createViewNodeTreeAndContext("test2", "TestImport", Value(viewModel));
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("padding", 42.0)
                  .addChild(DummyView("SCValdiLabel").addAttribute("color", "red").addAttribute("value", "Hello"))
                  .addChild(DummyView("SCValdiLabel")),
              getRootView(tree3));

    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_TRUE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/ChildDocument.vue.generated"))));
    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test"), STRING_LITERAL("src/DynamicAttributes.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(
        ResourceId(STRING_LITERAL("test2"), STRING_LITERAL("src/TestImport.vue.generated"))));
    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(ResourceId(STRING_LITERAL("Valdi"))));
}

TEST_P(RuntimeFixture, retainsReferencedModulesInNativeOnTrimMemory) {
    ResourceId moduleId(STRING_LITERAL("test"), STRING_LITERAL("src/WrapNativeObject"));

    ASSERT_FALSE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(moduleId));

    auto result =
        getJsModuleWithSchema(wrapper.runtime, nullptr, nullptr, moduleId.toAbsolutePath(), ValueSchema::untyped());
    ASSERT_TRUE(result) << result.description();

    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(moduleId));

    wrapper.runtimeManager->applicationIsLowInMemory();

    wrapper.flushQueues();

    ASSERT_TRUE(wrapper.runtime->getJavaScriptRuntime()->isJsModuleLoaded(moduleId));
}

class CustomViewFactory : public Valdi::ViewFactory {
public:
    CustomViewFactory(StringBox viewClassName, IViewManager& viewManager, Ref<BoundAttributes> boundAttributes)
        : Valdi::ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)) {}

    size_t getCreatedViewCount() const {
        return _createdViewCount;
    }

protected:
    Valdi::Ref<Valdi::View> doCreateView(ViewNodeTree* viewNodeTree, ViewNode* viewNode) override {
        _createdViewCount++;

        const auto& className = getViewClassName();
        return Valdi::makeShared<StandaloneView>(className, false, true);
    }

private:
    size_t _createdViewCount = 0;
};

size_t getPooledViewCount(RuntimeWrapper& wrapper, const StringBox& viewClassName) {
    size_t globalViewSize = 0;

    auto stats = wrapper.standaloneRuntime->getViewManagerContext()->getViewPoolsStats();
    const auto& it = stats.find(viewClassName);
    if (it != stats.end()) {
        globalViewSize = it->second;
    }

    return globalViewSize;
}

TEST_P(RuntimeFixture, canUseDeferredViewClass) {
    Value viewModel;
    viewModel.setMapValue("iosClassName", Value(STRING_LITERAL("InjectedViewClassIOS")));
    viewModel.setMapValue("androidClassName", Value(STRING_LITERAL("InjectedViewClassAndroid")));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DeferredViewClass@test/src/DeferredViewClass"), viewModel, Value());
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("InjectedViewClassIOS")), getRootView(tree));
}

TEST_P(RuntimeFixture, canUseDeferredViewFactory) {
    auto viewClassName = STRING_LITERAL("CustomViewClass");

    auto attributes =
        wrapper.standaloneRuntime->getViewManagerContext()->getAttributesManager().getAttributesForClass(viewClassName);

    auto viewFactory = Valdi::makeShared<CustomViewFactory>(
        viewClassName, wrapper.standaloneRuntime->getViewManagerContext()->getViewManager(), std::move(attributes));

    Value viewModel;
    viewModel.setMapValue("viewFactory", Value(viewFactory));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DeferredViewClass@test/src/DeferredViewClass"), viewModel, Value());
    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView").addChild(DummyView("CustomViewClass")), getRootView(tree));

    ASSERT_EQ(static_cast<size_t>(1), viewFactory->getCreatedViewCount());
}

TEST_P(RuntimeFixture, canUseViewPool) {
    wrapper.standaloneRuntime->getViewManager().setAllowViewPooling(true);

    auto viewClassName = STRING_LITERAL("SCValdiLabel");

    // First populating the global view pool
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.runtime->destroyViewNodeTree(*tree);

    ASSERT_EQ(static_cast<size_t>(1), getPooledViewCount(wrapper, viewClassName));

    // Creating the tree again
    tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    wrapper.waitUntilAllUpdatesCompleted();

    // The view pool should have been dequeued
    ASSERT_EQ(static_cast<size_t>(0), getPooledViewCount(wrapper, viewClassName));

    wrapper.runtime->destroyViewNodeTree(*tree);

    // View pool should be populated again after destroy
    ASSERT_EQ(static_cast<size_t>(1), getPooledViewCount(wrapper, viewClassName));

    // Now creating a new tree again, but injecting a ViewFactory

    auto attributes =
        wrapper.standaloneRuntime->getViewManagerContext()->getAttributesManager().getAttributesForClass(viewClassName);

    auto viewFactory =
        Valdi::makeShared<CustomViewFactory>(STRING_LITERAL("SCValdiLabel"),
                                             wrapper.standaloneRuntime->getViewManagerContext()->getViewManager(),
                                             std::move(attributes));

    tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    tree->registerLocalViewFactory(STRING_LITERAL("SCValdiLabel"), viewFactory);
    wrapper.waitUntilAllUpdatesCompleted();

    // The global pool should not have been used
    ASSERT_EQ(static_cast<size_t>(1), getPooledViewCount(wrapper, viewClassName));

    wrapper.runtime->destroyViewNodeTree(*tree);

    // We should still have one in the global view pool
    ASSERT_EQ(static_cast<size_t>(1), getPooledViewCount(wrapper, viewClassName));
    // We should have one view enqueued into our custom view factory
    ASSERT_EQ(static_cast<size_t>(1), viewFactory->getPoolSize());
    // We should have created one view instance from the view factory
    ASSERT_EQ(static_cast<size_t>(1), viewFactory->getCreatedViewCount());

    // Recreating the tree, and re-using the same ViewFactory
    tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    tree->registerLocalViewFactory(STRING_LITERAL("SCValdiLabel"), viewFactory);
    wrapper.waitUntilAllUpdatesCompleted();

    // Global pool should not have been touched
    ASSERT_EQ(static_cast<size_t>(1), getPooledViewCount(wrapper, viewClassName));
    // The local pool should be empty, as the view should have been dequeued from the pool
    ASSERT_EQ(static_cast<size_t>(0), viewFactory->getPoolSize());
    ASSERT_EQ(static_cast<size_t>(1), viewFactory->getCreatedViewCount());

    wrapper.runtime->destroyViewNodeTree(*tree);

    ASSERT_EQ(static_cast<size_t>(1), getPooledViewCount(wrapper, viewClassName));
    ASSERT_EQ(static_cast<size_t>(1), viewFactory->getPoolSize());
    ASSERT_EQ(static_cast<size_t>(1), viewFactory->getCreatedViewCount());
}

TEST_P(RuntimeFixture, canUseAndCreateStyles) {
    auto tree = wrapper.createViewNodeTreeAndContext(STRING_LITERAL("StyleTest@test/src/StyleTest"), Value(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("backgroundColor", "red")
                  .addAttribute("scaleX", 0.5)
                  .addAttribute("scaleY", 0.75),
              getRootView(tree));
}

TEST_P(RuntimeFixture, supportsTextAttribute) {
    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("TextAttribute@test/src/TextAttribute"), Value(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootView = getRootView(tree);

    ASSERT_EQ(static_cast<size_t>(1), rootView.getChildrenCount());

    auto value = rootView.getChild(0).getAttribute("value");

    ASSERT_EQ(ValueType::ValdiObject, value.getType());

    auto attributedText = value.getTypedRef<TextAttributeValue>();

    ASSERT_TRUE(attributedText != nullptr);

    ASSERT_EQ("Hello World!?!", attributedText->toString());

    ASSERT_EQ(static_cast<size_t>(6), attributedText->getPartsSize());

    {
        ASSERT_EQ(STRING_LITERAL("Hello"), attributedText->getContentAtIndex(0));
        const auto& style = attributedText->getStyleAtIndex(0);

        ASSERT_EQ(std::nullopt, style.font);
        ASSERT_EQ(TextDecoration::Unset, style.textDecoration);
        ASSERT_EQ(std::nullopt, style.color);
        ASSERT_EQ(nullptr, style.onTap);
    }

    {
        ASSERT_EQ(STRING_LITERAL(" "), attributedText->getContentAtIndex(1));
        const auto& style = attributedText->getStyleAtIndex(1);

        ASSERT_EQ(std::make_optional(STRING_LITERAL("title")), style.font);
        ASSERT_EQ(TextDecoration::Underline, style.textDecoration);
        ASSERT_EQ(std::make_optional(Valdi::Color(static_cast<int64_t>(0xFF0000FF))), style.color);
        ASSERT_EQ(nullptr, style.onTap);
    }

    {
        ASSERT_EQ(STRING_LITERAL("World"), attributedText->getContentAtIndex(2));
        const auto& style = attributedText->getStyleAtIndex(2);

        ASSERT_EQ(std::make_optional(STRING_LITERAL("title")), style.font);
        ASSERT_EQ(TextDecoration::Underline, style.textDecoration);
        ASSERT_EQ(std::make_optional(Valdi::Color(static_cast<int64_t>(0xFF0000FF))), style.color);
        ASSERT_EQ(nullptr, style.onTap);
    }

    {
        ASSERT_EQ(STRING_LITERAL("!"), attributedText->getContentAtIndex(3));
        const auto& style = attributedText->getStyleAtIndex(3);

        ASSERT_EQ(std::nullopt, style.font);
        ASSERT_EQ(TextDecoration::Unset, style.textDecoration);
        ASSERT_EQ(std::make_optional(Valdi::Color(static_cast<int64_t>(0x0000FFFF))), style.color);
        ASSERT_EQ(nullptr, style.onTap);
    }

    {
        ASSERT_EQ(STRING_LITERAL("?"), attributedText->getContentAtIndex(4));
        const auto& style = attributedText->getStyleAtIndex(4);

        ASSERT_EQ(std::nullopt, style.font);
        ASSERT_EQ(TextDecoration::Unset, style.textDecoration);
        ASSERT_EQ(std::make_optional(Valdi::Color(static_cast<int64_t>(0x008000FF))), style.color);
        ASSERT_EQ(nullptr, style.onTap);
    }

    {
        ASSERT_EQ(STRING_LITERAL("!"), attributedText->getContentAtIndex(5));
        const auto& style = attributedText->getStyleAtIndex(5);

        ASSERT_EQ(std::nullopt, style.font);
        ASSERT_EQ(TextDecoration::Unset, style.textDecoration);
        ASSERT_EQ(std::make_optional(Valdi::Color(static_cast<int64_t>(0x0000FFFF))), style.color);
        ASSERT_EQ(nullptr, style.onTap);
    }
}

TEST_P(RuntimeFixture, supportsAccesibilityValueInTextAttribute) {
    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("TextAttribute@test/src/TextAttribute"), Value(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    auto rootViewNode = tree->getRootViewNode();
    ASSERT_TRUE(rootViewNode != nullptr);
    ASSERT_EQ(static_cast<size_t>(1), rootViewNode->getChildCount());

    auto labelViewNode = rootViewNode->getChildAt(0);
    auto accessibilityValue = labelViewNode->getAccessibilityValue();

    ASSERT_EQ(STRING_LITERAL("Hello World!?!"), accessibilityValue);
}

TEST_P(RuntimeFixture, FLAKY_workerWorks) {
    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("WorkerTest@test/src/WorkerTest"), Value(), Value());
    wrapper.waitUntilAllUpdatesCompleted();

    std::promise<Value> valuePromise;
    auto valueFuture = valuePromise.get_future();
    auto callback = makeShared<ValueFunctionWithCallable>([&valuePromise](const auto& callContext) {
        valuePromise.set_value(callContext.getParameter(0));
        return Value::undefined();
    });
    auto params = ValueArray::make(1);
    params->emplace(0, Value(callback));
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("callWorker"), params);

    auto status = valueFuture.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(std::future_status::ready, status);

    auto res = valueFuture.get();
    ASSERT_TRUE(res.isString());
    ASSERT_EQ(res.toString(), "works");
}

TEST_P(RuntimeFixture, canLockAllJSContexts) {
    auto tree1 =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("WorkerTest@test/src/WorkerTest"), Value(), Value());
    auto tree2 =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("WorkerTest@test/src/WorkerTest"), Value(), Value());

    wrapper.waitUntilAllUpdatesCompleted();

    auto callback = makeShared<ValueFunctionWithCallable>([](const auto& callContext) { return Value::undefined(); });

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree1->getContext(), STRING_LITERAL("callWorker"), ValueArray::make({Value(callback)}));
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree2->getContext(), STRING_LITERAL("callWorker"), ValueArray::make({Value(callback)}));

    std::vector<IJavaScriptContext*> jsContexts;
    std::vector<Ref<IJavaScriptContext>> retainedJSContexts;
    wrapper.runtime->getJavaScriptRuntime()->lockAllJSContexts(jsContexts, [&]() {
        for (auto* jsContext : jsContexts) {
            retainedJSContexts.emplace_back(jsContext);
        }
    });

    ASSERT_EQ(static_cast<size_t>(3), retainedJSContexts.size());
}

TEST_P(RuntimeFixture, supportsLoadStrategy) {
    wrapper.runtime->getWorkerQueue()->sync([]() {});

    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_FALSE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));

    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("ModulePreload@test/src/ModulePreload"), Value(), Value());

    // Flush the worker queue since that's where the preload operations are happening
    wrapper.runtime->getWorkerQueue()->sync([]() {});

    // It should have loaded both test and test2
    ASSERT_TRUE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test")));
    ASSERT_TRUE(wrapper.runtime->getResourceManager().isBundleLoaded(STRING_LITERAL("test2")));
}

TEST_P(RuntimeFixture, supportsNativeModule) {
    auto result = callFunctionSync(wrapper, "test/src/NativeModule", "compute", {});

    ASSERT_TRUE(result) << result.description();

    ASSERT_TRUE(result.value().isNumber());
    ASSERT_EQ(50.0, result.value().toDouble());
}

TEST_P(RuntimeFixture, supportsExportedFunction) {
    auto result = snap::valdi_modules::test::MakeCalculator::resolve(*wrapper.runtime->getJavaScriptRuntime(), nullptr);
    ASSERT_TRUE(result) << result.description();

    auto calculator = result.value()();

    calculator->add(10);

    ASSERT_EQ(10.0, calculator->total());

    calculator->mul(3.0);

    ASSERT_EQ(30.0, calculator->total());

    ASSERT_EQ(StringBox::fromCString("30.0"),
              calculator->toString(snap::valdi_modules::test::CalculatorToStringFormat::DECIMAL));
    ASSERT_EQ(StringBox::fromCString("30"),
              calculator->toString(snap::valdi_modules::test::CalculatorToStringFormat::INTEGER));
}

TEST_P(RuntimeFixture, supportsLongObject) {
    auto parseResult =
        callFunctionSync(wrapper, "test/src/LongTest", "parse", {Value(STRING_LITERAL("3670116110564327421"))});

    ASSERT_TRUE(parseResult) << parseResult.description();

    ASSERT_TRUE(parseResult.value().isLong());
    ASSERT_EQ(static_cast<int64_t>(3670116110564327421L), parseResult.value().toLong());
}

class MyNativeObject : public ValdiObject {
    VALDI_CLASS_HEADER_IMPL(MyNativeObject)
};

TEST_P(RuntimeFixture, supportsUserCreatedNativeObjects) {
    Ref<MyNativeObject> nativeObject = makeShared<MyNativeObject>();

    auto objectsManager = wrapper.runtime->getJavaScriptRuntime()->createNativeObjectsManager("");

    ASSERT_EQ(1, nativeObject.use_count());

    auto jsResult =
        callFunctionSync(wrapper, objectsManager, "test/src/WrapNativeObject", "wrapper", {Value(nativeObject)});

    ASSERT_TRUE(jsResult) << jsResult.description();

    // Our native object should have been retained
    ASSERT_EQ(2, nativeObject.use_count());

    ASSERT_TRUE(jsResult.value().isFunction());

    auto unwrapObject = [&]() -> Value {
        return jsResult.value().getFunction()->call(Valdi::ValueFunctionFlagsCallSync, {}).value();
    };

    Value retValue = unwrapObject();

    // We should be able to unwrap our passed object
    ASSERT_TRUE(retValue.isValdiObject());
    ASSERT_EQ(nativeObject, castOrNull<MyNativeObject>(retValue.getValdiObject()));

    // We should have +1 inside the Value
    ASSERT_EQ(3, nativeObject.use_count());

    retValue = Value();

    ASSERT_EQ(2, nativeObject.use_count());

    // Now we dispose the objects manager

    wrapper.runtime->getJavaScriptRuntime()->dispatchSynchronouslyOnJsThread([&](auto& jsEntry) {
        wrapper.runtime->getJavaScriptRuntime()->destroyNativeObjectsManager(objectsManager);
        return;
    });

    // The reference should have been released
    ASSERT_EQ(1, nativeObject.use_count());

    // We should not be able to unwrap the object anymore
    retValue = unwrapObject();

    ASSERT_TRUE(retValue.isUndefined());
}

TEST_P(RuntimeFixture, destroysNativeRefsWhenUserCreatedContextIsDestroyed) {
    Ref<MyNativeObject> nativeObject = makeShared<MyNativeObject>();

    auto jsResult = callFunctionSync(wrapper,
                                     nullptr,
                                     "test/src/DestroyRefOnChildContext",
                                     "createChildContext",
                                     {Value(makeShared<ValueFunctionWithCallable>(
                                         [nativeObject = nativeObject.get()](const auto& callContext) -> Value {
                                             return Value(Ref<ValdiObject>(nativeObject));
                                         }))});

    ASSERT_TRUE(jsResult) << jsResult.description();

    auto contextId = static_cast<ContextId>(jsResult.value().getMapValue("contextId").toInt());

    ASSERT_EQ(2, contextId);

    // 1 ref in the test, 1 in JS
    ASSERT_EQ(2, nativeObject.use_count());

    wrapper.runtime->getContextManager().destroyContext(contextId);
    wrapper.flushQueues();

    // The native reference should have been released
    ASSERT_EQ(1, nativeObject.use_count());
}

TEST_P(RuntimeFixture, canDumpLogs) {
    wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DumpLogTest@test/src/DumpLogTest"), Value(makeShared<ValueMap>()), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    auto logs = wrapper.runtime->dumpLogs(true, true, false);

    ASSERT_EQ(static_cast<size_t>(2), logs.getVerbose().size());

    auto firstVerbose = logs.getVerbose()[0];

    ASSERT_EQ(STRING_LITERAL("[JS] Valdi Runtime"), firstVerbose.name);
    ASSERT_EQ(Value()
                  .setMapValue("treeId", Value(2.0))
                  .setMapValue("componentsCount", Value(2.0))
                  .setMapValue("elementsCount", Value(2.0))
                  .setMapValue("componentRerendersCount", Value(0.0))
                  .setMapValue("slotsCount", Value(0.0))
                  .setMapValue("rootRendersCount", Value(1.0)),
              firstVerbose.data);

    auto secondVerbose = logs.getVerbose()[1];

    ASSERT_EQ(STRING_LITERAL("[JS] CustomLog"), secondVerbose.name);
    ASSERT_EQ(Value(STRING_LITERAL("test123")), secondVerbose.data);

    ASSERT_EQ("ComponentPaths = [ DumpLogTest@test/src/DumpLogTest ]\nmyKey = myValue\n",
              logs.serializeMetadata().slowToString());
}

TEST_P(RuntimeFixture, canDumpLogsMetadataOnly) {
    wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DumpLogTest@test/src/DumpLogTest"), Value(makeShared<ValueMap>()), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    auto logs = wrapper.runtime->dumpLogs(true, false, false);

    ASSERT_TRUE(logs.getVerbose().empty());

    ASSERT_EQ("ComponentPaths = [ DumpLogTest@test/src/DumpLogTest ]\nmyKey = myValue\n",
              logs.serializeMetadata().slowToString());
}

TEST_P(RuntimeFixture, canDumpLogsVerboseOnly) {
    wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DumpLogTest@test/src/DumpLogTest"), Value(makeShared<ValueMap>()), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    auto logs = wrapper.runtime->dumpLogs(false, true, false);

    ASSERT_FALSE(logs.getVerbose().empty());
    ASSERT_EQ(static_cast<size_t>(2), logs.getVerbose().size());

    ASSERT_TRUE(logs.getMetadata().empty());
}

TEST_P(RuntimeFixture, canDumpLogsInBackgroundThread) {
    wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DumpLogTest@test/src/DumpLogTest"), Value(makeShared<ValueMap>()), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    DumpedLogs result;

    auto queue = DispatchQueue::createThreaded(STRING_LITERAL("Background Thread"), ThreadQoSClassHigh);
    queue->sync([&]() { result = wrapper.runtime->dumpLogs(true, true, false); });

    ASSERT_EQ(static_cast<size_t>(2), result.getVerbose().size());
    ASSERT_EQ(static_cast<size_t>(2), result.getMetadata().size());
}

TEST_P(RuntimeFixture, doesntDumpJsLogsWhenCrashing) {
    wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DumpLogTest@test/src/DumpLogTest"), Value(makeShared<ValueMap>()), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    auto logs = wrapper.runtime->dumpLogs(true, false, true);

    ASSERT_TRUE(logs.getVerbose().empty());

    ASSERT_EQ("ComponentPaths = [ DumpLogTest@test/src/DumpLogTest ]\n", logs.serializeMetadata().slowToString());
}

TEST_P(RuntimeFixture, rendererAndComponentUnregistersFromDumpLogWhenDestroyed) {
    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("DumpLogTest@test/src/DumpLogTest"), Value(makeShared<ValueMap>()), Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    auto logs = wrapper.runtime->dumpLogs(true, true, false);

    ASSERT_EQ(static_cast<size_t>(2), logs.getVerbose().size());
    ASSERT_EQ(static_cast<size_t>(2), logs.getMetadata().size());

    wrapper.runtime->destroyContext(tree->getContext());
    wrapper.flushQueues();

    logs = wrapper.runtime->dumpLogs(true, true, false);

    ASSERT_EQ(static_cast<size_t>(0), logs.getVerbose().size());
    ASSERT_EQ(static_cast<size_t>(1), logs.getMetadata().size());

    ASSERT_EQ("ComponentPaths = [  ]\n", logs.serializeMetadata().slowToString());
}

struct MockRuntimeMessageHandler : public snap::valdi::RuntimeMessageHandler {
    struct Messages {
        std::vector<std::pair<StringBox, StringBox>> errors;
        std::vector<std::pair<StringBox, StringBox>> errorStacks;
        std::vector<std::pair<StringBox, StringBox>> crashes;
        std::vector<std::pair<StringBox, StringBox>> crashStacks;
        std::vector<std::pair<int32_t, std::string>> debugMessages;
    };

    MockRuntimeMessageHandler() = default;

    Messages messages() const {
        std::lock_guard<Mutex> guard(_mutex);
        return _messages;
    }

    void clear() {
        std::lock_guard<Mutex> guard(_mutex);
        _messages = Messages();
    }

    void onUncaughtJsError(int32_t errorCode,
                           const Valdi::StringBox& moduleName,
                           const std::string& errorMessage,
                           const std::string& stackTrace) override {
        std::lock_guard<Mutex> guard(_mutex);
        _messages.errors.emplace_back(std::make_pair(moduleName, StringCache::getGlobal().makeString(errorMessage)));
        _messages.errorStacks.emplace_back(std::make_pair(moduleName, StringCache::getGlobal().makeString(stackTrace)));
    }

    void onJsCrash(const Valdi::StringBox& moduleName,
                   const std::string& errorMessage,
                   const std::string& stackTrace,
                   bool isANR) override {
        std::lock_guard<Mutex> guard(_mutex);
        _messages.crashes.emplace_back(std::make_pair(moduleName, StringCache::getGlobal().makeString(errorMessage)));
        _messages.crashStacks.emplace_back(std::make_pair(moduleName, StringCache::getGlobal().makeString(stackTrace)));
    }

    void onDebugMessage(int32_t level, const std::string& message) override {
        std::lock_guard<Mutex> guard(_mutex);
        _messages.debugMessages.emplace_back(std::make_pair(level, message));
    }

private:
    mutable Mutex _mutex;
    Messages _messages;
};

TEST_P(RuntimeFixture, attributesRenderErrorToContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(tree->getContext(),
                                                                   STRING_LITERAL("makeRenderError"));

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("Failed to render component 'AttributedError'"));
    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, attributesImportErrorToContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(tree->getContext(),
                                                                   STRING_LITERAL("makeImportError"));

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains(
        "No item named 'src/NotARealFile.js' in module 'invalid_module'"));

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, attributesImportInitErrorToContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(tree->getContext(),
                                                                   STRING_LITERAL("makeImportInitError"));

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("I throw on creation"));

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, attributesSetTimeoutErrorToContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(tree->getContext(),
                                                                   STRING_LITERAL("makeSetTimeoutError"));

    wrapper.flushQueues();
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("Error in setTimeout"));

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, attributesExternalCallbacksToContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    SharedAtomic<Value> jsCallbackHolder;

    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        jsCallbackHolder.set(callContext.getParameter(callContext.getParametersSize() - 1));
        return Value::undefined();
    });

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("makeCallbackError"), ValueArray::make({Value(callback)}));

    wrapper.flushQueues();

    auto jsCallback = jsCallbackHolder.get();

    ASSERT_TRUE(jsCallback.isFunction());
    (*jsCallback.getFunction())(ValueFunctionFlagsCallSync, {});

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("Error in Callback"));

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, attributesDelayedImportErrorToGlobalContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(tree->getContext(),
                                                                   STRING_LITERAL("makeDelayedImportError"));

    wrapper.flushQueues();
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL(""), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("I throw right after creation"));

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, doesntEmitErrorWhenUsingInterruptibleCallback) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    SharedAtomic<Value> jsCallbackHolder;
    SharedAtomic<Value> jsInterruptibleCallbackHolder;
    SharedAtomic<Value> jsInterruptibleCallbackHolder2;

    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        jsCallbackHolder.set(callContext.getParameter(callContext.getParametersSize() - 1));
        return Value::undefined();
    });
    auto callback2 = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        jsInterruptibleCallbackHolder.set(callContext.getParameter(callContext.getParametersSize() - 1));
        return Value::undefined();
    });
    auto callback3 = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        jsInterruptibleCallbackHolder2.set(callContext.getParameter(callContext.getParametersSize() - 1));
        return Value::undefined();
    });

    auto schema = ValueSchema::cls(
        STRING_LITERAL("MakeInterruptibleCallbackParams"),
        false,
        {
            ClassPropertySchema(STRING_LITERAL("interruptible"), ValueSchema::boolean()),
            ClassPropertySchema(
                STRING_LITERAL("cb"),
                ValueSchema::function(
                    ValueFunctionSchemaAttributes(),
                    ValueSchema::voidType(),
                    {
                        ValueSchema::function(ValueFunctionSchemaAttributes(), ValueSchema::doublePrecision(), {}),
                    })),
        });

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(),
        STRING_LITERAL("makeInterruptibleCallback"),
        ValueArray::make({Value().setMapValue("interruptible", Value(false)).setMapValue("cb", Value(callback))}));
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(),
        STRING_LITERAL("makeInterruptibleCallback"),
        ValueArray::make({Value().setMapValue("interruptible", Value(true)).setMapValue("cb", Value(callback2))}));
    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(),
        STRING_LITERAL("makeInterruptibleCallback"),
        ValueArray::make({Value(ValueTypedObject::make(schema.getClassRef(), {Value(true), Value(callback3)}))}));

    wrapper.flushQueues();

    auto jsCallback = jsCallbackHolder.get();
    auto jsInterruptibleCallback = jsInterruptibleCallbackHolder.get();
    auto jsInterruptibleCallback2 = jsInterruptibleCallbackHolder2.get();

    ASSERT_TRUE(jsCallback.isFunction());
    ASSERT_TRUE(jsInterruptibleCallback.isFunction());
    ASSERT_TRUE(jsInterruptibleCallback2.isFunction());
    auto result = (*jsCallback.getFunction())(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(42.0), result.value());

    result = (*jsInterruptibleCallback.getFunction())(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(42.0), result.value());

    result = (*jsInterruptibleCallback2.getFunction())(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(42.0), result.value());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    // Now destroy the component and try calling again

    wrapper.runtime->destroyContext(tree->getContext());

    wrapper.flushQueues();

    result = (*jsCallback.getFunction())(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value::undefined(), result.value());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("Cannot unwrap JS value reference"));

    messageHandler->clear();

    result = (*jsInterruptibleCallback.getFunction())(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value::undefined(), result.value());

    result = (*jsInterruptibleCallback2.getFunction())(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value::undefined(), result.value());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
}

TEST_P(RuntimeFixture, attributesHotReloadErrorOnContext) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    Value viewModel;
    viewModel.setMapValue("shouldErrorOut", Value(static_cast<bool>(true)));

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("StyleTest@test/src/StyleTest"), viewModel, Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    // Do a normal hot reload first

    auto result = wrapper.hotReload("test/src/AttributedError", "test/src/StyleTest");
    ASSERT_TRUE(result.success());
    wrapper.waitUntilAllUpdatesCompleted();
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("Failed to render component"));
    ASSERT_EQ(static_cast<size_t>(2), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, showStackTraceResponsibleForEmittingDanglingReference) {
    wrapper.runtime->getJavaScriptRuntime()->setForceStackTraceCapture(true);

    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto tree = wrapper.createViewNodeTreeAndContext(
        STRING_LITERAL("AttributedError@test/src/AttributedError"), Value::undefined(), Value::undefined());

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().debugMessages.size());

    SharedAtomic<Value> jsCallbackHolder;

    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        jsCallbackHolder.set(callContext.getParameter(callContext.getParametersSize() - 1));
        return Value::undefined();
    });

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(
        tree->getContext(), STRING_LITERAL("makeCallbackError"), ValueArray::make({Value(callback)}));

    wrapper.flushQueues();

    auto jsCallback = jsCallbackHolder.get();

    wrapper.runtime->destroyContext(tree->getContext());

    wrapper.flushQueues();

    ASSERT_TRUE(jsCallback.isFunction());
    (*jsCallback.getFunction())({});

    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_EQ(STRING_LITERAL("test"), messageHandler->messages().errors[0].first);

    auto lines = messageHandler->messages().errors[0].second.split('\n');
    auto stackLines = messageHandler->messages().errorStacks[0].second.split('\n');
    ASSERT_EQ(
        STRING_LITERAL("Cannot unwrap JS value reference 'makeCallbackError() -> <parameter 2>[0]() -> "
                       "<parameter 0>()' as it was disposed. Reference was taken from context id 2 with component path "
                       "AttributedError@test/src/AttributedError"),
        lines[0]);

    if (isJSCore()) {
        ASSERT_TRUE(stackLines.size() >= 2);
        ASSERT_TRUE(stackLines[0].contains("native_code"));
        ASSERT_TRUE(stackLines[1].contains("test/src/AttributedError"));
    } else if (isHermes()) {
        ASSERT_TRUE(stackLines.size() >= 3);
        ASSERT_TRUE(stackLines[0].contains("Error"));
        ASSERT_TRUE(stackLines[1].contains("native"));
        ASSERT_TRUE(stackLines[2].contains("test/src/AttributedError"));
    } else {
        ASSERT_TRUE(stackLines[0].contains("makeCallbackError"));
        ASSERT_TRUE(stackLines[0].contains("test/src/AttributedError"));
    }

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().debugMessages.size());
}

TEST_P(RuntimeFixture, scopeNameAppearsInDisposedReferenceError) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    Ref<MyNativeObject> nativeObject = makeShared<MyNativeObject>();

    // Create objectsManager with a specific scopeName
    auto objectsManager =
        wrapper.runtime->getJavaScriptRuntime()->createNativeObjectsManager("MyFeature.MyCallsite");

    // Wrap native object in JS
    auto jsResult =
        callFunctionSync(wrapper, objectsManager, "test/src/WrapNativeObject", "wrapper", {Value(nativeObject)});
    ASSERT_TRUE(jsResult) << jsResult.description();

    auto unwrapObject = [&]() {
        // Call the function - we expect it to fail, so don't call .value() on the result
        // The error will be logged to messageHandler
        (void)jsResult.value().getFunction()->call(Valdi::ValueFunctionFlagsCallSync, {});
    };

    // Dispose the objectsManager
    wrapper.runtime->getJavaScriptRuntime()->dispatchSynchronouslyOnJsThread([&](auto& jsEntry) {
        wrapper.runtime->getJavaScriptRuntime()->destroyNativeObjectsManager(objectsManager);
        return;
    });

    // Try to unwrap - should trigger error with scopeName (error is logged, not thrown)
    unwrapObject();
    wrapper.flushQueues();

    // Verify error contains scopeName
    ASSERT_GE(messageHandler->messages().errors.size(), static_cast<size_t>(1));
    auto errorMessage = messageHandler->messages().errors[0].second;
    ASSERT_TRUE(errorMessage.contains("MyFeature.MyCallsite"))
        << "Expected error message to contain scopeName 'MyFeature.MyCallsite', got: " << errorMessage.slowToString();
}

TEST_P(RuntimeFixture, supportsSingleCallJsFunction) {
    auto getCallbackFn =
        getJsModulePropertyWithSchema(wrapper.runtime, "test/src/SingleCall", "getCallback", "f():f!():s");

    ASSERT_TRUE(getCallbackFn) << getCallbackFn.description();

    auto singleCallbackResult = getCallbackFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_TRUE(singleCallbackResult) << singleCallbackResult.description();

    auto singleCallback = singleCallbackResult.value().getFunctionRef();

    ASSERT_TRUE(singleCallback != nullptr);

    // We should be able to call it the first time

    auto callResult = singleCallback->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ("Hello World", callResult.value().toString());

    // The second time it should fail
    callResult = singleCallback->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_FALSE(callResult);
}

TEST_P(RuntimeFixture, supportsSingleCallValueFunction) {
    auto callCallbackFn =
        getJsModulePropertyWithSchema(wrapper.runtime, "test/src/SingleCall", "callCallback", "f(i,f!())");

    ASSERT_TRUE(callCallbackFn) << callCallbackFn.description();

    auto cb = makeShared<ValueFunctionWithCallable>([](const auto& callContext) { return Value::undefined(); });

    // When calling the function only one time, it should succeed

    auto callResult =
        callCallbackFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {Value(1), Value(cb)});
    ASSERT_TRUE(callResult);
    callResult = callCallbackFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {Value(1), Value(cb)});
    ASSERT_TRUE(callResult);

    // If we pass more than 1 iteration to call the callback, on the second iteration the JS to ValueFunction
    // reference should be torn down
    callResult = callCallbackFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {Value(2), Value(cb)});
    ASSERT_FALSE(callResult);
}

TEST_P(RuntimeFixture, supportsSingleCallFunctionExplicitlyExportedFromJS) {
    auto jsResult = callFunctionSync(wrapper, "test/src/SingleCall", "getSingleCallback", {});

    ASSERT_TRUE(jsResult) << jsResult.description();
    ASSERT_TRUE(jsResult.value().isFunction()) << jsResult.value().toString();

    auto getSingleCallbackFn = jsResult.value().getFunctionRef();

    // First time it should work
    auto result = getSingleCallbackFn->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_TRUE(result) << result.description();
    // It should fail on the second attempt
    result = getSingleCallbackFn->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_FALSE(result) << result.description();
}

TEST_P(RuntimeFixture, supportsRegularCallWithPromiseReturnValue) {
    auto getStringAsyncFn =
        getJsModulePropertyWithSchema(wrapper.runtime, "test/src/Promise", "getStringAsync", "f(): p<s>");

    ASSERT_TRUE(getStringAsyncFn) << getStringAsyncFn.value();

    auto callResult = getStringAsyncFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_TRUE(callResult) << callResult.description();

    auto promise = callResult.value().getTypedRef<Promise>();

    ASSERT_TRUE(promise != nullptr);

    auto promiseResult = waitForPromise(promise);

    ASSERT_TRUE(promiseResult) << promiseResult.description();

    ASSERT_EQ("Hello World Async", promiseResult.value().toString());
}

static Result<Ref<Promise>> callValueFunctionWithPromiseReturnValue(const Value& function,
                                                                    std::initializer_list<Value> parameters) {
    SimpleExceptionTracker exceptionTracker;
    auto result = function.getFunction()->call(ValueFunctionFlagsNeverCallSync, std::move(parameters));
    if (!result) {
        return result.moveError();
    }

    auto promise = result.value().checkedToValdiObject<Promise>(exceptionTracker);
    return exceptionTracker.toResult(std::move(promise));
}

TEST_P(RuntimeFixture, supportsPromiseCallWithPromiseReturnValue) {
    auto getStringAsyncFn =
        getJsModulePropertyWithSchema(wrapper.runtime, "test/src/Promise", "getStringAsync", "f(): p<s>");

    ASSERT_TRUE(getStringAsyncFn) << getStringAsyncFn.value();

    auto promise = callValueFunctionWithPromiseReturnValue(getStringAsyncFn.value(), {});
    ASSERT_TRUE(promise) << promise.description();

    auto promiseResult = waitForPromise(promise.value());

    ASSERT_TRUE(promiseResult) << promiseResult.description();

    ASSERT_EQ("Hello World Async", promiseResult.value().toString());
}

TEST_P(RuntimeFixture, supportsPromiseCallWithRegularReturnValue) {
    auto getStringSync = getJsModulePropertyWithSchema(wrapper.runtime, "test/src/Promise", "getStringSync", "f(): s");

    ASSERT_TRUE(getStringSync) << getStringSync.value();

    auto promise = callValueFunctionWithPromiseReturnValue(getStringSync.value(), {});
    ASSERT_TRUE(promise) << promise.description();

    auto promiseResult = waitForPromise(promise.value());

    ASSERT_TRUE(promiseResult) << promiseResult.description();

    ASSERT_EQ("Hello World Sync", promiseResult.value().toString());
}

TEST_P(RuntimeFixture, supportsPromiseCallWithPromiseParametersStepByStep) {
    if (isJSCore()) {
        GTEST_SKIP() << "TODO(1929): The test is racy with the promise polyfill used on Linux JSCore.";
    }

    auto concatStringsFn = getJsModulePropertyWithSchema(
        wrapper.runtime, "test/src/Promise", "concatStrings", "f(p<s>, p<s>, f?(i)): p<s>");
    ASSERT_TRUE(concatStringsFn) << concatStringsFn.value();

    auto leftPromise = makeShared<ResolvablePromise>();
    auto rightPromise = makeShared<ResolvablePromise>();

    SharedAtomic<int32_t> currentStep;

    auto onStep = makeShared<ValueFunctionWithCallable>([currentStep](const ValueFunctionCallContext& callContext) {
        currentStep.set(callContext.getParameterAsInt(0));
        return Value::undefined();
    });

    auto promise = callValueFunctionWithPromiseReturnValue(concatStringsFn.value(),
                                                           {Value(leftPromise), Value(rightPromise), Value(onStep)});
    ASSERT_TRUE(promise) << promise.description();

    wrapper.flushJsQueue();

    // We should have reached the first step
    ASSERT_EQ(1, currentStep.get());

    // Resolve first promise

    leftPromise->fulfill(Value(STRING_LITERAL("Hello")));
    wrapper.flushJsQueue();

    // We should have reached the second step
    ASSERT_EQ(2, currentStep.get());

    rightPromise->fulfill(Value(STRING_LITERAL("World")));
    wrapper.flushJsQueue();

    // We should have reached the last step
    ASSERT_EQ(3, currentStep.get());

    // We can now get our result

    auto promiseResult = waitForPromise(promise.value());

    ASSERT_TRUE(promiseResult) << promiseResult.description();

    ASSERT_EQ("Hello_World", promiseResult.value().toString());
}

TEST_P(RuntimeFixture, supportsPromiseCallWithPromiseParameters) {
    auto concatStringsFn = getJsModulePropertyWithSchema(
        wrapper.runtime, "test/src/Promise", "concatStrings", "f(p<s>, p<s>, f?(i)): p<s>");
    ASSERT_TRUE(concatStringsFn) << concatStringsFn.value();

    auto leftPromise = makeShared<ResolvablePromise>();
    auto rightPromise = makeShared<ResolvablePromise>();

    auto promise = callValueFunctionWithPromiseReturnValue(
        concatStringsFn.value(), {Value(leftPromise), Value(rightPromise), Value::undefined()});
    ASSERT_TRUE(promise) << promise.description();

    leftPromise->fulfill(Value(STRING_LITERAL("LeftParam")));
    rightPromise->fulfill(Value(STRING_LITERAL("RightParam")));

    auto promiseResult = waitForPromise(promise.value());

    ASSERT_TRUE(promiseResult) << promiseResult.description();

    ASSERT_EQ("LeftParam_RightParam", promiseResult.value().toString());
}

TEST_P(RuntimeFixture, propagatesPromiseError) {
    auto concatStringsFn = getJsModulePropertyWithSchema(
        wrapper.runtime, "test/src/Promise", "concatStrings", "f(p<s>, p<s>, f?(i)): p<s>");
    ASSERT_TRUE(concatStringsFn) << concatStringsFn.value();

    auto leftPromise = makeShared<ResolvablePromise>();
    auto rightPromise = makeShared<ResolvablePromise>();

    auto promise = callValueFunctionWithPromiseReturnValue(
        concatStringsFn.value(), {Value(leftPromise), Value(rightPromise), Value::undefined()});
    ASSERT_TRUE(promise) << promise.description();

    leftPromise->fulfill(Error("You shall not pass"));

    auto promiseResult = waitForPromise(promise.value());

    ASSERT_FALSE(promiseResult) << promiseResult.description();

    auto promiseError = promiseResult.moveError();

    ASSERT_EQ("You shall not pass", promiseError.getMessage().toStringView());

    // Try again while fulfilling the first promise and failing the second one

    leftPromise = makeShared<ResolvablePromise>();
    rightPromise = makeShared<ResolvablePromise>();

    promise = callValueFunctionWithPromiseReturnValue(concatStringsFn.value(),
                                                      {Value(leftPromise), Value(rightPromise), Value::undefined()});
    ASSERT_TRUE(promise) << promise.description();

    leftPromise->fulfill(Value(STRING_LITERAL("This is great")));
    rightPromise->fulfill(Error("That is a fail again"));

    promiseResult = waitForPromise(promise.value());

    ASSERT_FALSE(promiseResult) << promiseResult.description();

    promiseError = promiseResult.moveError();

    ASSERT_EQ("That is a fail again", promiseError.getMessage().toStringView());
}

TEST_P(RuntimeFixture, supportsComplexPromiseChains) {
    auto concatStringsComplexFn = getJsModulePropertyWithSchema(
        wrapper.runtime, "test/src/Promise", "concatStringsComplex", "f(p<p<s>>, a<p<s>>): p<s>");
    ASSERT_TRUE(concatStringsComplexFn) << concatStringsComplexFn.value();

    auto suffixPromisePromise = makeShared<ResolvablePromise>();
    auto suffixPromise = makeShared<ResolvablePromise>();

    auto firstItem = makeShared<ResolvablePromise>();
    auto secondItem = makeShared<ResolvablePromise>();
    auto thirdItem = makeShared<ResolvablePromise>();
    auto fourthItem = makeShared<ResolvablePromise>();

    auto promiseArray = ValueArray::make({Value(firstItem), Value(secondItem), Value(thirdItem), Value(fourthItem)});

    auto promise = callValueFunctionWithPromiseReturnValue(concatStringsComplexFn.value(),
                                                           {Value(suffixPromisePromise), Value(promiseArray)});
    ASSERT_TRUE(promise) << promise.description();

    suffixPromisePromise->fulfill(Value(suffixPromise));
    suffixPromise->fulfill(Value(STRING_LITERAL("CheckThisOut")));

    // Resolve items in random order
    secondItem->fulfill(Value(STRING_LITERAL("Item#2")));
    fourthItem->fulfill(Value(STRING_LITERAL("Item#4")));
    firstItem->fulfill(Value(STRING_LITERAL("Item#1")));
    thirdItem->fulfill(Value(STRING_LITERAL("Item#3")));

    auto promiseResult = waitForPromise(promise.value());

    ASSERT_TRUE(promiseResult) << promiseResult.description();

    ASSERT_EQ("CheckThisOut->Item#1_Item#2_Item#3_Item#4", promiseResult.value().toString());
}

class TestPropagatesContextFunction : public ValueFunction {
public:
    explicit TestPropagatesContextFunction(bool propagatesOwnerContextOnCall)
        : _propagatesOwnerContextOnCall(propagatesOwnerContextOnCall) {}
    ~TestPropagatesContextFunction() override = default;

    Value operator()(const ValueFunctionCallContext& callContext) noexcept final {
        return Value(makeShared<ValueFunctionWithCallable>(
            [](const ValueFunctionCallContext& /*callContext*/) { return Value(42.0); }));
    }

    std::string_view getFunctionType() const override {
        return "test_fn";
    }

    bool propagatesOwnerContextOnCall() const override {
        return _propagatesOwnerContextOnCall;
    }

private:
    bool _propagatesOwnerContextOnCall;
};

class NativeModuleReferenceTest : public SharedPtrRefCountable, public snap::valdi_core::ModuleFactory {
public:
    StringBox getModulePath() override {
        return STRING_LITERAL("NativeModuleReferenceTest");
    }

    Value loadModule() override {
        auto fnWithPropagates = makeShared<TestPropagatesContextFunction>(true);
        auto fnWithoutPropagates = makeShared<TestPropagatesContextFunction>(false);

        return Value()
            .setMapValue("makeCallback1", Value(fnWithPropagates))
            .setMapValue("makeCallback2", Value(fnWithoutPropagates));
    }
};

TEST_P(RuntimeFixture, respectspropagatesOwnerContextOnCall) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);
    wrapper.runtime->registerNativeModuleFactory(makeShared<NativeModuleReferenceTest>().toShared());

    auto tree = wrapper.runtime->createViewNodeTreeAndContext(
        wrapper.standaloneRuntime->getViewManagerContext(),
        STRING_LITERAL("NativeModuleReference@test/src/NativeModuleReference"));

    wrapper.waitUntilAllUpdatesCompleted();

    auto callCallback1 = getJsModulePropertyAsUntypedFunction(
        wrapper.runtime, nullptr, "test/src/NativeModuleReference", "callCallback1");
    ASSERT_TRUE(callCallback1) << callCallback1.description();

    auto callCallback2 = getJsModulePropertyAsUntypedFunction(
        wrapper.runtime, nullptr, "test/src/NativeModuleReference", "callCallback2");
    ASSERT_TRUE(callCallback2) << callCallback2.description();

    Value retValue;

    retValue = callCallback1.value()->call(ValueFunctionFlagsCallSync, {}).value();

    ASSERT_EQ(Value(42.0), retValue);

    retValue = callCallback2.value()->call(ValueFunctionFlagsCallSync, {}).value();
    ASSERT_EQ(Value(42.0), retValue);

    // We now destroy our context, which should make callback2 unusable
    wrapper.runtime->destroyContext(tree->getContext());
    wrapper.flushQueues();

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());

    retValue = callCallback1.value()->call(ValueFunctionFlagsCallSync, {}).value();

    ASSERT_EQ(Value(42.0), retValue);
    wrapper.flushQueues();
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());

    // Callback2 should fail
    retValue = callCallback2.value()->call(ValueFunctionFlagsCallSync, {}).value();

    ASSERT_EQ(Value::undefined(), retValue);
    wrapper.flushQueues();
    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
}

TEST_P(RuntimeFixture, propagatesNativeCallErrorToJsError) {
    auto receiveError =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nullptr, "test/src/ErrorPropagation", "receiveError");
    ASSERT_TRUE(receiveError) << receiveError.description();

    auto cb = makeShared<ValueFunctionWithCallable>([](const auto& callContext) {
        callContext.getExceptionTracker().onError(Error("This call failed"));
        return Value::undefined();
    });

    auto result = receiveError.value()->call(ValueFunctionFlagsCallSync, {Value(cb)});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(ValueType::InternedString, result.value().getType());
    ASSERT_EQ(Value(STRING_LITERAL("This call failed")), result.value());
}

TEST_P(RuntimeFixture, canPropagateJSErrorToCaller) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto throwError =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nullptr, "test/src/ErrorPropagation", "throwError");
    ASSERT_TRUE(throwError) << throwError.description();

    wrapper.flushQueues();
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());

    auto result = (*throwError.value())(ValueFunctionFlagsCallSync, {});
    wrapper.flushQueues();

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value::undefined(), result.value());

    // Message should have been passed to the message handler
    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_TRUE(messageHandler->messages().errors[0].second.contains("This is an error"));

    messageHandler->clear();

    // Now call with propagate error flag

    result = (*throwError.value())(ValueFunctionFlagsPropagatesError, {});
    wrapper.flushQueues();

    ASSERT_FALSE(result) << result.description();
    ASSERT_TRUE(result.error().toStringBox().contains("This is an error"));

    // Nothing should have been passed to the message handler
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
}

TEST_P(RuntimeFixture, canSymbolicateError) {
    std::string symbolicateModuleBody = R"""(
    module.exports.symbolicate = function(error) {
        return new Error('Symbolicated error from: ' + error.message);
    };
    )""";

    wrapper.hotReload(STRING_LITERAL("valdi_core"), STRING_LITERAL("src/Symbolicator.js"), symbolicateModuleBody);

    auto finalError = Error("Invalid error");

    wrapper.runtime->getJavaScriptRuntime()->dispatchOnJsThread(
        nullptr, JavaScriptTaskScheduleTypeAlwaysSync, 0, [&](JavaScriptEntryParameters& entry) {
            auto error = entry.jsContext.newError("This is an error", std::nullopt, entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                return;
            }

            auto retainedError = JSValueRef::makeRetained(entry.jsContext, error.get());

            finalError = convertJSErrorToValdiError(entry.jsContext, retainedError, nullptr);
        });

    ASSERT_TRUE(finalError.toStringBox().hasPrefix("Symbolicated error from: This is an error"));
}

TEST_P(RuntimeFixture, handlesFailureSafelyInSymbolication) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    std::string symbolicateModuleBody = R"""(
    module.exports.symbolicate = function(error) {
        throw new Error('I Am Broken');
    };
    )""";

    wrapper.hotReload(STRING_LITERAL("valdi_core"), STRING_LITERAL("src/Symbolicator.js"), symbolicateModuleBody);

    auto finalError = Error("Invalid error");

    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());

    wrapper.runtime->getJavaScriptRuntime()->dispatchOnJsThread(
        nullptr, JavaScriptTaskScheduleTypeAlwaysSync, 0, [&](JavaScriptEntryParameters& entry) {
            auto error = entry.jsContext.newError("This is an error", std::nullopt, entry.exceptionTracker);
            if (!entry.exceptionTracker) {
                return;
            }
            auto retainedError = JSValueRef::makeRetained(entry.jsContext, error.get());

            finalError = convertJSErrorToValdiError(entry.jsContext, retainedError, nullptr);
        });

    wrapper.flushQueues();

    ASSERT_TRUE(finalError.toStringBox().hasPrefix("This is an error"));

    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    ASSERT_TRUE(messageHandler->messages().errors[0].second.hasPrefix(
        "Recoverable JS Error while performing action 'symbolicateError'\n[caused by]: I Am Broken"));
}

TEST_P(RuntimeFixture, supportsUncaughtExceptionHandler) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto emitUncaughtError =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nullptr, "test/src/ErrorHandler", "emitUncaughtError");
    ASSERT_TRUE(emitUncaughtError) << emitUncaughtError.description();

    SharedAtomic<Value> resultHolder;

    auto cb = makeShared<ValueFunctionWithCallable>([resultHolder](const auto& callContext) {
        resultHolder.set(callContext.getParameter(0));
        return Value(static_cast<int32_t>(/* IGNORE */ 1));
    });

    auto callResult = emitUncaughtError.value()->call(ValueFunctionFlagsCallSync, {Value(cb)});

    ASSERT_TRUE(callResult) << callResult.description();

    wrapper.flushQueues();
    wrapper.flushQueues();

    auto result = resultHolder.get();

    ASSERT_TRUE(result.isError());

    ASSERT_EQ(STRING_LITERAL("This has thrown an error"), result.getError().getMessage());
    ASSERT_TRUE(result.getError().getStack().contains("test/src/ErrorHandler"));
    // Should have no errors have we returned IGNORED
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
}

TEST_P(RuntimeFixture, supportsUnhandledRejectionHandler) {
    if (isJSCore() || isHermes()) {
        GTEST_SKIP() << "TODO(1929): This test doesn't work with the promise polyfill used on Linux JSCore "
                        "and on Hermes.";
    }

    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto emitRejectedPromise =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nullptr, "test/src/ErrorHandler", "emitRejectedPromise");
    ASSERT_TRUE(emitRejectedPromise) << emitRejectedPromise.description();

    SharedAtomic<Value> resultHolder;

    auto cb = makeShared<ValueFunctionWithCallable>([resultHolder](const auto& callContext) {
        resultHolder.set(callContext.getParameter(0));
        return Value(static_cast<int32_t>(/* IGNORE */ 1));
    });

    auto callResult = emitRejectedPromise.value()->call(ValueFunctionFlagsCallSync, {Value(cb)});

    ASSERT_TRUE(callResult) << callResult.description();

    wrapper.flushQueues();

    auto result = resultHolder.get();

    ASSERT_TRUE(result.isError());
    ASSERT_EQ(STRING_LITERAL("This promise has failed"), result.getError().getMessage());
    ASSERT_TRUE(result.getError().getStack().contains("test/src/ErrorHandler"));
    // Should have no errors have we returned IGNORED
    ASSERT_EQ(static_cast<size_t>(0), messageHandler->messages().errors.size());
}

TEST_P(RuntimeFixture, supportsNotifyWithUncaughtErrorHandler) {
    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    wrapper.runtime->setRuntimeMessageHandler(messageHandler);

    auto emitUncaughtError =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nullptr, "test/src/ErrorHandler", "emitUncaughtError");
    ASSERT_TRUE(emitUncaughtError) << emitUncaughtError.description();

    SharedAtomic<Value> resultHolder;

    auto cb = makeShared<ValueFunctionWithCallable>([resultHolder](const auto& callContext) {
        resultHolder.set(callContext.getParameter(0));
        return Value(static_cast<int32_t>(/* NOTIFY */ 0));
    });

    auto callResult = emitUncaughtError.value()->call(ValueFunctionFlagsCallSync, {Value(cb)});

    ASSERT_TRUE(callResult) << callResult.description();

    wrapper.flushQueues();
    wrapper.flushQueues();

    auto result = resultHolder.get();
    // Should have an error since we returned NOTIFY
    ASSERT_EQ(static_cast<size_t>(1), messageHandler->messages().errors.size());
    // We should have gotten the error through the callback anyway
    ASSERT_TRUE(result.isError());
}

TEST_P(RuntimeFixture, supportsCustomColorPalette) {
    auto tree = wrapper.createViewNodeTreeAndContext(STRING_LITERAL("ColorPaletteTest@test/src/ColorPaletteTest"),
                                                     Value(makeShared<ValueMap>()),
                                                     Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("border", Value(ValueArray::make({Value(1.0), Value(65535)})))
                  .addChild(DummyView("SCValdiView")
                                .addAttribute("background",
                                              Value(ValueArray::make({
                                                  Value(ValueArray::make({Value(8388863)})),
                                                  Value(ValueArray::make({})),
                                                  Value(static_cast<int32_t>(0)),
                                                  Value(false),
                                              })))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canUpdateCustomColorPalette) {
    auto tree = wrapper.createViewNodeTreeAndContext(STRING_LITERAL("ColorPaletteTest@test/src/ColorPaletteTest"),
                                                     Value(makeShared<ValueMap>()),
                                                     Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    wrapper.runtime->getJavaScriptRuntime()->callComponentFunction(tree->getContext(),
                                                                   STRING_LITERAL("updateColorPalette"));

    wrapper.flushQueues();

    ASSERT_EQ(
        DummyView("SCValdiView")
            .addAttribute("border", Value(ValueArray::make({Value(1.0), Value(static_cast<int64_t>(4278190335))})))
            .addChild(DummyView("SCValdiView")
                          .addAttribute("background",
                                        Value(ValueArray::make({
                                            Value(ValueArray::make({Value(static_cast<int64_t>(4294902015))})),
                                            Value(ValueArray::make({})),
                                            Value(static_cast<int32_t>(0)),
                                            Value(false),
                                        })))),
        getRootView(tree));
}

static Result<Value> postprocessArrayValueToLength(ViewNode& viewNode, const Value& in) {
    if (in.getArray() == nullptr) {
        return Value(0.0);
    } else {
        return Value(static_cast<double>(in.getArray()->size()));
    }
}

TEST_P(RuntimeFixture, supportsPostprocessors) {
    auto& attributesManager = wrapper.standaloneRuntime->getViewManagerContext()->getAttributesManager();

    // We inject a postprocess that takes an input array and returns its size
    attributesManager.registerPostprocessor(attributesManager.getAttributeIds().getIdForName("border"),
                                            &postprocessArrayValueToLength);
    attributesManager.registerPostprocessor(attributesManager.getAttributeIds().getIdForName("background"),
                                            &postprocessArrayValueToLength);

    auto tree = wrapper.createViewNodeTreeAndContext(STRING_LITERAL("ColorPaletteTest@test/src/ColorPaletteTest"),
                                                     Value(makeShared<ValueMap>()),
                                                     Value::undefined());

    wrapper.waitUntilAllUpdatesCompleted();

    wrapper.flushQueues();

    ASSERT_EQ(DummyView("SCValdiView")
                  .addAttribute("border", Value(2.0))
                  .addChild(DummyView("SCValdiView").addAttribute("background", Value(4.0))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, supportsPostprocessorsOnCompositeAttribute) {
    auto& attributesManager = wrapper.standaloneRuntime->getViewManagerContext()->getAttributesManager();

    // Inject a postprocessor that resolves a known value for the rStyle attribute
    attributesManager.registerPostprocessor(attributesManager.getAttributeIds().getIdForName("rStyle"),
                                            [](auto& viewNode, const auto& value) -> Result<Value> {
                                                if (value.toStringBox() == "dotted") {
                                                    return Value(42.0);
                                                }

                                                return value;
                                            });

    auto viewModel = makeShared<ValueMap>();
    (*viewModel)[STRING_LITERAL("style")] = Value(STRING_LITERAL("dotted"));
    (*viewModel)[STRING_LITERAL("left")] = Value(1.0);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "CompositeAttributes", Value(viewModel));

    wrapper.waitUntilAllUpdatesCompleted();

    auto values = ValueArray::make(5);
    (*values)[0] = Value(1.0);
    (*values)[1] = Value(2.0);
    (*values)[2] = Value(3.0);
    (*values)[3] = Value(4.0);
    (*values)[4] = Value(42.0);

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("UIRectangleView")
                                .addAttribute("id", "container")
                                .addAttribute("rectangle", Valdi::Value(values))),
              getRootView(tree));
}

class TestAsset : public ObservableAsset {
public:
    TestAsset(int width, int height) : ObservableAsset(AssetKey(STRING_LITERAL("")), Weak<AssetsManager>()) {
        setExpectedSize(width, height);
    }
};

TEST_P(RuntimeFixture, canMeasureAssetsWithIdenticalAspectRatio) {
    TestAsset asset(32, 32);

    ASSERT_TRUE(asset.canBeMeasured());

    // Constraints equal to asset size
    ASSERT_EQ(32.0, asset.measureWidth(32.0, 32.0));
    ASSERT_EQ(32.0, asset.measureHeight(32.0, 32.0));

    // Contraints higher than asset size
    ASSERT_EQ(32.0, asset.measureWidth(50.0, 50.0));
    ASSERT_EQ(32.0, asset.measureHeight(50.0, 50.0));

    // Constraints smaller than asset size
    ASSERT_EQ(20.0, asset.measureWidth(20.0, 20.0));
    ASSERT_EQ(20.0, asset.measureHeight(20.0, 20.0));
}

TEST_P(RuntimeFixture, canMeasureAssetsWithMismatchingAspectRatio) {
    TestAsset asset(32, 32);

    ASSERT_TRUE(asset.canBeMeasured());

    // One constraint higher than the our asset size
    ASSERT_EQ(32.0, asset.measureWidth(32.0, 50.0));
    ASSERT_EQ(32.0, asset.measureHeight(32.0, 50.0));

    ASSERT_EQ(32.0, asset.measureWidth(50.0, 32.0));
    ASSERT_EQ(32.0, asset.measureHeight(50.0, 32.0));

    // One constraint higher, one constraint lower
    ASSERT_EQ(16.0, asset.measureWidth(16.0, 50.0));
    ASSERT_EQ(16.0, asset.measureHeight(16.0, 50.0));

    ASSERT_EQ(16.0, asset.measureWidth(50.0, 16.0));
    ASSERT_EQ(16.0, asset.measureHeight(50.0, 16.0));

    // Both constraints higher
    ASSERT_EQ(32.0, asset.measureWidth(50.0, 70.0));
    ASSERT_EQ(32.0, asset.measureHeight(50.0, 70.0));

    ASSERT_EQ(32.0, asset.measureWidth(70.0, 50.0));
    ASSERT_EQ(32.0, asset.measureHeight(70.0, 50.0));
}

TEST_P(RuntimeFixture, canMeasureAssetsWithUnboundedConstraints) {
    TestAsset asset(32, 32);

    ASSERT_TRUE(asset.canBeMeasured());

    // One constraint unbounded
    ASSERT_EQ(32.0, asset.measureWidth(32.0, -1.0));
    ASSERT_EQ(32.0, asset.measureHeight(32.0, -1.0));

    ASSERT_EQ(32.0, asset.measureWidth(-1.0, 32.0));
    ASSERT_EQ(32.0, asset.measureHeight(-1.0, 32.0));

    // One constraint unbounded, one constraint lower
    ASSERT_EQ(16.0, asset.measureWidth(16.0, -1.0));
    ASSERT_EQ(16.0, asset.measureHeight(16.0, -1.0));

    ASSERT_EQ(16.0, asset.measureWidth(-1.0, 16.0));
    ASSERT_EQ(16.0, asset.measureHeight(-1.0, 16.0));

    // Both constraints unbounded
    ASSERT_EQ(32.0, asset.measureWidth(-1.0, -1.0));
    ASSERT_EQ(32.0, asset.measureHeight(-1.0, -1.0));

    ASSERT_EQ(32.0, asset.measureWidth(-1.0, -1.0));
    ASSERT_EQ(32.0, asset.measureHeight(-1.0, -1.0));
}

TEST_P(RuntimeFixture, doesntMeasureAssetsWithEmptySize) {
    TestAsset asset(0, 0);

    ASSERT_FALSE(asset.canBeMeasured());
    ASSERT_EQ(0.0, asset.measureWidth(300, 300));
    ASSERT_EQ(0.0, asset.measureHeight(300, 300));
}

TEST_P(RuntimeFixture, assetCanResolveImageLoadSize) {
    TestAsset asset(100, 150);

    ASSERT_TRUE(asset.canBeMeasured());

    auto result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 50, 50, 2);

    ASSERT_EQ(100, result.first);
    ASSERT_EQ(150, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 200, 200, 2);

    ASSERT_EQ(400, result.first);
    ASSERT_EQ(600, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 100, 150, 3);

    ASSERT_EQ(300, result.first);
    ASSERT_EQ(450, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 100, 50, 3);

    ASSERT_EQ(300, result.first);
    ASSERT_EQ(450, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 2, 4, 3);

    ASSERT_EQ(8, result.first);
    ASSERT_EQ(12, result.second);
}

TEST_P(RuntimeFixture, remoteAssetCanResolveImageLoadSize) {
    TestAsset asset(0, 0);

    ASSERT_FALSE(asset.canBeMeasured());

    auto result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 50, 50, 2);

    ASSERT_EQ(100, result.first);
    ASSERT_EQ(100, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 200, 200, 2);

    ASSERT_EQ(400, result.first);
    ASSERT_EQ(400, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 100, 150, 3);

    ASSERT_EQ(300, result.first);
    ASSERT_EQ(450, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 100, 50, 3);

    ASSERT_EQ(300, result.first);
    ASSERT_EQ(150, result.second);

    result = ViewNodeAssetHandler::computeAssetSizeInPixelsForBounds(asset, 2, 4, 3);

    ASSERT_EQ(6, result.first);
    ASSERT_EQ(12, result.second);
}

TEST_P(RuntimeFixture, resolvesRuntimeTweakValues) {
    ASSERT_FALSE(wrapper.runtime->getResourceManager().enableDeferredGC());

    auto tweakModule = makeShared<TestTweakValueProvider>();
    tweakModule->config.setMapValue("VALDI_ENABLE_DEFERRED_GC", Valdi::Value(static_cast<bool>(true)));

    wrapper.runtime->setRuntimeTweaks(makeShared<ValdiRuntimeTweaks>(tweakModule.toShared()));

    // The module factory should have been recognized as a tweak value provider
    ASSERT_TRUE(wrapper.runtime->getResourceManager().enableDeferredGC());

    tweakModule->config.setMapValue("VALDI_ENABLE_DEFERRED_GC", Valdi::Value(static_cast<bool>(false)));

    ASSERT_FALSE(wrapper.runtime->getResourceManager().enableDeferredGC());
}

TEST(MainThreadManager, runOnIdleCallbacks) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());

    // Flush the initial scheduling
    mainQueue->runNextTask();

    int executeCount = 0;
    mainThreadManager->onIdle(makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext&) -> Value {
        executeCount++;
        return Value::undefined();
    }));

    ASSERT_EQ(0, executeCount);

    auto success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    ASSERT_EQ(1, executeCount);

    success = mainQueue->runNextTask();
    ASSERT_FALSE(success);
}

TEST(MainThreadManager, delayIdleCallbacksWhenMainThreadIsBusy) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());

    // Flush the initial scheduling
    mainQueue->runNextTask();

    int executeCount = 0;
    mainThreadManager->onIdle(makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext&) -> Value {
        executeCount++;
        return Value::undefined();
    }));

    mainThreadManager->dispatch(nullptr, []() {});

    ASSERT_EQ(0, executeCount);
    auto success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    // Execution should have been deferred for later
    ASSERT_EQ(0, executeCount);

    // Flush the scheduled task
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);
    ASSERT_EQ(0, executeCount);

    // Now flush the idle callback, we flush two because the first one should fail
    // since a task was run right before.
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);
    ASSERT_EQ(1, executeCount);

    success = mainQueue->runNextTask();
    ASSERT_FALSE(success);
}

TEST(MainThreadManager, runOnIdleCallbacksOneByOne) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());

    // Flush the initial scheduling
    mainQueue->runNextTask();

    int executeCount = 0;
    mainThreadManager->onIdle(makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext&) -> Value {
        executeCount++;
        return Value::undefined();
    }));

    int executeCount2 = 0;
    mainThreadManager->onIdle(makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext&) -> Value {
        executeCount2++;
        return Value::undefined();
    }));

    int executeCount3 = 0;
    mainThreadManager->onIdle(makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext&) -> Value {
        executeCount3++;
        return Value::undefined();
    }));

    auto success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    ASSERT_EQ(1, executeCount);
    ASSERT_EQ(0, executeCount2);
    ASSERT_EQ(0, executeCount3);

    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    ASSERT_EQ(1, executeCount);
    ASSERT_EQ(1, executeCount2);
    ASSERT_EQ(0, executeCount3);

    // Now we make the main thread busy

    mainThreadManager->dispatch(nullptr, []() {});

    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    // Last callback should not have been called

    ASSERT_EQ(1, executeCount);
    ASSERT_EQ(1, executeCount2);
    ASSERT_EQ(0, executeCount3);

    // Flush our main thread busy dispatch
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    // Run the last onIdle callback
    // we flush two because the first one should fail
    // since a task was run right before.
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    ASSERT_EQ(1, executeCount);
    ASSERT_EQ(1, executeCount2);
    ASSERT_EQ(1, executeCount3);

    success = mainQueue->runNextTask();
    ASSERT_FALSE(success);
}

TEST(MainThreadManager, doesntRunIdleTaskWhenThreadWasBusy) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());

    // Flush the initial scheduling
    mainQueue->runNextTask();

    // Make main thread busy
    mainThreadManager->dispatch(nullptr, []() {});

    int executeCount = 0;
    mainThreadManager->onIdle(makeShared<ValueFunctionWithCallable>([&](const ValueFunctionCallContext&) -> Value {
        executeCount++;
        return Value::undefined();
    }));

    // Run the task from the main thread
    auto success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    // Run the idle callback
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    // The callback should not have run because the main thread was busy
    ASSERT_EQ(0, executeCount);

    // It should have scheduled a new dispatch and try again
    success = mainQueue->runNextTask();
    ASSERT_TRUE(success);

    ASSERT_EQ(1, executeCount);
}

TEST(MainThreadManager, respectOrderingInBatch) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    auto backgroundThread = DispatchQueue::create(STRING_LITERAL("Background Thread"), ThreadQoSClassMax);

    // Flush the initial scheduling
    mainQueue->runNextTask();

    std::vector<int> calls;

    mainThreadManager->beginBatch();

    backgroundThread->sync([&]() { mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(1); }); });

    backgroundThread->sync([&]() {
        MainThreadBatchAllowScope batchAllowScope;
        mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(2); });
    });

    ASSERT_TRUE(calls.empty());

    mainThreadManager->endBatch();

    ASSERT_EQ(static_cast<size_t>(2), calls.size());

    ASSERT_EQ(std::vector<int>({1, 2}), calls);
}

TEST(MainThreadManager, respectOrderingInNestedBeginBatch) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    auto backgroundThread = DispatchQueue::create(STRING_LITERAL("Background Thread"), ThreadQoSClassMax);

    // Flush the initial scheduling
    mainQueue->runNextTask();

    std::vector<int> calls;

    mainThreadManager->beginBatch();

    backgroundThread->sync([&]() {
        MainThreadBatchAllowScope batchAllowScope;
        mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(1); });
    });

    mainThreadManager->beginBatch();

    backgroundThread->sync([&]() {
        MainThreadBatchAllowScope batchAllowScope;
        mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(2); });
    });

    ASSERT_TRUE(calls.empty());

    mainThreadManager->endBatch();

    ASSERT_TRUE(calls.empty());

    mainThreadManager->endBatch();

    ASSERT_EQ(static_cast<size_t>(2), calls.size());

    ASSERT_EQ(std::vector<int>({1, 2}), calls);
}

TEST(MainThreadManager, flushPreviousTasksWhenFlushingBatch) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    auto backgroundThread = DispatchQueue::create(STRING_LITERAL("Background Thread"), ThreadQoSClassMax);

    // Flush the initial scheduling
    mainQueue->runNextTask();

    std::vector<int> calls;

    // Schedule an async task

    mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(1); });

    mainThreadManager->beginBatch();

    backgroundThread->sync([&]() {
        MainThreadBatchAllowScope batchAllowScope;
        mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(2); });
    });

    ASSERT_TRUE(calls.empty());

    mainThreadManager->endBatch();

    ASSERT_EQ(static_cast<size_t>(2), calls.size());

    ASSERT_EQ(std::vector<int>({1, 2}), calls);
}

TEST(MainThreadManager, doesntFlushTasksWithoutBatchAllowScope) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    auto backgroundThread = DispatchQueue::create(STRING_LITERAL("Background Thread"), ThreadQoSClassMax);

    // Flush the initial scheduling
    mainQueue->runNextTask();

    std::vector<int> calls;

    mainThreadManager->beginBatch();

    backgroundThread->sync([&]() {
        MainThreadBatchAllowScope allowScope;
        mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(1); });
    });

    backgroundThread->sync([&]() {
        MainThreadBatchAllowScope allowScope;
        mainThreadManager->dispatch(nullptr, [&]() {
            // Schedule a nested begin batch while we are flushing the main thread
            mainThreadManager->beginBatch();
            MainThreadBatchAllowScope allowScope;
            mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(2); });
            mainThreadManager->endBatch();
        });
    });

    ASSERT_TRUE(calls.empty());

    mainThreadManager->endBatch();

    ASSERT_EQ(std::vector<int>({1}), calls);

    mainQueue->runNextTask();

    ASSERT_EQ(std::vector<int>({1, 2}), calls);
}

TEST(MainThreadManager, doesntAllowReentrantBatchesWhileFlushingLastBatch) {
    auto mainQueue = makeShared<StandaloneMainQueue>();
    auto mainThreadManager = makeShared<MainThreadManager>(mainQueue->createMainThreadDispatcher());
    auto backgroundThread = DispatchQueue::create(STRING_LITERAL("Background Thread"), ThreadQoSClassMax);

    // Flush the initial scheduling
    mainQueue->runNextTask();

    std::vector<int> calls;

    mainThreadManager->beginBatch();

    backgroundThread->sync([&]() { mainThreadManager->dispatch(nullptr, [&]() { calls.emplace_back(1); }); });

    ASSERT_TRUE(calls.empty());

    mainThreadManager->endBatch();

    ASSERT_TRUE(calls.empty());

    mainQueue->runNextTask();

    ASSERT_EQ(std::vector<int>({1}), calls);
}

TEST_P(RuntimeFixture, canWaitUntilAllUpdatesCompletedSyncInTheMainThread) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    tree->getContext()->waitUntilAllUpdatesCompletedSync(false);

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canWaitUntilAllUpdatesCompletedSyncInOffThread) {
    AsyncGroup group;
    group.enter();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    auto thread = std::thread([&]() {
        wrapper.flushJsQueue();

        tree->getContext()->waitUntilAllUpdatesCompletedSync(true);

        wrapper.flushJsQueue();

        group.leave();
    });

    group.blockingWait();
    thread.join();

    // View operations should have been enqueued into the main thread, but not yet processed
    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    // The tree itself should have been mutated however
    ASSERT_EQ(static_cast<size_t>(5), tree->getRootViewNode()->getRecursiveChildCount());

    // We should have 3 tasks that were enqueued into the main thread:
    // - The renderRequest should have been enqueued
    // - the deferred ViewTransaction
    // - the ViewNodeVisibilityObserver main thread acknowledger

    ASSERT_TRUE(wrapper.mainQueue->runNextTask());
    ASSERT_TRUE(wrapper.mainQueue->runNextTask());
    ASSERT_FALSE(wrapper.mainQueue->runNextTask());

    // After flushing the main thread we should have our UI
    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canInvokeContextObjectLockFromCallback) {
    AsyncGroup group;
    group.enter();

    auto thread = std::thread([&]() {
        RuntimeWrapper wrapper(getJsBridge(), getTSNMode());
        auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

        ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

        tree->getContext()->waitUntilAllUpdatesCompleted(
            makeShared<ValueFunctionWithCallable>([&](const auto& /*parameters*/) -> Value {
                auto lock = tree->getContext()->lock();
                group.leave();
                return Value();
            }));

        tree->getContext()->waitUntilAllUpdatesCompletedSync(false);
        tree = nullptr;
        wrapper.teardown();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    thread.join();
}

TEST_P(RuntimeFixture, enqueuedRenderRequestsAreSafeAndDoNothingAfterOnDestroy) {
    AsyncGroup groupStartWork;
    AsyncGroup groupWaitSetup;
    groupStartWork.enter();
    groupWaitSetup.enter();

    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    wrapper.flushJsQueue();

    auto threadDestroy = std::thread([&]() {
        groupStartWork.blockingWait();
        tree->getContext()->onDestroy();
        return;
    });

    auto threadProcess = std::thread([&]() {
        tree->getContext()->getRuntime()->getMainThreadManager().markCurrentThreadIsMainThread();
        groupWaitSetup.leave();
        groupStartWork.blockingWait();
        tree->getContext()->waitUntilAllUpdatesCompletedSync(false);
        return;
    });

    groupWaitSetup.blockingWait();
    groupStartWork.leave();

    threadDestroy.join();
    threadProcess.join();
}

TEST_P(RuntimeFixture, runningOnEmptyQueueIsSafe) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    wrapper.flushJsQueue();
    tree->getContext()->waitUntilAllUpdatesCompletedSync(false);

    ASSERT_FALSE(tree->getContext()->runRenderRequest(1));
}

TEST_P(RuntimeFixture, renderRequestsQueueBeforeDestroyAreSafeToCallAfter) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    wrapper.flushJsQueue();
    tree->getContext()->waitUntilAllUpdatesCompletedSync(false);

    auto id = tree->getContext()->enqueueRenderRequest(makeShared<RenderRequest>());
    ASSERT_TRUE(id.has_value());

    tree->getContext()->onDestroy();
    ASSERT_FALSE(tree->getContext()->runRenderRequest(id.value()));
}

TEST_P(RuntimeFixture, enqueueingRenderRequestAfterDestoyReturnNoValue) {
    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
    wrapper.flushJsQueue();
    tree->getContext()->waitUntilAllUpdatesCompletedSync(false);
    tree->getContext()->onDestroy();

    auto id = tree->getContext()->enqueueRenderRequest(makeShared<RenderRequest>());
    ASSERT_FALSE(id.has_value());
}

TEST_P(RuntimeFixture, destroyingObjectWithRenderRequestThatContainDisposableDoesNotDeadlock) {
    AsyncGroup group;
    group.enter();

    auto thread = std::thread([&]() {
        RuntimeWrapper wrapper(getJsBridge(), getTSNMode());
        auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");
        wrapper.flushJsQueue();
        tree->getContext()->waitUntilAllUpdatesCompletedSync(false);
        Context::setCurrent(tree->getContext());

        Ref<WrappedJSValueRef> ref;
        tree->getContext()->getRuntime()->getJavaScriptRuntime()->dispatchOnJsThread(
            tree->getContext(), JavaScriptTaskScheduleTypeAlwaysSync, 0, [&](JavaScriptEntryParameters& entry) {
                auto object = entry.jsContext.newObject(entry.exceptionTracker);
                ref = makeShared<WrappedJSValueRef>(
                    entry.jsContext, object.get(), ReferenceInfo(), entry.exceptionTracker);
            });

        auto renderRequest = makeShared<RenderRequest>();
        renderRequest->setFrameObserverCallback(Value(ref));
        tree->getContext()->enqueueRenderRequest(renderRequest);
        ref = nullptr;
        renderRequest = nullptr;

        tree->getContext()->onDestroy();
        group.leave();
        tree = nullptr;
        wrapper.teardown();
    });

    ASSERT_TRUE(group.blockingWaitWithTimeout(std::chrono::seconds(5)));
    thread.join();
}

struct ExitOnDestruct {
    Shared<std::atomic_bool> shouldContinue;

    ExitOnDestruct() : shouldContinue(makeShared<std::atomic_bool>(true)) {}
    ~ExitOnDestruct() {
        *shouldContinue = false;
    }
};

TEST_P(RuntimeFixture, canCaptureStackTraces) {
    ExitOnDestruct exitOnDestruct;
    auto callback = makeShared<ValueFunctionWithCallable>(
        [shouldContinue = exitOnDestruct.shouldContinue](const auto& callContext) -> Value {
            return Value(static_cast<bool>(shouldContinue->load()));
        });

    auto function =
        getJsModulePropertyAsUntypedFunction(wrapper.runtime, nullptr, "test/src/InfiniteLoop", "callIndefinitely");
    ASSERT_TRUE(function) << function.description();
    function.value()->call(ValueFunctionFlagsNeverCallSync, {Value(callback)}).value();

    auto output = wrapper.runtime->getJavaScriptRuntime()->captureStackTraces(std::chrono::seconds(5));
    (*exitOnDestruct.shouldContinue) = false;

    ASSERT_EQ(static_cast<size_t>(1), output.size());

    ASSERT_EQ(JavaScriptCapturedStacktrace::Status::RUNNING, output[0].getStatus());
    ASSERT_TRUE(output[0].getStackTrace().contains("test/src/InfiniteLoop"));

    wrapper.flushJsQueue();

    output = wrapper.runtime->getJavaScriptRuntime()->captureStackTraces(std::chrono::seconds(5));

    ASSERT_EQ(static_cast<size_t>(1), output.size());
    ASSERT_EQ(JavaScriptCapturedStacktrace::Status::NOT_RUNNING, output[0].getStatus());
}

static std::atomic_int myAttributionFnCallCount = 0;
static const void* myAttributionFn(const Valdi::AttributionFunctionCallback& fn) {
    myAttributionFnCallCount++;
    fn();
    return nullptr;
}

TEST_P(RuntimeFixture, resolvesAttribution) {
    Valdi::AttributionResolver::MappingEntry mapping = {"test", "someOwner", &myAttributionFn};
    auto attributionResolver = makeShared<AttributionResolver>(&mapping, 1, wrapper.runtimeManager->getLogger());

    wrapper.runtimeManager->setAttributionResolver(attributionResolver);

    auto tree = wrapper.createViewNodeTreeAndContext("test", "BasicViewTree");

    auto attribution = tree->getContext()->getAttribution();

    ASSERT_EQ(&myAttributionFn, attribution.fn);
    ASSERT_EQ(STRING_LITERAL("test"), attribution.moduleName);
    ASSERT_EQ(STRING_LITERAL("someOwner"), attribution.owner);

    auto callCountBefore = myAttributionFnCallCount.load();
    wrapper.runtime->getJavaScriptRuntime()->dispatchOnJsThreadSync(tree->getContext(), [](const auto& /*jsEntry*/) {});

    // When scheduling on JS thread, it could use our attribution function
    ASSERT_GT(myAttributionFnCallCount.load(), callCountBefore);
}

TEST_P(RuntimeFixture, supportsValdiStyleModuleLoading) {
    wrapper.teardown();

    auto tweakModule = makeShared<TestTweakValueProvider>().toShared();
    tweakModule->config.setMapValue("VALDI_ENABLE_COMMONJS_MODULE_LOADER", Valdi::Value(static_cast<bool>(false)));

    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), false, tweakModule);

    auto jsResult = callFunctionSync(wrapper, "test/src/ModuleLoaderTest", "getImportedType", {});

    ASSERT_TRUE(jsResult) << jsResult.value();

    ASSERT_EQ(Value(STRING_LITERAL("object")), jsResult.value());

    auto scopedJsResult = callFunctionSync(wrapper, "test/src/ModuleLoaderTest", "getScopedImportedType", {});

    ASSERT_TRUE(scopedJsResult) << scopedJsResult.value();

    ASSERT_EQ(Value(STRING_LITERAL("object")), scopedJsResult.value());
}

TEST_P(RuntimeFixture, supportsCommonJsStyleModuleLoading) {
    wrapper.teardown();

    auto tweakModule = makeShared<TestTweakValueProvider>().toShared();
    tweakModule->config.setMapValue("VALDI_ENABLE_COMMONJS_MODULE_LOADER", Valdi::Value(static_cast<bool>(true)));

    wrapper = RuntimeWrapper(getJsBridge(), getTSNMode(), false, tweakModule);

    auto jsResult = callFunctionSync(wrapper, "test/src/ModuleLoaderTest", "getImportedType", {});

    ASSERT_TRUE(jsResult) << jsResult.value();

    ASSERT_EQ(Value(STRING_LITERAL("number")), jsResult.value());

    auto scopedJsResult = callFunctionSync(wrapper, "test/src/ModuleLoaderTest", "getScopedImportedType", {});

    ASSERT_TRUE(scopedJsResult) << scopedJsResult.value();

    ASSERT_EQ(Value(STRING_LITERAL("number")), scopedJsResult.value());
}

TEST_P(RuntimeFixture, canGetFileEntry) {
    auto jsResult = callFunctionSync(wrapper, "test/src/LoadFile", "loadFromString", {});
    ASSERT_TRUE(jsResult) << jsResult.value();

    ASSERT_EQ(Value().setMapValue("success", Value(STRING_LITERAL("yeah"))), jsResult.value());

    jsResult = callFunctionSync(wrapper, "test/src/LoadFile", "loadFromBytes", {});
    ASSERT_TRUE(jsResult) << jsResult.value();

    ASSERT_EQ(Value().setMapValue("success", Value(STRING_LITERAL("yeah"))), jsResult.value());
}

TEST_P(RuntimeFixture, supportsMicrotasks) {
    SharedAtomic<bool> setTimeoutCallbackCalled;
    SharedAtomic<bool> microTaskCallbackCalled;

    setTimeoutCallbackCalled.set(false);
    microTaskCallbackCalled.set(false);

    SharedAtomic<bool> setTimeoutCallbackCalledBeforeReturnSync;
    SharedAtomic<bool> microTaskCallbackCalledBeforeReturnSync;

    // We lock the JS thread without enqueueing a task, so that we can check that
    // our microtask was called by the time "callFunctionSync" ends, which should
    // enter the VM and leave it and flush the microtasks at that point.
    wrapper.runtime->getJavaScriptRuntime()->getJsDispatchQueue()->sync([&]() {
        auto setTimeoutCallback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
            setTimeoutCallbackCalled.set(true);
            return Value::undefined();
        });

        auto microtaskCallback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
            microTaskCallbackCalled.set(true);
            return Value::undefined();
        });

        callFunctionSync(
            wrapper, "test/src/Microtask", "testMicrotask", {Value(setTimeoutCallback), Value(microtaskCallback)});

        setTimeoutCallbackCalledBeforeReturnSync.set(setTimeoutCallbackCalled.get());
        microTaskCallbackCalledBeforeReturnSync.set(microTaskCallbackCalled.get());
    });

    ASSERT_FALSE(setTimeoutCallbackCalledBeforeReturnSync.get());
    ASSERT_TRUE(microTaskCallbackCalledBeforeReturnSync.get());

    wrapper.flushJsQueue();

    // After flushing the queue, our setTimeout calback should have been called.
    ASSERT_TRUE(setTimeoutCallbackCalled.get());
    ASSERT_TRUE(microTaskCallbackCalled.get());
}

TEST_P(RuntimeFixture, promiseUseMicrotaskOnFulfillment) {
    SharedAtomic<bool> onResolvedCalledBeforeReturnSync;

    // We lock the JS thread without enqueueing a task, so that we can check that
    // our microtask was called by the time "callFunctionSync" ends, which should
    // enter the VM and leave it and flush the microtasks at that point.
    wrapper.runtime->getJavaScriptRuntime()->getJsDispatchQueue()->sync([&]() {
        SharedAtomic<bool> onResolvedCalled;
        onResolvedCalled.set(false);

        auto onResolvedCallback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
            onResolvedCalled.set(true);
            return Value::undefined();
        });

        callFunctionSync(wrapper, "test/src/Microtask", "testPromiseFulfillment", {Value(onResolvedCallback)});

        onResolvedCalledBeforeReturnSync.set(onResolvedCalled.get());
    });

    wrapper.flushJsQueue();

    ASSERT_TRUE(onResolvedCalledBeforeReturnSync.get());
}

TEST_P(RuntimeFixture, canDestroyJsContextWithDanglingJsFunction) {
    Result<Value> getCallbackFn;
    {
        auto* jsBridge = getJsBridge();
        auto childWrapper = RuntimeWrapper(jsBridge, getTSNMode());
        getCallbackFn =
            getJsModulePropertyWithSchema(childWrapper.runtime, "test/src/SingleCall", "getCallback", "f():f!():s");

        ASSERT_TRUE(getCallbackFn) << getCallbackFn.description();
        childWrapper.teardown();
    }

    auto singleCallbackResult = getCallbackFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_TRUE(singleCallbackResult) << singleCallbackResult.description();
    ASSERT_EQ(ValueType::Undefined, singleCallbackResult.value().getType());
}

TEST_P(RuntimeFixture, providesPerformanceTimeOrigin) {
    auto performanceTimeOrigin = wrapper.runtime->getJavaScriptRuntime()->getPerformanceTimeOrigin();
    auto now = TimePoint::now();
    ASSERT_TRUE(now > performanceTimeOrigin);
    ASSERT_TRUE(performanceTimeOrigin.getTime() > 0);

    std::string evalBody = "return performance.timeOrigin;";

    auto evalResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(evalBody)->toBytesView(), STRING_LITERAL("eval.js"));

    ASSERT_TRUE(evalResult) << evalResult.description();

    auto timeOriginSeconds = evalResult.value().toDouble();

    ASSERT_EQ(performanceTimeOrigin.getTimeMs(), TimePoint::fromSeconds(timeOriginSeconds).getTimeMs());
}

TEST_P(RuntimeFixture, supportsTypeConverters) {
    // Register the type converter so that a MyValueObject reference with converted type hint
    // will be automatically converted.
    wrapper.runtime->registerTypeConverter(STRING_LITERAL("MyValueObject"),
                                           STRING_LITERAL("makeConverter@test/src/TypeConverterTest"));

    auto registry = makeShared<ValueSchemaRegistry>();
    auto clsSchema = ValueSchema::cls(
        STRING_LITERAL("MyValueObject"), false, {ClassPropertySchema(STRING_LITERAL("value"), ValueSchema::string())});
    registry->registerSchema(clsSchema);

    auto addFn = getJsModulePropertyWithSchema(
        wrapper.runtime, nullptr, registry.get(), "test/src/TypeConverterTest", "add", "f(r<c>:'MyValueObject'):d");
    ASSERT_TRUE(addFn) << addFn.description();

    // Call the add function. The object should be converted using the registered TypeConverter,
    // it should then call the add() function with the converted type and return the result
    auto callResult = addFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError,
                                                        {Value(ValueTypedObject::make(clsSchema.getClassRef(),
                                                                                      {
                                                                                          Value(STRING_LITERAL("11 7")),
                                                                                      }))});

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(Value(18.0), callResult.value());

    // Now check that the conversion on the other side works

    auto makeObjectFn = getJsModulePropertyWithSchema(wrapper.runtime,
                                                      nullptr,
                                                      registry.get(),
                                                      "test/src/TypeConverterTest",
                                                      "makeObject",
                                                      "f(d, d): r<c>:'MyValueObject'");
    ASSERT_TRUE(makeObjectFn) << makeObjectFn.description();

    callResult = makeObjectFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {Value(7.0), Value(4.0)});

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(ValueType::TypedObject, callResult.value().getType());

    auto* typedObject = callResult.value().getTypedObject();

    // TypedObject should have the MyValueObject class
    ASSERT_EQ(clsSchema.getClass(), typedObject->getSchema().get());

    ASSERT_EQ(std::string("7 4"), typedObject->getProperty(0).toString());
}

TEST_P(RuntimeFixture, canDetectANR) {
    auto simulateANRFn =
        getJsModulePropertyWithSchema(wrapper.runtime, "test/src/ANRDetection", "simulateANR", "f(f():b)");
    ASSERT_TRUE(simulateANRFn) << simulateANRFn.description();

    auto anrListener = makeShared<TestANRDetectorListener>();
    wrapper.runtimeManager->getANRDetector()->setListener(anrListener);
    wrapper.runtimeManager->getANRDetector()->setTickInterval(std::chrono::milliseconds(5));

    auto group = makeShared<AsyncGroup>();
    Shared<std::atomic_bool> didNotifyGroup = makeShared<std::atomic_bool>(false);
    Shared<std::atomic_bool> shouldContinue = makeShared<std::atomic_bool>(true);

    group->enter();
    auto callback = makeShared<ValueFunctionWithCallable>([=](const auto& callContext) -> Value {
        if (!*didNotifyGroup) {
            *didNotifyGroup = true;
            group->leave();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return Value(shouldContinue->load());
    });

    wrapper.flushQueues();

    auto tree = wrapper.createViewNodeTreeAndContext(STRING_LITERAL("ANRDetection@test/src/ANRDetection"),
                                                     Value().setMapValue("shouldContinue", Value(callback)),
                                                     Value());

    // We wait to start the ANR detector until the TS code started calling into our callbacks
    ASSERT_TRUE(group->blockingWaitWithTimeout(std::chrono::seconds(5)));

    wrapper.runtimeManager->getANRDetector()->start(std::chrono::milliseconds(10));

    auto anr = anrListener->waitForANR();
    *shouldContinue = false;

    ASSERT_EQ("Detected ANR in 'test' after 10.0 ms", anr.getMessage());
    ASSERT_EQ(static_cast<size_t>(1), anr.getCapturedStacktraces().size());

    const auto& stacktrace = anr.getCapturedStacktraces()[0].getStackTrace();

    ASSERT_TRUE(stacktrace.contains("at onCreate"));
    ASSERT_TRUE(stacktrace.contains("test/src/ANRDetection"));
}

TEST_P(RuntimeFixture, cancelsComponentCreationIfDestroyIsCalledBeforeComponentIsCreated) {
    std::atomic_bool onRenderCalled = false;

    // Lock the JS thread, so that we can destroy the context before the onRender processes
    auto group = makeShared<AsyncGroup>();

    group->enter();

    wrapper.runtime->getJavaScriptRuntime()->dispatchOnJsThreadAsync(
        nullptr, [group](JavaScriptEntryParameters& entry) { group->blockingWait(); });

    auto callback = makeShared<ValueFunctionWithCallable>([&](const auto& callContext) -> Value {
        onRenderCalled = true;
        return Value::undefined();
    });

    auto tree =
        wrapper.createViewNodeTreeAndContext(STRING_LITERAL("CallCallbackOnRender@test/src/CallCallbackOnRender"),
                                             Value().setMapValue("onRender", Value(callback)),
                                             Value());
    wrapper.runtime->destroyContext(tree->getContext());

    // Release the JS thread
    group->leave();
    wrapper.flushQueues();

    ASSERT_FALSE(onRenderCalled);
}

TEST_P(RuntimeFixture, canRenderContextOutsideOfMainThread) {
    auto& runtime = *wrapper.runtime;

    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    runtime.setRuntimeMessageHandler(messageHandler);

    auto context = runtime.getContextManager().createContext(
        runtime.getJavaScriptRuntime()->getContextHandler(),
        wrapper.standaloneRuntime->getViewManagerContext(),
        ComponentPath::parse(STRING_LITERAL("ComponentClass@test/src/BasicViewTree.vue.generated")),
        nullptr,
        nullptr,
        true,
        false);

    auto tree = runtime.createViewNodeTree(context, ViewNodeTreeThreadAffinity::ANY);
    tree->setRootViewWithDefaultViewClass();

    auto thread = std::thread([&]() { context->onCreate(); });
    thread.join();

    ASSERT_TRUE(messageHandler->messages().errors.empty());
    ASSERT_FALSE(context->hasPendingRenderRequests());

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, canDeferRenderContextOutsideOfMainThread) {
    auto& runtime = *wrapper.runtime;

    wrapper.standaloneRuntime->getViewManager().setAlwaysRenderInMainThread(true);

    auto messageHandler = makeShared<MockRuntimeMessageHandler>();
    runtime.setRuntimeMessageHandler(messageHandler);

    auto context = runtime.getContextManager().createContext(
        runtime.getJavaScriptRuntime()->getContextHandler(),
        wrapper.standaloneRuntime->getViewManagerContext(),
        ComponentPath::parse(STRING_LITERAL("ComponentClass@test/src/BasicViewTree.vue.generated")),
        nullptr,
        nullptr,
        true,
        false);

    auto tree = runtime.createViewNodeTree(context, ViewNodeTreeThreadAffinity::ANY);
    tree->setRootViewWithDefaultViewClass();

    auto thread = std::thread([&]() { context->onCreate(); });
    thread.join();

    ASSERT_TRUE(messageHandler->messages().errors.empty());

    ASSERT_EQ(DummyView("SCValdiView"), getRootView(tree));

    ASSERT_FALSE(context->hasPendingRenderRequests());

    ASSERT_TRUE(wrapper.mainQueue->runNextTask());

    ASSERT_EQ(DummyView("SCValdiView")
                  .addChild(DummyView("SCValdiLabel"))
                  .addChild(DummyView("SCValdiView")
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))
                                .addChild(DummyView("UIButton"))),
              getRootView(tree));
}

TEST_P(RuntimeFixture, supportsTSN) {
    if (!isWithTSN()) {
        return;
    }

    registerTSNTestModule("test/tsn");

    auto sayHelloFn = getJsModulePropertyWithSchema(wrapper.runtime, "test/tsn", "hello", "f():s");

    ASSERT_TRUE(sayHelloFn) << sayHelloFn.description();

    auto callResult = sayHelloFn.value().getFunction()->call(ValueFunctionFlagsPropagatesError, {});

    ASSERT_TRUE(callResult) << callResult.description();

    ASSERT_EQ(Value("Hello from Native!").toStringBox(), callResult.value().toStringBox());
}

class TestModuleFactory : public snap::valdi_core::ModuleFactory {
public:
    TestModuleFactory() = default;
    ~TestModuleFactory() override = default;

    StringBox getModulePath() final {
        return STRING_LITERAL("my_module/src/TestModule");
    }

    Value loadModule() final {
        return Value().setMapValue("CONSTANT", Value(21));
    }
};

class TestModule2Factory : public snap::valdi_core::ModuleFactory {
public:
    TestModule2Factory() = default;
    ~TestModule2Factory() override = default;

    StringBox getModulePath() final {
        return STRING_LITERAL("my_module/src/TestModule2");
    }

    Value loadModule() final {
        return Value().setMapValue("CONSTANT", Value(7));
    }
};

class NativeCalculator : public snap::valdi_modules::test::ICalculator {
public:
    NativeCalculator() = default;
    ~NativeCalculator() override = default;

    void add(double value) final {
        _value += value;
    }

    void sub(double value) final {
        _value -= value;
    }

    void mul(double value) final {
        _value *= value;
    }

    void div(double value) final {
        _value /= value;
    }

    double total() final {
        return _value;
    }

    Valdi::StringBox toString(snap::valdi_modules::test::CalculatorToStringFormat format) final {
        switch (format) {
            case snap::valdi_modules::test::CalculatorToStringFormat::DECIMAL:
                return Valdi::StringBox::fromString(std::to_string(_value));
            case snap::valdi_modules::test::CalculatorToStringFormat::INTEGER:
                return Valdi::StringBox::fromString(std::to_string(static_cast<int64_t>(_value)));
        }
    }

private:
    double _value = 0;
};

class NativeCalculatorModuleFactoryImpl : public snap::valdi_modules::test::NativeCalculatorModuleFactory {
public:
    NativeCalculatorModuleFactoryImpl() = default;

    Ref<snap::valdi_modules::test::NativeCalculatorModule> onLoadModule() final {
        class NativeCalculatorModuleImpl : public snap::valdi_modules::test::NativeCalculatorModule {
        public:
            NativeCalculatorModuleImpl() = default;
            ~NativeCalculatorModuleImpl() override = default;

            Ref<snap::valdi_modules::test::ICalculator> makeCalculator() final {
                return makeShared<NativeCalculator>();
            }
        };

        return makeShared<NativeCalculatorModuleImpl>();
    }
};
auto registerNativeCalculatorModule = RegisterModuleFactory::registerTyped<NativeCalculatorModuleFactoryImpl>();

auto registerTestModule = RegisterModuleFactory::registerTyped<TestModuleFactory>();
auto registerTestModule2 = RegisterModuleFactory::registerTyped<TestModule2Factory>();

TEST_P(RuntimeFixture, supportsModuleRegisteredThroughModuleRegistry) {
    std::string evalBody = "return global.require(\"my_module/src/TestModule\").CONSTANT * 2;";

    auto evalResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(evalBody)->toBytesView(), STRING_LITERAL("eval.js"));

    ASSERT_TRUE(evalResult) << evalResult.description();

    ASSERT_EQ(42, evalResult.value().toInt());

    evalBody = "return global.require(\"my_module/src/TestModule2\").CONSTANT;";

    evalResult = wrapper.runtime->getJavaScriptRuntime()->evaluateScript(
        makeShared<ByteBuffer>(evalBody)->toBytesView(), STRING_LITERAL("eval.js"));

    ASSERT_TRUE(evalResult) << evalResult.description();

    ASSERT_EQ(7, evalResult.value().toInt());
}

INSTANTIATE_TEST_SUITE_P(RuntimeTests,
                         RuntimeFixture,
                         ::testing::Values(JavaScriptEngineTestCase::Hermes,
                                           JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::QuickJSWithTSN,
                                           JavaScriptEngineTestCase::JSCore),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
