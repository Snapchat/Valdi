//
//  SCValdiInlineTextChildLayout.h
//  valdi-ios
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class SCValdiProcessedText;

NS_ASSUME_NONNULL_BEGIN

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

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END
