#import <UIKit/UIKit.h>

#import "valdi_core/SCValdiView.h"

#import "valdi_core/SCValdiAction.h"
#import "valdi_core/SCValdiActionUtils.h"
#import "valdi_core/SCValdiActionWithBlock.h"
#import "valdi_core/SCValdiActions.h"

#import "valdi_core/SCValdiRuntimeProtocol.h"

#import "valdi_core/SCValdiINavigator.h"
#import "valdi_core/SCValdiScrollView.h"
#import "valdi_core/SCValdiScrollViewDelegate.h"

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiRootViewProtocol.h"
#import "valdi_core/SCValdiViewComponent.h"
#import "valdi_core/SCValdiViewOwner.h"

#import "valdi_core/UIView+ValdiBase.h"
#import "valdi_core/UIView+ValdiObjects.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/**
View classes generated from valdi modules will use SCValdiRootView as the parent class.
This header is also an "umbrella header" defining the imports necessary for those
view classes.
*/
@interface SCValdiRootView : SCValdiView <SCValdiViewComponent, SCValdiRootViewProtocol>

@property (weak, nonatomic, nullable) id<SCValdiViewOwner> owner;

@property (readonly, nonatomic) NSString* bundleName;
@property (readonly, nonatomic) NSString* viewName;
@property (readonly, nonatomic) NSString* componentPath;

/**
 * Whether view inflation should stay enabled when the view is invisible.
 * When disabled, all the subviews are removed to free up memory when the view is not active.
 */
@property (assign, nonatomic) BOOL enableViewInflationWhenInvisible;

/**
 * Instantiates and inflates the UI with the component from self.componentPath.
 */
- (instancetype)initWithViewModelUntyped:(nullable id)viewModelUntyped
                 componentContextUntyped:(nullable id)componentContextUntyped
                                 runtime:(id<SCValdiRuntimeProtocol>)runtime;

/**
 * Instantiates and inflates the UI with the component from self.componentPath.
 * @note You shouldn't need to use the @c owner parameter. It exists for historical reasons.
 *
 * This is a designated initializer.
 */
- (instancetype)initWithOwner:(nullable id<SCValdiViewOwner>)owner
                    viewModel:(nullable id)viewModel
             componentContext:(nullable id)componentContext
                      runtime:(id<SCValdiRuntimeProtocol>)runtime NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithOwner:(nullable id<SCValdiViewOwner>)owner
                cppMarshaller:(nullable void*)cppMarshaller
                      runtime:(id<SCValdiRuntimeProtocol>)runtime NS_DESIGNATED_INITIALIZER;
/**
 * Use this initializer e.g. when you need to create a Valdi root view instance for later use with a
 * pre-prepared ValdiContext.
 *
 * This is a designated initializer.
 */
- (instancetype)initWithoutValdiContext NS_SWIFT_NAME(init()) NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder __attribute__((unavailable("initWithCoder: is not available")));

- (instancetype)init __attribute__((unavailable("plain init is not available")));
- (instancetype)initWithFrame:(CGRect)frame __attribute__((unavailable("initWithFrame is not available")));
+ (instancetype)new __attribute__((unavailable("new is not available")));

- (void)waitUntilInitialRenderWithCompletion:(void (^)(void))completion;

/**
  Registers a callback to be called whenever the Valdi layout becomes dirty. This happens
  whenever a node is moved/removed/added or when a layout attribute is changed during a render.
  A dirty layout means that the measured size of the Valdi component might change.
 */
- (void)onLayoutDirty:(void (^)(void))onLayoutDirtyCallback;

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
  The componentPath to use when inflating this root view
 */
+ (NSString*)componentPath;

/**
 * Check if the view is scrollable in the specified direction at the specified touch point
 */
- (BOOL)canScrollAtPoint:(CGPoint)point direction:(SCValdiScrollDirection)direction;

@end

extern NSNotificationName const SCValdiRootViewDisplayInsetDidChangeNotificationKey;
extern NSNotificationName const SCValdiRootViewTraitCollectionDidChangeNotificationKey;

NS_ASSUME_NONNULL_END
