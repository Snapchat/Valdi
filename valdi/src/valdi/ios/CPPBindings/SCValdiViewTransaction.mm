//
//  SCValdiViewTransaction.mm
//  valdi-ios
//
//  Created by Simon Corsin on 7/29/24.
//

#import "valdi/ios/CPPBindings/SCValdiViewTransaction.h"
#import "valdi/ios/CPPBindings/UIViewHolder.h"

#import "valdi/ios/Animations/SCValdiAnimator.h"
#import "valdi/ios/SCValdiContext.h"
#import "valdi/ios/Utils/ContextUtils.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"
#import "valdi/ios/SCValdiViewNode.h"

#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiValueUtils.h"
#import "valdi_core/SCValdiViewAssetHandlingProtocol.h"
#import "valdi_core/SCValdiViewOwner.h"

#import "valdi/runtime/Attributes/Animator.hpp"
#import "valdi_core/cpp/Resources/LoadedAsset.hpp"

#import "valdi_core/SCValdiContentViewProviding.h"
#import "valdi_core/SCValdiRectUtils.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiView.h"
#import "valdi_core/SCValdiViewComponent.h"
#import "valdi_core/UIView+ValdiObjects.h"

#import "valdi_core/SCNValdiCoreAnimator+Private.h"

namespace ValdiIOS {

static void addCrossfadeAnimationIfNeeded(UIView *parentView, const Valdi::Ref<Valdi::Animator> &animator) {
    if (animator == nullptr) {
        return;
    }

    SCValdiAnimator *objcAnimator = (SCValdiAnimator *)djinni_generated_client::valdi_core::Animator::fromCpp(animator->getNativeAnimator());
    if (objcAnimator.crossfade) {
        [objcAnimator addTransitionOnLayer:parentView.layer];
    }
}

static void insertSubviewAtIndex(UIView *parentView, UIView *view, int index)
{
    UIView *contentView = parentView;
    if ([parentView respondsToSelector:@selector(contentViewForInsertingValdiChildren)]) {
        contentView = [(id)parentView contentViewForInsertingValdiChildren];
    }

    assert((NSUInteger)index <= contentView.subviews.count);
    [contentView insertSubview:view atIndex:(NSUInteger)index];
}

static UIView *toUIView(const Valdi::Ref<Valdi::View>& view) {
    return UIViewHolder::uiViewFromRef(view);
}

ViewTransaction::ViewTransaction() = default;
ViewTransaction::~ViewTransaction() = default;

    void ViewTransaction::flush(bool sync) {}

    void ViewTransaction::willUpdateRootView(const Valdi::Ref<Valdi::View>& view) {}

    void ViewTransaction::didUpdateRootView(const Valdi::Ref<Valdi::View>& view, bool layoutDidBecomeDirty) {
        if (layoutDidBecomeDirty) {
            [toUIView(view) invalidateIntrinsicContentSize];
        }
    }

    void ViewTransaction::moveViewToTree(const Valdi::Ref<Valdi::View>& view,
                        Valdi::ViewNodeTree* viewNodeTree,
                        Valdi::ViewNode* viewNode) {
        SCValdiContext *valdiContext = getValdiContext(viewNodeTree->getContext());

        if (valdiContext == nil) {
            auto context = viewNodeTree->getContext();
            auto contextPtr = context.get();
            SCLogValdiError(@"moveViewToTree: valdiContext is nil. "
                            @"context=%p, contextId=%u, hasUserData=%d",
                            contextPtr,
                            contextPtr ? contextPtr->getContextId() : 0,
                            contextPtr ? (contextPtr->getUserData() != nullptr) : false);
        }

        UIView *uiView = toUIView(view);
        uiView.valdiContext = valdiContext;

        if (viewNode != nullptr) {
            SCValdiViewNode *valdiViewNode = [[SCValdiViewNode alloc] initWithViewNode:viewNode];
            uiView.valdiViewNode = valdiViewNode;

            if ([uiView respondsToSelector:@selector(didMoveToValdiContext:viewNode:)]) {
                [(id<SCValdiViewComponent>)uiView
                didMoveToValdiContext:valdiContext
                viewNode:valdiViewNode];
            }
        } else {
            uiView.valdiViewNode = nullptr;
        }
    }

    void ViewTransaction::insertChildView(const Valdi::Ref<Valdi::View>& view,
                         const Valdi::Ref<Valdi::View>& childView,
                         int index,
                         const Valdi::Ref<Valdi::Animator>& animator) {
        UIView *childUIView = toUIView(childView);
        UIView *uiView = toUIView(view);
        if (uiView && childUIView) {
            addCrossfadeAnimationIfNeeded(uiView, animator);
            insertSubviewAtIndex(uiView, childUIView, index);
        }
    }

    void ViewTransaction::removeViewFromParent(const Valdi::Ref<Valdi::View>& view,
                              const Valdi::Ref<Valdi::Animator>& animator,
                              bool shouldClearViewNode) {
        UIView *uiView = toUIView(view);
        UIView *existingParent = uiView.superview;
        if (existingParent) {
            [uiView.layer removeAllAnimations];
            addCrossfadeAnimationIfNeeded(existingParent, animator);
            [uiView removeFromSuperview];
        }
    }

    void ViewTransaction::invalidateViewLayout(const Valdi::Ref<Valdi::View>& view) {
        [toUIView(view) invalidateLayoutAndMarkFlexBoxDirty:NO];
    }

    void ViewTransaction::setViewFrame(const Valdi::Ref<Valdi::View>& view,
                      const Valdi::Frame& newFrame,
                      bool isRightToLeft,
                      const Valdi::Ref<Valdi::Animator>& animator) {
        UIView *uiView = toUIView(view);
        if (!uiView) {
            return;
        }

        SCValdiViewLayout calculatedLayout =
            SCValdiMakeViewLayout(newFrame.x, newFrame.y, newFrame.width, newFrame.height);

        CGRect currentBounds = uiView.bounds;
        CGSize oldSize = currentBounds.size;
        currentBounds.size = calculatedLayout.size;

        SCValdiAnimator *objcAnimator;

        if (animator != nullptr) {
            objcAnimator = (SCValdiAnimator *)djinni_generated_client::valdi_core::Animator::fromCpp(animator->getNativeAnimator());

            [objcAnimator addAnimationOnLayer:uiView.layer
                                forKeyPath:@"position"
                                        value:[NSValue valueWithCGPoint:calculatedLayout.center]];
            [objcAnimator addAnimationOnLayer:uiView.layer
                                forKeyPath:@"bounds"
                                        value:[NSValue valueWithCGRect:currentBounds]];

            BOOL sizeChanged = !CGSizeEqualToSize(oldSize, calculatedLayout.size);
            if (sizeChanged) {
                if (oldSize.width == 0 || oldSize.height == 0) {
                    // We're animating the view appearance, make sure the view displays something
                    [uiView setNeedsDisplay];
                } else {
                    [objcAnimator addCompletion:^{
                        [uiView setNeedsDisplay];
                    }];
                }
            }
        } else {
            uiView.center = calculatedLayout.center;
            uiView.bounds = currentBounds;
        }

        [uiView.valdiViewNode didApplyLayoutWithAnimator:objcAnimator];
    }

    void ViewTransaction::setViewScrollSpecs(const Valdi::Ref<Valdi::View>& view,
                            const Valdi::Point& directionDependentContentOffset,
                            const Valdi::Size& contentSize,
                            bool animated) {
        UIView *uiView = toUIView(view);
        if (!uiView) {
            return;
        }

        CGPoint objcContentOffset = CGPointMake(CGFloatNormalize(directionDependentContentOffset.x), CGFloatNormalize(directionDependentContentOffset.y));
        CGSize objcContentSize = CGSizeMake(CGFloatNormalize(contentSize.width), CGFloatNormalize(contentSize.height));
        [uiView scrollSpecsDidChangeWithContentOffset:objcContentOffset contentSize:objcContentSize animated:animated];
    }

    void ViewTransaction::setViewLoadedAsset(const Valdi::Ref<Valdi::View>& view,
                            const Valdi::Ref<Valdi::LoadedAsset>& loadedAsset,
                            bool shouldDrawFlipped) {
        id<SCValdiViewAssetHandlingProtocol> uiView = (id<SCValdiViewAssetHandlingProtocol>)toUIView(view);
        if (!uiView || ![uiView respondsToSelector:@selector(onValdiAssetDidChange:shouldFlip:)]) {
            return;
        }

        [uiView onValdiAssetDidChange:NSObjectFromValdiObject(loadedAsset, YES) shouldFlip:shouldDrawFlipped];
    }

    void ViewTransaction::layoutView(const Valdi::Ref<Valdi::View>& view) {
        [toUIView(view) layoutIfNeeded];
    }

    void ViewTransaction::cancelAllViewAnimations(const Valdi::Ref<Valdi::View>& view) {
            UIView *uiView = toUIView(view);
            if (!uiView) {
                return;
            }

            [uiView.layer removeAllAnimations];
        }

    void ViewTransaction::willEnqueueViewToPool(const Valdi::Ref<Valdi::View>& view,
                                    Valdi::Function<void(Valdi::View&)> onEnqueue) {
        UIView *uiView = toUIView(view);

        BOOL allowRecycling = YES;

        uiView.valdiContext = nil;
        uiView.valdiViewNode = nil;

        if (![uiView respondsToSelector:@selector(willEnqueueIntoValdiPool)] || ![uiView willEnqueueIntoValdiPool]) {
            allowRecycling = NO;
        }

        if (allowRecycling) {
            if (uiView.layer.animationKeys.count) {
                [uiView.layer removeAllAnimations];
                // If we had animations, the CALayer is going to be dirty and re-using
                // it immediately might cause issues.
                auto onEnqueueCopy = onEnqueue;
                auto selfView = view;
                dispatch_async(dispatch_get_main_queue(), ^{
                    onEnqueueCopy(*selfView);
                });
            } else {
                onEnqueue(*view);
            }
        }
    }

    void ViewTransaction::snapshotView(const Valdi::Ref<Valdi::View>& view,
                      Valdi::Function<void(Valdi::Result<Valdi::BytesView>)> cb) {
        UIView *uiView = toUIView(view);

        if (!uiView) {
            cb(Valdi::BytesView());
            return;
        }

        CGRect bounds = uiView.bounds;
        UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc] initWithBounds:bounds];
        NSData *data = [renderer PNGDataWithActions:^(UIGraphicsImageRendererContext * _Nonnull rendererContext) {
            [uiView drawViewHierarchyInRect:bounds afterScreenUpdates:YES];
        }];

        if (data) {
            cb(ValdiIOS::BufferFromNSData(data));
        } else {
            cb(Valdi::BytesView());
        }
    }

    void ViewTransaction::flushAnimator(const Valdi::Ref<Valdi::Animator>& animator,
                       const Valdi::Value& completionCallback) {
        animator->getNativeAnimator()->flushAnimations(completionCallback);
    }

    void ViewTransaction::cancelAnimator(const Valdi::Ref<Valdi::Animator>& animator) {
        animator->getNativeAnimator()->cancel();
    }

    void ViewTransaction::executeInTransactionThread(Valdi::DispatchFunction executeFn) {
        executeFn();
    }
}
