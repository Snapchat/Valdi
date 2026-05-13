//
//  SCValdiScrollViewDelegate.h
//  Valdi
//
//  Created by Simon Corsin on 5/13/18.
//

#import "valdi_core/SCValdiFunction.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol SCValdiIScrollPerfLoggerBridge;

NS_ASSUME_NONNULL_BEGIN

@interface SCValdiScrollViewDelegate : NSObject <UIScrollViewDelegate>

// Cancels all gestures recognizers on all of the scroll view's child views when scrolling begins
// Default YES
@property (assign, nonatomic) BOOL cancelsTouchesOnScroll;

@property (strong, nonatomic, nullable) id<SCValdiIScrollPerfLoggerBridge> scrollPerfLoggerBridge;

- (void)scrollViewDidEndScrolling:(UIScrollView*)scrollView;

@end

NS_ASSUME_NONNULL_END
