//
//  SCValdiScrollView.h
//  Valdi
//
//  Created by Simon Corsin on 5/12/18.
//

#import "valdi_core/SCValdiContentViewProviding.h"
#import "valdi_core/SCValdiView.h"
#import "valdi_core/SCValdiViewComponent.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiScrollView : SCValdiView <SCValdiViewComponent>

/**
 * @deprecated Please avoid interacting directly with the ValdiScrollView
 *
 * Please consider using the typescript API to interact with the scrolling behavior (or using canScrollAtPoint).
 *
 * Here is a few common API that can help integration within the native code:
 *  - SCValdiRootView.canScrollAtPoint: will allow checking the scrollable state of your feature at runtime
 *  - the <scroll/> attributes can help when working with nested scrollview sitations (or swipe/dismiss with scroll)
 *     - "bouncesFromDragAtStart={false}" can help when scrolling backward should trigger another gesture
 *     - "bouncesFromDragAtEnd={false}" can help when scrolling forward should trigger another gesture
 *     - "bounces={false}" can help if we don't want the bouncing effect, so the scroll doesn't swallow events
 */
@property (readonly, nonatomic) UIScrollView* innerScrollView;

/**
 * @deprecated Please avoid interacting directly with the ValdiScrollView
 *
 * (warning) Any change to the contentOffset "may" be overriden by typescript behavior at any point in time.
 *
 * Please consider manipulating the contentOffset of your scrollview directly in typescript using a ScrollViewHandler
 */
- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated;

@end

NS_ASSUME_NONNULL_END
