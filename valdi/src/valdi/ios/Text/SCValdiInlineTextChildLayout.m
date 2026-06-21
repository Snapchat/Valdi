//
//  SCValdiInlineTextChildLayout.m
//  valdi-ios
//

#import "valdi/ios/Text/SCValdiInlineTextChildLayout.h"

#import "valdi/ios/Text/SCValdiInlineViewAttachmentInfo.h"
#import "valdi/ios/Text/SCValdiProcessedText.h"
#import "valdi/ios/Text/SCValdiTextAnimationPresentation.h"

#import "valdi_core/SCValdiRectUtils.h"
#import "valdi_core/SCValdiViewNodeProtocol.h"
#import "valdi_core/UIView+ValdiObjects.h"

static void SCValdiApplyInlineTextChildPresentation(UIView *childView,
                                                    SCValdiTextAnimationPresentation *_Nullable presentation)
{
    id<SCValdiViewNodeProtocol> viewNode = childView.valdiViewNode;
    if (viewNode == nil) {
        return;
    }

    if (presentation != nil && presentation.hasOpacityOverride) {
        [viewNode setValue:@(presentation.opacity) forValdiAttribute:@"opacity"];
    } else {
        [viewNode removeValueForValdiAttribute:@"opacity"];
    }

    if (presentation != nil && presentation.hasTransformOverride) {
        [viewNode setValue:@(presentation.translationY) forValdiAttribute:@"translationY"];
        [viewNode setValue:@(presentation.scale) forValdiAttribute:@"scaleX"];
        [viewNode setValue:@(presentation.scale) forValdiAttribute:@"scaleY"];
    } else {
        [viewNode removeValueForValdiAttribute:@"translationY"];
        [viewNode removeValueForValdiAttribute:@"scaleX"];
        [viewNode removeValueForValdiAttribute:@"scaleY"];
    }
}

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

        SCValdiInlineViewAttachmentInfo *attachment = [processedText inlineViewAttachmentForViewIndex:index
                                                                                        effectiveRange:NULL];
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

void SCValdiApplyInlineTextChildAnimations(SCValdiProcessedText *_Nullable processedText,
                                           UIView *containerView,
                                           SCValdiInlineTextChildPresentationProvider presentationProvider)
{
    if (containerView == nil) {
        return;
    }

    [containerView.subviews enumerateObjectsUsingBlock:^(UIView *childView, NSUInteger index, BOOL *stop) {
        (void)stop;
        SCValdiTextAnimationPresentation *presentation = nil;
        NSRange range = NSMakeRange(NSNotFound, 0);
        [processedText inlineViewAttachmentForViewIndex:index effectiveRange:&range];
        if (range.location != NSNotFound) {
            presentation = presentationProvider(range);
        }
        SCValdiApplyInlineTextChildPresentation(childView, presentation);
    }];
}
