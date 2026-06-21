//
//  SCValdiInlineTextChildLayout.h
//  valdi-ios
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class SCValdiProcessedText;
@class SCValdiTextAnimationPresentation;

NS_ASSUME_NONNULL_BEGIN

typedef SCValdiTextAnimationPresentation *_Nullable (^SCValdiInlineTextChildPresentationProvider)(NSRange range);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Applies frames to inline Valdi child UIViews from TextKit attachment geometry.
 *
 * Children referenced by processedText are positioned according to text layout;
 * children in the container that are no longer referenced are assigned an empty
 * frame so stale inline views cannot remain visible or tappable.
 */
void SCValdiApplyInlineTextChildFrames(SCValdiProcessedText *_Nullable processedText,
                                       NSLayoutManager *_Nullable layoutManager,
                                       NSTextContainer *_Nullable textContainer,
                                       CGPoint originOffset,
                                       UIView *containerView);

/**
 * Applies animated opacity and transform attributes to inline Valdi child UIViews.
 *
 * Children without an inline attachment, or whose attachment has no active presentation,
 * have the temporary animation attributes removed through their Valdi view node.
 */
void SCValdiApplyInlineTextChildAnimations(SCValdiProcessedText *_Nullable processedText,
                                           UIView *containerView,
                                           SCValdiInlineTextChildPresentationProvider presentationProvider);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
