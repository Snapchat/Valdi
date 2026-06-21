//
//  SCValdiInlineTextChildLayout.m
//  valdi-ios
//

#import "valdi/ios/Text/SCValdiInlineTextChildLayout.h"

#import "valdi/ios/Text/SCValdiInlineViewAttachmentInfo.h"
#import "valdi/ios/Text/SCValdiProcessedText.h"

#import "valdi_core/SCValdiRectUtils.h"

void SCValdiApplyInlineTextChildFrames(SCValdiProcessedText *_Nullable processedText,
                                       NSLayoutManager *_Nullable layoutManager,
                                       NSTextContainer *_Nullable textContainer,
                                       CGPoint originOffset,
                                       UIView *containerView)
{
    if (containerView == nil) {
        return;
    }

    NSArray<UIView *> *children = containerView.subviews;

    [children enumerateObjectsUsingBlock:^(UIView *childView, NSUInteger index, BOOL *stop) {
        (void)stop;

        SCValdiInlineViewAttachmentInfo *attachment = [processedText inlineViewAttachmentForViewIndex:index];
        if (attachment == nil) {
            childView.frame = CGRectZero;
            return;
        }

        if (layoutManager == nil || textContainer == nil) {
            return;
        }

        CGRect frame = [processedText rectForInlineViewAttachment:attachment
                                                    layoutManager:layoutManager
                                                    textContainer:textContainer];
        if (CGRectIsNull(frame)) {
            return;
        }

        frame.origin.x += originOffset.x;
        frame.origin.y += originOffset.y;

        SCValdiViewLayout calculatedLayout =
            SCValdiMakeViewLayout(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
        CGRect currentBounds = childView.bounds;
        currentBounds.size = calculatedLayout.size;
        childView.center = calculatedLayout.center;
        childView.bounds = currentBounds;
    }];
}
