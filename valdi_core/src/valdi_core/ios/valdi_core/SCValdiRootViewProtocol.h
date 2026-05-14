#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "valdi_core/SCValdiScrollDirection.h"

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiRootViewProtocol <NSObject>

/**
 * Whether view inflation should stay enabled when the view is invisible.
 * When disabled, all the subviews are removed to free up memory when the view is not active.
 */
@property (assign, nonatomic) BOOL enableViewInflationWhenInvisible;

/**
 * Set whether the layout size and direction should be kept when the layout is invalidated. When this option
 * is set, the underlying Valdi Context will be able to immediately recalculate the layout on layout invalidation
 * without asking platform about the new layout specs. You can use this when the root view's size is not dependent on
 * calculating the size of the Valdi tree.
 */
@property (assign, nonatomic) BOOL retainsLayoutSpecsOnInvalidateLayout;

/**
 The full component path to the component in the .valdimodule.
*/
@property (readonly, nonatomic) NSString* componentPath;

/// Derived from the componentPath
@property (readonly, nonatomic) NSString* bundleName;

- (void)waitUntilInitialRenderWithCompletion:(void (^)(void))completion;

/**
 * Check if the view is scrollable in the specified direction at the specified touch point
 */
- (BOOL)canScrollAtPoint:(CGPoint)point direction:(SCValdiScrollDirection)direction;

/**
 * Ensure the valdi context currently assigned is aware of the current UITraitCollection
 */
- (void)updateTraitCollection;

@end

NS_ASSUME_NONNULL_END
