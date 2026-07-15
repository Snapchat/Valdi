//
//  SCSnapDrawingUIView.h
//  valdi-ios
//
//  Created by Simon Corsin on 8/30/22.
//

#import "valdi_core/SCMacros.h"
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCSnapDrawingRuntime;

@interface SCSnapDrawingUIView : UIView

- (instancetype)initWithRuntime:(id<SCSnapDrawingRuntime>)runtime;

// Recomputes the on-screen visible region and clamps the drawable(s) to it. layoutSubviews only
// fires on size changes, so a host must call this when the view's position on screen changes
// (e.g. panning a zoomed layer) -- otherwise the clamp region goes stale and the clip edge shows.
- (void)refreshRenderViewport;

VALDI_NO_VIEW_INIT

@end

NS_ASSUME_NONNULL_END
