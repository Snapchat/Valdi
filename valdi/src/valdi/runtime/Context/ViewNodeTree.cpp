//
//  ViewNodesContext.cpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Attributes/AttributesManager.hpp"
#include "valdi/runtime/Attributes/NoOpDefaultAttributeHandler.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/ContextEntry.hpp"
#include "valdi/runtime/Context/IViewNodesAssetTracker.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNodePath.hpp"
#include "valdi/runtime/Context/ViewNodesFrameObserver.hpp"
#include "valdi/runtime/Context/ViewNodesVisibilityObserver.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Resources/AssetsManager.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/ValdiBuildFlags.hpp"
#include "valdi/runtime/Views/GlobalViewFactories.hpp"
#include "valdi/runtime/Views/MeasureDelegate.hpp"
#include "valdi/runtime/Views/ViewTransactionScope.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"

#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include "valdi_core/cpp/Utils/ContainerUtils.hpp"
#include <cmath>
#include <yoga/YGNode.h>

// Flush the runTreeUpdates() throughput metric at most every 32ms
static constexpr size_t kUpdateRunTreeWindowDurationsMs = 32;
static constexpr size_t kUpdateRunTreeMinPercentage = 5;

namespace Valdi {

class ParentAttributeOwner : public AttributeOwner {
public:
    int getAttributePriority(AttributeId /*id*/) const override {
        return kAttributeOwnerPriorityParentOverriden;
    }

    StringBox getAttributeSource(AttributeId /*id*/) const override {
        static auto kSource = STRING_LITERAL("parent");

        return kSource;
    }
};

ViewNodeTree::ViewNodeTree(const SharedContext& context,
                           const Ref<ViewManagerContext>& viewManagerContext,
                           IViewManager* viewManager,
                           std::shared_ptr<Runtime> runtime,
                           MainThreadManager* mainThreadManager,
                           bool shouldRenderInMainThread)
    : _context(context),
      _viewManagerContext(viewManagerContext),
      _viewManager(viewManager),
      _runtime(std::move(runtime)),
      _mainThreadManager(mainThreadManager),
      _shouldRenderInMainThread(shouldRenderInMainThread) {}

ViewNodeTree::~ViewNodeTree() {
    clear();
}

void ViewNodeTree::clear() {
    flushRunUpdatesInnerStatsIfNeeded();
    clearRunUpdatesMetricsSession();
    _runtime = nullptr;
    _viewFactories.clear();
    _updateFunctions.clear();
}

void ViewNodeTree::clearRunUpdatesMetricsSession() {
    _runUpdatesInnerSessionStart = std::nullopt;
    _runUpdatesInnerSessionStop = std::nullopt;
    _runUpdatesInnerAccumulatedTime = std::chrono::steady_clock::duration(0);
}

Ref<View> ViewNodeTree::getViewForNodePath(const ViewNodePath& nodePath) const {
    auto viewNode = getViewNodeForNodePath(nodePath);
    if (viewNode == nullptr) {
        return nullptr;
    }

    return viewNode->getViewAndDisablePooling();
}

Ref<ViewNode> ViewNodeTree::getViewNodeForNodePath(const ViewNodePath& nodePath) const {
    auto current = getRootViewNode();
    if (current == nullptr) {
        return nullptr;
    }

    std::vector<SharedViewNode> nodes;

    for (const auto& entry : nodePath.getEntries()) {
        current->findAllNodesWithId(entry.nodeId, nodes);

        if (nodes.empty()) {
            return nullptr;
        }

        auto itemIndex = static_cast<size_t>(entry.itemIndex);
        if (itemIndex >= nodes.size()) {
            return nullptr;
        }

        current = nodes[itemIndex];
        nodes.clear();
    }

    return current;
}

const Ref<ViewNode>& ViewNodeTree::getRootViewNode() const {
    return _rootViewNode;
}

Ref<View> ViewNodeTree::getRootView() const {
    if (_rootView != nullptr) {
        return _rootView;
    }

    if (_rootViewNode == nullptr) {
        return nullptr;
    }

    return _rootViewNode->getViewAndDisablePooling();
}

void ViewNodeTree::setRootViewWithDefaultViewClass() {
    SC_ASSERT_NOTNULL(_viewManager);
    auto defaultViewFactory = getOrCreateViewFactory(_viewManager->getDefaultViewClass());
    auto rootView = defaultViewFactory->createView(this, _rootViewNode.get(), true);
    setRootView(rootView);
}

void ViewNodeTree::setRootView(const Ref<View>& view) {
    withLock([&]() {
        if (_rootView != view) {
            _rootView = view;

            auto disableUpdate = beginDisableUpdates();

            if (_rootViewNode != nullptr) {
                _rootViewNode->removeView(getCurrentViewTransactionScope());
            }

            if (_rootView != nullptr) {
                _rootView->setCanBeReused(false);
            }

            setViewToRootViewNode(_rootViewNode, _rootView);

            if (_rootView == nullptr) {
                if (_rootViewNode != nullptr) {
                    // Force an immediate view tree update when clearing the root view to remove all the views
                    _rootViewNode->updateViewTree(getCurrentViewTransactionScope());
                }
            } else {
                if (_layoutSpecsDirty) {
                    getCurrentViewTransactionScope().transaction().invalidateViewLayout(_rootView);
                }
            }
        }
    });
}

void ViewNodeTree::registerLocalViewFactory(const StringBox& viewClassName, const Ref<ViewFactory>& viewFactory) {
    _viewFactories[viewClassName] = getCompatibleViewFactory(viewFactory);
}

Ref<ViewFactory> ViewNodeTree::getCompatibleViewFactory(const Ref<ViewFactory>& viewFactory) const {
    if (viewFactory == nullptr) {
        return nullptr;
    }
    const auto& viewManagerContext = getViewManagerContext();

    if (&viewFactory->getViewManager() == &viewManagerContext->getViewManager()) {
        return viewFactory;
    }

    auto defaultAttributes = viewManagerContext->getAttributesManager().getAttributesForClass(
        viewManagerContext->getViewManager().getDefaultViewClass());

    return viewManagerContext->getViewManager().bridgeViewFactory(viewFactory, defaultAttributes);
}

const FlatMap<StringBox, Ref<ViewFactory>>& ViewNodeTree::getViewFactories() const {
    return _viewFactories;
}

Ref<ViewFactory> ViewNodeTree::getViewFactory(const StringBox& viewClassName) const {
    const auto& it = _viewFactories.find(viewClassName);
    if (it == _viewFactories.end()) {
        return nullptr;
    }

    return it->second;
}

Ref<ViewFactory> ViewNodeTree::getOrCreateViewFactory(const StringBox& viewClassName) {
    auto viewFactory = getViewFactory(viewClassName);
    if (viewFactory != nullptr) {
        return viewFactory;
    }

    if (viewClassName == AttributesManager::getDeferredClassName()) {
        viewFactory = createDeferredViewFactory();
    } else {
        viewFactory =
            getCompatibleViewFactory(getViewManagerContext()->getGlobalViewFactories()->getViewFactory(viewClassName));
    }

    // Register it in our ViewNodeTree to avoid the double lookup next time
    registerLocalViewFactory(viewClassName, viewFactory);

    return viewFactory;
}

Ref<ViewFactory> ViewNodeTree::createDeferredViewFactory() {
    // For the deferred view factory, we create a ViewFactory that conforms to the default view class,
    // but we pass it a no-op default attribute handler to skip attributes validation when they are set.
    // This is done because attributes might be set on the view node before the view class or the viewFactory
    // is provided.

    SC_ASSERT_NOTNULL(_viewManager);
    auto& viewManager = *_viewManager;
    auto defaultViewClass = viewManager.getDefaultViewClass();
    auto defaultViewFactory = getOrCreateViewFactory(defaultViewClass);

    AttributeHandlerById attributes;
    auto defaultHandler = makeShared<NoOpDefaultAttributeHandler>();

    auto modifiedAttributes = defaultViewFactory->getBoundAttributes()->merge(
        defaultViewClass, attributes, defaultHandler, nullptr, false, false);
    return viewManager.createViewFactory(defaultViewClass, modifiedAttributes);
}

void ViewNodeTree::performUpdates() {
    // This does the following:
    // - calculate the layout if needed
    // - determine yoga node visibility
    // - rerender every contexts which has one node that came
    // on or off screen
    // We keep doing that until we haven't found any more contexts to update.
    // To avoid potential infinite recursion, we re-render a given context only once.

    if (!_hasLayoutSpecs) {
        return;
    }

    auto rootViewNode = getRootViewNode();
    if (rootViewNode == nullptr) {
        return;
    }

    if (_layoutDirty) {
        _layoutDirty = false;
        rootViewNode->performLayout(getCurrentViewTransactionScope(), _layoutSize, _layoutDirection);
    }

    flushOnLayoutCallbacks();

    rootViewNode->updateVisibilityAndPerformUpdates(getCurrentViewTransactionScope());
}

Size ViewNodeTree::measureLayout(
    float width, MeasureMode widthMode, float height, MeasureMode heightMode, LayoutDirection layoutDirection) {
    Size measuredSize;
    withLock([&]() {
        if (_rootViewNode == nullptr) {
            measuredSize = Size(0, 0);
        } else {
            measuredSize = _rootViewNode->measureLayout(width, widthMode, height, heightMode, layoutDirection);
        }
    });

    return measuredSize;
}

bool ViewNodeTree::isLayoutSpecsDirty() const {
    return _layoutSpecsDirty;
}

void ViewNodeTree::setRetainsLayoutSpecsOnInvalidateLayout(bool retainsLayoutSpecsOnInvalidateLayout) {
    auto lockGuard = lock();
    _retainsLayoutSpecsOnInvalidateLayout = retainsLayoutSpecsOnInvalidateLayout;
}

bool ViewNodeTree::retainsLayoutSpecsOnInvalidateLayout() const {
    auto lockGuard = lock();
    return _retainsLayoutSpecsOnInvalidateLayout;
}

void ViewNodeTree::setLayoutSpecs(Size layoutSize, LayoutDirection layoutDirection) {
    auto lockGuard = lock();
    _hasLayoutSpecs = true;
    _layoutSpecsDirty = false;

    if (_layoutSize != layoutSize) {
        _layoutSize = layoutSize;
        _layoutDirty = true;
    }

    if (_layoutDirection != layoutDirection) {
        _layoutDirection = layoutDirection;
        _layoutDirty = true;
    }

#if VALDI_DEBUG_TREE_UPDATES
    scheduleExclusiveUpdate([this]() { this->performUpdates(); }, DispatchFunction(), "setLayoutSpecs");
#else
    scheduleExclusiveUpdate([this]() { this->performUpdates(); }, DispatchFunction());
#endif
}

void ViewNodeTree::setViewport(std::optional<Frame> viewport) {
    auto lockGuard = lock();
    if (_viewport != viewport) {
        _viewport = viewport;
        schedulePerformUpdates();
    }
}

std::optional<Frame> ViewNodeTree::getViewport() const {
    auto lockGuard = lock();
    return _viewport;
}

void ViewNodeTree::onLayoutDirty() {
    _layoutDirty = true;
    _layoutDirtyCounter++;

    if (!_layoutSpecsDirty && !_retainsLayoutSpecsOnInvalidateLayout) {
        _layoutSpecsDirty = true;

        if (_rootView != nullptr) {
            getCurrentViewTransactionScope().transaction().invalidateViewLayout(_rootView);
        }
    }
}

void ViewNodeTree::onRootViewNodeNeedsUpdate() {
    schedulePerformUpdates();
}

void ViewNodeTree::schedulePerformUpdates() {
    if (!_scheduledPerformUpdates) {
        _scheduledPerformUpdates = true;
#if VALDI_DEBUG_TREE_UPDATES
        scheduleExclusiveUpdate(
            [this]() { this->performUpdatesIfLayoutSpecsUpToDate(); }, DispatchFunction(), "root_needs_update");
#else
        scheduleExclusiveUpdate([this]() { this->performUpdatesIfLayoutSpecsUpToDate(); }, DispatchFunction());
#endif
    }
}

void ViewNodeTree::performUpdatesIfLayoutSpecsUpToDate() {
    _scheduledPerformUpdates = false;

    if (_layoutSpecsDirty) {
        return;
    }

    performUpdates();
}

void ViewNodeTree::scheduleReapplyAttributesRecursive(const std::vector<StringBox>& attributeNames,
                                                      bool invalidateMeasure) {
#if VALDI_DEBUG_TREE_UPDATES
    std::string trigger = "reapply_attributes:";
    for (size_t i = 0; i < attributeNames.size(); ++i) {
        if (i != 0) {
            trigger += ",";
        }
        trigger += attributeNames[i].slowToString();
    }
    if (invalidateMeasure) {
        trigger += ",invalidateMeasure";
    }
    scheduleExclusiveUpdate(
        [this, attributeNames, invalidateMeasure]() {
            auto& attributesManager = getViewManagerContext()->getAttributesManager();
            auto attributeIds = attributesManager.getAttributeIds().getIdsForNames(attributeNames);
            auto rootViewNode = getRootViewNode();
            if (rootViewNode != nullptr) {
                rootViewNode->reapplyAttributesRecursive(
                    getCurrentViewTransactionScope(), attributeIds, invalidateMeasure);
            }
        },
        DispatchFunction(),
        std::move(trigger));
#else
    scheduleExclusiveUpdate(
        [this, attributeNames, invalidateMeasure]() {
            auto& attributesManager = getViewManagerContext()->getAttributesManager();
            auto attributeIds = attributesManager.getAttributeIds().getIdsForNames(attributeNames);
            auto rootViewNode = getRootViewNode();
            if (rootViewNode != nullptr) {
                rootViewNode->reapplyAttributesRecursive(
                    getCurrentViewTransactionScope(), attributeIds, invalidateMeasure);
            }
        },
        DispatchFunction());
#endif
}

void ViewNodeTree::updateCSS(const SharedAnimator& animator) {
    if (_rootViewNode != nullptr) {
        _rootViewNode->updateCSS(getCurrentViewTransactionScope(), animator);
    }
}

const SharedContext& ViewNodeTree::getContext() const {
    return _context;
}

const Ref<Runtime>& ViewNodeTree::getRuntime() const {
    return _runtime;
}

Ref<Metrics> ViewNodeTree::getMetrics() const {
    if (_runtime == nullptr) {
        return nullptr;
    }

    return _runtime->getMetrics();
}

void ViewNodeTree::attachAnimator(SharedAnimator animator, AnimationCancelToken token) {
    flushAnimator();
    _animator = std::move(animator);
    _pendingCancellableAnimations[token] = _animator;
}

void ViewNodeTree::cancelAnimation(AnimationCancelToken token) {
    const auto& it = _pendingCancellableAnimations.find(token);
    if (it != _pendingCancellableAnimations.end()) {
        auto animation = it->second;
        getCurrentViewTransactionScope().transaction().cancelAnimator(animation);
        // This animation will get cleaned up by the parent ViewNodeRenderer
        // after it's cancellation has completed.
        // ../Rendering/ViewNodeRenderer.cpp#L153
    }
}

void ViewNodeTree::removePendingAnimation(AnimationCancelToken token) {
    auto guard = lock();
    const auto& it = _pendingCancellableAnimations.find(token);
    if (it != _pendingCancellableAnimations.end()) {
        _pendingCancellableAnimations.erase(it);
    }
}

void ViewNodeTree::performUpdatesAndFlushAnimatorIfNeeded() {
    if (_animator != nullptr) {
        // Perform a layout to calculate the before and after frames
        performUpdates();
        // Commit all animations
        flushAnimator();
    }
}

void ViewNodeTree::flushAnimator() {
    if (_animator != nullptr) {
        getCurrentViewTransactionScope().transaction().flushAnimator(_animator, _animator->makeCompletionFunction());
        _animator = nullptr;
    }
}

const SharedAnimator& ViewNodeTree::getAttachedAnimator() const {
    return _animator;
}

// Raw APIs

Ref<ViewNode> ViewNodeTree::removeViewNode(RawViewNodeId id) {
    const auto& iterator = _rawViewNodes.find(id);
    if (iterator == _rawViewNodes.end()) {
        return nullptr;
    }

    auto viewNode = iterator->second;
    _rawViewNodes.erase(iterator);

    auto& viewTransactionScope = getCurrentViewTransactionScope();

    while (viewNode->getChildCount() > 0) {
        auto* child = viewNode->getChildAt(viewNode->getChildCount() - 1);
        if (removeViewNode(child->getRawId()) == nullptr) {
            child->removeFromParent(viewTransactionScope);
        }
    }

    viewNode->clear(viewTransactionScope);

    if (viewNode == _rootViewNode) {
        setRootViewNode(nullptr, false);
    }

    return viewNode;
}

Ref<ViewNode> ViewNodeTree::getViewNode(RawViewNodeId id) {
    const auto& iterator = _rawViewNodes.find(id);
    if (iterator == _rawViewNodes.end()) {
        return nullptr;
    }
    return iterator->second;
}

void ViewNodeTree::setViewToRootViewNode(const Ref<ViewNode>& rootViewNode, const Ref<View>& view) {
    auto& viewTransactionScope = getCurrentViewTransactionScope();
    if (rootViewNode != nullptr) {
        rootViewNode->setView(viewTransactionScope, view, nullptr);
    }

    if (view != nullptr) {
        viewTransactionScope.transaction().moveViewToTree(view, this, rootViewNode.get());
    }

    if (rootViewNode != nullptr) {
        rootViewNode->setViewTreeNeedsUpdate();
    }
}

void ViewNodeTree::setRootViewNode(Ref<ViewNode> rootViewNode, bool useDefaultViewFactory) {
    auto oldRootViewNode = std::move(_rootViewNode);
    if (oldRootViewNode != nullptr) {
        setViewToRootViewNode(oldRootViewNode, nullptr);
    }

    auto& viewTransactionScope = getCurrentViewTransactionScope();

    _rootViewNode = std::move(rootViewNode);

    if (_rootViewNode != nullptr) {
        _rootViewNode->removeFromParent(viewTransactionScope);
        if (useDefaultViewFactory) {
            SC_ASSERT_NOTNULL(_viewManager);
            _rootViewNode->setViewFactory(viewTransactionScope,
                                          getOrCreateViewFactory(_viewManager->getDefaultViewClass()));
        }

        if (_rootViewNode->isFlexLayoutDirty()) {
            onLayoutDirty();
        }
    }

    setViewToRootViewNode(_rootViewNode, _rootView);

    if (oldRootViewNode != nullptr &&
        (_rootViewNode != nullptr && _rootViewNode->getRawId() != oldRootViewNode->getRawId())) {
        // If we are switching from a root view node to another one with a different id, we automatically destroy
        // the previous view node to ensure we don't have a dangling subtree
        removeViewNode(oldRootViewNode->getRawId());
    }
}

void ViewNodeTree::addViewNode(const Ref<ViewNode>& viewNode) {
    SC_ASSERT(viewNode->getRawId() != 0);
    auto insertion = _rawViewNodes.try_emplace(viewNode->getRawId(), viewNode);
    if (insertion.second) {
        viewNode->setViewNodeTree(this);
    } else {
        // Remove the existing none
        removeViewNode(viewNode->getRawId());
        // Insert the new one
        addViewNode(viewNode);
    }
}

void ViewNodeTree::setKeepViewAliveOnDestroy(bool keepViewAliveOnDestroy) {
    if (_keepViewAliveOnDestroy != keepViewAliveOnDestroy) {
        _keepViewAliveOnDestroy = keepViewAliveOnDestroy;
    }
}

bool ViewNodeTree::keepViewAliveOnDestroy() const {
    return _keepViewAliveOnDestroy;
}

void ViewNodeTree::registerViewNodesVisibilityObserverCallback(const Ref<ValueFunction>& callback) {
    if (_visibilityObserver == nullptr) {
        _visibilityObserver = Valdi::makeShared<ViewNodesVisibilityObserver>(
            _shouldRenderInMainThread ? _mainThreadManager.get() : nullptr);
    }

    _visibilityObserver->setCallback(callback);
}

void ViewNodeTree::registerViewNodesFrameObserverCallback(const Ref<ValueFunction>& callback) {
    if (_framesObserver == nullptr) {
        _framesObserver = Valdi::makeShared<ViewNodesFrameObserver>();
    }

    _framesObserver->setCallback(callback);
}

void ViewNodeTree::onNextLayout(const Ref<ValueFunction>& callback) {
    _onLayoutCallbacks.emplace_back(callback);
    schedulePerformUpdates();
}

void ViewNodeTree::flushOnLayoutCallbacks() {
    if (_onLayoutCallbacks.empty()) {
        return;
    }
    auto onLayoutCallbacks = std::move(_onLayoutCallbacks);
    for (const auto& onLayoutCallback : onLayoutCallbacks) {
        if (onLayoutCallback == nullptr) {
            continue;
        }

        (*onLayoutCallback)();
    }
}

ViewNodesVisibilityObserver* ViewNodeTree::getViewNodesVisibilityObserver() const {
    return _visibilityObserver.get();
}

void ViewNodeTree::flushVisibilityObservers() {
    if (_visibilityObserver != nullptr) {
        _visibilityObserver->flush(getContext());
    }
}

ViewNodesFrameObserver* ViewNodeTree::getViewNodesFrameObserver() const {
    return _framesObserver.get();
}

void ViewNodeTree::flushFrameObserver() {
    if (_framesObserver != nullptr) {
        _framesObserver->flush();
    }
}

const AttributeOwner* ViewNodeTree::getParentAttributeOwner() {
    if (_parentAttributeOwner == nullptr) {
        _parentAttributeOwner = std::make_unique<ParentAttributeOwner>();
    }
    return _parentAttributeOwner.get();
}

const Ref<MainThreadManager>& ViewNodeTree::getMainThreadManager() const {
    return _mainThreadManager;
}

int ViewNodeTree::getAttributePriority(AttributeId /*id*/) const {
    return kAttributeOwnerPriorityViewNode;
}

StringBox ViewNodeTree::getAttributeSource(AttributeId /*id*/) const {
    auto kSource = STRING_LITERAL("viewNode");
    return kSource;
}

void ViewNodeTree::scheduleExclusiveUpdate(DispatchFunction updateFunction) {
    scheduleExclusiveUpdate(std::move(updateFunction), DispatchFunction(), {});
}

void ViewNodeTree::scheduleExclusiveUpdate(DispatchFunction updateFunction, DispatchFunction completion) {
    scheduleExclusiveUpdate(std::move(updateFunction), std::move(completion), {});
}

void ViewNodeTree::scheduleExclusiveUpdate(DispatchFunction updateFunction,
                                           DispatchFunction completion,
                                           std::string traceTrigger) {
    auto lockGuard = lock();
    _updateFunctions.emplace_back(std::move(updateFunction), std::move(completion), std::move(traceTrigger));

    if (!_updating) {
        runUpdates();
    }
}

TrackedLock ViewNodeTree::lock() const {
    return TrackedLock(_mutex);
}

void ViewNodeTree::withLock(const DispatchFunction& fn) {
    auto guard = lock();
    ContextEntry contextEntry(_context);
    _context->withAttribution([&]() {
        auto viewTransationScope = beginViewTransaction();
        fn();
        endViewTransaction(viewTransationScope, /* layoutDidBecomeDirty */ false);
    });
}

void ViewNodeTree::runUpdates() {
    if (_updateFunctions.empty()) {
        return;
    }

    _context->withAttribution([&]() { runUpdatesInner(); });
}

void ViewNodeTree::runUpdatesInner() {
    const auto runUpdatesStart = std::chrono::steady_clock::now();
    if (!_runUpdatesInnerSessionStart.has_value()) {
        _runUpdatesInnerSessionStart = runUpdatesStart;
    }

    ContextEntry contextEntry(_context);
    auto viewTransactionScope = beginViewTransaction();

    auto layoutDirtyCounter = _layoutDirtyCounter;
    _updating = true;
    SmallVector<DispatchFunction, 4> completions;

    Ref<AssetsManager> assetsManager;
    if (_runtime != nullptr) {
        assetsManager = _runtime->getResourceManager().getAssetsManager();
        assetsManager->beginPauseUpdates();
    }

#if !VALDI_DEBUG_TREE_UPDATES
    VALDI_TRACE("Valdi.runTreeUpdates");
#endif

    while (!_updateFunctions.empty()) {
        auto updates = std::move(_updateFunctions.front());
        _updateFunctions.pop_front();

#if VALDI_DEBUG_TREE_UPDATES
        // Emit trace per update with component name (symbol only) and trigger.
        std::string traceSuffix;
        if (!_context->getPath().isEmpty()) {
            const auto& path = _context->getPath();
            traceSuffix = path.getSymbolName().isEmpty() ? path.toString() : path.getSymbolName().slowToString();
        }
        if (!updates.traceTrigger.empty()) {
            if (!traceSuffix.empty()) {
                traceSuffix += "|";
            }
            traceSuffix += updates.traceTrigger;
        }
        if (traceSuffix.empty()) {
            traceSuffix = "unknown";
        }
        VALDI_TRACE_META("Valdi.runTreeUpdates", traceSuffix);
#endif

        updates.performUpdates();
        if (updates.completion) {
            completions.emplace_back(std::move(updates.completion));
        }
    }
    _updating = false;

    for (const auto& completion : completions) {
        completion();
    }

    if (assetsManager != nullptr) {
        assetsManager->flushUpdates();
    }

    auto layoutDidBecomeDirty = layoutDirtyCounter != _layoutDirtyCounter;
    if (layoutDidBecomeDirty && _runtime != nullptr) {
        _runtime->onViewNodeTreeLayoutBecameDirty(*this);
    }

    if (assetsManager != nullptr) {
        assetsManager->endPauseUpdates();
    }

    endViewTransaction(viewTransactionScope, layoutDidBecomeDirty);

    _runUpdatesInnerSessionStop = std::chrono::steady_clock::now();
    _runUpdatesInnerAccumulatedTime += *_runUpdatesInnerSessionStop - runUpdatesStart;

    flushRunUpdatesInnerStatsIfNeeded();
}

void ViewNodeTree::flushRunUpdatesInnerStatsIfNeeded() {
    auto metrics = getMetrics();
    if (metrics == nullptr || !_runUpdatesInnerSessionStart.has_value() || !_runUpdatesInnerSessionStop.has_value()) {
        return;
    }

    const auto sessionDuration = *_runUpdatesInnerSessionStop - *_runUpdatesInnerSessionStart;
    const auto sessionDurationMs = std::chrono::duration<double, std::milli>(sessionDuration).count();
    if (sessionDurationMs <= kUpdateRunTreeWindowDurationsMs) {
        // Throttle by time interval by checking that the session duration is long enough
        return;
    }

    const auto runUpdatesInnerMs = std::chrono::duration<double, std::milli>(_runUpdatesInnerAccumulatedTime).count();
    const auto percentage = static_cast<size_t>(std::ceil((runUpdatesInnerMs / sessionDurationMs) * 100.0));

    if (percentage >= kUpdateRunTreeMinPercentage) {
        metrics->emitRunUpdatesInnerTimePercentage(_context->getPath().getResourceId().bundleName, percentage);
    }

    clearRunUpdatesMetricsSession();
}

bool ViewNodeTree::inExclusiveUpdate() const {
    return _updating;
}

bool ViewNodeTree::isViewInflationEnabled() const {
    return _viewInflationEnabled && _rootView != nullptr;
}

void ViewNodeTree::setViewInflationEnabled(bool viewInflationEnabled) {
    if (_viewInflationEnabled != viewInflationEnabled) {
        _viewInflationEnabled = viewInflationEnabled;

        if (_rootViewNode != nullptr) {
            _rootViewNode->setViewTreeNeedsUpdate();
        }
    }
}

const Ref<ViewManagerContext>& ViewNodeTree::getViewManagerContext() const {
    return _viewManagerContext;
}

IViewManager* ViewNodeTree::getViewManager() const {
    return _viewManager;
}

ViewNodeTreeDisableUpdates ViewNodeTree::beginDisableUpdates() {
    auto lk = lock();
    _disableUpdatesCounter++;
    if (_disableUpdatesCounter == 1) {
        _wasUpdatingBeforeDisablingUpdates = _updating;
        _updating = true;
    }

    return ViewNodeTreeDisableUpdates(strongSmallRef(this), std::move(lk));
}

void ViewNodeTree::lockFreeEndDisableUpdates() {
    _disableUpdatesCounter--;

    if (_disableUpdatesCounter == 0) {
        _updating = _wasUpdatingBeforeDisablingUpdates;
        if (!_updating) {
            runUpdates();
        }
    }
}

void ViewNodeTree::setParentTree(const Ref<ViewNodeTree>& parentTree) {
    if (parentTree == nullptr) {
        return;
    }

    for (const auto& it : parentTree->_viewFactories) {
        if (getViewFactory(it.first) == nullptr) {
            registerLocalViewFactory(it.first, it.second);
        }
    }
}

ViewTransactionScope& ViewNodeTree::getCurrentViewTransactionScope() {
    const auto& ref = getCurrentViewTransactionScopeRef();
    SC_ABORT_UNLESS(ref != nullptr && _beginViewTransactionCounter > 0,
                    "ViewNodeTree does not have a current view transaction scope");

    return *ref;
}

const Ref<ViewTransactionScope>& ViewNodeTree::getCurrentViewTransactionScopeRef() {
    _mutex.assertIsLocked();
    return _currentViewTransactionScope;
}

void ViewNodeTree::unsafeSetCurrentViewTransactionScope(const Ref<ViewTransactionScope>& currentViewTransactionScope) {
    _currentViewTransactionScope = currentViewTransactionScope;
}

void ViewNodeTree::unsafeBeginViewTransaction() {
    beginViewTransaction();
}

void ViewNodeTree::unsafeEndViewTransaction() {
    endViewTransaction(_currentViewTransactionScope, false);
}

Ref<ViewTransactionScope> ViewNodeTree::beginViewTransaction() {
    _beginViewTransactionCounter++;

    auto viewTransactionScope = _currentViewTransactionScope;
    if (viewTransactionScope == nullptr) {
        viewTransactionScope =
            makeShared<ViewTransactionScope>(_viewManager, _mainThreadManager, _shouldRenderInMainThread);
        _currentViewTransactionScope = viewTransactionScope;
    }

    if (_beginViewTransactionCounter == 1) {
        viewTransactionScope->setRootView(getRootView());
    }

    return viewTransactionScope;
}

void ViewNodeTree::endViewTransaction(const Ref<ViewTransactionScope>& viewTransactionScope,
                                      bool layoutDidBecomeDirty) {
    SC_ABORT_UNLESS(_beginViewTransactionCounter > 0, "Unbalanced beginViewTransaction/endViewTransaction call");

    if (layoutDidBecomeDirty) {
        viewTransactionScope->setLayoutDidBecomeDirty(true);
    }

    if (_beginViewTransactionCounter == 1) {
        viewTransactionScope->submit();
    }

    _beginViewTransactionCounter--;
}

bool ViewNodeTree::shouldRenderInMainThread() const {
    return _shouldRenderInMainThread;
}

const Ref<IViewNodesAssetTracker>& ViewNodeTree::getAssetTracker() const {
    return _assetTracker;
}

void ViewNodeTree::setAssetTracker(const Ref<IViewNodesAssetTracker>& assetTracker) {
    _assetTracker = assetTracker;
}

ViewNodeTreeDisableUpdates::ViewNodeTreeDisableUpdates() : _viewNodeTree(nullptr) {}

ViewNodeTreeDisableUpdates::ViewNodeTreeDisableUpdates(const Ref<ViewNodeTree>& tree, TrackedLock&& guard)
    : _viewNodeTree(tree), _guard(std::move(guard)), _disabled(true) {}

ViewNodeTreeDisableUpdates& ViewNodeTreeDisableUpdates::operator=(ViewNodeTreeDisableUpdates&& other) noexcept {
    _viewNodeTree = std::move(other._viewNodeTree);
    _guard = std::move(other._guard);
    _disabled = other._disabled;
    return *this;
}

ViewNodeTreeDisableUpdates::~ViewNodeTreeDisableUpdates() {
    endDisableUpdates();
}

void ViewNodeTreeDisableUpdates::endDisableUpdates() {
    if (_disabled && _viewNodeTree != nullptr) {
        _viewNodeTree->lockFreeEndDisableUpdates();
        _disabled = false;
        _guard.unlock();
    }
}

} // namespace Valdi
