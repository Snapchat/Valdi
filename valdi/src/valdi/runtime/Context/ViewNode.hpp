//
//  ViewNode.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 9/4/18.
//

#pragma once

#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi/runtime/Attributes/ViewNodeAttributesApplier.hpp"
#include "valdi/runtime/CSS/CSSAttributesManager.hpp"
#include "valdi/runtime/Context/RawViewNodeId.hpp"
#include "valdi/runtime/Context/ScrollAnchorPosition.hpp"
#include "valdi/runtime/Context/ViewNodeAccessibility.hpp"
#include "valdi/runtime/Views/Frame.hpp"
#include "valdi/runtime/Views/Measure.hpp"
#include "valdi/runtime/Views/View.hpp"

#include "valdi_core/cpp/Context/PlatformType.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Function.hpp"
#include "valdi_core/cpp/Utils/ObjectPool.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include <bitset>
#include <memory>

struct YGNode;
struct YGConfig;

namespace Valdi {

class CSSDocument;
class ViewNode;
class ViewNodeChildrenIndexer;
class ViewFactory;
class View;
class IViewNodeAssetHandler;

using SharedViewNode = Ref<ViewNode>;

enum LimitToViewport : uint8_t {
    // Unset means disabled, unless the parent ViewNode
    // is enabled
    LimitToViewportUnset,
    LimitToViewportEnabled,
    LimitToViewportDisabled,
};

class ViewNodeTree;
class AttributesApplier;
class AttributeIds;
class ILogger;
class IViewManager;
class Animator;
class BoundAttributes;
class AttributeOwner;
class ViewNodesFrameObserver;
class Metrics;

class ViewNode;
class ViewNodeIterator {
public:
    ViewNodeIterator(const ViewNode* viewNode, size_t index);

    ViewNodeIterator& operator++();
    ViewNodeIterator& operator--();
    ViewNode* operator*() const;

    bool operator!=(const ViewNodeIterator& other) const;

private:
    const ViewNode* _viewNode;
    size_t _index = 0;
};

struct LazyLayoutData {
    YGNode* yogaNode = nullptr;
    float availableWidth = 0;
    float availableHeight = 0;
    float estimatedWidth = 0;
    float estimatedHeight = 0;
    Ref<ValueFunction> onMeasureCallback;

    ~LazyLayoutData();

    void destroyNode();
};

struct ViewNodeUpdateViewTreeResult {
    int visitedNodes = 0;
    int reinsertedViews = 0;
    int destroyedViews = 0;
    int createdViews = 0;

    ViewNodeUpdateViewTreeResult() = default;
    constexpr ViewNodeUpdateViewTreeResult(int visitedNodes, int reinsertedViews, int destroyedViews, int createdViews)
        : visitedNodes(visitedNodes),
          reinsertedViews(reinsertedViews),
          destroyedViews(destroyedViews),
          createdViews(createdViews) {}
};

struct CSSUpdateResult {
    int visitedNodes;
    int updatedNodes;
};

class ViewNodeScrollState;
class ViewNodeAccessibilityState;

class ViewNodeViewStats;

class CSSAttributesManager;

class ViewNodeAttributesApplier;
class MeasureDelegate;
class ViewTransactionScope;

struct ViewNodeTreeInfo {
    int nodesCount = 0;
    int viewsCount = 0;
};

enum LayoutDirection {
    LayoutDirectionLTR,
    LayoutDirectionRTL,
};

enum ScrollDirection {
    ScrollDirectionTopToBottom = 0,
    ScrollDirectionBottomToTop = 1,
    ScrollDirectionLeftToRight = 2,
    ScrollDirectionRightToLeft = 3,
    ScrollDirectionHorizontalForward = 4,
    ScrollDirectionHorizontalBackward = 5,
    ScrollDirectionVerticalForward = 6,
    ScrollDirectionVerticalBackward = 7,
    ScrollDirectionForward = 8,
    ScrollDirectionBackward = 9,
};

enum SimplifiedScrollDirection {
    SimplifiedScrollDirectionTopToBottom = 0,
    SimplifiedScrollDirectionBottomToTop = 1,
    SimplifiedScrollDirectionLeftToRight = 2,
    SimplifiedScrollDirectionRightToLeft = 3,
};

class ViewNode : public SharedPtrRefCountable {
public:
    ViewNode(YGConfig* yogaConfig, AttributeIds& attributeIds, ILogger& logger);

    ~ViewNode() override;

    void clear(ViewTransactionScope& viewTransactionScope);

    void setScrollContentOffset(ViewTransactionScope& viewTransactionScope,
                                const Point& directionAgnosticContentOffset,
                                bool animated = false);
    Point getDirectionAgnosticScrollContentOffset() const;

    /**
     * Returns the frame that the backing View of this ViewNode should have,
     * taken in account any offset for layout nodes if needed.
     */
    const Frame& getViewFrame() const;

    /**
     * Returns the calculated frame from Yoga, taking in account RTL offset.
     */
    const Frame& getCalculatedFrame() const;

    /**
     * Get the calculated frame without taking in account the LTR or RTL direction.
     */
    Frame getDirectionAgnosticFrame() const;

    /**
     * Given a position in this ViewNode's coordinate space,
     * returns the position as if the ViewNode was in LTR.
     */
    Point convertPointToRelativeDirectionAgnostic(const Point& directionDependentPoint) const;

    /**
     * Given a position in this ViewNode's coordinate space,
     * returns the position from the root as if the ViewNode was in LTR.
     */
    Point convertPointToAbsoluteDirectionAgnostic(const Point& directionDependentPoint) const;

    /**
     * Given a position in this ViewNode's coordinate space,
     * returns the position as it visually appears in its parent,
     * taking in account LTR or RTL direction, translations and scroll offsets.
     */
    Point convertSelfVisualToParentVisual(const Point& directionDependentVisual) const;

    /**
     * Given a position in the ViewNode's parent coordinate space,
     * returns the same position in the current ViewNode's coordinate space,
     * taking in account LTR or RTL direction, translations and scroll offsets.
     */
    Point convertParentVisualToSelfVisual(const Point& directionDependentVisual) const;

    /**
     * Given a position in this ViewNode's coordinate space,
     * returns the visual position as it visually appears from the root,
     * taking in account LTR or RTL direction, translations and scroll offsets.
     */
    Point convertSelfVisualToRootVisual(const Point& directionDependentVisual) const;

    /**
     * Get the point representing the origin within this ViewNode's coordinate space.
     */
    Point getBoundsOriginPoint() const;

    Point getDirectionDependentTransform() const;
    Point getDirectionAgnosticTransform() const;

    float getChildrenSpaceWidth() const;
    float getChildrenSpaceHeight() const;
    float getRtlScrollOffsetX() const;

    float getViewOffsetX() const;
    float getViewOffsetY() const;

    LimitToViewport limitToViewport() const;
    void setLimitToViewport(LimitToViewport limitToViewport);

    bool calculatedViewportNeedsUpdate() const;
    void setCalculatedViewportNeedsUpdate();
    bool calculatedViewportHasChildNeedsUpdate() const;

    void setCalculatedViewport(const Frame& calculatedViewport);
    const Frame& getCalculatedViewport() const;

    bool isVisibleInViewport() const;

    bool invalidateMeasuredSize();
    bool markLayoutDirty();
    /**
     Returns whether a full layout calculation is required from this node
     */
    bool isFlexLayoutDirty() const;
    /**
     Returns whether this node or a child has a lazy layout that needs calculation.
     */
    bool isLazyLayoutDirty() const;

    bool viewTreeNeedsUpdate() const;
    ViewNodeUpdateViewTreeResult updateViewTree(ViewTransactionScope& viewTransactionScope);
    void setViewTreeNeedsUpdate();

    void removeViewFromParent(ViewTransactionScope& viewTransactionScope);
    void removeViewFromParent(ViewTransactionScope& viewTransactionScope, bool shouldViewClearViewNode);
    void removeViewFromParent(ViewTransactionScope& viewTransactionScope,
                              bool willUpdateViewTree,
                              bool shouldViewClearViewNode);

    SharedViewNode getParent() const;

    const Ref<View>& getView() const;
    Ref<View> getViewAndDisablePooling() const;

    /**
     * Return a Platform object representation for this ViewNode.
     * If "wrapInPlatformReference" is true, the returned object
     * will be inside an additional platform reference object like
     * SCValdiRef on iOS.
     */
    Value toPlaformRepresentation(bool wrapInPlatformReference);

    void setView(ViewTransactionScope& viewTransactionScope, const Ref<View>& view, const Ref<Animator>& animator);
    bool removeView(ViewTransactionScope& viewTransactionScope);

    const StringBox& getViewClassName() const;

    void setViewClassNameForPlatform(ViewTransactionScope& viewTransactionScope,
                                     const StringBox& viewClassName,
                                     PlatformType platformType);

    void setViewFactory(ViewTransactionScope& viewTransactionScope, const Ref<ViewFactory>& viewFactory);
    const Ref<ViewFactory>& getViewFactory() const;

    float getPointScale() const;

    bool hasView() const;

    bool isLayout() const;

    bool isLazyLayout() const;
    void setPrefersLazyLayout(ViewTransactionScope& viewTransactionScope, bool prefersLazyLayout);

    bool isAnimationsEnabled() const;
    void setAnimationsEnabled(ViewTransactionScope& viewTransactionScope, bool animationsEnabled);

    void setShouldReceiveVisibilityUpdates(bool shouldReceiveVisibilityUpdates);
    bool shouldReceiveVisibilityUpdates() const;

    bool hasParent() const;

    void setViewNodeTree(ViewNodeTree* viewNodeTree);
    ViewNodeTree* getViewNodeTree() const;

    void valueChanged(AttributeId attribute, const Value& value, bool shouldNotifySync = false);

    bool setAttribute(ViewTransactionScope& viewTransactionScope,
                      AttributeId attribute,
                      const AttributeOwner* attributeOwner,
                      const Value& attributeValue,
                      const Ref<Animator>& animator);

    bool setAttribute(ViewTransactionScope& viewTransactionScope,
                      AttributeId attribute,
                      const AttributeOwner* attributeOwner,
                      const Value& attributeValue,
                      const Ref<Animator>& animator,
                      bool isOverridenFromParent);

    void reapplyAttributesRecursive(ViewTransactionScope& viewTransactionScope,
                                    const std::vector<AttributeId>& attributes,
                                    bool invalidateMeasure);

    void notifyAttributeFailed(AttributeId attributeId, const Error& error);

    AttributesApplier& getAttributesApplier();

    std::optional<Point> onScroll(const Point& directionDependentContentOffset,
                                  const Point& directionDependentUnclampedContentOffset,
                                  const Point& directionDependentVelocity);
    void onScrollEnd(const Point& directionDependentContentOffset,
                     const Point& directionDependentUnclampedContentOffset);
    void onDragStart(const Point& directionDependentContentOffset,
                     const Point& directionDependentUnclampedContentOffset,
                     const Point& directionDependentVelocity);
    std::optional<Point> onDragEnding(const Point& directionDependentContentOffset,
                                      const Point& directionDependentUnclampedContentOffset,
                                      const Point& directionDependentVelocity);

    float getCanScrollDistance(ScrollDirection scrollDirection);
    bool canScroll(const Point& parentDirectionDependentVisual, ScrollDirection scrollDirection);
    void setCanAlwaysScrollHorizontal(bool canAlwaysScrollHorizontal);
    void setCanAlwaysScrollVertical(bool canAlwaysScrollVertical);

    bool isScrollingOrAnimatingScroll() const;

    bool isMeasurerPlaceholder() const;

    /**
     Measure the node by itself, ignoring its children.
     */
    Size onMeasure(float width, MeasureMode widthMode, float height, MeasureMode heightMode);

    std::string getLayoutDebugDescription() const;

    Ref<ValueMap> getDebugDescriptionMap() const;

    std::string toXML(bool recursive = true) const;
    void printXML() const;

    YGNode* getYogaNode() const;
    YGNode* getYogaNodeForInsertingChildren();

    ViewNodeIterator begin() const;
    ViewNodeIterator end() const;

    std::vector<SharedViewNode> copyChildren() const;
    std::vector<SharedViewNode> copyChildrenWithDebugId(const StringBox& nodeId) const;

    void findAllNodesWithId(const StringBox& nodeId, std::vector<SharedViewNode>& output);

    size_t getChildCount() const;
    ViewNode* getChildAt(size_t index) const;
    void insertChildAt(ViewTransactionScope& viewTransactionScope, const Ref<ViewNode>& child, size_t index);
    void appendChild(ViewTransactionScope& viewTransactionScope, const Ref<ViewNode>& child);
    void removeFromParent(ViewTransactionScope& viewTransactionScope);

    int getViewChildrenCount() const;

    size_t getRecursiveChildCount() const;

    Size measureLayout(
        float width, MeasureMode widthMode, float height, MeasureMode heightMode, LayoutDirection direction);
    void performLayout(ViewTransactionScope& viewTransactionScope, Size size, LayoutDirection direction);

    /**
     Update the visibility of all the nodes in this subtree,
     and perform updates based on what changes.
     */
    void updateVisibilityAndPerformUpdates(ViewTransactionScope& viewTransactionScope);

    /**
     Schedule an update pass
     */
    void scheduleUpdates();

    void setOnViewCreatedCallback(Ref<ValueFunction> onViewCreatedCallback);
    void setOnViewDestroyedCallback(Ref<ValueFunction> onViewDestroyedCallback);
    void setOnViewChangedCallback(Ref<ValueFunction> onViewChangedCallback);
    void setOnLayoutCompletedCallback(Ref<ValueFunction> onLayoutCompletedCallback);
    void setOnMeasureCallback(ViewTransactionScope& viewTransactionScope, Ref<ValueFunction> onMeasureCallback);

    void setOnScrollCallback(Ref<ValueFunction> onScrollCallback);
    void setOnScrollEndCallback(Ref<ValueFunction> onScrollEndCallback);
    void setOnDragStartCallback(Ref<ValueFunction> onDragStartCallback);
    void setOnDragEndingCallback(Ref<ValueFunction> onDragEndingCallback);
    void setOnDragEndCallback(Ref<ValueFunction> onDragEndCallback);
    void setOnContentSizeChangeCallback(Ref<ValueFunction> onContentSizeChangeCallback);

    bool isRightToLeft() const;
    PlatformType getPlatformType() const;

    StringBox getDebugId() const;
    const StringBox& getModuleName() const;
    Shared<CSSDocument> getCSSDocument() const;

    SharedViewNode getRoot();

    void scheduleLazyLayout();

    void setZIndex(ViewTransactionScope& viewTransactionScope, int zIndex);
    int getZIndex() const;

    std::unique_ptr<ViewNodeViewStats> getViewStats() const;

    ILogger& getLogger() const;

    /**
     Returns a string that allows to identify this ViewNode
     when logging.
     */
    std::string getLoggerFormatPrefix() const;

    RawViewNodeId getRawId() const;
    void setRawId(RawViewNodeId rawId);

    // CSS Attributes, used for ViewNode in raw render modes

    CSSAttributesManager& getCSSAttributesManager();

    AttributeIds& getAttributeIds() const;

    // Used by the ViewNodeChildrenIndexer to know whether this node was processed in the
    // findChildrenVisibility() pass.
    int getLastChildrenIndexerId() const;
    void setLastChildrenIndexerId(int lastChildrenIndexerId);

    /**
     Whether the CSS nodes are dirty and need to be updated.
     */
    bool cssNeedsUpdate() const;

    /**
     Update the CSS attributes using the given animator.
     */
    void updateCSS(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator);

    void setHorizontalScroll(bool horizontalScroll);

    void setScrollStaticContentWidth(float staticContentWidth);
    void setScrollStaticContentHeight(float staticContentHeight);

    void setScrollCircularRatio(int circularRatio);

    void setViewportExtensionTop(float viewportExtensionTop);
    void setViewportExtensionBottom(float viewportExtensionBottom);
    void setViewportExtensionLeft(float viewportExtensionLeft);
    void setViewportExtensionRight(float viewportExtensionRight);

    // Scroll anchor: pin a child at the top or bottom of the viewport during pagination
    void setScrollAnchorPosition(int position);
    int getScrollAnchorPosition() const;
    void setMaintainScrollAnchor(bool maintain);

    /**
     * Accessibility attributes (checkout NativeTemplateElement.ts for more info)
     */

    // The type of accessibility element
    void setAccessibilityCategory(int accessibilityCategoryInt);
    AccessibilityCategory getAccessibilityCategory();

    // How the element behave in the accessibility navigation tree
    void setAccessibilityNavigation(int accessibilityNavigationInt);
    AccessibilityNavigation getAccessibilityNavigation();

    // Priority for accessibility navigation ordering
    void setAccessibilityPriority(float accessibilityPriority);
    float getAccessibilityPriority();

    // Accessibility Label, to announce context about the element
    void setAccessibilityLabel(const StringBox& accessibilityLabel);
    StringBox getAccessibilityLabel();

    // Accessibility Hint, to announce what the element will do once activated
    void setAccessibilityHint(const StringBox& accessibilityHint);
    StringBox getAccessibilityHint();

    // Accessibility Value, to announce the intrinsic content of the element
    void setAccessibilityValue(const StringBox& accessibilityValue);
    StringBox getAccessibilityValue();

    // Accessibility Identifier
    void setAccessibilityId(const StringBox& accessibilityId);
    StringBox getAccessibilityId() const;

    // Accessibility States, flags giving information about the element's status and context
    void setAccessibilityStateDisabled(bool accessibilityStateDisabled);
    bool getAccessibilityStateDisabled();
    void setAccessibilityStateSelected(bool accessibilityStateSelected);
    bool getAccessibilityStateSelected();
    void setAccessibilityStateLiveRegion(bool accessibilityStateLiveRegion);
    bool getAccessibilityStateLiveRegion();

    bool isAccessibilityFocusable();   // If true, it will be used as a concrete focusable accessibility node
    bool isAccessibilityContainer();   // If true, it is allowed to read its accessibility children
    bool isAccessibilityPassthrough(); // If true, its children are treated like its siblings (flattened navigation)

    bool isAccessibilityLeaf(); // If true, the node is a leaf in the flattened accessibility tree (ignore children)

    std::vector<ViewNode*> getAccessibilityChildrenShallow();
    std::vector<ViewNode*> getAccessibilityChildrenRecursive();

    ViewNode* hitTestAccessibilityChildren(const Point& parentDirectionDependentVisual);

    /**
     * Visibility Flags
     */

    bool extendViewportWithChildren() const;
    void setExtendViewportWithChildren(bool extendViewportWithChildren);

    bool ignoreParentViewport() const;
    void setIgnoreParentViewport(bool ignoreParentViewport);

    float getTranslationX() const;
    void setTranslationX(float translationX);

    /**
     * Returns the effective translation X that should be used for the backing view.
     * This takes in account RTL.
     */
    float getDirectionDependentTranslationX() const;

    float getTranslationY() const;
    void setTranslationY(float translationY);

    void setEstimatedWidth(float estimatedWidth);
    void setEstimatedHeight(float estimatedHeight);

    bool isHorizontal();

    const ViewNodeChildrenIndexer* getChildrenIndexer() const;

    ViewNodeTreeInfo dumpTreeInfo() const;

    void createSnapshot(ViewTransactionScope& viewTransactionScope, Function<void(Result<BytesView>)>&& callback) const;

    bool setVisibleInViewport(bool visibleInViewport);

    bool isIncludedInViewParent() const;

    /**
     This can be used to setup a view and measure it.
     This will set all the view layout attributes into the given view
     */

    Ref<ViewNode> makePlaceholderViewNode(ViewTransactionScope& viewTransactionScope, const Ref<View>& placeholderView);

    Result<Ref<ValueMap>> copyProcessedViewLayoutAttributes();

    Frame computeVisualFrameInRoot() const;

    void ensureFrameIsVisibleWithinParentScrolls(ViewTransactionScope& viewTransactionScope, bool animated) const;
    void ensureFrameIsVisibleWithinParentScrolls(ViewTransactionScope& viewTransactionScope,
                                                 Point directionAgnosticVisual,
                                                 const Size& size,
                                                 bool animated) const;

    bool scrollByOnePage(ViewTransactionScope& viewTransactionScope,
                         ScrollDirection scrollDirection,
                         bool animated,
                         float* outScrollPageCurrent,
                         float* outScrollPageMaximum);
    float getPageNumberForContentOffset(const Point& directionAgnosticContentOffset);

    bool hasChildWithAccessibilityId() const;
    bool hasAccessibilityId() const;

    bool accessibilityTreeNeedsUpdate() const;

    bool isUserSpecifiedView() const;

    const Ref<IViewNodeAssetHandler>& getAssetHandler() const;
    void setAssetHandler(const Ref<IViewNodeAssetHandler>& assetHandler);

private:
    YGNode* _yogaNode = nullptr;
    Weak<ViewNode> _parent;
    AttributeIds& _attributeIds;
    ILogger& _logger;
    std::unique_ptr<LazyLayoutData> _lazyLayoutData;

    CSSAttributesManager _cssAttributesManager;
    ViewNodeAttributesApplier _attributesApplier;
    // Will be set for ViewNode used during measure passes.
    Ref<ViewNode> _emittingViewNode;

    Frame _calculatedViewport;
    Frame _calculatedFrame;
    Frame _viewFrame;
    Frame _previousViewFrame;
    float _translationX = 0;
    float _translationY = 0;
    std::unique_ptr<ViewNodeScrollState> _scrollState;
    std::unique_ptr<ViewNodeAccessibilityState> _accessibilityState;
    std::unique_ptr<ViewNodeChildrenIndexer> _childrenIndexer;

    // The number of children views inside this view node subtree
    // that are inserted in the nearest parent view.
    int _numberOfViewChildrenInsertedInTree = 0;
    // The number of children views inside this view node subtree.
    int _numberOfViewChildren = 0;
    int _zIndex = 0;
    int _animationsCount = 0;
    int _lastChildrenIndexerId = 0;
    RawViewNodeId _rawId = 0;
    int _scrollAnchorPosition = ScrollAnchorPositionNone;

    std::bitset<30> _flags;

    ViewNodeTree* _viewNodeTree = nullptr;

    Ref<View> _view;
    Ref<ViewFactory> _viewFactory;
    Ref<IViewNodeAssetHandler> _assetHandler;

    Ref<ValueFunction> _onViewCreatedCallback;
    Ref<ValueFunction> _onViewDestroyedCallback;
    Ref<ValueFunction> _onViewChangedCallback;
    Ref<ValueFunction> _onLayoutCompletedCallback;

    void layoutFinished(ViewTransactionScope& viewTransactionScope, bool didPerformLayout);
    void layoutFinished(ViewTransactionScope& viewTransactionScope,
                        bool didPerformLayout,
                        float viewOffsetX,
                        float viewOffsetY,
                        float rtlOffsetX,
                        const Ref<Animator>& parentAnimator,
                        ViewNodeChildrenIndexer* parentChildrenIndexer,
                        ViewNodesFrameObserver* frameObserver);

    void updateScrollState();

    Point directionAgnosticVelocityFromDirectionDependentVelocity(const Point& directionDependentVelocity);

    bool updateViewFrameIfNeeded(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator);
    void setViewFrameNeedsUpdate();

    bool updateLazyLayout();
    void doUpdateViewTree(ViewTransactionScope& viewTransactionScope,
                          const Ref<View>& currentParentView,
                          bool parentVisibleInViewport,
                          bool limitToViewportEnabled,
                          bool parentViewChanged,
                          bool viewInflationEnabled,
                          const Ref<Animator>& parentAnimator,
                          ViewNodeUpdateViewTreeResult& updateResult,
                          int* currentViewIndex,
                          int* viewsCount);

    bool updateCalculatedFrame(float viewOffsetX,
                               float viewOffsetY,
                               float rtlOffsetX,
                               bool* calculatedFrameDidChange,
                               bool* calculatedSizeDidChange);

    bool calculateLayoutOnNodeIfNeeded(YGNode* yogaNode,
                                       float width,
                                       MeasureMode widthMode,
                                       float height,
                                       MeasureMode heightMode,
                                       LayoutDirection direction,
                                       bool forceLayout,
                                       bool isFromLazyLayout) const;

    bool createView(ViewTransactionScope& viewTransactionScope, const Ref<Animator>& animator);
    bool removeView(ViewTransactionScope& viewTransactionScope, bool safeRemove);

    void callViewChangedIfNeeded();

    void syncScrollSpecsWithViewIfNeeded(ViewTransactionScope& viewTransactionScope);
    void setScrollSpecsOnView(ViewTransactionScope& viewTransactionScope,
                              ViewNodeScrollState& scrollState,
                              const Point& directionAgnosticContentOffset,
                              bool animated);
    void setUpdatingScrollSpecs(bool updatingScrollSpecs);
    bool isUpdatingScrollSpecs() const;

    template<typename F>
    void updateViewportExtension(F&& handler);

    void handleOnScroll(const Point& directionDependentContentOffset,
                        const Point& directionDependentUnclampedContentOffset,
                        const Point& directionDependentVelocity);

    void applyFrame(ViewTransactionScope& viewTransactionScope,
                    const Ref<Animator>& animator,
                    const Frame& frame,
                    bool notifyAssetHandler) const;

    Frame calculateSelfViewport() const;
    inline bool isInScrollMode() const;

    Size measureExternal(float width, MeasureMode widthMode, float height, MeasureMode heightMode);

    bool hasChildWithZIndex() const;
    void setHasChildWithZIndex();

    void setHasChildWithAccessibilityId();

    void toXML(std::ostream& stream, int indent, bool recursive) const;

    bool doUpdateVisibility(const Frame& viewport, bool parentHasChanged, bool parentIsVisible, int& visitedNodes);

    ReusableArray<ViewNode*> sortChildrenByZIndex() const;

    const Ref<Animator>& resolveAnimator(const Ref<Animator>& parentAnimator) const;

    ViewNodeScrollState& getOrCreateScrollState();
    ViewNodeAccessibilityState& getOrCreateAccessibilityState();

    void getAccessibilityChildrenDeepWalk(std::vector<ViewNode*>& outChildren);

    SimplifiedScrollDirection simplifyScrollDirection(ScrollDirection scrollDirection);

    void populateViewStats(ViewNodeViewStats& viewStats) const;

    void setIsLayout(bool isLayout);
    void updateYogaMeasureFunc();
    bool needsYogaMeasureFunc() const;

    void setHasParent(bool hasParent);

    void setCSSNeedsUpdate();

    void setCSSHasChildNeedsUpdate();

    void updateCSS(ViewTransactionScope& viewTransactionScope,
                   const Ref<Animator>& animator,
                   bool force,
                   int siblingsCount,
                   int indexAmongSiblings,
                   CSSUpdateResult& updateResult);
    bool handleCSSChange(bool cssChanged);

    void onFinishedAnimating();

    void onChildrenChanged();
    void setChildrenIndexerNeedsUpdate();

    void updateTranslation(float translation, float* outValue);

    void setCalculatedViewportHasChildNeedsUpdate();

    void updateScrollAttributeValueIfNeeded(AttributeId attribute, float value, bool shouldNotifySync);

    void setIsLazyLayout(ViewTransactionScope& viewTransactionScope, bool isLazyLayout);
    void updateIsLazyLayout(ViewTransactionScope& viewTransactionScope);

    void setAccessibilityTreeNeedsUpdate();
    void propagateAccessibilityTreeUpToDate();

    bool isMemberOfAccessibilityTree();

    LazyLayoutData& getOrCreateLazyLayoutData();
    YGNode* getLazyLayoutYogaNode() const;
    const YGNode* getContainerYogaNode() const;

    Ref<Metrics> getMetrics() const;
};

} // namespace Valdi
