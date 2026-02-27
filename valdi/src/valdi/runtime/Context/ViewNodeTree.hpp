//
//  ViewNodesContext.hpp
//  ValdiRuntime
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#pragma once

#include "valdi/runtime/Attributes/Animator.hpp"
#include "valdi/runtime/Attributes/AttributeOwner.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Views/Measure.hpp"
#include "valdi/runtime/Views/View.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"
#include "valdi/runtime/Views/ViewTransactionScope.hpp"
#include "valdi_core/cpp/Utils/FlatMap.hpp"
#include "valdi_core/cpp/Utils/Mutex.hpp"
#include "valdi_core/cpp/Utils/TrackedLock.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include <chrono>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace Valdi {

class Runtime;
class ViewNodePath;

class ViewNodeTree;
using SharedViewNodeTree = Ref<ViewNodeTree>;

class ViewNodesVisibilityObserver;
class ViewNodesFrameObserver;
class IViewNodesAssetTracker;
class MainThreadManager;
class AttributesManager;
class Metrics;

struct ViewNodeTreeUpdates {
    DispatchFunction performUpdates;
    DispatchFunction completion;
    /** Optional trigger reason for tracing (e.g. "render_request", "setLayoutSpecs", "reapply_attributes:height"). */
    std::string traceTrigger;

    inline ViewNodeTreeUpdates(DispatchFunction&& performUpdates,
                               DispatchFunction&& completion,
                               std::string traceTrigger = {})
        : performUpdates(std::move(performUpdates)),
          completion(std::move(completion)),
          traceTrigger(std::move(traceTrigger)) {}
};

class ViewNodeTreeDisableUpdates;

using AnimationCancelToken = unsigned int;
class ViewTransactionScope;

/**
 * Container of the root ViewNode which can also index the ViewNodes by their ids. Indexing happens when a user gives an
 * id attribute to an element. This is used so that one can quickly retrieve a ViewNode from an id. Auto-generated ids
 * are not indexed in the ViewNodes, but they are part of the ViewNode tree.
 */
class ViewNodeTree : public SharedPtrRefCountable, public AttributeOwner {
public:
    ViewNodeTree(const SharedContext& context,
                 const Ref<ViewManagerContext>& viewManagerContext,
                 IViewManager* viewManager,
                 std::shared_ptr<Runtime> runtime,
                 MainThreadManager* mainThreadManager,
                 bool shouldRenderInMainThread);
    ~ViewNodeTree() override;

    void clear();
    void clearRunUpdatesMetricsSession();

    void updateCSS(const SharedAnimator& animator);

    /**
     Get the first view for the given node path
     */
    Ref<View> getViewForNodePath(const ViewNodePath& nodePath) const;

    /**
     Get the view node for the given absolute node path.
     */
    Ref<ViewNode> getViewNodeForNodePath(const ViewNodePath& nodePath) const;

    const Ref<ViewNode>& getRootViewNode() const;

    Ref<View> getRootView() const;

    /**
     Attach the root view to the ViewNodeTree.
     The view tree won't be created until a root view is provided.
     */
    void setRootView(const Ref<View>& view);

    /**
     Create and set the root view using the default view class from the ViewManager
     */
    void setRootViewWithDefaultViewClass();

    /**
     Sets the layout specifications. This will potentially calculate the layout if the size or orientation has changed .
     */
    void setLayoutSpecs(Size layoutSize, LayoutDirection layoutDirection);

    /**
     Measure the layout for the given size constraint.
     Returns the desired layout size.
     */
    Size measureLayout(
        float width, MeasureMode widthMode, float height, MeasureMode heightMode, LayoutDirection layoutDirection);

    /**
     * Set the visible viewport that should be used when computing the viewports from
     * the root node. This can be used to only render a smaller area of the root node.
     * When set to null optional, the root node will be considered as completely
     * visible.
     */
    void setViewport(std::optional<Frame> viewport);

    /**
     * Returns the viewport which was previously set from a setViewport call
     */
    std::optional<Frame> getViewport() const;

    /**
     Perform all pending updates to the ViewNode tree.
     */
    void performUpdates();

    const SharedContext& getContext() const;

    AttributesManager& getAttributesManager() const;

    const Ref<Runtime>& getRuntime() const;
    Ref<Metrics> getMetrics() const;

    void attachAnimator(SharedAnimator animator, AnimationCancelToken token);
    void cancelAnimation(AnimationCancelToken token);
    void removePendingAnimation(AnimationCancelToken token);
    const SharedAnimator& getAttachedAnimator() const;

    const Ref<ViewManagerContext>& getViewManagerContext() const;
    IViewManager* getViewManager() const;

    void flushAnimator();
    void performUpdatesAndFlushAnimatorIfNeeded();

    void scheduleReapplyAttributesRecursive(const std::vector<StringBox>& attributeNames, bool invalidateMeasure);

    Ref<ViewFactory> getViewFactory(const StringBox& viewClassName) const;
    const FlatMap<StringBox, Ref<ViewFactory>>& getViewFactories() const;
    void registerLocalViewFactory(const StringBox& viewClassName, const Ref<ViewFactory>& viewFactory);

    Ref<ViewFactory> getOrCreateViewFactory(const StringBox& viewClassName);
    Ref<ViewFactory> getCompatibleViewFactory(const Ref<ViewFactory>& viewFactory) const;

    // Raw APIs
    void addViewNode(const Ref<ViewNode>& viewNode);
    Ref<ViewNode> removeViewNode(RawViewNodeId id);

    Ref<ViewNode> getViewNode(RawViewNodeId id);
    void setRootViewNode(Ref<ViewNode> rootViewNode, bool useDefaultViewFactory);

    bool keepViewAliveOnDestroy() const;
    void setKeepViewAliveOnDestroy(bool keepViewAliveOnDestroy);

    /**
     Set whether the layout specs should be kept when the layout is invalidated. When this option is set, the
     ViewNodeTree will be able to immediately recalculate the layout on layout invalidation without asking platform
     about new layout specs.
     */
    void setRetainsLayoutSpecsOnInvalidateLayout(bool retainsLayoutSpecsOnInvalidateLayout);
    bool retainsLayoutSpecsOnInvalidateLayout() const;

    /**
     Schedule an exclusive update function, which will be called once
     all the pending exclusive updates have finished running.
     */
    void scheduleExclusiveUpdate(DispatchFunction updateFunction);

    /**
     Schedule an exclusive update function, which will be called once
     all the pending exclusive updates have finished running.
     */
    void scheduleExclusiveUpdate(DispatchFunction updateFunction, DispatchFunction completion);

    /**
     Schedule an exclusive update with an optional trace trigger (e.g. "render_request",
     "reapply_attributes:height,padding") for visibility in traces.
     */
    void scheduleExclusiveUpdate(DispatchFunction updateFunction,
                                 DispatchFunction completion,
                                 std::string traceTrigger);

    void withLock(const DispatchFunction& fn);

    bool inExclusiveUpdate() const;

    bool isLayoutSpecsDirty() const;

    bool isViewInflationEnabled() const;
    void setViewInflationEnabled(bool viewInflationEnabled);

    void registerViewNodesVisibilityObserverCallback(const Ref<ValueFunction>& callback);
    void registerViewNodesFrameObserverCallback(const Ref<ValueFunction>& callback);

    ViewNodesVisibilityObserver* getViewNodesVisibilityObserver() const;
    void flushVisibilityObservers();

    ViewNodesFrameObserver* getViewNodesFrameObserver() const;
    void flushFrameObserver();

    const AttributeOwner* getParentAttributeOwner();

    const Ref<MainThreadManager>& getMainThreadManager() const;

    int getAttributePriority(AttributeId id) const override;

    StringBox getAttributeSource(AttributeId id) const override;

    void setParentTree(const Ref<ViewNodeTree>& parentTree);

    const Ref<IViewNodesAssetTracker>& getAssetTracker() const;
    void setAssetTracker(const Ref<IViewNodesAssetTracker>& assetTracker);

    void onNextLayout(const Ref<ValueFunction>& callback);

    [[nodiscard]] ViewNodeTreeDisableUpdates beginDisableUpdates();

    void onLayoutDirty();
    void onRootViewNodeNeedsUpdate();

    // Unsafe to call unless the ViewNodeTree lock is acquired.
    ViewTransactionScope& getCurrentViewTransactionScope();
    const Ref<ViewTransactionScope>& getCurrentViewTransactionScopeRef();
    void unsafeSetCurrentViewTransactionScope(const Ref<ViewTransactionScope>& currentViewTransactionScope);

    // Exposed for unit tests only
    void unsafeBeginViewTransaction();
    void unsafeEndViewTransaction();

    bool shouldRenderInMainThread() const;

    TrackedLock lock() const;

private:
    SharedContext _context;
    Ref<ViewManagerContext> _viewManagerContext;
    IViewManager* _viewManager;
    Ref<Runtime> _runtime;

    FlatMap<StringBox, Ref<ViewFactory>> _viewFactories;
    FlatMap<RawViewNodeId, Ref<ViewNode>> _rawViewNodes;
    Ref<ViewNode> _rootViewNode;
    Ref<ViewNodesVisibilityObserver> _visibilityObserver;
    Ref<ViewNodesFrameObserver> _framesObserver;
    Ref<IViewNodesAssetTracker> _assetTracker;
    SharedAnimator _animator;
    Ref<View> _rootView;
    Ref<MainThreadManager> _mainThreadManager;
    Ref<ViewTransactionScope> _currentViewTransactionScope;
    std::deque<ViewNodeTreeUpdates> _updateFunctions;
    std::vector<Ref<ValueFunction>> _onLayoutCallbacks;
    mutable RecursiveMutex _mutex;

    FlatMap<AnimationCancelToken, SharedAnimator> _pendingCancellableAnimations;

    Size _layoutSize;
    LayoutDirection _layoutDirection = LayoutDirectionLTR;
    std::optional<Frame> _viewport;
    bool _keepViewAliveOnDestroy = false;
    bool _shouldRenderInMainThread;
    bool _updating = false;
    bool _wasUpdatingBeforeDisablingUpdates = false;

    // Whether we ever had layout specs set.
    // We prevent any sort of updates until the first layoutSpecs comes in
    bool _hasLayoutSpecs = false;
    // Whether our layout is dirty and needs to be calculated
    // This will be toggled when the flexbox layout is invalidated,
    // or if the layoutSpecs are changing
    bool _layoutDirty = true;
    // Whether we are awaiting updated layoutSpecs.
    // We keep this flag separately from hasLayoutSpecs, as for animations
    // purposes, we re-use the previous layoutSpecs even if they are dirty.
    // When starting animations, we currently can't wait until the updated specs comes
    // in, since we need to properly calculate the "from" and "to" states synchronously.
    bool _layoutSpecsDirty = true;
    // Whether we scheduled a performUpdates() pass. This flag will toggle when the root
    // view node notifies us that it needs some updates.
    bool _scheduledPerformUpdates = false;
    bool _viewInflationEnabled = true;
    bool _retainsLayoutSpecsOnInvalidateLayout = false;
    int _disableUpdatesCounter = 0;
    int _beginViewTransactionCounter = 0;
    size_t _layoutDirtyCounter = 0;
    std::chrono::steady_clock::duration _runUpdatesInnerAccumulatedTime{0};
    std::optional<std::chrono::steady_clock::time_point> _runUpdatesInnerSessionStart;
    std::optional<std::chrono::steady_clock::time_point> _runUpdatesInnerSessionStop;

    std::unique_ptr<AttributeOwner> _parentAttributeOwner;

    void flushRunUpdatesInnerStatsIfNeeded();

    void attachRootNodeInParentTreeIfNeeded();
    void runUpdates();
    void runUpdatesInner();

    void flushOnLayoutCallbacks();

    void schedulePerformUpdates();
    void performUpdatesIfLayoutSpecsUpToDate();

    Ref<ViewFactory> createDeferredViewFactory();
    void setViewToRootViewNode(const Ref<ViewNode>& rootViewNode, const Ref<View>& view);

    Ref<ViewTransactionScope> beginViewTransaction();
    void endViewTransaction(const Ref<ViewTransactionScope>& viewTransactionScope, bool layoutDidBecomeDirty);

    friend ViewNodeTreeDisableUpdates;

    void lockFreeEndDisableUpdates();
};

class ViewNodeTreeDisableUpdates {
public:
    ViewNodeTreeDisableUpdates();

    explicit ViewNodeTreeDisableUpdates(const Ref<ViewNodeTree>& tree, TrackedLock&& guard);

    ViewNodeTreeDisableUpdates& operator=(ViewNodeTreeDisableUpdates&& other) noexcept;
    ~ViewNodeTreeDisableUpdates();

    void endDisableUpdates();

private:
    Ref<ViewNodeTree> _viewNodeTree;
    TrackedLock _guard;
    bool _disabled = false;
};

} // namespace Valdi
