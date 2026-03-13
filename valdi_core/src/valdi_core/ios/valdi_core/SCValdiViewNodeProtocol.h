#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "valdi_core/SCValdiRectUtils.h"
#import "valdi_core/SCValdiScrollDirection.h"

@protocol SCValdiAnimatorProtocol;

NS_ASSUME_NONNULL_BEGIN

typedef void (^SCValdiContextDidFinishLayoutBlock)(__kindof UIView* view, id<SCValdiAnimatorProtocol> animator);

/**
 Represents a single node in Valdi's "shadow node tree" representation of a given view hierarchy.
 */
@protocol SCValdiViewNodeProtocol <NSObject>

/**
 Get the view that this ViewNode holds
 */
@property (readonly, nonatomic) UIView* view;

/**
 Is the current UI yoga flexbox layout direction horizontal (i.e. row or row-reversed)?

 Used by SCValdiScrollView.
*/
@property (readonly, nonatomic) BOOL isLayoutDirectionHorizontal;

/**
 Is the current UI layout direction RTL?

 Used internally by SCValdiGestureRecognizers implementation.
*/
@property (readonly, nonatomic) BOOL isRightToLeft;

/**
 Mark the current yoga layout calculation as dirty, to be recalculated.

 Used by UIView+ValdiBase.m.
*/
- (void)markLayoutDirty;

/**
 Dynamically associate an object for a given key.

 Used internally by SCValdiAttributesBinder.mm.
*/
- (void)setRetainedObject:(id _Nullable)object forKey:(NSString*)key;

/**


 Used internally by SCValdiViewManager.mm.
*/
- (void)didApplyLayoutWithAnimator:(id<SCValdiAnimatorProtocol>)animator;

/**
 Set the value for a given attribute (as if it was set on the JS side)

 Used by UIView+Valdi.m.
*/
- (void)setValue:(id _Nullable)value forValdiAttribute:(NSString*)attributeName;

/**
 Gets the current value for a given attribute
*/
- (id _Nullable)valueForValdiAttribute:(NSString*)attributeName;

/**
 Remove the value for a given attribute (as if it was removed on the JS side)

 Used by UIView+Valdi.m.
*/
- (void)removeValueForValdiAttribute:(NSString*)attributeName;

/**
 Gets the current preprocessed value for a given attribute
*/
- (id _Nullable)preprocessedValueForValdiAttribute:(NSString*)attributeName;

/**
 Set a block to be executed once layout completes.

 Used by UIView+Valdi.m.
*/
- (void)setDidFinishLayoutBlock:(SCValdiContextDidFinishLayoutBlock _Nullable)block forKey:(NSString*)key;

/**
 Returns whether the view node has a registered setDidFinishLayoutBlock for the given key
 */
- (BOOL)hasDidFinishLayoutBlockForKey:(NSString*)key;

/**
 Return a copy of the children of this ViewNode
 */
- (NSArray<id<SCValdiViewNodeProtocol>>*)children;

/**
 * Given a position in this ViewNode's coordinate space,
 * returns the position as if the ViewNode was in LTR.
 */
- (CGPoint)relativeDirectionAgnosticPointFromPoint:(CGPoint)point;

/**
 * Given a position in this ViewNode's coordinate space,
 * returns the position from the root as if the ViewNode was in LTR.
 */
- (CGPoint)absoluteDirectionAgnosticPointFromPoint:(CGPoint)point;

/**
 Given a d/x value within the ViewNode's coordinate space, returns
 the d/x value in either direction agnostic coordinates or view coordinates.
 */
- (CGFloat)resolveDeltaX:(CGFloat)deltaX directionAgnostic:(BOOL)directionAgnostic;

- (void)notifyOnScrollWithContentOffset:(CGPoint)contentOffset
                   updatedContentOffset:(inout CGPoint* _Nonnull)updatedContentOffset
                               velocity:(CGPoint)velocity;

- (void)notifyOnScrollEndWithContentOffset:(CGPoint)contentOffset;

- (void)notifyOnDragStartWithContentOffset:(CGPoint)contentOffset velocity:(CGPoint)velocity;

- (void)notifyOnDragEndingWithContentOffset:(CGPoint)contentOffset
                                   velocity:(CGPoint)velocity
                       updatedContentOffset:(inout CGPoint* _Nonnull)updatedContentOffset;

- (BOOL)canScrollAtPoint:(CGPoint)point direction:(SCValdiScrollDirection)direction;

@end

NS_ASSUME_NONNULL_END
