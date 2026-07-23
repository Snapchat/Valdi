//
//  ViewNode.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/4/18.
//

#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Attributes/AttributeOwner.hpp"
#include "valdi/runtime/Attributes/ViewNodeAttributesApplier.hpp"
#include "valdi/runtime/Attributes/Yoga/Yoga.hpp"
#include "valdi/runtime/CSS/CSSAttributesManager.hpp"
#include "valdi/runtime/Context/IViewNodeAssetHandler.hpp"
#include "valdi/runtime/Context/ScrollAnchorPosition.hpp"
#include "valdi/runtime/Context/StickyPosition.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/Context/ViewNodeAccessibilityState.hpp"
#include "valdi/runtime/Context/ViewNodeChildrenIndexer.hpp"
#include "valdi/runtime/Context/ViewNodeScrollState.hpp"
#include "valdi/runtime/Context/ViewNodeTree.hpp"
#include "valdi/runtime/Context/ViewNodeViewStats.hpp"
#include "valdi/runtime/Context/ViewNodesFrameObserver.hpp"
#include "valdi/runtime/Context/ViewNodesVisibilityObserver.hpp"
#include "valdi/runtime/Interfaces/IViewManager.hpp"
#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/ValdiBuildFlags.hpp"
#include "valdi/runtime/Views/MeasureDelegate.hpp"
#include "valdi/runtime/Views/ViewTransactionScope.hpp"
#include "valdi_core/cpp/Constants.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueArrayBuilder.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

#include "valdi/runtime/Metrics/Metrics.hpp"

#include "utils/debugging/Assert.hpp"
#include "utils/time/StopWatch.hpp"
#include "valdi_core/cpp/Utils/ContainerUtils.hpp"
#include <cmath>
#include <fmt/format.h>
#include <limits>
#include <sstream>
#include <yoga/Yoga.h>
#include <yoga/algorithm/CalculateLayout.h>
#include <yoga/enums/FlexDirection.h>
#include <yoga/node/Node.h>
#include <yoga/style/StyleSizeLength.h>

namespace Valdi {

YGSize ygMeasureYoga(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode);
void ygDirtiedCallback(YGNodeConstRef node);

void destroyYogaNode(YGNode* yogaNode) {
    if (yogaNode == nullptr) {
        return;
    }
    auto* node = facebook::yoga::resolveRef(yogaNode);
    node->setContext(nullptr);
    node->setDirtiedFunc(nullptr);
    node->setMeasureFunc(nullptr);
    YGNodeFree(yogaNode);
}

void setupYogaNode(YGNode* yogaNode, ViewNode* viewNode) {
    Yoga::attachViewNode(yogaNode, viewNode);
    auto* node = facebook::yoga::resolveRef(yogaNode);
    node->setDirty(true);
    node->setDirtiedFunc(ygDirtiedCallback);
}

static facebook::yoga::Style& getYogaStyle(YGNodeRef node) {
    return facebook::yoga::resolveRef(node)->style();
}

static facebook::yoga::Node* resolveYogaNode(YGNodeRef node) {
    return facebook::yoga::resolveRef(node);
}

static const facebook::yoga::Node* resolveYogaNode(YGNodeConstRef node) {
    return facebook::yoga::resolveRef(node);
}

static auto getBackend(ViewNodeTree* tree) {
    if (!tree) {
        return RenderingBackendTypeUnset;
    }
    auto viewManagerContext = tree->getViewManagerContext();
    if (viewManagerContext == nullptr) {
        return RenderingBackendTypeUnset;
    }

    return viewManagerContext->getViewManager().getRenderingBackendType();
}

static inline float sanitizeYogaValue(float yogaValue) {
    if (std::isnan(yogaValue)) {
        return 0.0;
    }

    return yogaValue;
}

static Frame ygNodeGetFrame(YGNode* yogaNode, float offsetX) {
    const auto& layout = resolveYogaNode(yogaNode)->getLayout();
    return Frame(sanitizeYogaValue(layout.position(facebook::yoga::PhysicalEdge::Left) + offsetX),
                 sanitizeYogaValue(layout.position(facebook::yoga::PhysicalEdge::Top)),
                 sanitizeYogaValue(layout.dimension(facebook::yoga::Dimension::Width)),
                 sanitizeYogaValue(layout.dimension(facebook::yoga::Dimension::Height)));
}

LazyLayoutData::~LazyLayoutData() {
    destroyNode();
}

void LazyLayoutData::destroyNode() {
    if (yogaNode != nullptr) {
        destroyYogaNode(yogaNode);
        yogaNode = nullptr;
    }
}

float ViewNodeTranslation::getResolvedValue(float referenceLength, bool isPercent) const {
    if (isPercent) {
        return referenceLength * (value / 100.0f);
    }
    return value;
}

const SharedAnimator& nullAnimator() {
    static SharedAnimator nullAnimator;
    return nullAnimator;
}

constexpr size_t kUpdatingScrollSpecs = 0;
constexpr size_t kVisibleInViewportFlag = 1;
constexpr size_t kCalculatedViewportNeedsUpdateFlag = 2;
constexpr size_t kCalculatedViewportHasChildNeedsUpdateFlag = 3;
constexpr size_t kViewTreeNeedsUpdateFlag = 4;
constexpr size_t kLayoutDidCompleteOnceFlag = 5;
constexpr size_t kIsLayoutFlag = 6;
constexpr size_t kViewIncludedInParentFlag = 7;
constexpr size_t kIsLazyLayoutFlag = 8;
constexpr size_t kPrefersLazyLayoutFlag = 9;
constexpr size_t kHasLazyLayoutNeedingCalculationFlag = 10;
constexpr size_t kNeedsFrameUpdateFlag = 11;
constexpr size_t kCalculatingLayoutFlag = 12;
constexpr size_t kLimitToViewportEnabledFlag = 13;
constexpr size_t kLimitToViewportDisabledFlag = 14;
constexpr size_t kHasChildWithZIndex = 15;
constexpr size_t kAnimationsEnabled = 16;
constexpr size_t kHasParent = 17;
constexpr size_t kShouldReceiveVisibilityUpdates = 19;
constexpr size_t kCSSNeedsUpdate = 20;
constexpr size_t kCSSHasChildNeedsUpdate = 21;
constexpr size_t kExtendViewportWithChildren = 22;
constexpr size_t kIgnoreParentViewport = 23;
constexpr size_t kScrollAttributesBound = 24;
constexpr size_t kLayoutIsRightToLeft = 25;
constexpr size_t kHasChildWithAccessibilityId = 26;
constexpr size_t kCanAlwaysScrollHorizontal = 27;
constexpr size_t kCanAlwaysScrollVertical = 28;
constexpr size_t kAccessibilityTreeNeedsUpdate = 29;
constexpr size_t kTranslationXIsPercent = 30;
constexpr size_t kTranslationYIsPercent = 31;

ViewNode::ViewNode(YGConfig* yogaConfig, AttributeIds& attributeIds, ILogger& logger)
    : _yogaNode(yogaConfig != nullptr ? Yoga::createNode(yogaConfig) : nullptr),
      _attributeIds(attributeIds),
      _logger(logger),
      _attributesApplier(this) {
    if (_yogaNode != nullptr) {
        setupYogaNode(_yogaNode, this);
    }

    _flags[kCalculatedViewportNeedsUpdateFlag] = true;
    _flags[kViewTreeNeedsUpdateFlag] = true;
    _flags[kAnimationsEnabled] = true;
    _flags[kAccessibilityTreeNeedsUpdate] = true;
}

ViewNode::~ViewNode() {
    _attributesApplier.destroy();
    destroyYogaNode(_yogaNode);
    _lazyLayoutData = nullptr;
}

void ViewNode::clear(ViewTransactionScope& viewTransactionScope) {
    removeView(viewTransactionScope);
    removeFromParent(viewTransactionScope);
    setViewNodeTree(nullptr);
    _viewFactory = nullptr;
}

void ViewNode::setViewNodeTree(ViewNodeTree* viewNodeTree) {
    _viewNodeTree = viewNodeTree;
}

ViewNodeTree* ViewNode::getViewNodeTree() const {
    return _viewNodeTree;
}

const Ref<View>& ViewNode::getView() const {
    return _view;
}

Ref<View> ViewNode::getViewAndDisablePooling() const {
    if (_view != nullptr) {
        _view->setCanBeReused(false);
    }

    return _view;
}

Value ViewNode::toPlaformRepresentation(bool wrapInPlatformReference) {
    return _viewNodeTree->getViewManagerContext()->getViewManager().createViewNodeWrapper(strongSmallRef(this),
                                                                                          wrapInPlatformReference);
}

static void callViewCallbackIfNeeded(const Ref<ValueFunction>& valueFunction) {
    if (valueFunction != nullptr) {
        (*valueFunction)();
    }
}

void ViewNode::callViewChangedIfNeeded() {
    if (_onViewChangedCallback == nullptr) {
        return;
    }
    auto param = toPlaformRepresentation(true);
    (*_onViewChangedCallback)(ValueFunctionFlagsNone, {param});
}

void ViewNode::removeViewFromParent(ViewTransactionScope& viewTransactionScope) {
    removeViewFromParent(viewTransactionScope, /*willUpdateViewTree*/ false, /*shouldViewClearViewNode*/ false);
}

void ViewNode::removeViewFromParent(ViewTransactionScope& viewTransactionScope, bool shouldViewClearViewNode) {
    removeViewFromParent(viewTransactionScope, /*willUpdateViewTree*/ false, shouldViewClearViewNode);
}

void ViewNode::removeViewFromParent(ViewTransactionScope& viewTransactionScope,
                                    bool willUpdateViewTree,
                                    bool shouldViewClearViewNode) {
    if (_flags[kViewIncludedInParentFlag]) {
        _flags[kViewIncludedInParentFlag] = false;

        if (hasView()) {
            const auto& animator = resolveAnimator(nullAnimator());

            viewTransactionScope.transaction().removeViewFromParent(_view, animator, shouldViewClearViewNode);

            if (willUpdateViewTree) {
                setViewTreeNeedsUpdate();
            }
        } else if (isLayout()) {
            // For a layout, this means that one of our child is inside our parent view.
            for (auto i = getChildCount(); i > 0; i--) {
                getChildAt(i - 1)->removeViewFromParent(
                    viewTransactionScope, willUpdateViewTree, shouldViewClearViewNode);
            }
        }
        _numberOfViewChildren = 0;
        _numberOfViewChildrenInsertedInTree = 0;
    }
}

void ViewNode::setView(ViewTransactionScope& viewTransactionScope,
                       const Ref<View>& view,
                       const Ref<Animator>& animator) {
    if (view != _view) {
        SC_ASSERT(view == nullptr || !isLayout());

        removeViewFromParent(viewTransactionScope, /*shouldViewClearViewNode*/ true);

        if (hasView()) {
            callViewCallbackIfNeeded(_onViewDestroyedCallback);

            // We don't enqueue the root view of the root context to the view pool,
            // nor do we explicitly remove it from the view hierarchy. We let
            // native code handle the root view itself.
            auto shouldEnqueueToPool = _view->canBeReused() && !getViewClassName().isEmpty();

            // Restore the view to default
            _attributesApplier.willRemoveView(viewTransactionScope);

            if (shouldEnqueueToPool && _viewFactory != nullptr) {
                viewTransactionScope.transaction().willEnqueueViewToPool(
                    _view, [viewFactory = _viewFactory](View& view) {
                        viewFactory->enqueueViewToPool(strongSmallRef(&view));
                    });
            }
        }

        _view = view;

        if (_scrollState != nullptr) {
            _scrollState->setNeedsSyncWithView(true);
        }

        if (hasView()) {
            _attributesApplier.didAddView(viewTransactionScope, animator);

            callViewCallbackIfNeeded(_onViewCreatedCallback);
            if (_flags[kLayoutDidCompleteOnceFlag]) {
                setViewFrameNeedsUpdate();
            }
        }

        callViewChangedIfNeeded();
    }
}

bool ViewNode::removeView(ViewTransactionScope& viewTransactionScope) {
    return removeView(viewTransactionScope, true);
}

bool ViewNode::removeView(ViewTransactionScope& viewTransactionScope, bool safeRemove) {
    if (hasView()) {
        if (safeRemove) {
            // To ensure that we are not going to enqueue a view that has children,
            // we explicitly remove all the view children from this view.
            for (auto i = getChildCount(); i > 0; i--) {
                getChildAt(i - 1)->removeViewFromParent(viewTransactionScope, false);
            }
        }
        setView(viewTransactionScope, nullptr, nullptr);
        return true;
    } else {
        return false;
    }
}

bool ViewNode::createView(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator) {
    if (hasView() || isLayout() || _viewNodeTree == nullptr || _viewFactory == nullptr) {
        return false;
    }

    auto view = _viewFactory->createView(_viewNodeTree, this, true);

    if (view != nullptr) {
        viewTransactionScope.transaction().moveViewToTree(view, _viewNodeTree, this);
        setView(viewTransactionScope, view, animator);
        return true;
    } else {
        VALDI_WARN(getLogger(), "{} Could not create view with class {}", getLoggerFormatPrefix(), getViewClassName());
        return false;
    }
}

bool ViewNode::hasView() const {
    return _view != nullptr;
}

int ViewNode::getViewChildrenCount() const {
    return _numberOfViewChildren;
}

bool ViewNode::viewTreeNeedsUpdate() const {
    return _flags[kViewTreeNeedsUpdateFlag];
}

void ViewNode::setViewTreeNeedsUpdate() {
    if (!_flags[kViewTreeNeedsUpdateFlag]) {
        _flags[kViewTreeNeedsUpdateFlag] = true;
        // The flag will have to be re-updated
        _numberOfViewChildren = 0;
        _numberOfViewChildrenInsertedInTree = 0;
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setViewTreeNeedsUpdate();
        } else {
            scheduleUpdates();
        }
    }
}

const SharedAnimator& ViewNode::resolveAnimator(const SharedAnimator& parentAnimator) const {
    if (!isAnimationsEnabled()) {
        return nullAnimator();
    }

    const auto& animator = _viewNodeTree->getAttachedAnimator();

    if (animator != nullptr) {
        return animator;
    }

    return parentAnimator;
}

void ViewNode::onFinishedAnimating() {
    _animationsCount--;

    if (_animationsCount == 0) {
        if (!viewTreeNeedsUpdate()) {
            setViewTreeNeedsUpdate();
        }
    }
}

void ViewNode::doUpdateViewTree(ViewTransactionScope& viewTransactionScope,
                                const Ref<View>& currentParentView,
                                bool parentVisibleInViewport,
                                bool limitToViewportEnabled,
                                bool parentViewChanged,
                                bool viewInflationEnabled,
                                const SharedAnimator& parentAnimator,
                                ViewNodeUpdateViewTreeResult& updateResult,
                                int* currentViewIndex,
                                int* viewsCount) {
    auto needUpdate = viewTreeNeedsUpdate();

    updateResult.visitedNodes++;
    _flags[kViewTreeNeedsUpdateFlag] = false;

    auto visibleInViewport = parentVisibleInViewport && isVisibleInViewport();

    const auto& animator = resolveAnimator(parentAnimator);

    // Update the limit to viewport state with our current view node configuration
    auto limitToViewportState = limitToViewport();
    if (limitToViewportState == LimitToViewportEnabled) {
        limitToViewportEnabled = true;
    } else if (limitToViewportState == LimitToViewportDisabled) {
        limitToViewportEnabled = false;
    }

    if (animator != nullptr && limitToViewportEnabled && !visibleInViewport && isIncludedInViewParent()) {
        // If we are animating, we don't want limitToViewport to kick in during the animation if we are
        // becoming invisible.
        _animationsCount++;

        animator->appendCompletion([weakThis = weakRef(this)](const auto& /*callContext*/) {
            if (auto strongThis = weakThis.lock()) {
                strongThis->onFinishedAnimating();
            }

            return Value::undefined();
        });
    }

    if (_animationsCount > 0) {
        // Always consider that we are visible when animating to avoid being destroyed.
        visibleInViewport = true;
    }

    bool needView;
    if (!hasParent()) {
        // If we are root, we only want to have a view if we already had one provided.
        // The root view is always externally provided.
        needView = hasView();
    } else {
        // For the rest of the nodes, we want a view if view inflation is enabled and we are visible in the viewport
        needView = (viewInflationEnabled && (!limitToViewportEnabled || visibleInViewport));
    }

    if (isLayout()) {
        if (_flags[kViewIncludedInParentFlag] != needView) {
            _flags[kViewIncludedInParentFlag] = needView;
            needUpdate = true;
        }

        if (parentViewChanged && _numberOfViewChildren > 0) {
            needUpdate = true;
        }

        if (needUpdate) {
            int beforeViewIndex = *currentViewIndex;
            int beforeViewsCount = *viewsCount;

            if (hasChildWithZIndex()) {
                auto children = sortChildrenByZIndex();
                for (ViewNode* childViewNode : *children) {
                    childViewNode->doUpdateViewTree(viewTransactionScope,
                                                    currentParentView,
                                                    visibleInViewport,
                                                    limitToViewportEnabled,
                                                    parentViewChanged,
                                                    viewInflationEnabled,
                                                    animator,
                                                    updateResult,
                                                    currentViewIndex,
                                                    viewsCount);
                }
            } else {
                for (ViewNode* childViewNode : *this) {
                    childViewNode->doUpdateViewTree(viewTransactionScope,
                                                    currentParentView,
                                                    visibleInViewport,
                                                    limitToViewportEnabled,
                                                    parentViewChanged,
                                                    viewInflationEnabled,
                                                    animator,
                                                    updateResult,
                                                    currentViewIndex,
                                                    viewsCount);
                }
            }

            _numberOfViewChildrenInsertedInTree = *currentViewIndex - beforeViewIndex;
            _numberOfViewChildren = *viewsCount - beforeViewsCount;
        } else {
            *currentViewIndex += _numberOfViewChildrenInsertedInTree;
            *viewsCount += _numberOfViewChildren;
        }
    } else {
        bool viewChanged = false;
        if (needView) {
            if (createView(viewTransactionScope, animator)) {
                viewChanged = true;
                updateResult.createdViews++;
            }

            if (parentViewChanged && _flags[kViewIncludedInParentFlag] && currentParentView == nullptr) {
                // Case where the parent view has changed, we were included in another parent view
                // and we don't have a parent view anymore. We need to remove ourself from our previous parent.
                removeViewFromParent(viewTransactionScope);
            } else if ((parentViewChanged || !_flags[kViewIncludedInParentFlag]) && currentParentView != nullptr &&
                       hasView()) {
                // Case where the parent view has changed and we are not included in the new parent
                // We need to insert ourself in the new parent
                _flags[kViewIncludedInParentFlag] = true;

                viewTransactionScope.transaction().insertChildView(
                    currentParentView, _view, *currentViewIndex, animator);

                updateResult.reinsertedViews++;
                syncScrollSpecsWithViewIfNeeded(viewTransactionScope);
                updateViewFrameIfNeeded(viewTransactionScope, animator);
            }

            if (_flags[kViewIncludedInParentFlag]) {
                ++(*currentViewIndex);
            }
            ++(*viewsCount);
        } else {
            if (removeView(viewTransactionScope, false)) {
                viewChanged = true;
                updateResult.destroyedViews++;
            }
        }

        if (needUpdate || viewChanged) {
            int viewIndex = 0;
            int viewsCount = 0;

            if (hasChildWithZIndex()) {
                auto children = sortChildrenByZIndex();
                for (ViewNode* childViewNode : *children) {
                    childViewNode->doUpdateViewTree(viewTransactionScope,
                                                    _view,
                                                    visibleInViewport,
                                                    limitToViewportEnabled,
                                                    viewChanged,
                                                    viewInflationEnabled,
                                                    animator,
                                                    updateResult,
                                                    &viewIndex,
                                                    &viewsCount);
                }
            } else {
                for (ViewNode* childViewNode : *this) {
                    childViewNode->doUpdateViewTree(viewTransactionScope,
                                                    _view,
                                                    visibleInViewport,
                                                    limitToViewportEnabled,
                                                    viewChanged,
                                                    viewInflationEnabled,
                                                    animator,
                                                    updateResult,
                                                    &viewIndex,
                                                    &viewsCount);
                }
            }
            _numberOfViewChildrenInsertedInTree = viewIndex;
            _numberOfViewChildren = viewsCount;
        }
    }
}

size_t ViewNode::getRecursiveChildCount() const {
    size_t count = 0;

    for (const auto* child : *this) {
        count += 1 + child->getRecursiveChildCount();
    }

    return count;
}

ViewNodeUpdateViewTreeResult ViewNode::updateViewTree(ViewTransactionScope& viewTransactionScope) {
    // Here we search the closest View in which we can start updating the view tree.
    auto parent = getParent();

    if (parent != nullptr) {
        // We need to start updating the tree from the first ViewNode that has a view, to have proper
        // view indexes for the Layout nodes.
        return parent->updateViewTree(viewTransactionScope);
    }

    ViewNodeUpdateViewTreeResult updateResult;

    if (!viewTreeNeedsUpdate()) {
        return updateResult;
    }

    snap::utils::time::StopWatch sw;
    sw.start();

    VALDI_TRACE("Valdi.updateViewTree");

    auto viewIndex = 0;
    auto viewsCount = 0;

    auto viewInflationEnabled = _viewNodeTree == nullptr || _viewNodeTree->isViewInflationEnabled();

    doUpdateViewTree(viewTransactionScope,
                     nullptr,
                     /* parentVisibleInViewport */ true,
                     /* limitToViewportEnabled */ true,
                     false,
                     viewInflationEnabled,
                     nullptr,
                     updateResult,
                     &viewIndex,
                     &viewsCount);

    if (updateResult.visitedNodes > 0 && Valdi::traceRenderingPerformance) {
        VALDI_INFO(getLogger(),
                   "Update view tree: visited {} nodes in {}, total {} nodes, created {} views, destroyed {} views, "
                   "reinserted {} views",
                   updateResult.visitedNodes,
                   sw.elapsed(),
                   getRecursiveChildCount(),
                   updateResult.createdViews,
                   updateResult.destroyedViews,
                   updateResult.reinsertedViews);
    }

    return updateResult;
}

void ViewNode::updateVisibilityAndPerformUpdates(ViewTransactionScope& viewTransactionScope) {
    Frame viewport;

    auto parent = getParent();
    auto parentIsVisible = false;
    if (parent != nullptr) {
        viewport = parent->getCalculatedViewport();
        parentIsVisible = parent->isVisibleInViewport();
    } else {
        std::optional<Frame> parentViewport;
        if (_viewNodeTree != nullptr) {
            parentViewport = _viewNodeTree->getViewport();
        }
        if (parentViewport) {
            viewport = *parentViewport;
        } else {
            viewport = Frame(0, 0, getCalculatedFrame().width, getCalculatedFrame().height);
        }
        parentIsVisible = true;
    }

    auto visibilityChanged = false;

    while (true) {
        VALDI_TRACE("Valdi.updateVisibility");
        int visitedNodes = 0;
        snap::utils::time::StopWatch sw;
        sw.start();

        visibilityChanged |= doUpdateVisibility(viewport, true, parentIsVisible, visitedNodes);

        if (Valdi::traceRenderingPerformance) {
            VALDI_INFO(getLogger(), "Update ViewNode visibility: visited {} nodes in {}", visitedNodes, sw.elapsed());
        }

        if (isLazyLayoutDirty() && !isFlexLayoutDirty()) {
            layoutFinished(viewTransactionScope, false);
        } else {
            break;
        }
    }

    updateViewTree(viewTransactionScope);

    if (visibilityChanged) {
        _viewNodeTree->flushVisibilityObservers();
    }
}

bool ViewNode::doUpdateVisibility(const Valdi::Frame& viewport,
                                  bool parentHasChanged,
                                  bool parentIsVisible,
                                  int& visitedNodes) {
    visitedNodes++;

    auto needsUpdate = calculatedViewportNeedsUpdate() || parentHasChanged;
    auto childrenNeedsUpdate = calculatedViewportHasChildNeedsUpdate();
    auto viewportChanged = false;
    auto changed = false;

    if (needsUpdate) {
        Frame clipRect;

        auto visibleInViewport = parentIsVisible;

        // Only bother calculating the viewport if the parent is visible,
        // otherwise viewport is assumed to be empty frame.
        if (parentIsVisible) {
            auto bounds = calculateSelfViewport();

            if (VALDI_UNLIKELY(ignoreParentViewport())) {
                clipRect = bounds;
            } else {
                visibleInViewport = bounds.intersects(viewport);
                if (visibleInViewport) {
                    clipRect = bounds.intersection(viewport);
                }
            }

            if (visibleInViewport) {
                clipRect.x -= bounds.x;
                clipRect.y -= bounds.y;

                // Reverse-map the clip rect from visual (scaled) space back into
                // the local coordinate space so children see the correct visible
                // range. Negative scale also flips the axis; positive scale != 1
                // only compresses/expands without flipping.
                if (_scaleX < 0.0f) {
                    const float absScaleX = -_scaleX;
                    clipRect.x = (bounds.width - (clipRect.x + clipRect.width)) / absScaleX;
                    clipRect.width = clipRect.width / absScaleX;
                } else if (_scaleX > 0.0f && _scaleX != 1.0f) {
                    clipRect.x /= _scaleX;
                    clipRect.width /= _scaleX;
                }
                if (_scaleY < 0.0f) {
                    const float absScaleY = -_scaleY;
                    clipRect.y = (bounds.height - (clipRect.y + clipRect.height)) / absScaleY;
                    clipRect.height = clipRect.height / absScaleY;
                } else if (_scaleY > 0.0f && _scaleY != 1.0f) {
                    clipRect.y /= _scaleY;
                    clipRect.height /= _scaleY;
                }

                if (_scrollState != nullptr) {
                    _scrollState->resolveClipRect(clipRect);
                }
            }
        }

        changed = setVisibleInViewport(visibleInViewport);

        if (changed) {
            if (shouldReceiveVisibilityUpdates()) {
                auto* visibilityObserver = _viewNodeTree->getViewNodesVisibilityObserver();
                if (visibilityObserver != nullptr) {
                    visibilityObserver->onViewNodeVisibilityChanged(_rawId, visibleInViewport);
                }
            }
        }

        if (clipRect != getCalculatedViewport()) {
            setCalculatedViewport(clipRect);
            // if the viewport changed, we need to re-calculate all viewports for the children.
            childrenNeedsUpdate = true;
            viewportChanged = true;
            changed = true;

            if (shouldReceiveVisibilityUpdates()) {
                auto* visibilityObserver = _viewNodeTree->getViewNodesVisibilityObserver();
                if (visibilityObserver != nullptr) {
                    visibilityObserver->onViewNodeViewportChanged(_rawId, clipRect);
                }
            }
        }
    }

    if (childrenNeedsUpdate) {
        auto isVisible = isVisibleInViewport();

        if (_childrenIndexer != nullptr) {
            // Use our childrenIndexer if we have one, which will efficiently find the elements which
            // have become visible or invisible
            auto result = _childrenIndexer->findChildrenVisibility(getCalculatedViewport());
            for (auto* childViewNode : *result.invisibleChildren) {
                changed |=
                    childViewNode->doUpdateVisibility(getCalculatedViewport(), viewportChanged, false, visitedNodes);
            }
            for (auto* childViewNode : *result.visibleChildren) {
                changed |= childViewNode->doUpdateVisibility(
                    getCalculatedViewport(), viewportChanged, isVisible, visitedNodes);
            }
        } else {
            // Without childrenIndexer, just go over all our children
            for (ViewNode* childViewNode : *this) {
                changed |= childViewNode->doUpdateVisibility(
                    getCalculatedViewport(), viewportChanged, isVisible, visitedNodes);
            };
        }
    }

    _flags[kCalculatedViewportNeedsUpdateFlag] = false;
    _flags[kCalculatedViewportHasChildNeedsUpdateFlag] = false;

    return changed;
}

void ViewNode::setLimitToViewport(LimitToViewport limitToViewport) {
    switch (limitToViewport) {
        case LimitToViewportUnset:
            _flags[kLimitToViewportEnabledFlag] = false;
            _flags[kLimitToViewportDisabledFlag] = false;
            break;
        case LimitToViewportEnabled:
            _flags[kLimitToViewportEnabledFlag] = true;
            _flags[kLimitToViewportDisabledFlag] = false;
            break;
        case LimitToViewportDisabled:
            _flags[kLimitToViewportEnabledFlag] = false;
            _flags[kLimitToViewportDisabledFlag] = true;
            break;
    }
}

LimitToViewport ViewNode::limitToViewport() const {
    if (_flags[kLimitToViewportEnabledFlag]) {
        return LimitToViewportEnabled;
    } else if (_flags[kLimitToViewportDisabledFlag]) {
        return LimitToViewportDisabled;
    } else {
        return LimitToViewportUnset;
    }
}

SharedViewNode ViewNode::getRoot() {
    auto root = strongSmallRef(this);
    while (true) {
        auto parent = root->getParent();
        if (parent == nullptr) {
            return root;
        }
        root = std::move(parent);
    }
}

void ViewNode::setScrollContentOffset(ViewTransactionScope& viewTransactionScope,
                                      const Point& directionAgnosticContentOffset,
                                      bool animated) {
    VALDI_TRACE("Valdi.setScrollContentOffset");
    auto& scrollState = getOrCreateScrollState();

    // We can skip contentOffset change this if we are already settled at the correct contentOffset
    bool isScrollCircularRotating = scrollState.getCircularRatio() > 1;
    bool isScrollCurrentlyAnimating = scrollState.isCurrentlyAnimating();
    bool isScrollContentOffsetChanging =
        directionAgnosticContentOffset != scrollState.getDirectionAgnosticContentOffset();
    if (!isScrollCircularRotating && !isScrollCurrentlyAnimating && !isScrollContentOffsetChanging) {
        return;
    }

    // We don't yet support for animating contentOffset when the view is not already created
    if (!hasView()) {
        animated = false;
    }

    if (animated) {
        // When animating contentOffset
        // We mark the scroll state as currently animating,
        // to make sure any future change cancel the current animation
        scrollState.setCurrentlyAnimating(true);
        // Then we update the scroll specs with an animation
        // and we let the backing view notify us on each animation frame when the content
        // offset is changing
        setScrollSpecsOnView(viewTransactionScope, scrollState, directionAgnosticContentOffset, true);
    } else {
        // When immediately updating the contentOffset
        // We change the scroll state directly
        scrollState.updateDirectionAgnosticContentOffset(directionAgnosticContentOffset,
                                                         directionAgnosticContentOffset);
        // We forcefully mark the last animation as finished
        scrollState.setCurrentlyAnimating(false);
        // Then propagate the information to the platform view through scrollSpecs
        // (to cancel any pending animation and update the visual contentOffset)
        scrollState.setNeedsSyncWithView(true);
        syncScrollSpecsWithViewIfNeeded(viewTransactionScope);
        // Then we manually trigger the onScroll+onScrollEnd manually if the contentOffset has changed since last time
        if (isScrollCircularRotating || isScrollContentOffsetChanging) {
            auto directionDependentContentOffset = scrollState.getDirectionDependentContentOffset();
            handleOnScroll(directionDependentContentOffset, directionDependentContentOffset, Point(0, 0));
            onScrollEnd(directionDependentContentOffset, directionDependentContentOffset);
        }
    }
}

Point ViewNode::getDirectionAgnosticScrollContentOffset() const {
    if (_scrollState == nullptr) {
        return Point();
    }

    return _scrollState->getDirectionAgnosticContentOffset();
}

namespace {

// Max depth to descend/search the scroll subtree when (re)finding the preserve anchor.
constexpr int kPreserveAnchorMaxDepth = 12;

// Absolute Y of `node` relative to `scrollNode` (sum of Yoga tops up the parent chain).
float preserveAnchorAbsoluteTop(ViewNode* scrollNode, ViewNode* node) {
    // A node may have no Yoga node (e.g. not-yet-laid-out lazy rows); treat its offset as 0.
    float y = node->getYogaNode() ? sanitizeYogaValue(YGNodeLayoutGetTop(node->getYogaNode())) : 0.0f;
    for (auto* parent = node->getParent().get(); parent != nullptr && parent != scrollNode;
         parent = parent->getParent().get()) {
        if (parent->getYogaNode()) {
            y += sanitizeYogaValue(YGNodeLayoutGetTop(parent->getYogaNode()));
        }
    }
    return y;
}

// Descend to the deepest node that straddles `probeY` (a content-space Y). Callers probe the
// viewport CENTER, not its leading edge: the center is always real message content, whereas the
// leading edge at the newest/oldest end sits on chrome (top padding, loading spinners, whitespace)
// pinned to the content edge, which doesn't move when messages are inserted -- a useless anchor.
// We pick by position (inverted lists are column-reverse) and descend to a leaf-sized node.
ViewNode* preserveDescendantAtY(ViewNode* scrollNode, float probeY) {
    ViewNode* anchor = nullptr;
    ViewNode* node = scrollNode;
    // Carry the running absolute top of `node` down the descent so a child's absolute top is just
    // nodeTop + its own Yoga top -- no per-child walk back up to scrollNode (this runs per scroll
    // frame). scrollNode is the origin, so its children's tops are relative to it (nodeTop = 0).
    float nodeTop = 0.0f;
    for (int depth = 0; depth < kPreserveAnchorMaxDepth; depth++) {
        ViewNode* next = nullptr;
        float nextTop = 0.0f;
        float nearestBelowTop = 0.0f;
        bool haveNearestBelow = false;
        for (auto* child : *node) {
            // Skip children without a layout node -- no position/size to anchor on.
            if (!child->getYogaNode()) {
                continue;
            }
            float top = nodeTop + sanitizeYogaValue(YGNodeLayoutGetTop(child->getYogaNode()));
            float height = sanitizeYogaValue(YGNodeLayoutGetHeight(child->getYogaNode()));
            if (probeY >= top && probeY < top + height) {
                next = child;
                nextTop = top;
                break;
            }
            if (top >= probeY && (!haveNearestBelow || top < nearestBelowTop)) {
                next = child;
                nextTop = top;
                nearestBelowTop = top;
                haveNearestBelow = true;
            }
        }
        if (!next)
            break;
        // Track the deepest node that has a trackable (non-zero) rawId. If the deepest node is
        // id-less we fall back to this ancestor rather than returning an unanchorable node, which
        // would clear the anchor and let the next reflow jump.
        if (next->getRawId() != 0) {
            anchor = next;
        }
        node = next;
        nodeTop = nextTop;
    }
    return anchor;
}

ViewNode* preserveFindDescendantById(ViewNode* node, RawViewNodeId id, int depth) {
    // id 0 is the unset/default rawId (e.g. synthetic placeholder nodes); never "match" it, or
    // we would anchor to the first id-less node (content top) and jump the viewport.
    if (depth <= 0 || id == 0)
        return nullptr;
    for (auto* child : *node) {
        if (child->getRawId() == id)
            return child;
        auto* found = preserveFindDescendantById(child, id, depth - 1);
        if (found)
            return found;
    }
    return nullptr;
}

// Re-pick the anchor (the node at the viewport center) and remember its id + screen position
// (absolute top relative to the viewport top) so a later content reflow can pin it back. Called
// from both layout (updateScrollState) and scroll (handleOnScroll) so the anchor never goes stale
// between layout passes.
void refreshPreserveAnchor(ViewNode* scrollNode, ViewNodeScrollState& scrollState, float offsetY, float viewportH) {
    float probeY = offsetY + viewportH * 0.5f;
    // Only anchor on a node with a real, trackable rawId. id 0 (unset/default) can't be re-found
    // across passes, so don't record it -- clear instead and re-pick next pass.
    if (auto* anchor = preserveDescendantAtY(scrollNode, probeY); anchor != nullptr && anchor->getRawId() != 0) {
        scrollState.setPreserveAnchor(anchor->getRawId(), preserveAnchorAbsoluteTop(scrollNode, anchor) - offsetY);
    } else {
        scrollState.clearPreserveAnchor();
    }
}

} // namespace

void ViewNode::handleOnScroll(const Point& directionDependentContentOffset,
                              const Point& directionDependentUnclampedContentOffset,
                              const Point& directionDependentVelocity) {
    VALDI_TRACE("Valdi.handleOnScroll");
    auto tree = Ref(getViewNodeTree());

    tree->getContext()->withAttribution([&]() {
        auto disableUpdates = tree->beginDisableUpdates();
        auto& scrollState = getOrCreateScrollState();
        auto shouldUpdateAttributeSync = scrollState.onScrollCallbackPrefersSyncCalls();

        auto directionAgnosticContentOffset =
            scrollState.resolveDirectionAgnosticContentOffset(directionDependentContentOffset);
        auto directionAgnosticUnclampedContentOffset =
            scrollState.resolveDirectionAgnosticContentOffset(directionDependentUnclampedContentOffset);

        updateScrollAttributeValueIfNeeded(
            DefaultAttributeContentOffsetX, directionAgnosticContentOffset.x, shouldUpdateAttributeSync);
        updateScrollAttributeValueIfNeeded(
            DefaultAttributeContentOffsetY, directionAgnosticContentOffset.y, shouldUpdateAttributeSync);

        scrollState.notifyOnScroll(
            directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset, directionDependentVelocity);

        // Keep the preserveScrollPosition anchor in sync with the live scroll position. Layout
        // passes (updateScrollState) only run on relayout, but the offset also moves on scroll and
        // clamp; refreshing here means a later content reflow pins the node the user is actually
        // looking at, not a stale one captured at an old offset.
        if (scrollState.getPreserveScrollPosition()) {
            refreshPreserveAnchor(this, scrollState, directionAgnosticContentOffset.y, _calculatedFrame.height);
        }

        // Sticky headers: apply cached-measurement clamp inside the scroll frame so the
        // transform lands in the same VSYNC as the scroll gesture. No layout thrash.
        updateStickyHeaders(/*refreshCache=*/false);

        setCalculatedViewportNeedsUpdate();
    });
}

std::optional<Point> ViewNode::onScroll(const Point& directionDependentContentOffset,
                                        const Point& directionDependentUnclampedContentOffset,
                                        const Point& directionDependentVelocity) {
    if (isUpdatingScrollSpecs()) {
        return std::nullopt;
    }

    auto backendString = getBackendString(getBackend(_viewNodeTree));
    auto metrics = Metrics::scopedOnScrollLatency(getMetrics(), getModuleName(), backendString);

    auto& scrollState = getOrCreateScrollState();

    if (!scrollState.updateDirectionDependentContentOffset(directionDependentContentOffset,
                                                           directionDependentUnclampedContentOffset)) {
        return std::nullopt;
    }

    auto directionAgnosticVelocity =
        directionAgnosticVelocityFromDirectionDependentVelocity(directionDependentVelocity);
    handleOnScroll(
        directionDependentContentOffset, directionDependentUnclampedContentOffset, directionAgnosticVelocity);

    return {scrollState.getDirectionDependentContentOffset()};
}

void ViewNode::onScrollEnd(const Point& directionDependentContentOffset,
                           const Point& directionDependentUnclampedContentOffset) {
    if (isUpdatingScrollSpecs()) {
        return;
    }

    auto& scrollState = getOrCreateScrollState();

    // Make sure everyone is notified that we are not animating anymore
    scrollState.setCurrentlyAnimating(false);

    // Trigger typescript's onScrollEnd

    auto directionAgnosticContentOffset =
        scrollState.resolveDirectionAgnosticContentOffset(directionDependentContentOffset);
    auto directionAgnosticUnclampedContentOffset =
        scrollState.resolveDirectionAgnosticContentOffset(directionDependentUnclampedContentOffset);

    scrollState.notifyOnScrollEnd(directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset);
}

Point ViewNode::directionAgnosticVelocityFromDirectionDependentVelocity(const Point& directionDependentVelocity) {
    if (isHorizontal()) {
        auto resolvedVelocityX = isRightToLeft() ? directionDependentVelocity.x * -1 : directionDependentVelocity.x;
        return Point(resolvedVelocityX, 0);
    } else {
        return Point(0, directionDependentVelocity.y);
    }
}

void ViewNode::onDragStart(const Point& directionDependentContentOffset,
                           const Point& directionDependentUnclampedContentOffset,
                           const Point& directionDependentVelocity) {
    if (isUpdatingScrollSpecs()) {
        return;
    }

    auto& scrollState = getOrCreateScrollState();

    auto directionAgnosticVelocity =
        directionAgnosticVelocityFromDirectionDependentVelocity(directionDependentVelocity);

    auto directionAgnosticContentOffset =
        scrollState.resolveDirectionAgnosticContentOffset(directionDependentContentOffset);
    auto directionAgnosticUnclampedContentOffset =
        scrollState.resolveDirectionAgnosticContentOffset(directionDependentUnclampedContentOffset);

    scrollState.notifyOnDragStart(
        directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset, directionAgnosticVelocity);
}

std::optional<Point> ViewNode::onDragEnding(const Point& directionDependentContentOffset,
                                            const Point& directionDependentUnclampedContentOffset,
                                            const Point& directionDependentVelocity) {
    if (isUpdatingScrollSpecs()) {
        return std::nullopt;
    }

    auto& scrollState = getOrCreateScrollState();

    auto directionAgnosticContentOffset =
        scrollState.resolveDirectionAgnosticContentOffset(directionDependentContentOffset);
    auto directionAgnosticUnclampedContentOffset =
        scrollState.resolveDirectionAgnosticContentOffset(directionDependentUnclampedContentOffset);

    auto directionAgnosticVelocity =
        directionAgnosticVelocityFromDirectionDependentVelocity(directionDependentVelocity);

    auto result = scrollState.notifyOnDragEnding(
        directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset, directionAgnosticVelocity);
    scrollState.notifyOnDragEnd(
        directionAgnosticContentOffset, directionAgnosticUnclampedContentOffset, directionAgnosticVelocity);
    if (!result) {
        VALDI_ERROR(getLogger(), "Invalid onDragEnding result: {}", result.error());
        return std::nullopt;
    }
    auto directionAgnosticTargetContentOffset = result.moveValue();
    if (!directionAgnosticTargetContentOffset) {
        return std::nullopt;
    }

    return {scrollState.resolveDirectionDependentContentOffset(directionAgnosticTargetContentOffset.value())};
}

float ViewNode::getCanScrollDistance(ScrollDirection scrollDirection) {
    if (_scrollState != nullptr) {
        auto frame = getCalculatedFrame();
        auto directionDependentContentOffset = _scrollState->getDirectionDependentContentOffset();
        auto contentSize = _scrollState->getContentSize();
        switch (simplifyScrollDirection(scrollDirection)) {
            case SimplifiedScrollDirectionTopToBottom:
                return contentSize.height - frame.height - directionDependentContentOffset.y;
            case SimplifiedScrollDirectionBottomToTop:
                return directionDependentContentOffset.y;
            case SimplifiedScrollDirectionLeftToRight:
                return contentSize.width - frame.width - directionDependentContentOffset.x;
            case SimplifiedScrollDirectionRightToLeft:
                return directionDependentContentOffset.x;
        }
    }
    return 0;
}

bool ViewNode::canScroll(const Point& parentDirectionDependentVisual, ScrollDirection scrollDirection) {
    auto frame = getCalculatedFrame();
    if (!frame.contains(parentDirectionDependentVisual)) {
        return false;
    }

    auto simplifiedScrollDirection = simplifyScrollDirection(scrollDirection);

    // manually declared always scrollable elements
    if (_flags[kCanAlwaysScrollHorizontal] && (simplifiedScrollDirection == SimplifiedScrollDirectionLeftToRight ||
                                               simplifiedScrollDirection == SimplifiedScrollDirectionRightToLeft)) {
        return true;
    }
    if (_flags[kCanAlwaysScrollVertical] && (simplifiedScrollDirection == SimplifiedScrollDirectionTopToBottom ||
                                             simplifiedScrollDirection == SimplifiedScrollDirectionBottomToTop)) {
        return true;
    }

    if (getCanScrollDistance(scrollDirection) > 0) {
        return true;
    }
    Point selfDirectionDependentVisual = convertParentVisualToSelfVisual(parentDirectionDependentVisual);
    for (ViewNode* childViewNode : *this) {
        if (childViewNode->canScroll(selfDirectionDependentVisual, scrollDirection)) {
            return true;
        }
    }
    return false;
}

bool ViewNode::isScrollingOrAnimatingScroll() const {
    if (!isInScrollMode()) {
        return false;
    }

    return _scrollState->isScrolling() || _scrollState->isCurrentlyAnimating();
}

void ViewNode::setCanAlwaysScrollHorizontal(bool canAlwaysScrollHorizontal) {
    _flags[kCanAlwaysScrollHorizontal] = canAlwaysScrollHorizontal;
}

void ViewNode::setCanAlwaysScrollVertical(bool canAlwaysScrollVertical) {
    _flags[kCanAlwaysScrollVertical] = canAlwaysScrollVertical;
}

const Frame& ViewNode::getCalculatedViewport() const {
    return _calculatedViewport;
}

void ViewNode::setCalculatedViewport(const Valdi::Frame& calculatedViewport) {
    _calculatedViewport = calculatedViewport;
}

bool ViewNode::isVisibleInViewport() const {
    return _flags[kVisibleInViewportFlag];
}

bool ViewNode::setVisibleInViewport(bool visibleInViewport) {
    if (_flags[kVisibleInViewportFlag] != visibleInViewport) {
        _flags[kVisibleInViewportFlag] = visibleInViewport;
        if (visibleInViewport && _flags[kIsLazyLayoutFlag]) {
            scheduleLazyLayout();
        }

        setViewTreeNeedsUpdate();

        return true;
    } else {
        return false;
    }
}

bool ViewNode::calculatedViewportNeedsUpdate() const {
    return _flags[kCalculatedViewportNeedsUpdateFlag];
}

void ViewNode::setCalculatedViewportNeedsUpdate() {
    if (!_flags[kCalculatedViewportNeedsUpdateFlag]) {
        _flags[kCalculatedViewportNeedsUpdateFlag] = true;

        auto parent = getParent();
        if (parent != nullptr) {
            parent->setCalculatedViewportHasChildNeedsUpdate();
        } else {
            scheduleUpdates();
        }
    }
}

void ViewNode::setCalculatedViewportHasChildNeedsUpdate() {
    if (!_flags[kCalculatedViewportHasChildNeedsUpdateFlag]) {
        _flags[kCalculatedViewportHasChildNeedsUpdateFlag] = true;

        auto parent = getParent();
        if (parent != nullptr) {
            parent->setCalculatedViewportHasChildNeedsUpdate();
        } else {
            scheduleUpdates();
        }
    }
}

bool ViewNode::calculatedViewportHasChildNeedsUpdate() const {
    return _flags[kCalculatedViewportHasChildNeedsUpdateFlag];
}

const Frame& ViewNode::getViewFrame() const {
    return _viewFrame;
}

const Frame& ViewNode::getCalculatedFrame() const {
    return _calculatedFrame;
}

Frame ViewNode::getDirectionAgnosticFrame() const {
    auto frame = getCalculatedFrame();

    auto parent = getParent();
    if (parent != nullptr && parent->isRightToLeft()) {
        frame.x = parent->getChildrenSpaceWidth() - frame.getRight();
    }

    return frame;
}

Point ViewNode::convertPointToRelativeDirectionAgnostic(const Point& directionDependentPoint) const {
    if (isRightToLeft()) {
        return Point(getCalculatedFrame().width - directionDependentPoint.x, directionDependentPoint.y);
    } else {
        return directionDependentPoint;
    }
}

Point ViewNode::convertPointToAbsoluteDirectionAgnostic(const Point& directionDependentPoint) const {
    auto absolutePoint = convertPointToRelativeDirectionAgnostic(directionDependentPoint);
    const auto* current = this;
    while (current != nullptr) {
        auto directionUnawareFrame = current->getDirectionAgnosticFrame();
        absolutePoint.x += directionUnawareFrame.x;
        absolutePoint.y += directionUnawareFrame.y;
        current = current->getParent().get();
    }
    return absolutePoint;
}

Point ViewNode::convertSelfVisualToParentVisual(const Point& selfDirectionDependentVisual) const {
    auto parentDirectionDependentVisual = selfDirectionDependentVisual;
    parentDirectionDependentVisual += getDirectionDependentTransform();
    if (isInScrollMode()) {
        parentDirectionDependentVisual -= _scrollState->getDirectionDependentContentOffset();
    }
    return parentDirectionDependentVisual;
}

Point ViewNode::convertParentVisualToSelfVisual(const Point& parentDirectionDependentVisual) const {
    auto directionDependentVisual = parentDirectionDependentVisual;
    directionDependentVisual -= getDirectionDependentTransform();
    if (isInScrollMode()) {
        directionDependentVisual += _scrollState->getDirectionDependentContentOffset();
    }
    return directionDependentVisual;
}

Point ViewNode::convertSelfVisualToRootVisual(const Point& selfDirectionDependentVisual) const {
    auto rootDirectionDependentVisual = selfDirectionDependentVisual;
    const auto* current = this;
    while (current != nullptr) {
        rootDirectionDependentVisual = current->convertSelfVisualToParentVisual(rootDirectionDependentVisual);
        current = current->getParent().get();
    }
    return rootDirectionDependentVisual;
}

Point ViewNode::getDirectionDependentTransform() const {
    auto directionDependentFrame = getCalculatedFrame();
    return Point(directionDependentFrame.x + getDirectionDependentTranslationX(),
                 directionDependentFrame.y + getTranslationY());
}

Point ViewNode::getDirectionAgnosticTransform() const {
    auto directionAgnosticFrame = getDirectionAgnosticFrame();
    return Point(directionAgnosticFrame.x + getTranslationX(), directionAgnosticFrame.y + getTranslationY());
}

Point ViewNode::getBoundsOriginPoint() const {
    if (isInScrollMode()) {
        return _scrollState->getDirectionDependentContentOffset();
    } else {
        return Point();
    }
}

float ViewNode::getChildrenSpaceWidth() const {
    if (_scrollState == nullptr || !_scrollState->isInScrollMode()) {
        return _calculatedFrame.width;
    }
    return _scrollState->getContentSize().width;
}

float ViewNode::getChildrenSpaceHeight() const {
    if (_scrollState == nullptr || !_scrollState->isInScrollMode()) {
        return _calculatedFrame.height;
    }

    return _scrollState->getContentSize().height;
}

float ViewNode::getRtlScrollOffsetX() const {
    if (_scrollState == nullptr || !_scrollState->isInScrollMode()) {
        return 0;
    }
    return _scrollState->getRtlOffsetX();
}

float ViewNode::getViewOffsetX() const {
    return _viewFrame.x - _calculatedFrame.x;
}

float ViewNode::getViewOffsetY() const {
    return _viewFrame.y - _calculatedFrame.y;
}

const StringBox& ViewNode::getViewClassName() const {
    const auto& boundAttributes = _attributesApplier.getBoundAttributes();
    if (boundAttributes == nullptr) {
        return StringBox::emptyString();
    }

    return boundAttributes->getClassName();
}

void ViewNode::setViewFactory(ViewTransactionScope& viewTransactionScope, const Ref<ViewFactory>& viewFactory) {
    if (_viewFactory != viewFactory) {
        removeView(viewTransactionScope);
        _viewFactory = viewFactory;

        if (viewFactory != nullptr) {
            setIsLayout(viewFactory->getViewClassName() == AttributesManager::getLayoutPlaceholderClassName());
            auto boundAttributes = viewFactory->getBoundAttributes();
            _attributesApplier.setBoundAttributes(boundAttributes);
            _flags[kScrollAttributesBound] =
                (boundAttributes != nullptr && boundAttributes->isBackingClassScrollable());

            if (_flags[kScrollAttributesBound]) {
                getYogaStyle(getYogaNodeForInsertingChildren()).setOverflow(facebook::yoga::Overflow::Scroll);
            }
        } else {
            setIsLayout(true);
            _attributesApplier.setBoundAttributes(nullptr);
        }
    }
}

void ViewNode::setViewClassNameForPlatform(ViewTransactionScope& viewTransactionScope,
                                           const StringBox& viewClassName,
                                           PlatformType platformType) {
    auto currentPlatformType = _viewNodeTree->getViewManagerContext()->getViewManager().getPlatformType();
    if (currentPlatformType != platformType) {
        // macOS and Linux fall through to iOS class names as the base fallback.
        // macosClass / linuxClass overwrite iosClass when explicitly set (bound later).
        bool isFallthrough = ((currentPlatformType == PlatformTypeMacOS || currentPlatformType == PlatformTypeLinux) &&
                              platformType == PlatformTypeIOS) ||
                             // Linux also accepts macosClass as a shared-desktop override.
                             (currentPlatformType == PlatformTypeLinux && platformType == PlatformTypeMacOS);
        if (!isFallthrough) {
            return;
        }
    }

    if (viewClassName.isEmpty()) {
        setViewFactory(viewTransactionScope, nullptr);
    } else {
        setViewFactory(viewTransactionScope, _viewNodeTree->getOrCreateViewFactory(viewClassName));
    }
}

const Ref<ViewFactory>& ViewNode::getViewFactory() const {
    return _viewFactory;
}

float ViewNode::getPointScale() const {
    if (_viewFactory == nullptr) {
        return 0;
    }
    return _viewFactory->getViewManager().getPointScale();
}

void ViewNode::onChildrenChanged() {
    setChildrenIndexerNeedsUpdate();
    setViewTreeNeedsUpdate();
    setAccessibilityTreeNeedsUpdate();
}

void ViewNode::setChildrenIndexerNeedsUpdate() {
    if (_childrenIndexer != nullptr) {
        _childrenIndexer->setNeedsUpdate();
        // Also set this flag to ensure we will visit
        // our node to update the children indexer
        setCalculatedViewportHasChildNeedsUpdate();
    }
}

void ViewNode::removeFromParent(ViewTransactionScope& viewTransactionScope) {
    if (!hasParent()) {
        return;
    }
    auto parent = getParent();

    auto* owner = resolveYogaNode(_yogaNode)->getOwner();
    if (owner != nullptr) {
        YGNodeRemoveChild(owner, _yogaNode);
    }

    removeViewFromParent(viewTransactionScope);

    parent->onChildrenChanged();
    getCSSAttributesManager().setParent(nullptr);
    _parent.reset();
    setHasParent(false);
}

void ViewNode::appendChild(ViewTransactionScope& viewTransactionScope, const Ref<ViewNode>& child) {
    insertChildAt(viewTransactionScope, child, getChildCount());
}

void ViewNode::insertChildAt(ViewTransactionScope& viewTransactionScope, const Ref<ViewNode>& child, size_t index) {
    SC_ASSERT(child.get() != this);

    [[maybe_unused]] auto childHadParent = child->hasParent();
    child->removeFromParent(viewTransactionScope);
    child->_parent = weakRef(this);
    child->setHasParent(true);

    auto* yogaContainer = getYogaNodeForInsertingChildren();
    auto* yogaContainerNode = resolveYogaNode(yogaContainer);
    if (yogaContainerNode->getChildCount() < index) {
        VALDI_ERROR(getLogger(),
                    "Cannot insert child at index {} into container with size {} (child had parent: {})",
                    index,
                    yogaContainerNode->getChildCount(),
                    childHadParent);
        VALDI_ERROR(getLogger(), "Root component path: {}", getViewNodeTree()->getContext()->getPath().toString());
        VALDI_ERROR(getLogger(), "Parent XML: {}", toXML());
        VALDI_ERROR(getLogger(), "Child XML: {}", child->toXML());
        SC_ASSERT_FAIL("Out of bounds insertion");
        return;
    }

    yogaContainerNode->setMeasureFunc(nullptr);
    YGNodeInsertChild(yogaContainer, child->_yogaNode, index);

    child->getCSSAttributesManager().setParent(&getCSSAttributesManager());

    if (child->getCSSAttributesManager().needUpdateCSS()) {
        child->setCSSNeedsUpdate();
    }
    if (child->cssNeedsUpdate()) {
        setCSSHasChildNeedsUpdate();
    }
    if (child->getZIndex() != 0) {
        setHasChildWithZIndex();
    }
    if (child->hasAccessibilityId() || child->isUserSpecifiedView()) {
        setHasChildWithAccessibilityId();
    }
    if (getChildCount() > kMaxChildrenBeforeIndexing && _childrenIndexer == nullptr) {
        _childrenIndexer = std::make_unique<ViewNodeChildrenIndexer>(this);
    }

    setCalculatedViewportHasChildNeedsUpdate();

    onChildrenChanged();
}

void ViewNode::scheduleUpdates() {
    if (_viewNodeTree != nullptr && !hasParent()) {
        _viewNodeTree->onRootViewNodeNeedsUpdate();
    }
}

bool ViewNode::invalidateMeasuredSize() {
    if (_emittingViewNode != nullptr) {
        return _emittingViewNode->invalidateMeasuredSize();
    }

    if (_yogaNode == nullptr) {
        return false;
    }

    if (!facebook::yoga::resolveRef(_yogaNode)->hasMeasureFunc()) {
        return false;
    }

    return markLayoutDirty();
}

bool ViewNode::markLayoutDirty() {
    if (_yogaNode == nullptr) {
        return false;
    }

    if (isFlexLayoutDirty()) {
        return false;
    }

    facebook::yoga::resolveRef(_yogaNode)->markDirtyAndPropagate();
    return true;
}

bool ViewNode::isFlexLayoutDirty() const {
    return resolveYogaNode(_yogaNode)->isDirty();
}

bool ViewNode::isLazyLayoutDirty() const {
    return _flags[kHasLazyLayoutNeedingCalculationFlag];
}

Size ViewNode::onMeasure(float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
    if (_lazyLayoutData != nullptr && _lazyLayoutData->onMeasureCallback != nullptr) {
        VALDI_TRACE("Valdi.onMeasureNode.external");
        return measureExternal(width, widthMode, height, heightMode);
    } else if (_attributesApplier.getBoundAttributes() != nullptr &&
               _attributesApplier.getBoundAttributes()->getMeasureDelegate() != nullptr) {
        VALDI_TRACE("Valdi.onMeasureNode.delegate");
        return _attributesApplier.getBoundAttributes()->getMeasureDelegate()->measure(
            *this, width, widthMode, height, heightMode);
    } else if (_lazyLayoutData != nullptr) {
        return Size(_lazyLayoutData->estimatedWidth, _lazyLayoutData->estimatedHeight);
    } else {
        return Size();
    }
}

Ref<ViewNode> ViewNode::makePlaceholderViewNode(ViewTransactionScope& viewTransactionScope,
                                                const Ref<View>& placeholderView) {
    auto viewNode = Valdi::makeShared<ViewNode>(nullptr, _attributeIds, _logger);
    viewNode->setViewNodeTree(_viewNodeTree);
    viewNode->setViewFactory(viewTransactionScope, _viewFactory);
    viewNode->_emittingViewNode = strongSmallRef(this);
    viewNode->_cssAttributesManager.copyCSSDocument(_cssAttributesManager);
    placeholderView->setCanBeReused(false);

    _attributesApplier.copyViewLayoutAttributes(viewNode->_attributesApplier);

    viewNode->setView(viewTransactionScope, placeholderView, nullptr);
    viewTransactionScope.transaction().moveViewToTree(placeholderView, _viewNodeTree, viewNode.get());

    return viewNode;
}

Result<Ref<ValueMap>> ViewNode::copyProcessedViewLayoutAttributes() {
    return _attributesApplier.copyProcessedViewLayoutAttributes();
}

Size ViewNode::measureExternal(float width, MeasureMode widthMode, float height, MeasureMode heightMode) {
    std::initializer_list<Value> parameters = {Value(static_cast<double>(width)),
                                               Value(static_cast<int32_t>(widthMode)),
                                               Value(static_cast<double>(height)),
                                               Value(static_cast<int32_t>(heightMode))};

    auto retValue = _lazyLayoutData->onMeasureCallback->call(ValueFunctionFlagsCallSync, parameters);
    if (!retValue) {
        VALDI_ERROR(getLogger(), "onMeasure callback returned an error: {}", retValue.error());
        return Size();
    }
    const auto* array = retValue.value().getArray();

    if (array == nullptr || array->size() != 2) {
        VALDI_ERROR(getLogger(),
                    "onMeasure callback should return an array with two values, got {}",
                    retValue.value().toString());
        return Size();
    }

    return Size((*array)[0].toFloat(), (*array)[1].toFloat());
}

bool ViewNode::isMeasurerPlaceholder() const {
    return _emittingViewNode != nullptr;
}

void ViewNode::valueChanged(AttributeId attribute, const Value& value, bool shouldNotifySync) {
    _attributesApplier.updateAttributeWithoutUpdate(attribute, value);

    auto* viewNodeTree = getViewNodeTree();
    if (viewNodeTree != nullptr && viewNodeTree->getContext() != nullptr) {
        viewNodeTree->getContext()->onAttributeChange(
            getRawId(), _attributeIds.getNameForId(attribute), value, shouldNotifySync);
    }
}

void ViewNode::updateScrollAttributeValueIfNeeded(AttributeId attribute, float value, bool shouldNotifySync) {
    if (!_flags[kScrollAttributesBound]) {
        return;
    }

    auto existingValue = _attributesApplier.getResolvedAttributeValue(attribute);
    if (existingValue.isNumber() && existingValue.toFloat() == value) {
        return;
    }
    valueChanged(attribute, Value(static_cast<double>(value)), shouldNotifySync);
}

bool ViewNode::isLayout() const {
    return _flags[kIsLayoutFlag];
}

void ViewNode::setIsLayout(bool isLayout) {
    if (_flags[kIsLayoutFlag] != isLayout) {
        _flags[kIsLayoutFlag] = isLayout;
        setViewTreeNeedsUpdate();

        updateYogaMeasureFunc();
    }
}

bool ViewNode::needsYogaMeasureFunc() const {
    if (!hasParent() || resolveYogaNode(_yogaNode)->getChildCount() > 0) {
        // No measure func if we are the root or if we are a container
        return false;
    }

    if (isLayout()) {
        // If we are a layout, we only use a measure func if we had lazyLayoutData set,
        // which will contain either the onMeasureCallback or the estimatedWidth/estimatedHeight
        return _lazyLayoutData != nullptr;
    } else {
        // If we are a view, we always use a measure func if we are a leaf
        return true;
    }
}

void ViewNode::updateYogaMeasureFunc() {
    if (_yogaNode == nullptr) {
        return;
    }

    auto* node = facebook::yoga::resolveRef(_yogaNode);
    if (needsYogaMeasureFunc()) {
        node->setMeasureFunc(ygMeasureYoga);
    } else {
        node->setMeasureFunc(nullptr);
    }
}

bool ViewNode::isLazyLayout() const {
    return _flags[kIsLazyLayoutFlag];
}

void ViewNode::setIsLazyLayout(ViewTransactionScope& viewTransactionScope, bool isLazyLayout) {
    if (_flags[kIsLazyLayoutFlag] == isLazyLayout) {
        return;
    }

    auto allChildren = this->copyChildren();

    _flags[kIsLazyLayoutFlag] = isLazyLayout;

    if (_lazyLayoutData != nullptr) {
        _lazyLayoutData->destroyNode();
    }

    // Re-insert children, as they will have to be put in a different
    // yoga container.
    for (const auto& childViewNode : allChildren) {
        childViewNode->removeFromParent(viewTransactionScope);
    }

    for (const auto& childViewNode : allChildren) {
        appendChild(viewTransactionScope, childViewNode);
    }
}

void ViewNode::setPrefersLazyLayout(ViewTransactionScope& viewTransactionScope, bool prefersLazyLayout) {
    _flags[kPrefersLazyLayoutFlag] = prefersLazyLayout;
    updateIsLazyLayout(viewTransactionScope);
}

void ViewNode::updateIsLazyLayout(ViewTransactionScope& viewTransactionScope) {
    setIsLazyLayout(viewTransactionScope,
                    _flags[kPrefersLazyLayoutFlag] ||
                        (_lazyLayoutData != nullptr && _lazyLayoutData->onMeasureCallback != nullptr));
}

LazyLayoutData& ViewNode::getOrCreateLazyLayoutData() {
    if (_lazyLayoutData == nullptr) {
        _lazyLayoutData = std::make_unique<LazyLayoutData>();
    }
    return *_lazyLayoutData;
}

YGNode* ViewNode::getLazyLayoutYogaNode() const {
    return _lazyLayoutData != nullptr ? _lazyLayoutData->yogaNode : nullptr;
}

const YGNode* ViewNode::getContainerYogaNode() const {
    if (_lazyLayoutData == nullptr || _lazyLayoutData->yogaNode == nullptr) {
        return _yogaNode;
    } else {
        return _lazyLayoutData->yogaNode;
    }
}

YGNode* ViewNode::getYogaNodeForInsertingChildren() {
    if (!_flags[kIsLazyLayoutFlag]) {
        return _yogaNode;
    }
    auto& lazyLayoutData = getOrCreateLazyLayoutData();
    if (lazyLayoutData.yogaNode == nullptr) {
        lazyLayoutData.yogaNode =
            Yoga::createNode(const_cast<facebook::yoga::Config*>(facebook::yoga::resolveRef(_yogaNode)->getConfig()));
        setupYogaNode(lazyLayoutData.yogaNode, this);
        // First time we are inserting in this lazyLayout, schedule a lazyLayout pass since our node
        // will be already dirty and won't trigger the yoga dirtied callbacks.
        scheduleLazyLayout();
    }

    return lazyLayoutData.yogaNode;
}

size_t ViewNode::getChildCount() const {
    const auto* yogaNode = getContainerYogaNode();
    if (yogaNode == nullptr) {
        return 0;
    }

    return resolveYogaNode(yogaNode)->getChildCount();
}

ViewNode* ViewNode::getChildAt(size_t index) const {
    const auto* yogaNode = getContainerYogaNode();
    SC_ASSERT_NOTNULL(yogaNode);

    auto* childYogaNode = resolveYogaNode(yogaNode)->getChild(index);
    auto* childViewNode = reinterpret_cast<ViewNode*>(Yoga::getAttachedViewNode(childYogaNode));
    return childViewNode;
}

std::string ViewNode::getLayoutDebugDescription() const {
    SC_ASSERT_NOTNULL(_yogaNode);
    return Yoga::nodeToString(_yogaNode);
}

std::string ViewNode::toXML(bool recursive) const {
    std::stringstream ss;
    toXML(ss, 0, recursive);
    return ss.str();
}

void ViewNode::printXML() const {
    VALDI_INFO(getLogger(), "{}", toXML());
}

static void appendIndent(std::ostream& stream, int indent) {
    for (int i = 0; i < indent; i++) {
        stream << "  ";
    }
}

template<typename Key, typename Value>
void appendKeyValue(std::ostream& stream, const Key& key, const Value& value) {
    stream << key << "=\"" << value << "\" ";
}

void ViewNode::toXML(std::ostream& stream, int indent, bool recursive) const {
    appendIndent(stream, indent);
    stream << "<" << getViewClassName() << std::endl;

    appendIndent(stream, indent + 1);
    appendKeyValue(stream, "frame", getCalculatedFrame().toString());
    stream << std::endl;

    appendIndent(stream, indent + 1);
    appendKeyValue(stream, "view", Valdi::Value(getView()).toString());
    stream << std::endl;

    auto dumpedAttributes = _attributesApplier.dumpResolvedAttributes();

    if (!dumpedAttributes->empty()) {
        appendIndent(stream, indent + 1);
        for (const auto& attribute : *dumpedAttributes) {
            appendKeyValue(stream, attribute.first.toStringView(), attribute.second.toString());
        }
        stream << std::endl;
    }

    if (getChildCount() == 0 || !recursive) {
        appendIndent(stream, indent);
        stream << "/>" << std::endl;
    } else {
        for (ViewNode* childViewNode : *this) {
            childViewNode->toXML(stream, indent + 1, recursive);
        }

        appendIndent(stream, indent);
        stream << "</" << getViewClassName() << ">" << std::endl;
    }
}

StringBox ViewNode::getDebugId() const {
    return _cssAttributesManager.getNodeId();
}

const StringBox& ViewNode::getModuleName() const {
    if (_viewNodeTree == nullptr || _viewNodeTree->getContext() == nullptr) {
        return StringBox::emptyString();
    }

    return _viewNodeTree->getContext()->getPath().getResourceId().bundleName;
}

Shared<CSSDocument> ViewNode::getCSSDocument() const {
    return _cssAttributesManager.getCSSDocument();
}

Ref<ValueMap> ViewNode::getDebugDescriptionMap() const {
    auto out = makeShared<ValueMap>();

    static StringBox viewClassKey = STRING_LITERAL("viewClass");
    static StringBox nodeTagKey = STRING_LITERAL("nodeTag");
    static StringBox idKey = STRING_LITERAL("id");
    static StringBox idsKey = STRING_LITERAL("ids");
    static StringBox rawIdKey = STRING_LITERAL("rawId");
    static StringBox moduleNameKey = STRING_LITERAL("moduleName");
    static StringBox documentPathKey = STRING_LITERAL("documentPath");
    static StringBox attributesKey = STRING_LITERAL("attributes");
    static StringBox frameKey = STRING_LITERAL("frame");
    static StringBox childrenKey = STRING_LITERAL("children");

    (*out)[rawIdKey] = Valdi::Value(getRawId());
    (*out)[moduleNameKey] = Valdi::Value(getModuleName());
    (*out)[viewClassKey] = Valdi::Value(getViewClassName());

    (*out)[attributesKey] = Valdi::Value(_attributesApplier.dumpResolvedAttributes());
    (*out)[frameKey] = Valdi::Value(getCalculatedFrame().toMap());

    ValueArrayBuilder children;

    for (ViewNode* child : *this) {
        children.emplace(child->getDebugDescriptionMap());
    }

    (*out)[childrenKey] = Valdi::Value(children.build());

    return out;
}

AttributesApplier& ViewNode::getAttributesApplier() {
    return _attributesApplier;
}

YGNode* ViewNode::getYogaNode() const {
    return _yogaNode;
}

SharedViewNode ViewNode::getParent() const {
    return _parent.lock();
}

bool ViewNode::hasParent() const {
    return _flags[kHasParent];
}

void ViewNode::setHasParent(bool hasParent) {
    if (_flags[kHasParent] != hasParent) {
        _flags[kHasParent] = hasParent;
        updateYogaMeasureFunc();
    }
}

Frame ViewNode::calculateSelfViewport() const {
    auto tx = getDirectionDependentTranslationX();
    auto ty = getTranslationY();
    auto& f = _calculatedFrame;
    Frame bounds;
    if (VALDI_LIKELY(_scaleX == 1.0f && _scaleY == 1.0f)) {
        bounds = f.withOffset(tx, ty);
    } else {
        // Scale is applied around the center anchor point (matching iOS CALayer default
        // anchor of 0.5,0.5 and Android View default pivot at center).
        auto scaledX = f.x + (f.width * (1.0f - _scaleX)) / 2.0f + tx;
        auto scaledY = f.y + (f.height * (1.0f - _scaleY)) / 2.0f + ty;
        bounds = Frame(scaledX, scaledY, f.width * _scaleX, f.height * _scaleY);
        // Normalize to ensure positive dimensions so that Frame::intersects() and
        // setLeft/Right/Top/Bottom all work correctly (negative scale flips sign).
        if (bounds.width < 0.0f) {
            bounds.x += bounds.width;
            bounds.width = -bounds.width;
        }
        if (bounds.height < 0.0f) {
            bounds.y += bounds.height;
            bounds.height = -bounds.height;
        }
    }
    if (VALDI_UNLIKELY(extendViewportWithChildren())) {
        for (auto* child : *this) {
            auto childBounds = child->calculateSelfViewport();

            bounds.setLeft(std::min(bounds.x, childBounds.x));
            bounds.setTop(std::min(bounds.y, childBounds.y));
            bounds.setRight(std::max(bounds.getRight(), childBounds.getRight()));
            bounds.setBottom(std::max(bounds.getBottom(), childBounds.getBottom()));
        }
    }
    return bounds;
}

Frame ViewNode::computeVisualFrameInRoot() const {
    auto selfFrame = getCalculatedFrame();
    auto rootVisual = convertSelfVisualToRootVisual(Point(0, 0));
    return Frame(rootVisual.x, rootVisual.y, selfFrame.width, selfFrame.height);
}

void ViewNode::ensureFrameIsVisibleWithinParentScrolls(ViewTransactionScope& viewTransactionScope,
                                                       bool animated) const {
    auto selfFrame = getCalculatedFrame();
    auto directionAgnosticVisual = Point(0, 0);
    auto size = Size(selfFrame.width, selfFrame.height);
    ensureFrameIsVisibleWithinParentScrolls(viewTransactionScope, directionAgnosticVisual, size, animated);
}

void ViewNode::ensureFrameIsVisibleWithinParentScrolls(ViewTransactionScope& viewTransactionScope,
                                                       Point directionAgnosticVisual,
                                                       const Size& size,
                                                       bool animated) const {
    // No parent, can't move it
    if (!hasParent()) {
        return;
    }
    // Where are we in the parent's content
    directionAgnosticVisual += getDirectionAgnosticTransform();
    // If we have a scrollable parent, try to move the content offset if needed
    auto parent = getParent();
    auto parentFrame = parent->getCalculatedFrame();
    auto& parentScrollState = parent->_scrollState;
    if (parentScrollState != nullptr) {
        // What's the current occlusion in this scroll view
        auto directionAgnosticContentOffset = parentScrollState->getDirectionAgnosticContentOffset();
        directionAgnosticVisual -= directionAgnosticContentOffset;
        // Compute the displacement needed to fit the frame into the scroll content viewport
        auto directionAgnosticDisplacement = Point(0, 0);
        if (directionAgnosticVisual.x < 0) {
            directionAgnosticDisplacement.x = directionAgnosticVisual.x;
        } else if (directionAgnosticVisual.x + size.width > parentFrame.width) {
            directionAgnosticDisplacement.x = directionAgnosticVisual.x + size.width - parentFrame.width;
        }
        if (directionAgnosticVisual.y < 0) {
            directionAgnosticDisplacement.y = directionAgnosticVisual.y;
        } else if (directionAgnosticVisual.y + size.height > parentFrame.height) {
            directionAgnosticDisplacement.y = directionAgnosticVisual.y + size.height - parentFrame.height;
        }
        // Apply the displacement
        parent->setScrollContentOffset(
            viewTransactionScope, directionAgnosticContentOffset + directionAgnosticDisplacement, animated);
        // Make sure the scrollview is visible also
        parent->ensureFrameIsVisibleWithinParentScrolls(viewTransactionScope, animated);
    } else {
        // Make sure we're also visible in parent's parents
        parent->ensureFrameIsVisibleWithinParentScrolls(viewTransactionScope, directionAgnosticVisual, size, animated);
    }
}

bool ViewNode::scrollByOnePage(ViewTransactionScope& viewTransactionScope,
                               ScrollDirection scrollDirection,
                               bool animated,
                               float* outScrollPageCurrent,
                               float* outScrollPageMaximum) {
    float canScrollDistance = getCanScrollDistance(scrollDirection);
    if (canScrollDistance > 0 && _scrollState != nullptr) {
        auto frame = getCalculatedFrame();

        auto directionDependentContentDisplacement = Point(0, 0);
        switch (simplifyScrollDirection(scrollDirection)) {
            case SimplifiedScrollDirectionTopToBottom:
                directionDependentContentDisplacement.y = std::min(frame.height, canScrollDistance);
                break;
            case SimplifiedScrollDirectionBottomToTop:
                directionDependentContentDisplacement.y = -std::min(frame.height, canScrollDistance);
                break;
            case SimplifiedScrollDirectionLeftToRight:
                directionDependentContentDisplacement.x = std::min(frame.width, canScrollDistance);
                break;
            case SimplifiedScrollDirectionRightToLeft:
                directionDependentContentDisplacement.x = -std::min(frame.width, canScrollDistance);
                break;
        }

        auto directionDependentContentOffset = _scrollState->getDirectionDependentContentOffset();
        directionDependentContentOffset += directionDependentContentDisplacement;

        auto directionAgnosticContentOffset =
            _scrollState->resolveDirectionAgnosticContentOffset(directionDependentContentOffset);
        setScrollContentOffset(viewTransactionScope, directionAgnosticContentOffset, animated);

        if (outScrollPageCurrent != nullptr) {
            *outScrollPageCurrent = getPageNumberForContentOffset(directionAgnosticContentOffset);
        }
        if (outScrollPageMaximum != nullptr) {
            auto contentSize = _scrollState->getContentSize();
            auto directionAgnosticContentMaximum =
                Point(contentSize.width - frame.width, contentSize.height - frame.height);
            *outScrollPageMaximum = getPageNumberForContentOffset(directionAgnosticContentMaximum);
        }

        return true;
    }

    if (hasParent()) {
        return getParent()->scrollByOnePage(
            viewTransactionScope, scrollDirection, animated, outScrollPageCurrent, outScrollPageMaximum);
    } else {
        return false;
    }
}

SimplifiedScrollDirection ViewNode::simplifyScrollDirection(ScrollDirection scrollDirection) {
    switch (scrollDirection) {
        case ScrollDirectionTopToBottom:
            return SimplifiedScrollDirectionTopToBottom;
        case ScrollDirectionBottomToTop:
            return SimplifiedScrollDirectionBottomToTop;
        case ScrollDirectionLeftToRight:
            return SimplifiedScrollDirectionLeftToRight;
        case ScrollDirectionRightToLeft:
            return SimplifiedScrollDirectionRightToLeft;
        case ScrollDirectionHorizontalForward:
            if (isRightToLeft()) {
                return SimplifiedScrollDirectionRightToLeft;
            } else {
                return SimplifiedScrollDirectionLeftToRight;
            }
        case ScrollDirectionHorizontalBackward:
            if (isRightToLeft()) {
                return SimplifiedScrollDirectionLeftToRight;
            } else {
                return SimplifiedScrollDirectionRightToLeft;
            }
        case ScrollDirectionVerticalForward:
            return SimplifiedScrollDirectionTopToBottom;
        case ScrollDirectionVerticalBackward:
            return SimplifiedScrollDirectionBottomToTop;
        case ScrollDirectionForward:
            if (isHorizontal()) {
                return simplifyScrollDirection(ScrollDirectionHorizontalForward);
            } else {
                return simplifyScrollDirection(ScrollDirectionVerticalForward);
            }
        case ScrollDirectionBackward:
            if (isHorizontal()) {
                return simplifyScrollDirection(ScrollDirectionHorizontalBackward);
            } else {
                return simplifyScrollDirection(ScrollDirectionVerticalBackward);
            }
    }
}

float ViewNode::getPageNumberForContentOffset(const Point& directionAgnosticContentOffset) {
    auto frame = getCalculatedFrame();
    if (isHorizontal()) {
        if (frame.width != 0) {
            return directionAgnosticContentOffset.x / frame.width;
        }
    } else {
        if (frame.height != 0) {
            return directionAgnosticContentOffset.y / frame.height;
        }
    }
    return 0;
}

void ViewNode::scheduleLazyLayout() {
    if (_flags[kHasLazyLayoutNeedingCalculationFlag]) {
        return;
    }

    _flags[kHasLazyLayoutNeedingCalculationFlag] = true;

    auto parent = getParent();
    if (parent != nullptr) {
        parent->scheduleLazyLayout();
    } else {
        scheduleUpdates();
    }
}

struct MeasureMetrics {
    uint32_t totalMeasure = 0;
};

static thread_local MeasureMetrics* currentMeasureMetrics = nullptr;

facebook::yoga::StyleSizeLength resolveYogaValue(float containerSize, facebook::yoga::StyleSizeLength appliedValue) {
    float resolvedValue;

    if (appliedValue.isPoints()) {
        resolvedValue = std::min(appliedValue.value().unwrap(), containerSize);
    } else if (appliedValue.isPercent()) {
        resolvedValue = appliedValue.value().unwrap() * containerSize * 0.01f;
    } else {
        resolvedValue = containerSize;
    }

    return facebook::yoga::StyleSizeLength::points(resolvedValue);
}

static void doCalculateLayoutOnNode(YGNode* yogaNode,
                                    float width,
                                    MeasureMode widthMode,
                                    float height,
                                    MeasureMode heightMode,
                                    LayoutDirection direction,
                                    MeasureMetrics& measureCount) {
    facebook::yoga::Direction yogaDirection;
    switch (direction) {
        case LayoutDirectionLTR:
            yogaDirection = facebook::yoga::Direction::LTR;
            break;
        case LayoutDirectionRTL:
            yogaDirection = facebook::yoga::Direction::RTL;
            break;
    }

    // Backward compatible shim, make sure that we use unspecified if nan is passed
    if (std::isnan(width)) {
        widthMode = MeasureModeUnspecified;
    }
    if (std::isnan(height)) {
        heightMode = MeasureModeUnspecified;
    }

    auto savedMaxWidth = getYogaStyle(yogaNode).maxDimension(facebook::yoga::Dimension::Width);
    auto savedMaxHeight = getYogaStyle(yogaNode).maxDimension(facebook::yoga::Dimension::Height);

    float ownerWidth;
    switch (widthMode) {
        case MeasureMode::MeasureModeUnspecified:
            ownerWidth = YGUndefined;
            break;
        case MeasureModeExactly:
            ownerWidth = width;
            break;
        case MeasureModeAtMost:
            ownerWidth = width;
            getYogaStyle(yogaNode).setMaxDimension(facebook::yoga::Dimension::Width,
                                                   resolveYogaValue(width, savedMaxWidth));
            break;
    }

    float ownerHeight;
    switch (heightMode) {
        case MeasureMode::MeasureModeUnspecified:
            ownerHeight = YGUndefined;
            break;
        case MeasureModeExactly:
            ownerHeight = height;
            break;
        case MeasureModeAtMost:
            ownerHeight = height;
            getYogaStyle(yogaNode).setMaxDimension(facebook::yoga::Dimension::Height,
                                                   resolveYogaValue(height, savedMaxHeight));
            break;
    }

    auto* previousMeasureMetrics = currentMeasureMetrics;
    currentMeasureMetrics = &measureCount;
    facebook::yoga::calculateLayout(facebook::yoga::resolveRef(yogaNode), ownerWidth, ownerHeight, yogaDirection);
    currentMeasureMetrics = previousMeasureMetrics;

    getYogaStyle(yogaNode).setMaxDimension(facebook::yoga::Dimension::Width, savedMaxWidth);
    getYogaStyle(yogaNode).setMaxDimension(facebook::yoga::Dimension::Height, savedMaxHeight);
}

bool ViewNode::calculateLayoutOnNodeIfNeeded(YGNode* yogaNode,
                                             float width,
                                             MeasureMode widthMode,
                                             float height,
                                             MeasureMode heightMode,
                                             LayoutDirection direction,
                                             bool forceLayout,
                                             bool isFromLazyLayout) const {
    if (!resolveYogaNode(yogaNode)->isDirty() && !forceLayout) {
        return false;
    }

    auto backendString = getBackendString(getBackend(_viewNodeTree));
    auto module = getModuleName();

#if VALDI_DEBUG_TREE_UPDATES
    VALDI_TRACE_META("Valdi.calculateLayout", std::to_string(getRecursiveChildCount()));
#else
    VALDI_TRACE("Valdi.calculateLayout");
#endif
    auto metricsObj = getMetrics();
    ScopedMetrics metrics = isFromLazyLayout ?
                                Metrics::scopedCalculateLazyLayoutLatency(metricsObj, module, backendString) :
                                Metrics::scopedCalculateLayoutLatency(metricsObj, module, backendString);

    MeasureMetrics measureCount;
    doCalculateLayoutOnNode(yogaNode, width, widthMode, height, heightMode, direction, measureCount);

    if (measureCount.totalMeasure > 0) {
        if (isFromLazyLayout) {
            if (metricsObj != nullptr) {
                metricsObj->emitCalculateLazyLayoutLatencyMeasure(module, backendString, metrics.elapsed());
            }
        } else {
            if (metricsObj != nullptr) {
                metricsObj->emitCalculateLayoutLatencyMeasure(module, backendString, metrics.elapsed());
            }
        }
    }

    if (Valdi::traceRenderingPerformance) {
        VALDI_INFO(getLogger(), "Calculated layout in {} from node {}", metrics.elapsed(), this->getDebugId());
    }
    return true;
}

void ViewNode::performLayout(ViewTransactionScope& viewTransactionScope, Size size, LayoutDirection direction) {
    _flags[kCalculatingLayoutFlag] = true;
    // snap::utils::time::StopWatch sw;
    // sw.start();
    calculateLayoutOnNodeIfNeeded(_yogaNode,
                                  size.width,
                                  MeasureModeExactly,
                                  size.height,
                                  MeasureModeExactly,
                                  direction,
                                  /* forceLayout */ true,
                                  /* isFromLazyLayout */ false);
    layoutFinished(viewTransactionScope, true);
    // VALDI_INFO(getLogger(),
    //               "{} - PERFORM LAYOUT WITH SIZE {}x{} in {}",
    //               _viewNodeTree->getContext()->getPath().toString(),
    //               size.width,
    //               size.height,
    //               sw.elapsed());
    _flags[kCalculatingLayoutFlag] = false;
}

Size ViewNode::measureLayout(
    float width, MeasureMode widthMode, float height, MeasureMode heightMode, LayoutDirection direction) {
    if (!_flags[kCalculatingLayoutFlag]) {
        // snap::utils::time::StopWatch sw;
        // sw.start();

        calculateLayoutOnNodeIfNeeded(_yogaNode,
                                      width,
                                      widthMode,
                                      height,
                                      heightMode,
                                      direction,
                                      /* forceLayout */ true,
                                      /* isFromLazyLayout */ false);
        // VALDI_INFO(getLogger(),
        //               "{} - MEASURE LAYOUT WITH MAX SIZE {}x{} in {}",
        //               _viewNodeTree->getContext()->getPath().toString(),
        //               maxSize.width,
        //               maxSize.height,
        //               sw.elapsed());
        return ygNodeGetFrame(_yogaNode, 0).size();
    } else {
        VALDI_ERROR(
            _logger,
            "Attempting to calculate a layout while a layout is in progress. Will return last calculated frame");
        return _calculatedFrame.size();
    }
}

void ViewNode::layoutFinished(ViewTransactionScope& viewTransactionScope, bool didPerformLayout) {
    ViewNodesFrameObserver* frameObserver = nullptr;
    if (_viewNodeTree != nullptr) {
        frameObserver = _viewNodeTree->getViewNodesFrameObserver();
    }

    float rtlOffsetX = 0.0f;
    ViewNodeChildrenIndexer* parentChildrenIndexer = nullptr;
    auto parent = getParent();
    if (parent != nullptr) {
        rtlOffsetX = parent->getRtlScrollOffsetX();
        parentChildrenIndexer = parent->_childrenIndexer.get();
    }

    VALDI_TRACE("Valdi.updateCalculatedFrames");
    layoutFinished(viewTransactionScope,
                   didPerformLayout,
                   getViewOffsetX(),
                   getViewOffsetY(),
                   rtlOffsetX,
                   nullptr,
                   parentChildrenIndexer,
                   frameObserver);
    if (frameObserver != nullptr) {
        frameObserver->flush();
    }
}

bool ViewNode::isInScrollMode() const {
    return _scrollState != nullptr && _scrollState->isInScrollMode();
}

ViewNodeScrollState& ViewNode::getOrCreateScrollState() {
    if (_scrollState == nullptr) {
        _scrollState = std::make_unique<ViewNodeScrollState>();
    }

    return *_scrollState;
}

void ViewNode::setViewFrameNeedsUpdate() {
    _flags[kNeedsFrameUpdateFlag] = true;
}

bool ViewNode::updateCalculatedFrame(float viewOffsetX,
                                     float viewOffsetY,
                                     float rtlOffsetX,
                                     bool* calculatedFrameDidChange,
                                     bool* calculatedSizeDidChange) {
    auto newFrame = ygNodeGetFrame(_yogaNode, rtlOffsetX);
    auto newViewFrame = newFrame.withOffset(viewOffsetX, viewOffsetY);

    auto hasNewLayout = resolveYogaNode(_yogaNode)->getHasNewLayout();
    auto hadLayout = _flags[kLayoutDidCompleteOnceFlag];
    auto layoutIsRightToLeft = isRightToLeft();
    auto layoutDirectionDidChange = layoutIsRightToLeft != _flags[kLayoutIsRightToLeft];
    if (!hadLayout || _calculatedFrame != newFrame) {
        *calculatedSizeDidChange = _calculatedFrame.size() != newFrame.size();
        *calculatedFrameDidChange = true;
        _calculatedFrame = newFrame;

        setCalculatedViewportNeedsUpdate();
        hasNewLayout = true;
    }

    if (!hadLayout || _viewFrame != newViewFrame || layoutDirectionDidChange) {
        _previousViewFrame = _viewFrame;
        _viewFrame = newViewFrame;
        _flags[kLayoutIsRightToLeft] = layoutIsRightToLeft;

        setViewFrameNeedsUpdate();
        hasNewLayout = true;
    }

    if (!hasNewLayout) {
        return false;
    }

    _flags[kLayoutDidCompleteOnceFlag] = true;

    resolveYogaNode(_yogaNode)->setHasNewLayout(false);

    return true;
}

bool ViewNode::updateLazyLayout() {
    // Using lazy-layout creates a new "detached" yoga subtree, so the device-level RTL/LTR style that
    // we set on the root node doesn't propagate to that detached subtree.
    // Need to manually set the Direction style on the detached subtree.
    auto direction = facebook::yoga::resolveRef(_yogaNode)->getLayout().direction();
    auto previousDirection = facebook::yoga::resolveRef(_lazyLayoutData->yogaNode)->getLayout().direction();
    auto directionHasChanged = direction != previousDirection;
    getYogaStyle(_lazyLayoutData->yogaNode).setDirection(direction);

    auto sizeHasChanged = _lazyLayoutData->availableWidth != _calculatedFrame.width ||
                          _lazyLayoutData->availableHeight != _calculatedFrame.height;

    bool forceLayout = sizeHasChanged || directionHasChanged;
    auto updated = calculateLayoutOnNodeIfNeeded(_lazyLayoutData->yogaNode,
                                                 _calculatedFrame.width,
                                                 MeasureModeExactly,
                                                 _calculatedFrame.height,
                                                 MeasureModeExactly,
                                                 LayoutDirectionLTR /* The direction here doesn't matter since we are
                                                                       setting the direction on the style directly */
                                                 ,
                                                 forceLayout,
                                                 /* isFromLazyLayout */ true);

    _lazyLayoutData->availableWidth = _calculatedFrame.width;
    _lazyLayoutData->availableHeight = _calculatedFrame.height;
    return updated;
}

void ViewNode::updateStickyHeaders(bool refreshCache) {
    if (_scrollState == nullptr || !_scrollState->getNativeStickyEnabled()) {
        return;
    }
    if (_viewNodeTree == nullptr) {
        return;
    }

    float scrollY = _scrollState->getDirectionAgnosticContentOffset().y;
    // Pixels of visual overhang above sticky headers. Extends the effective header
    // height for the clamp so a header rendering a bar above its yoga bounds
    // (SectionList.stickyCover) stops sliding before the next section arrives.
    // Mirrors SectionList.tsx's `headerHeight = getHeaderHeight() + coverHeight`.
    float stickyCover = _scrollState->getNativeStickyCover();
    // Pixels below scroll viewport top where headers pin. Matches CSS `top: N`.
    // Effectively shifts the pin position down so headers aren't clipped behind
    // a floating page header with visual footprint past its Yoga bounds.
    float stickyOffset = _scrollState->getNativeStickyOffset();

    // We push translationY updates through the attribute-set path so both the C++ state
    // and the platform-side transform binder fire in the same frame. withLock acquires
    // the tree mutex (recursive -- safe if already held on the Android scroll path) and
    // opens a ViewTransactionScope. Nested calls piggyback on the outer transaction; a
    // top-level call submits at end-of-scope, pushing transforms in the same VSYNC as
    // the scroll gesture.
    _viewNodeTree->withLock([&]() {
        auto& scope = _viewNodeTree->getCurrentViewTransactionScope();

        // Bounded-depth subtree walk. Header nesting in SubscreenSections is deep:
        // scroll -> PullToRefresh -> SubscreenContent -> paddingRight layout ->
        // SectionList root -> SectionListItem root -> column-reverse layout -> header
        // (~8 levels). 16 covers that plus any consumer wrapping without unbounded cost.
        // Skip into nested scrolls: each scroll owns its own sticky pass.
        static constexpr int kStickyMaxDepth = 16;
        auto* owner = AttributeOwner::getNativeOverridenAttributeOwner();
        std::function<void(ViewNode*, int)> walk = [&](ViewNode* node, int depth) {
            if (depth <= 0) {
                return;
            }
            for (auto* child : *node) {
                if (child->getStickyPosition() == StickyPositionTop) {
                    if (refreshCache) {
                        auto* parent = child->getParent().get();
                        // parent Y relative to this scroll node (sum yoga Top edges up the chain).
                        float parentY = 0.0f;
                        float parentH = 0.0f;
                        if (parent == this) {
                            // Direct child of the scroll: no parent section to clamp against.
                            // Treat the sticky track as unbounded so the header stays pinned once
                            // scrolled past its natural Y, matching CSS `position: sticky`
                            // semantics for elements whose containing block is the scroll itself.
                            parentH = std::numeric_limits<float>::max();
                        } else if (parent != nullptr && parent->getYogaNode() != nullptr) {
                            parentH = sanitizeYogaValue(resolveYogaNode(parent->getYogaNode())
                                                            ->getLayout()
                                                            .dimension(facebook::yoga::Dimension::Height));
                            for (auto* p = parent; p != nullptr && p != this; p = p->getParent().get()) {
                                // Skip logical grouping / lazy-layout wrappers that don't have a
                                // Yoga node -- their child positions are still relative to the
                                // enclosing laid-out ancestor. Matches preserveAnchorAbsoluteTop.
                                if (p->getYogaNode() == nullptr) {
                                    continue;
                                }
                                parentY += sanitizeYogaValue(resolveYogaNode(p->getYogaNode())
                                                                 ->getLayout()
                                                                 .position(facebook::yoga::PhysicalEdge::Top));
                            }
                        }
                        float childH = 0.0f;
                        float childTop = 0.0f;
                        if (child->getYogaNode() != nullptr) {
                            childTop = sanitizeYogaValue(resolveYogaNode(child->getYogaNode())
                                                             ->getLayout()
                                                             .position(facebook::yoga::PhysicalEdge::Top));
                            childH = sanitizeYogaValue(resolveYogaNode(child->getYogaNode())
                                                           ->getLayout()
                                                           .dimension(facebook::yoga::Dimension::Height));
                        }
                        // Absorb child's Top offset within its parent so the sticky trigger
                        // fires when the child (not the parent) reaches the pin position.
                        // For SectionList headers childTop is 0 (via column-reverse), so
                        // this is a no-op there; for any other sticky consumer whose child
                        // isn't at Top=0 of its parent, this makes the clamp behave like
                        // CSS `position: sticky` (per-element trigger) rather than parallax
                        // from the parent's top.
                        child->_stickyCachedParentY = parentY + childTop;
                        child->_stickyCachedParentH =
                            parentH == std::numeric_limits<float>::max() ? parentH : std::max(0.0f, parentH - childTop);
                        child->_stickyCachedChildH = childH;
                    }

                    // displacement = clamp(scrollY - parentY + offset, 0, parentH - (childH + cover))
                    // - `cover` matches JS's `headerHeight = getHeaderHeight() + coverHeight`
                    //   so headers stop sliding before the next section's header arrives.
                    // - `offset` matches CSS `position: sticky; top: N`. Not auto-wired by
                    //   SectionList; consumers can set it directly if their scroll extends
                    //   behind a transparent floating header.
                    //
                    // Behavior: full-overlap. Section N pins at max; Section N+1 rides up
                    // from below at its natural position, arrives at viewport top, and
                    // paints over Section N by later-sibling paint order + zIndex on the
                    // header. User sees clean single-header handoff. Matches
                    // UITableView / UICollectionView pinToVisibleBounds defaults.
                    //
                    // Alternative "push-out" mode (Section N slides above viewport as
                    // Section N+1 arrives) is a v2 opt-in -- see valdi-docs/bcollins RFC.
                    float maxDisplacement =
                        std::max(0.0f, child->_stickyCachedParentH - child->_stickyCachedChildH - stickyCover);
                    float distance = scrollY - child->_stickyCachedParentY + stickyOffset;
                    float displacement = std::max(0.0f, std::min(distance, maxDisplacement));

                    // setAttribute so processAttributeChange fires both the C++ setter
                    // (updates _translationY) and the platform transform binder (Android
                    // View.setTranslationY) in one call. On iOS translationY is a composite
                    // part of transformComposite -- setAttribute only marks the composite
                    // dirty; flush() drains it to apply layer.transform same-frame.
                    child->setAttribute(
                        scope, DefaultAttributeTranslationY, owner, Value(static_cast<double>(displacement)), nullptr);
                    child->getAttributesApplier().flush(scope);
                }
                // Skip recursion into nested scrolls (each owns its own sticky pass), but
                // only AFTER checking stickyPosition above -- so a scroll container tagged
                // stickyPosition='top' (e.g. horizontal tab bar) can act as a sticky header.
                if (child->_flags[kScrollAttributesBound]) {
                    continue;
                }
                walk(child, depth - 1);
            }
        };
        walk(this, kStickyMaxDepth);
    });
}

void ViewNode::updateScrollState() {
    auto& scrollState = getOrCreateScrollState();
    scrollState.setInScrollMode(true);

    auto hadOverflow = resolveYogaNode(_yogaNode)->getLayout().hadOverflow();
    auto* yogaContainer = getYogaNodeForInsertingChildren();
    auto* yogaContainerNode = resolveYogaNode(yogaContainer);

    float rtlOffsetX = 0;

    if (isRightToLeft()) {
        if (hadOverflow && getYogaStyle(_yogaNode).flexDirection() == facebook::yoga::FlexDirection::Row) {
            // On RTL horizontal, we need to adjust the calculated frames as yoga doesn't move the items to the
            // right of the container.
            float lowestLeft = 0;

            for (auto* childYogaNode : yogaContainerNode->getChildren()) {
                auto left = ygNodeGetFrame(childYogaNode,
                                           -sanitizeYogaValue(
                                               childYogaNode->getLayout().margin(facebook::yoga::PhysicalEdge::Left)))
                                .getLeft();

                lowestLeft = std::min(left, lowestLeft);
            }

            lowestLeft -=
                sanitizeYogaValue(resolveYogaNode(_yogaNode)->getLayout().padding(facebook::yoga::PhysicalEdge::Left));

            rtlOffsetX = -lowestLeft;
        } else {
            rtlOffsetX = 0;
        }
    } else {
        rtlOffsetX = 0;
    }

    float highestWidth = 0;
    float highestHeight = 0;

    for (auto* childYogaNode : yogaContainerNode->getChildren()) {
        auto frame = ygNodeGetFrame(childYogaNode, rtlOffsetX);

        auto right = frame.getRight() + childYogaNode->getLayout().margin(facebook::yoga::PhysicalEdge::Right);
        auto bottom = frame.getBottom() + childYogaNode->getLayout().margin(facebook::yoga::PhysicalEdge::Bottom);

        highestWidth = std::max(right, highestWidth);
        highestHeight = std::max(bottom, highestHeight);
    };

    highestWidth += yogaContainerNode->getLayout().padding(facebook::yoga::PhysicalEdge::Right);
    highestHeight += yogaContainerNode->getLayout().padding(facebook::yoga::PhysicalEdge::Bottom);

    auto updateResult = scrollState.updateContentSizeAndRtlOffset(
        Size(highestWidth, highestHeight), _calculatedFrame.size(), rtlOffsetX, getPointScale(), isHorizontal());

    // Scroll anchor: find a descendant with scrollAnchorPosition != 0 and pin the scroll
    // offset so it appears at the specified viewport edge. Only active when maintainScrollAnchor is true.
    if (scrollState.getMaintainScrollAnchor()) {
        float viewportH = _calculatedFrame.height;

        // Find the child with scrollAnchorPosition != 0
        std::function<ViewNode*(ViewNode*, int)> findAnchor = [&](ViewNode* node, int depth) -> ViewNode* {
            if (depth <= 0)
                return nullptr;
            for (auto* child : *node) {
                if (child->getScrollAnchorPosition() != ScrollAnchorPositionNone)
                    return child;
                auto* found = findAnchor(child, depth - 1);
                if (found)
                    return found;
            }
            return nullptr;
        };
        auto* anchorNode = findAnchor(this, 5);

        if (anchorNode) {
            int anchorPosition = anchorNode->getScrollAnchorPosition();

            // Compute anchor's Y relative to this scroll node
            float anchorY = sanitizeYogaValue(
                resolveYogaNode(anchorNode->getYogaNode())->getLayout().position(facebook::yoga::PhysicalEdge::Top));
            for (auto* parent = anchorNode->getParent().get(); parent != nullptr && parent != this;
                 parent = parent->getParent().get()) {
                anchorY += sanitizeYogaValue(
                    resolveYogaNode(parent->getYogaNode())->getLayout().position(facebook::yoga::PhysicalEdge::Top));
            }

            // Pin anchor to the specified viewport edge
            float currentOffset = scrollState.getDirectionAgnosticContentOffset().y;
            float targetOffset;
            if (anchorPosition == ScrollAnchorPositionTop) {
                targetOffset = anchorY;
            } else {
                targetOffset = anchorY - viewportH;
            }

            // Clamp to [0, maxScroll]
            float maxScroll = std::max(highestHeight - viewportH, 0.0f);
            targetOffset = std::max(0.0f, std::min(targetOffset, maxScroll));

            float deltaY = targetOffset - currentOffset;
            if (std::abs(deltaY) > 0.5f) {
                auto currentContentOffset = scrollState.getDirectionAgnosticContentOffset();
                currentContentOffset.y += deltaY;
                scrollState.updateDirectionAgnosticContentOffset(currentContentOffset, currentContentOffset);
                updateResult.changed = true;
                scrollState.setNeedsSyncWithView(true);
            }
        }
    }

    // Preserve scroll position: keep on-screen content fixed by anchoring the first on-screen
    // child and pinning it back to its recorded screen position after a content reflow. The anchor
    // is refreshed on every scroll event (handleOnScroll) and layout pass, so it tracks the live
    // viewport and never goes stale. We only *apply* the pin when the viewport is stationary (not
    // being driven by an active drag or fling), so we don't fight the user's scroll. This replaces
    // a net content-size delta, which was wrong whenever content was added at one end and trimmed
    // at the other in the same pass. (maintainScrollAnchor above is a separate path used by in-game
    // chat; PG drives this one instead.)
    if (scrollState.getPreserveScrollPosition()) {
        float viewportH = _calculatedFrame.height;
        float maxScroll = std::max(highestHeight - viewportH, 0.0f);
        float offsetY = scrollState.getDirectionAgnosticContentOffset().y;
        bool stationary = !scrollState.isScrolling() && !scrollState.isCurrentlyAnimating();

        // 1) Pin the anchor recorded last (its screen position is where the user is looking). Only
        //    while stationary -- mid-scroll the native view owns the offset. We target
        //    newAbsoluteTop - screenPos rather than offset + delta, so a clamp or scroll between
        //    record and now can't double-count.
        if (scrollState.hasPreserveAnchor() && stationary) {
            auto* anchorNode =
                preserveFindDescendantById(this, scrollState.getPreserveAnchorId(), kPreserveAnchorMaxDepth);
            // Only pin if the anchor is still laid out. A recycled/un-laid-out row keeps its rawId
            // but has no Yoga node, so preserveAnchorAbsoluteTop would read 0 and slam the viewport
            // to the content top. Skip and let step 2 re-pick a valid anchor.
            if (anchorNode != nullptr && anchorNode->getYogaNode() != nullptr) {
                float newTop = preserveAnchorAbsoluteTop(this, anchorNode);
                float targetY = std::max(0.0f, std::min(newTop - scrollState.getPreserveAnchorScreenPos(), maxScroll));
                float delta = targetY - offsetY;
                if (std::abs(delta) > 0.5f) {
                    auto offset = scrollState.getDirectionAgnosticContentOffset();
                    offset.y = targetY;
                    scrollState.updateDirectionAgnosticContentOffset(offset, offset);
                    offsetY = targetY;
                    updateResult.changed = true;
                    scrollState.setNeedsSyncWithView(true);
                }
            }
        }

        // 2) Refresh the anchor to the node at the current viewport center.
        refreshPreserveAnchor(this, scrollState, offsetY, viewportH);
    }

    // Sticky headers: refresh per-child measurement cache from the current layout, then
    // reposition. Called on every layout pass to survive content-size changes (section
    // expand/collapse, rotation, keyboard, initial mount).
    updateStickyHeaders(/*refreshCache=*/true);

    if (updateResult.changed) {
        setCalculatedViewportNeedsUpdate();

        if (updateResult.contentOffsetAdjusted) {
            auto directionDependentContentOffset = scrollState.getDirectionDependentContentOffset();
            handleOnScroll(directionDependentContentOffset, directionDependentContentOffset, Point(0, 0));
            onScrollEnd(directionDependentContentOffset, directionDependentContentOffset);
        }
    }
}

void ViewNode::layoutFinished(ViewTransactionScope& viewTransactionScope,
                              bool didPerformLayout,
                              float viewOffsetX,
                              float viewOffsetY,
                              float rtlOffsetX,
                              const SharedAnimator& parentAnimator,
                              ViewNodeChildrenIndexer* parentChildrenIndexer,
                              ViewNodesFrameObserver* frameObserver) {
    auto didPerformLayoutForChildren = didPerformLayout;
    auto shouldVisitChildren = false;
    bool calculatedFrameDidChange = false;
    bool calculatedSizeDidChange = false;

    // Step 1: If we finished a layout pass, we update the calculated frame and the view frame

    if (didPerformLayout) {
        if (updateCalculatedFrame(
                viewOffsetX, viewOffsetY, rtlOffsetX, &calculatedFrameDidChange, &calculatedSizeDidChange)) {
            // Sync the horizontal state with our children indexer
            if (_childrenIndexer != nullptr) {
                _childrenIndexer->setHorizontal(isHorizontal());
            }

            // Step 2: If we are a scroll element, we compute the RTL offset X and the children space,
            // so that the backing scroll view knows how much it can scroll

            if (_flags[kScrollAttributesBound]) {
                updateScrollState();
            } else if (_scrollState != nullptr) {
                _scrollState->setInScrollMode(false);
            }

            // If we had a change in this layout subtree, we need to visit the children since
            // they may have changed
            shouldVisitChildren = true;
        }
    }

    if (_flags[kHasLazyLayoutNeedingCalculationFlag]) {
        _flags[kHasLazyLayoutNeedingCalculationFlag] = false;
        // One of our child has a dirty lazyLayout that needs to be calculated, so we need to visit it
        shouldVisitChildren = true;
    }

    // Step 3: If we are a lazyLayout, we compute the nested layout if we are visible

    if (_flags[kIsLazyLayoutFlag] && getLazyLayoutYogaNode() != nullptr && _flags[kVisibleInViewportFlag]) {
        if (updateLazyLayout()) {
            didPerformLayoutForChildren = true;
            shouldVisitChildren = true;
        } else {
            // If we did not update the nested layout, didPerformLayout should be false for this subtree,
            // unless our frame was updated.
            didPerformLayoutForChildren = shouldVisitChildren;
        }
    }

    if (!shouldVisitChildren) {
        return;
    }

    float childrenViewOffsetX = 0.0f;
    float childrenViewOffsetY = 0.0f;

    if (isLayout()) {
        // If we are a layout node, we won't have a view so we need to compute the offset
        childrenViewOffsetX = viewOffsetX + _calculatedFrame.x;
        childrenViewOffsetY = viewOffsetY + _calculatedFrame.y;
    } else if (!hasParent()) {
        // If we are the root, we use the calculated x,y as the initial offset
        // since the root view frame is fixed in platform. If our flexbox calculation
        // had a non zero x or y for the root node, we wouldn't be able to apply it to
        // the backing view, but we can at least offset the children so that they appear
        // at the right position. This allows the use of the margin attributes on root nodes.
        childrenViewOffsetX = _calculatedFrame.x;
        childrenViewOffsetY = _calculatedFrame.y;
    }

    float childrenRtlOffsetX = isInScrollMode() ? getOrCreateScrollState().getRtlOffsetX() : 0.0f;
    const auto& resolvedAnimator = resolveAnimator(parentAnimator);

    if (_onLayoutCompletedCallback != nullptr && frameObserver != nullptr) {
        frameObserver->appendCompleteCallback(_onLayoutCompletedCallback);
    }

    auto* childrenIndexer = _childrenIndexer.get();
    for (auto* childViewNode : *this) {
        childViewNode->layoutFinished(viewTransactionScope,
                                      didPerformLayoutForChildren,
                                      childrenViewOffsetX,
                                      childrenViewOffsetY,
                                      childrenRtlOffsetX,
                                      resolvedAnimator,
                                      childrenIndexer,
                                      frameObserver);
    }

    syncScrollSpecsWithViewIfNeeded(viewTransactionScope);
    updateViewFrameIfNeeded(viewTransactionScope, resolvedAnimator);

    if (calculatedFrameDidChange) {
        if (calculatedSizeDidChange) {
            setChildrenIndexerNeedsUpdate();
        }

        if (parentChildrenIndexer != nullptr) {
            parentChildrenIndexer->setNeedsUpdate();
        }

        if (frameObserver != nullptr) {
            frameObserver->onFrameChanged(_rawId, getDirectionAgnosticFrame());
        }
    }
}

bool ViewNode::updateViewFrameIfNeeded(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator) {
    if (!_flags[kNeedsFrameUpdateFlag] || !hasView()) {
        return false;
    }
    _flags[kNeedsFrameUpdateFlag] = false;
    if (animator != nullptr && !_previousViewFrame.isEmpty()) {
        // We are animating from destroyed to non destroyed. Apply previous frame so that we can animate
        // from it.
        applyFrame(viewTransactionScope, nullptr, _previousViewFrame, false);
        // Apply new frame with the animation.
        applyFrame(viewTransactionScope, animator, getViewFrame(), true);
    } else {
        // Apply frame without animation
        applyFrame(viewTransactionScope, nullptr, getViewFrame(), true);
    }

    return true;
}

void ViewNode::syncScrollSpecsWithViewIfNeeded(ViewTransactionScope& viewTransactionScope) {
    if (!hasView() || !isInScrollMode()) {
        return;
    }

    auto& scrollState = getOrCreateScrollState();
    if (!scrollState.needsSyncWithView()) {
        return;
    }

    scrollState.setNeedsSyncWithView(false);

    setScrollSpecsOnView(viewTransactionScope, scrollState, scrollState.getDirectionAgnosticContentOffset(), false);
}

void ViewNode::setScrollSpecsOnView(ViewTransactionScope& viewTransactionScope,
                                    ViewNodeScrollState& scrollState,
                                    const Point& directionAgnosticContentOffset,
                                    bool animated) {
    if (!hasView()) {
        return;
    }

    setUpdatingScrollSpecs(true);

    auto directionDependentContentOffset =
        scrollState.resolveDirectionDependentContentOffset(directionAgnosticContentOffset);
    auto contentSize = scrollState.getContentSize();

    viewTransactionScope.transaction().setViewScrollSpecs(
        getView(), directionDependentContentOffset, contentSize, animated);
    setUpdatingScrollSpecs(false);
}

void ViewNode::setUpdatingScrollSpecs(bool updatingScrollSpecs) {
    _flags[kUpdatingScrollSpecs] = updatingScrollSpecs;
}

bool ViewNode::isUpdatingScrollSpecs() const {
    return _flags[kUpdatingScrollSpecs];
}

void ViewNode::applyFrame(ViewTransactionScope& viewTransactionScope,
                          const SharedAnimator& animator,
                          const Frame& frame,
                          bool notifyAssetHandler) const {
    if (!hasParent() || !hasView()) {
        // Root node doesn't need to call onViewFrameChanged.
        // Its frame is driven by native code.
        return;
    }

    viewTransactionScope.transaction().setViewFrame(getView(), frame, _flags[kLayoutIsRightToLeft], animator);

    if (notifyAssetHandler && _assetHandler != nullptr) {
        _assetHandler->onContainerSizeChanged(frame.width, frame.height);
    }
}

bool ViewNode::handleCSSChange(bool cssChanged) {
    if (cssChanged) {
        setCSSNeedsUpdate();
    }
    return cssChanged;
}

bool ViewNode::isHorizontal() {
    auto flexDirection = getYogaStyle(getYogaNodeForInsertingChildren()).flexDirection();
    return flexDirection == facebook::yoga::FlexDirection::Row ||
           flexDirection == facebook::yoga::FlexDirection::RowReverse;
}

bool ViewNode::setAttribute(ViewTransactionScope& viewTransactionScope,
                            AttributeId attributeId,
                            const AttributeOwner* attributeOwner,
                            const Value& attributeValue,
                            const Ref<Animator>& animator) {
    return setAttribute(viewTransactionScope, attributeId, attributeOwner, attributeValue, animator, false);
}

bool ViewNode::setAttribute(ViewTransactionScope& viewTransactionScope,
                            AttributeId attributeId,
                            const AttributeOwner* attributeOwner,
                            const Value& attributeValue,
                            const Ref<Animator>& animator,
                            bool isOverridenFromParent) {
    if (attributeId == DefaultAttributeId) {
        return handleCSSChange(
            getCSSAttributesManager().setElementId(attributeValue.toStringBox(), isOverridenFromParent));
    } else if (attributeId == DefaultAttributeElementTag) {
        return handleCSSChange(
            getCSSAttributesManager().setElementTag(attributeValue.toStringBox(), isOverridenFromParent));
    } else if (attributeId == DefaultAttributeCSSDocument) {
        return handleCSSChange(getCSSAttributesManager().setCSSDocument(attributeValue, isOverridenFromParent));
    } else if (attributeId == DefaultAttributeCSSClass) {
        return handleCSSChange(getCSSAttributesManager().setCSSClass(attributeValue, isOverridenFromParent));
    } else if (attributeId == DefaultAttributeStyle) {
        return getCSSAttributesManager().apply(
            attributeValue,
            CSSAttributesManagerUpdateContext(viewTransactionScope, getAttributesApplier(), getLogger(), animator));
    } else if (attributeId == DefaultAttributeLazyLayout) {
        setPrefersLazyLayout(viewTransactionScope, attributeValue.toBool());
        return true;
    } else {
        if (_attributesApplier.getBoundAttributes() == nullptr) {
            return false;
        }
        auto changed = getAttributesApplier().setAttribute(
            viewTransactionScope, attributeId, attributeOwner, attributeValue, animator);

        if (changed) {
            handleCSSChange(_cssAttributesManager.attributeChanged(attributeId));
        }

        return changed;
    }
}

void ViewNode::reapplyAttributesRecursive(ViewTransactionScope& viewTransactionScope,
                                          const std::vector<AttributeId>& attributes,
                                          bool invalidateMeasure) {
    if (invalidateMeasure) {
        invalidateMeasuredSize();
    }

    if (_attributesApplier.getBoundAttributes() != nullptr) {
        for (const auto& attribute : attributes) {
            getAttributesApplier().reapplyAttribute(viewTransactionScope, attribute);
        }
        getAttributesApplier().flush(viewTransactionScope);
    }

    for (auto* child : *this) {
        child->reapplyAttributesRecursive(viewTransactionScope, attributes, invalidateMeasure);
    }
}

void ViewNode::notifyAttributeFailed(AttributeId attributeId, const Error& error) {
    _attributesApplier.onApplyAttributeFailed(attributeId, error);
}

void ViewNode::setOnViewCreatedCallback(Ref<ValueFunction> onViewCreatedCallback) {
    _onViewCreatedCallback = std::move(onViewCreatedCallback);

    if (_onViewCreatedCallback != nullptr && hasView()) {
        (*_onViewCreatedCallback)();
    }
}

void ViewNode::setOnViewDestroyedCallback(Ref<ValueFunction> onViewDestroyedCallback) {
    _onViewDestroyedCallback = std::move(onViewDestroyedCallback);
}

void ViewNode::setOnViewChangedCallback(Ref<ValueFunction> onViewChangedCallback) {
    _onViewChangedCallback = std::move(onViewChangedCallback);

    if (hasView()) {
        callViewChangedIfNeeded();
    }
}

void ViewNode::setOnLayoutCompletedCallback(Ref<ValueFunction> onLayoutCompletedCallback) {
    _onLayoutCompletedCallback = std::move(onLayoutCompletedCallback);
}

void ViewNode::setOnMeasureCallback(ViewTransactionScope& viewTransactionScope, Ref<ValueFunction> onMeasureCallback) {
    auto& lazyLayoutData = getOrCreateLazyLayoutData();
    if (lazyLayoutData.onMeasureCallback != onMeasureCallback) {
        lazyLayoutData.onMeasureCallback = std::move(onMeasureCallback);
        updateIsLazyLayout(viewTransactionScope);
        updateYogaMeasureFunc();
        markLayoutDirty();
    }
}

void ViewNode::setOnScrollCallback(Ref<ValueFunction> onScrollCallback) {
    getOrCreateScrollState().setOnScrollCallback(std::move(onScrollCallback));
}

void ViewNode::setOnScrollEndCallback(Ref<ValueFunction> onScrollEndCallback) {
    getOrCreateScrollState().setOnScrollEndCallback(std::move(onScrollEndCallback));
}

void ViewNode::setOnDragStartCallback(Ref<ValueFunction> onDragStartCallback) {
    getOrCreateScrollState().setOnDragStartCallback(std::move(onDragStartCallback));
}

void ViewNode::setOnDragEndingCallback(Ref<ValueFunction> onDragEndingCallback) {
    getOrCreateScrollState().setOnDragEndingCallback(std::move(onDragEndingCallback));
}

void ViewNode::setOnDragEndCallback(Ref<ValueFunction> onDragEndCallback) {
    getOrCreateScrollState().setOnDragEndCallback(std::move(onDragEndCallback));
}

void ViewNode::setOnContentSizeChangeCallback(Ref<ValueFunction> onContentSizeChangeCallback) {
    auto& scrollState = getOrCreateScrollState();
    scrollState.setOnContentSizeChangeCallback(std::move(onContentSizeChangeCallback));
}

std::vector<SharedViewNode> ViewNode::copyChildren() const {
    std::vector<SharedViewNode> out;
    out.reserve(static_cast<size_t>(getChildCount()));

    for (ViewNode* viewNode : *this) {
        out.emplace_back(strongSmallRef(viewNode));
    };
    return out;
}

std::vector<SharedViewNode> ViewNode::copyChildrenWithDebugId(const StringBox& nodeId) const {
    std::vector<SharedViewNode> out;
    for (auto* child : *this) {
        if (child->getDebugId() == nodeId) {
            out.emplace_back(strongSmallRef(child));
        }
    }

    return out;
}

void ViewNode::findAllNodesWithId(const StringBox& nodeId, std::vector<SharedViewNode>& output) {
    if (_cssAttributesManager.hasNodeId(nodeId)) {
        output.emplace_back(strongSmallRef(this));
        return;
    }

    for (auto* childNode : *this) {
        childNode->findAllNodesWithId(nodeId, output);
    }
}

bool ViewNode::isRightToLeft() const {
    if (_yogaNode == nullptr) {
        return false;
    }
    return facebook::yoga::resolveRef(_yogaNode)->getLayout().direction() == facebook::yoga::Direction::RTL;
}

PlatformType ViewNode::getPlatformType() const {
    return _viewNodeTree->getViewManagerContext()->getViewManager().getPlatformType();
}

bool ViewNode::isIncludedInViewParent() const {
    return _flags[kViewIncludedInParentFlag];
}

ViewNodeIterator ViewNode::begin() const {
    return ViewNodeIterator(this, 0);
}

ViewNodeIterator ViewNode::end() const {
    return ViewNodeIterator(this, getChildCount());
}

void ViewNode::setZIndex(ViewTransactionScope& viewTransactionScope, int zIndex) {
    if (_zIndex != zIndex) {
        _zIndex = zIndex;

        auto parent = getParent();
        if (zIndex != 0 && parent != nullptr) {
            parent->setHasChildWithZIndex();
        }
        removeViewFromParent(viewTransactionScope);
        setViewTreeNeedsUpdate();
    }
}

int ViewNode::getZIndex() const {
    return _zIndex;
}

bool ViewNode::hasChildWithZIndex() const {
    return _flags[kHasChildWithZIndex];
}

void ViewNode::setHasChildWithZIndex() {
    _flags[kHasChildWithZIndex] = true;
}

bool ViewNode::hasChildWithAccessibilityId() const {
    return _flags[kHasChildWithAccessibilityId];
}

bool ViewNode::hasAccessibilityId() const {
    return !getAccessibilityId().isEmpty();
}

bool ViewNode::isUserSpecifiedView() const {
    return _viewFactory != nullptr && _viewFactory->isUserSpecified();
}

void ViewNode::setHasChildWithAccessibilityId() {
    auto previous = _flags[kHasChildWithAccessibilityId];
    if (!previous) {
        _flags[kHasChildWithAccessibilityId] = true;
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setHasChildWithAccessibilityId();
        }
    }
}

void ViewNode::setAnimationsEnabled(ViewTransactionScope& viewTransactionScope, bool animationsEnabled) {
    if (_flags[kAnimationsEnabled] != animationsEnabled) {
        _flags[kAnimationsEnabled] = animationsEnabled;

        if (!animationsEnabled && hasView()) {
            viewTransactionScope.transaction().cancelAllViewAnimations(getView());
        }
    }
}

bool ViewNode::isAnimationsEnabled() const {
    return _flags[kAnimationsEnabled];
}

bool ViewNode::shouldReceiveVisibilityUpdates() const {
    return _flags[kShouldReceiveVisibilityUpdates];
}

void ViewNode::setShouldReceiveVisibilityUpdates(bool shouldReceiveVisibilityUpdates) {
    _flags[kShouldReceiveVisibilityUpdates] = shouldReceiveVisibilityUpdates;
}

ReusableArray<ViewNode*> ViewNode::sortChildrenByZIndex() const {
    auto out = makeReusableArray<ViewNode*>();
    out->reserve(getChildCount());

    for (auto* childViewNode : *this) {
        out->push_back(childViewNode);
    }

    std::stable_sort(out->begin(), out->end(), [](ViewNode* left, ViewNode* right) {
        return left->getZIndex() < right->getZIndex();
    });

    return out;
}

const Ref<IViewNodeAssetHandler>& ViewNode::getAssetHandler() const {
    return _assetHandler;
}

void ViewNode::setAssetHandler(const Ref<IViewNodeAssetHandler>& assetHandler) {
    _assetHandler = assetHandler;
}

ILogger& ViewNode::getLogger() const {
    return _logger;
}

std::string ViewNode::getLoggerFormatPrefix() const {
    std::string prefix = "[ViewNode:";
    prefix.append(std::to_string(_rawId));
    prefix.append("]");

    return prefix;
}

std::unique_ptr<ViewNodeViewStats> ViewNode::getViewStats() const {
    auto viewStats = std::make_unique<ViewNodeViewStats>();
    populateViewStats(*viewStats);
    return viewStats;
}

void ViewNode::populateViewStats(ViewNodeViewStats& viewStats) const {
    if (hasView()) {
        viewStats.addViewClass(getViewClassName());
    } else if (isLayout()) {
        viewStats.addLayout();
    }

    for (const auto* child : *this) {
        child->populateViewStats(viewStats);
    }
}

void ViewNode::setRawId(RawViewNodeId rawId) {
    _rawId = rawId;
}

RawViewNodeId ViewNode::getRawId() const {
    return _rawId;
}

CSSAttributesManager& ViewNode::getCSSAttributesManager() {
    return _cssAttributesManager;
}

void ViewNode::setLastChildrenIndexerId(int lastChildrenIndexerId) {
    _lastChildrenIndexerId = lastChildrenIndexerId;
}

int ViewNode::getLastChildrenIndexerId() const {
    return _lastChildrenIndexerId;
}

bool ViewNode::cssNeedsUpdate() const {
    return _flags[kCSSHasChildNeedsUpdate];
}

void ViewNode::setCSSNeedsUpdate() {
    if (!_flags[kCSSNeedsUpdate]) {
        _flags[kCSSNeedsUpdate] = true;
        setCSSHasChildNeedsUpdate();
    }
}

void ViewNode::setCSSHasChildNeedsUpdate() {
    if (!_flags[kCSSHasChildNeedsUpdate]) {
        _flags[kCSSHasChildNeedsUpdate] = true;
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setCSSHasChildNeedsUpdate();
        }
    }
}

void ViewNode::updateCSS(ViewTransactionScope& viewTransactionScope,
                         const Ref<Animator>& animator,
                         bool force,
                         int siblingsCount,
                         int indexAmongSiblings,
                         CSSUpdateResult& updateResult) {
    updateResult.visitedNodes++;

    handleCSSChange(getCSSAttributesManager().setSiblingsIndexes(siblingsCount, indexAmongSiblings));

    auto needUpdateSelf = force || _flags[kCSSNeedsUpdate];
    auto needUpdateChildren = needUpdateSelf || _flags[kCSSHasChildNeedsUpdate];

    if (needUpdateSelf) {
        updateResult.updatedNodes++;
        getCSSAttributesManager().updateCSS(
            CSSAttributesManagerUpdateContext(viewTransactionScope, getAttributesApplier(), getLogger(), animator));
    }
    _attributesApplier.flush(viewTransactionScope);

    if (needUpdateChildren) {
        auto childCount = static_cast<int>(getChildCount());
        int index = 0;
        for (auto* childViewNode : *this) {
            childViewNode->updateCSS(viewTransactionScope, animator, needUpdateSelf, childCount, index, updateResult);
            index++;
        }
    }

    _flags[kCSSNeedsUpdate] = false;
    _flags[kCSSHasChildNeedsUpdate] = false;
}

void ViewNode::updateCSS(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator) {
    snap::utils::time::StopWatch sw;
    sw.start();

    VALDI_TRACE("Valdi.updateCSS");

    CSSUpdateResult updateResult;
    std::memset(&updateResult, 0, sizeof(updateResult));

    getRoot()->updateCSS(viewTransactionScope, animator, false, 0, 0, updateResult);

    if (updateResult.updatedNodes > 0 && Valdi::traceRenderingPerformance) {
        VALDI_INFO(getLogger(),
                   "Update CSS: updates {} nodes in {}, visited {} nodes in total",
                   updateResult.updatedNodes,
                   sw.elapsed(),
                   updateResult.visitedNodes);
    }
}

void ViewNode::setHorizontalScroll(bool horizontalScroll) {
    facebook::yoga::FlexDirection newDirection;
    if (horizontalScroll) {
        newDirection = facebook::yoga::FlexDirection::Row;
    } else {
        newDirection = facebook::yoga::FlexDirection::Column;
    }

    auto* yogaNode = getYogaNodeForInsertingChildren();
    if (getYogaStyle(yogaNode).flexDirection() != newDirection) {
        getYogaStyle(yogaNode).setFlexDirection(newDirection);
        markLayoutDirty();
    }

    auto& scrollState = getOrCreateScrollState();
    if (scrollState.getIsHorizontal() != horizontalScroll) {
        scrollState.setIsHorizontal(horizontalScroll);
        scrollState.setNeedsSyncWithView(true);
    }
}

void ViewNode::setScrollCircularRatio(int circularRatio) {
    auto& scrollState = getOrCreateScrollState();
    if (scrollState.getCircularRatio() != circularRatio) {
        scrollState.setCircularRatio(circularRatio);
        scrollState.setNeedsSyncWithView(true);
    }
}

void ViewNode::setScrollStaticContentWidth(float staticContentWidth) {
    auto& scrollState = getOrCreateScrollState();
    if (scrollState.getStaticContentWidth() != staticContentWidth) {
        scrollState.setStaticContentWidth(staticContentWidth);
        scrollState.setNeedsSyncWithView(true);
    }
}

void ViewNode::setScrollStaticContentHeight(float staticContentHeight) {
    auto& scrollState = getOrCreateScrollState();
    if (scrollState.getStaticContentHeight() != staticContentHeight) {
        scrollState.setStaticContentHeight(staticContentHeight);
        scrollState.setNeedsSyncWithView(true);
    }
}

void ViewNode::setScrollAnchorPosition(int position) {
    _scrollAnchorPosition = position;
}

int ViewNode::getScrollAnchorPosition() const {
    return _scrollAnchorPosition;
}

void ViewNode::setStickyPosition(int position) {
    int previous = _stickyPosition;
    _stickyPosition = position;
    // When transitioning out of sticky mode, release our native-priority hold on
    // translationY so JS writes (fade path, tests, or a later consumer) can regain
    // control. Without this, the header freezes at its last native-set displacement.
    if (previous == StickyPositionTop && position != StickyPositionTop && _viewNodeTree != nullptr) {
        _viewNodeTree->withLock([&]() {
            auto& scope = _viewNodeTree->getCurrentViewTransactionScope();
            getAttributesApplier().removeAttribute(
                scope, DefaultAttributeTranslationY, AttributeOwner::getNativeOverridenAttributeOwner(), nullptr);
            getAttributesApplier().flush(scope);
        });
    } else if (previous != StickyPositionTop && position == StickyPositionTop && _viewNodeTree != nullptr) {
        // Runtime toggle none -> top: walk up to the nearest scroll ancestor with
        // nativeStickyEnabled=true and refresh its sticky cache. Without this, a header
        // that becomes sticky after the enclosing scroll has already run its layout pass
        // won't have parentY/parentH/childH populated and will read defaults on next scroll.
        for (auto* p = getParent().get(); p != nullptr; p = p->getParent().get()) {
            if (p->_flags[kScrollAttributesBound]) {
                if (p->_scrollState != nullptr && p->_scrollState->getNativeStickyEnabled()) {
                    p->updateStickyHeaders(/*refreshCache=*/true);
                }
                break;
            }
        }
    }
}

int ViewNode::getStickyPosition() const {
    return _stickyPosition;
}

void ViewNode::setNativeStickyEnabled(bool enabled) {
    auto& scrollState = getOrCreateScrollState();
    bool previous = scrollState.getNativeStickyEnabled();
    scrollState.setNativeStickyEnabled(enabled);

    // Runtime toggle false -> true: build the sticky measurement cache now so the
    // next scroll frame doesn't read default 0.0f values and misposition headers.
    // Layout-driven refresh (updateScrollState -> refreshCache=true) would eventually
    // populate it on the next layout pass, but scroll can fire before that.
    if (!previous && enabled) {
        updateStickyHeaders(/*refreshCache=*/true);
    }

    // When turning off, walk sticky descendants and release the native-priority
    // hold on translationY so JS (or the consumer's next render) can reclaim it.
    // Without this the pinned headers freeze at their last native-set displacement.
    if (previous && !enabled && _viewNodeTree != nullptr) {
        _viewNodeTree->withLock([&]() {
            auto& scope = _viewNodeTree->getCurrentViewTransactionScope();
            auto* owner = AttributeOwner::getNativeOverridenAttributeOwner();
            static constexpr int kStickyMaxDepth = 16;
            std::function<void(ViewNode*, int)> walk = [&](ViewNode* node, int depth) {
                if (depth <= 0) {
                    return;
                }
                for (auto* child : *node) {
                    if (child->getStickyPosition() == StickyPositionTop) {
                        child->getAttributesApplier().removeAttribute(
                            scope, DefaultAttributeTranslationY, owner, nullptr);
                        child->getAttributesApplier().flush(scope);
                    }
                    // Skip recursion into nested scrolls (they own their own sticky pass),
                    // but only AFTER releasing on the scroll itself if it happened to be
                    // tagged sticky.
                    if (child->_flags[kScrollAttributesBound]) {
                        continue;
                    }
                    walk(child, depth - 1);
                }
            };
            walk(this, kStickyMaxDepth);
        });
    }
}

void ViewNode::setNativeStickyCover(float cover) {
    auto& scrollState = getOrCreateScrollState();
    scrollState.setNativeStickyCover(cover);
    // Reapply the clamp with the new cover using the current cache. No re-measure needed:
    // cover affects the clamp math (maxDisplacement = parentH - childH - cover), not layout.
    if (scrollState.getNativeStickyEnabled()) {
        updateStickyHeaders(/*refreshCache=*/false);
    }
}

void ViewNode::setNativeStickyOffset(float offset) {
    auto& scrollState = getOrCreateScrollState();
    scrollState.setNativeStickyOffset(offset);
    if (scrollState.getNativeStickyEnabled()) {
        updateStickyHeaders(/*refreshCache=*/false);
    }
}

void ViewNode::setMaintainScrollAnchor(bool maintain) {
    auto& scrollState = getOrCreateScrollState();
    scrollState.setMaintainScrollAnchor(maintain);
}

void ViewNode::setPreserveScrollPosition(bool preserve) {
    auto& scrollState = getOrCreateScrollState();
    scrollState.setPreserveScrollPosition(preserve);
    // Capture the anchor immediately on enable, from the current (still valid, pre-insertion)
    // layout. Otherwise the anchor stays empty until the next scroll/layout pass; if that pass is
    // a content insertion (live message / forward page), Step 1 in updateScrollState would be
    // skipped (no anchor) and the viewport would jump before the anchor is first recorded.
    if (preserve && _calculatedFrame.height > 0.0f) {
        refreshPreserveAnchor(
            this, scrollState, scrollState.getDirectionAgnosticContentOffset().y, _calculatedFrame.height);
    }
}

template<typename F>
void ViewNode::updateViewportExtension(F&& handler) {
    auto& scrollState = getOrCreateScrollState();
    handler(scrollState);
    setCalculatedViewportNeedsUpdate();
}

void ViewNode::setViewportExtensionTop(float viewportExtensionTop) {
    updateViewportExtension([&](auto& scrollState) { scrollState.setViewportExtensionTop(viewportExtensionTop); });
}

void ViewNode::setViewportExtensionBottom(float viewportExtensionBottom) {
    updateViewportExtension(
        [&](auto& scrollState) { scrollState.setViewportExtensionBottom(viewportExtensionBottom); });
}

void ViewNode::setViewportExtensionLeft(float viewportExtensionLeft) {
    updateViewportExtension([&](auto& scrollState) { scrollState.setViewportExtensionLeft(viewportExtensionLeft); });
}

void ViewNode::setViewportExtensionRight(float viewportExtensionRight) {
    updateViewportExtension([&](auto& scrollState) { scrollState.setViewportExtensionRight(viewportExtensionRight); });
}

ViewNodeAccessibilityState& ViewNode::getOrCreateAccessibilityState() {
    if (_accessibilityState == nullptr) {
        _accessibilityState = std::make_unique<ViewNodeAccessibilityState>(_attributesApplier);
    }
    return *_accessibilityState;
}

void ViewNode::setAccessibilityCategory(int accessibilityCategoryInt) {
    auto accessibilityCategory = static_cast<AccessibilityCategory>(accessibilityCategoryInt);
    getOrCreateAccessibilityState().setAccessibilityCategory(accessibilityCategory);
    setAccessibilityTreeNeedsUpdate();
}
AccessibilityCategory ViewNode::getAccessibilityCategory() {
    return getOrCreateAccessibilityState().getAccessibilityCategory();
}

void ViewNode::setAccessibilityNavigation(int accessibilityNavigationInt) {
    auto accessibilityNavigation = static_cast<AccessibilityNavigation>(accessibilityNavigationInt);
    getOrCreateAccessibilityState().setAccessibilityNavigation(accessibilityNavigation);
    setAccessibilityTreeNeedsUpdate();
}
AccessibilityNavigation ViewNode::getAccessibilityNavigation() {
    return getOrCreateAccessibilityState().getAccessibilityNavigation();
}

void ViewNode::setAccessibilityPriority(float accessibilityPriority) {
    getOrCreateAccessibilityState().setAccessibilityPriority(accessibilityPriority);
    setAccessibilityTreeNeedsUpdate();
}
float ViewNode::getAccessibilityPriority() {
    return getOrCreateAccessibilityState().getAccessibilityPriority();
}

void ViewNode::setAccessibilityLabel(const StringBox& accessibilityLabel) {
    getOrCreateAccessibilityState().setAccessibilityLabel(accessibilityLabel);
    setAccessibilityTreeNeedsUpdate();
}
StringBox ViewNode::getAccessibilityLabel() {
    return getOrCreateAccessibilityState().getAccessibilityLabel();
}

void ViewNode::setAccessibilityHint(const StringBox& accessibilityHint) {
    getOrCreateAccessibilityState().setAccessibilityHint(accessibilityHint);
    setAccessibilityTreeNeedsUpdate();
}
StringBox ViewNode::getAccessibilityHint() {
    return getOrCreateAccessibilityState().getAccessibilityHint();
}

void ViewNode::setAccessibilityValue(const StringBox& accessibilityValue) {
    getOrCreateAccessibilityState().setAccessibilityValue(accessibilityValue);
    setAccessibilityTreeNeedsUpdate();
}
StringBox ViewNode::getAccessibilityValue() {
    return getOrCreateAccessibilityState().getAccessibilityValue();
}

void ViewNode::setAccessibilityStateDisabled(bool accessibilityStateDisabled) {
    getOrCreateAccessibilityState().setAccessibilityStateDisabled(accessibilityStateDisabled);
}
bool ViewNode::getAccessibilityStateDisabled() {
    return getOrCreateAccessibilityState().getAccessibilityStateDisabled();
}

void ViewNode::setAccessibilityStateSelected(bool accessibilityStateSelected) {
    getOrCreateAccessibilityState().setAccessibilityStateSelected(accessibilityStateSelected);
}
bool ViewNode::getAccessibilityStateSelected() {
    return getOrCreateAccessibilityState().getAccessibilityStateSelected();
}

void ViewNode::setAccessibilityStateLiveRegion(bool accessibilityStateLiveRegion) {
    getOrCreateAccessibilityState().setAccessibilityStateLiveRegion(accessibilityStateLiveRegion);
}
bool ViewNode::getAccessibilityStateLiveRegion() {
    return getOrCreateAccessibilityState().getAccessibilityStateLiveRegion();
}

void ViewNode::setAccessibilityId(const StringBox& accessibilityId) {
    getOrCreateAccessibilityState().setAccessibilityId(accessibilityId);
    setAccessibilityTreeNeedsUpdate();

    if (!accessibilityId.isEmpty()) {
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setHasChildWithAccessibilityId();
        }
    }
}

StringBox ViewNode::getAccessibilityId() const {
    if (_accessibilityState == nullptr) {
        return StringBox();
    }

    return _accessibilityState->getAccessibilityId();
}

bool ViewNode::isAccessibilityFocusable() {
    auto accessibilityNavigation = getAccessibilityNavigation();
    switch (accessibilityNavigation) {
        case AccessibilityNavigationLeaf:
        case AccessibilityNavigationCover:
        case AccessibilityNavigationGroup:
            return true;
        default:
            return false;
    }
}

bool ViewNode::isAccessibilityContainer() {
    auto accessibilityNavigation = getAccessibilityNavigation();
    switch (accessibilityNavigation) {
        case AccessibilityNavigationCover:
        case AccessibilityNavigationGroup:
        case AccessibilityNavigationPassthrough:
            return true;
        default:
            return false;
    }
}

bool ViewNode::isAccessibilityPassthrough() {
    auto accessibilityNavigation = getAccessibilityNavigation();
    switch (accessibilityNavigation) {
        case AccessibilityNavigationCover:
        case AccessibilityNavigationPassthrough:
            return true;
        default:
            return false;
    }
}

bool ViewNode::isAccessibilityLeaf() {
    bool focusable = isAccessibilityFocusable();
    bool container = isAccessibilityContainer();
    bool passthrough = isAccessibilityPassthrough();
    return focusable && (!container || passthrough);
}

std::vector<ViewNode*> ViewNode::getAccessibilityChildrenShallow() {
    std::vector<ViewNode*> children;
    if (!isAccessibilityContainer() && !hasChildWithAccessibilityId()) {
        return children;
    }
    children.reserve(getChildCount());
    bool needSorting = false;
    for (ViewNode* child : *this) {
        children.emplace_back(child);
        if (child->getAccessibilityPriority() != 0) {
            needSorting = true;
        }
    }
    if (needSorting) {
        std::stable_sort(children.begin(), children.end(), [](ViewNode* viewNode1, ViewNode* viewNode2) {
            float priority1 = viewNode1->getAccessibilityPriority();
            float priority2 = viewNode2->getAccessibilityPriority();
            return priority1 > priority2;
        });
    }
    return children;
}

std::vector<ViewNode*> ViewNode::getAccessibilityChildrenRecursive() {
    propagateAccessibilityTreeUpToDate();
    std::vector<ViewNode*> children;
    getAccessibilityChildrenDeepWalk(children);
    return children;
}

void ViewNode::propagateAccessibilityTreeUpToDate() {
    if (!_flags[kAccessibilityTreeNeedsUpdate]) {
        return;
    }
    // Go over all of the nodes that are not part of the accessibility tree and set
    // the kAccessibilityTreeNeedsUpdate as false. Other nodes are going to be visited
    // through getAccessibilityChildrenDeepWalk()

    _flags[kAccessibilityTreeNeedsUpdate] = false;

    for (auto* child : *this) {
        if (!child->isMemberOfAccessibilityTree()) {
            child->propagateAccessibilityTreeUpToDate();
        }
    }
}

bool ViewNode::isMemberOfAccessibilityTree() {
    return hasAccessibilityId() || isUserSpecifiedView() || isAccessibilityFocusable();
}

void ViewNode::getAccessibilityChildrenDeepWalk(std::vector<ViewNode*>& outChildren) {
    for (ViewNode* child : getAccessibilityChildrenShallow()) {
        // Add the children into the list first.
        // For iOS, z-ordering is done bottom to top. Element 0 is in front of element 1 etc.
        // Put the children first so they aren't occluded by their container parents.
        if (child->isAccessibilityPassthrough()) {
            child->getAccessibilityChildrenDeepWalk(outChildren);
        }

        if (child->isMemberOfAccessibilityTree()) {
            outChildren.emplace_back(child);
        }
    }
}

ViewNode* ViewNode::hitTestAccessibilityChildren(const Point& parentDirectionDependentVisual) {
    auto frame = getCalculatedFrame();
    if (!frame.contains(parentDirectionDependentVisual)) {
        return nullptr;
    }

    Point selfDirectionDependentVisual = convertParentVisualToSelfVisual(parentDirectionDependentVisual);
    for (ViewNode* child : getAccessibilityChildrenShallow()) {
        auto result = child->hitTestAccessibilityChildren(selfDirectionDependentVisual);
        if (result != nullptr) {
            return result;
        }
    }
    if (isAccessibilityFocusable()) {
        return this;
    }
    return nullptr;
}

void ViewNode::setExtendViewportWithChildren(bool extendViewportWithChildren) {
    if (_flags[kExtendViewportWithChildren] != extendViewportWithChildren) {
        _flags[kExtendViewportWithChildren] = extendViewportWithChildren;
        setCalculatedViewportNeedsUpdate();
    }
}

bool ViewNode::extendViewportWithChildren() const {
    return _flags[kExtendViewportWithChildren];
}

void ViewNode::setIgnoreParentViewport(bool ignoreParentViewport) {
    if (_flags[kIgnoreParentViewport] != ignoreParentViewport) {
        _flags[kIgnoreParentViewport] = ignoreParentViewport;
        setCalculatedViewportNeedsUpdate();
    }
}

bool ViewNode::ignoreParentViewport() const {
    return _flags[kIgnoreParentViewport];
}

float ViewNode::getTranslationX() const {
    return _translationX.getResolvedValue(_calculatedFrame.width, _flags[kTranslationXIsPercent]);
}

void ViewNode::setTranslationX(float translationX, bool isPercent) {
    updateTranslation(translationX, isPercent, &_translationX, kTranslationXIsPercent);
}

float ViewNode::getDirectionDependentTranslationX() const {
    auto translationX = getTranslationX();
    if (isRightToLeft()) {
        return translationX * -1;
    } else {
        return translationX;
    }
}

float ViewNode::getTranslationY() const {
    return _translationY.getResolvedValue(_calculatedFrame.height, _flags[kTranslationYIsPercent]);
}

void ViewNode::setTranslationY(float translationY, bool isPercent) {
    updateTranslation(translationY, isPercent, &_translationY, kTranslationYIsPercent);
}

void ViewNode::setScaleX(float scaleX) {
    if (_scaleX != scaleX) {
        _scaleX = scaleX;
        setCalculatedViewportNeedsUpdate();
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setChildrenIndexerNeedsUpdate();
        }
    }
}

void ViewNode::setScaleY(float scaleY) {
    if (_scaleY != scaleY) {
        _scaleY = scaleY;
        setCalculatedViewportNeedsUpdate();
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setChildrenIndexerNeedsUpdate();
        }
    }
}

void ViewNode::updateTranslation(float translation,
                                 bool isPercent,
                                 ViewNodeTranslation* outTranslation,
                                 size_t percentFlag) {
    if (outTranslation->value != translation || _flags[percentFlag] != isPercent) {
        outTranslation->value = translation;
        _flags[percentFlag] = isPercent;
        setCalculatedViewportNeedsUpdate();

        auto parent = getParent();
        if (parent != nullptr) {
            parent->setChildrenIndexerNeedsUpdate();
        }
    }
}

void ViewNode::setEstimatedWidth(float estimatedWidth) {
    auto& lazyLayoutData = getOrCreateLazyLayoutData();
    if (lazyLayoutData.estimatedWidth != estimatedWidth) {
        lazyLayoutData.estimatedWidth = estimatedWidth;
        invalidateMeasuredSize();
    }
}

void ViewNode::setEstimatedHeight(float estimatedHeight) {
    auto& lazyLayoutData = getOrCreateLazyLayoutData();
    if (lazyLayoutData.estimatedHeight != estimatedHeight) {
        lazyLayoutData.estimatedHeight = estimatedHeight;
        invalidateMeasuredSize();
    }
}

ViewNodeTreeInfo ViewNode::dumpTreeInfo() const {
    ViewNodeTreeInfo info;
    info.nodesCount = 1;
    if (hasView()) {
        info.viewsCount = 1;
    }

    for (auto* child : *this) {
        auto childInfo = child->dumpTreeInfo();

        info.nodesCount += childInfo.nodesCount;
        info.viewsCount += childInfo.viewsCount;
    }

    return info;
}

void ViewNode::createSnapshot(ViewTransactionScope& viewTransactionScope,
                              Function<void(Result<BytesView>)>&& callback) const {
    if (!hasView()) {
        callback(BytesView());
    } else {
        viewTransactionScope.transaction().snapshotView(getView(), std::move(callback));
    }
}

const ViewNodeChildrenIndexer* ViewNode::getChildrenIndexer() const {
    return _childrenIndexer.get();
}

AttributeIds& ViewNode::getAttributeIds() const {
    return _attributeIds;
}

bool ViewNode::accessibilityTreeNeedsUpdate() const {
    return _flags[kAccessibilityTreeNeedsUpdate];
}

void ViewNode::setAccessibilityTreeNeedsUpdate() {
    if (!_flags[kAccessibilityTreeNeedsUpdate]) {
        _flags[kAccessibilityTreeNeedsUpdate] = true;
        auto parent = getParent();
        if (parent != nullptr) {
            parent->setAccessibilityTreeNeedsUpdate();
        }
    }
}

Ref<Metrics> ViewNode::getMetrics() const {
    if (_viewNodeTree == nullptr) {
        return nullptr;
    }

    return _viewNodeTree->getMetrics();
}

// YOGA C callbacks

static MeasureMode yogaMeasureModeToValdiMeasureMode(YGMeasureMode measureMode) {
    if (measureMode == YGMeasureModeExactly) {
        return MeasureModeExactly;
    } else if (measureMode == YGMeasureModeAtMost) {
        return MeasureModeAtMost;
    } else {
        return MeasureModeUnspecified;
    }
}

YGSize ygMeasureYoga(
    YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
    auto* viewNode = reinterpret_cast<ViewNode*>(facebook::yoga::resolveRef(node)->getContext());
    if (viewNode == nullptr) {
        return {.width = 0, .height = 0};
    }
    SC_ASSERT(currentMeasureMetrics != nullptr);
    currentMeasureMetrics->totalMeasure++;

    auto convertedWidthMode = yogaMeasureModeToValdiMeasureMode(widthMode);
    auto convertedHeightMode = yogaMeasureModeToValdiMeasureMode(heightMode);

    auto measuredSize = viewNode->onMeasure(width, convertedWidthMode, height, convertedHeightMode);

    auto pointScale = viewNode->getPointScale();

    return (YGSize){
        .width = sanitizeMeasurement(width, measuredSize.width, pointScale, convertedWidthMode),
        .height = sanitizeMeasurement(height, measuredSize.height, pointScale, convertedHeightMode),
    };
}

void ygDirtiedCallback(YGNodeConstRef node) {
    auto* viewNode = reinterpret_cast<ViewNode*>(facebook::yoga::resolveRef(node)->getContext());
    if (viewNode == nullptr) {
        return;
    }

    if (viewNode->isLazyLayout()) {
        viewNode->scheduleLazyLayout();
    }

    if (!viewNode->hasParent()) {
        auto* viewNodeTree = viewNode->getViewNodeTree();
        if (viewNodeTree != nullptr) {
            viewNodeTree->onLayoutDirty();
        }
        viewNode->scheduleUpdates();
    }
}

// ViewNodeIterator

ViewNodeIterator::ViewNodeIterator(const ViewNode* viewNode, size_t index) : _viewNode(viewNode), _index(index) {}

ViewNodeIterator& ViewNodeIterator::operator++() {
    _index++;
    return *this;
}

ViewNodeIterator& ViewNodeIterator::operator--() {
    _index--;
    return *this;
}

ViewNode* ViewNodeIterator::operator*() const {
    return _viewNode->getChildAt(_index);
}

bool ViewNodeIterator::operator!=(const ViewNodeIterator& other) const {
    return _index != other._index;
}

} // namespace Valdi
