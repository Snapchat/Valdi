#import "valdi_core/SCValdiGestureListener.h"
#import "valdi_core/SCValdiInternedStringBase.h"
#import "valdi_core/SCValdiRootViewProtocol.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class SCValdiActions;
@protocol SCValdiAttributesBinderProtocol;
@protocol SCValdiRuntimeProtocol;

typedef NS_ENUM(NSUInteger, SCValdiLayoutDirection) {
    SCValdiLayoutDirectionLTR,
    SCValdiLayoutDirectionRTL,
};

typedef uint32_t SCValdiContextId;

NS_ASSUME_NONNULL_BEGIN

typedef UIView* _Nullable (^SCValdiViewFactoryBlock)(void);
typedef void (^SCValdiBindAttributesCallback)(__kindof id<SCValdiAttributesBinderProtocol> _Nullable attributesBinder);

/**
 A Valdi context is created for every Valdi-based view subtree and attaches to the root view.
 The context attaches to the root view and connects the Valdi node tree to the root platform UIView.
 */
@protocol SCValdiContextProtocol <NSObject>

/**
 Return the contextId of the ValdiContext
*/
@property (readonly, nonatomic) SCValdiContextId contextId;

/**
 Get a reference to the context's runtime instance.
*/
@property (readonly, nonatomic) id<SCValdiRuntimeProtocol> runtime;

/**
 * When enabled, full valdi accessibility is enabled
 */
@property (assign, nonatomic) BOOL enableAccessibility;

/**
 Whether view inflation should be enabled. View inflation can be explicitly disabled for a given node tree
 e.g. if the containing view is not in the platform view hierarchy - to save resources.

 Used internally by framework utilities.
 */
@property (assign, nonatomic) BOOL viewInflationEnabled;

/**
 * Set whether the layout size and direction should be kept when the layout is invalidated. When this option
 * is set, the Context will be able to immediately recalculate the layout on layout invalidation without asking
 * platform about the new layout specs.
 */
@property (assign, nonatomic) BOOL retainsLayoutSpecsOnInvalidateLayout;

/**
 Set whether the touch gestures system should respect the animated frames instead of always
 looking at the final state of the animation. This makes the touch gestures accurate when using
 animations.
 */
@property (assign, nonatomic) BOOL enableAccurateTouchGesturesInAnimations;

/**
 Reference to the root view's component's viewModel.

 Used by SCValdiView subclasses.
*/
@property (strong, nonatomic, nullable) id viewModel;

/**
 Reference to the root view's component's action handler.

 This is a legacy API currently used in iOS. Will be removed.
*/
@property (weak, nonatomic, nullable) id actionHandler;

/**
 Reference to the root view's SCValdiViewOwner.

 This is a legacy API, currently used in iOS. Will be removed.
*/
@property (weak, nonatomic, nullable) id owner;

/**
 Returns whether this Valdi Context was destroyed
 */
@property (readonly, nonatomic) BOOL destroyed;

/**
 * Returns whether the hit test sync deadline (callSyncWithDeadline) should be disabled,
 * reverting to the legacy blocking dispatch_sync behavior. Controlled via the
 * VALDI_DISABLE_HIT_TEST_SYNC_DEADLINE runtime tweak.
 */
@property (readonly, nonatomic) BOOL disableHitTestSyncDeadline;

/**
 Reference to the root view's component's actions.

 This is a legacy API used to allow JS code to trigger Objective-C actions.
 Currently used in iOS. Will be removed.
*/
@property (strong, nonatomic, nullable) SCValdiActions* actions;

/**
 * Reference to the trait collection that was propagated to the valdi context from the root view
 * This trait collection is used to scale and render text based on the accessibility settings of the user's device
 * If this trait collection is not set, valdi will use default scaling and text rendering, ignoring accessibility
 */
@property (strong, nonatomic, nullable) UITraitCollection* traitCollection;

/**
 Reference to the root view's Valdi context.

 This is a legacy API currently used in iOS. Will be removed.
*/
@property (readonly, nonatomic, nullable) id<SCValdiContextProtocol> rootContext;

/** returns YES if this view has rendered a view model at least once, or has none, and all of its child components also
 * have.
 * This is a legacy API, currently used in iOS. Will be removed.
 */
// TODO(simon): Remove once iOS stop using it
@property (nonatomic, readonly) BOOL hasCompletedInitialRenderIncludingChildComponents;

/**
 Returns whether the root view should destroy the context when it's deallocated
 */
@property (readonly, nonatomic) BOOL rootValdiViewShouldDestroyContext;

/**
 * Whether the legacy measure behavior should be used, which means that for calls of
 * measureLayout, defined values within the given size constraint would be treated
 * as if the container size was fixed, instead of using the size as a maximum constraint.
 * Example for CGSize(200, CGFLOAT_MAX) given to measureLayout:
 * - With the legacy measure behavior: the width 200 would be treated as a hardcoded container width.
 *   The layout would therefore always be of width 200, and never less.
 * - Without the legacy measure behavior: the width 200 is treated as a maximum width, the layout
 *   can be of any width between 0 and 200.
 */
@property (assign, nonatomic) BOOL useLegacyMeasureBehavior;

/**
 Set a gesture listener that will be notified whenever a gesture is trigerred.
 */
@property (strong, nonatomic, nullable) id<SCValdiGestureListener> gestureListener;

/**
 Returns a copy of all the Objective-C references that were tracked.
 Will return nil if references tracking was not enabled.
 */
@property (readonly, nonatomic, nullable) NSArray<id>* trackedObjCReferences;

/**
 Notify the runtime that a value has changed for the given attribute.
 This should be called only for attributes that can be mutated outside of Valdi.
 For example the text value of an editable text view.

 Prefer using didChangeValue:forInternedValdiAttribute:inViewNode: for better performance.
 */
- (void)didChangeValue:(nullable id)value
     forValdiAttribute:(NSString*)attributeName
            inViewNode:(id<SCValdiViewNodeProtocol>)viewNode;

/**
 Notify the runtime that a value has changed for the given attribute.
 This should be called only for attributes that can be mutated outside of Valdi.
 For example the text value of an editable text view.

 */
- (void)didChangeValue:(nullable id)value
    forInternedValdiAttribute:(SCValdiInternedStringRef)attributeName
                   inViewNode:(id<SCValdiViewNodeProtocol>)viewNode;

/**
 Call destroy once you are sure that this Valdi node tree is not needed anymore and should release all possible
 resources.
*/
- (void)destroy;

/**
 Get a reference to the root platform UIView instance.

 Used internally by framework utilities.
*/
- (nullable UIView*)rootValdiView;

/**
 Set the root view to use on this Valdi Context.
 The view hierarchy will be populated and added to the given rootView.
 Passing nil will teardown the view hierarchy from the previous root view.
 */
- (void)setRootValdiView:(nullable UIView<SCValdiRootViewProtocol>*)rootValdiView;

/**
 Get the root ViewNode, or nil if the Valdi Context was never rendered
 */
- (nullable id<SCValdiViewNodeProtocol>)rootViewNode;

/**
 Set the layout size and direction for the component. This will potentially
 calculate the layout and update the view hierarchy.

 Used internally by framework utilities.
*/
- (void)setLayoutSize:(CGSize)size direction:(SCValdiLayoutDirection)direction;

/**
 Measure the size of the component within the given size constraints.
 */
- (CGSize)measureLayoutWithMaxSize:(CGSize)size direction:(SCValdiLayoutDirection)direction;

/**
 * Set the visible viewport that should be used when computing the viewports from
 * the root node. This can be used to only render a smaller area of the root node.
 * When not set, the root node will be considered as completely visible.
 */
- (void)setVisibleViewportWithFrame:(CGRect)frame;

/**
 * Unset a previously set visible viewport. The root node will be considered
 * as completely visible.
 */
- (void)unsetVisibleViewport;

/**
 Execute the completion block once the initial render pass has completed.

 Used internally by framework utilities.
*/
- (void)waitUntilInitialRenderWithCompletion:(void (^)(void))completion;

/**
 Register a custom view factory for the given view class. The created views from
 this factory will be enqueued into a local view pool instead of the global view pool.
 */
- (void)registerViewFactory:(SCValdiViewFactoryBlock)viewFactory forClass:(Class)viewClass;

/**
 Register a custom view factory for the given view class. The created views from
 this factory will be enqueued into a local view pool instead of the global view pool.
 If provided, the attributesBinder callback will be used to bind any additional attributes.
 */
- (void)registerViewFactory:(SCValdiViewFactoryBlock)viewFactory
           attributesBinder:(nullable SCValdiBindAttributesCallback)attributesBinder
                   forClass:(Class)viewClass;

/**

Return the platform UIView instance for a given node identifier.

Used from SCValdiView subclasses to implement view getters based on identifiers.
*/
- (nullable UIView*)viewForNodeId:(NSString*)nodeId;

/**

Used from SCValdiView subclasses to implement @Action selectors.
*/
- (void)performJsAction:(NSString*)actionName parameters:(nullable NSArray*)parameters;

/**
 Attaches this context to the given parent context.
 */
- (void)setParentContext:(nullable id<SCValdiContextProtocol>)parentContext;

/**
 Synchronously wait until the context has completed rendering
 */
- (void)waitUntilRenderCompletedSync;

/**
 Synchronously wait until the context has completed rendering,
 and optionally flush renders into the calling thread if shouldFlushRenderRequests
 is true. In this case, this will cause the calling thread to process renders
 scheduled into the context instead of waiting for the main thread to process them.
 */
- (void)waitUntilRenderCompletedSyncWithFlush:(BOOL)shouldFlushRenderRequests;

/**
 Asynchronously wait until the context has completed rendering, and call the
 completion when done.
 */
- (void)waitUntilRenderCompletedWithCompletion:(void (^)(void))completion;

/**
  Registers a callback to be called whenever the Valdi layout becomes dirty. This happens
  whenever a node is moved/removed/added or when a layout attribute is changed during a render.
  A dirty layout means that the measured size of the Valdi component might change.
 */
- (void)onLayoutDirty:(void (^)(void))onLayoutDirtyCallback;

@end

NS_ASSUME_NONNULL_END
